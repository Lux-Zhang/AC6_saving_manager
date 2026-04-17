#pragma once

#include "ac6dm/contracts/emblem_workflow.hpp"
#include "ac6dm/contracts/slot_contracts.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace ac6dm::emblem {

constexpr std::uint8_t kEmblemUserCategory = 1;
constexpr std::uint8_t kEmblemImportedUserCategory = 2;

class EmblemFormatError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct DateTimeBlock final {
    std::uint64_t filetime{0};
    std::uint64_t packedSystemTime{0};

    static DateTimeBlock now();
    static DateTimeBlock deserialize(const std::vector<std::uint8_t>& payload);
    std::vector<std::uint8_t> serialize() const;
    std::string toIso8601() const;
};

struct GroupData final {
    std::int16_t decalId{0};
    std::int16_t posX{0};
    std::int16_t posY{0};
    std::int16_t scaleX{0};
    std::int16_t scaleY{0};
    std::int16_t angle{0};
    std::uint8_t red{0};
    std::uint8_t green{0};
    std::uint8_t blue{0};
    std::uint8_t alpha{0};
    std::int16_t maskMode{0};
    std::int16_t pad{0};
};

struct EmblemGroup final {
    GroupData data;
    std::vector<EmblemGroup> children;
};

struct EmblemLayer final {
    EmblemGroup group;
};

struct EmblemImage final {
    std::vector<EmblemLayer> layers;
};

struct EmblemProvenance final {
    std::string sourceBucket;
    int fileIndex{-1};
    std::string fileType;
};

struct EmblemBlock final {
    std::string name;
    std::vector<std::uint8_t> payload;
};

struct EmblemRecord final {
    std::int8_t category{0};
    std::string ugcId;
    std::optional<std::int64_t> creatorId;
    DateTimeBlock dateTime;
    EmblemImage image;
    std::vector<EmblemBlock> rawBlocks;
    std::optional<EmblemProvenance> provenance;
};

struct UserDataFile final {
    std::string fileType;
    std::vector<std::uint8_t> data;
};

struct UserDataContainer final {
    std::array<std::uint8_t, 16> iv{};
    std::int32_t unk1{0};
    std::int32_t unk2{0};
    std::int32_t unk3{0};
    std::int32_t unk4{0};
    std::vector<UserDataFile> files;
    std::vector<UserDataFile> extraFiles;
};

struct EmblemSelection final {
    std::string assetId;
    std::string sourceBucket;
    int fileIndex{-1};
    EmblemRecord record;
    std::vector<std::uint8_t> rawPayload;
};

struct EmblemCatalogSnapshot final {
    std::vector<contracts::CatalogItemDto> catalog;
    std::vector<EmblemSelection> selections;
    std::vector<int> userSlotsInUse;
    std::array<int, 4> userBucketCounts{};
    int userFileCount{0};
};

struct EmblemImportReadback final {
    int targetUserSlotIndex{-1};
    int targetFileIndex{-1};
    EmblemSelection selection;
};

class UserData007Codec final {
public:
    UserDataContainer readEncrypted(const std::filesystem::path& path) const;
    std::vector<std::uint8_t> writeEncrypted(const UserDataContainer& container) const;
};

EmblemCatalogSnapshot buildCatalogSnapshot(const UserDataContainer& container);
std::vector<contracts::ImportTargetChoiceDto> buildTargetChoices(const EmblemCatalogSnapshot& snapshot);
std::optional<int> nextWritableFileIndexForSnapshot(
    const EmblemCatalogSnapshot& snapshot,
    int targetUserSlotIndex);

std::vector<std::uint8_t> serializeEmblemRecordForTests(const EmblemRecord& record);
const EmblemSelection& findSelectionByAssetId(const EmblemCatalogSnapshot& snapshot, const std::string& assetId);
std::vector<std::uint8_t> exportSelectionPayload(const EmblemCatalogSnapshot& snapshot, const std::string& assetId);
std::vector<std::uint8_t> buildEditableClonePayload(
    const std::vector<std::uint8_t>& rawPayload,
    std::uint8_t editableCategory = kEmblemImportedUserCategory);

UserDataContainer applyEmblemPayloadToUserSlot(
    const UserDataContainer& container,
    const std::vector<std::uint8_t>& rawPayload,
    int targetUserSlotIndex,
    int* assignedSlot = nullptr);

contracts::ImportPlanDto buildSingleShareImportPlan(
    const EmblemCatalogSnapshot& snapshot,
    const std::string& assetId,
    std::optional<int> targetUserSlotIndex = std::nullopt);

UserDataContainer applySingleShareImport(
    const UserDataContainer& container,
    const std::string& assetId,
    int targetUserSlotIndex,
    int* assignedSlot = nullptr);

UserDataContainer applySingleShareImport(
    const UserDataContainer& container,
    const std::string& assetId,
    int* assignedSlot);

EmblemImportReadback verifySingleShareImportReadback(
    const EmblemCatalogSnapshot& snapshot,
    int targetUserSlotIndex,
    const std::vector<std::uint8_t>& expectedPayload);

}  // namespace ac6dm::emblem
