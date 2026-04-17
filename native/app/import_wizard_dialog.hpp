#pragma once

#include "ac6dm/contracts/emblem_workflow.hpp"

#include <QDialog>
#include <QLabel>
#include <QLineEdit>

#include <filesystem>
#include <optional>

namespace ac6dm::app {

class ImportWizardDialog final : public QDialog {
    Q_OBJECT

public:
    explicit ImportWizardDialog(
        const contracts::CatalogItemDto& item,
        const contracts::ImportPlanDto& plan,
        QWidget* parent = nullptr);

    std::optional<std::filesystem::path> outputPath() const;

private slots:
    void chooseOutputPath();

private:
    QLineEdit* outputPathEdit_{nullptr};
};

}  // namespace ac6dm::app
