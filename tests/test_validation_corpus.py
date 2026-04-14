from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from ac6_data_manager.validation import GoldenCorpusManifestHelper


class GoldenCorpusManifestTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tempdir = tempfile.TemporaryDirectory()
        self.root = Path(self.tempdir.name)
        self.corpus_root = self.root / "golden-corpus"
        self.corpus_root.mkdir(parents=True, exist_ok=True)
        (self.corpus_root / "emblems").mkdir()
        (self.corpus_root / "emblems" / "alpha.bin").write_bytes(b"alpha")
        (self.corpus_root / "emblems" / "beta.bin").write_bytes(b"beta")

    def tearDown(self) -> None:
        self.tempdir.cleanup()

    def test_build_and_verify_manifest(self) -> None:
        helper = GoldenCorpusManifestHelper(self.corpus_root)
        manifest_path = self.root / "artifacts" / "golden-manifest.json"
        manifest = helper.build_manifest(
            manifest_path=manifest_path,
            dataset_name="emblem-m0-corpus",
            tags_by_relative_path={"emblems/alpha.bin": ["share", "editable"]},
            metadata={"lane": "M0"},
        )

        self.assertEqual(manifest.dataset_name, "emblem-m0-corpus")
        self.assertEqual(len(manifest.entries), 2)
        verification = helper.verify_manifest(manifest_path)
        self.assertTrue(verification.ok)
        self.assertEqual(verification.changed, ())
        self.assertEqual(verification.missing, ())
        self.assertEqual(verification.unexpected, ())

        (self.corpus_root / "emblems" / "beta.bin").write_bytes(b"beta-mutated")
        broken = helper.verify_manifest(manifest_path)
        self.assertFalse(broken.ok)
        self.assertEqual(broken.changed, ("emblems/beta.bin",))
