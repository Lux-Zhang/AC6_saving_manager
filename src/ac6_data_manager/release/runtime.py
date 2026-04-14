from __future__ import annotations

import json
import os
from dataclasses import dataclass
from pathlib import Path
from typing import Mapping, Sequence

from ac6_data_manager.emblem import (
    DateTimeBlock,
    EmblemAsset,
    EmblemGroup,
    EmblemImage,
    EmblemLayer,
    EmblemOrigin,
    EmblemRecord,
    GroupData,
    build_catalog,
    build_share_to_user_import_plan,
)
from ac6_data_manager.gui import (
    AuditEntryDto,
    CatalogItemDto,
    DeleteImpactPlanDto,
    DetailFieldDto,
    FixtureGuiDataProvider,
    ImportOperationDto,
    ImportPlanDto,
    PreviewHandleDto,
    ServiceBundleGuiDataProvider,
)
from ac6_data_manager.gui.services import (
    AuditService,
    CatalogService,
    DeleteImpactPlanService,
    ImportPlanService,
)

DEFAULT_DEV_ENV_KEYS = ("AC6DM_DEV_MODE", "AC6DM_GUI_MODE")
DEFAULT_APP_ROOT_SENTINEL = "__AUTO__"


def _is_truthy(value: str | None) -> bool:
    if value is None:
        return False
    return value.strip().lower() in {"1", "true", "yes", "on", "demo", "dev", "development"}


def _format_slot(slot_id: int | None) -> str:
    if slot_id is None:
        return "SHARE"
    return f"USER_EMBLEM_{slot_id:02d}"


def _make_record(*, category: int, ugc_id: str, creator_id: int) -> EmblemRecord:
    return EmblemRecord(
        category=category,
        ugc_id=ugc_id,
        creator_id=creator_id,
        datetime_block=DateTimeBlock.now(),
        image=EmblemImage(
            layers=(
                EmblemLayer(
                    group=EmblemGroup(
                        data=GroupData(
                            decal_id=100,
                            pos_x=0,
                            pos_y=0,
                            scale_x=512,
                            scale_y=512,
                            angle=0,
                            rgba=(255, 255, 255, 255),
                        )
                    )
                ),
            )
        ),
    )


def _build_release_assets() -> tuple[EmblemAsset, ...]:
    user_record = _make_record(category=1, ugc_id="USER-STEEL-01", creator_id=42)
    share_record = _make_record(category=9, ugc_id="SHARE-RAVEN-02", creator_id=77)
    return (
        EmblemAsset(
            asset_id="user:steel-haze-01",
            title="Steel Haze Mark",
            payload=user_record.serialize(),
            origin=EmblemOrigin.USER,
            storage_bucket="files",
            category=1,
            slot_id=7,
            creator_id="42",
            dependency_refs=("restore-point-required", "post-write-readback"),
        ),
        EmblemAsset(
            asset_id="share:raven-02",
            title="Raven Share Mark",
            payload=share_record.serialize(),
            origin=EmblemOrigin.SHARE,
            storage_bucket="extraFiles",
            category=9,
            creator_id="77",
            dependency_refs=("share-catalog", "stable-emblem-lane"),
        ),
    )


def build_demo_provider() -> FixtureGuiDataProvider:
    item = CatalogItemDto(
        asset_id="emblem-user-001",
        display_name="Steel Haze Mark",
        slot_id="USER_EMBLEM_07",
        origin="user",
        editable_state="editable",
        source_label="share/manual-pick",
        preview=PreviewHandleDto(
            label="preview",
            provenance="direct extract",
            source_hint="fixture/demo",
            image_bytes=None,
            note="演示预览图来自 fixture。",
        ),
        description="演示数据：展示 M1 徽章库详情、预览与风险状态。",
        tags=("favorite", "pvp", "manual-import"),
        dependency_references=("restore-point-required", "audit-visible"),
        detail_fields=(
            DetailFieldDto("作者", "fixture"),
            DetailFieldDto("导入通道", "share -> user"),
        ),
    )
    import_plan = ImportPlanDto(
        title="ImportPlan / Steel Haze Mark",
        source_asset_ids=(item.asset_id,),
        target_slot="USER_EMBLEM_07",
        mode="dry_run_then_apply",
        summary="先在 shadow workspace 中执行 dry-run，再生成 restore point、写回并做 post-write readback。",
        operations=(
            ImportOperationDto("选择分享徽章", "share catalog", "selected"),
            ImportOperationDto("槽位分配", "USER_EMBLEM_07", "compatible"),
            ImportOperationDto("写后回读", "readback", "required", "M1 stable lane hard gate"),
        ),
        warnings=("禁止 in-place overwrite", "失败时必须 fail-closed 并保留 incident artifact"),
    )
    delete_plan = DeleteImpactPlanDto(
        title="DeleteImpactPlan / Steel Haze Mark",
        asset_id=item.asset_id,
        summary="当前仅展示删除影响分析原型，不做真实删除。",
        impacted_assets=("贴花布局 A", "分享映射缓存"),
        warnings=("删除功能未开放", "需先人工确认依赖图"),
        risk_level="read_only",
        allowed=False,
    )
    audit = AuditEntryDto(
        timestamp="2026-04-14T06:30:00Z",
        action="emblem_import_dry_run",
        target=item.asset_id,
        result_status="dry_run_ready",
        summary="fixture 导入计划已生成，等待 Apply。",
        restore_point_id="rp-demo-001",
        details=("toolchain=WitchyBND", "shadow workspace prepared", "readback pending"),
    )
    return FixtureGuiDataProvider(
        catalog_items=(item,),
        import_plans_by_assets={(item.asset_id,): import_plan},
        delete_plans_by_asset={item.asset_id: delete_plan},
        audit_entries=(audit,),
    )


@dataclass(frozen=True)
class RuntimeMode:
    name: str
    uses_fixture_provider: bool
    reason: str


@dataclass(frozen=True)
class RuntimeContext:
    provider_kind: str
    provider_class: str
    runtime_mode: str
    demo_allowed: bool
    is_demo: bool
    reason: str
    app_root: str
    stable_lane: str = "emblem"
    toolchain_policy: str = "bundled-only"

    def to_dict(self) -> dict[str, object]:
        return {
            "provider_kind": self.provider_kind,
            "provider_class": self.provider_class,
            "runtime_mode": self.runtime_mode,
            "demo_allowed": self.demo_allowed,
            "is_demo": self.is_demo,
            "reason": self.reason,
            "app_root": self.app_root,
            "stable_lane": self.stable_lane,
            "toolchain_policy": self.toolchain_policy,
        }


@dataclass(frozen=True)
class ProviderBundle:
    provider: object
    context: RuntimeContext


class ReleaseCatalogService(CatalogService):
    def __init__(self) -> None:
        self.catalog = build_catalog(_build_release_assets())

    def list_catalog_items(self) -> Sequence[CatalogItemDto]:
        items: list[CatalogItemDto] = []
        for item in self.catalog.items:
            asset = self.catalog.assets_by_id[item.asset_id]
            preview = PreviewHandleDto(
                label="preview" if asset.preview_bytes else "No Preview",
                provenance="stable-emblem-service",
                image_bytes=asset.preview_bytes,
                source_hint=f"{asset.storage_bucket}:{asset.asset_id}",
                note="默认入口使用真实 stable-emblem service bundle。",
            )
            items.append(
                CatalogItemDto(
                    asset_id=asset.asset_id,
                    display_name=asset.title,
                    slot_id=_format_slot(asset.slot_id),
                    origin=asset.origin.value,
                    editable_state="editable" if item.editable else "read_only",
                    source_label=f"{asset.origin.value}/{asset.storage_bucket}",
                    preview=preview,
                    description="通过 stable-emblem 域模型构建的 release baseline 样本。",
                    tags=("stable-emblem", asset.origin.value),
                    dependency_references=tuple(asset.dependency_refs),
                    detail_fields=(
                        DetailFieldDto("ugc_id", asset.ugc_id),
                        DetailFieldDto("storage_bucket", asset.storage_bucket),
                        DetailFieldDto("category", str(asset.category)),
                    ),
                )
            )
        return tuple(items)


class ReleaseImportPlanService(ImportPlanService):
    def __init__(self, catalog_service: ReleaseCatalogService) -> None:
        self.catalog_service = catalog_service

    def build_import_plan(self, asset_ids: Sequence[str]) -> ImportPlanDto:
        try:
            plan = build_share_to_user_import_plan(
                self.catalog_service.catalog,
                selected_asset_ids=tuple(asset_ids),
            )
        except ValueError as error:
            return ImportPlanDto(
                title="ImportPlan / blocked",
                source_asset_ids=tuple(asset_ids),
                target_slot="TBD",
                mode="dry_run_then_apply",
                summary="当前选择不满足 share -> user 导入条件。",
                blockers=(str(error),),
                warnings=("仅支持 share emblem -> user 导入。",),
            )

        operations = tuple(
            ImportOperationDto(
                title="share -> user import",
                target=_format_slot(operation.target_slot_id),
                result="ready",
                note=f"source={operation.source_asset_id}",
            )
            for operation in plan.operations
        )
        target_slot = _format_slot(plan.operations[0].target_slot_id) if plan.operations else "TBD"
        warnings = tuple(plan.warnings) + (
            "发布链路必须先 dry-run，再 apply 到 fresh destination。",
            "post-write readback 是硬 gate。",
        )
        return ImportPlanDto(
            title="ImportPlan / share -> user",
            source_asset_ids=tuple(plan.selected_asset_ids),
            target_slot=target_slot,
            mode="dry_run_then_apply",
            summary="默认入口使用 stable-emblem 域计划器生成导入计划。",
            operations=operations,
            warnings=warnings,
        )


class ReleaseDeleteImpactPlanService(DeleteImpactPlanService):
    def build_delete_impact_plan(self, asset_id: str) -> DeleteImpactPlanDto:
        return DeleteImpactPlanDto(
            title=f"DeleteImpactPlan / {asset_id}",
            asset_id=asset_id,
            summary="M1.5 release lane 仍只开放只读删除影响分析。",
            impacted_assets=("audit trail", "restore-point requirement"),
            warnings=("删除功能未开放", "release lane 保持 fail-closed"),
            risk_level="read_only",
            allowed=False,
        )


class ReleaseAuditService(AuditService):
    def __init__(self, context: RuntimeContext) -> None:
        self.context = context

    def list_audit_entries(self) -> Sequence[AuditEntryDto]:
        return (
            AuditEntryDto(
                timestamp="2026-04-14T08:00:00Z",
                action="provider_bootstrap",
                target="runtime",
                result_status="release_service_bundle",
                summary="默认入口已绑定真实 stable-emblem provider。",
                details=(
                    f"provider_kind={self.context.provider_kind}",
                    f"runtime_mode={self.context.runtime_mode}",
                    f"toolchain_policy={self.context.toolchain_policy}",
                ),
            ),
            AuditEntryDto(
                timestamp="2026-04-14T08:00:01Z",
                action="stable_emblem_lane_ready",
                target="share->user",
                result_status="ready",
                summary="release lane 已准备 share emblem -> user import。",
                details=("dry_run_before_apply", "shadow_workspace", "post_write_readback"),
            ),
        )


def resolve_runtime_mode(
    argv: Sequence[str] | None = None,
    *,
    environ: Mapping[str, str] | None = None,
) -> RuntimeMode:
    arguments = tuple(argv or ())
    environment = dict(environ or os.environ)
    if "--demo" in arguments:
        return RuntimeMode(name="demo", uses_fixture_provider=True, reason="explicit --demo flag")
    for key in DEFAULT_DEV_ENV_KEYS:
        if _is_truthy(environment.get(key)):
            return RuntimeMode(
                name="development",
                uses_fixture_provider=True,
                reason=f"development env override via {key}",
            )
    return RuntimeMode(
        name="release",
        uses_fixture_provider=False,
        reason="default stable-emblem service bundle",
    )


def _resolve_app_root(app_root: str | Path | None) -> Path:
    if app_root is None or str(app_root) == DEFAULT_APP_ROOT_SENTINEL:
        return Path(__file__).resolve().parents[3]
    return Path(app_root)


def build_provider_bundle(
    argv: Sequence[str] | None = None,
    *,
    environ: Mapping[str, str] | None = None,
    app_root: str | Path | None = DEFAULT_APP_ROOT_SENTINEL,
) -> ProviderBundle:
    mode = resolve_runtime_mode(argv, environ=environ)
    resolved_app_root = _resolve_app_root(app_root)
    if mode.uses_fixture_provider:
        provider = build_demo_provider()
        context = RuntimeContext(
            provider_kind="fixture",
            provider_class=type(provider).__name__,
            runtime_mode=mode.name,
            demo_allowed=True,
            is_demo=True,
            reason=mode.reason,
            app_root=str(resolved_app_root),
        )
        return ProviderBundle(provider=provider, context=context)

    catalog_service = ReleaseCatalogService()
    context = RuntimeContext(
        provider_kind="service_bundle",
        provider_class=ServiceBundleGuiDataProvider.__name__,
        runtime_mode=mode.name,
        demo_allowed=False,
        is_demo=False,
        reason=mode.reason,
        app_root=str(resolved_app_root),
    )
    provider = ServiceBundleGuiDataProvider(
        catalog_service=catalog_service,
        import_plan_service=ReleaseImportPlanService(catalog_service),
        delete_impact_plan_service=ReleaseDeleteImpactPlanService(),
        audit_service=ReleaseAuditService(context),
    )
    return ProviderBundle(provider=provider, context=context)


def emit_provider_proof(
    output_path: Path | None,
    *,
    argv: Sequence[str] | None = None,
    environ: Mapping[str, str] | None = None,
    app_root: str | Path | None = DEFAULT_APP_ROOT_SENTINEL,
) -> dict[str, object]:
    bundle = build_provider_bundle(argv, environ=environ, app_root=app_root)
    payload = bundle.context.to_dict()
    rendered = json.dumps(payload, ensure_ascii=False, indent=2) + "\n"
    if output_path is None:
        return payload
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(rendered, encoding="utf-8")
    return payload
