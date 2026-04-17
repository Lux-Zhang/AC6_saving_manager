#include "target_slot_dialog.hpp"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>

namespace ac6dm::app {
namespace {

QString assetTypeLabel(const contracts::CatalogItemDto& item) {
    if (item.assetKind == contracts::AssetKind::Ac) {
        return item.origin == contracts::AssetOrigin::Package
            ? QObject::tr("AC file")
            : QObject::tr("AC");
    }
    return item.origin == contracts::AssetOrigin::Package
        ? QObject::tr("Emblem file")
        : QObject::tr("Emblem");
}

QString assetSummaryText(const contracts::CatalogItemDto& item) {
    if (item.assetKind == contracts::AssetKind::Emblem) {
        const QString shareCode = QString::fromStdString(item.shareCode).trimmed();
        return shareCode.isEmpty()
            ? QObject::tr("No in-game name / no share code")
            : shareCode;
    }

    const QString archiveName = QString::fromStdString(item.archiveName).trimmed();
    const QString machineName = QString::fromStdString(item.machineName).trimmed();
    const QString shareCode = QString::fromStdString(item.shareCode).trimmed();

    QStringList lines;
    lines << QObject::tr("Archive: %1").arg(archiveName.isEmpty() ? QObject::tr("-") : archiveName);
    lines << QObject::tr("AC: %1").arg(machineName.isEmpty() ? QObject::tr("-") : machineName);
    lines << QObject::tr("Share code: %1").arg(shareCode.isEmpty() ? QObject::tr("-") : shareCode);
    return lines.join("\n");
}

QString originLabel(const contracts::AssetOrigin origin) {
    switch (origin) {
    case contracts::AssetOrigin::User:
        return QObject::tr("User");
    case contracts::AssetOrigin::Share:
        return QObject::tr("Share");
    case contracts::AssetOrigin::Package:
        return QObject::tr("Exchange file");
    }
    return QObject::tr("Unknown");
}

QString importSummaryText(const contracts::CatalogItemDto& item) {
    return item.origin == contracts::AssetOrigin::Package
        ? QObject::tr("This will import the selected exchange file into the opened save as the newest local record.")
        : QObject::tr("This will copy the selected Share asset into the opened save as the newest local record.");
}

QString userSlotFallbackLabel(const int slotIndex) {
    return QObject::tr("User %1").arg(slotIndex + 1);
}

QString translatedPlanText(const std::string& rawText) {
    QString text = QString::fromStdString(rawText).trimmed();
    if (text.isEmpty()) {
        return text;
    }

    const struct Replacement final {
        const char* from;
        const char* to;
    } replacements[] = {
        {"待用户选择", "Choose a target user slot"},
        {"无效栏位", "Invalid slot"},
        {"下一写入栏位：", "Next write slot: "},
        {"下一写入栏位:", "Next write slot: "},
        {"推荐目标使用者栏位", "Recommended user slot"},
        {"目标使用者栏位", "Target user slot"},
        {"使用者", "User "},
        {"徽章", "Emblem "},
        {"分享", "Share"},
    };
    for (const auto& replacement : replacements) {
        text.replace(QString::fromUtf8(replacement.from), QString::fromUtf8(replacement.to));
    }
    return text;
}

QString issueBlockText(const QString& heading, const std::vector<std::string>& issues) {
    if (issues.empty()) {
        return {};
    }

    QStringList lines;
    lines << heading;
    for (const auto& issue : issues) {
        lines << QObject::tr("- %1").arg(translatedPlanText(issue));
    }
    return lines.join("\n");
}

}  // namespace

TargetSlotDialog::TargetSlotDialog(
    const contracts::CatalogItemDto& item,
    const contracts::ImportPlanDto& plan,
    QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Import to User Slot"));
    resize(560, 360);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto* introLabel = new QLabel(importSummaryText(item), this);
    introLabel->setWordWrap(true);
    introLabel->setObjectName(QStringLiteral("targetSlotIntroLabel"));
    layout->addWidget(introLabel);

    auto* summaryFrame = new QFrame(this);
    summaryFrame->setObjectName(QStringLiteral("targetSlotSummaryFrame"));
    auto* form = new QFormLayout(summaryFrame);
    form->setContentsMargins(12, 12, 12, 12);
    form->setSpacing(8);

    auto* assetValueLabel = new QLabel(assetSummaryText(item), summaryFrame);
    assetValueLabel->setObjectName(QStringLiteral("targetSlotAssetSummaryLabel"));
    assetValueLabel->setWordWrap(true);
    form->addRow(tr("Selected asset"), assetValueLabel);
    form->addRow(tr("Asset type"), new QLabel(assetTypeLabel(item), summaryFrame));
    form->addRow(tr("Origin"), new QLabel(originLabel(item.origin), summaryFrame));

    slotCombo_ = new QComboBox(summaryFrame);
    slotCombo_->setObjectName(QStringLiteral("targetSlotCombo"));
    if (!plan.targetChoices.empty()) {
        for (const auto& choice : plan.targetChoices) {
            const QString label = choice.code >= 0 && choice.code < 4
                ? userSlotFallbackLabel(choice.code)
                : translatedPlanText(choice.label);
            const QString note = translatedPlanText(choice.note).trimmed();
            slotCombo_->addItem(note.isEmpty() ? label : QObject::tr("%1 (%2)").arg(label, note), choice.code);
        }
    } else {
        for (int slotIndex = 0; slotIndex < 4; ++slotIndex) {
            slotCombo_->addItem(userSlotFallbackLabel(slotIndex), slotIndex);
        }
    }
    if (plan.suggestedTargetUserSlotIndex.has_value()) {
        const int comboIndex = slotCombo_->findData(*plan.suggestedTargetUserSlotIndex);
        if (comboIndex >= 0) {
            slotCombo_->setCurrentIndex(comboIndex);
        }
    }
    form->addRow(tr("Target user slot"), slotCombo_);
    layout->addWidget(summaryFrame);

    auto* warningLabel = new QLabel(this);
    warningLabel->setObjectName(QStringLiteral("targetSlotWarningLabel"));
    warningLabel->setWordWrap(true);
    const QString warningText = issueBlockText(tr("Warnings"), plan.warnings);
    warningLabel->setText(warningText);
    warningLabel->setVisible(!warningText.isEmpty());
    layout->addWidget(warningLabel);

    auto* blockerLabel = new QLabel(this);
    blockerLabel->setObjectName(QStringLiteral("targetSlotBlockerLabel"));
    blockerLabel->setWordWrap(true);
    const QString blockerText = issueBlockText(tr("Blocked"), plan.blockers);
    blockerLabel->setText(blockerText);
    blockerLabel->setVisible(!blockerText.isEmpty());
    layout->addWidget(blockerLabel);

    buttons_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons_->button(QDialogButtonBox::Ok)->setText(tr("Import"));
    buttons_->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    layout->addWidget(buttons_);

    connect(buttons_, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);

    allowAccept_ = plan.blockers.empty() && slotCombo_->count() > 0;
    auto* confirmButton = buttons_->button(QDialogButtonBox::Ok);
    confirmButton->setEnabled(allowAccept_);
    if (!allowAccept_) {
        confirmButton->setToolTip(tr("This import is blocked until the listed issues are resolved."));
    }
}

int TargetSlotDialog::selectedTargetSlotIndex() const {
    return slotCombo_->currentData().toInt();
}

QString TargetSlotDialog::selectedTargetSlotLabel() const {
    return slotCombo_->currentText();
}

}  // namespace ac6dm::app
