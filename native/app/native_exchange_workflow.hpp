#pragma once

#include "ac6dm/contracts/service_interfaces.hpp"
#include "native_save_workflow.hpp"

#include <memory>
#include <optional>

namespace ac6dm::app {

class NativeExchangeService final : public contracts::IExchangeService {
public:
    explicit NativeExchangeService(std::shared_ptr<NativeWorkflowState> state);

    std::optional<contracts::CatalogItemDto> inspectExchangeFile(const std::filesystem::path& exchangeFilePath) override;
    contracts::ActionResultDto exportAsset(const std::string& assetId, const std::filesystem::path& outputPath) override;
    contracts::ImportPlanDto buildExchangeImportPlan(
        const std::filesystem::path& exchangeFilePath,
        std::optional<int> targetUserSlotIndex = std::nullopt) override;
    contracts::ImportResultDto importExchangeFile(
        const std::filesystem::path& exchangeFilePath,
        const std::filesystem::path& outputSavePath,
        std::optional<int> targetUserSlotIndex) override;

private:
    std::shared_ptr<NativeWorkflowState> state_;
};

}  // namespace ac6dm::app
