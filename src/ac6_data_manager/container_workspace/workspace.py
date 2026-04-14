from __future__ import annotations

import json
import secrets
import shutil
from dataclasses import dataclass
from datetime import UTC, datetime
from pathlib import Path
from typing import Any

from .adapter import ToolchainInfo, UNPACK_METADATA_FILENAME, sha256_file


def utc_now() -> datetime:
    return datetime.now(UTC)


def iso_now() -> str:
    return utc_now().replace(microsecond=0).isoformat().replace("+00:00", "Z")


def sanitize_component(value: str) -> str:
    safe = []
    for char in value:
        if char.isalnum() or char in {"-", "_", "."}:
            safe.append(char)
        else:
            safe.append("-")
    return "".join(safe).strip("-") or "item"


def make_workspace_id(prefix: str = "ws") -> str:
    timestamp = utc_now().strftime("%Y%m%dT%H%M%SZ")
    return f"{prefix}-{timestamp}-{secrets.token_hex(4)}"


def snapshot_directory_manifest(root: Path) -> dict[str, str]:
    manifest: dict[str, str] = {}
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        if path.name == UNPACK_METADATA_FILENAME:
            continue
        manifest[path.relative_to(root).as_posix()] = sha256_file(path)
    return manifest


@dataclass(frozen=True)
class WorkspaceLayout:
    workspace_id: str
    root: Path
    shadow_root: Path
    shadow_container: Path
    unpacked_root: Path
    readback_root: Path
    restore_points_root: Path
    incidents_root: Path


@dataclass(frozen=True)
class RestorePoint:
    restore_point_id: str
    root: Path
    backup_path: Path
    manifest_path: Path
    created_at: str
    source_sha256: str


class SaveWorkspace:
    def __init__(
        self,
        source_save: Path,
        working_root: Path,
        *,
        workspace_id: str | None = None,
    ) -> None:
        self.source_save = Path(source_save)
        self.working_root = Path(working_root)
        resolved_workspace_id = workspace_id or make_workspace_id()
        root = self.working_root / resolved_workspace_id
        shadow_root = root / "shadow"
        self.layout = WorkspaceLayout(
            workspace_id=resolved_workspace_id,
            root=root,
            shadow_root=shadow_root,
            shadow_container=shadow_root / "input" / self.source_save.name,
            unpacked_root=shadow_root / "unpacked",
            readback_root=root / "readback",
            restore_points_root=root / "restore-points",
            incidents_root=root / "incidents",
        )

    def initialize(self) -> WorkspaceLayout:
        self.layout.shadow_container.parent.mkdir(parents=True, exist_ok=True)
        self.layout.unpacked_root.parent.mkdir(parents=True, exist_ok=True)
        self.layout.restore_points_root.mkdir(parents=True, exist_ok=True)
        self.layout.incidents_root.mkdir(parents=True, exist_ok=True)
        return self.layout

    def prepare_shadow_copy(self) -> Path:
        if not self.source_save.exists():
            raise FileNotFoundError(f"Source save does not exist: {self.source_save}")
        if not self.source_save.is_file():
            raise FileNotFoundError(f"Source save is not a file: {self.source_save}")
        self.initialize()
        shutil.copy2(self.source_save, self.layout.shadow_container)
        return self.layout.shadow_container

    def create_restore_point(
        self,
        *,
        plan_summary: str,
        rule_set_version: str,
        toolchain: ToolchainInfo,
        source_hash: str | None = None,
        metadata: dict[str, Any] | None = None,
    ) -> RestorePoint:
        self.initialize()
        timestamp = utc_now().strftime("%Y%m%dT%H%M%SZ")
        restore_point_id = (
            f"rp-{sanitize_component(self.source_save.stem)}-"
            f"{timestamp}-{self.layout.workspace_id[-8:]}"
        )
        restore_root = self.layout.restore_points_root / restore_point_id
        backup_root = restore_root / "original"
        backup_root.mkdir(parents=True, exist_ok=True)
        backup_path = backup_root / self.source_save.name
        shutil.copy2(self.source_save, backup_path)
        source_sha256 = source_hash or sha256_file(self.source_save)
        manifest_path = restore_root / "manifest.json"
        created_at = iso_now()
        payload = {
            "restore_point_id": restore_point_id,
            "created_at": created_at,
            "workspace_id": self.layout.workspace_id,
            "source_save": str(self.source_save),
            "backup_path": str(backup_path),
            "source_sha256": source_sha256,
            "backup_sha256": sha256_file(backup_path),
            "rule_set_version": rule_set_version,
            "toolchain": {
                "name": toolchain.name,
                "executable": str(toolchain.executable),
                "version": toolchain.version,
                "executable_sha256": toolchain.executable_sha256,
            },
            "plan_summary": plan_summary,
            "metadata": metadata or {},
        }
        manifest_path.write_text(
            json.dumps(payload, ensure_ascii=True, indent=2) + "\n",
            encoding="utf-8",
        )
        return RestorePoint(
            restore_point_id=restore_point_id,
            root=restore_root,
            backup_path=backup_path,
            manifest_path=manifest_path,
            created_at=created_at,
            source_sha256=source_sha256,
        )

    def capture_incident(
        self,
        *,
        stage: str,
        error: str,
        plan_summary: str,
        rule_set_version: str,
        toolchain: ToolchainInfo,
        payload: dict[str, Any],
    ) -> Path:
        self.initialize()
        incident_id = (
            f"incident-{utc_now().strftime('%Y%m%dT%H%M%SZ')}-"
            f"{self.layout.workspace_id[-8:]}"
        )
        incident_root = self.layout.incidents_root / incident_id
        incident_root.mkdir(parents=True, exist_ok=True)
        incident_path = incident_root / "incident.json"
        document = {
            "incident_id": incident_id,
            "captured_at": iso_now(),
            "workspace_id": self.layout.workspace_id,
            "stage": stage,
            "error": error,
            "plan_summary": plan_summary,
            "rule_set_version": rule_set_version,
            "toolchain": {
                "name": toolchain.name,
                "executable": str(toolchain.executable),
                "version": toolchain.version,
                "executable_sha256": toolchain.executable_sha256,
            },
            "source_save": str(self.source_save),
            "shadow_container": str(self.layout.shadow_container),
            "payload": payload,
        }
        incident_path.write_text(
            json.dumps(document, ensure_ascii=True, indent=2) + "\n",
            encoding="utf-8",
        )
        return incident_path

    def copy_shadow_to_output(self, output_save: Path) -> Path:
        output_save = Path(output_save)
        if output_save.resolve() == self.source_save.resolve():
            raise ValueError(
                "In-place overwrite is forbidden; output_save must differ "
                "from source_save"
            )
        output_save.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(self.layout.shadow_container, output_save)
        return output_save


@dataclass(frozen=True)
class ManifestDelta:
    created: tuple[str, ...]
    updated: tuple[str, ...]
    deleted: tuple[str, ...]
    before: dict[str, str]
    after: dict[str, str]


def diff_manifests(before: dict[str, str], after: dict[str, str]) -> ManifestDelta:
    before_keys = set(before)
    after_keys = set(after)
    created = tuple(sorted(after_keys - before_keys))
    deleted = tuple(sorted(before_keys - after_keys))
    updated = tuple(
        sorted(path for path in before_keys & after_keys if before[path] != after[path])
    )
    return ManifestDelta(
        created=created,
        updated=updated,
        deleted=deleted,
        before=dict(before),
        after=dict(after),
    )
