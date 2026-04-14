from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable, Iterable

from .adapter import ContainerAdapter, ToolchainInfo, sha256_file
from .workspace import (
    ManifestDelta,
    RestorePoint,
    SaveWorkspace,
    diff_manifests,
    iso_now,
    snapshot_directory_manifest,
)

MutationFn = Callable[[Path], Iterable[str | Path] | None]


def _normalize_tracked_paths(
    tracked_paths: Iterable[str | Path] | None,
    *,
    root: Path,
    fallback_manifest: dict[str, str],
) -> tuple[str, ...]:
    if tracked_paths is None:
        return tuple(sorted(fallback_manifest))

    normalized: set[str] = set()
    for item in tracked_paths:
        path = Path(item)
        if path.is_absolute():
            relative = path.relative_to(root)
        else:
            relative = path
        normalized.add(relative.as_posix())
    return tuple(sorted(normalized or fallback_manifest))


@dataclass(frozen=True)
class ReadbackReport:
    success: bool
    compared_files: tuple[str, ...]
    changed_files: tuple[str, ...]
    missing_files: tuple[str, ...]
    unexpected_files: tuple[str, ...]
    expected_manifest: dict[str, str]
    actual_manifest: dict[str, str]
    readback_root: Path


@dataclass(frozen=True)
class AuditEntry:
    timestamp: str
    operation: str
    result_status: str
    workspace_id: str
    save_hash: str
    toolchain: ToolchainInfo
    rule_set_version: str
    plan_summary: str
    details: dict[str, Any]

    def to_dict(self) -> dict[str, Any]:
        return {
            "timestamp": self.timestamp,
            "operation": self.operation,
            "result_status": self.result_status,
            "workspace_id": self.workspace_id,
            "save_hash": self.save_hash,
            "toolchain": {
                "name": self.toolchain.name,
                "executable": str(self.toolchain.executable),
                "version": self.toolchain.version,
                "executable_sha256": self.toolchain.executable_sha256,
                "launcher": list(self.toolchain.launcher),
                "required_version": self.toolchain.required_version,
                "required_sha256": self.toolchain.required_sha256,
            },
            "rule_set_version": self.rule_set_version,
            "plan_summary": self.plan_summary,
            "details": self.details,
        }


@dataclass(frozen=True)
class PackTransactionResult:
    workspace_id: str
    dry_run: bool
    source_save: Path
    output_save: Path | None
    restore_point: RestorePoint
    manifest_delta: ManifestDelta
    readback_report: ReadbackReport | None
    audit_entry: AuditEntry
    incident_bundle: Path | None
    workspace_root: Path

    def to_dict(self) -> dict[str, Any]:
        return {
            "workspace_id": self.workspace_id,
            "dry_run": self.dry_run,
            "source_save": str(self.source_save),
            "output_save": str(self.output_save) if self.output_save else None,
            "restore_point": {
                "restore_point_id": self.restore_point.restore_point_id,
                "root": str(self.restore_point.root),
                "backup_path": str(self.restore_point.backup_path),
                "manifest_path": str(self.restore_point.manifest_path),
                "created_at": self.restore_point.created_at,
                "source_sha256": self.restore_point.source_sha256,
            },
            "manifest_delta": {
                "created": list(self.manifest_delta.created),
                "updated": list(self.manifest_delta.updated),
                "deleted": list(self.manifest_delta.deleted),
            },
            "readback_report": None
            if self.readback_report is None
            else {
                "success": self.readback_report.success,
                "compared_files": list(self.readback_report.compared_files),
                "changed_files": list(self.readback_report.changed_files),
                "missing_files": list(self.readback_report.missing_files),
                "unexpected_files": list(self.readback_report.unexpected_files),
                "readback_root": str(self.readback_report.readback_root),
            },
            "audit_entry": self.audit_entry.to_dict(),
            "incident_bundle": str(self.incident_bundle) if self.incident_bundle else None,
            "workspace_root": str(self.workspace_root),
        }


class PackTransactionError(RuntimeError):
    def __init__(
        self,
        message: str,
        *,
        result: PackTransactionResult | None = None,
    ) -> None:
        super().__init__(message)
        self.result = result


class PostWriteReadbackError(PackTransactionError):
    """Raised when repacked output fails post-write readback validation."""


class PackTransaction:
    def __init__(
        self,
        adapter: ContainerAdapter,
        *,
        working_root: Path,
        rule_set_version: str = "v1.0-baseline",
    ) -> None:
        self.adapter = adapter
        self.working_root = Path(working_root)
        self.rule_set_version = rule_set_version

    def run(
        self,
        source_save: Path,
        *,
        plan_summary: str,
        mutate: MutationFn,
        dry_run: bool,
        output_save: Path | None = None,
        workspace_id: str | None = None,
    ) -> PackTransactionResult:
        source_save = Path(source_save)
        workspace = SaveWorkspace(
            source_save,
            self.working_root,
            workspace_id=workspace_id,
        )
        toolchain = self.adapter.describe_toolchain()
        source_hash = sha256_file(source_save)
        shadow_save = workspace.prepare_shadow_copy()
        incident_bundle: Path | None = None
        restore_point: RestorePoint | None = None
        manifest_delta: ManifestDelta | None = None
        readback_report: ReadbackReport | None = None

        try:
            unpacked_root = self.adapter.unpack(
                shadow_save,
                expected_directory=workspace.layout.unpacked_root,
            )
            before_manifest = snapshot_directory_manifest(unpacked_root)
            restore_point = workspace.create_restore_point(
                plan_summary=plan_summary,
                rule_set_version=self.rule_set_version,
                toolchain=toolchain,
                source_hash=source_hash,
                metadata={"operation": "dry-run" if dry_run else "apply"},
            )
            tracked_paths = mutate(unpacked_root)
            after_manifest = snapshot_directory_manifest(unpacked_root)
            manifest_delta = diff_manifests(before_manifest, after_manifest)
            compared_files = _normalize_tracked_paths(
                tracked_paths,
                root=unpacked_root,
                fallback_manifest=after_manifest,
            )

            if dry_run:
                audit_entry = self._build_audit_entry(
                    workspace=workspace,
                    toolchain=toolchain,
                    operation="dry-run",
                    result_status="dry_run_ready",
                    save_hash=source_hash,
                    plan_summary=plan_summary,
                    details={
                        "compared_files": list(compared_files),
                        "manifest_delta": {
                            "created": list(manifest_delta.created),
                            "updated": list(manifest_delta.updated),
                            "deleted": list(manifest_delta.deleted),
                        },
                    },
                )
                return PackTransactionResult(
                    workspace_id=workspace.layout.workspace_id,
                    dry_run=True,
                    source_save=source_save,
                    output_save=None,
                    restore_point=restore_point,
                    manifest_delta=manifest_delta,
                    readback_report=None,
                    audit_entry=audit_entry,
                    incident_bundle=None,
                    workspace_root=workspace.layout.root,
                )

            if output_save is None:
                raise ValueError("output_save is required for apply operations")

            self.adapter.repack(unpacked_root, expected_container=shadow_save)
            readback_root = self.adapter.unpack(
                shadow_save,
                expected_directory=workspace.layout.readback_root,
            )
            readback_manifest = snapshot_directory_manifest(readback_root)
            readback_report = self._compare_readback(
                expected_manifest=after_manifest,
                actual_manifest=readback_manifest,
                compared_files=compared_files,
                readback_root=readback_root,
            )

            if not readback_report.success:
                audit_entry = self._build_audit_entry(
                    workspace=workspace,
                    toolchain=toolchain,
                    operation="apply",
                    result_status="blocked",
                    save_hash=source_hash,
                    plan_summary=plan_summary,
                    details={
                        "readback_report": self._readback_details(readback_report)
                    },
                )
                incident_bundle = workspace.capture_incident(
                    stage="post-write-readback",
                    error="post-write readback mismatch",
                    plan_summary=plan_summary,
                    rule_set_version=self.rule_set_version,
                    toolchain=toolchain,
                    payload={
                        "manifest_delta": {
                            "created": list(manifest_delta.created),
                            "updated": list(manifest_delta.updated),
                            "deleted": list(manifest_delta.deleted),
                        },
                        "readback_report": self._readback_details(readback_report),
                        "audit_entry": audit_entry.to_dict(),
                    },
                )
                result = PackTransactionResult(
                    workspace_id=workspace.layout.workspace_id,
                    dry_run=False,
                    source_save=source_save,
                    output_save=None,
                    restore_point=restore_point,
                    manifest_delta=manifest_delta,
                    readback_report=readback_report,
                    audit_entry=audit_entry,
                    incident_bundle=incident_bundle,
                    workspace_root=workspace.layout.root,
                )
                raise PostWriteReadbackError(
                    "Post-write readback mismatch",
                    result=result,
                )

            final_output = workspace.copy_shadow_to_output(output_save)
            audit_entry = self._build_audit_entry(
                workspace=workspace,
                toolchain=toolchain,
                operation="apply",
                result_status="applied",
                save_hash=source_hash,
                plan_summary=plan_summary,
                details={
                    "output_save": str(final_output),
                    "readback_report": self._readback_details(readback_report),
                },
            )
            return PackTransactionResult(
                workspace_id=workspace.layout.workspace_id,
                dry_run=False,
                source_save=source_save,
                output_save=final_output,
                restore_point=restore_point,
                manifest_delta=manifest_delta,
                readback_report=readback_report,
                audit_entry=audit_entry,
                incident_bundle=None,
                workspace_root=workspace.layout.root,
            )
        except PackTransactionError:
            raise
        except Exception as error:
            if restore_point is None:
                restore_point = workspace.create_restore_point(
                    plan_summary=plan_summary,
                    rule_set_version=self.rule_set_version,
                    toolchain=toolchain,
                    source_hash=source_hash,
                    metadata={"operation": "failed-before-restore"},
                )
            audit_entry = self._build_audit_entry(
                workspace=workspace,
                toolchain=toolchain,
                operation="dry-run" if dry_run else "apply",
                result_status="blocked",
                save_hash=source_hash,
                plan_summary=plan_summary,
                details={"error": str(error)},
            )
            incident_bundle = workspace.capture_incident(
                stage="transaction-exception",
                error=str(error),
                plan_summary=plan_summary,
                rule_set_version=self.rule_set_version,
                toolchain=toolchain,
                payload={
                    "manifest_delta": None
                    if manifest_delta is None
                    else {
                        "created": list(manifest_delta.created),
                        "updated": list(manifest_delta.updated),
                        "deleted": list(manifest_delta.deleted),
                    },
                    "readback_report": None
                    if readback_report is None
                    else self._readback_details(readback_report),
                    "audit_entry": audit_entry.to_dict(),
                },
            )
            result = PackTransactionResult(
                workspace_id=workspace.layout.workspace_id,
                dry_run=dry_run,
                source_save=source_save,
                output_save=None,
                restore_point=restore_point,
                manifest_delta=manifest_delta or diff_manifests({}, {}),
                readback_report=readback_report,
                audit_entry=audit_entry,
                incident_bundle=incident_bundle,
                workspace_root=workspace.layout.root,
            )
            raise PackTransactionError(str(error), result=result) from error

    def _compare_readback(
        self,
        *,
        expected_manifest: dict[str, str],
        actual_manifest: dict[str, str],
        compared_files: tuple[str, ...],
        readback_root: Path,
    ) -> ReadbackReport:
        changed = []
        missing = []
        unexpected = []
        for relative_path in compared_files:
            expected_hash = expected_manifest.get(relative_path)
            actual_hash = actual_manifest.get(relative_path)
            if expected_hash is None and actual_hash is not None:
                unexpected.append(relative_path)
            elif expected_hash is not None and actual_hash is None:
                missing.append(relative_path)
            elif expected_hash != actual_hash:
                changed.append(relative_path)

        return ReadbackReport(
            success=not (changed or missing or unexpected),
            compared_files=compared_files,
            changed_files=tuple(sorted(changed)),
            missing_files=tuple(sorted(missing)),
            unexpected_files=tuple(sorted(unexpected)),
            expected_manifest=dict(expected_manifest),
            actual_manifest=dict(actual_manifest),
            readback_root=readback_root,
        )

    def _build_audit_entry(
        self,
        *,
        workspace: SaveWorkspace,
        toolchain: ToolchainInfo,
        operation: str,
        result_status: str,
        save_hash: str,
        plan_summary: str,
        details: dict[str, Any],
    ) -> AuditEntry:
        return AuditEntry(
            timestamp=iso_now(),
            operation=operation,
            result_status=result_status,
            workspace_id=workspace.layout.workspace_id,
            save_hash=save_hash,
            toolchain=toolchain,
            rule_set_version=self.rule_set_version,
            plan_summary=plan_summary,
            details=details,
        )

    @staticmethod
    def _readback_details(readback_report: ReadbackReport) -> dict[str, Any]:
        return {
            "success": readback_report.success,
            "compared_files": list(readback_report.compared_files),
            "changed_files": list(readback_report.changed_files),
            "missing_files": list(readback_report.missing_files),
            "unexpected_files": list(readback_report.unexpected_files),
            "readback_root": str(readback_report.readback_root),
        }
