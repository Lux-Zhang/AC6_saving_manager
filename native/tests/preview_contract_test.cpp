#include "../core/preview/preview_models.hpp"

#include <gtest/gtest.h>

namespace {

TEST(PreviewContractTest, StateStringsStayStable) {
    using ac6dm::preview::PreviewState;
    EXPECT_STREQ(ac6dm::preview::toString(PreviewState::NativeEmbedded), "native_embedded");
    EXPECT_STREQ(ac6dm::preview::toString(PreviewState::DerivedRender), "derived_render");
    EXPECT_STREQ(ac6dm::preview::toString(PreviewState::Unavailable), "unavailable");
    EXPECT_STREQ(ac6dm::preview::toString(PreviewState::Unknown), "unknown");
}

TEST(PreviewContractTest, DetailFieldsExposeStableProvenanceKeys) {
    auto resolution = ac6dm::preview::makeUnknownPreview("ac.embedded-preview-probe", "Preview provenance pending qualification");
    std::vector<ac6dm::contracts::DetailField> fields;
    ac6dm::preview::appendPreviewDetailFields(fields, resolution);

    ASSERT_EQ(fields.size(), 3U);
    EXPECT_EQ(fields[0].label, "PreviewState");
    EXPECT_EQ(fields[0].value, "unknown");
    EXPECT_EQ(fields[1].label, "PreviewProvenance");
    EXPECT_EQ(fields[1].value, "ac.embedded-preview-probe");
    EXPECT_EQ(fields[2].label, "PreviewNote");
}

}  // namespace
