from __future__ import annotations

from dataclasses import dataclass, field
from typing import Mapping, Protocol, Sequence

from .dto import AuditEntryDto, CatalogItemDto, DeleteImpactPlanDto, ImportPlanDto


class CatalogService(Protocol):
    def list_catalog_items(self) -> Sequence[CatalogItemDto]: ...


class ImportPlanService(Protocol):
    def build_import_plan(self, asset_ids: Sequence[str]) -> ImportPlanDto: ...


class DeleteImpactPlanService(Protocol):
    def build_delete_impact_plan(self, asset_id: str) -> DeleteImpactPlanDto: ...


class AuditService(Protocol):
    def list_audit_entries(self) -> Sequence[AuditEntryDto]: ...


class GuiDataProvider(Protocol):
    def get_catalog(self) -> Sequence[CatalogItemDto]: ...

    def get_import_plan(self, asset_ids: Sequence[str]) -> ImportPlanDto: ...

    def get_delete_impact_plan(self, asset_id: str) -> DeleteImpactPlanDto: ...

    def get_audit_entries(self) -> Sequence[AuditEntryDto]: ...


@dataclass
class ServiceBundleGuiDataProvider:
    catalog_service: CatalogService
    import_plan_service: ImportPlanService
    delete_impact_plan_service: DeleteImpactPlanService
    audit_service: AuditService

    def get_catalog(self) -> Sequence[CatalogItemDto]:
        return tuple(self.catalog_service.list_catalog_items())

    def get_import_plan(self, asset_ids: Sequence[str]) -> ImportPlanDto:
        return self.import_plan_service.build_import_plan(tuple(asset_ids))

    def get_delete_impact_plan(self, asset_id: str) -> DeleteImpactPlanDto:
        return self.delete_impact_plan_service.build_delete_impact_plan(asset_id)

    def get_audit_entries(self) -> Sequence[AuditEntryDto]:
        return tuple(self.audit_service.list_audit_entries())


@dataclass
class FixtureGuiDataProvider:
    catalog_items: Sequence[CatalogItemDto] = field(default_factory=tuple)
    import_plans_by_assets: Mapping[tuple[str, ...], ImportPlanDto] = field(default_factory=dict)
    delete_plans_by_asset: Mapping[str, DeleteImpactPlanDto] = field(default_factory=dict)
    audit_entries: Sequence[AuditEntryDto] = field(default_factory=tuple)

    def get_catalog(self) -> Sequence[CatalogItemDto]:
        return tuple(self.catalog_items)

    def get_import_plan(self, asset_ids: Sequence[str]) -> ImportPlanDto:
        key = tuple(asset_ids)
        if key in self.import_plans_by_assets:
            return self.import_plans_by_assets[key]
        singleton = tuple(asset_ids[:1])
        if singleton in self.import_plans_by_assets:
            return self.import_plans_by_assets[singleton]
        return ImportPlanDto(
            title='ImportPlan',
            source_asset_ids=key,
            target_slot='TBD',
            mode='dry_run_only',
            summary='未提供计划数据；当前仅展示空白导入计划壳层。',
            blockers=('缺少注入的 ImportPlanDto',),
        )

    def get_delete_impact_plan(self, asset_id: str) -> DeleteImpactPlanDto:
        if asset_id in self.delete_plans_by_asset:
            return self.delete_plans_by_asset[asset_id]
        return DeleteImpactPlanDto(
            title='DeleteImpactPlan',
            asset_id=asset_id,
            summary='未提供删除影响分析数据；当前仅允许只读查看。',
            warnings=('缺少注入的 DeleteImpactPlanDto',),
            risk_level='unknown',
            allowed=False,
        )

    def get_audit_entries(self) -> Sequence[AuditEntryDto]:
        return tuple(self.audit_entries)
