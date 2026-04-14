from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable, Mapping

from ac6_data_manager.container_workspace.transaction import AuditEntry

REQUIRED_AUDIT_FIELDS = (
    "timestamp",
    "operation",
    "result_status",
    "workspace_id",
    "save_hash",
    "toolchain",
    "rule_set_version",
    "plan_summary",
)


class AuditValidationError(ValueError):
    """Raised when an audit or incident document is structurally invalid."""


@dataclass(frozen=True)
class AuditEvent:
    timestamp: str
    operation: str
    result_status: str
    workspace_id: str
    save_hash: str
    toolchain: dict[str, Any]
    rule_set_version: str
    plan_summary: str
    details: dict[str, Any]
    source_save: str | None = None
    output_save: str | None = None
    restore_point_id: str | None = None
    incident_bundle: str | None = None

    @classmethod
    def from_audit_entry(
        cls,
        entry: AuditEntry,
        *,
        source_save: str | Path | None = None,
        output_save: str | Path | None = None,
        restore_point_id: str | None = None,
        incident_bundle: str | Path | None = None,
    ) -> "AuditEvent":
        payload = entry.to_dict()
        return cls(
            timestamp=payload["timestamp"],
            operation=payload["operation"],
            result_status=payload["result_status"],
            workspace_id=payload["workspace_id"],
            save_hash=payload["save_hash"],
            toolchain=dict(payload["toolchain"]),
            rule_set_version=payload["rule_set_version"],
            plan_summary=payload["plan_summary"],
            details=dict(payload["details"]),
            source_save=None if source_save is None else str(source_save),
            output_save=None if output_save is None else str(output_save),
            restore_point_id=restore_point_id,
            incident_bundle=None if incident_bundle is None else str(incident_bundle),
        )

    @classmethod
    def from_dict(cls, payload: Mapping[str, Any]) -> "AuditEvent":
        missing = [field for field in REQUIRED_AUDIT_FIELDS if field not in payload]
        if missing:
            raise AuditValidationError(
                f"Audit event is missing required fields: {', '.join(missing)}"
            )
        return cls(
            timestamp=str(payload["timestamp"]),
            operation=str(payload["operation"]),
            result_status=str(payload["result_status"]),
            workspace_id=str(payload["workspace_id"]),
            save_hash=str(payload["save_hash"]),
            toolchain=dict(payload["toolchain"]),
            rule_set_version=str(payload["rule_set_version"]),
            plan_summary=str(payload["plan_summary"]),
            details=dict(payload.get("details") or {}),
            source_save=_string_or_none(payload.get("source_save")),
            output_save=_string_or_none(payload.get("output_save")),
            restore_point_id=_string_or_none(payload.get("restore_point_id")),
            incident_bundle=_string_or_none(payload.get("incident_bundle")),
        )

    def to_dict(self) -> dict[str, Any]:
        return {
            "timestamp": self.timestamp,
            "operation": self.operation,
            "result_status": self.result_status,
            "workspace_id": self.workspace_id,
            "save_hash": self.save_hash,
            "toolchain": dict(self.toolchain),
            "rule_set_version": self.rule_set_version,
            "plan_summary": self.plan_summary,
            "details": dict(self.details),
            "source_save": self.source_save,
            "output_save": self.output_save,
            "restore_point_id": self.restore_point_id,
            "incident_bundle": self.incident_bundle,
        }


class AuditJsonlWriter:
    def __init__(self, path: Path) -> None:
        self.path = Path(path)

    def append(self, event: AuditEvent | Mapping[str, Any]) -> AuditEvent:
        normalized = event if isinstance(event, AuditEvent) else AuditEvent.from_dict(event)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        with self.path.open("a", encoding="utf-8") as handle:
            handle.write(json.dumps(normalized.to_dict(), ensure_ascii=True) + "\n")
        return normalized


class AuditJsonlReader:
    def __init__(self, path: Path) -> None:
        self.path = Path(path)

    def iter_events(self) -> Iterable[AuditEvent]:
        if not self.path.exists():
            return ()
        events: list[AuditEvent] = []
        with self.path.open("r", encoding="utf-8") as handle:
            for line_number, line in enumerate(handle, start=1):
                stripped = line.strip()
                if not stripped:
                    continue
                try:
                    payload = json.loads(stripped)
                except json.JSONDecodeError as error:
                    raise AuditValidationError(
                        f"Invalid JSONL at {self.path}:{line_number}"
                    ) from error
                events.append(AuditEvent.from_dict(payload))
        return tuple(events)

    def read_all(self) -> tuple[AuditEvent, ...]:
        return tuple(self.iter_events())


@dataclass(frozen=True)
class IncidentBundleIndexEntry:
    incident_id: str
    captured_at: str
    workspace_id: str
    stage: str
    error: str
    plan_summary: str
    rule_set_version: str
    source_save: str
    incident_bundle: str
    toolchain: dict[str, Any]

    @classmethod
    def from_dict(cls, payload: Mapping[str, Any]) -> "IncidentBundleIndexEntry":
        required = (
            "incident_id",
            "captured_at",
            "workspace_id",
            "stage",
            "error",
            "plan_summary",
            "rule_set_version",
            "source_save",
            "incident_bundle",
            "toolchain",
        )
        missing = [field for field in required if field not in payload]
        if missing:
            raise AuditValidationError(
                f"Incident bundle index entry is missing required fields: {', '.join(missing)}"
            )
        return cls(
            incident_id=str(payload["incident_id"]),
            captured_at=str(payload["captured_at"]),
            workspace_id=str(payload["workspace_id"]),
            stage=str(payload["stage"]),
            error=str(payload["error"]),
            plan_summary=str(payload["plan_summary"]),
            rule_set_version=str(payload["rule_set_version"]),
            source_save=str(payload["source_save"]),
            incident_bundle=str(payload["incident_bundle"]),
            toolchain=dict(payload["toolchain"]),
        )

    def to_dict(self) -> dict[str, Any]:
        return {
            "incident_id": self.incident_id,
            "captured_at": self.captured_at,
            "workspace_id": self.workspace_id,
            "stage": self.stage,
            "error": self.error,
            "plan_summary": self.plan_summary,
            "rule_set_version": self.rule_set_version,
            "source_save": self.source_save,
            "incident_bundle": self.incident_bundle,
            "toolchain": dict(self.toolchain),
        }


class IncidentBundleIndex:
    def __init__(self, path: Path) -> None:
        self.path = Path(path)

    def append_from_bundle(self, incident_bundle: Path) -> IncidentBundleIndexEntry:
        incident_path = Path(incident_bundle)
        payload = _read_json_document(incident_path)
        entry = IncidentBundleIndexEntry.from_dict(
            {
                "incident_id": payload.get("incident_id"),
                "captured_at": payload.get("captured_at"),
                "workspace_id": payload.get("workspace_id"),
                "stage": payload.get("stage"),
                "error": payload.get("error"),
                "plan_summary": payload.get("plan_summary"),
                "rule_set_version": payload.get("rule_set_version"),
                "source_save": payload.get("source_save"),
                "incident_bundle": str(incident_path),
                "toolchain": payload.get("toolchain") or {},
            }
        )
        self.path.parent.mkdir(parents=True, exist_ok=True)
        with self.path.open("a", encoding="utf-8") as handle:
            handle.write(json.dumps(entry.to_dict(), ensure_ascii=True) + "\n")
        return entry

    def read_all(self) -> tuple[IncidentBundleIndexEntry, ...]:
        if not self.path.exists():
            return ()
        entries: list[IncidentBundleIndexEntry] = []
        with self.path.open("r", encoding="utf-8") as handle:
            for line_number, line in enumerate(handle, start=1):
                stripped = line.strip()
                if not stripped:
                    continue
                try:
                    payload = json.loads(stripped)
                except json.JSONDecodeError as error:
                    raise AuditValidationError(
                        f"Invalid JSONL at {self.path}:{line_number}"
                    ) from error
                entries.append(IncidentBundleIndexEntry.from_dict(payload))
        return tuple(entries)


def _read_json_document(path: Path) -> dict[str, Any]:
    try:
        return json.loads(Path(path).read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        raise AuditValidationError(f"Invalid incident bundle JSON: {path}") from error


def _string_or_none(value: Any) -> str | None:
    if value is None:
        return None
    return str(value)
