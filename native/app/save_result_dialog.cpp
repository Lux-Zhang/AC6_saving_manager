#include "save_result_dialog.hpp"

#include "ac6dm/contracts/error_contracts.hpp"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace ac6dm::app {
namespace {

QString normalizedStatusText(const QString& status) {
    const QString lowered = status.trimmed().toLower();
    if (lowered == QStringLiteral("ok")) {
        return QObject::tr("OK");
    }
    if (lowered == QStringLiteral("blocked")) {
        return QObject::tr("Blocked");
    }
    if (lowered.isEmpty()) {
        return QObject::tr("Unavailable");
    }
    return status;
}

QStringList buildImportDetails(const contracts::ImportResultDto& result) {
    QStringList lines;
    for (const auto& detail : result.action.details) {
        lines << QString::fromStdString(detail);
    }
    if (!result.action.evidencePaths.empty()) {
        lines << QString();
        lines << QObject::tr("Evidence");
        for (const auto& evidence : result.action.evidencePaths) {
            lines << QObject::tr("%1: %2")
                         .arg(QString::fromStdString(evidence.role),
                             QString::fromStdWString(evidence.path.wstring()));
        }
    }
    if (!result.diagnostics.empty()) {
        lines << QString();
        lines << QObject::tr("Diagnostics");
        for (const auto& diagnostic : result.diagnostics) {
            lines << QObject::tr("[%1] %2")
                         .arg(QString::fromStdString(diagnostic.code),
                             QString::fromStdString(diagnostic.message));
            for (const auto& detail : diagnostic.details) {
                lines << QObject::tr("  - %1").arg(QString::fromStdString(detail));
            }
        }
        lines << QString();
        lines << QObject::tr("Support actions");
        lines << QString::fromUtf8(ac6dm::contracts::errors::kCopyErrorDetailsLabel.data());
        lines << QString::fromUtf8(ac6dm::contracts::errors::kOpenSupportBundleDirectoryLabel.data());
    }
    return lines;
}

QStringList buildActionDetails(const contracts::ActionResultDto& result) {
    QStringList lines;
    for (const auto& detail : result.details) {
        lines << QString::fromStdString(detail);
    }
    if (!result.evidencePaths.empty()) {
        lines << QString();
        lines << QObject::tr("Evidence");
        for (const auto& evidence : result.evidencePaths) {
            lines << QObject::tr("%1: %2")
                         .arg(QString::fromStdString(evidence.role),
                             QString::fromStdWString(evidence.path.wstring()));
        }
    }
    return lines;
}

void populateDialog(
    QDialog* dialog,
    const QString& windowTitle,
    const QString& summary,
    const QString& status,
    const QStringList& details) {
    dialog->setWindowTitle(windowTitle);
    dialog->resize(620, 420);

    auto* layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto* title = new QLabel(summary.trimmed().isEmpty() ? QObject::tr("No summary available.") : summary, dialog);
    title->setWordWrap(true);
    title->setObjectName(QStringLiteral("saveResultSummaryLabel"));
    layout->addWidget(title);

    auto* detailView = new QPlainTextEdit(dialog);
    detailView->setObjectName(QStringLiteral("saveResultDetailsText"));
    detailView->setReadOnly(true);
    QStringList lines;
    lines << QObject::tr("Status: %1").arg(normalizedStatusText(status));
    lines.append(details);
    detailView->setPlainText(lines.join("\n"));
    layout->addWidget(detailView, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    buttons->button(QDialogButtonBox::Close)->setText(QObject::tr("Close"));
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    layout->addWidget(buttons);
}

}  // namespace

SaveResultDialog::SaveResultDialog(
    const QString& windowTitle,
    const QString& summary,
    const QString& status,
    const QStringList& details,
    QWidget* parent)
    : QDialog(parent) {
    populateDialog(this, windowTitle, summary, status, details);
}

SaveResultDialog::SaveResultDialog(const contracts::ImportResultDto& result, QWidget* parent)
    : SaveResultDialog(
          tr("Import details"),
          QString::fromStdString(result.action.summary),
          QString::fromStdString(result.action.status),
          buildImportDetails(result),
          parent) {}

SaveResultDialog::SaveResultDialog(const contracts::ActionResultDto& result, QWidget* parent)
    : SaveResultDialog(
          tr("Action details"),
          QString::fromStdString(result.summary),
          QString::fromStdString(result.status),
          buildActionDetails(result),
          parent) {}

}  // namespace ac6dm::app
