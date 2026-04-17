#include "native_exchange_workflow.hpp"

#include "ac6dm/contracts/error_contracts.hpp"
#include "ac6dm/contracts/evidence_contracts.hpp"
#include "ac6dm/contracts/exchange_contracts.hpp"
#include "ac6dm/contracts/slot_contracts.hpp"
#include "core/ac/ac_catalog.hpp"
#include "core/exchange/package_codec.hpp"
#include "core/tool_adapter/WitchyBndProcessAdapter.hpp"

#include <QByteArray>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <stdexcept>

namespace ac6dm::app {
namespace {

constexpr int kEmblemEntriesPerUserBucket = 19;

struct ParsedExchangeFile final {
    contracts::ExchangeAssetKind assetKind{contracts::ExchangeAssetKind::Emblem};
    contracts::SourceBucket sourceBucket{contracts::SourceBucket::Share};
    contracts::PreviewState previewState{contracts::PreviewState::Unknown};
    std::string title;
    std::string archiveName;
    std::string machineName;
    std::string shareCode;
    std::string recordRef;
    std::string previewProvenance;
    std::vector<std::uint8_t> previewBytes;
    std::vector<std::uint8_t> recordPayload;
    std::string checksumSha256;
    std::string producer;
    std::string payloadSchema;
    std::string editableClonePatchVersion;
    std::filesystem::path sourcePath;
};

contracts::SessionStatusDto buildSessionStatus(
    const NativeWorkflowState& state,
    const std::string& mode,
    const std::string& summary) {
    contracts::SessionStatusDto status;
    status.workflowAvailable = true;
    status.hasRealSave = state.container.has_value();
    status.currentMode = mode;
    status.summary = summary;
    status.sourceSavePath = state.sourceSave;
    status.currentSavePath = state.stagedSave;
    status.artifactsRoot = state.workspace.has_value()
        ? std::optional<std::filesystem::path>(state.workspace->root())
        : std::nullopt;
    return status;
}

contracts::ActionResultDto blockedAction(
    const std::string& title,
    const contracts::ResultCode code,
    const std::string& summary,
    const std::vector<std::string>& details) {
    contracts::ActionResultDto result;
    result.title = title;
    result.code = code;
    result.status = "blocked";
    result.summary = summary;
    result.details = details;
    return result;
}

contracts::ImportResultDto blockedImport(
    const NativeWorkflowState& state,
    const contracts::ResultCode code,
    const std::string& summary,
    const std::vector<std::string>& details) {
    contracts::ImportResultDto result;
    result.action = blockedAction("Import asset file", code, summary, details);
    result.session = buildSessionStatus(state, "blocked", summary);
    result.diagnostics.push_back({
        .code = summary,
        .message = summary,
        .details = details,
    });
    return result;
}

bool isUserSlotIndexValid(const int slotIndex) {
    return slotIndex >= 0 && slotIndex < 4;
}

int firstAvailableUserSlot(const std::vector<int>& slotsInUse) {
    for (int slotIndex = 0; slotIndex < 4; ++slotIndex) {
        if (std::find(slotsInUse.begin(), slotsInUse.end(), slotIndex) == slotsInUse.end()) {
            return slotIndex;
        }
    }
    return 0;
}

std::string displayUserSlot(const int slotIndex) {
    return "User " + std::to_string(slotIndex + 1);
}

std::string formatEmblemTargetLabel(const int slotIndex, const int fileIndex) {
    char buffer[32] = {0};
    std::snprintf(buffer, sizeof(buffer), "%02d", (fileIndex % kEmblemEntriesPerUserBucket) + 1);
    return displayUserSlot(slotIndex) + " / Emblem " + std::string(buffer);
}

bool hasContiguousPrefixThrough(const std::vector<int>& slotsInUse, const int targetSlotIndex) {
    for (int slotIndex = 0; slotIndex < targetSlotIndex; ++slotIndex) {
        if (std::find(slotsInUse.begin(), slotsInUse.end(), slotIndex) == slotsInUse.end()) {
            return false;
        }
    }
    return true;
}

int firstFileIndexForUserSlot(const int targetSlotIndex) {
    return targetSlotIndex * kEmblemEntriesPerUserBucket;
}

std::string expectedEditableClonePatchVersionFor(const contracts::ExchangeAssetKind assetKind) {
    return assetKind == contracts::ExchangeAssetKind::Emblem
        ? "emblem-category-patch-v1"
        : "ac-location-copy-v2";
}

std::string preferredPackageTitle(const contracts::CatalogItemDto& item) {
    if (!item.archiveName.empty()) {
        return item.archiveName;
    }
    if (!item.machineName.empty()) {
        return item.machineName;
    }
    if (!item.shareCode.empty()) {
        return item.shareCode;
    }
    return item.displayName;
}

ac6dm::exchange::ExchangePackage buildExchangePackage(
    const contracts::CatalogItemDto& item,
    contracts::ExchangeAssetKind assetKind,
    const std::vector<std::uint8_t>& recordPayload) {
    ac6dm::exchange::ExchangePackage package;
    package.formatVersion = std::string(contracts::kExchangeFormatVersionV1);
    package.assetKind = assetKind;
    package.title = preferredPackageTitle(item);
    package.archiveName = item.archiveName;
    package.machineName = item.machineName;
    package.shareCode = item.shareCode;
    package.sourceBucket = contracts::toString(item.sourceBucket);
    package.recordRef = item.recordRef;
    package.previewState = "unknown";
    package.previewProvenance = "preview-disabled.temporarily";
    package.recordPayload = recordPayload;
    package.checksumSha256 = ac6dm::exchange::computePayloadSha256(recordPayload);
    package.producer = "ac6dm-native-cpp/0.0.1";
    package.minReaderVersion = "0.0.1";
    package.payloadSchema = ac6dm::exchange::defaultPayloadSchemaFor(assetKind);
    package.editableClonePatchVersion = expectedEditableClonePatchVersionFor(assetKind);
    return package;
}

std::optional<contracts::SourceBucket> parseSourceBucket(const QString& value) {
    if (value == "user1") {
        return contracts::SourceBucket::User1;
    }
    if (value == "user2") {
        return contracts::SourceBucket::User2;
    }
    if (value == "user3") {
        return contracts::SourceBucket::User3;
    }
    if (value == "user4") {
        return contracts::SourceBucket::User4;
    }
    if (value == "share") {
        return contracts::SourceBucket::Share;
    }
    return std::nullopt;
}

std::optional<contracts::PreviewState> parsePreviewState(const QString& value) {
    if (value == "native_embedded") {
        return contracts::PreviewState::NativeEmbedded;
    }
    if (value == "derived_render") {
        return contracts::PreviewState::DerivedRender;
    }
    if (value == "unavailable") {
        return contracts::PreviewState::Unavailable;
    }
    if (value == "unknown") {
        return contracts::PreviewState::Unknown;
    }
    return std::nullopt;
}

std::optional<contracts::ExchangeAssetKind> parseExchangeAssetKind(const QString& value) {
    if (value == "emblem") {
        return contracts::ExchangeAssetKind::Emblem;
    }
    if (value == "ac") {
        return contracts::ExchangeAssetKind::Ac;
    }
    return std::nullopt;
}

ParsedExchangeFile parseExchangeFile(const std::filesystem::path& exchangeFilePath) {
    const auto package = ac6dm::exchange::readExchangePackage(exchangeFilePath);
    const auto assetKind = parseExchangeAssetKind(QString::fromStdString(
        package.assetKind == contracts::ExchangeAssetKind::Emblem ? "emblem" : "ac"));
    if (!assetKind.has_value()) {
        throw std::runtime_error("The asset exchange file is missing a valid asset_kind.");
    }

    const auto sourceBucket = parseSourceBucket(QString::fromStdString(package.sourceBucket));
    if (!sourceBucket.has_value()) {
        throw std::runtime_error("The asset exchange file is missing a valid source_bucket.");
    }

    const auto previewState = parsePreviewState(QString::fromStdString(package.previewState));
    if (!previewState.has_value()) {
        throw std::runtime_error("The asset exchange file is missing a valid preview_state.");
    }

    ParsedExchangeFile parsed;
    parsed.assetKind = *assetKind;
    parsed.sourceBucket = *sourceBucket;
    parsed.previewState = *previewState;
    parsed.title = package.title;
    parsed.archiveName = package.archiveName;
    parsed.machineName = package.machineName;
    parsed.shareCode = package.shareCode;
    parsed.recordRef = package.recordRef;
    parsed.previewProvenance = package.previewProvenance;
    if (package.previewBytes.has_value()) {
        parsed.previewBytes = *package.previewBytes;
    }
    parsed.recordPayload = package.recordPayload;
    parsed.checksumSha256 = package.checksumSha256;
    parsed.producer = package.producer;
    parsed.payloadSchema = package.payloadSchema;
    parsed.editableClonePatchVersion = package.editableClonePatchVersion;
    parsed.sourcePath = exchangeFilePath;
    return parsed;
}

contracts::CatalogItemDto buildCatalogItemFromParsedFile(const ParsedExchangeFile& parsed) {
    contracts::CatalogItemDto item;
    item.assetId = "package:" + parsed.sourcePath.filename().generic_string();
    item.assetKind = parsed.assetKind == contracts::ExchangeAssetKind::Emblem
        ? contracts::AssetKind::Emblem
        : contracts::AssetKind::Ac;
    item.sourceBucket = parsed.sourceBucket;
    item.slotIndex = contracts::slotIndexOf(parsed.sourceBucket);
    item.displayName = !parsed.title.empty()
        ? parsed.title
        : !parsed.archiveName.empty()
            ? parsed.archiveName
            : !parsed.machineName.empty()
                ? parsed.machineName
                : !parsed.shareCode.empty()
                    ? parsed.shareCode
                    : parsed.sourcePath.stem().generic_string();
    item.archiveName = parsed.archiveName;
    item.machineName = parsed.machineName;
    item.shareCode = parsed.shareCode;
    item.previewState = parsed.previewState;
    item.writeCapability = contracts::WriteCapability::ReadOnly;
    item.recordRef = parsed.recordRef;
    item.detailProvenance = parsed.payloadSchema.empty() ? "exchange-file-v1" : parsed.payloadSchema;
    item.slotLabel = item.slotIndex.has_value()
        ? contracts::formatUserSlotLabel(*item.slotIndex)
        : "SHARE";
    item.origin = contracts::AssetOrigin::Package;
    item.editable = false;
    item.sourceLabel = parsed.sourceBucket == contracts::SourceBucket::User1
        ? "User 1"
        : parsed.sourceBucket == contracts::SourceBucket::User2
            ? "User 2"
            : parsed.sourceBucket == contracts::SourceBucket::User3
                ? "User 3"
                : parsed.sourceBucket == contracts::SourceBucket::User4
                    ? "User 4"
                    : "Share";
    item.description = item.assetKind == contracts::AssetKind::Emblem
        ? "Emblem loaded from a .ac6emblemdata file."
        : "AC loaded from a .ac6acdata file.";
    item.tags = {item.assetKind == contracts::AssetKind::Emblem ? "emblem-exchange" : "ac-exchange"};
    item.preview.provenance = parsed.previewProvenance;
    item.detailFields.push_back({"Title", item.displayName});
    item.detailFields.push_back({"ArchiveName", item.archiveName});
    item.detailFields.push_back({"AcName", item.machineName});
    item.detailFields.push_back({"ShareCode", item.shareCode});
    item.detailFields.push_back({"AssetKind", contracts::toString(item.assetKind)});
    item.detailFields.push_back({"SourceBucket", contracts::toString(item.sourceBucket)});
    item.detailFields.push_back({"PreviewState", contracts::toString(item.previewState)});
    item.detailFields.push_back({"RecordRef", item.recordRef});
    item.detailFields.push_back({"Producer", parsed.producer});
    item.detailFields.push_back({"PayloadSchema", parsed.payloadSchema});
    item.detailFields.push_back({"EditableClonePatchVersion", parsed.editableClonePatchVersion});
    item.detailFields.push_back({"ChecksumSha256", parsed.checksumSha256});
    return item;
}

std::optional<contracts::CatalogItemDto> findCatalogItem(
    const NativeWorkflowState& state,
    const std::string& assetId) {
    if (state.snapshot.has_value()) {
        const auto it = std::find_if(state.snapshot->catalog.begin(), state.snapshot->catalog.end(), [&](const auto& item) {
            return item.assetId == assetId;
        });
        if (it != state.snapshot->catalog.end()) {
            return *it;
        }
    }
    if (state.acSnapshot.has_value()) {
        const auto it = std::find_if(state.acSnapshot->catalog.begin(), state.acSnapshot->catalog.end(), [&](const auto& item) {
            return item.assetId == assetId;
        });
        if (it != state.acSnapshot->catalog.end()) {
            return *it;
        }
    }
    return std::nullopt;
}

std::optional<emblem::EmblemSelection> findEmblemSelection(
    const NativeWorkflowState& state,
    const std::string& assetId) {
    if (!state.snapshot.has_value()) {
        return std::nullopt;
    }
    const auto it = std::find_if(state.snapshot->selections.begin(), state.snapshot->selections.end(), [&](const auto& selection) {
        return selection.assetId == assetId;
    });
    if (it == state.snapshot->selections.end()) {
        return std::nullopt;
    }
    return *it;
}

contracts::ImportPlanDto buildAcExchangeImportPlan(
    const NativeWorkflowState& state,
    const ParsedExchangeFile& parsed,
    const std::filesystem::path& exchangeFilePath,
    std::optional<int> targetUserSlotIndex) {
    contracts::ImportPlanDto plan = state.acSnapshot.has_value()
        ? ac::buildExchangeAcImportPlan(*state.acSnapshot, targetUserSlotIndex)
        : contracts::ImportPlanDto{};
    plan.title = "File import plan";
    plan.sourceAssetIds.push_back(exchangeFilePath.generic_string());
    if (!state.acSnapshot.has_value()) {
        plan.mode = "blocked";
        plan.summary = "The AC catalog is not loaded, so the .ac6acdata import plan cannot be built.";
        plan.blockers.push_back("ac catalog is not loaded");
        return plan;
    }
    if (parsed.editableClonePatchVersion != expectedEditableClonePatchVersionFor(contracts::ExchangeAssetKind::Ac)) {
        plan.mode = "blocked";
        plan.summary = "The AC exchange file is incompatible and has been fail-closed.";
        plan.blockers.push_back("editable clone patch version mismatch");
    }
    return plan;
}

}  // namespace

NativeExchangeService::NativeExchangeService(std::shared_ptr<NativeWorkflowState> state)
    : state_(std::move(state)) {}

std::optional<contracts::CatalogItemDto> NativeExchangeService::inspectExchangeFile(
    const std::filesystem::path& exchangeFilePath) {
    try {
        return ac6dm::exchange::inspectExchangePackage(exchangeFilePath);
    } catch (...) {
        return std::nullopt;
    }
}

contracts::ActionResultDto NativeExchangeService::exportAsset(
    const std::string& assetId,
    const std::filesystem::path& outputPath) {
    const auto catalogItem = findCatalogItem(*state_, assetId);
    if (!catalogItem.has_value()) {
        return blockedAction(
            "Export asset file",
            contracts::ResultCode::InvalidInput,
            "Export failed because the selected asset could not be found.",
            {"assetId=" + assetId});
    }
    if (std::filesystem::exists(outputPath)) {
        return blockedAction(
            catalogItem->assetKind == contracts::AssetKind::Ac ? "Export selected AC" : "Export selected emblem",
            contracts::ResultCode::OutputCollision,
            "Export failed because the target file already exists.",
            {"fresh-destination-only: output path already exists"});
    }

    std::vector<std::uint8_t> payload;
    contracts::ExchangeAssetKind assetKind = contracts::ExchangeAssetKind::Emblem;
    if (catalogItem->assetKind == contracts::AssetKind::Ac) {
        if (!state_->acSnapshot.has_value() || !state_->unpackedDir.has_value()) {
            return blockedAction(
                "Export selected AC",
                contracts::ResultCode::InvalidInput,
                "Export failed because the current session does not have an AC record-level catalog.",
                {"ac catalog is not loaded"});
        }
        assetKind = contracts::ExchangeAssetKind::Ac;
        payload = ac::readRecordPayload(*state_->unpackedDir, assetId, *state_->acSnapshot);
    } else {
        const auto selection = findEmblemSelection(*state_, assetId);
        if (!selection.has_value()) {
            return blockedAction(
                "Export selected emblem",
                contracts::ResultCode::InvalidInput,
                "Export failed because the current session cannot locate the selected emblem payload.",
                {"assetId=" + assetId});
        }
        payload = selection->rawPayload;
    }

    try {
        const auto package = buildExchangePackage(*catalogItem, assetKind, payload);
        ac6dm::exchange::writeExchangePackage(outputPath, package);
    } catch (const std::exception& exception) {
        return blockedAction(
            catalogItem->assetKind == contracts::AssetKind::Ac ? "Export selected AC" : "Export selected emblem",
            contracts::ResultCode::UnsupportedEnvironment,
            "Export failed because the target file could not be written.",
            {
                "output=" + outputPath.generic_string(),
                exception.what(),
            });
    }

    contracts::ActionResultDto result;
    result.title = catalogItem->assetKind == contracts::AssetKind::Ac ? "Export selected AC" : "Export selected emblem";
    result.code = contracts::ResultCode::Ok;
    result.status = "ok";
    result.summary = catalogItem->assetKind == contracts::AssetKind::Ac
        ? "Export completed. The .ac6acdata file has been created."
        : "Export completed. The .ac6emblemdata file has been created.";
    result.details = {
        "assetId=" + assetId,
        "output=" + outputPath.generic_string(),
    };
    result.evidencePaths.push_back({outputPath, "exchange-file"});
    return result;
}

contracts::ImportPlanDto NativeExchangeService::buildExchangeImportPlan(
    const std::filesystem::path& exchangeFilePath,
    std::optional<int> targetUserSlotIndex) {
    ParsedExchangeFile parsed;
    try {
        parsed = parseExchangeFile(exchangeFilePath);
    } catch (const std::exception& exception) {
        contracts::ImportPlanDto plan;
        plan.title = "File import plan";
        plan.mode = "save-as-new";
        plan.summary = "The asset exchange file could not be read.";
        plan.blockers.push_back(exception.what());
        plan.sourceAssetIds.push_back(exchangeFilePath.generic_string());
        return plan;
    }

    if (parsed.assetKind == contracts::ExchangeAssetKind::Ac) {
        return buildAcExchangeImportPlan(*state_, parsed, exchangeFilePath, targetUserSlotIndex);
    }

    contracts::ImportPlanDto plan;
    plan.title = "File import plan";
    plan.mode = "in-place-update";
    plan.summary = "Import the .ac6emblemdata file into the selected user slot and update the currently opened save in place.";
    plan.sourceAssetIds.push_back(exchangeFilePath.generic_string());
    if (!state_->snapshot.has_value()) {
        plan.blockers.push_back("Open a real save first.");
        return plan;
    }

    plan.targetChoices = emblem::buildTargetChoices(*state_->snapshot);

    if (!targetUserSlotIndex.has_value()) {
        const int suggestedTargetSlot = !plan.targetChoices.empty() ? plan.targetChoices.front().code : 0;
        plan.targetSlot = "Choose a target user slot";
        plan.suggestedTargetUserSlotIndex = suggestedTargetSlot;
        plan.warnings.push_back("Choose the destination user slot explicitly in the next step. The emblem will be appended to that user's next writable emblem slot.");
        plan.operations.push_back(
            {"Recommended user slot", displayUserSlot(suggestedTargetSlot), "suggested", ""});
        if (plan.targetChoices.empty()) {
            plan.blockers.push_back("There is no safe writable user emblem slot.");
            plan.summary = "File import plan is blocked.";
        }
        return plan;
    }

    const auto nextFileIndex = emblem::nextWritableFileIndexForSnapshot(*state_->snapshot, *targetUserSlotIndex);
    if (!nextFileIndex.has_value()) {
        plan.targetSlot = "Invalid slot";
        plan.blockers.push_back("The target user slot cannot be appended safely, or it is already full.");
        return plan;
    }

    const int targetSlot = *targetUserSlotIndex;
    plan.targetSlot = formatEmblemTargetLabel(targetSlot, *nextFileIndex);
    plan.suggestedTargetUserSlotIndex = targetSlot;
    plan.operations.push_back({"Selected emblem file", exchangeFilePath.filename().generic_string(), "selected", ""});
    plan.operations.push_back({"Target user slot", plan.targetSlot, "append",
        "The selected emblem file will be appended to the next writable slot in this user bucket."});
    plan.operations.push_back({"Save mode", "Opened save (in place)", "in-place-update", ""});
    plan.warnings.push_back("The write runs inside the staging workspace. Readback must pass before the opened save is replaced.");
    return plan;
}

contracts::ImportResultDto NativeExchangeService::importExchangeFile(
    const std::filesystem::path& exchangeFilePath,
    const std::filesystem::path& outputSavePath,
    std::optional<int> targetUserSlotIndex) {
    std::ignore = outputSavePath;
    if (!state_->container.has_value() || !state_->snapshot.has_value() || !state_->workspace.has_value()
        || !state_->stagedSave.has_value() || !state_->unpackedDir.has_value() || !state_->userData007Path.has_value()
        || !state_->sourceSave.has_value()) {
        return blockedImport(
            *state_,
            contracts::ResultCode::InvalidInput,
            std::string(contracts::errors::kSaveFailedUserMessage),
            {"native workflow state is incomplete; open a real save first"});
    }
    if (!targetUserSlotIndex.has_value()) {
        return blockedImport(
            *state_,
            contracts::ResultCode::ImportPlanBlocked,
            std::string(contracts::errors::kSaveFailedUserMessage),
            {"target user slot selection is required"});
    }

    ParsedExchangeFile parsed;
    try {
        parsed = parseExchangeFile(exchangeFilePath);
    } catch (const std::exception& exception) {
        return blockedImport(
            *state_,
            contracts::ResultCode::InvalidInput,
            std::string(contracts::errors::kSaveFailedUserMessage),
            {exception.what()});
    }
    if (parsed.assetKind == contracts::ExchangeAssetKind::Ac) {
        if (parsed.editableClonePatchVersion != expectedEditableClonePatchVersionFor(contracts::ExchangeAssetKind::Ac)) {
            return blockedImport(
                *state_,
                contracts::ResultCode::ImportPlanBlocked,
                "The AC exchange file is incompatible and has been fail-closed.",
                {"editable clone patch version mismatch"});
        }
    }

    const auto confirmedPlan = buildExchangeImportPlan(exchangeFilePath, targetUserSlotIndex);
    if (!confirmedPlan.blockers.empty()) {
        return blockedImport(
            *state_,
            contracts::ResultCode::ImportPlanBlocked,
            std::string(contracts::errors::kSaveFailedUserMessage),
            confirmedPlan.blockers);
    }

    int assignedSlot = -1;
    std::string assignedLabel;
    try {
        if (parsed.assetKind == contracts::ExchangeAssetKind::Ac) {
            assignedSlot = *targetUserSlotIndex;
            assignedLabel = confirmedPlan.targetSlot;
            std::vector<std::uint8_t> expectedPayload;
            ac::applyPayloadToUserSlot(*state_->unpackedDir, parsed.recordPayload, assignedSlot, &expectedPayload);
            const auto readback = ac::verifyShareAcPayloadReadback(
                *state_->unpackedDir,
                assignedSlot,
                expectedPayload,
                parsed.sourcePath.filename().generic_string());
            if (!readback.payloadMatches) {
                throw std::runtime_error("AC exchange import readback failed: target record payload mismatch");
            }
            state_->acSnapshot = ac::buildProvisionalCatalogSnapshot(*state_->unpackedDir);
        } else {
            auto tempContainer = *state_->container;
            tempContainer.extraFiles.push_back({
                .fileType = "EMBC",
                .data = parsed.recordPayload,
            });
            const auto tempAssetId = "extraFiles:" + std::to_string(static_cast<int>(tempContainer.extraFiles.size()) - 1);
            auto updated = emblem::applySingleShareImport(
                tempContainer,
                tempAssetId,
                *targetUserSlotIndex,
                &assignedSlot);
            assignedLabel = formatEmblemTargetLabel(*targetUserSlotIndex, assignedSlot);
            if (!updated.extraFiles.empty()) {
                updated.extraFiles.pop_back();
            }

            emblem::UserData007Codec codec;
            const auto encrypted = codec.writeEncrypted(updated);
            std::ofstream output(*state_->userData007Path, std::ios::binary | std::ios::trunc);
            output.write(reinterpret_cast<const char*>(encrypted.data()), static_cast<std::streamsize>(encrypted.size()));
            output.close();

            const auto readback = codec.readEncrypted(*state_->userData007Path);
            state_->snapshot = emblem::buildCatalogSnapshot(readback);
            state_->container = readback;
            state_->acSnapshot = ac::buildProvisionalCatalogSnapshot(*state_->unpackedDir);
            const auto expectedPayload = readback.files.at(static_cast<std::size_t>(assignedSlot)).data;
            std::ignore = emblem::verifySingleShareImportReadback(*state_->snapshot, *targetUserSlotIndex, expectedPayload);
        }
    } catch (const std::exception& exception) {
        return blockedImport(
            *state_,
            contracts::ResultCode::ReadbackFailed,
            std::string(contracts::errors::kSaveFailedUserMessage),
            {exception.what()});
    }

    tool_adapter::WitchyBndProcessAdapter adapter(state_->appRoot);
    const auto repackResult = adapter.repack(
        *state_->unpackedDir,
        state_->workspace->repackDir() / state_->stagedSave->filename());
    if (repackResult.process.returnCode != 0) {
        return blockedImport(
            *state_,
            contracts::ResultCode::SidecarInvocationFailed,
            std::string(contracts::errors::kSaveFailedUserMessage),
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
            contracts::ResultCode::UnsupportedEnvironment,
            std::string(contracts::errors::kSaveFailedUserMessage),
            {exception.what()});
    }

    contracts::ImportResultDto result;
    result.session = buildSessionStatus(*state_, "imported-exchange", "Asset file import completed. The currently opened save has been updated.");
    result.session.applyOutputPath = *state_->sourceSave;
    result.catalog = state_->snapshot->catalog;
    if (state_->acSnapshot.has_value()) {
        result.catalog.insert(
            result.catalog.end(),
            state_->acSnapshot->catalog.begin(),
            state_->acSnapshot->catalog.end());
    }
    result.action.title = "Import asset file";
    result.action.code = contracts::ResultCode::Ok;
    result.action.status = "ok";
    result.action.summary = "Asset file import completed. The currently opened save has been updated.";
    result.action.details = {
        "input=" + exchangeFilePath.generic_string(),
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
