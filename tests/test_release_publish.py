from __future__ import annotations

import base64
import json
import shutil
import tempfile
import unittest
from pathlib import Path

from ac6_data_manager.container_workspace import PackTransaction, ToolchainInfo
from ac6_data_manager.release import write_publish_artifacts, write_rollback_artifact
from ac6_data_manager.validation import AuditJsonlWriter, RollbackService


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


class ReleasePublishArtifactTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tempdir = tempfile.TemporaryDirectory()
        self.root = Path(self.tempdir.name)
        self.source_save = self.root / "AC60000.sl2"
        self.source_save.write_text(
            json.dumps(
                {"files": {"USER_DATA007": base64.b64encode(b"before").decode("ascii")}},
                ensure_ascii=True,
                indent=2,
            ),
            encoding="utf-8",
        )

    def tearDown(self) -> None:
        self.tempdir.cleanup()

    def test_publish_and_rollback_artifacts_are_written(self) -> None:
        transaction = PackTransaction(FakeContainerAdapter(), working_root=self.root / "workspaces")

        def mutate(unpacked_root: Path) -> list[str]:
            (unpacked_root / "USER_DATA007").write_bytes(b"after")
            return ["USER_DATA007"]

        result = transaction.run(
            self.source_save,
            plan_summary="publish artifact test",
            mutate=mutate,
            dry_run=False,
            output_save=self.root / "outputs" / "managed.sl2",
        )
        publish_paths = write_publish_artifacts(result, self.root / "artifacts" / "live_acceptance")

        writer = AuditJsonlWriter(self.root / "audit" / "audit.jsonl")
        rollback = RollbackService(audit_writer=writer).restore(
            result.restore_point.manifest_path,
            output_path=self.root / "rollback" / "restored.sl2",
            plan_summary="rollback artifact test",
        )
        rollback_path = write_rollback_artifact(rollback, self.root / "artifacts" / "live_acceptance")

        self.assertIn("publish_audit", publish_paths)
        self.assertIn("readback", publish_paths)
        self.assertTrue(Path(publish_paths["publish_audit"]).exists())
        self.assertTrue(Path(publish_paths["readback"]).exists())
        self.assertTrue(Path(rollback_path).exists())
