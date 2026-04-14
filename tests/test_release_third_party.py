from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from ac6_data_manager.release import build_manifest_for_release, verify_manifest


class ReleaseThirdPartyManifestTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tempdir = tempfile.TemporaryDirectory()
        self.app_root = Path(self.tempdir.name) / "AC6 saving manager"
        tool_root = self.app_root / "third_party" / "WitchyBND"
        tool_root.mkdir(parents=True, exist_ok=True)
        (tool_root / "WitchyBND.exe").write_bytes(b"exe")
        (tool_root / "helper.dll").write_bytes(b"dll")

    def tearDown(self) -> None:
        self.tempdir.cleanup()

    def test_manifest_generation_and_verification_pass_for_exact_directory(self) -> None:
        manifest = build_manifest_for_release(
            self.app_root,
            version_label="acvi-1.0",
            source_baseline="baseline-ref",
        )
        result = verify_manifest(
            self.app_root,
            manifest,
            expected_version_label="acvi-1.0",
        )

        self.assertTrue(result.success)
        self.assertEqual(result.missing_files, ())
        self.assertEqual(result.extra_files, ())

    def test_manifest_verification_blocks_extra_files(self) -> None:
        manifest = build_manifest_for_release(
            self.app_root,
            version_label="acvi-1.0",
            source_baseline="baseline-ref",
        )
        (self.app_root / "third_party" / "WitchyBND" / "rogue.txt").write_text(
            "rogue",
            encoding="utf-8",
        )

        result = verify_manifest(self.app_root, manifest)

        self.assertFalse(result.success)
        self.assertEqual(result.extra_files, ("third_party/WitchyBND/rogue.txt",))

    def test_manifest_verification_blocks_hash_mismatch_and_version_mismatch(self) -> None:
        manifest = build_manifest_for_release(
            self.app_root,
            version_label="acvi-1.0",
            source_baseline="baseline-ref",
        )
        (self.app_root / "third_party" / "WitchyBND" / "helper.dll").write_bytes(b"changed")

        result = verify_manifest(
            self.app_root,
            manifest,
            expected_version_label="acvi-2.0",
        )

        self.assertFalse(result.success)
        self.assertEqual(len(result.hash_mismatches), 1)
        self.assertIn("version mismatch", "\n".join(result.errors))
