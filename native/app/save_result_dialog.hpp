#pragma once

#include "ac6dm/contracts/emblem_workflow.hpp"

#include <QDialog>
#include <QString>
#include <QStringList>

namespace ac6dm::app {

class SaveResultDialog final : public QDialog {
    Q_OBJECT

public:
    explicit SaveResultDialog(
        const QString& windowTitle,
        const QString& summary,
        const QString& status,
        const QStringList& details,
        QWidget* parent = nullptr);
    explicit SaveResultDialog(const contracts::ImportResultDto& result, QWidget* parent = nullptr);
    explicit SaveResultDialog(const contracts::ActionResultDto& result, QWidget* parent = nullptr);
};

}  // namespace ac6dm::app
