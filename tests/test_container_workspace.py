from __future__ import annotations

import base64
import json
import shutil
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

from src.ac6_data_manager.container_workspace import (
    PackTransaction,
    PackTransactionError,
    PostWriteReadbackError,
    ToolchainInfo,
    WitchyBndAdapter,
)
from src.ac6_data_manager.container_workspace.workspace import snapshot_directory_manifest


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
            if not file_path.is_file():
                continue
            if file_path.name == ".ac6dm_unpack_meta.json":
                continue
            files[file_path.relative_to(unpacked_directory).as_posix()] = base64.b64encode(
                file_path.read_bytes()
            ).decode("ascii")
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(json.dumps({"files": files}, ensure_ascii=True, indent=2), encoding="utf-8")
        return target


class SaveWorkspaceTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tempdir = tempfile.TemporaryDirectory()
        self.root = Path(self.tempdir.name)
        self.source_save = self.root / "AC60000.sl2"
        self.source_save.write_text(
            json.dumps(
                {
                    "files": {
                        "USER_DATA007": base64.b64encode(b"before").decode("ascii"),
                        "catalog/meta.txt": base64.b64encode(b"metadata").decode("ascii"),
                    }
                },
                ensure_ascii=True,
                indent=2,
            ),
            encoding="utf-8",
        )

    def tearDown(self) -> None:
        self.tempdir.cleanup()

    def test_dry_run_uses_shadow_workspace_without_overwriting_source(self) -> None:
        transaction = PackTransaction(FakeContainerAdapter(), working_root=self.root / "workspaces")

        def mutate(unpacked_root: Path) -> list[str]:
            target = unpacked_root / "USER_DATA007"
            target.write_bytes(b"after")
            return ["USER_DATA007"]

        result = transaction.run(
            self.source_save,
            plan_summary="dry-run emblem import",
            mutate=mutate,
            dry_run=True,
        )

        source_payload = json.loads(self.source_save.read_text(encoding="utf-8"))
        self.assertEqual(result.output_save, None)
        self.assertEqual(result.audit_entry.result_status, "dry_run_ready")
        self.assertEqual(
            base64.b64decode(source_payload["files"]["USER_DATA007"]),
            b"before",
        )
        self.assertTrue(result.restore_point.backup_path.exists())
        manifest = json.loads(result.restore_point.manifest_path.read_text(encoding="utf-8"))
        self.assertEqual(manifest["source_sha256"], result.restore_point.source_sha256)
        self.assertEqual(result.manifest_delta.updated, ("USER_DATA007",))

    def test_apply_repacks_and_readback_verifies_tracked_files(self) -> None:
        transaction = PackTransaction(FakeContainerAdapter(), working_root=self.root / "workspaces")
        output_save = self.root / "outputs" / "AC60000-managed.sl2"

        def mutate(unpacked_root: Path) -> list[str]:
            (unpacked_root / "USER_DATA007").write_bytes(b"after-apply")
            (unpacked_root / "catalog" / "new.txt").write_text("new-entry", encoding="utf-8")
            return ["USER_DATA007", "catalog/new.txt"]

        result = transaction.run(
            self.source_save,
            plan_summary="apply emblem import",
            mutate=mutate,
            dry_run=False,
            output_save=output_save,
        )

        self.assertEqual(result.audit_entry.result_status, "applied")
        self.assertIsNotNone(result.readback_report)
        self.assertTrue(result.readback_report.success)
        self.assertEqual(result.readback_report.changed_files, ())
        self.assertTrue(output_save.exists())
        output_payload = json.loads(output_save.read_text(encoding="utf-8"))
        self.assertEqual(
            base64.b64decode(output_payload["files"]["USER_DATA007"]),
            b"after-apply",
        )
        self.assertEqual(
            base64.b64decode(output_payload["files"]["catalog/new.txt"]),
            b"new-entry",
        )

    def test_readback_mismatch_creates_incident_bundle_and_blocks_output(self) -> None:
        transaction = PackTransaction(
            FakeContainerAdapter(corrupt_readback=True),
            working_root=self.root / "workspaces",
        )
        output_save = self.root / "outputs" / "AC60000-managed.sl2"

        def mutate(unpacked_root: Path) -> list[str]:
            (unpacked_root / "USER_DATA007").write_bytes(b"after-readback")
            return ["USER_DATA007"]

        with self.assertRaises(PostWriteReadbackError) as raised:
            transaction.run(
                self.source_save,
                plan_summary="apply emblem import with readback mismatch",
                mutate=mutate,
                dry_run=False,
                output_save=output_save,
            )

        result = raised.exception.result
        assert result is not None
        self.assertFalse(output_save.exists())
        self.assertIsNotNone(result.incident_bundle)
        assert result.incident_bundle is not None
        self.assertTrue(result.incident_bundle.exists())
        self.assertEqual(result.audit_entry.result_status, "blocked")
        self.assertEqual(result.readback_report.changed_files, ("USER_DATA007",))

    def test_apply_rejects_in_place_overwrite(self) -> None:
        transaction = PackTransaction(FakeContainerAdapter(), working_root=self.root / "workspaces")

        def mutate(unpacked_root: Path) -> list[str]:
            (unpacked_root / "USER_DATA007").write_bytes(b"after-apply")
            return ["USER_DATA007"]

        with self.assertRaises(PackTransactionError) as raised:
            transaction.run(
                self.source_save,
                plan_summary="invalid apply",
                mutate=mutate,
                dry_run=False,
                output_save=self.source_save,
            )

        self.assertIn("In-place overwrite is forbidden", str(raised.exception))


class WitchyAdapterTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tempdir = tempfile.TemporaryDirectory()
        self.root = Path(self.tempdir.name)
        self.fake_witchy = self.root / "fake_witchy.py"
        self.fake_witchy.write_text(
            textwrap.dedent(
                """
                import base64
                import json
                import sys
                from pathlib import Path

                META_NAME = ".ac6dm_unpack_meta.json"

                def default_unpack_dir(container: Path) -> Path:
                    suffix = container.suffix.lstrip(".")
                    return container.with_name(f"{container.stem}-{suffix}") if suffix else container.with_name(f"{container.name}-unpacked")

                def pack_directory(directory: Path) -> dict[str, str]:
                    files = {}
                    for path in sorted(directory.rglob("*")):
                        if not path.is_file():
                            continue
                        if path.name == META_NAME:
                            continue
                        files[path.relative_to(directory).as_posix()] = base64.b64encode(path.read_bytes()).decode("ascii")
                    return files

                def unpack_container(container: Path) -> None:
                    payload = json.loads(container.read_text(encoding="utf-8"))
                    target = default_unpack_dir(container)
                    if target.exists():
                        import shutil
                        shutil.rmtree(target)
                    target.mkdir(parents=True, exist_ok=True)
                    for relative_path, encoded in payload["files"].items():
                        file_path = target / relative_path
                        file_path.parent.mkdir(parents=True, exist_ok=True)
                        file_path.write_bytes(base64.b64decode(encoded))

                def repack_directory(directory: Path) -> None:
                    metadata = json.loads((directory / META_NAME).read_text(encoding="utf-8"))
                    target = directory.parent / metadata["container_name"]
                    target.write_text(json.dumps({"files": pack_directory(directory)}, ensure_ascii=True, indent=2), encoding="utf-8")

                def main() -> int:
                    argument = sys.argv[1]
                    if argument == "--version":
                        print("fake-witchy 1.0")
                        return 0
                    path = Path(argument)
                    if path.is_file():
                        unpack_container(path)
                        return 0
                    if path.is_dir():
                        repack_directory(path)
                        return 0
                    print(f"unsupported path: {path}", file=sys.stderr)
                    return 2

                raise SystemExit(main())
                """
            ).strip()
            + "\n",
            encoding="utf-8",
        )
        self.adapter = WitchyBndAdapter(
            self.fake_witchy,
            launcher=(sys.executable,),
            required_version="fake-witchy 1.0",
        )

    def tearDown(self) -> None:
        self.tempdir.cleanup()

    def test_witchy_adapter_round_trips_unpack_and_repack(self) -> None:
        container = self.root / "AC60000.sl2"
        container.write_text(
            json.dumps(
                {
                    "files": {
                        "USER_DATA007": base64.b64encode(b"adapter-before").decode("ascii"),
                        "folder/item.txt": base64.b64encode(b"nested").decode("ascii"),
                    }
                },
                ensure_ascii=True,
                indent=2,
            ),
            encoding="utf-8",
        )

        toolchain = self.adapter.describe_toolchain()
        unpacked_root = self.adapter.unpack(container, expected_directory=self.root / "shadow" / "unpacked")
        self.assertEqual(toolchain.version, "fake-witchy 1.0")
        self.assertTrue((unpacked_root / ".ac6dm_unpack_meta.json").exists())
        self.assertEqual((unpacked_root / "USER_DATA007").read_bytes(), b"adapter-before")

        (unpacked_root / "USER_DATA007").write_bytes(b"adapter-after")
        repacked = self.adapter.repack(unpacked_root, expected_container=self.root / "shadow" / "AC60000.sl2")
        readback = self.adapter.unpack(repacked, expected_directory=self.root / "readback")
        manifest = snapshot_directory_manifest(readback)
        self.assertEqual(manifest["USER_DATA007"], snapshot_directory_manifest(unpacked_root)["USER_DATA007"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
