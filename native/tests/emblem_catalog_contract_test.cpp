#include "core/emblem/emblem_domain.hpp"

#include <gtest/gtest.h>

namespace {

ac6dm::emblem::EmblemRecord makeRecord(std::string ugcId) {
    ac6dm::emblem::EmblemRecord record;
    record.category = static_cast<std::int8_t>(ac6dm::emblem::kEmblemUserCategory);
    record.ugcId = std::move(ugcId);
    return record;
}

ac6dm::emblem::UserDataFile makeEmbcFile(const ac6dm::emblem::EmblemRecord& record) {
    return ac6dm::emblem::UserDataFile{
        .fileType = "EMBC",
        .data = ac6dm::emblem::serializeEmblemRecordForTests(record),
    };
}

}  // namespace

TEST(EmblemCatalogContractTest, FilesBucketIsSplitIntoFourUserSourceBuckets) {
    ac6dm::emblem::UserDataContainer container;
    for (int index = 0; index < 76; ++index) {
        container.files.push_back(makeEmbcFile(makeRecord("user-" + std::to_string(index))));
    }
    container.extraFiles.push_back(makeEmbcFile(makeRecord("share-0")));

    const auto snapshot = ac6dm::emblem::buildCatalogSnapshot(container);
    ASSERT_EQ(snapshot.catalog.size(), 77U);

    EXPECT_EQ(ac6dm::contracts::toString(snapshot.catalog.at(0).sourceBucket), "user1");
    EXPECT_EQ(snapshot.catalog.at(0).slotIndex.value_or(-1), 0);
    EXPECT_EQ(ac6dm::contracts::toString(snapshot.catalog.at(18).sourceBucket), "user1");
    EXPECT_EQ(ac6dm::contracts::toString(snapshot.catalog.at(19).sourceBucket), "user2");
    EXPECT_EQ(ac6dm::contracts::toString(snapshot.catalog.at(38).sourceBucket), "user3");
    EXPECT_EQ(ac6dm::contracts::toString(snapshot.catalog.at(57).sourceBucket), "user4");
    EXPECT_EQ(ac6dm::contracts::toString(snapshot.catalog.back().sourceBucket), "share");
    EXPECT_EQ(ac6dm::contracts::toString(snapshot.catalog.front().assetKind), "emblem");
    EXPECT_EQ(ac6dm::contracts::toString(snapshot.catalog.front().writeCapability), "editable");
    EXPECT_EQ(ac6dm::contracts::toString(snapshot.catalog.back().writeCapability), "read_only");
    EXPECT_EQ(snapshot.catalog.front().recordRef, "USER_DATA007/files/0");
    EXPECT_EQ(snapshot.catalog.back().recordRef, "USER_DATA007/extraFiles/0");
    EXPECT_EQ(snapshot.userSlotsInUse.size(), 4U);
}
