from __future__ import annotations

import unittest
from pathlib import Path


class M2ReadinessReportTests(unittest.TestCase):
    def test_m2_readiness_report_exists_and_mentions_next_steps(self) -> None:
        report_path = Path("docs/appendices/ac6-data-manager.m2-readiness.md")
        self.assertTrue(report_path.exists())
        content = report_path.read_text(encoding="utf-8")
        self.assertIn("# AC6 Data Manager M2 Readiness", content)
        self.assertIn("record-map", content)
        self.assertIn("dependency", content)
        self.assertIn("preview-provenance", content)
