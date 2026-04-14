from __future__ import annotations

import hashlib
import json
import tempfile
import unittest
from pathlib import Path

from ac6_data_manager.release import (
    derive_release_verdict,
    write_evidence_manifest,
    write_release_content_manifest,
)


class ReleaseEvidenceTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tempdir = tempfile.TemporaryDirectory()
        self.root = Path(self.tempdir.name)

    def tearDown(self) -> None:
        self.tempdir.cleanup()

    def test_write_release_content_manifest_records_relative_paths_and_hashes(self) -> None:
        app_root = self.root / "AC6 saving manager"
        (app_root / "runtime").mkdir(parents=True, exist_ok=True)
        (app_root / "docs").mkdir(parents=True, exist_ok=True)
        (app_root / "AC6 saving manager.exe").write_bytes(b"exe-binary")
        guide_path = app_root / "docs" / "QUICK_START.txt"
        guide_path.write_text("quick-start", encoding="utf-8")
        runtime_path = app_root / "runtime" / "engine.dll"
        runtime_path.write_bytes(b"runtime-binary")

        output_path = self.root / "artifacts" / "release" / "content-manifest.json"
        result = write_release_content_manifest(app_root, output_path)
        payload = json.loads(output_path.read_text(encoding="utf-8"))
        files = {entry["path"]: entry for entry in payload["files"]}

        self.assertEqual(result, output_path)
        self.assertEqual(payload["app_root"], str(app_root))
        self.assertEqual(
            sorted(files),
            [
                "AC6 saving manager.exe",
                "docs/QUICK_START.txt",
                "runtime/engine.dll",
            ],
        )
        self.assertEqual(
            files["runtime/engine.dll"]["sha256"],
            hashlib.sha256(b"runtime-binary").hexdigest(),
        )
        self.assertEqual(files["docs/QUICK_START.txt"]["size_bytes"], len("quick-start".encode("utf-8")))

    def test_write_evidence_manifest_serializes_present_and_missing_artifacts(self) -> None:
        release_root = self.root / "release" / "AC6 saving manager"
        release_root.mkdir(parents=True, exist_ok=True)
        release_content_manifest = self.root / "artifacts" / "release" / "content-manifest.json"
        release_content_manifest.parent.mkdir(parents=True, exist_ok=True)
        release_content_manifest.write_text("{}", encoding="utf-8")
        preflight_report = self.root / "artifacts" / "preflight" / "preflight-report.json"
        preflight_report.parent.mkdir(parents=True, exist_ok=True)
        preflight_report.write_text("{}", encoding="utf-8")
        runtime_proof = self.root / "artifacts" / "preflight" / "real-entry-proof.json"
        runtime_proof.write_text("{}", encoding="utf-8")
        publish_audit = self.root / "artifacts" / "live_acceptance" / "publish-audit.json"
        publish_audit.parent.mkdir(parents=True, exist_ok=True)
        publish_audit.write_text("{}", encoding="utf-8")
        rollback_report = self.root / "artifacts" / "live_acceptance" / "rollback.json"
        rollback_report.write_text("{}", encoding="utf-8")
        checklist_capture = self.root / "artifacts" / "live_acceptance" / "checklist-manual.md"
        checklist_capture.write_text("# checklist", encoding="utf-8")

        output_path = self.root / "artifacts" / "release" / "evidence-manifest.json"
        result = write_evidence_manifest(
            output_path,
            release_root=release_root,
            release_content_manifest=release_content_manifest,
            preflight_report=preflight_report,
            runtime_proof=runtime_proof,
            publish_audit=publish_audit,
            rollback_report=rollback_report,
            checklist_capture=checklist_capture,
            verdict="release-pending",
            notes=["Awaiting ZPX-GE77 live evidence."],
        )
        payload = json.loads(output_path.read_text(encoding="utf-8"))

        self.assertEqual(result, output_path)
        self.assertEqual(payload["verdict"], "release-pending")
        self.assertEqual(payload["release_root"], str(release_root))
        self.assertEqual(
            payload["artifacts"],
            {
                "release_content_manifest": str(release_content_manifest),
                "third_party_report": None,
                "preflight_report": str(preflight_report),
                "runtime_proof": str(runtime_proof),
                "smoke_report": None,
                "publish_audit": str(publish_audit),
                "rollback_report": str(rollback_report),
                "huorong_matrix": None,
                "checklist_capture": str(checklist_capture),
            },
        )
        self.assertEqual(payload["notes"], ["Awaiting ZPX-GE77 live evidence."])

    def test_release_verdict_fails_when_huorong_blocks_any_step(self) -> None:
        verdict = derive_release_verdict(
            {
                "zip": "pass",
                "first-launch": "blocked",
                "preflight": "pass",
            },
            preflight_ok=True,
            real_entry_ok=True,
            publish_ok=True,
            rollback_ok=True,
            game_verify_ok=True,
            evidence_complete=True,
        )

        self.assertEqual(verdict, "fail")

    def test_release_verdict_allows_conditional_pass_for_manual_allow_matrix(self) -> None:
        verdict = derive_release_verdict(
            {
                "zip": "manual-allow",
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
            game_verify_ok=True,
            evidence_complete=True,
        )

        self.assertEqual(verdict, "conditional-pass")

    def test_release_verdict_reaches_live_pass_only_when_all_gates_are_complete(self) -> None:
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
