#pragma once

#include "ac6dm/contracts/emblem_workflow.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace ac6dm::contracts {

class ISaveOpenService {
public:
    virtual ~ISaveOpenService() = default;
    virtual OpenSaveResultDto openSourceSave(const std::filesystem::path& sourceSave) = 0;
};

class IImportPlanningService {
public:
    virtual ~IImportPlanningService() = default;
    virtual ImportPlanDto buildImportPlan(
        const std::vector<std::string>& assetIds,
        std::optional<int> targetUserSlotIndex = std::nullopt) = 0;
};

class IImportExecutionService {
public:
    virtual ~IImportExecutionService() = default;
    virtual ImportResultDto executeImport(const ImportRequestDto& request) = 0;
};

class IExchangeService {
public:
    virtual ~IExchangeService() = default;
    virtual std::optional<CatalogItemDto> inspectExchangeFile(const std::filesystem::path& exchangeFilePath) = 0;
    virtual ActionResultDto exportAsset(const std::string& assetId, const std::filesystem::path& outputPath) = 0;
    virtual ImportPlanDto buildExchangeImportPlan(
        const std::filesystem::path& exchangeFilePath,
        std::optional<int> targetUserSlotIndex = std::nullopt) = 0;
    virtual ImportResultDto importExchangeFile(
        const std::filesystem::path& exchangeFilePath,
        const std::filesystem::path& outputSavePath,
        std::optional<int> targetUserSlotIndex) = 0;
};

class ICatalogQueryService {
public:
    virtual ~ICatalogQueryService() = default;
    virtual std::vector<CatalogItemDto> currentCatalog() const = 0;
    virtual SessionStatusDto currentStatus() const = 0;
};

}  // namespace ac6dm::contracts
