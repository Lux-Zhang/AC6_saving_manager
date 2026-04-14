from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from ac6_data_manager.release import (
    build_manifest_for_release,
    derive_release_verdict,
    run_preflight,
)


class ReleasePreflightTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tempdir = tempfile.TemporaryDirectory()
        self.app_root = Path(self.tempdir.name) / "AC6 saving manager"
        (self.app_root / "runtime").mkdir(parents=True, exist_ok=True)
        (self.app_root / "resources").mkdir(parents=True, exist_ok=True)
        (self.app_root / "docs").mkdir(parents=True, exist_ok=True)
        (self.app_root / "third_party" / "WitchyBND").mkdir(parents=True, exist_ok=True)
        (self.app_root / "AC6 saving manager.exe").write_bytes(b"exe")
        (self.app_root / "docs" / "QUICK_START.txt").write_text("quick", encoding="utf-8")
        (self.app_root / "docs" / "LIVE_ACCEPTANCE_CHECKLIST.txt").write_text("checklist", encoding="utf-8")
        (self.app_root / "docs" / "KNOWN_LIMITATIONS.txt").write_text("limits", encoding="utf-8")
        (self.app_root / "third_party" / "WitchyBND" / "WitchyBND.exe").write_bytes(b"exe")
        manifest = build_manifest_for_release(
            self.app_root,
            version_label="acvi-1.0",
            source_baseline="baseline-ref",
        )
        manifest.write(self.app_root / "third_party" / "third_party_manifest.json")

    def tearDown(self) -> None:
        self.tempdir.cleanup()

    def test_run_preflight_reports_pass_for_complete_release_layout(self) -> None:
        report = run_preflight(
            self.app_root,
            expected_version_label="acvi-1.0",
            platform_system="Windows",
            machine="AMD64",
        )

        self.assertEqual(report.overall_status, "pass")
        check_ids = {check.check_id: check.status for check in report.checks}
        self.assertEqual(check_ids["runtime-entry"], "pass")
        self.assertEqual(check_ids["third-party-manifest"], "pass")

    def test_preflight_runtime_entry_reports_release_self_check_details(self) -> None:
        report = run_preflight(
            self.app_root,
            expected_version_label="acvi-1.0",
            environ={"PATH": r"C:\\Fake\\Toolchain"},
            platform_system="Windows",
            machine="AMD64",
        )

        runtime_check = next(check for check in report.checks if check.check_id == "runtime-entry")

        self.assertEqual(runtime_check.status, "pass")
        self.assertEqual(runtime_check.details["provider_kind"], "service_bundle")
        self.assertEqual(runtime_check.details["runtime_mode"], "release")
        self.assertEqual(runtime_check.details["toolchain_policy"], "bundled-only")
        self.assertFalse(runtime_check.details["is_demo"])

    def test_preflight_blocks_explicit_demo_runtime_entry(self) -> None:
        report = run_preflight(
            self.app_root,
            expected_version_label="acvi-1.0",
            argv=("--demo",),
            platform_system="Windows",
            machine="AMD64",
        )

        runtime_check = next(check for check in report.checks if check.check_id == "runtime-entry")

        self.assertEqual(runtime_check.status, "blocked")
        self.assertEqual(runtime_check.summary, "默认入口落入 demo/fixture provider")
        self.assertEqual(report.overall_status, "blocked")

    def test_release_verdict_stays_pending_without_live_game_verification(self) -> None:
        verdict = derive_release_verdict(
            {
                "zip": "pass",
                "first-launch": "pass",
                "preflight": "pass",
                "sidecar": "pass",
                "apply": "pass",
                "rollback": "pass",
            },
            preflight_ok=True,
            real_entry_ok=True,
            publish_ok=True,
            rollback_ok=True,
            game_verify_ok=False,
            evidence_complete=True,
        )

        self.assertEqual(verdict, "release-pending")
