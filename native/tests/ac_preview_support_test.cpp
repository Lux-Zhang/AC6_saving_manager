#include "../core/ac/ac_preview_support.hpp"

#include <gtest/gtest.h>

namespace {

TEST(AcPreviewSupportTest, Loader4StarterHashResolvesAllSlotsAndBuildLink) {
    const std::array<std::uint32_t, 16> assembleWords{
        0x12fc0380U,
        0x130b45c0U,
        0x131a8800U,
        0x1329ca40U,
        0x63949a00U,
        0x53dfd240U,
        0x742c1d80U,
        0xffffffffU,
        0x01315410U,
        0x0098bdf4U,
        0xffffffffU,
        0x01ca1204U,
        0x00049124U,
        0x0004905cU,
        0xffffffffU,
        0xffffffffU,
    };

    const auto preview = ac6dm::ac::tryBuildAdvancedGaragePreview(assembleWords);
    ASSERT_TRUE(preview.has_value());

    EXPECT_TRUE(preview->buildLinkCompatible);
    EXPECT_EQ(
        preview->buildLinkUrl,
        "https://matteosal.github.io/ac6-advanced-garage/?build=64-37-8-231-115-135-150-168-196-205-214-233");
    EXPECT_EQ(preview->note, "Advanced Garage preview mapping verified for all 12 slots.");

    ASSERT_EQ(preview->assemblySlots.size(), 12U);
    EXPECT_EQ(preview->assemblySlots[0].partName, "RF-024 TURNER");
    EXPECT_EQ(preview->assemblySlots[1].partName, "HI-32: BU-TT/A");
    EXPECT_EQ(preview->assemblySlots[2].partName, "BML-G1/P20MLT-04");
    EXPECT_EQ(preview->assemblySlots[3].partName, "(NOTHING)");
    EXPECT_EQ(preview->assemblySlots[4].partName, "HC-2000 FINDER EYE");
    EXPECT_EQ(preview->assemblySlots[5].partName, "CC-2000 ORBITER");
    EXPECT_EQ(preview->assemblySlots[6].partName, "AC-2000 TOOL ARM");
    EXPECT_EQ(preview->assemblySlots[7].partName, "2C-2000 CRAWLER");
    EXPECT_EQ(preview->assemblySlots[8].partName, "BST-G1/P10");
    EXPECT_EQ(preview->assemblySlots[9].partName, "FCS-G1/P01");
    EXPECT_EQ(preview->assemblySlots[10].partName, "AG-J-098 JOSO");
    EXPECT_EQ(preview->assemblySlots[11].partName, "(NOTHING)");
}

TEST(AcPreviewSupportTest, Loader4617VariantResolvesP05AndYabaBuildLink) {
    const std::array<std::uint32_t, 16> assembleWords{
        0x12fc0380U,
        0x130b45c0U,
        0x131a8800U,
        0x1329ca40U,
        0x63949a00U,
        0x53dffae0U,
        0x742c4490U,
        0xffffffffU,
        0x01315410U,
        0x00990bb0U,
        0x017d78a4U,
        0x01ccd0c0U,
        0x00049124U,
        0x0004905cU,
        0xffffffffU,
        0x047868c0U,
    };

    const auto preview = ac6dm::ac::tryBuildAdvancedGaragePreview(assembleWords);
    ASSERT_TRUE(preview.has_value());

    EXPECT_TRUE(preview->buildLinkCompatible);
    EXPECT_EQ(preview->assemblySlots[8].partName, "BST-G1/P10");
    EXPECT_EQ(preview->assemblySlots[9].partName, "FCS-G2/P05");
    EXPECT_EQ(preview->assemblySlots[10].partName, "AG-E-013 YABA");
    EXPECT_FALSE(preview->buildLinkUrl.empty());
    EXPECT_EQ(preview->note, "Advanced Garage preview mapping verified for all 12 slots.");
}

TEST(AcPreviewSupportTest, Loader4619VariantResolvesP12SmlAndYabaBuildLink) {
    const std::array<std::uint32_t, 16> assembleWords{
        0x12fc0380U,
        0x130b45c0U,
        0x131a8800U,
        0x1329ca40U,
        0x63949a00U,
        0x53dffae0U,
        0x742c4558U,
        0xffffffffU,
        0x00e5cc20U,
        0x0098bdf4U,
        0x0216ab00U,
        0x01ca5fc0U,
        0x00049124U,
        0x0004905cU,
        0xffffffffU,
        0xffffffffU,
    };

    const auto preview = ac6dm::ac::tryBuildAdvancedGaragePreview(assembleWords);
    ASSERT_TRUE(preview.has_value());

    EXPECT_TRUE(preview->buildLinkCompatible);
    EXPECT_EQ(preview->assemblySlots[9].partName, "FCS-G2/P12SML");
    EXPECT_EQ(preview->assemblySlots[10].partName, "AG-E-013 YABA");
    EXPECT_EQ(preview->assemblySlots[11].partName, "(NOTHING)");
    EXPECT_FALSE(preview->buildLinkUrl.empty());
    EXPECT_EQ(preview->note, "Advanced Garage preview mapping verified for all 12 slots.");
}

TEST(AcPreviewSupportTest, NameTableMappingResolvesLittleGemAndTianQiang) {
    const std::array<std::uint32_t, 16> assembleWords{
        50010000U + 0x10000000U,
        0x130b45c0U,
        0x131a8800U,
        0x1329ca40U,
        0x63949a00U,
        0x53dfd240U,
        0x742c1d80U,
        0xffffffffU,
        0x01315410U,
        10040000U,
        0xffffffffU,
        0x01ca1204U,
        0x00049124U,
        0x0004905cU,
        0xffffffffU,
        0xffffffffU,
    };

    const auto preview = ac6dm::ac::tryBuildAdvancedGaragePreview(assembleWords);
    ASSERT_TRUE(preview.has_value());

    EXPECT_EQ(preview->assemblySlots[0].partName, "LITTLE GEM");
    EXPECT_EQ(preview->assemblySlots[4].partName, "DF-HD-08 TIAN-QIANG");
    EXPECT_TRUE(preview->buildLinkCompatible);
}

}  // namespace
