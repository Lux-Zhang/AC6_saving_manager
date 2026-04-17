#include "app_services.hpp"

#include "ac6dm/contracts/error_contracts.hpp"
#include "ac6dm/contracts/slot_contracts.hpp"
#include "native_exchange_workflow.hpp"
#include "native_save_workflow.hpp"

#include <QCoreApplication>

#include <filesystem>

namespace ac6dm::app {

using namespace ac6dm::contracts;

namespace {

std::filesystem::path resolveAppRoot() {
    const std::vector<std::filesystem::path> candidates = {
        std::filesystem::path(QCoreApplication::applicationDirPath().toStdWString()),
        std::filesystem::current_path(),
        std::filesystem::current_path() / "dist" / "AC6 saving manager",
    };
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate / "third_party" / "third_party_manifest.json")) {
            return candidate;
        }
    }
    return std::filesystem::current_path();
}

}  // namespace

OpenSaveResultDto UnwiredSaveOpenService::openSourceSave(const std::filesystem::path& sourceSave) {
    OpenSaveResultDto result;
    result.session.workflowAvailable = false;
    result.session.hasRealSave = false;
    result.session.currentMode = "not-wired";
    result.session.summary = "真实存档服务尚未接线；当前仅提供 native GUI 与契约骨架。";
    result.session.sourceSavePath = sourceSave;
    result.diagnostics.push_back(DiagnosticEntry{
        .code = std::string(errors::kOpenSaveBlockedCode),
        .message = std::string(errors::kOpenSaveFailedUserMessage),
        .details = {"save-open service is not wired yet"},
    });
    return result;
}

ImportPlanDto UnwiredImportPlanningService::buildImportPlan(
    const std::vector<std::string>& assetIds,
    std::optional<int> targetUserSlotIndex) {
    ImportPlanDto plan;
    plan.title = "导入计划";
    plan.sourceAssetIds = assetIds;
    plan.targetSlot = targetUserSlotIndex.has_value()
        ? formatUserSlotLabel(*targetUserSlotIndex)
        : "待用户选择";
    plan.suggestedTargetUserSlotIndex = targetUserSlotIndex;
    plan.mode = "save-as-new";
    plan.summary = "真实导入计划服务尚未接线；当前仅保留普通用户流程骨架。";
    plan.blockers.push_back("import-planning service is not wired yet");
    return plan;
}

ImportResultDto UnwiredImportExecutionService::executeImport(const ImportRequestDto& request) {
    ImportResultDto result;
    result.action.title = "保存为新存档";
    result.action.code = ResultCode::InternalError;
    result.action.status = "blocked";
    result.action.summary = std::string(errors::kSaveFailedUserMessage);
    result.action.details = {
        "import-execution service is not wired yet",
        "requested output: " + request.outputSavePath.generic_string(),
    };
    result.session.workflowAvailable = false;
    result.session.currentMode = "not-wired";
    result.session.summary = "真实导入执行尚未接线。";
    result.diagnostics.push_back(DiagnosticEntry{
        .code = std::string(errors::kReadbackFailedCode),
        .message = std::string(errors::kSaveFailedUserMessage),
        .details = {"import-execution service is not wired yet"},
    });
    return result;
}

std::vector<CatalogItemDto> SessionBackedCatalogQueryService::currentCatalog() const {
    return currentResult_.catalog;
}

SessionStatusDto SessionBackedCatalogQueryService::currentStatus() const {
    return currentResult_.session;
}

void SessionBackedCatalogQueryService::replace(OpenSaveResultDto result) {
    currentResult_ = std::move(result);
}

void SessionBackedCatalogQueryService::replace(ImportResultDto result) {
    currentResult_.session = result.session;
    currentResult_.catalog = std::move(result.catalog);
    currentResult_.diagnostics = std::move(result.diagnostics);
}

AppServices AppServices::createDefault() {
    AppServices services;
    auto workflowState = std::make_shared<NativeWorkflowState>();
    workflowState->appRoot = resolveAppRoot();
    services.openSaveService = std::make_shared<NativeOpenSaveService>(workflowState);
    services.importPlanningService = std::make_shared<NativeImportPlanningService>(workflowState);
    services.importExecutionService = std::make_shared<NativeImportExecutionService>(workflowState);
    services.exchangeService = std::make_shared<NativeExchangeService>(workflowState);
    services.catalogQueryService = std::make_shared<SessionBackedCatalogQueryService>();
    return services;
}

}  // namespace ac6dm::app
