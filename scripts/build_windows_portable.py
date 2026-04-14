from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
SRC_ROOT = REPO_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from ac6_data_manager.release import (
    APP_DIRNAME,
    run_preflight,
    stage_portable_release,
    verify_manifest,
    write_evidence_manifest,
    write_preflight_outputs,
    write_release_content_manifest,
)
from ac6_data_manager.release.runtime import emit_provider_proof


def main() -> int:
    parser = argparse.ArgumentParser(description="Build AC6 Data Manager Windows portable release.")
    parser.add_argument("--dist-app-root", type=Path, default=REPO_ROOT / "dist" / APP_DIRNAME)
    parser.add_argument("--release-base-dir", type=Path, default=REPO_ROOT / "artifacts" / "release")
    parser.add_argument("--third-party-source", type=Path, required=True)
    parser.add_argument("--version-label", required=True)
    parser.add_argument("--source-baseline", default="ACVIEmblemCreator bundled version")
    parser.add_argument("--skip-pyinstaller", action="store_true")
    args = parser.parse_args()

    if not args.skip_pyinstaller:
        spec_path = REPO_ROOT / "packaging" / "pyinstaller.spec"
        subprocess.run(
            [sys.executable, "-m", "PyInstaller", "--noconfirm", str(spec_path)],
            cwd=REPO_ROOT,
            check=True,
        )

    layout, manifest = stage_portable_release(
        args.dist_app_root,
        args.release_base_dir,
        third_party_source=args.third_party_source,
        packaging_assets_dir=REPO_ROOT / "packaging" / "assets",
        version_label=args.version_label,
        source_baseline=args.source_baseline,
    )

    preflight_root = REPO_ROOT / "artifacts" / "preflight"
    preflight_root.mkdir(parents=True, exist_ok=True)
    release_manifest_path = args.release_base_dir / "content-manifest.json"
    write_release_content_manifest(layout.app_root, release_manifest_path)
    provider_proof_path = preflight_root / "real-entry-proof.json"
    emit_provider_proof(provider_proof_path, argv=(), app_root=layout.app_root)

    third_party_result = verify_manifest(
        layout.app_root,
        manifest,
        expected_version_label=args.version_label,
    )
    third_party_report_path = preflight_root / "third-party-report.json"
    third_party_report_path.write_text(
        json.dumps(third_party_result.to_dict(), ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )

    report = run_preflight(layout.app_root, expected_version_label=args.version_label)
    preflight_json = preflight_root / "preflight-report.json"
    preflight_txt = preflight_root / "preflight-report.txt"
    write_preflight_outputs(report, json_path=preflight_json, text_path=preflight_txt)

    evidence_manifest_path = args.release_base_dir / "evidence-manifest.json"
    write_evidence_manifest(
        evidence_manifest_path,
        release_root=layout.app_root,
        release_content_manifest=release_manifest_path,
        third_party_report=third_party_report_path,
        preflight_report=preflight_json,
        runtime_proof=provider_proof_path,
        verdict="release-pending",
        notes=("Requires ZPX-GE77 live acceptance evidence before live-pass.",),
    )

    summary = {
        "release_root": str(layout.app_root),
        "preflight_status": report.overall_status,
        "third_party_success": third_party_result.success,
        "evidence_manifest": str(evidence_manifest_path),
    }
    sys.stdout.write(json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
