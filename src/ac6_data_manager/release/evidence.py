from __future__ import annotations

import json
from pathlib import Path
from typing import Mapping

from ac6_data_manager.container_workspace.adapter import sha256_file
from ac6_data_manager.container_workspace.workspace import iso_now


def collect_release_content_manifest(app_root: Path) -> list[dict[str, object]]:
    app_root = Path(app_root)
    entries: list[dict[str, object]] = []
    for file_path in sorted(app_root.rglob("*")):
        if not file_path.is_file():
            continue
        entries.append(
            {
                "path": file_path.relative_to(app_root).as_posix(),
                "size_bytes": file_path.stat().st_size,
                "sha256": sha256_file(file_path),
            }
        )
    return entries


def write_release_content_manifest(app_root: Path, output_path: Path) -> Path:
    payload = {
        "generated_at": iso_now(),
        "app_root": str(Path(app_root)),
        "files": collect_release_content_manifest(app_root),
    }
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    return output_path


def derive_release_verdict(
    huorong_matrix: Mapping[str, str] | None,
    *,
    preflight_ok: bool,
    real_entry_ok: bool,
    publish_ok: bool | None,
    rollback_ok: bool | None,
    game_verify_ok: bool,
    evidence_complete: bool,
) -> str:
    matrix = dict(huorong_matrix or {})
    normalized = {step: value.strip().lower() for step, value in matrix.items()}
    if any(value == "blocked" for value in normalized.values()):
        return "fail"
    if not preflight_ok or not real_entry_ok:
        return "fail"
    if publish_ok is False or rollback_ok is False:
        return "fail"
    if not evidence_complete or publish_ok is None or rollback_ok is None or not game_verify_ok:
        return "release-pending"
    if any(value == "manual-allow" for value in normalized.values()):
        return "conditional-pass"
    return "live-pass"


def write_evidence_manifest(
    output_path: Path,
    *,
    release_root: Path,
    release_content_manifest: Path,
    third_party_report: Path | None = None,
    preflight_report: Path | None = None,
    runtime_proof: Path | None = None,
    smoke_report: Path | None = None,
    publish_audit: Path | None = None,
    rollback_report: Path | None = None,
    huorong_matrix: Path | None = None,
    checklist_capture: Path | None = None,
    verdict: str,
    notes: list[str] | tuple[str, ...] = (),
) -> Path:
    payload = {
        "generated_at": iso_now(),
        "release_root": str(Path(release_root)),
        "verdict": verdict,
        "artifacts": {
            "release_content_manifest": str(release_content_manifest),
            "third_party_report": None if third_party_report is None else str(third_party_report),
            "preflight_report": None if preflight_report is None else str(preflight_report),
            "runtime_proof": None if runtime_proof is None else str(runtime_proof),
            "smoke_report": None if smoke_report is None else str(smoke_report),
            "publish_audit": None if publish_audit is None else str(publish_audit),
            "rollback_report": None if rollback_report is None else str(rollback_report),
            "huorong_matrix": None if huorong_matrix is None else str(huorong_matrix),
            "checklist_capture": None if checklist_capture is None else str(checklist_capture),
        },
        "notes": list(notes),
    }
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    return output_path
