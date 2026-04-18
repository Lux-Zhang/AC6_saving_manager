#pragma once

#include "app_services.hpp"
#include "home_library_view.hpp"

#include <QMainWindow>
#include <QStringList>
#include <QString>
#include <filesystem>
#include <optional>

namespace ac6dm::app {

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(AppServices services, QWidget* parent = nullptr);

private slots:
    void handleOpenSave();
    void handlePrimaryAction();
    void handleImportFile();
    void handleExportSelected();
    void handleInlineDetailsRequested();
    void handleCreateBuildLink();

private:
    struct DetailDialogPayload final {
        QString title;
        QString summary;
        QString status;
        QStringList details;
    };

    void refreshFromSession();
    void showOpenSaveOutcome(const contracts::OpenSaveResultDto& result);
    void runImportToUserSlot(const contracts::CatalogItemDto& selected);
    void runExchangeImport(const std::filesystem::path& exchangeFilePath);
    void presentActionResult(const contracts::ActionResultDto& result, const QString& successSummary,
        const QString& failureSummary, const std::vector<contracts::DiagnosticEntry>* diagnostics = nullptr);
    void presentPlanBlocked(const QString& summary, const contracts::ImportPlanDto& plan);
    void presentDetailDialog(const DetailDialogPayload& payload);
    void cacheDetailDialogPayload(const DetailDialogPayload& payload);
    void clearDetailDialogPayload();
    DetailDialogPayload buildOpenSaveDetailPayload(const contracts::OpenSaveResultDto& result) const;
    DetailDialogPayload buildActionDetailPayload(const QString& title, const QString& summary,
        const contracts::ActionResultDto& result, const std::vector<contracts::DiagnosticEntry>* diagnostics) const;

    AppServices services_;
    HomeLibraryView* homeView_{nullptr};
    std::optional<DetailDialogPayload> lastDetailDialogPayload_{};
};

}  // namespace ac6dm::app
