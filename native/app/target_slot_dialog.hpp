#pragma once

#include "ac6dm/contracts/emblem_workflow.hpp"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>

namespace ac6dm::app {

class TargetSlotDialog final : public QDialog {
    Q_OBJECT

public:
    explicit TargetSlotDialog(
        const contracts::CatalogItemDto& item,
        const contracts::ImportPlanDto& plan,
        QWidget* parent = nullptr);

    int selectedTargetSlotIndex() const;
    QString selectedTargetSlotLabel() const;

private:
    bool allowAccept_{true};
    QComboBox* slotCombo_{nullptr};
    QDialogButtonBox* buttons_{nullptr};
};

}  // namespace ac6dm::app
