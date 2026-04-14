from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from ac6_data_manager.release import build_smoke_report, write_evidence_manifest, write_smoke_report


class ReleaseEvidenceTests(unittest.TestCase):
    def test_build_smoke_report_passes_for_real_entry_and_preflight_pass(self) -> None:
        payload = build_smoke_report(
            Path("artifacts/release/AC6 saving manager"),
            provider_proof={
                "provider_kind": "service_bundle",
                "is_demo": False,
                "runtime_mode": "release",
            },
            preflight_report={
                "overall_status": "pass",
                "checks": [],
            },
        )

        self.assertEqual(payload["status"], "pass")
        self.assertEqual(payload["checks"]["provider-kind"]["status"], "pass")
        self.assertEqual(payload["checks"]["demo-gate"]["status"], "pass")
        self.assertEqual(payload["checks"]["preflight"]["status"], "pass")

    def test_write_smoke_report_blocks_fixture_or_preflight_failure(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir:
            output_path = Path(tempdir) / "smoke-baseline.json"
            write_smoke_report(
                output_path,
                release_root=Path(tempdir) / "AC6 saving manager",
                provider_proof={
                    "provider_kind": "fixture",
                    "is_demo": True,
                    "runtime_mode": "demo",
                },
                preflight_report={
                    "overall_status": "blocked",
                    "checks": [],
                },
            )
            payload = json.loads(output_path.read_text(encoding="utf-8"))

        self.assertEqual(payload["status"], "blocked")
        self.assertEqual(payload["checks"]["provider-kind"]["status"], "blocked")
        self.assertEqual(payload["checks"]["demo-gate"]["status"], "blocked")
        self.assertEqual(payload["checks"]["preflight"]["status"], "blocked")

    def test_write_evidence_manifest_keeps_smoke_report_pointer(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir:
            root = Path(tempdir)
            output_path = root / "evidence-manifest.json"
            smoke_report = root / "preflight" / "smoke-baseline.json"
            smoke_report.parent.mkdir(parents=True, exist_ok=True)
            smoke_report.write_text("{}", encoding="utf-8")
            release_manifest = root / "release" / "content-manifest.json"
            release_manifest.parent.mkdir(parents=True, exist_ok=True)
            release_manifest.write_text("{}", encoding="utf-8")

    def test_release_verdict_reaches_live_pass_when_complete(
        self,
    ) -> None:
        verdict = derive_release_verdict(
            {
                "zip": "pass",
                "first-launch": "pass",
                "preflight": "pass",
                "sidecar": "pass",
                "apply": "pass",
                "rollback": "pass",
                "second-launch": "pass",
            },
            preflight_ok=True,
            real_entry_ok=True,
            publish_ok=True,
            rollback_ok=True,
            game_verify_ok=True,
            evidence_complete=True,
        )

        self.assertEqual(payload["artifacts"]["smoke_report"], str(smoke_report))
