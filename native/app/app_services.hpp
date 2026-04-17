#pragma once

#include "ac6dm/contracts/error_contracts.hpp"
#include "ac6dm/contracts/service_interfaces.hpp"

#include <memory>

namespace ac6dm::app {

class UnwiredSaveOpenService final : public contracts::ISaveOpenService {
public:
    contracts::OpenSaveResultDto openSourceSave(const std::filesystem::path& sourceSave) override;
};

class UnwiredImportPlanningService final : public contracts::IImportPlanningService {
public:
    contracts::ImportPlanDto buildImportPlan(
        const std::vector<std::string>& assetIds,
        std::optional<int> targetUserSlotIndex = std::nullopt) override;
};

class UnwiredImportExecutionService final : public contracts::IImportExecutionService {
public:
    contracts::ImportResultDto executeImport(const contracts::ImportRequestDto& request) override;
};

class SessionBackedCatalogQueryService final : public contracts::ICatalogQueryService {
public:
    std::vector<contracts::CatalogItemDto> currentCatalog() const override;
    contracts::SessionStatusDto currentStatus() const override;

    void replace(contracts::OpenSaveResultDto result);
    void replace(contracts::ImportResultDto result);

private:
    contracts::OpenSaveResultDto currentResult_{};
};

struct AppServices final {
    std::shared_ptr<contracts::ISaveOpenService> openSaveService;
    std::shared_ptr<contracts::IImportPlanningService> importPlanningService;
    std::shared_ptr<contracts::IImportExecutionService> importExecutionService;
    std::shared_ptr<contracts::IExchangeService> exchangeService;
    std::shared_ptr<SessionBackedCatalogQueryService> catalogQueryService;

    static AppServices createDefault();
};

}  // namespace ac6dm::app
