from __future__ import annotations

import json
import shutil
from dataclasses import dataclass
from enum import StrEnum
from pathlib import Path
from typing import Any

from ac6_data_manager.container_workspace.adapter import sha256_file
from ac6_data_manager.container_workspace.workspace import iso_now, sanitize_component

from .audit import AuditEvent, AuditJsonlReader, AuditJsonlWriter
from .rollback import RollbackError, RollbackService


class AuditAction(StrEnum):
    APPLY = "apply"
    ROLLBACK = "rollback"
    BLOCKED_ACTION = "blocked_action"


class AuditStatus(StrEnum):
    APPLIED = "applied"
    ROLLED_BACK = "rolled_back"
    BLOCKED = "blocked"
    FAILED = "failed"


@dataclass(frozen=True)
class LegacyAuditEntry:
    timestamp: str
    action: AuditAction
    result_status: AuditStatus
    save_hash: str
    workspace_id: str
    toolchain_version: str
    rule_set_version: str
    plan_summary: str
    details: dict[str, Any]
    incident_bundle: str | None = None


class AuditLog:
    def __init__(self, path: Path) -> None:
        self.path = Path(path)
        self._writer = AuditJsonlWriter(self.path)
        self._reader = AuditJsonlReader(self.path)

    def append(self, entry: LegacyAuditEntry) -> LegacyAuditEntry:
        self._writer.append(
            AuditEvent(
                timestamp=entry.timestamp,
                operation=entry.action.value,
                result_status=entry.result_status.value,
                workspace_id=entry.workspace_id,
                save_hash=entry.save_hash,
                toolchain={"version": entry.toolchain_version},
                rule_set_version=entry.rule_set_version,
                plan_summary=entry.plan_summary,
                details=dict(entry.details),
                incident_bundle=entry.incident_bundle,
            )
        )
        return entry

    def read_all(self) -> tuple[LegacyAuditEntry, ...]:
        events = self._reader.read_all()
        items: list[LegacyAuditEntry] = []
        for event in events:
            action_value = (
                AuditAction.BLOCKED_ACTION
                if event.operation == "blocked_action"
                else AuditAction(event.operation)
            )
            status_value = AuditStatus(event.result_status)
            items.append(
                LegacyAuditEntry(
                    timestamp=event.timestamp,
                    action=action_value,
                    result_status=status_value,
                    save_hash=event.save_hash,
                    workspace_id=event.workspace_id,
                    toolchain_version=str(event.toolchain.get("version", "")),
                    rule_set_version=event.rule_set_version,
                    plan_summary=event.plan_summary,
                    details=dict(event.details),
                    incident_bundle=event.incident_bundle,
                )
            )
        return tuple(items)


@dataclass(frozen=True)
class LegacyRestorePoint:
    restore_point_id: str
    source_path: str
    backup_path: str
    manifest_path: str
    workspace_id: str
    description: str
    source_sha256: str
    backup_sha256: str


def create_restore_point(
    target_path: str | Path,
    restore_points_root: str | Path,
    *,
    workspace_id: str,
    description: str,
) -> LegacyRestorePoint:
    source = Path(target_path)
    restore_points_root = Path(restore_points_root)
    restore_point_id = (
        f"rp-{sanitize_component(source.stem)}-"
        f"{iso_now().replace(':', '').replace('-', '')}"
    )
    restore_root = restore_points_root / restore_point_id
    backup_root = restore_root / "original"
    backup_root.mkdir(parents=True, exist_ok=True)
    backup_path = backup_root / source.name
    shutil.copy2(source, backup_path)
    source_sha = sha256_file(source)
    backup_sha = sha256_file(backup_path)
    manifest_path = restore_root / "manifest.json"
    manifest_path.write_text(
        json.dumps(
            {
                "restore_point_id": restore_point_id,
                "created_at": iso_now(),
                "workspace_id": workspace_id,
                "source_save": str(source),
                "backup_path": str(backup_path),
                "source_sha256": source_sha,
                "backup_sha256": backup_sha,
                "rule_set_version": "legacy-compat",
                "toolchain": {},
                "plan_summary": description,
                "metadata": {"description": description},
            },
            ensure_ascii=True,
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    return LegacyRestorePoint(
        restore_point_id=restore_point_id,
        source_path=str(source),
        backup_path=str(backup_path),
        manifest_path=str(manifest_path),
        workspace_id=workspace_id,
        description=description,
        source_sha256=source_sha,
        backup_sha256=backup_sha,
    )


def record_blocked_action(
    audit_log: AuditLog,
    *,
    save_hash: str,
    workspace_id: str,
    toolchain_version: str,
    rule_set_version: str,
    plan_summary: str,
    details: dict[str, Any],
) -> LegacyAuditEntry:
    entry = LegacyAuditEntry(
        timestamp=iso_now(),
        action=AuditAction.BLOCKED_ACTION,
        result_status=AuditStatus.BLOCKED,
        save_hash=save_hash,
        workspace_id=workspace_id,
        toolchain_version=toolchain_version,
        rule_set_version=rule_set_version,
        plan_summary=plan_summary,
        details=dict(details),
    )
    return audit_log.append(entry)


class IncidentBundleWriter:
    def __init__(self, root: str | Path) -> None:
        self.root = Path(root)

    def write(
        self,
        *,
        workspace_id: str,
        stage: str,
        error: str,
        plan_summary: str,
        rule_set_version: str,
        source_save: str,
        files: dict[str, str],
    ) -> str:
        bundle_root = self.root / f"incident-{sanitize_component(workspace_id)}-{iso_now().replace(':', '').replace('-', '')}"
        bundle_root.mkdir(parents=True, exist_ok=True)
        incident_manifest = {
            "incident_id": bundle_root.name,
            "captured_at": iso_now(),
            "workspace_id": workspace_id,
            "stage": stage,
            "error": error,
            "plan_summary": plan_summary,
            "rule_set_version": rule_set_version,
            "source_save": source_save,
            "files": {},
        }
        for filename, content in files.items():
            candidate = bundle_root / filename
            candidate.parent.mkdir(parents=True, exist_ok=True)
            candidate.write_text(content, encoding="utf-8")
            incident_manifest["files"][filename] = str(candidate)
        (bundle_root / "incident.json").write_text(
            json.dumps(incident_manifest, ensure_ascii=True, indent=2) + "\n",
            encoding="utf-8",
        )
        return str(bundle_root)


@dataclass(frozen=True)
class RollbackRequest:
    target_path: str
    restore_point: LegacyRestorePoint
    reason: str
    toolchain_version: str
    rule_set_version: str
    plan_summary: str


@dataclass(frozen=True)
class RollbackExecutionResult:
    restored: bool
    incident_bundle: str | None = None


class RollbackCoordinator:
    def __init__(
        self,
        *,
        audit_log: AuditLog,
        incident_writer: IncidentBundleWriter,
    ) -> None:
        self.audit_log = audit_log
        self.incident_writer = incident_writer

    def execute(self, request: RollbackRequest) -> RollbackExecutionResult:
        manifest_path = Path(request.restore_point.manifest_path)
        output_path = Path(request.target_path)
        try:
            service = RollbackService()
            service.restore(manifest_path, output_path=output_path)
        except RollbackError as error:
            incident_bundle = self.incident_writer.write(
                workspace_id=request.restore_point.workspace_id,
                stage="rollback",
                error=str(error),
                plan_summary=request.plan_summary,
                rule_set_version=request.rule_set_version,
                source_save=request.restore_point.source_path,
                files={
                    "rollback_request.json": json.dumps(
                        {
                            "target_path": request.target_path,
                            "reason": request.reason,
                            "restore_point_id": request.restore_point.restore_point_id,
                        },
                        ensure_ascii=True,
                        indent=2,
                    )
                    + "\n",
                    "restore_point_manifest.json": manifest_path.read_text(
                        encoding="utf-8"
                    ),
                },
            )
            self.audit_log.append(
                LegacyAuditEntry(
                    timestamp=iso_now(),
                    action=AuditAction.ROLLBACK,
                    result_status=AuditStatus.FAILED,
                    save_hash=request.restore_point.source_sha256,
                    workspace_id=request.restore_point.workspace_id,
                    toolchain_version=request.toolchain_version,
                    rule_set_version=request.rule_set_version,
                    plan_summary=request.plan_summary,
                    details={
                        "restore_point_id": request.restore_point.restore_point_id,
                        "reason": request.reason,
                        "error": str(error),
                    },
                    incident_bundle=incident_bundle,
                )
            )
            return RollbackExecutionResult(restored=False, incident_bundle=incident_bundle)

        self.audit_log.append(
            LegacyAuditEntry(
                timestamp=iso_now(),
                action=AuditAction.ROLLBACK,
                result_status=AuditStatus.ROLLED_BACK,
                save_hash=request.restore_point.source_sha256,
                workspace_id=request.restore_point.workspace_id,
                toolchain_version=request.toolchain_version,
                rule_set_version=request.rule_set_version,
                plan_summary=request.plan_summary,
                details={
                    "restore_point_id": request.restore_point.restore_point_id,
                    "reason": request.reason,
                },
            )
        )
        return RollbackExecutionResult(restored=True, incident_bundle=None)
