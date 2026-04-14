from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
SRC_ROOT = REPO_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))

from ac6_data_manager.release import ThirdPartyManifest, verify_manifest


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify bundled WitchyBND directory manifest.")
    parser.add_argument("--release-root", type=Path, required=True)
    parser.add_argument("--expected-version-label")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    manifest_path = args.release_root / "third_party" / "third_party_manifest.json"
    manifest = ThirdPartyManifest.load(manifest_path)
    result = verify_manifest(
        args.release_root,
        manifest,
        manifest_path=manifest_path,
        expected_version_label=args.expected_version_label,
    )
    rendered = json.dumps(result.to_dict(), ensure_ascii=False, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
    sys.stdout.write(rendered)
    return 0 if result.success else 2


if __name__ == "__main__":
    raise SystemExit(main())
