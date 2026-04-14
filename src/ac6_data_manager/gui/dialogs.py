from __future__ import annotations

from PyQt5.QtCore import Qt
from PyQt5.QtWidgets import (
    QDialog,
    QDialogButtonBox,
    QFormLayout,
    QGroupBox,
    QLabel,
    QListWidget,
    QListWidgetItem,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)

from .dto import DeleteImpactPlanDto, ImportPlanDto


class _ReadOnlyList(QListWidget):
    def __init__(self, values: tuple[str, ...]) -> None:
        super().__init__()
        for value in values:
            QListWidgetItem(value, self)
        self.setSelectionMode(QListWidget.NoSelection)
        self.setFocusPolicy(Qt.NoFocus)


def _make_group(title: str, body: QWidget) -> QGroupBox:
    box = QGroupBox(title)
    layout = QVBoxLayout(box)
    layout.addWidget(body)
    return box


class ImportPlanDialog(QDialog):
    def __init__(self, plan: ImportPlanDto, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.plan = plan
        self.setWindowTitle(plan.title)
        self.resize(760, 560)

        layout = QVBoxLayout(self)

        form = QFormLayout()
        form.addRow('模式', QLabel(plan.mode))
        form.addRow('目标槽位', QLabel(plan.target_slot))
        form.addRow('源资产', QLabel(', '.join(plan.source_asset_ids) or '-'))
        form.addRow('Restore Point', QLabel('需要' if plan.restore_point_required else '可选'))
        form.addRow('写后回读', QLabel('需要' if plan.post_write_readback_required else '可选'))
        summary = QTextEdit(plan.summary)
        summary.setReadOnly(True)
        summary.setMinimumHeight(96)
        form.addRow('摘要', summary)
        details_host = QWidget()
        details_host.setLayout(form)
        layout.addWidget(_make_group('计划摘要', details_host))

        op_list = _ReadOnlyList(
            tuple(
                f'{operation.title} -> {operation.target} | {operation.result}'
                + (f' | {operation.note}' if operation.note else '')
                for operation in plan.operations
            )
            or ('暂无操作明细',)
        )
        layout.addWidget(_make_group('操作序列', op_list))

        warning_list = _ReadOnlyList(plan.warnings or ('无',))
        layout.addWidget(_make_group('警告', warning_list))

        blocker_list = _ReadOnlyList(plan.blockers or ('无',))
        layout.addWidget(_make_group('阻断条件', blocker_list))

        buttons = QDialogButtonBox(QDialogButtonBox.Close)
        buttons.rejected.connect(self.reject)
        buttons.accepted.connect(self.accept)
        buttons.button(QDialogButtonBox.Close).setText('关闭')
        layout.addWidget(buttons)


class DeleteImpactPlanDialog(QDialog):
    def __init__(self, plan: DeleteImpactPlanDto, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.plan = plan
        self.setWindowTitle(plan.title)
        self.resize(720, 520)

        layout = QVBoxLayout(self)

        form = QFormLayout()
        form.addRow('资产 ID', QLabel(plan.asset_id))
        form.addRow('风险级别', QLabel(plan.risk_level))
        form.addRow('允许执行', QLabel('否（只读原型）' if not plan.allowed else '是'))
        summary = QTextEdit(plan.summary)
        summary.setReadOnly(True)
        summary.setMinimumHeight(96)
        form.addRow('摘要', summary)
        summary_host = QWidget()
        summary_host.setLayout(form)
        layout.addWidget(_make_group('删除影响摘要', summary_host))

        impacts = _ReadOnlyList(plan.impacted_assets or ('未注入依赖影响项',))
        layout.addWidget(_make_group('潜在受影响资产', impacts))

        warnings = _ReadOnlyList(plan.warnings or ('无',))
        layout.addWidget(_make_group('风险提示', warnings))

        footer = QLabel('当前阶段仅提供 DeleteImpactPlan 只读查看，不执行真实删除。')
        footer.setWordWrap(True)
        layout.addWidget(footer)

        buttons = QDialogButtonBox(QDialogButtonBox.Close)
        buttons.rejected.connect(self.reject)
        buttons.accepted.connect(self.accept)
        buttons.button(QDialogButtonBox.Close).setText('关闭')
        layout.addWidget(buttons)
