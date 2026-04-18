#pragma once

#include "ac6dm/contracts/asset_catalog.hpp"
#include "ac6dm/contracts/emblem_workflow.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace ac6dm::ac {

class AcQualificationError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct AcCatalogRecord final {
    std::string assetId;
    contracts::SourceBucket sourceBucket{contracts::SourceBucket::Share};
    std::optional<int> slotIndex;
    struct AcRecordRef final {
        std::filesystem::path containerFile;
        std::optional<int> ownerSlot;
        std::string logicalBucket;
        int recordIndex{-1};
        std::uint64_t byteOffset{0};
        std::uint64_t byteLength{0};
        std::string stableId;
        bool qualified{false};
    } recordRef;
    std::uintmax_t byteSize{0};
};

using AcRecordRef = AcCatalogRecord::AcRecordRef;

struct AcEditableClonePatchV1 final {
    std::string version{"v1"};
    bool qualified{false};
    std::string note{"paired diff not qualified"};
};

struct AcCatalogSnapshot final {
    std::vector<contracts::CatalogItemDto> catalog;
    std::vector<AcCatalogRecord> records;
    bool recordMapQualified{false};
    AcEditableClonePatchV1 editableClonePatch;
};

struct AcWriteReadback final {
    std::string sourceAssetId;
    int targetUserSlotIndex{-1};
    AcRecordRef targetRecordRef;
    std::size_t expectedPayloadBytes{0};
    std::size_t actualPayloadBytes{0};
    bool payloadMatches{false};
};

AcCatalogSnapshot buildProvisionalCatalogSnapshot(const std::filesystem::path& unpackedSaveDir);
std::string serializeRecordRef(const AcRecordRef& recordRef);

contracts::ImportPlanDto buildShareAcImportPlan(
    const AcCatalogSnapshot& snapshot,
    const std::string& assetId,
    std::optional<int> targetUserSlotIndex = std::nullopt);

contracts::ImportPlanDto buildExchangeAcImportPlan(
    const AcCatalogSnapshot& snapshot,
    std::optional<int> targetUserSlotIndex = std::nullopt);

std::string describeTargetSlotCode(int targetUserSlotIndex);

std::vector<std::uint8_t> readRecordPayload(
    const std::filesystem::path& unpackedSaveDir,
    const std::string& assetId,
    const AcCatalogSnapshot& snapshot);

void applyPayloadToUserSlot(
    const std::filesystem::path& unpackedSaveDir,
    const std::vector<std::uint8_t>& payload,
    int targetUserSlotIndex,
    std::vector<std::uint8_t>* outWrittenPayload = nullptr);

std::vector<std::uint8_t> applyShareAcPayloadCopy(
    const std::filesystem::path& unpackedSaveDir,
    const AcCatalogSnapshot& snapshot,
    const std::string& assetId,
    int targetUserSlotIndex);

AcWriteReadback verifyShareAcPayloadReadback(
    const std::filesystem::path& unpackedSaveDir,
    int targetUserSlotIndex,
    const std::vector<std::uint8_t>& expectedPayload,
    std::string sourceAssetId = "ac:share:0");

}  // namespace ac6dm::ac
