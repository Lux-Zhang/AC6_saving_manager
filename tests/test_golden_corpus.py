from __future__ import annotations

from pathlib import Path

from ac6_data_manager.validation import GoldenCorpusManifest


def test_manifest_round_trip_and_verify(tmp_path: Path) -> None:
    corpus_root = tmp_path / "corpus"
    corpus_root.mkdir()
    (corpus_root / "sample-a.bin").write_bytes(b"alpha")
    (corpus_root / "sample-b.bin").write_bytes(b"beta")

    manifest = GoldenCorpusManifest.from_files(
        corpus_root,
        [corpus_root / "sample-a.bin", corpus_root / "sample-b.bin"],
        generated_at="2026-04-14T00:00:00Z",
        rule_set_version="v1.0-baseline",
        metadata={"lane": "validation"},
    )
    manifest_path = tmp_path / "manifest.json"
    manifest.write(manifest_path)

    loaded = GoldenCorpusManifest.read(manifest_path)
    report = loaded.verify(corpus_root)

    assert report.passed is True
    assert report.verified == ("sample-a.bin", "sample-b.bin")


def test_manifest_detects_mismatch_and_unexpected_files(tmp_path: Path) -> None:
    corpus_root = tmp_path / "corpus"
    corpus_root.mkdir()
    target = corpus_root / "sample-a.bin"
    target.write_bytes(b"alpha")

    manifest = GoldenCorpusManifest.from_files(
        corpus_root,
        [target],
        generated_at="2026-04-14T00:00:00Z",
        rule_set_version="v1.0-baseline",
    )

    target.write_bytes(b"alpha-mutated")
    (corpus_root / "extra.bin").write_bytes(b"shadow")

    report = manifest.verify(corpus_root)

    assert report.passed is False
    assert report.missing == ()
    assert report.mismatched[0].relative_path == "sample-a.bin"
    assert report.unexpected == ("extra.bin",)
