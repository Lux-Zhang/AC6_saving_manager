from __future__ import annotations

import base64
import os
import unittest

os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')

from PyQt5.QtCore import Qt

from ac6_data_manager.gui import (
    AuditEntryDto,
    AuditTableModel,
    CatalogItemDto,
    DetailFieldDto,
    EmblemCatalogTableModel,
    PreviewHandleDto,
)

SAMPLE_PNG = base64.b64decode(
    'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIHWP4////fwAJ+wP9KobjigAAAABJRU5ErkJggg=='
)


class GuiModelTests(unittest.TestCase):
    def test_emblem_catalog_model_exposes_columns_and_tooltips(self) -> None:
        item = CatalogItemDto(
            asset_id='emblem-001',
            display_name='Raven Mark',
            slot_id='USER_EMBLEM_01',
            origin='share',
            editable_state='editable',
            source_label='share/manual',
            preview=PreviewHandleDto(
                label='preview',
                provenance='fixture',
                image_bytes=SAMPLE_PNG,
                note='fixture preview',
            ),
            description='sample description',
            tags=('pve',),
            dependency_references=('dep-1',),
            detail_fields=(DetailFieldDto('author', 'fixture'),),
        )
        model = EmblemCatalogTableModel((item,))

        self.assertEqual(model.rowCount(), 1)
        self.assertEqual(model.columnCount(), 5)
        self.assertEqual(model.headerData(0, Qt.Horizontal), '名称')
        self.assertEqual(model.data(model.index(0, 0), Qt.DisplayRole), 'Raven Mark')
        self.assertEqual(model.data(model.index(0, 4), Qt.DisplayRole), 'share/manual')
        self.assertEqual(model.data(model.index(0, 0), Qt.ToolTipRole), 'sample description')
        self.assertEqual(model.item_at(0), item)

    def test_audit_model_exposes_restore_point_and_details_tooltip(self) -> None:
        entry = AuditEntryDto(
            timestamp='2026-04-14T06:00:00Z',
            action='import',
            target='emblem-001',
            result_status='dry_run_ready',
            summary='audit-summary',
            restore_point_id='rp-001',
            details=('line-1', 'line-2'),
        )
        model = AuditTableModel((entry,))

        self.assertEqual(model.rowCount(), 1)
        self.assertEqual(model.columnCount(), 6)
        self.assertEqual(model.data(model.index(0, 5), Qt.DisplayRole), 'rp-001')
        self.assertEqual(model.data(model.index(0, 0), Qt.ToolTipRole), 'line-1\nline-2')
