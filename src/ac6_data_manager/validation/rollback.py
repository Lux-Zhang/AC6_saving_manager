from __future__ import annotations

import json
import shutil
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Mapping

from ac6_data_manager.container_workspace.adapter import sha256_file
from ac6_data_manager.container_workspace.workspace import iso_now

from .audit import AuditEvent, AuditJsonlWriter


class RollbackError(RuntimeError):
    """Raised when a restore point cannot be restored safely."""


@dataclass(frozen=True)
class RestorePointManifest:
    restore_point_id: str
    created_at: str
    workspace_id: str
    source_save: Path
    backup_path: Path
    source_sha256: str
    backup_sha256: str
    rule_set_version: str
    toolchain: dict[str, Any]
    plan_summary: str
    metadata: dict[str, Any]
    manifest_path: Path

    @classmethod
    def load(cls, manifest_path: Path) -> "RestorePointManifest":
        payload = json.loads(Path(manifest_path).read_text(encoding="utf-8"))
        return cls.from_dict(payload, manifest_path=manifest_path)

    @classmethod
    def from_dict(
        cls,
        payload: Mapping[str, Any],
        *,
        manifest_path: Path,
    ) -> "RestorePointManifest":
        return cls(
            restore_point_id=str(payload["restore_point_id"]),
            created_at=str(payload["created_at"]),
            workspace_id=str(payload["workspace_id"]),
            source_save=Path(payload["source_save"]),
            backup_path=Path(payload["backup_path"]),
            source_sha256=str(payload["source_sha256"]),
            backup_sha256=str(payload["backup_sha256"]),
            rule_set_version=str(payload["rule_set_version"]),
            toolchain=dict(payload.get("toolchain") or {}),
            plan_summary=str(payload["plan_summary"]),
            metadata=dict(payload.get("metadata") or {}),
            manifest_path=Path(manifest_path),
        )


@dataclass(frozen=True)
class RollbackResult:
    restore_point_id: str
    workspace_id: str
    backup_path: Path
    output_path: Path
    restored_sha256: str
    source_sha256: str
    backup_sha256: str
    audit_event: AuditEvent | None


class RollbackService:
    def __init__(self, *, audit_writer: AuditJsonlWriter | None = None) -> None:
        self.audit_writer = audit_writer

    def restore(
        self,
        restore_point_manifest: Path | RestorePointManifest,
        *,
        output_path: Path,
        plan_summary: str | None = None,
    ) -> RollbackResult:
        manifest = (
            restore_point_manifest
            if isinstance(restore_point_manifest, RestorePointManifest)
            else RestorePointManifest.load(Path(restore_point_manifest))
        )
        output_path = Path(output_path)
        source_resolved = manifest.source_save.resolve()
        backup_resolved = manifest.backup_path.resolve()
        output_resolved = output_path.resolve()

        if output_resolved == source_resolved:
            raise RollbackError(
                "In-place rollback is forbidden; output_path must differ from source_save"
            )
        if output_resolved == backup_resolved:
            raise RollbackError(
                "Rollback output_path must differ from restore-point backup_path"
            )
        if output_path.exists():
            raise RollbackError(
                "Rollback output_path already exists; choose a fresh destination"
            )
        if not manifest.backup_path.exists():
            raise RollbackError(
                f"Restore-point backup is missing: {manifest.backup_path}"
            )

        current_backup_hash = sha256_file(manifest.backup_path)
        if current_backup_hash != manifest.backup_sha256:
            raise RollbackError(
                "Restore-point backup hash mismatch; refusing rollback"
            )

        output_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(manifest.backup_path, output_path)
        restored_sha256 = sha256_file(output_path)
        if restored_sha256 != manifest.backup_sha256:
            raise RollbackError("Rollback output hash mismatch after copy")

        audit_event = None
        if self.audit_writer is not None:
            audit_event = self.audit_writer.append(
                AuditEvent(
                    timestamp=iso_now(),
                    operation="rollback",
                    result_status="rolled_back",
                    workspace_id=manifest.workspace_id,
                    save_hash=manifest.source_sha256,
                    toolchain=dict(manifest.toolchain),
                    rule_set_version=manifest.rule_set_version,
                    plan_summary=plan_summary or f"rollback {manifest.restore_point_id}",
                    details={
                        "restore_point_id": manifest.restore_point_id,
                        "backup_path": str(manifest.backup_path),
                        "manifest_path": str(manifest.manifest_path),
                    },
                    source_save=str(manifest.source_save),
                    output_save=str(output_path),
                    restore_point_id=manifest.restore_point_id,
                )
            )

        return RollbackResult(
            restore_point_id=manifest.restore_point_id,
            workspace_id=manifest.workspace_id,
            backup_path=manifest.backup_path,
            output_path=output_path,
            restored_sha256=restored_sha256,
            source_sha256=manifest.source_sha256,
            backup_sha256=manifest.backup_sha256,
            audit_event=audit_event,
        )
