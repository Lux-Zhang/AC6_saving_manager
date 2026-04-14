from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from ac6_data_manager.release import (
    build_release_layout,
    stage_portable_release,
    write_release_content_manifest,
)


class ReleaseBuildTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tempdir = tempfile.TemporaryDirectory()
        root = Path(self.tempdir.name)
        self.dist_app_root = root / "dist" / "AC6 saving manager"
        self.dist_app_root.mkdir(parents=True, exist_ok=True)
        (self.dist_app_root / "AC6 saving manager.exe").write_bytes(b"exe")
        self.third_party_source = root / "baseline" / "WitchyBND"
        self.third_party_source.mkdir(parents=True, exist_ok=True)
        (self.third_party_source / "WitchyBND.exe").write_bytes(b"witchy")
        self.assets_root = root / "assets"
        self.assets_root.mkdir(parents=True, exist_ok=True)
        for name in ("QUICK_START.txt", "LIVE_ACCEPTANCE_CHECKLIST.txt", "KNOWN_LIMITATIONS.txt"):
            (self.assets_root / name).write_text(name, encoding="utf-8")
        self.release_base_dir = root / "release"

    def tearDown(self) -> None:
        self.tempdir.cleanup()

    def test_stage_portable_release_creates_expected_layout_and_manifest(self) -> None:
        layout, _manifest = stage_portable_release(
            self.dist_app_root,
            self.release_base_dir,
            third_party_source=self.third_party_source,
            packaging_assets_dir=self.assets_root,
            version_label="acvi-1.0",
            source_baseline="baseline-ref",
        )
        manifest_path = self.release_base_dir / "content-manifest.json"
        write_release_content_manifest(layout.app_root, manifest_path)

        self.assertTrue(layout.exe_path.exists())
        self.assertTrue(layout.manifest_path.exists())
        self.assertTrue((layout.docs_root / "KNOWN_LIMITATIONS.txt").exists())
        self.assertTrue(manifest_path.exists())

    def test_stage_portable_release_requires_fresh_destination(self) -> None:
        layout = build_release_layout(self.release_base_dir)
        layout.app_root.mkdir(parents=True, exist_ok=True)

        with self.assertRaises(FileExistsError):
            stage_portable_release(
                self.dist_app_root,
                self.release_base_dir,
                third_party_source=self.third_party_source,
                packaging_assets_dir=self.assets_root,
                version_label="acvi-1.0",
                source_baseline="baseline-ref",
            )
