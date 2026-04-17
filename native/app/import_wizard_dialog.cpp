#include "import_wizard_dialog.hpp"

#include "file_dialog_utils.hpp"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>

namespace ac6dm::app {

ImportWizardDialog::ImportWizardDialog(
    const contracts::CatalogItemDto& item,
    const contracts::ImportPlanDto& plan,
    QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("导入所选徽章"));
    resize(560, 420);

    auto* layout = new QVBoxLayout(this);

    auto* form = new QFormLayout();
    form->addRow(tr("分享徽章"), new QLabel(QString::fromStdString(item.displayName), this));
    form->addRow(tr("目标槽位"), new QLabel(QString::fromStdString(plan.targetSlot), this));
    form->addRow(tr("保存方式"), new QLabel(tr("保存为新存档"), this));
    layout->addLayout(form);

    auto* summary = new QPlainTextEdit(this);
    summary->setReadOnly(true);
    QStringList lines;
    lines << QString::fromStdString(plan.summary);
    if (!plan.warnings.empty()) {
        lines << tr("") << tr("注意事项：");
        for (const auto& warning : plan.warnings) {
            lines << QString::fromStdString("- " + warning);
        }
    }
    if (!plan.blockers.empty()) {
        lines << tr("") << tr("当前阻断：");
        for (const auto& blocker : plan.blockers) {
            lines << QString::fromStdString("- " + blocker);
        }
    }
    summary->setPlainText(lines.join("\n"));
    layout->addWidget(summary, 1);

    auto* outputRow = new QHBoxLayout();
    outputPathEdit_ = new QLineEdit(this);
    outputPathEdit_->setPlaceholderText(tr("选择“新存档”输出路径"));
    auto* browseButton = new QPushButton(tr("选择输出位置"), this);
    outputRow->addWidget(outputPathEdit_, 1);
    outputRow->addWidget(browseButton);
    layout->addLayout(outputRow);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("开始导入"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("取消"));
    buttons->button(QDialogButtonBox::Ok)->setEnabled(plan.blockers.empty());
    layout->addWidget(buttons);

    connect(browseButton, &QPushButton::clicked, this, &ImportWizardDialog::chooseOutputPath);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

std::optional<std::filesystem::path> ImportWizardDialog::outputPath() const {
    if (outputPathEdit_->text().trimmed().isEmpty()) {
        return std::nullopt;
    }
    return std::filesystem::path(outputPathEdit_->text().toStdWString());
}

void ImportWizardDialog::chooseOutputPath() {
    const QString path = chooseSaveFileWithHiddenItems(
        this,
        tr("选择新存档输出路径"),
        QString(),
        tr("AC6 存档 (*.sl2);;所有文件 (*)"));
    if (!path.isEmpty()) {
        outputPathEdit_->setText(path);
    }
}

}  // namespace ac6dm::app
