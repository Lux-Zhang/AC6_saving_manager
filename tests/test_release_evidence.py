from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from ac6_data_manager.release import write_evidence_manifest


REPO_ROOT = Path(__file__).resolve().parents[1]


class ReleaseEvidenceTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tempdir = tempfile.TemporaryDirectory()
        self.root = Path(self.tempdir.name)
        self.release_root = self.root / "release" / "AC6 saving manager"
        self.release_root.mkdir(parents=True, exist_ok=True)
        (self.release_root / "AC6 saving manager.exe").write_bytes(b"exe")

        self.release_content_manifest = self.root / "artifacts" / "release" / "content-manifest.json"
        self.release_content_manifest.parent.mkdir(parents=True, exist_ok=True)
        self.release_content_manifest.write_text("{}", encoding="utf-8")

        self.third_party_report = self.root / "artifacts" / "preflight" / "third-party-report.json"
        self.preflight_report = self.root / "artifacts" / "preflight" / "preflight-report.json"
        self.runtime_proof = self.root / "artifacts" / "preflight" / "real-entry-proof.json"
        self.publish_audit = self.root / "artifacts" / "live_acceptance" / "publish-audit-1.json"
        self.readback_report = self.root / "artifacts" / "live_acceptance" / "readback-1.json"
        self.rollback_report = self.root / "artifacts" / "live_acceptance" / "rollback-1.json"
        self.incident_artifact = self.root / "artifacts" / "live_acceptance" / "incident-1.json"
        self.huorong_matrix = self.root / "artifacts" / "huorong" / "matrix.json"
        self.checklist_capture = self.root / "artifacts" / "live_acceptance" / "checklist-manual.md"

        for path in (
            self.third_party_report,
            self.preflight_report,
            self.runtime_proof,
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

    def test_write_evidence_manifest_records_readback_and_incident_paths(self) -> None:
        output_path = self.root / "artifacts" / "release" / "evidence-manifest.json"

        write_evidence_manifest(
            output_path,
            release_root=self.release_root,
            release_content_manifest=self.release_content_manifest,
            third_party_report=self.third_party_report,
            preflight_report=self.preflight_report,
            runtime_proof=self.runtime_proof,
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
        self.assertEqual(payload["artifacts"]["readback_report"], str(self.readback_report))
        self.assertEqual(payload["artifacts"]["incident_artifact"], str(self.incident_artifact))

