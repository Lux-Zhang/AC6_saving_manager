from __future__ import annotations

import base64
import json
import shutil
import tempfile
import unittest
from pathlib import Path

from ac6_data_manager.container_workspace import PackTransaction, PostWriteReadbackError, ToolchainInfo
from ac6_data_manager.validation import AuditEvent, AuditJsonlReader, AuditJsonlWriter, IncidentBundleIndex


class FakeContainerAdapter:
    def __init__(self, *, corrupt_readback: bool = False) -> None:
        self.corrupt_readback = corrupt_readback
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
            content = base64.b64decode(encoded_content)
            if self.corrupt_readback and target.name == "readback" and relative_path == "USER_DATA007":
                content += b"-corrupted"
            file_path.write_bytes(content)
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


class ValidationAuditTests(unittest.TestCase):
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

    def test_audit_jsonl_round_trip_from_transaction_result(self) -> None:
        transaction = PackTransaction(FakeContainerAdapter(), working_root=self.root / "workspaces")

        def mutate(unpacked_root: Path) -> list[str]:
            (unpacked_root / "USER_DATA007").write_bytes(b"after")
            return ["USER_DATA007"]

        result = transaction.run(
            self.source_save,
            plan_summary="audit writer smoke test",
            mutate=mutate,
            dry_run=False,
            output_save=self.root / "outputs" / "managed.sl2",
        )
        writer = AuditJsonlWriter(self.root / "audit" / "audit.jsonl")
        event = writer.append(
            AuditEvent.from_audit_entry(
                result.audit_entry,
                source_save=result.source_save,
                output_save=result.output_save,
                restore_point_id=result.restore_point.restore_point_id,
            )
        )

        events = AuditJsonlReader(self.root / "audit" / "audit.jsonl").read_all()
        self.assertEqual(len(events), 1)
        self.assertEqual(events[0].operation, "apply")
        self.assertEqual(events[0].result_status, "applied")
        self.assertEqual(events[0].restore_point_id, result.restore_point.restore_point_id)
        self.assertEqual(event.output_save, str(result.output_save))

    def test_incident_bundle_index_reads_transaction_incident_artifact(self) -> None:
        transaction = PackTransaction(
            FakeContainerAdapter(corrupt_readback=True),
            working_root=self.root / "workspaces",
        )

        def mutate(unpacked_root: Path) -> list[str]:
            (unpacked_root / "USER_DATA007").write_bytes(b"after")
            return ["USER_DATA007"]

        with self.assertRaises(PostWriteReadbackError) as raised:
            transaction.run(
                self.source_save,
                plan_summary="incident bundle index test",
                mutate=mutate,
                dry_run=False,
                output_save=self.root / "outputs" / "managed.sl2",
            )

        result = raised.exception.result
        assert result is not None and result.incident_bundle is not None
        index = IncidentBundleIndex(self.root / "audit" / "incident-index.jsonl")
        entry = index.append_from_bundle(result.incident_bundle)
        entries = index.read_all()
        self.assertEqual(len(entries), 1)
        self.assertEqual(entry.workspace_id, result.workspace_id)
        self.assertEqual(entries[0].stage, "post-write-readback")
        self.assertTrue(entries[0].incident_bundle.endswith("incident.json"))
