from __future__ import annotations

from dataclasses import dataclass, field, replace
from enum import Enum
from hashlib import sha256


class EmblemOrigin(str, Enum):
    USER = "user"
    SHARE = "share"
    PACKAGE = "package"


class PreviewKind(str, Enum):
    PNG = "png"
    BINARY = "binary"
    NONE = "none"


def sha256_hex(payload: bytes) -> str:
    return sha256(payload).hexdigest()


@dataclass(frozen=True)
class PreviewHandle:
    kind: PreviewKind
    sha256: str | None
    media_type: str | None
    size_bytes: int = 0


@dataclass(frozen=True)
class EmblemAsset:
    asset_id: str
    title: str
    payload: bytes
    origin: EmblemOrigin
    storage_bucket: str
    category: int
    slot_id: int | None = None
    ugc_id: str = ""
    creator_id: str | None = None
    created_at: str | None = None
    preview_bytes: bytes | None = None
    dependency_refs: tuple[str, ...] = ()

    @property
    def payload_sha256(self) -> str:
        return sha256_hex(self.payload)

    @property
    def preview_sha256(self) -> str | None:
        if self.preview_bytes is None:
            return None
        return sha256_hex(self.preview_bytes)

    @property
    def editable(self) -> bool:
        return self.origin is EmblemOrigin.USER

    def as_user_copy(
        self,
        *,
        slot_id: int,
        category: int = 2,
        asset_id: str | None = None,
    ) -> "EmblemAsset":
        return replace(
            self,
            asset_id=asset_id or self.asset_id,
            origin=EmblemOrigin.USER,
            storage_bucket="files",
            slot_id=slot_id,
            category=category,
        )


@dataclass(frozen=True)
class CatalogItem:
    asset_id: str
    title: str
    slot_id: int | None
    origin: EmblemOrigin
    editable: bool
    preview: PreviewHandle
    payload_sha256: str
    dependency_refs: tuple[str, ...] = ()


@dataclass(frozen=True)
class PlannedImport:
    source_asset_id: str
    source_origin: EmblemOrigin
    source_slot_id: int | None
    target_slot_id: int
    target_category: int
    action: str = "copy_share_to_user"


@dataclass(frozen=True)
class ImportPlan:
    selected_asset_ids: tuple[str, ...]
    operations: tuple[PlannedImport, ...]
    warnings: tuple[str, ...] = ()
    compatibility: str = "emblem-stable"
    dry_run_required: bool = True
    shadow_workspace_required: bool = True
    post_write_readback_required: bool = True


@dataclass(frozen=True)
class PackageV1:
    manifest: dict[str, object]
    assets: tuple[EmblemAsset, ...]
    raw_bytes: bytes = field(repr=False)
