from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
SRC_ROOT = REPO_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from ac6_data_manager.release import run_preflight, write_preflight_outputs
from ac6_data_manager.release.runtime import emit_provider_proof


def main() -> int:
    parser = argparse.ArgumentParser(description="Run AC6 Data Manager portable release preflight.")
    parser.add_argument("--release-root", type=Path, required=True)
    parser.add_argument("--expected-version-label")
    parser.add_argument("--json-out", type=Path)
    parser.add_argument("--text-out", type=Path)
    parser.add_argument("--provider-proof-out", type=Path)
    args = parser.parse_args()

    report = run_preflight(
        args.release_root,
        expected_version_label=args.expected_version_label,
    )
    if args.provider_proof_out:
        emit_provider_proof(args.provider_proof_out, argv=(), app_root=args.release_root)
    if args.json_out and args.text_out:
        write_preflight_outputs(report, json_path=args.json_out, text_path=args.text_out)
    rendered = json.dumps(report.to_dict(), ensure_ascii=False, indent=2) + "\n"
    sys.stdout.write(rendered)
    return 0 if report.overall_status == "pass" else 2


if __name__ == "__main__":
    raise SystemExit(main())
