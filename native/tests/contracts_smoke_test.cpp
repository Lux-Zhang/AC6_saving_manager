#include "ac6dm/contracts/asset_catalog.hpp"
#include "ac6dm/contracts/common_types.hpp"
#include "ac6dm/contracts/error_contracts.hpp"
#include "ac6dm/contracts/emblem_workflow.hpp"
#include "ac6dm/contracts/slot_contracts.hpp"

#include <gtest/gtest.h>

TEST(ContractsSmokeTest, CatalogItemDefaultsAreUsable) {
    ac6dm::contracts::CatalogItemDto item;
    item.assetId = "extraFiles:0";
    item.displayName = "Share Emblem";
    item.assetKind = ac6dm::contracts::AssetKind::Emblem;
    item.sourceBucket = ac6dm::contracts::SourceBucket::Share;
    item.slotLabel = "SHARE";
    item.origin = ac6dm::contracts::AssetOrigin::Share;

    EXPECT_EQ(item.assetId, "extraFiles:0");
    EXPECT_EQ(item.slotLabel, "SHARE");
    EXPECT_EQ(ac6dm::contracts::toString(item.assetKind), "emblem");
    EXPECT_EQ(ac6dm::contracts::toString(item.sourceBucket), "share");
}

TEST(ContractsSmokeTest, SlotFormatterAndMessagesStayStable) {
    EXPECT_EQ(ac6dm::contracts::formatUserSlotLabel(7), "USER_EMBLEM_07");
    EXPECT_EQ(std::string(ac6dm::contracts::errors::kOpenSaveFailedUserMessage), "无法打开该存档，请确认文件未被占用。");
}
