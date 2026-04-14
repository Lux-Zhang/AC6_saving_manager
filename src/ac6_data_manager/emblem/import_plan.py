from __future__ import annotations

from dataclasses import dataclass

from .catalog import CatalogBuildResult, build_catalog
from .models import EmblemAsset, EmblemOrigin, ImportPlan, PlannedImport


@dataclass(frozen=True)
class SlotAllocator:
    max_slots: int

    def allocate(
        self,
        *,
        occupied_slots: set[int] | frozenset[int],
        count: int,
    ) -> tuple[int, ...]:
        available = [
            slot
            for slot in range(self.max_slots)
            if slot not in occupied_slots
        ]
        if len(available) < count:
            raise ValueError(
                "no free user emblem slots available for the selected share emblems"
            )
        return tuple(available[:count])


def build_share_to_user_import_plan(
    catalog: CatalogBuildResult,
    *,
    selected_asset_ids: list[str] | tuple[str, ...],
    max_user_slots: int = 128,
) -> ImportPlan:
    if not selected_asset_ids:
        raise ValueError("manual share import requires at least one selected asset")
    if len(set(selected_asset_ids)) != len(selected_asset_ids):
        raise ValueError("manual share import cannot contain duplicate selections")

    selected_assets: list[EmblemAsset] = []
    for asset_id in selected_asset_ids:
        asset = catalog.assets_by_id.get(asset_id)
        if asset is None:
            raise ValueError(f"selected asset not found in catalog: {asset_id}")
        if asset.origin is not EmblemOrigin.SHARE:
            raise ValueError(
                f"share -> user import only accepts share emblems: {asset_id}"
            )
        selected_assets.append(asset)

    allocator = SlotAllocator(max_slots=max_user_slots)
    target_slots = allocator.allocate(
        occupied_slots=catalog.user_slots_in_use,
        count=len(selected_assets),
    )

    operations = tuple(
        PlannedImport(
            source_asset_id=asset.asset_id,
            source_origin=asset.origin,
            source_slot_id=asset.slot_id,
            target_slot_id=target_slot,
            target_category=2,
        )
        for asset, target_slot in zip(selected_assets, target_slots, strict=True)
    )

    warnings: list[str] = []
    for asset in selected_assets:
        if any(
            candidate.origin is EmblemOrigin.USER
            and candidate.payload_sha256 == asset.payload_sha256
            for candidate in catalog.assets_by_id.values()
        ):
            warnings.append(
                f"share emblem already exists in user library by payload hash: {asset.asset_id}"
            )

    return ImportPlan(
        selected_asset_ids=tuple(selected_asset_ids),
        operations=operations,
        warnings=tuple(warnings),
    )


def apply_share_to_user_import_plan(
    catalog: CatalogBuildResult,
    plan: ImportPlan,
) -> CatalogBuildResult:
    if not plan.operations:
        return catalog

    occupied_slots = set(catalog.user_slots_in_use)
    new_assets = list(catalog.assets_by_id.values())

    for operation in plan.operations:
        if operation.target_slot_id in occupied_slots:
            raise ValueError(
                f"target slot already occupied during simulated apply: {operation.target_slot_id}"
            )

        source = catalog.assets_by_id[operation.source_asset_id]
        if source.origin is not EmblemOrigin.SHARE:
            raise ValueError(
                f"simulated apply only accepts share sources: {operation.source_asset_id}"
            )

        new_assets.append(
            source.as_user_copy(
                asset_id=f"{source.asset_id}#user-slot-{operation.target_slot_id}",
                slot_id=operation.target_slot_id,
                category=operation.target_category,
            )
        )
        occupied_slots.add(operation.target_slot_id)

    return build_catalog(new_assets)
