from __future__ import annotations

import argparse
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
SRC_ROOT = REPO_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from ac6_data_manager.release import write_evidence_manifest


def _require_existing(label: str, path: Path) -> Path:
    resolved = Path(path)
    if not resolved.exists():
        raise FileNotFoundError(f"{label} does not exist: {resolved}")
    return resolved


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Record live acceptance evidence into the release evidence manifest. "
            "Any provided artifact path must already exist."
        )
    )
    parser.add_argument("--release-root", type=Path, required=True)
    parser.add_argument("--release-content-manifest", type=Path, required=True)
    parser.add_argument(
        "--output-path",
        type=Path,
        default=REPO_ROOT / "artifacts" / "release" / "evidence-manifest.json",
    )
    parser.add_argument("--third-party-report", type=Path)
    parser.add_argument("--preflight-report", type=Path)
    parser.add_argument("--runtime-proof", type=Path)
    parser.add_argument("--smoke-report", type=Path)
    parser.add_argument("--publish-audit", type=Path)
    parser.add_argument("--readback-report", type=Path)
    parser.add_argument("--rollback-report", type=Path)
    parser.add_argument("--incident-artifact", type=Path)
    parser.add_argument("--huorong-matrix", type=Path)
    parser.add_argument("--checklist-capture", type=Path)
    parser.add_argument("--verdict", default="release-pending")
    parser.add_argument(
        "--note",
        action="append",
        default=[],
        help="Append a note to the evidence manifest. Can be specified multiple times.",
    )
    args = parser.parse_args()

    release_root = _require_existing("release-root", args.release_root)
    release_content_manifest = _require_existing(
        "release-content-manifest", args.release_content_manifest
    )

    def optional(label: str, path: Path | None) -> Path | None:
        if path is None:
            return None
        return _require_existing(label, path)

    output_path = write_evidence_manifest(
        args.output_path,
        release_root=release_root,
        release_content_manifest=release_content_manifest,
        third_party_report=optional("third-party-report", args.third_party_report),
        preflight_report=optional("preflight-report", args.preflight_report),
        runtime_proof=optional("runtime-proof", args.runtime_proof),
        smoke_report=optional("smoke-report", args.smoke_report),
        publish_audit=optional("publish-audit", args.publish_audit),
        readback_report=optional("readback-report", args.readback_report),
        rollback_report=optional("rollback-report", args.rollback_report),
        incident_artifact=optional("incident-artifact", args.incident_artifact),
        huorong_matrix=optional("huorong-matrix", args.huorong_matrix),
        checklist_capture=optional("checklist-capture", args.checklist_capture),
        verdict=args.verdict,
        notes=tuple(args.note),
    )
    sys.stdout.write(str(output_path) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
