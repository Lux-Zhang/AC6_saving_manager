#pragma once

#include "ac6dm/contracts/asset_catalog.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ac6dm::contracts {

struct ImportOperationDto final {
    std::string title;
    std::string target;
    std::string result;
    std::string note;
};

struct ImportTargetChoiceDto final {
    int code{0};
    std::string label;
    std::string note;
};

struct ImportPlanDto final {
    std::string title;
    std::vector<std::string> sourceAssetIds;
    std::string targetSlot;
    std::optional<int> suggestedTargetUserSlotIndex;
    std::vector<ImportTargetChoiceDto> targetChoices;
    std::string mode;
    std::string summary;
    std::vector<ImportOperationDto> operations;
    std::vector<std::string> warnings;
    std::vector<std::string> blockers;
    bool requiresExplicitTargetUserSlotSelection{true};
    bool dryRunRequired{true};
    bool shadowWorkspaceRequired{true};
    bool postWriteReadbackRequired{true};
};

struct SessionStatusDto final {
    bool workflowAvailable{false};
    bool hasRealSave{false};
    std::string currentMode{"startup"};
    std::string summary;
    std::optional<std::filesystem::path> sourceSavePath;
    std::optional<std::filesystem::path> currentSavePath;
    std::optional<std::filesystem::path> applyOutputPath;
    std::optional<std::filesystem::path> artifactsRoot;
    std::optional<std::string> lastActionSummary;
};

struct ActionResultDto final {
    std::string title;
    ResultCode code{ResultCode::InternalError};
    std::string status;
    std::string summary;
    std::vector<std::string> details;
    std::vector<EvidencePath> evidencePaths;
};

struct OpenSaveResultDto final {
    SessionStatusDto session;
    std::vector<CatalogItemDto> catalog;
    std::vector<DiagnosticEntry> diagnostics;
};

struct ImportRequestDto final {
    std::vector<std::string> sourceAssetIds;
    std::optional<int> targetUserSlotIndex;
    std::filesystem::path outputSavePath;
    bool overwriteAllowed{false};
};

struct ImportResultDto final {
    ActionResultDto action;
    SessionStatusDto session;
    std::vector<CatalogItemDto> catalog;
    std::vector<DiagnosticEntry> diagnostics;
};

}  // namespace ac6dm::contracts
