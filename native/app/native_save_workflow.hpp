#pragma once

#include "app_services.hpp"
#include "core/ac/ac_catalog.hpp"
#include "core/emblem/emblem_domain.hpp"
#include "core/tool_adapter/WitchyBndProcessAdapter.hpp"
#include "core/workspace/SessionWorkspace.hpp"

#include <memory>
#include <optional>

namespace ac6dm::app {

struct NativeWorkflowState final {
    std::filesystem::path appRoot;
    std::optional<std::filesystem::path> sourceSave;
    std::optional<std::filesystem::path> stagedSave;
    std::optional<std::filesystem::path> unpackedDir;
    std::optional<std::filesystem::path> userData007Path;
    std::optional<workspace::SessionWorkspace> workspace;
    std::optional<emblem::UserDataContainer> container;
    std::optional<emblem::EmblemCatalogSnapshot> snapshot;
    std::optional<ac::AcCatalogSnapshot> acSnapshot;
};

class NativeOpenSaveService final : public contracts::ISaveOpenService {
public:
    explicit NativeOpenSaveService(std::shared_ptr<NativeWorkflowState> state);
    contracts::OpenSaveResultDto openSourceSave(const std::filesystem::path& sourceSave) override;

private:
    std::shared_ptr<NativeWorkflowState> state_;
};

class NativeImportPlanningService final : public contracts::IImportPlanningService {
public:
    explicit NativeImportPlanningService(std::shared_ptr<NativeWorkflowState> state);
    contracts::ImportPlanDto buildImportPlan(
        const std::vector<std::string>& assetIds,
        std::optional<int> targetUserSlotIndex = std::nullopt) override;

private:
    std::shared_ptr<NativeWorkflowState> state_;
};

class NativeImportExecutionService final : public contracts::IImportExecutionService {
public:
    explicit NativeImportExecutionService(std::shared_ptr<NativeWorkflowState> state);
    contracts::ImportResultDto executeImport(const contracts::ImportRequestDto& request) override;

private:
    std::shared_ptr<NativeWorkflowState> state_;
};

}  // namespace ac6dm::app
