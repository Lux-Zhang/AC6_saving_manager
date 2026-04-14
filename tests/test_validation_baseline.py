from __future__ import annotations

import unittest

from ac6_data_manager.validation import (
    Capability,
    GateId,
    MilestoneId,
    build_default_baseline,
)


class ValidationBaselineTests(unittest.TestCase):
    def test_default_baseline_contains_expected_gates_and_milestones(self) -> None:
        baseline = build_default_baseline()

        self.assertEqual(set(baseline.gates), set(GateId))
        self.assertEqual(
            set(baseline.milestones),
            {MilestoneId.M0, MilestoneId.M05, MilestoneId.M1},
        )

        artifact_paths = baseline.required_artifact_paths()
        self.assertIn("artifacts/golden-corpus", artifact_paths)
        self.assertIn("artifacts/incidents", artifact_paths)
        self.assertIn("docs/reverse/ac-record-map.md", artifact_paths)

    def test_ac_capabilities_are_guarded_by_gate_state(self) -> None:
        baseline = build_default_baseline()

        dry_run = baseline.assess_capability(Capability.AC_DRY_RUN, {GateId.RECORD_MAP})
        self.assertFalse(dry_run.allowed)
        self.assertEqual(dry_run.blocking_gates, (GateId.DEPENDENCY_GRAPH,))

        experimental = baseline.assess_capability(
            Capability.AC_EXPERIMENTAL_IMPORT,
            {GateId.RECORD_MAP, GateId.DEPENDENCY_GRAPH, GateId.EDITABILITY_TRANSITION},
        )
        self.assertFalse(experimental.allowed)
        self.assertIn(GateId.PREVIEW_PROVENANCE, experimental.blocking_gates)
        self.assertIn(GateId.ROUND_TRIP_CORPUS, experimental.blocking_gates)

        unlocked = baseline.assess_capability(
            Capability.AC_EXPERIMENTAL_IMPORT,
            set(GateId),
        )
        self.assertTrue(unlocked.allowed)

    def test_m0_to_m1_required_checks_cover_validation_surface(self) -> None:
        baseline = build_default_baseline()
        checks = baseline.required_checks()
        check_ids = {check.check_id for check in checks}

        self.assertIn("M0-fault-injection-fail-closed", check_ids)
        self.assertIn("M05-integration-shadow-readback", check_ids)
        self.assertIn("M1-gui-audit-view", check_ids)
        self.assertEqual(len(checks), 12)
