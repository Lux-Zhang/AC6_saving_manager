#include "../core/ac/ac_preview_probe_service.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace {

std::vector<std::uint8_t> makeMinimalBmp() {
    return {
        0x42, 0x4D, 70, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0,
        40, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 1, 0, 24, 0,
        0, 0, 0, 0, 16, 0, 0, 0, 19, 0x0B, 0, 0, 19, 0x0B, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00,
        0xFF, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
    };
}

TEST(AcPreviewProbeTest, VerifiedEmbeddedPreviewUsesNativeEmbeddedState) {
    ac6dm::ac::AcPreviewProbeInput input;
    input.assetId = "ac:user1:0";
    input.recordRef = "files/ac/0";
    input.embeddedPreviewBytes = {0x89, 0x50, 0x4E, 0x47};
    input.embeddedPreviewVerified = true;

    const auto resolution = ac6dm::ac::probeAcPreview(input);
    EXPECT_EQ(std::string(ac6dm::preview::toString(resolution.state)), "native_embedded");
    EXPECT_EQ(resolution.handle.provenance, "native_embedded");
    ASSERT_TRUE(resolution.handle.sourceHint.has_value());
    EXPECT_EQ(*resolution.handle.sourceHint, "record-ref:files/ac/0");
}

TEST(AcPreviewProbeTest, EmbeddedBmpScanStabilizesPreviewProvenance) {
    ac6dm::ac::AcPreviewProbeInput input;
    input.assetId = "ac:share";
    input.recordRef = "USER_DATA006";
    input.recordPayloadBytes = {0x01, 0x02, 0x03, 0x04};
    const auto bmp = makeMinimalBmp();
    input.recordPayloadBytes.insert(input.recordPayloadBytes.end(), bmp.begin(), bmp.end());
    input.note = "scan";
    input.forensicPayloadScanEnabled = true;

    const auto resolution = ac6dm::ac::probeAcPreview(input);
    EXPECT_EQ(std::string(ac6dm::preview::toString(resolution.state)), "unknown");
    EXPECT_EQ(resolution.provenanceTag, "ac.embedded-preview-scan");
    ASSERT_TRUE(resolution.sourceHint.has_value());
    EXPECT_EQ(*resolution.sourceHint, "embedded-bmp@4");
    EXPECT_TRUE(resolution.handle.imageBytes.empty());
}

TEST(AcPreviewProbeTest, InvalidJpegHeaderDoesNotQualifyAsPreview) {
    ac6dm::ac::AcPreviewProbeInput input;
    input.assetId = "ac:share";
    input.recordRef = "USER_DATA006";
    input.recordPayloadBytes = {
        0x10, 0x20, 0xFF, 0xD8, 0xFF, 0x1A, 0xFA, 0x71,
        0xF3, 0x69, 0xEA, 0x6E, 0x92, 0xC4, 0xFF, 0xD9,
    };
    input.forensicPayloadScanEnabled = true;

    const auto resolution = ac6dm::ac::probeAcPreview(input);

    EXPECT_EQ(std::string(ac6dm::preview::toString(resolution.state)), "unknown");
    EXPECT_EQ(resolution.provenanceTag, "ac.embedded-preview-probe");
    EXPECT_FALSE(resolution.sourceHint.has_value());
    EXPECT_TRUE(resolution.handle.imageBytes.empty());
}

TEST(AcPreviewProbeTest, InvalidBmpHeaderDoesNotQualifyAsPreview) {
    ac6dm::ac::AcPreviewProbeInput input;
    input.assetId = "ac:user1:0";
    input.recordRef = "USER_DATA002/record/0";
    input.recordPayloadBytes = {
        0x42, 0x4D, 0x98, 0x2E, 0x51, 0xB7, 0x14, 0x5C,
        0xE7, 0x23, 0xD3, 0x1B, 0x83, 0x4A, 0x9A, 0x00,
        0x40, 0x00, 0x5E, 0x76, 0xAF, 0xE7, 0xC9, 0xDA,
        0xA5, 0x49, 0x86, 0xC4, 0x44, 0x4C, 0x6C, 0xE7,
    };
    input.forensicPayloadScanEnabled = true;

    const auto resolution = ac6dm::ac::probeAcPreview(input);

    EXPECT_EQ(std::string(ac6dm::preview::toString(resolution.state)), "unknown");
    EXPECT_EQ(resolution.provenanceTag, "ac.embedded-preview-probe");
    EXPECT_FALSE(resolution.sourceHint.has_value());
    EXPECT_TRUE(resolution.handle.imageBytes.empty());
}

TEST(AcPreviewProbeTest, MissingPreviewMapsToUnavailable) {
    ac6dm::ac::AcPreviewProbeInput input;
    input.assetId = "ac:share:2";
    input.recordRef = "extraFiles/ac/2";
    input.knownUnavailable = true;

    const auto resolution = ac6dm::ac::probeAcPreview(input);
    EXPECT_EQ(std::string(ac6dm::preview::toString(resolution.state)), "unavailable");
    EXPECT_TRUE(resolution.handle.imageBytes.empty());
    ASSERT_TRUE(resolution.handle.note.has_value());
    EXPECT_EQ(*resolution.handle.note, "Native preview not present in current sample");
}

TEST(AcPreviewProbeTest, UnqualifiedPreviewMapsToUnknown) {
    ac6dm::ac::AcPreviewProbeInput input;
    input.assetId = "ac:user2:1";
    input.recordRef = "files/ac/1";

    const auto resolution = ac6dm::ac::probeAcPreview(input);
    EXPECT_EQ(std::string(ac6dm::preview::toString(resolution.state)), "unknown");
    EXPECT_TRUE(resolution.handle.imageBytes.empty());
    ASSERT_TRUE(resolution.handle.note.has_value());
    EXPECT_EQ(*resolution.handle.note, "Preview provenance pending qualification");
}

}  // namespace
