#include "../core/emblem/emblem_preview_service.hpp"

#include <QApplication>

#include <gtest/gtest.h>

namespace {

QApplication& ensureApp() {
    static int argc = 1;
    static char appName[] = "ac6dm_native_tests";
    static char* argv[] = {appName, nullptr};
    static QApplication app(argc, argv);
    return app;
}

ac6dm::emblem::EmblemRecord makeSimpleRecord() {
    ac6dm::emblem::EmblemRecord record;
    ac6dm::emblem::EmblemGroup group;
    group.data.decalId = 100;
    group.data.scaleX = 512;
    group.data.scaleY = 512;
    group.data.red = 255;
    group.data.green = 64;
    group.data.blue = 64;
    group.data.alpha = 255;

    ac6dm::emblem::EmblemLayer layer;
    layer.group = group;
    record.image.layers.push_back(layer);
    return record;
}

TEST(EmblemPreviewServiceTest, DerivedRenderPreviewProducesPngBytes) {
    ensureApp();
    const auto resolution = ac6dm::emblem::buildEmblemPreview(makeSimpleRecord());

    EXPECT_EQ(std::string(ac6dm::preview::toString(resolution.state)), "derived_render");
    EXPECT_EQ(resolution.provenanceTag, "emblem.qt-basic-solids-only-v1");
    ASSERT_FALSE(resolution.handle.imageBytes.empty());
    EXPECT_EQ(resolution.handle.kind, ac6dm::contracts::PreviewKind::Png);
    EXPECT_EQ(resolution.handle.provenance, "derived_render");
    ASSERT_TRUE(resolution.handle.sourceHint.has_value());
    EXPECT_EQ(*resolution.handle.sourceHint, "rendered-from-emblem-record");
}

TEST(EmblemPreviewServiceTest, UnsupportedDecalIdsAreReportedInPreviewNote) {
    ensureApp();
    auto record = makeSimpleRecord();
    record.image.layers.front().group.data.decalId = 777;

    const auto resolution = ac6dm::emblem::buildEmblemPreview(record);

    EXPECT_EQ(std::string(ac6dm::preview::toString(resolution.state)), "derived_render");
    ASSERT_TRUE(resolution.handle.note.has_value());
    EXPECT_TRUE(resolution.handle.note->find("777") != std::string::npos);
}

}  // namespace
