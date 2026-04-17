#include "main_window.hpp"

#include "file_dialog_utils.hpp"
#include "save_result_dialog.hpp"
#include "target_slot_dialog.hpp"

#include <QMessageBox>

namespace ac6dm::app {
namespace {

QString preferredExportBaseName(const contracts::CatalogItemDto& item) {
    QString baseName;
    if (!item.archiveName.empty()) {
        baseName = QString::fromStdString(item.archiveName);
    } else if (!item.machineName.empty()) {
        baseName = QString::fromStdString(item.machineName);
    } else if (!item.shareCode.empty()) {
        baseName = QString::fromStdString(item.shareCode);
    } else {
        baseName = QString::fromStdString(item.displayName);
    }

    baseName.replace('\n', '_');
    baseName.replace('/', '_');
    baseName.replace('\\', '_');
    return baseName.trimmed();
}

bool isActionSuccess(const contracts::ActionResultDto& result) {
    return result.code == contracts::ResultCode::Ok || result.status == "ok";
}

QString fileNameText(const std::filesystem::path& path) {
    const auto fileName = path.filename().wstring();
    return fileName.empty() ? QString::fromStdWString(path.wstring()) : QString::fromStdWString(fileName);
}

QString assetKindLabel(const contracts::AssetKind assetKind) {
    return assetKind == contracts::AssetKind::Ac
        ? QObject::tr("AC")
        : QObject::tr("emblem");
}

QString normalizedStatusText(const std::string& status) {
    const QString lowered = QString::fromStdString(status).trimmed().toLower();
    if (lowered == QStringLiteral("ok")) {
        return QObject::tr("OK");
    }
    if (lowered == QStringLiteral("blocked")) {
        return QObject::tr("Blocked");
    }
    return lowered.isEmpty() ? QObject::tr("Unavailable") : QString::fromStdString(status);
}

QStringList buildPlanIssueLines(const std::vector<std::string>& issues) {
    QStringList lines;
    for (const auto& issue : issues) {
        lines << QObject::tr("- %1").arg(QString::fromStdString(issue));
    }
    return lines;
}

}  // namespace

MainWindow::MainWindow(AppServices services, QWidget* parent)
    : QMainWindow(parent), services_(std::move(services)) {
    setWindowTitle(tr("AC6 Saving Manager"));
    resize(1280, 800);

    homeView_ = new HomeLibraryView(this);
    setCentralWidget(homeView_);

    connect(homeView_, &HomeLibraryView::openSaveRequested, this, &MainWindow::handleOpenSave);
    connect(homeView_, &HomeLibraryView::primaryActionRequested, this, &MainWindow::handlePrimaryAction);
    connect(homeView_, &HomeLibraryView::importFileRequested, this, &MainWindow::handleImportFile);
    connect(homeView_, &HomeLibraryView::exportSelectedRequested, this, &MainWindow::handleExportSelected);
    connect(homeView_, &HomeLibraryView::inlineDetailsRequested, this, &MainWindow::handleInlineDetailsRequested);

    refreshFromSession();
}

void MainWindow::handleOpenSave() {
    const QString path = chooseOpenFileWithHiddenItems(
        this,
        tr("Open AC6 Save"),
        tr("AC6 Save (*.sl2);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }

    const auto result = services_.openSaveService->openSourceSave(std::filesystem::path(path.toStdWString()));
    services_.catalogQueryService->replace(result);
    refreshFromSession();
    showOpenSaveOutcome(result);
}

void MainWindow::handlePrimaryAction() {
    const auto selected = homeView_->selectedItem();
    if (!selected.has_value()) {
        QMessageBox::information(this, tr("No selection"), tr("Select an asset first."));
        return;
    }
    runImportToUserSlot(*selected);
}

void MainWindow::handleImportFile() {
    const bool isAc = homeView_->currentLibraryAssetKind() == contracts::AssetKind::Ac;

    const QString path = chooseOpenFileWithHiddenItems(
        this,
        isAc ? tr("Import AC File") : tr("Import Emblem File"),
        isAc ? tr("AC6 AC files (*.ac6acdata);;All files (*)")
             : tr("AC6 emblem files (*.ac6emblemdata);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }

    runExchangeImport(std::filesystem::path(path.toStdWString()));
}

void MainWindow::handleExportSelected() {
    const auto selected = homeView_->selectedItem();
    if (!selected.has_value()) {
        QMessageBox::information(this, tr("No selection"), tr("Select an asset first."));
        return;
    }

    const bool isEmblem = selected->assetKind == contracts::AssetKind::Emblem;
    const QString extension = isEmblem
        ? QStringLiteral(".ac6emblemdata")
        : QStringLiteral(".ac6acdata");
    const QString selectedName = preferredExportBaseName(*selected);
    const QString suggestedName = selectedName.isEmpty()
        ? QStringLiteral("asset") + extension
        : selectedName + extension;
    const QString filter = isEmblem
        ? tr("AC6 emblem files (*.ac6emblemdata);;All files (*)")
        : tr("AC6 AC files (*.ac6acdata);;All files (*)");
    const QString outputPath = chooseSaveFileWithHiddenItems(
        this,
        isEmblem ? tr("Export Selected Emblem") : tr("Export Selected AC"),
        suggestedName,
        filter);
    if (outputPath.isEmpty()) {
        return;
    }

    const auto result = services_.exchangeService->exportAsset(
        selected->assetId,
        std::filesystem::path(outputPath.toStdWString()));
    presentActionResult(
        result,
        tr("Exported selected %1 to %2.")
            .arg(assetKindLabel(selected->assetKind), fileNameText(std::filesystem::path(outputPath.toStdWString()))),
        tr("Failed to export selected %1.").arg(assetKindLabel(selected->assetKind)));
}

void MainWindow::handleInlineDetailsRequested() {
    if (!lastDetailDialogPayload_.has_value()) {
        return;
    }
    presentDetailDialog(*lastDetailDialogPayload_);
}

void MainWindow::runImportToUserSlot(const contracts::CatalogItemDto& selected) {
    const std::vector<std::string> assetIds{selected.assetId};
    const auto plan = services_.importPlanningService->buildImportPlan(assetIds);
    TargetSlotDialog dialog(selected, plan, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const auto confirmedPlan = services_.importPlanningService->buildImportPlan(
        assetIds,
        dialog.selectedTargetSlotIndex());
    if (!confirmedPlan.blockers.empty()) {
        presentPlanBlocked(
            tr("Cannot import into %1.").arg(dialog.selectedTargetSlotLabel()),
            confirmedPlan);
        return;
    }

    contracts::ImportRequestDto request;
    request.sourceAssetIds = assetIds;
    request.targetUserSlotIndex = dialog.selectedTargetSlotIndex();
    auto result = services_.importExecutionService->executeImport(request);
    services_.catalogQueryService->replace(result);
    refreshFromSession();

    presentActionResult(
        result.action,
        tr("Imported selected %1 into %2.").arg(assetKindLabel(selected.assetKind), dialog.selectedTargetSlotLabel()),
        tr("Failed to import selected %1 into %2.")
            .arg(assetKindLabel(selected.assetKind), dialog.selectedTargetSlotLabel()),
        &result.diagnostics);
}

void MainWindow::runExchangeImport(const std::filesystem::path& exchangeFilePath) {
    const auto inspectedItem = services_.exchangeService->inspectExchangeFile(exchangeFilePath);
    if (!inspectedItem.has_value()) {
        clearDetailDialogPayload();
        homeView_->setLastActionSummary(tr("Failed to read the selected exchange file."), true);
        homeView_->setInlineStatus(tr("Failed to read the selected exchange file."), true, false);
        return;
    }

    auto plan = services_.exchangeService->buildExchangeImportPlan(exchangeFilePath);
    TargetSlotDialog dialog(*inspectedItem, plan, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    plan = services_.exchangeService->buildExchangeImportPlan(exchangeFilePath, dialog.selectedTargetSlotIndex());
    if (!plan.blockers.empty()) {
        presentPlanBlocked(
            tr("Cannot import into %1.").arg(dialog.selectedTargetSlotLabel()),
            plan);
        return;
    }

    auto result = services_.exchangeService->importExchangeFile(
        exchangeFilePath,
        std::filesystem::path{},
        dialog.selectedTargetSlotIndex());
    services_.catalogQueryService->replace(result);
    refreshFromSession();

    presentActionResult(
        result.action,
        tr("Imported %1 file into %2.")
            .arg(assetKindLabel(inspectedItem->assetKind), dialog.selectedTargetSlotLabel()),
        tr("Failed to import %1 file into %2.")
            .arg(assetKindLabel(inspectedItem->assetKind), dialog.selectedTargetSlotLabel()),
        &result.diagnostics);
}

void MainWindow::refreshFromSession() {
    const auto session = services_.catalogQueryService->currentStatus();
    const auto catalog = services_.catalogQueryService->currentCatalog();

    std::vector<contracts::CatalogItemDto> emblemItems;
    std::vector<contracts::CatalogItemDto> acItems;
    emblemItems.reserve(catalog.size());
    acItems.reserve(catalog.size());

    for (const auto& item : catalog) {
        if (item.assetKind == contracts::AssetKind::Ac) {
            acItems.push_back(item);
        } else {
            emblemItems.push_back(item);
        }
    }

    homeView_->setSessionStatus(session);
    homeView_->setCatalog(emblemItems);
    homeView_->setAcCatalog(acItems);
}

void MainWindow::showOpenSaveOutcome(const contracts::OpenSaveResultDto& result) {
    const QString saveName = result.session.sourceSavePath.has_value()
        ? fileNameText(*result.session.sourceSavePath)
        : tr("save");

    if (result.session.hasRealSave && result.diagnostics.empty()) {
        const QString summary = tr("Opened save %1.").arg(saveName);
        clearDetailDialogPayload();
        homeView_->setLastActionSummary(summary, false);
        homeView_->setInlineStatus(summary, false, false);
        return;
    }

    const bool failed = !result.session.hasRealSave;
    const QString summary = failed
        ? tr("Failed to open save %1.").arg(saveName)
        : tr("Opened save %1 with diagnostics.").arg(saveName);
    const auto payload = buildOpenSaveDetailPayload(result);
    cacheDetailDialogPayload(payload);
    homeView_->setLastActionSummary(summary, failed);
    homeView_->setInlineStatus(summary, failed, true);
    presentDetailDialog(payload);
}

void MainWindow::presentActionResult(const contracts::ActionResultDto& result, const QString& successSummary,
    const QString& failureSummary, const std::vector<contracts::DiagnosticEntry>* diagnostics) {
    const bool success = isActionSuccess(result);
    const bool hasDiagnostics = diagnostics != nullptr && !diagnostics->empty();
    const QString summary = success ? successSummary : failureSummary;

    homeView_->setLastActionSummary(summary, !success);

    if (success && !hasDiagnostics) {
        clearDetailDialogPayload();
        homeView_->setInlineStatus(summary, false, false);
        return;
    }

    const auto payload = buildActionDetailPayload(
        success ? tr("Action details") : tr("Action failed"),
        summary,
        result,
        diagnostics);
    cacheDetailDialogPayload(payload);
    homeView_->setInlineStatus(summary, !success, true);
    presentDetailDialog(payload);
}

void MainWindow::presentPlanBlocked(const QString& summary, const contracts::ImportPlanDto& plan) {
    QStringList details;
    if (!plan.summary.empty()) {
        details << tr("Plan summary: %1").arg(QString::fromStdString(plan.summary));
    }
    if (!plan.warnings.empty()) {
        details << QString();
        details << tr("Warnings");
        details.append(buildPlanIssueLines(plan.warnings));
    }
    if (!plan.blockers.empty()) {
        details << QString();
        details << tr("Blocked");
        details.append(buildPlanIssueLines(plan.blockers));
    }

    const DetailDialogPayload payload{
        .title = tr("Import blocked"),
        .summary = summary,
        .status = tr("Blocked"),
        .details = details,
    };
    cacheDetailDialogPayload(payload);
    homeView_->setLastActionSummary(summary, true);
    homeView_->setInlineStatus(summary, true, true);
    presentDetailDialog(payload);
}

void MainWindow::presentDetailDialog(const DetailDialogPayload& payload) {
    SaveResultDialog dialog(payload.title, payload.summary, payload.status, payload.details, this);
    dialog.exec();
}

void MainWindow::cacheDetailDialogPayload(const DetailDialogPayload& payload) {
    lastDetailDialogPayload_ = payload;
}

void MainWindow::clearDetailDialogPayload() {
    lastDetailDialogPayload_.reset();
}

MainWindow::DetailDialogPayload MainWindow::buildOpenSaveDetailPayload(
    const contracts::OpenSaveResultDto& result) const {
    QStringList details;
    if (!result.session.summary.empty()) {
        details << tr("Session summary: %1").arg(QString::fromStdString(result.session.summary));
    }
    if (result.session.sourceSavePath.has_value()) {
        details << tr("Source save: %1").arg(QString::fromStdWString(result.session.sourceSavePath->wstring()));
    }
    if (!result.diagnostics.empty()) {
        details << QString();
        details << tr("Diagnostics");
        for (const auto& diagnostic : result.diagnostics) {
            details << tr("[%1] %2")
                           .arg(QString::fromStdString(diagnostic.code),
                               QString::fromStdString(diagnostic.message));
            for (const auto& detail : diagnostic.details) {
                details << tr("  - %1").arg(QString::fromStdString(detail));
            }
        }
    }

    return DetailDialogPayload{
        .title = result.session.hasRealSave ? tr("Open save details") : tr("Open save failed"),
        .summary = result.session.hasRealSave
            ? tr("The save opened with diagnostics.")
            : tr("The save could not be opened."),
        .status = result.session.hasRealSave ? tr("Warning") : tr("Error"),
        .details = details,
    };
}

MainWindow::DetailDialogPayload MainWindow::buildActionDetailPayload(const QString& title, const QString& summary,
    const contracts::ActionResultDto& result, const std::vector<contracts::DiagnosticEntry>* diagnostics) const {
    QStringList details;
    details << tr("Backend summary: %1").arg(QString::fromStdString(result.summary));
    for (const auto& detail : result.details) {
        details << QString::fromStdString(detail);
    }
    if (!result.evidencePaths.empty()) {
        details << QString();
        details << tr("Evidence");
        for (const auto& evidence : result.evidencePaths) {
            details << tr("%1: %2")
                           .arg(QString::fromStdString(evidence.role),
                               QString::fromStdWString(evidence.path.wstring()));
        }
    }
    if (diagnostics != nullptr && !diagnostics->empty()) {
        details << QString();
        details << tr("Diagnostics");
        for (const auto& diagnostic : *diagnostics) {
            details << tr("[%1] %2")
                           .arg(QString::fromStdString(diagnostic.code),
                               QString::fromStdString(diagnostic.message));
            for (const auto& detail : diagnostic.details) {
                details << tr("  - %1").arg(QString::fromStdString(detail));
            }
        }
    }

    return DetailDialogPayload{
        .title = title,
        .summary = summary,
        .status = normalizedStatusText(result.status),
        .details = details,
    };
}

}  // namespace ac6dm::app
