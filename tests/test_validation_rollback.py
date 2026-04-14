from __future__ import annotations

import base64
import json
import shutil
import tempfile
import unittest
from pathlib import Path

from ac6_data_manager.container_workspace import PackTransaction, ToolchainInfo
from ac6_data_manager.validation import (
    AuditLog,
    AuditJsonlReader,
    AuditJsonlWriter,
    IncidentBundleWriter,
    RollbackCoordinator,
    RollbackError,
    RollbackRequest,
    RollbackService,
    create_restore_point,
)


class FakeContainerAdapter:
    def __init__(self) -> None:
        self.toolchain = ToolchainInfo(
            name="FakeWitchy",
            executable=Path("/tmp/fake-witchy"),
            executable_sha256="fake-sha256",
            version="fake-1.0",
        )

    def describe_toolchain(self) -> ToolchainInfo:
        return self.toolchain

    def unpack(self, container_path: Path, *, expected_directory: Path | None = None) -> Path:
        payload = json.loads(Path(container_path).read_text(encoding="utf-8"))
        target = Path(expected_directory or container_path.with_name(f"{container_path.stem}-sl2"))
        if target.exists():
            shutil.rmtree(target)
        target.mkdir(parents=True, exist_ok=True)
        for relative_path, encoded_content in payload["files"].items():
            file_path = target / relative_path
            file_path.parent.mkdir(parents=True, exist_ok=True)
            file_path.write_bytes(base64.b64decode(encoded_content))
        (target / ".ac6dm_unpack_meta.json").write_text(
            json.dumps({"container_name": Path(container_path).name}, ensure_ascii=True),
            encoding="utf-8",
        )
        return target

    def repack(self, unpacked_directory: Path, *, expected_container: Path | None = None) -> Path:
        target = Path(expected_container or unpacked_directory.with_suffix(".sl2"))
        files: dict[str, str] = {}
        for file_path in sorted(Path(unpacked_directory).rglob("*")):
            if not file_path.is_file() or file_path.name == ".ac6dm_unpack_meta.json":
                continue
            files[file_path.relative_to(unpacked_directory).as_posix()] = base64.b64encode(
                file_path.read_bytes()
            ).decode("ascii")
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(json.dumps({"files": files}, ensure_ascii=True, indent=2), encoding="utf-8")
        return target


class RollbackServiceTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tempdir = tempfile.TemporaryDirectory()
        self.root = Path(self.tempdir.name)
        self.source_save = self.root / "AC60000.sl2"
        self.source_save.write_text(
            json.dumps(
                {
                    "files": {
                        "USER_DATA007": base64.b64encode(b"before").decode("ascii"),
                    }
                },
                ensure_ascii=True,
                indent=2,
            ),
            encoding="utf-8",
        )

    def tearDown(self) -> None:
        self.tempdir.cleanup()

    def test_restore_point_rolls_back_to_fresh_output_path_and_writes_audit(self) -> None:
        transaction = PackTransaction(FakeContainerAdapter(), working_root=self.root / "workspaces")

        def mutate(unpacked_root: Path) -> list[str]:
            (unpacked_root / "USER_DATA007").write_bytes(b"after")
            return ["USER_DATA007"]

        result = transaction.run(
            self.source_save,
            plan_summary="rollback rehearsal",
            mutate=mutate,
            dry_run=False,
            output_save=self.root / "outputs" / "managed.sl2",
        )
        writer = AuditJsonlWriter(self.root / "audit" / "audit.jsonl")
        service = RollbackService(audit_writer=writer)
        restored_path = self.root / "rollback" / "AC60000-rollback.sl2"

        rollback = service.restore(
            result.restore_point.manifest_path,
            output_path=restored_path,
            plan_summary="rollback rehearsal result",
        )

        self.assertEqual(rollback.output_path, restored_path)
        self.assertTrue(restored_path.exists())
        self.assertEqual(restored_path.read_text(encoding="utf-8"), self.source_save.read_text(encoding="utf-8"))
        events = AuditJsonlReader(self.root / "audit" / "audit.jsonl").read_all()
        self.assertEqual(len(events), 1)
        self.assertEqual(events[0].operation, "rollback")
        self.assertEqual(events[0].result_status, "rolled_back")

    def test_restore_rejects_in_place_output(self) -> None:
        transaction = PackTransaction(FakeContainerAdapter(), working_root=self.root / "workspaces")

        def mutate(unpacked_root: Path) -> list[str]:
            (unpacked_root / "USER_DATA007").write_bytes(b"after")
            return ["USER_DATA007"]

        result = transaction.run(
            self.source_save,
            plan_summary="rollback in-place guard",
            mutate=mutate,
            dry_run=True,
        )
        service = RollbackService()
        with self.assertRaises(RollbackError):
            service.restore(result.restore_point.manifest_path, output_path=self.source_save)

    def test_legacy_rollback_coordinator_fails_closed_for_in_place_target(self) -> None:
        restore_point = create_restore_point(
            self.source_save,
            self.root / "legacy-restore-points",
            workspace_id="workspace-legacy",
            description="legacy restore point",
        )
        coordinator = RollbackCoordinator(
            audit_log=AuditLog(self.root / "audit" / "legacy-audit.jsonl"),
            incident_writer=IncidentBundleWriter(self.root / "incidents"),
        )

        result = coordinator.execute(
            RollbackRequest(
                target_path=str(self.source_save),
                restore_point=restore_point,
                reason="legacy in-place attempt",
                toolchain_version="fake-1.0",
                rule_set_version="v1.0-baseline",
                plan_summary="legacy compatibility should fail closed",
            )
        )
        self.assertFalse(result.restored)
        self.assertIsNotNone(result.incident_bundle)
