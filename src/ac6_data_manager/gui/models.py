from __future__ import annotations

from typing import Any, Sequence

from PyQt5.QtCore import QAbstractTableModel, QModelIndex, Qt

from .dto import AuditEntryDto, CatalogItemDto


class EmblemCatalogTableModel(QAbstractTableModel):
    HEADERS = ('名称', '槽位', '来源', '可编辑状态', '分享来源')

    def __init__(self, items: Sequence[CatalogItemDto] | None = None) -> None:
        super().__init__()
        self._items = list(items or ())

    @property
    def items(self) -> list[CatalogItemDto]:
        return list(self._items)

    def set_items(self, items: Sequence[CatalogItemDto]) -> None:
        self.beginResetModel()
        self._items = list(items)
        self.endResetModel()

    def rowCount(self, parent: QModelIndex = QModelIndex()) -> int:
        if parent.isValid():
            return 0
        return len(self._items)

    def columnCount(self, parent: QModelIndex = QModelIndex()) -> int:
        if parent.isValid():
            return 0
        return len(self.HEADERS)

    def headerData(
        self,
        section: int,
        orientation: Qt.Orientation,
        role: int = Qt.DisplayRole,
    ) -> Any:
        if role != Qt.DisplayRole:
            return None
        if orientation == Qt.Horizontal and 0 <= section < len(self.HEADERS):
            return self.HEADERS[section]
        return super().headerData(section, orientation, role)

    def data(self, index: QModelIndex, role: int = Qt.DisplayRole) -> Any:
        if not index.isValid():
            return None
        item = self._items[index.row()]
        if role == Qt.DisplayRole:
            if index.column() == 0:
                return item.display_name
            if index.column() == 1:
                return item.slot_id
            if index.column() == 2:
                return item.origin
            if index.column() == 3:
                return item.editable_state
            if index.column() == 4:
                return item.source_label
        if role == Qt.ToolTipRole:
            return item.description or item.preview.note or item.preview.provenance
        return None

    def item_at(self, row: int) -> CatalogItemDto | None:
        if 0 <= row < len(self._items):
            return self._items[row]
        return None


class AuditTableModel(QAbstractTableModel):
    HEADERS = ('时间', '动作', '目标', '结果', '摘要', '恢复点')

    def __init__(self, entries: Sequence[AuditEntryDto] | None = None) -> None:
        super().__init__()
        self._entries = list(entries or ())

    def set_entries(self, entries: Sequence[AuditEntryDto]) -> None:
        self.beginResetModel()
        self._entries = list(entries)
        self.endResetModel()

    def rowCount(self, parent: QModelIndex = QModelIndex()) -> int:
        if parent.isValid():
            return 0
        return len(self._entries)

    def columnCount(self, parent: QModelIndex = QModelIndex()) -> int:
        if parent.isValid():
            return 0
        return len(self.HEADERS)

    def headerData(
        self,
        section: int,
        orientation: Qt.Orientation,
        role: int = Qt.DisplayRole,
    ) -> Any:
        if role != Qt.DisplayRole:
            return None
        if orientation == Qt.Horizontal and 0 <= section < len(self.HEADERS):
            return self.HEADERS[section]
        return super().headerData(section, orientation, role)

    def data(self, index: QModelIndex, role: int = Qt.DisplayRole) -> Any:
        if not index.isValid():
            return None
        entry = self._entries[index.row()]
        if role == Qt.DisplayRole:
            columns = (
                entry.timestamp,
                entry.action,
                entry.target,
                entry.result_status,
                entry.summary,
                entry.restore_point_id or '-',
            )
            return columns[index.column()]
        if role == Qt.ToolTipRole and entry.details:
            return '\n'.join(entry.details)
        return None
