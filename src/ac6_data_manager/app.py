from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from .gui import Ac6DataManagerWindow
from .gui.main_window import create_application
from .release.runtime import build_demo_provider, build_provider_bundle


def main(argv: list[str] | None = None) -> int:
    argv = list(argv or sys.argv)
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--demo", action="store_true")
    parser.add_argument("--provider-proof-json")
    parser.add_argument("--headless-provider-check", action="store_true")
    options, _ = parser.parse_known_args(argv[1:])

    bundle = build_provider_bundle(argv=argv[1:])
    if options.provider_proof_json or options.headless_provider_check:
        payload = bundle.context.to_dict()
        rendered = json.dumps(payload, ensure_ascii=False, indent=2) + "\n"
        if options.provider_proof_json:
            target = Path(options.provider_proof_json)
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(rendered, encoding="utf-8")
        else:
            sys.stdout.write(rendered)
        return 0

    app = create_application()
    window = Ac6DataManagerWindow(bundle.provider)
    window.show()
    return app.exec_()


if __name__ == "__main__":
    raise SystemExit(main())
