from __future__ import annotations

from dataclasses import dataclass, field
from typing import Sequence


@dataclass(frozen=True)
class PreviewHandleDto:
    label: str
    provenance: str
    image_bytes: bytes | None = None
    source_hint: str | None = None
    note: str | None = None


@dataclass(frozen=True)
class DetailFieldDto:
    label: str
    value: str


@dataclass(frozen=True)
class CatalogItemDto:
    asset_id: str
    display_name: str
    slot_id: str
    origin: str
    editable_state: str
    source_label: str
    preview: PreviewHandleDto
    description: str = ''
    tags: tuple[str, ...] = ()
    dependency_references: tuple[str, ...] = ()
    detail_fields: tuple[DetailFieldDto, ...] = ()

    @property
    def is_editable(self) -> bool:
        return self.editable_state.lower() in {'editable', 'yes', 'true', 'user'}


@dataclass(frozen=True)
class ImportOperationDto:
    title: str
    target: str
    result: str
    note: str = ''


@dataclass(frozen=True)
class ImportPlanDto:
    title: str
    source_asset_ids: tuple[str, ...]
    target_slot: str
    mode: str
    summary: str
    operations: tuple[ImportOperationDto, ...] = ()
    warnings: tuple[str, ...] = ()
    blockers: tuple[str, ...] = ()
    restore_point_required: bool = True
    post_write_readback_required: bool = True


@dataclass(frozen=True)
class DeleteImpactPlanDto:
    title: str
    asset_id: str
    summary: str
    impacted_assets: tuple[str, ...] = ()
    warnings: tuple[str, ...] = ()
    risk_level: str = 'read_only'
    allowed: bool = False


@dataclass(frozen=True)
class AuditEntryDto:
    timestamp: str
    action: str
    target: str
    result_status: str
    summary: str
    restore_point_id: str | None = None
    details: tuple[str, ...] = field(default_factory=tuple)


def ensure_tuple(values: Sequence[str] | None) -> tuple[str, ...]:
    return tuple(values or ())
