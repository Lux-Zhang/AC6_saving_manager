#include "../core/ac/ac_regulation_catalog.hpp"

#include <gtest/gtest.h>

namespace {

TEST(AcRegulationCatalogTest, LoadsValidatedSnapshotMetadata) {
    const auto& catalog = ac6dm::ac::regulationPresetCatalog();

    EXPECT_EQ(catalog.source, "validated-from-regulation-bin-snapshot");
    EXPECT_FALSE(catalog.generatedAt.empty());
    EXPECT_EQ(
        catalog.runtimeMetadata.validatedUserData010PlainSha256,
        "556513fc33bf2ac8143a7c18d8a768d805de9fb3770c98a1de05c2c5de3b10d6");
    EXPECT_EQ(catalog.runtimeMetadata.validatedUserData010PlainLength, 2359344U);
    EXPECT_EQ(catalog.runtimeMetadata.validatedUserData010EmbeddedOffset, 20U);
    EXPECT_EQ(catalog.runtimeMetadata.validatedUserData010HeaderMagic, " GER");
    EXPECT_EQ(catalog.runtimeMetadata.validatedUserData010Version, 2U);
    EXPECT_GE(catalog.charaRows.size(), 500U);
    EXPECT_EQ(catalog.arenaRows.size(), 41U);
}

TEST(AcRegulationCatalogTest, NightfallArenaLinkResolvesToDirectCharaEquipment) {
    const auto* arenaRow = ac6dm::ac::findRegulationArenaRow(208);
    ASSERT_NE(arenaRow, nullptr);
    EXPECT_EQ(arenaRow->name, "51-016 GA / NIGHTFALL");
    EXPECT_EQ(arenaRow->charaId, 10009000);
    EXPECT_EQ(arenaRow->npcId, 10000000);

    const auto* charaRow = ac6dm::ac::findRegulationArenaResolvedCharaRow(208);
    ASSERT_NE(charaRow, nullptr);
    EXPECT_EQ(charaRow->name, "NIGHTFALL (Raven)");
    EXPECT_EQ(ac6dm::ac::findRegulationCharaEquipValue(*charaRow, "equip_Wep_Right"), 15010100);
    EXPECT_EQ(ac6dm::ac::findRegulationCharaEquipValue(*charaRow, "equip_Wep_Left"), 20010000);
    EXPECT_EQ(ac6dm::ac::findRegulationCharaEquipValue(*charaRow, "equip_Bolt"), 30210000);
    EXPECT_EQ(ac6dm::ac::findRegulationCharaEquipValue(*charaRow, "equip_AssaultDrive"), 75010000);
}

TEST(AcRegulationCatalogTest, TenderfootRowIsPresentAsDirectPresetCandidate) {
    const auto* charaRow = ac6dm::ac::findRegulationCharaRow(11409000);
    ASSERT_NE(charaRow, nullptr);
    EXPECT_EQ(charaRow->name, "TENDERFOOT (G13 Raven)");
    EXPECT_EQ(ac6dm::ac::findRegulationCharaEquipValue(*charaRow, "equip_Wep_Right"), 10000100);
    EXPECT_EQ(ac6dm::ac::findRegulationCharaEquipValue(*charaRow, "equip_Wep_Left"), 10020100);
    EXPECT_EQ(ac6dm::ac::findRegulationCharaEquipValue(*charaRow, "equip_Bolt"), 30080000);
    EXPECT_EQ(ac6dm::ac::findRegulationCharaEquipValue(*charaRow, "equip_Arrow"), 25010100);
}

}  // namespace
