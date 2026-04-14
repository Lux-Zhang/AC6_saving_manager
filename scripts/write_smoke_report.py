from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
SRC_ROOT = REPO_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from ac6_data_manager.release import write_smoke_report


def main() -> int:
    parser = argparse.ArgumentParser(description="Compose portable release smoke baseline report.")
    parser.add_argument("--release-root", type=Path, required=True)
    parser.add_argument("--provider-proof", type=Path, required=True)
    parser.add_argument("--preflight-report", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    provider_proof = json.loads(args.provider_proof.read_text(encoding="utf-8"))
    preflight_report = json.loads(args.preflight_report.read_text(encoding="utf-8"))
    write_smoke_report(
        args.output,
        release_root=args.release_root,
        provider_proof=provider_proof,
        preflight_report=preflight_report,
    )
    sys.stdout.write(str(args.output) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
