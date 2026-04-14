from __future__ import annotations

import base64
import os
import unittest

os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')

from PyQt5.QtWidgets import QApplication

from ac6_data_manager.app import build_demo_provider
from ac6_data_manager.gui import Ac6DataManagerWindow, DeleteImpactPlanDialog, ImportPlanDialog


class GuiWindowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.app = QApplication.instance() or QApplication([])

    def test_main_window_initializes_and_populates_detail_panel(self) -> None:
        provider = build_demo_provider()
        window = Ac6DataManagerWindow(provider)
        window.emblem_page.table.selectRow(0)
        self.app.processEvents()

        self.assertEqual(window.navigation.count(), 2)
        self.assertEqual(window.emblem_page.catalog_model.rowCount(), 1)
        self.assertIn('Steel Haze Mark', window.emblem_page.detail_panel.title_label.text())
        self.assertEqual(window.emblem_page.detail_panel.editable_label.text(), 'editable')
        self.assertIn('direct extract', window.emblem_page.detail_panel.source_label.text())

    def test_import_plan_dialog_initializes_from_fixture(self) -> None:
        provider = build_demo_provider()
        plan = provider.get_import_plan(('emblem-user-001',))
        dialog = ImportPlanDialog(plan)

        self.assertEqual(dialog.windowTitle(), 'ImportPlan / Steel Haze Mark')
        self.assertTrue(dialog.plan.restore_point_required)
        self.assertEqual(dialog.plan.target_slot, 'USER_EMBLEM_07')

    def test_delete_impact_dialog_initializes_as_read_only(self) -> None:
        provider = build_demo_provider()
        plan = provider.get_delete_impact_plan('emblem-user-001')
        dialog = DeleteImpactPlanDialog(plan)

        self.assertEqual(dialog.windowTitle(), 'DeleteImpactPlan / Steel Haze Mark')
        self.assertFalse(dialog.plan.allowed)
        self.assertEqual(dialog.plan.risk_level, 'read_only')
