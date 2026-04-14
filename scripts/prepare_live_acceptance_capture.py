from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
SRC_ROOT = REPO_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from ac6_data_manager.release import write_live_acceptance_capture_templates


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Prepare live acceptance checklist and Huorong template artifacts "
            "for the Windows release validation run."
        )
    )
    parser.add_argument("--release-root", type=Path, required=True)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=REPO_ROOT / "artifacts" / "live_acceptance",
    )
    parser.add_argument(
        "--huorong-dir",
        type=Path,
        default=REPO_ROOT / "artifacts" / "huorong",
    )
    parser.add_argument("--machine", default="ZPX-GE77")
    parser.add_argument("--operator", default="fill manually")
    args = parser.parse_args()

    if not args.release_root.exists():
        raise FileNotFoundError(f"release-root does not exist: {args.release_root}")

    payload = write_live_acceptance_capture_templates(
        args.output_dir,
        release_root=args.release_root,
        huorong_dir=args.huorong_dir,
        machine=args.machine,
        operator=args.operator,
    )
    sys.stdout.write(json.dumps(payload, ensure_ascii=False, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
