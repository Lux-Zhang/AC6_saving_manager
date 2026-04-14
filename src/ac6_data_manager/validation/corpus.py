from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable, Mapping

from ac6_data_manager.container_workspace.adapter import sha256_file
from ac6_data_manager.container_workspace.workspace import iso_now


@dataclass(frozen=True)
class GoldenCorpusEntry:
    sample_id: str
    relative_path: str
    sha256: str
    size: int
    kind: str
    tags: tuple[str, ...]

    @classmethod
    def from_dict(cls, payload: Mapping[str, Any]) -> "GoldenCorpusEntry":
        return cls(
            sample_id=str(payload["sample_id"]),
            relative_path=str(payload["relative_path"]),
            sha256=str(payload["sha256"]),
            size=int(payload["size"]),
            kind=str(payload["kind"]),
            tags=tuple(str(item) for item in payload.get("tags", ())),
        )

    def to_dict(self) -> dict[str, Any]:
        return {
            "sample_id": self.sample_id,
            "relative_path": self.relative_path,
            "sha256": self.sha256,
            "size": self.size,
            "kind": self.kind,
            "tags": list(self.tags),
        }


@dataclass(frozen=True)
class GoldenCorpusManifest:
    dataset_name: str
    generated_at: str
    corpus_root: str
    rule_set_version: str
    entries: tuple[GoldenCorpusEntry, ...]
    metadata: dict[str, Any]

    @classmethod
    def from_dict(cls, payload: Mapping[str, Any]) -> "GoldenCorpusManifest":
        return cls(
            dataset_name=str(payload["dataset_name"]),
            generated_at=str(payload["generated_at"]),
            corpus_root=str(payload["corpus_root"]),
            rule_set_version=str(payload["rule_set_version"]),
            entries=tuple(
                GoldenCorpusEntry.from_dict(item) for item in payload.get("entries", ())
            ),
            metadata=dict(payload.get("metadata") or {}),
        )

    def to_dict(self) -> dict[str, Any]:
        return {
            "dataset_name": self.dataset_name,
            "generated_at": self.generated_at,
            "corpus_root": self.corpus_root,
            "rule_set_version": self.rule_set_version,
            "entries": [entry.to_dict() for entry in self.entries],
            "metadata": dict(self.metadata),
        }


@dataclass(frozen=True)
class GoldenCorpusVerification:
    ok: bool
    missing: tuple[str, ...]
    changed: tuple[str, ...]
    unexpected: tuple[str, ...]


class GoldenCorpusManifestHelper:
    def __init__(self, corpus_root: Path) -> None:
        self.corpus_root = Path(corpus_root)

    def build_manifest(
        self,
        *,
        manifest_path: Path,
        dataset_name: str,
        sample_paths: Iterable[Path] | None = None,
        kind: str = "emblem",
        rule_set_version: str = "m0-baseline",
        tags_by_relative_path: Mapping[str, Iterable[str]] | None = None,
        metadata: Mapping[str, Any] | None = None,
    ) -> GoldenCorpusManifest:
        candidates = tuple(sample_paths) if sample_paths is not None else tuple(
            path for path in sorted(self.corpus_root.rglob("*")) if path.is_file()
        )
        entries: list[GoldenCorpusEntry] = []
        tag_map = tags_by_relative_path or {}
        for file_path in candidates:
            resolved = Path(file_path)
            relative_path = resolved.relative_to(self.corpus_root).as_posix()
            entries.append(
                GoldenCorpusEntry(
                    sample_id=resolved.stem,
                    relative_path=relative_path,
                    sha256=sha256_file(resolved),
                    size=resolved.stat().st_size,
                    kind=kind,
                    tags=tuple(sorted(str(item) for item in tag_map.get(relative_path, ()))),
                )
            )
        manifest = GoldenCorpusManifest(
            dataset_name=dataset_name,
            generated_at=iso_now(),
            corpus_root=str(self.corpus_root),
            rule_set_version=rule_set_version,
            entries=tuple(entries),
            metadata=dict(metadata or {}),
        )
        manifest_path = Path(manifest_path)
        manifest_path.parent.mkdir(parents=True, exist_ok=True)
        manifest_path.write_text(
            json.dumps(manifest.to_dict(), ensure_ascii=True, indent=2) + "\n",
            encoding="utf-8",
        )
        return manifest

    def load_manifest(self, manifest_path: Path) -> GoldenCorpusManifest:
        return GoldenCorpusManifest.from_dict(
            json.loads(Path(manifest_path).read_text(encoding="utf-8"))
        )

    def verify_manifest(
        self,
        manifest: GoldenCorpusManifest | Path,
    ) -> GoldenCorpusVerification:
        loaded = manifest if isinstance(manifest, GoldenCorpusManifest) else self.load_manifest(manifest)
        expected = {entry.relative_path: entry.sha256 for entry in loaded.entries}
        actual = {
            path.relative_to(self.corpus_root).as_posix(): sha256_file(path)
            for path in sorted(self.corpus_root.rglob("*"))
            if path.is_file()
        }
        expected_keys = set(expected)
        actual_keys = set(actual)
        missing = tuple(sorted(expected_keys - actual_keys))
        unexpected = tuple(sorted(actual_keys - expected_keys))
        changed = tuple(
            sorted(
                relative_path
                for relative_path in expected_keys & actual_keys
                if expected[relative_path] != actual[relative_path]
            )
        )
        return GoldenCorpusVerification(
            ok=not (missing or changed or unexpected),
            missing=missing,
            changed=changed,
            unexpected=unexpected,
        )
