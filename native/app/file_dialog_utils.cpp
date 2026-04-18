#include "file_dialog_utils.hpp"

#include <QDir>
#include <QDialog>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QGridLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QSplitter>
#include <QTreeView>

namespace ac6dm::app {
namespace {

void configureDialog(QFileDialog& dialog) {
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.resize(900, 620);
    dialog.setMinimumSize(820, 560);

    const auto visibleFilters = QDir::AllDirs
        | QDir::Files
        | QDir::Drives
        | QDir::Hidden
        | QDir::System
        | QDir::NoDotAndDotDot;
    if (auto* model = dialog.findChild<QFileSystemModel*>(); model != nullptr) {
        model->setFilter(visibleFilters);
    }

    if (auto* lookInCombo = dialog.findChild<QComboBox*>(QStringLiteral("lookInCombo"));
        lookInCombo != nullptr) {
        lookInCombo->setMinimumContentsLength(18);
        lookInCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        lookInCombo->setMaximumWidth(440);
    }

    if (auto* fileTypeCombo = dialog.findChild<QComboBox*>(QStringLiteral("fileTypeCombo"));
        fileTypeCombo != nullptr) {
        fileTypeCombo->setMinimumContentsLength(20);
        fileTypeCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    }

    if (auto* fileNameEdit = dialog.findChild<QLineEdit*>(QStringLiteral("fileNameEdit"));
        fileNameEdit != nullptr) {
        fileNameEdit->setMinimumWidth(280);
    }

    if (auto* splitter = dialog.findChild<QSplitter*>(); splitter != nullptr) {
        splitter->setStretchFactor(0, 0);
        splitter->setStretchFactor(1, 1);
        splitter->setSizes({180, 520});
    }

    if (auto* treeView = dialog.findChild<QTreeView*>(QStringLiteral("treeView")); treeView != nullptr) {
        treeView->setColumnWidth(0, 280);
        treeView->setColumnWidth(1, 80);
        treeView->setColumnWidth(2, 100);
        treeView->setColumnWidth(3, 120);
    }

    if (auto* layout = qobject_cast<QGridLayout*>(dialog.layout()); layout != nullptr) {
        layout->setHorizontalSpacing(8);
        layout->setVerticalSpacing(8);
    }
}

}  // namespace

QString chooseOpenFileWithHiddenItems(
    QWidget* parent,
    const QString& title,
    const QString& filter) {
    QFileDialog dialog(parent, title, QString(), filter);
    configureDialog(dialog);
    dialog.setFileMode(QFileDialog::ExistingFile);
    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) {
        return {};
    }
    return dialog.selectedFiles().front();
}

QString chooseSaveFileWithHiddenItems(
    QWidget* parent,
    const QString& title,
    const QString& suggestedName,
    const QString& filter) {
    QFileDialog dialog(parent, title, QString(), filter);
    configureDialog(dialog);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    if (!suggestedName.isEmpty()) {
        dialog.selectFile(suggestedName);
    }
    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) {
        return {};
    }
    return dialog.selectedFiles().front();
}

}  // namespace ac6dm::app
