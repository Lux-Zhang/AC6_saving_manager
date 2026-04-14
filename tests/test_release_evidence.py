from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from ac6_data_manager.release import (
    build_smoke_report,
    derive_release_verdict,
    write_evidence_manifest,
    write_live_acceptance_capture_templates,
    write_smoke_report,
)


REPO_ROOT = Path(__file__).resolve().parents[1]


class ReleaseEvidenceTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tempdir = tempfile.TemporaryDirectory()
        self.root = Path(self.tempdir.name)
        self.release_root = self.root / "release" / "AC6 saving manager"
        self.release_root.mkdir(parents=True, exist_ok=True)
        (self.release_root / "AC6 saving manager.exe").write_bytes(b"exe")

        self.release_content_manifest = (
            self.root / "artifacts" / "release" / "content-manifest.json"
        )
        self.release_content_manifest.parent.mkdir(parents=True, exist_ok=True)
        self.release_content_manifest.write_text("{}", encoding="utf-8")

        self.third_party_report = (
            self.root / "artifacts" / "preflight" / "third-party-report.json"
        )
        self.preflight_report = (
            self.root / "artifacts" / "preflight" / "preflight-report.json"
        )
        self.runtime_proof = (
            self.root / "artifacts" / "preflight" / "real-entry-proof.json"
        )
        self.smoke_report = (
            self.root / "artifacts" / "preflight" / "smoke-baseline.json"
        )
        self.publish_audit = (
            self.root / "artifacts" / "live_acceptance" / "publish-audit-1.json"
        )
        self.readback_report = (
            self.root / "artifacts" / "live_acceptance" / "readback-1.json"
        )
        self.rollback_report = (
            self.root / "artifacts" / "live_acceptance" / "rollback-1.json"
        )
        self.incident_artifact = (
            self.root / "artifacts" / "live_acceptance" / "incident-1.json"
        )
        self.huorong_matrix = self.root / "artifacts" / "huorong" / "matrix.json"
        self.checklist_capture = (
            self.root / "artifacts" / "live_acceptance" / "checklist-manual.md"
        )

        for path in (
            self.third_party_report,
            self.preflight_report,
            self.runtime_proof,
            self.smoke_report,
            self.publish_audit,
            self.readback_report,
            self.rollback_report,
            self.incident_artifact,
            self.huorong_matrix,
            self.checklist_capture,
        ):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("{}", encoding="utf-8")

    def tearDown(self) -> None:
        self.tempdir.cleanup()

    def test_build_smoke_report_passes_for_real_entry_and_preflight_pass(self) -> None:
        payload = build_smoke_report(
            self.release_root,
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
        output_path = self.root / "artifacts" / "preflight" / "smoke-blocked.json"
        write_smoke_report(
            output_path,
            release_root=self.release_root,
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

    def test_derive_release_verdict_reaches_live_pass_when_complete(self) -> None:
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

        self.assertEqual(verdict, "live-pass")

    def test_write_evidence_manifest_records_smoke_readback_and_incident_paths(self) -> None:
        output_path = self.root / "artifacts" / "release" / "evidence-manifest.json"

        write_evidence_manifest(
            output_path,
            release_root=self.release_root,
            release_content_manifest=self.release_content_manifest,
            third_party_report=self.third_party_report,
            preflight_report=self.preflight_report,
            runtime_proof=self.runtime_proof,
            smoke_report=self.smoke_report,
            publish_audit=self.publish_audit,
            readback_report=self.readback_report,
            rollback_report=self.rollback_report,
            incident_artifact=self.incident_artifact,
            huorong_matrix=self.huorong_matrix,
            checklist_capture=self.checklist_capture,
            verdict="release-pending",
            notes=("worker-3 scope",),
        )

        payload = json.loads(output_path.read_text(encoding="utf-8"))
        artifacts = payload["artifacts"]
        self.assertEqual(artifacts["smoke_report"], str(self.smoke_report))
        self.assertEqual(artifacts["publish_audit"], str(self.publish_audit))
        self.assertEqual(artifacts["readback_report"], str(self.readback_report))
        self.assertEqual(artifacts["rollback_report"], str(self.rollback_report))
        self.assertEqual(artifacts["incident_artifact"], str(self.incident_artifact))

    def test_record_live_acceptance_evidence_script_requires_existing_inputs(self) -> None:
        output_path = self.root / "artifacts" / "release" / "evidence-manifest.from-script.json"
        command = [
            sys.executable,
            str(REPO_ROOT / "scripts" / "record_live_acceptance_evidence.py"),
            "--release-root",
            str(self.release_root),
            "--release-content-manifest",
            str(self.release_content_manifest),
            "--output-path",
            str(output_path),
            "--third-party-report",
            str(self.third_party_report),
            "--preflight-report",
            str(self.preflight_report),
            "--runtime-proof",
            str(self.runtime_proof),
            "--smoke-report",
            str(self.smoke_report),
            "--publish-audit",
            str(self.publish_audit),
            "--readback-report",
            str(self.readback_report),
            "--rollback-report",
            str(self.rollback_report),
            "--incident-artifact",
            str(self.incident_artifact),
            "--huorong-matrix",
            str(self.huorong_matrix),
            "--checklist-capture",
            str(self.checklist_capture),
            "--verdict",
            "release-pending",
            "--note",
            "manual evidence capture pending",
        ]

        completed = subprocess.run(
            command,
            cwd=REPO_ROOT,
            check=True,
            capture_output=True,
            text=True,
        )

        self.assertEqual(completed.stdout.strip(), str(output_path))
        payload = json.loads(output_path.read_text(encoding="utf-8"))
        self.assertEqual(payload["artifacts"]["smoke_report"], str(self.smoke_report))
        self.assertEqual(
            payload["artifacts"]["readback_report"],
            str(self.readback_report),
        )
        self.assertEqual(
            payload["artifacts"]["incident_artifact"],
            str(self.incident_artifact),
        )

    def test_write_live_acceptance_capture_templates_writes_context_and_matrix(self) -> None:
        output_dir = self.root / "artifacts" / "live_acceptance"
        huorong_dir = self.root / "artifacts" / "huorong"

        outputs = write_live_acceptance_capture_templates(
            output_dir,
            release_root=self.release_root,
            huorong_dir=huorong_dir,
            machine="ZPX-GE77",
            operator="qa-user",
        )

        self.assertIn("checklist_capture", outputs)
        self.assertIn("capture_context", outputs)
        self.assertIn("huorong_matrix_template", outputs)
        checklist_text = Path(outputs["checklist_capture"]).read_text(encoding="utf-8")
        self.assertIn("apply -> fresh destination", checklist_text)
        context = json.loads(Path(outputs["capture_context"]).read_text(encoding="utf-8"))
        self.assertTrue(context["contract"]["fresh_destination_only"])
        matrix = json.loads(
            Path(outputs["huorong_matrix_template"]).read_text(encoding="utf-8")
        )
        self.assertEqual(matrix["steps"][0]["step"], "unzip")
        self.assertEqual(matrix["steps"][0]["result"], "pending")

    def test_prepare_live_acceptance_capture_script_writes_expected_artifacts(self) -> None:
        output_dir = self.root / "artifacts" / "live_acceptance-cli"
        huorong_dir = self.root / "artifacts" / "huorong-cli"
        command = [
            sys.executable,
            str(REPO_ROOT / "scripts" / "prepare_live_acceptance_capture.py"),
            "--release-root",
            str(self.release_root),
            "--output-dir",
            str(output_dir),
            "--huorong-dir",
            str(huorong_dir),
            "--machine",
            "ZPX-GE77",
            "--operator",
            "qa-user",
        ]

        completed = subprocess.run(
            command,
            cwd=REPO_ROOT,
            check=True,
            capture_output=True,
            text=True,
        )

        payload = json.loads(completed.stdout)
        self.assertTrue(Path(payload["checklist_capture"]).exists())
        self.assertTrue(Path(payload["capture_context"]).exists())
        self.assertTrue(Path(payload["huorong_matrix_template"]).exists())
