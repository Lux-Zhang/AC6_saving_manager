#include "native_save_workflow.hpp"

#include "ac6dm/contracts/error_contracts.hpp"
#include "ac6dm/contracts/slot_contracts.hpp"
#include "ac6dm/contracts/evidence_contracts.hpp"

#include <QCoreApplication>
#include <QDateTime>

#include <cstdio>
#include <fstream>
#include <utility>

namespace ac6dm::app {

using namespace ac6dm::contracts;

namespace {

constexpr int kEmblemEntriesPerUserBucket = 19;

bool isAcAssetId(const std::string& assetId) {
    return assetId.rfind("ac:", 0) == 0;
}

std::optional<std::filesystem::path> findUserData007(const std::filesystem::path& root) {
    if (!std::filesystem::exists(root)) {
        return std::nullopt;
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (entry.is_regular_file() && entry.path().filename() == "USER_DATA007") {
            return entry.path();
        }
    }
    return std::nullopt;
}

std::string makeWorkspaceSessionId() {
    return "native-gui-session-" + std::to_string(QCoreApplication::applicationPid()) + "-"
        + std::to_string(QDateTime::currentMSecsSinceEpoch());
}

std::filesystem::path preferredRuntimeRootFor(const std::filesystem::path& appRoot) {
    return appRoot / "runtime" / "workspaces";
}

SessionStatusDto buildSessionStatus(const NativeWorkflowState& state, const std::string& mode, const std::string& summary) {
    SessionStatusDto status;
    status.workflowAvailable = true;
    status.hasRealSave = state.container.has_value();
    status.currentMode = mode;
    status.summary = summary;
    status.sourceSavePath = state.sourceSave;
    status.currentSavePath = state.stagedSave;
    status.artifactsRoot = state.workspace.has_value() ? std::optional<std::filesystem::path>(state.workspace->root()) : std::nullopt;
    return status;
}

OpenSaveResultDto blockedOpenSave(const NativeWorkflowState& state, const std::string& code, const std::string& message, const std::vector<std::string>& details) {
    OpenSaveResultDto result;
    result.session = buildSessionStatus(state, "blocked", message);
    result.diagnostics.push_back(DiagnosticEntry{.code = code, .message = message, .details = details});
    return result;
}

ImportResultDto blockedImport(const NativeWorkflowState& state, ResultCode code, const std::string& message, const std::vector<std::string>& details) {
    ImportResultDto result;
    result.session = buildSessionStatus(state, "blocked", message);
    result.action.title = "Update opened save";
    result.action.code = code;
    result.action.status = "blocked";
    result.action.summary = message;
    result.action.details = details;
    result.diagnostics.push_back(DiagnosticEntry{.code = message, .message = message, .details = details});
    return result;
}

std::shared_ptr<NativeWorkflowState> makeDefaultState() {
    auto state = std::make_shared<NativeWorkflowState>();
    state->appRoot = std::filesystem::path(QCoreApplication::applicationDirPath().toStdWString());
    return state;
}

void appendAcCatalog(
    std::vector<CatalogItemDto>& catalog,
    const std::optional<ac::AcCatalogSnapshot>& acSnapshot) {
    if (!acSnapshot.has_value()) {
        return;
    }
    catalog.insert(catalog.end(), acSnapshot->catalog.begin(), acSnapshot->catalog.end());
}

std::string describeAcTargetSlot(const int encodedTargetSlot) {
    return ac::describeTargetSlotCode(encodedTargetSlot);
}

}  // namespace

NativeOpenSaveService::NativeOpenSaveService(std::shared_ptr<NativeWorkflowState> state)
    : state_(std::move(state)) {}

OpenSaveResultDto NativeOpenSaveService::openSourceSave(const std::filesystem::path& sourceSave) {
    state_->sourceSave = sourceSave;
    state_->container.reset();
    state_->snapshot.reset();
    state_->acSnapshot.reset();
    state_->runtimeRegulationSnapshot.reset();

    tool_adapter::WitchyBndProcessAdapter adapter(state_->appRoot);
    const auto manifestResult = adapter.verifyBundledManifest();
    if (!manifestResult.success) {
        return blockedOpenSave(
            *state_,
            std::string(errors::kManifestBlockedCode),
            std::string(errors::kOpenSaveFailedUserMessage),
            manifestResult.errors);
    }

    try {
        if (state_->workspace.has_value() && std::filesystem::exists(state_->workspace->root())) {
            std::filesystem::remove_all(state_->workspace->root());
        }
        state_->workspace = workspace::SessionWorkspace::create(
            makeWorkspaceSessionId(),
            preferredRuntimeRootFor(state_->appRoot));
        state_->stagedSave = state_->workspace->stageSourceSave(sourceSave);
    } catch (const std::exception& exception) {
        return blockedOpenSave(
            *state_,
            std::string(errors::kOpenSaveBlockedCode),
            std::string(errors::kOpenSaveFailedUserMessage),
            {exception.what()});
    }

    const auto unpackResult = adapter.unpack(*state_->stagedSave, state_->workspace->unpackDir());
    if (unpackResult.process.returnCode != 0) {
        return blockedOpenSave(
            *state_,
            std::string(errors::kSidecarInvocationFailedCode),
            std::string(errors::kOpenSaveFailedUserMessage),
            {
                "returnCode=" + std::to_string(unpackResult.process.returnCode),
                "stderr=" + unpackResult.process.stderrText,
                "stdout=" + unpackResult.process.stdoutText,
            });
    }

    state_->unpackedDir = unpackResult.outputPath;
    state_->userData007Path = findUserData007(*state_->unpackedDir);
    if (!state_->userData007Path.has_value()) {
        return blockedOpenSave(
            *state_,
            std::string(errors::kOpenSaveBlockedCode),
            std::string(errors::kOpenSaveFailedUserMessage),
            {"USER_DATA007 was not found after WitchyBND unpack"});
    }

    try {
        emblem::UserData007Codec codec;
        state_->container = codec.readEncrypted(*state_->userData007Path);
        state_->snapshot = emblem::buildCatalogSnapshot(*state_->container);
        state_->acSnapshot = ac::buildProvisionalCatalogSnapshot(*state_->unpackedDir);
        state_->runtimeRegulationSnapshot = ac::inspectRuntimeRegulationSnapshot(*state_->unpackedDir);
    } catch (const std::exception& exception) {
        return blockedOpenSave(
            *state_,
            std::string(errors::kOpenSaveBlockedCode),
            std::string(errors::kOpenSaveFailedUserMessage),
            {exception.what()});
    }

    OpenSaveResultDto result;
    result.session = buildSessionStatus(
        *state_,
        "real-save",
        "Real save opened. Emblems and AC records are available for browse/import/export, including AC record-level import/export.");
    result.catalog = state_->snapshot->catalog;
    if (state_->acSnapshot.has_value()) {
        result.catalog.insert(
            result.catalog.end(),
            state_->acSnapshot->catalog.begin(),
            state_->acSnapshot->catalog.end());
    }
    return result;
}

NativeImportPlanningService::NativeImportPlanningService(std::shared_ptr<NativeWorkflowState> state)
    : state_(std::move(state)) {}

ImportPlanDto NativeImportPlanningService::buildImportPlan(
    const std::vector<std::string>& assetIds,
    std::optional<int> targetUserSlotIndex) {
    if (!state_->snapshot.has_value()) {
        ImportPlanDto plan;
        plan.title = "Import plan";
        plan.mode = "save-as-new";
        plan.summary = "Open a real save first.";
        plan.blockers.push_back("A real save is not loaded.");
        return plan;
    }
    if (assetIds.empty()) {
        ImportPlanDto plan;
        plan.title = "Import plan";
        plan.mode = "save-as-new";
        plan.summary = "Select a Share asset first.";
        plan.blockers.push_back("No Share asset is selected.");
        return plan;
    }
    if (isAcAssetId(assetIds.front())) {
        if (!state_->acSnapshot.has_value()) {
            ImportPlanDto plan;
            plan.title = "Import selected AC to user slot";
            plan.mode = "blocked";
            plan.summary = "The AC catalog is not loaded, so the import plan cannot be built.";
            plan.blockers.push_back("ac catalog is not loaded");
            return plan;
        }
        return ac::buildShareAcImportPlan(*state_->acSnapshot, assetIds.front(), targetUserSlotIndex);
    }
    return emblem::buildSingleShareImportPlan(*state_->snapshot, assetIds.front(), targetUserSlotIndex);
}

NativeImportExecutionService::NativeImportExecutionService(std::shared_ptr<NativeWorkflowState> state)
    : state_(std::move(state)) {}

ImportResultDto NativeImportExecutionService::executeImport(const ImportRequestDto& request) {
    std::ignore = request.outputSavePath;
    std::ignore = request.overwriteAllowed;
    if (!state_->container.has_value() || !state_->snapshot.has_value() || !state_->workspace.has_value()
        || !state_->stagedSave.has_value() || !state_->unpackedDir.has_value() || !state_->userData007Path.has_value()
        || !state_->sourceSave.has_value()) {
        return blockedImport(
            *state_,
            ResultCode::InvalidInput,
            std::string(errors::kSaveFailedUserMessage),
            {"native workflow state is incomplete; open a real save first"});
    }
    if (request.sourceAssetIds.size() != 1U) {
        return blockedImport(
            *state_,
            ResultCode::ImportPlanBlocked,
            std::string(errors::kSaveFailedUserMessage),
            {"current MVP supports exactly one share asset per import"});
    }
    if (!request.targetUserSlotIndex.has_value()) {
        return blockedImport(
            *state_,
            ResultCode::ImportPlanBlocked,
            std::string(errors::kSaveFailedUserMessage),
            {"target user slot selection is required"});
    }

    const bool importingAc = isAcAssetId(request.sourceAssetIds.front());
    const auto confirmedPlan = importingAc
        ? ac::buildShareAcImportPlan(*state_->acSnapshot, request.sourceAssetIds.front(), request.targetUserSlotIndex)
        : emblem::buildSingleShareImportPlan(*state_->snapshot, request.sourceAssetIds.front(), request.targetUserSlotIndex);
    if (!confirmedPlan.blockers.empty()) {
        return blockedImport(
            *state_,
            ResultCode::ImportPlanBlocked,
            std::string(errors::kSaveFailedUserMessage),
            confirmedPlan.blockers);
    }

    int assignedSlot = -1;
    std::string assignedLabel;
    try {
        if (importingAc) {
            assignedSlot = *request.targetUserSlotIndex;
            assignedLabel = confirmedPlan.targetSlot;
            const auto expectedPayload = ac::applyShareAcPayloadCopy(
                *state_->unpackedDir,
                *state_->acSnapshot,
                request.sourceAssetIds.front(),
                assignedSlot);
            const auto readback = ac::verifyShareAcPayloadReadback(
                *state_->unpackedDir,
                assignedSlot,
                expectedPayload,
                request.sourceAssetIds.front());
            if (!readback.payloadMatches) {
                throw std::runtime_error("AC readback failed: target record payload mismatch");
            }
            state_->acSnapshot = ac::buildProvisionalCatalogSnapshot(*state_->unpackedDir);
        } else {
            emblem::UserDataContainer updated = emblem::applySingleShareImport(
                *state_->container,
                request.sourceAssetIds.front(),
                *request.targetUserSlotIndex,
                &assignedSlot);
            const int targetBucket = *request.targetUserSlotIndex;
            char buffer[32] = {0};
            std::snprintf(buffer, sizeof(buffer), "%02d", (assignedSlot % kEmblemEntriesPerUserBucket) + 1);
            assignedLabel = "User " + std::to_string(targetBucket + 1) + " / Emblem " + std::string(buffer);
            emblem::UserData007Codec codec;
            const auto encrypted = codec.writeEncrypted(updated);
            std::ofstream output(*state_->userData007Path, std::ios::binary | std::ios::trunc);
            output.write(reinterpret_cast<const char*>(encrypted.data()), static_cast<std::streamsize>(encrypted.size()));
            output.close();

            // Post-write readback in staging before publish.
            const auto readback = codec.readEncrypted(*state_->userData007Path);
            state_->snapshot = emblem::buildCatalogSnapshot(readback);
            state_->container = readback;
            state_->acSnapshot = ac::buildProvisionalCatalogSnapshot(*state_->unpackedDir);
            const auto& expectedPayload = updated.files.at(static_cast<std::size_t>(assignedSlot)).data;
            std::ignore = emblem::verifySingleShareImportReadback(
                *state_->snapshot,
                targetBucket,
                expectedPayload);
        }
    } catch (const std::exception& exception) {
        return blockedImport(
            *state_,
            ResultCode::ReadbackFailed,
            std::string(errors::kSaveFailedUserMessage),
            {exception.what()});
    }

    tool_adapter::WitchyBndProcessAdapter adapter(state_->appRoot);
    const auto repackResult = adapter.repack(
        *state_->unpackedDir,
        state_->workspace->repackDir() / state_->stagedSave->filename());
    if (repackResult.process.returnCode != 0) {
        return blockedImport(
            *state_,
            ResultCode::SidecarInvocationFailed,
            std::string(errors::kSaveFailedUserMessage),
            {
                "returnCode=" + std::to_string(repackResult.process.returnCode),
                "stderr=" + repackResult.process.stderrText,
                "stdout=" + repackResult.process.stdoutText,
            });
    }

    try {
        std::filesystem::copy_file(
            repackResult.outputPath,
            *state_->sourceSave,
            std::filesystem::copy_options::overwrite_existing);
    } catch (const std::exception& exception) {
        return blockedImport(
            *state_,
            ResultCode::UnsupportedEnvironment,
            std::string(errors::kSaveFailedUserMessage),
            {exception.what()});
    }

    ImportResultDto result;
    result.session = buildSessionStatus(*state_, "imported", "Import completed. The currently opened save has been updated.");
    result.session.applyOutputPath = *state_->sourceSave;
    result.catalog = state_->snapshot->catalog;
    appendAcCatalog(result.catalog, state_->acSnapshot);
    result.action.title = "Update opened save";
    result.action.code = ResultCode::Ok;
    result.action.status = "ok";
    result.action.summary = "Import completed. The currently opened save has been updated.";
    result.action.details = {
        "targetSlot=" + assignedLabel,
        "saveMode=in-place-update",
    };
    result.action.evidencePaths.push_back({
        .path = state_->workspace->root() / std::string(contracts::evidence::kN0Root),
        .role = "workspace-root",
    });
    return result;
}

}  // namespace ac6dm::app
