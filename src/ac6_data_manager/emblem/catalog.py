from __future__ import annotations

from dataclasses import dataclass

from .binary import (
    EMBLEM_IMPORTED_USER_CATEGORY,
    EMBLEM_USER_CATEGORY,
    EmblemRecord,
    UserDataContainer,
    iter_emblem_selections,
)
from .models import CatalogItem, EmblemAsset, EmblemOrigin, PreviewHandle, PreviewKind
from .preview import extract_preview_handle


@dataclass(frozen=True)
class EmblemCatalogItem:
    asset_id: str
    source_bucket: str
    file_index: int
    record: EmblemRecord
    editable: bool
    preview_provenance: str


@dataclass(frozen=True)
class EmblemCatalog:
    items: tuple[EmblemCatalogItem, ...]

    def by_asset_id(self) -> dict[str, EmblemCatalogItem]:
        return {item.asset_id: item for item in self.items}


USER_EDITABLE_CATEGORIES = {EMBLEM_USER_CATEGORY, EMBLEM_IMPORTED_USER_CATEGORY}


class EmblemCatalogBuilder:
    def build(self, container: UserDataContainer) -> EmblemCatalog:
        items: list[EmblemCatalogItem] = []
        for selection in iter_emblem_selections(container):
            editable = selection.source_bucket == "files" and selection.record.category in USER_EDITABLE_CATEGORIES
            items.append(
                EmblemCatalogItem(
                    asset_id=f"{selection.source_bucket}:{selection.file_index}",
                    source_bucket=selection.source_bucket,
                    file_index=selection.file_index,
                    record=selection.record,
                    editable=editable,
                    preview_provenance="pil-basic-shapes-v1",
                )
            )
        return EmblemCatalog(items=tuple(items))


@dataclass(frozen=True)
class CatalogBuildResult:
    items: tuple[CatalogItem, ...]
    assets_by_id: dict[str, EmblemAsset]
    user_slots_in_use: frozenset[int]



def _catalog_sort_key(item: CatalogItem) -> tuple[int, int, str]:
    if item.origin is EmblemOrigin.USER:
        return (0, item.slot_id if item.slot_id is not None else 10_000, item.asset_id)
    return (1, item.slot_id if item.slot_id is not None else 10_000, item.asset_id)



def build_catalog(
    assets: list[EmblemAsset] | tuple[EmblemAsset, ...],
) -> CatalogBuildResult:
    assets_by_id: dict[str, EmblemAsset] = {}
    used_slots: set[int] = set()
    items: list[CatalogItem] = []
    for asset in assets:
        if asset.asset_id in assets_by_id:
            raise ValueError(f"duplicate emblem asset id: {asset.asset_id}")
        if asset.origin is EmblemOrigin.USER and asset.slot_id is not None:
            if asset.slot_id in used_slots:
                raise ValueError(f"duplicate user slot detected: {asset.slot_id}")
            used_slots.add(asset.slot_id)
        preview = extract_preview_handle(asset.preview_bytes)
        items.append(
            CatalogItem(
                asset_id=asset.asset_id,
                title=asset.title,
                slot_id=asset.slot_id,
                origin=asset.origin,
                editable=asset.editable,
                preview=preview,
                payload_sha256=asset.payload_sha256,
                dependency_refs=asset.dependency_refs,
            )
        )
        assets_by_id[asset.asset_id] = asset
    items.sort(key=_catalog_sort_key)
    return CatalogBuildResult(
        items=tuple(items),
        assets_by_id=assets_by_id,
        user_slots_in_use=frozenset(used_slots),
    )
