from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path

from ac6_data_manager.container_workspace.adapter import sha256_file
from ac6_data_manager.container_workspace.workspace import iso_now


@dataclass(frozen=True)
class ThirdPartyManifestFile:
    path: str
    sha256: str
    required: bool
    file_role: str
    size_bytes: int

    def to_dict(self) -> dict[str, object]:
        return {
            "path": self.path,
            "sha256": self.sha256,
            "required": self.required,
            "file_role": self.file_role,
            "size_bytes": self.size_bytes,
        }


@dataclass(frozen=True)
class ThirdPartyManifest:
    schema_version: int
    name: str
    source_baseline: str
    version_label: str
    entrypoint: str
    required: bool
    preflight_policy: str
    generated_at: str
    files: tuple[ThirdPartyManifestFile, ...]

    def to_dict(self) -> dict[str, object]:
        return {
            "schema_version": self.schema_version,
            "name": self.name,
            "source_baseline": self.source_baseline,
            "version_label": self.version_label,
            "entrypoint": self.entrypoint,
            "required": self.required,
            "preflight_policy": self.preflight_policy,
            "generated_at": self.generated_at,
            "files": [item.to_dict() for item in self.files],
        }

    def write(self, path: Path) -> Path:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(
            json.dumps(self.to_dict(), ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
        return path

    @classmethod
    def load(cls, path: Path) -> "ThirdPartyManifest":
        payload = json.loads(Path(path).read_text(encoding="utf-8"))
        return cls(
            schema_version=int(payload["schema_version"]),
            name=str(payload["name"]),
            source_baseline=str(payload["source_baseline"]),
            version_label=str(payload["version_label"]),
            entrypoint=str(payload["entrypoint"]),
            required=bool(payload["required"]),
            preflight_policy=str(payload["preflight_policy"]),
            generated_at=str(payload["generated_at"]),
            files=tuple(
                ThirdPartyManifestFile(
                    path=str(item["path"]),
                    sha256=str(item["sha256"]),
                    required=bool(item["required"]),
                    file_role=str(item["file_role"]),
                    size_bytes=int(item["size_bytes"]),
                )
                for item in payload.get("files", [])
            ),
        )


@dataclass(frozen=True)
class ManifestVerificationResult:
    success: bool
    manifest_path: str
    entrypoint_path: str
    actual_version_label: str
    expected_version_label: str | None
    missing_files: tuple[str, ...]
    extra_files: tuple[str, ...]
    hash_mismatches: tuple[dict[str, str], ...]
    errors: tuple[str, ...]

    def to_dict(self) -> dict[str, object]:
        return {
            "success": self.success,
            "manifest_path": self.manifest_path,
            "entrypoint_path": self.entrypoint_path,
            "actual_version_label": self.actual_version_label,
            "expected_version_label": self.expected_version_label,
            "missing_files": list(self.missing_files),
            "extra_files": list(self.extra_files),
            "hash_mismatches": list(self.hash_mismatches),
            "errors": list(self.errors),
        }


def _classify_role(relative_path: str, *, entrypoint: str) -> str:
    if relative_path == entrypoint:
        return "entrypoint"
    suffix = Path(relative_path).suffix.lower()
    if suffix == ".dll":
        return "dll"
    if suffix in {".json", ".config", ".ini", ".txt"}:
        return "config"
    if suffix in {".png", ".jpg", ".jpeg", ".bin"}:
        return "asset"
    return "other"


def build_manifest_for_release(
    app_root: Path,
    *,
    version_label: str,
    source_baseline: str,
) -> ThirdPartyManifest:
    app_root = Path(app_root)
    tool_root = app_root / "third_party" / "WitchyBND"
    entrypoint = "third_party/WitchyBND/WitchyBND.exe"
    files: list[ThirdPartyManifestFile] = []
    for file_path in sorted(tool_root.rglob("*")):
        if not file_path.is_file():
            continue
        relative_path = file_path.relative_to(app_root).as_posix()
        files.append(
            ThirdPartyManifestFile(
                path=relative_path,
                sha256=sha256_file(file_path),
                required=True,
                file_role=_classify_role(relative_path, entrypoint=entrypoint),
                size_bytes=file_path.stat().st_size,
            )
        )
    return ThirdPartyManifest(
        schema_version=1,
        name="WitchyBND",
        source_baseline=source_baseline,
        version_label=version_label,
        entrypoint=entrypoint,
        required=True,
        preflight_policy="fail_closed",
        generated_at=iso_now(),
        files=tuple(files),
    )


def verify_manifest(
    app_root: Path,
    manifest: ThirdPartyManifest,
    *,
    manifest_path: Path | None = None,
    expected_version_label: str | None = None,
) -> ManifestVerificationResult:
    app_root = Path(app_root)
    expected_files = {item.path: item for item in manifest.files}
    tool_root = app_root / "third_party" / "WitchyBND"
    actual_files: dict[str, str] = {}
    for file_path in sorted(tool_root.rglob("*")):
        if file_path.is_file():
            actual_files[file_path.relative_to(app_root).as_posix()] = sha256_file(file_path)

    missing_files = tuple(
        sorted(path for path, item in expected_files.items() if item.required and path not in actual_files)
    )
    extra_files = tuple(sorted(path for path in actual_files if path not in expected_files))
    hash_mismatches: list[dict[str, str]] = []
    for path, item in expected_files.items():
        actual_hash = actual_files.get(path)
        if actual_hash is None or actual_hash == item.sha256:
            continue
        hash_mismatches.append(
            {
                "path": path,
                "expected": item.sha256,
                "actual": actual_hash,
            }
        )

    errors: list[str] = []
    entrypoint_path = app_root / manifest.entrypoint
    if not entrypoint_path.exists():
        errors.append(f"entrypoint missing: {manifest.entrypoint}")
    if missing_files:
        errors.append(f"required files missing: {', '.join(missing_files)}")
    if extra_files:
        errors.append(f"unexpected extra files: {', '.join(extra_files)}")
    if hash_mismatches:
        errors.extend(f"sha256 mismatch: {item['path']}" for item in hash_mismatches)
    if expected_version_label and manifest.version_label != expected_version_label:
        errors.append(
            f"version mismatch: expected {expected_version_label}, got {manifest.version_label}"
        )

    return ManifestVerificationResult(
        success=not errors,
        manifest_path=str(manifest_path or app_root / "third_party" / "third_party_manifest.json"),
        entrypoint_path=str(entrypoint_path),
        actual_version_label=manifest.version_label,
        expected_version_label=expected_version_label,
        missing_files=missing_files,
        extra_files=extra_files,
        hash_mismatches=tuple(hash_mismatches),
        errors=tuple(errors),
    )
