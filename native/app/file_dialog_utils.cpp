#include "file_dialog_utils.hpp"

#include <QDir>
#include <QDialog>
#include <QFileDialog>
#include <QFileSystemModel>

namespace ac6dm::app {
namespace {

void configureDialog(QFileDialog& dialog) {
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);

    const auto visibleFilters = QDir::AllDirs
        | QDir::Files
        | QDir::Drives
        | QDir::Hidden
        | QDir::System
        | QDir::NoDotAndDotDot;
    if (auto* model = dialog.findChild<QFileSystemModel*>(); model != nullptr) {
        model->setFilter(visibleFilters);
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
