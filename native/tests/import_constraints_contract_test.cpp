#include "core/emblem/emblem_domain.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>

namespace {

ac6dm::emblem::EmblemRecord makeRecord(const std::string& ugcId, std::int8_t category) {
    ac6dm::emblem::GroupData group;
    group.decalId = 100;
    group.scaleX = 256;
    group.scaleY = 256;
    group.red = 255;
    group.green = 64;
    group.blue = 64;
    group.alpha = 255;

    ac6dm::emblem::EmblemRecord record;
    record.category = category;
    record.ugcId = ugcId;
    record.dateTime = ac6dm::emblem::DateTimeBlock::now();
    record.image.layers.push_back(ac6dm::emblem::EmblemLayer{.group = ac6dm::emblem::EmblemGroup{.data = group}});
    return record;
}

ac6dm::emblem::UserDataFile makeEmbcFile(const std::string& ugcId, std::int8_t category) {
    ac6dm::emblem::UserDataFile file;
    file.fileType = "EMBC";
    file.data = ac6dm::emblem::serializeEmblemRecordForTests(makeRecord(ugcId, category));
    return file;
}

std::vector<std::uint8_t> makeBlock(const std::string& name, const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> bytes(16, 0);
    std::copy(name.begin(), name.end(), bytes.begin());
    const auto appendLe32 = [&](std::uint32_t value) {
        bytes.push_back(static_cast<std::uint8_t>(value & 0xFF));
        bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
        bytes.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
        bytes.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
    };
    const auto appendLe64Zero = [&]() {
        for (int index = 0; index < 8; ++index) {
            bytes.push_back(0);
        }
    };
    appendLe32(static_cast<std::uint32_t>(payload.size()));
    appendLe32(0);
    appendLe64Zero();
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    return bytes;
}

std::vector<std::uint8_t> withUnknownBlockBeforeEnd(std::vector<std::uint8_t> payload) {
    constexpr std::size_t kBlockFooterBytes = 32;
    const auto customBlock = makeBlock("Memo", {0xAA, 0xBB, 0xCC});
    payload.insert(
        payload.end() - static_cast<std::ptrdiff_t>(kBlockFooterBytes),
        customBlock.begin(),
        customBlock.end());
    return payload;
}

ac6dm::emblem::UserDataContainer makeContainer() {
    ac6dm::emblem::UserDataContainer container;
    for (int index = 0; index < 38; ++index) {
        container.files.push_back(makeEmbcFile("user-slot-" + std::to_string(index), ac6dm::emblem::kEmblemUserCategory));
    }
    container.extraFiles.push_back(makeEmbcFile("share-slot-0", ac6dm::emblem::kEmblemUserCategory));
    return container;
}

}  // namespace

TEST(ImportConstraintsContractTest, PlanBlocksWhenCatalogIsEmpty) {
    ac6dm::emblem::EmblemCatalogSnapshot snapshot;
    const auto plan = ac6dm::emblem::buildSingleShareImportPlan(snapshot, "extraFiles:0");
    EXPECT_FALSE(plan.blockers.empty());
}

TEST(ImportConstraintsContractTest, PlanRequiresExplicitTargetUserSlot) {
    const auto snapshot = ac6dm::emblem::buildCatalogSnapshot(makeContainer());
    const auto plan = ac6dm::emblem::buildSingleShareImportPlan(snapshot, "extraFiles:0", std::nullopt);
    EXPECT_TRUE(plan.blockers.empty());
    EXPECT_EQ(plan.targetSlot, "待用户选择");
    ASSERT_TRUE(plan.suggestedTargetUserSlotIndex.has_value());
    EXPECT_EQ(*plan.suggestedTargetUserSlotIndex, 2);
}

TEST(ImportConstraintsContractTest, PlanRejectsNonShareSourceAndInvalidTarget) {
    const auto snapshot = ac6dm::emblem::buildCatalogSnapshot(makeContainer());
    const auto nonSharePlan = ac6dm::emblem::buildSingleShareImportPlan(snapshot, "files:0", 0);
    ASSERT_FALSE(nonSharePlan.blockers.empty());

    const auto invalidTargetPlan = ac6dm::emblem::buildSingleShareImportPlan(snapshot, "extraFiles:0", 4);
    ASSERT_FALSE(invalidTargetPlan.blockers.empty());
    EXPECT_EQ(invalidTargetPlan.targetSlot, "无效栏位");
}

TEST(ImportConstraintsContractTest, ApplyUsesExplicitTargetSlotAndReadbackMatchesPayload) {
    const auto container = makeContainer();
    int assignedSlot = -1;
    const auto updated = ac6dm::emblem::applySingleShareImport(container, "extraFiles:0", 2, &assignedSlot);

    EXPECT_EQ(assignedSlot, 38);
    ASSERT_GT(updated.files.size(), 38U);

    const auto snapshot = ac6dm::emblem::buildCatalogSnapshot(updated);
    const auto readback = ac6dm::emblem::verifySingleShareImportReadback(snapshot, 2, updated.files[38].data);

    EXPECT_EQ(readback.targetUserSlotIndex, 2);
    EXPECT_EQ(readback.targetFileIndex, 38);
    EXPECT_EQ(readback.selection.record.ugcId, "share-slot-0");
    EXPECT_EQ(readback.selection.record.category, ac6dm::emblem::kEmblemImportedUserCategory);
}

TEST(ImportConstraintsContractTest, ApplyPreservesUnknownBlocksAndOnlyPatchesCategory) {
    auto container = makeContainer();
    container.extraFiles.front().data = withUnknownBlockBeforeEnd(container.extraFiles.front().data);

    const auto expectedPayload = ac6dm::emblem::buildEditableClonePayload(container.extraFiles.front().data);
    const auto updated = ac6dm::emblem::applySingleShareImport(container, "extraFiles:0", 2, nullptr);

    ASSERT_GT(updated.files.size(), 38U);
    EXPECT_EQ(updated.files[38].data, expectedPayload);
    const auto snapshot = ac6dm::emblem::buildCatalogSnapshot(updated);
    const auto readback = ac6dm::emblem::verifySingleShareImportReadback(snapshot, 2, expectedPayload);
    EXPECT_EQ(readback.selection.record.category, ac6dm::emblem::kEmblemImportedUserCategory);
    ASSERT_EQ(readback.selection.record.rawBlocks.size(), 5U);
    EXPECT_EQ(readback.selection.record.rawBlocks[0].name, "Category");
    EXPECT_EQ(readback.selection.record.rawBlocks[1].name, "UgcID");
    EXPECT_EQ(readback.selection.record.rawBlocks[2].name, "DateTime");
    EXPECT_EQ(readback.selection.record.rawBlocks[3].name, "Image");
    EXPECT_EQ(readback.selection.record.rawBlocks[4].name, "Memo");
}

TEST(ImportConstraintsContractTest, ApplyRejectsSparseOrOutOfRangeTargetSlot) {
    const auto container = makeContainer();
    EXPECT_THROW(ac6dm::emblem::applySingleShareImport(container, "extraFiles:0", 4, nullptr), ac6dm::emblem::EmblemFormatError);
    EXPECT_THROW(ac6dm::emblem::applySingleShareImport(container, "extraFiles:0", 1, nullptr), ac6dm::emblem::EmblemFormatError);

    ac6dm::emblem::UserDataContainer sparseContainer;
    sparseContainer.files.push_back(makeEmbcFile("user-slot-0", ac6dm::emblem::kEmblemUserCategory));
    sparseContainer.extraFiles.push_back(makeEmbcFile("share-slot-0", ac6dm::emblem::kEmblemUserCategory));
    EXPECT_THROW(ac6dm::emblem::applySingleShareImport(sparseContainer, "extraFiles:0", 3, nullptr), ac6dm::emblem::EmblemFormatError);
}
