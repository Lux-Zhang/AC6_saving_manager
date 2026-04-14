from __future__ import annotations

from PyQt5.QtCore import Qt
from PyQt5.QtGui import QColor, QFont, QPainter, QPixmap
from PyQt5.QtWidgets import (
    QAbstractItemView,
    QApplication,
    QFrame,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QListWidget,
    QListWidgetItem,
    QMainWindow,
    QPushButton,
    QSplitter,
    QStackedWidget,
    QTableView,
    QTextEdit,
    QToolBar,
    QVBoxLayout,
    QWidget,
)

from .dialogs import DeleteImpactPlanDialog, ImportPlanDialog
from .dto import CatalogItemDto
from .models import AuditTableModel, EmblemCatalogTableModel
from .services import GuiDataProvider

DARK_STYLESHEET = """
QWidget {
    background-color: #11161d;
    color: #d7dde8;
    font-size: 12px;
}
QMainWindow, QFrame, QToolBar, QTableView, QListWidget, QTextEdit, QLineEdit {
    background-color: #17202b;
}
QHeaderView::section {
    background-color: #1f2c3a;
    color: #cfe2ff;
    padding: 4px;
    border: 0;
}
QPushButton {
    background-color: #25405f;
    border: 1px solid #456789;
    padding: 6px 10px;
}
QPushButton:disabled {
    color: #728196;
    border-color: #334455;
}
QLabel#titleLabel {
    font-size: 18px;
    font-weight: 600;
    color: #f3f7ff;
}
QLabel#riskLabel {
    color: #ffd479;
    font-weight: 600;
}

"""

def make_placeholder_pixmap(width: int, height: int, label: str = 'No Preview') -> QPixmap:
    pixmap = QPixmap(width, height)
    pixmap.fill(QColor('#18222d'))
    painter = QPainter(pixmap)
    painter.setPen(QColor('#7aa2d6'))
    painter.drawRect(0, 0, width - 1, height - 1)
    painter.drawText(pixmap.rect(), Qt.AlignCenter, label)
    painter.end()
    return pixmap


class EmblemDetailPanel(QWidget):
    def __init__(self) -> None:
        super().__init__()
        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)

        self.title_label = QLabel('未选择徽章')
        self.title_label.setObjectName('titleLabel')
        self.meta_label = QLabel('选择左侧资产后显示详细信息。')
        self.meta_label.setWordWrap(True)
        self.preview_label = QLabel('无预览')
        self.preview_label.setAlignment(Qt.AlignCenter)
        self.preview_label.setFixedSize(260, 260)
        self.preview_label.setFrameShape(QFrame.Box)
        self.preview_label.setStyleSheet('border: 1px solid #39536f;')
        self.source_label = QLabel('-')
        self.source_label.setWordWrap(True)
        self.editable_label = QLabel('-')
        self.editable_label.setObjectName('riskLabel')
        self.tags_label = QLabel('-')
        self.tags_label.setWordWrap(True)
        self.dependencies_label = QLabel('-')
        self.dependencies_label.setWordWrap(True)
        self.description_box = QTextEdit()
        self.description_box.setReadOnly(True)
        self.description_box.setMinimumHeight(120)

        layout.addWidget(self.title_label)
        layout.addWidget(self.meta_label)
        layout.addWidget(self.preview_label, alignment=Qt.AlignHCenter)
        layout.addWidget(QLabel('来源'))
        layout.addWidget(self.source_label)
        layout.addWidget(QLabel('可编辑状态'))
        layout.addWidget(self.editable_label)
        layout.addWidget(QLabel('标签'))
        layout.addWidget(self.tags_label)
        layout.addWidget(QLabel('依赖'))
        layout.addWidget(self.dependencies_label)
        layout.addWidget(QLabel('说明'))
        layout.addWidget(self.description_box)
        layout.addStretch(1)

    def set_item(self, item: CatalogItemDto | None) -> None:
        if item is None:
            self.title_label.setText('未选择徽章')
            self.meta_label.setText('选择左侧资产后显示详细信息。')
            self.source_label.setText('-')
            self.editable_label.setText('-')
            self.tags_label.setText('-')
            self.dependencies_label.setText('-')
            self.description_box.setPlainText('')
            self.preview_label.setPixmap(make_placeholder_pixmap(self.preview_label.width(), self.preview_label.height(), '无预览'))
            self.preview_label.setText('')
            return

        self.title_label.setText(f'{item.display_name}  [{item.slot_id}]')
        provenance_bits = [item.origin, item.preview.provenance]
        if item.preview.source_hint:
            provenance_bits.append(item.preview.source_hint)
        self.meta_label.setText(f'资产 ID: {item.asset_id}')
        self.source_label.setText(' / '.join(bit for bit in provenance_bits if bit))
        self.editable_label.setText(item.editable_state)
        self.tags_label.setText(', '.join(item.tags) or '-')
        self.dependencies_label.setText(', '.join(item.dependency_references) or '无依赖')
        details = [item.description] if item.description else []
        details.extend(f'{field.label}: {field.value}' for field in item.detail_fields)
        self.description_box.setPlainText('\n'.join(details) or '无附加说明')
        pixmap = QPixmap()
        preview_bytes = item.preview.image_bytes
        if preview_bytes and pixmap.loadFromData(preview_bytes):
            scaled = pixmap.scaled(
                self.preview_label.size(),
                Qt.KeepAspectRatio,
                Qt.SmoothTransformation,
            )
            self.preview_label.setPixmap(scaled)
            self.preview_label.setText('')
        else:
            self.preview_label.setPixmap(make_placeholder_pixmap(self.preview_label.width(), self.preview_label.height(), item.preview.label))
            self.preview_label.setText('')


class AuditView(QWidget):
    def __init__(self, model: AuditTableModel) -> None:
        super().__init__()
        layout = QVBoxLayout(self)
        title = QLabel('审计视图')
        title.setObjectName('titleLabel')
        subtitle = QLabel('展示 restore point、结果状态与摘要；底层 mutation 结果通过服务层注入。')
        subtitle.setWordWrap(True)
        self.table = QTableView()
        self.table.setModel(model)
        self.table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.table.setSelectionMode(QAbstractItemView.SingleSelection)
        self.table.horizontalHeader().setStretchLastSection(True)
        self.table.verticalHeader().setVisible(False)
        layout.addWidget(title)
        layout.addWidget(subtitle)
        layout.addWidget(self.table)


class EmblemLibraryPage(QWidget):
    def __init__(self, provider: GuiDataProvider) -> None:
        super().__init__()
        self.provider = provider
        self.catalog_model = EmblemCatalogTableModel()
        self.catalog_items = list(provider.get_catalog())
        self.filtered_items = list(self.catalog_items)

        outer = QVBoxLayout(self)
        title = QLabel('徽章库 / Emblem Library')
        title.setObjectName('titleLabel')
        subtitle = QLabel('M1 GUI 原型：浏览、详情、预览、来源、可编辑状态，以及导入/删除影响计划入口。')
        subtitle.setWordWrap(True)
        outer.addWidget(title)
        outer.addWidget(subtitle)

        self.search_box = QLineEdit()
        self.search_box.setPlaceholderText('按名称、槽位、来源或标签筛选')
        self.search_box.textChanged.connect(self.apply_filter)
        outer.addWidget(self.search_box)

        split = QSplitter(Qt.Horizontal)
        outer.addWidget(split, stretch=1)

        left = QWidget()
        left_layout = QVBoxLayout(left)
        self.table = QTableView()
        self.table.setModel(self.catalog_model)
        self.table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.table.setSelectionMode(QAbstractItemView.SingleSelection)
        self.table.horizontalHeader().setStretchLastSection(True)
        self.table.verticalHeader().setVisible(False)
        self.table.selectionModel().selectionChanged.connect(self._sync_selection)
        left_layout.addWidget(self.table)

        button_row = QHBoxLayout()
        self.import_plan_button = QPushButton('查看 ImportPlan')
        self.import_plan_button.clicked.connect(self.show_import_plan)
        self.import_plan_button.setEnabled(False)
        self.delete_plan_button = QPushButton('查看 DeleteImpactPlan')
        self.delete_plan_button.clicked.connect(self.show_delete_impact_plan)
        self.delete_plan_button.setEnabled(False)
        self.refresh_button = QPushButton('刷新')
        self.refresh_button.clicked.connect(self.reload_from_provider)
        button_row.addWidget(self.import_plan_button)
        button_row.addWidget(self.delete_plan_button)
        button_row.addStretch(1)
        button_row.addWidget(self.refresh_button)
        left_layout.addLayout(button_row)

        self.detail_panel = EmblemDetailPanel()
        split.addWidget(left)
        split.addWidget(self.detail_panel)
        split.setStretchFactor(0, 3)
        split.setStretchFactor(1, 2)

        self.reload_from_provider()

    def reload_from_provider(self) -> None:
        self.catalog_items = list(self.provider.get_catalog())
        self.apply_filter(self.search_box.text())
        if self.catalog_model.rowCount() > 0:
            self.table.selectRow(0)
        else:
            self.detail_panel.set_item(None)
            self.import_plan_button.setEnabled(False)
            self.delete_plan_button.setEnabled(False)

    def apply_filter(self, text: str) -> None:
        query = text.strip().lower()
        if not query:
            self.filtered_items = list(self.catalog_items)
        else:
            self.filtered_items = [
                item
                for item in self.catalog_items
                if query in item.display_name.lower()
                or query in item.slot_id.lower()
                or query in item.origin.lower()
                or query in item.source_label.lower()
                or any(query in tag.lower() for tag in item.tags)
            ]
        self.catalog_model.set_items(self.filtered_items)
        self.table.resizeColumnsToContents()

    def current_item(self) -> CatalogItemDto | None:
        index = self.table.currentIndex()
        if not index.isValid():
            return None
        return self.catalog_model.item_at(index.row())

    def _sync_selection(self) -> None:
        item = self.current_item()
        self.detail_panel.set_item(item)
        enabled = item is not None
        self.import_plan_button.setEnabled(enabled)
        self.delete_plan_button.setEnabled(enabled)

    def show_import_plan(self) -> None:
        item = self.current_item()
        if item is None:
            return
        plan = self.provider.get_import_plan((item.asset_id,))
        dialog = ImportPlanDialog(plan, self)
        dialog.exec_()

    def show_delete_impact_plan(self) -> None:
        item = self.current_item()
        if item is None:
            return
        plan = self.provider.get_delete_impact_plan(item.asset_id)
        dialog = DeleteImpactPlanDialog(plan, self)
        dialog.exec_()


class Ac6DataManagerWindow(QMainWindow):
    def __init__(self, provider: GuiDataProvider) -> None:
        super().__init__()
        self.provider = provider
        self.setWindowTitle('AC6 Data Manager Prototype')
        self.resize(1380, 860)
        self.setStyleSheet(DARK_STYLESHEET)

        self.audit_model = AuditTableModel(tuple(provider.get_audit_entries()))
        self.emblem_page = EmblemLibraryPage(provider)
        self.audit_page = AuditView(self.audit_model)

        self.navigation = QListWidget()
        self.navigation.setMaximumWidth(180)
        self.navigation.addItem(QListWidgetItem('徽章库'))
        self.navigation.addItem(QListWidgetItem('审计日志'))
        self.navigation.currentRowChanged.connect(self._switch_page)

        self.stack = QStackedWidget()
        self.stack.addWidget(self.emblem_page)
        self.stack.addWidget(self.audit_page)

        central = QWidget()
        layout = QHBoxLayout(central)
        layout.addWidget(self.navigation)
        layout.addWidget(self.stack, stretch=1)
        self.setCentralWidget(central)

        toolbar = QToolBar('状态')
        toolbar.setMovable(False)
        status_label = QLabel('模式：Emblem stable lane / AC read-only')
        status_label.setFont(QFont('', 10))
        toolbar.addWidget(status_label)
        toolbar.addSeparator()
        self.audit_refresh_button = QPushButton('刷新审计')
        self.audit_refresh_button.clicked.connect(self.reload_audit_entries)
        toolbar.addWidget(self.audit_refresh_button)
        toolbar.addSeparator()
        self.notice_label = QLabel('所有 mutation 仍需经 dry-run、shadow workspace、post-write readback。')
        toolbar.addWidget(self.notice_label)
        self.addToolBar(toolbar)

        self.navigation.setCurrentRow(0)

    def _switch_page(self, row: int) -> None:
        if row < 0:
            row = 0
        self.stack.setCurrentIndex(row)

    def reload_audit_entries(self) -> None:
        self.audit_model.set_entries(tuple(self.provider.get_audit_entries()))
        self.audit_page.table.resizeColumnsToContents()


def create_application() -> QApplication:
    app = QApplication.instance()
    if app is None:
        app = QApplication([])
    app.setApplicationName('AC6 Data Manager Prototype')
    return app
