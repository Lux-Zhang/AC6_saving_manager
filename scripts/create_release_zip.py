from __future__ import annotations

import argparse
import sys
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile


def main() -> int:
    parser = argparse.ArgumentParser(description="Create portable zip from release directory.")
    parser.add_argument("--release-root", type=Path, required=True)
    parser.add_argument("--output-zip", type=Path, required=True)
    args = parser.parse_args()

    args.output_zip.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(args.output_zip, mode="w", compression=ZIP_DEFLATED) as archive:
        for file_path in sorted(args.release_root.rglob("*")):
            if file_path.is_file():
                archive.write(file_path, arcname=file_path.relative_to(args.release_root.parent))
    sys.stdout.write(str(args.output_zip) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
