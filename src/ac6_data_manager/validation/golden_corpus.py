from __future__ import annotations

from dataclasses import dataclass, field
import hashlib
import json
from pathlib import Path
from typing import Iterable, Mapping


DEFAULT_HASH_ALGORITHM = "sha256"


def compute_sha256_bytes(payload: bytes) -> str:
    digest = hashlib.sha256()
    digest.update(payload)
    return digest.hexdigest()


def compute_sha256_file(path: str | Path) -> str:
    digest = hashlib.sha256()
    with Path(path).open("rb") as handle:
        for chunk in iter(lambda: handle.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


@dataclass(frozen=True, slots=True)
class GoldenCorpusEntry:
    logical_name: str
    relative_path: str
    sha256: str
    byte_size: int
    tags: tuple[str, ...] = ()
    expectations: Mapping[str, str] = field(default_factory=dict)

    def to_dict(self) -> dict[str, object]:
        return {
            "logical_name": self.logical_name,
            "relative_path": self.relative_path,
            "sha256": self.sha256,
            "byte_size": self.byte_size,
            "tags": list(self.tags),
            "expectations": dict(self.expectations),
        }

    @classmethod
    def from_dict(cls, payload: Mapping[str, object]) -> "GoldenCorpusEntry":
        return cls(
            logical_name=str(payload["logical_name"]),
            relative_path=str(payload["relative_path"]),
            sha256=str(payload["sha256"]),
            byte_size=int(payload["byte_size"]),
            tags=tuple(str(tag) for tag in payload.get("tags", [])),
            expectations={
                str(key): str(value)
                for key, value in dict(payload.get("expectations", {})).items()
            },
        )


@dataclass(frozen=True, slots=True)
class HashMismatch:
    relative_path: str
    expected_sha256: str
    actual_sha256: str | None


@dataclass(frozen=True, slots=True)
class GoldenCorpusVerificationReport:
    verified: tuple[str, ...]
    missing: tuple[str, ...]
    mismatched: tuple[HashMismatch, ...]
    unexpected: tuple[str, ...]

    @property
    def passed(self) -> bool:
        return not self.missing and not self.mismatched and not self.unexpected


@dataclass(frozen=True, slots=True)
class GoldenCorpusManifest:
    version: str
    rule_set_version: str
    generated_at: str
    entries: tuple[GoldenCorpusEntry, ...]
    metadata: Mapping[str, str] = field(default_factory=dict)
    hash_algorithm: str = DEFAULT_HASH_ALGORITHM

    @classmethod
    def from_files(
        cls,
        base_dir: str | Path,
        files: Iterable[str | Path],
        *,
        generated_at: str,
        rule_set_version: str,
        metadata: Mapping[str, str] | None = None,
    ) -> "GoldenCorpusManifest":
        base_path = Path(base_dir).resolve()
        entries: list[GoldenCorpusEntry] = []
        for file_path in files:
            candidate = Path(file_path).resolve()
            relative_path = candidate.relative_to(base_path)
            entries.append(
                GoldenCorpusEntry(
                    logical_name=relative_path.stem,
                    relative_path=relative_path.as_posix(),
                    sha256=compute_sha256_file(candidate),
                    byte_size=candidate.stat().st_size,
                )
            )
        return cls(
            version="v1",
            rule_set_version=rule_set_version,
            generated_at=generated_at,
            entries=tuple(sorted(entries, key=lambda entry: entry.relative_path)),
            metadata=dict(metadata or {}),
        )

    def to_dict(self) -> dict[str, object]:
        return {
            "version": self.version,
            "rule_set_version": self.rule_set_version,
            "generated_at": self.generated_at,
            "hash_algorithm": self.hash_algorithm,
            "metadata": dict(self.metadata),
            "entries": [entry.to_dict() for entry in self.entries],
        }

    @classmethod
    def from_dict(cls, payload: Mapping[str, object]) -> "GoldenCorpusManifest":
        return cls(
            version=str(payload["version"]),
            rule_set_version=str(payload["rule_set_version"]),
            generated_at=str(payload["generated_at"]),
            hash_algorithm=str(payload.get("hash_algorithm", DEFAULT_HASH_ALGORITHM)),
            metadata={str(key): str(value) for key, value in dict(payload.get("metadata", {})).items()},
            entries=tuple(
                GoldenCorpusEntry.from_dict(entry_payload)
                for entry_payload in payload.get("entries", [])
            ),
        )

    def write(self, path: str | Path) -> Path:
        destination = Path(path)
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(
            json.dumps(self.to_dict(), ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        return destination

    @classmethod
    def read(cls, path: str | Path) -> "GoldenCorpusManifest":
        return cls.from_dict(json.loads(Path(path).read_text(encoding="utf-8")))

    def verify(
        self,
        base_dir: str | Path,
        *,
        allow_unexpected: bool = False,
    ) -> GoldenCorpusVerificationReport:
        base_path = Path(base_dir)
        verified: list[str] = []
        missing: list[str] = []
        mismatched: list[HashMismatch] = []

        expected_paths = {entry.relative_path for entry in self.entries}
        for entry in self.entries:
            candidate = base_path / entry.relative_path
            if not candidate.exists():
                missing.append(entry.relative_path)
                continue
            actual_sha256 = compute_sha256_file(candidate)
            if actual_sha256 != entry.sha256:
                mismatched.append(
                    HashMismatch(
                        relative_path=entry.relative_path,
                        expected_sha256=entry.sha256,
                        actual_sha256=actual_sha256,
                    )
                )
                continue
            verified.append(entry.relative_path)

        unexpected: list[str] = []
        if not allow_unexpected:
            for candidate in base_path.rglob("*"):
                if not candidate.is_file():
                    continue
                relative_path = candidate.relative_to(base_path).as_posix()
                if relative_path not in expected_paths:
                    unexpected.append(relative_path)

        return GoldenCorpusVerificationReport(
            verified=tuple(sorted(verified)),
            missing=tuple(sorted(missing)),
            mismatched=tuple(sorted(mismatched, key=lambda item: item.relative_path)),
            unexpected=tuple(sorted(unexpected)),
        )
