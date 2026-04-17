#include "app/home_library_view.hpp"

#include <QApplication>
#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>

#include <gtest/gtest.h>

namespace ac6dm::app {
namespace {

QApplication& ensureApp() {
    static int argc = 1;
    static char appName[] = "ac6dm_native_tests";
    static char* argv[] = {appName, nullptr};
    static QApplication app(argc, argv);
    return app;
}

contracts::SourceBucket sourceBucketForLabel(const std::string& sourceLabel) {
    if (sourceLabel == "user1") {
        return contracts::SourceBucket::User1;
    }
    if (sourceLabel == "user2") {
        return contracts::SourceBucket::User2;
    }
    if (sourceLabel == "user3") {
        return contracts::SourceBucket::User3;
    }
    if (sourceLabel == "user4") {
        return contracts::SourceBucket::User4;
    }
    return contracts::SourceBucket::Share;
}

contracts::CatalogItemDto makeItem(const std::string& name, contracts::AssetOrigin origin, const std::string& sourceLabel) {
    contracts::CatalogItemDto item;
    item.assetId = name;
    item.displayName = name;
    item.sourceBucket = sourceBucketForLabel(sourceLabel);
    item.origin = origin;
    item.sourceLabel = sourceLabel;
    item.description = "test description";

    if (name.rfind("ac-", 0) == 0) {
        item.assetKind = contracts::AssetKind::Ac;
        item.archiveName = "Archive " + name.substr(3);
        item.machineName = "Machine " + name.substr(3);
        item.shareCode = origin == contracts::AssetOrigin::Share ? "SHARE-" + name.substr(3) : std::string{};
        item.writeCapability = origin == contracts::AssetOrigin::Share
            ? contracts::WriteCapability::ReadOnly
            : contracts::WriteCapability::Editable;
        item.slotLabel = "AC_SLOT";
        item.recordRef = "USER_DATA002/record/0?bucket=user1";
    } else {
        item.assetKind = contracts::AssetKind::Emblem;
        item.shareCode = origin == contracts::AssetOrigin::Share ? name : std::string{};
        item.writeCapability = origin == contracts::AssetOrigin::User
            ? contracts::WriteCapability::Editable
            : contracts::WriteCapability::ReadOnly;
        item.slotLabel = sourceLabel == "share" ? "SHARE" : "USER_EMBLEM_00";
    }

    return item;
}

TEST(HomeLibraryViewTest, ExposesEnglishWorkflowTabsFiltersAndInspector) {
    ensureApp();
    HomeLibraryView view;
    contracts::SessionStatusDto session;
    session.workflowAvailable = true;
    session.hasRealSave = true;
    session.summary = "real save opened";
    view.setSessionStatus(session);
    view.setCatalog({makeItem("share-emblem", contracts::AssetOrigin::Share, "share")});
    view.setAcCatalog({makeItem("ac-share", contracts::AssetOrigin::Share, "share")});
    view.show();

    auto* tabs = view.findChild<QTabWidget*>("libraryTabs");
    ASSERT_NE(tabs, nullptr);
    EXPECT_EQ(tabs->count(), 2);
    EXPECT_EQ(tabs->tabText(0), QString("Emblems"));
    EXPECT_EQ(tabs->tabText(1), QString("ACs"));

    auto* emblemTable = view.findChild<QTableWidget*>("emblemTable");
    auto* emblemSourceFilter = view.findChild<QComboBox*>("emblemSourceFilterCombo");
    auto* emblemImport = view.findChild<QPushButton*>("emblemImportSelectedButton");
    auto* emblemImportFile = view.findChild<QPushButton*>("emblemImportFileButton");
    auto* emblemExport = view.findChild<QPushButton*>("emblemExportButton");
    auto* emblemStatusBadge = view.findChild<QLabel*>("emblemStatusBadgeLabel");
    ASSERT_NE(emblemTable, nullptr);
    ASSERT_NE(emblemSourceFilter, nullptr);
    ASSERT_NE(emblemImport, nullptr);
    ASSERT_NE(emblemImportFile, nullptr);
    ASSERT_NE(emblemExport, nullptr);
    ASSERT_NE(emblemStatusBadge, nullptr);
    EXPECT_EQ(emblemSourceFilter->count(), 6);
    emblemTable->selectRow(0);
    QApplication::processEvents();
    EXPECT_EQ(emblemTable->columnCount(), 4);
    EXPECT_EQ(emblemImportFile->text(), QString("Import emblem"));
    EXPECT_TRUE(emblemImport->isEnabled());
    EXPECT_TRUE(emblemImportFile->isEnabled());
    EXPECT_TRUE(emblemExport->isEnabled());
    EXPECT_EQ(emblemStatusBadge->text(), QString("Read-only"));

    tabs->setCurrentIndex(1);
    QApplication::processEvents();
    auto* acTable = view.findChild<QTableWidget*>("acTable");
    auto* acSourceFilter = view.findChild<QComboBox*>("acSourceFilterCombo");
    auto* acImport = view.findChild<QPushButton*>("acImportSelectedButton");
    auto* acImportFile = view.findChild<QPushButton*>("acImportFileButton");
    auto* acExport = view.findChild<QPushButton*>("acExportButton");
    auto* acInspectorTitle = view.findChild<QLabel*>("acInspectorTitleLabel");
    ASSERT_NE(acTable, nullptr);
    ASSERT_NE(acSourceFilter, nullptr);
    ASSERT_NE(acImport, nullptr);
    ASSERT_NE(acImportFile, nullptr);
    ASSERT_NE(acExport, nullptr);
    ASSERT_NE(acInspectorTitle, nullptr);
    EXPECT_EQ(acSourceFilter->count(), 6);
    acTable->selectRow(0);
    QApplication::processEvents();
    EXPECT_EQ(acTable->columnCount(), 6);
    EXPECT_EQ(acImportFile->text(), QString("Import AC"));
    EXPECT_TRUE(acImport->isEnabled());
    EXPECT_TRUE(acImportFile->isEnabled());
    EXPECT_TRUE(acExport->isEnabled());
    EXPECT_TRUE(acInspectorTitle->text().contains(QString("Archive share")));
}

TEST(HomeLibraryViewTest, AppliesSourceFilterAndInlineBannerState) {
    ensureApp();
    HomeLibraryView view;
    contracts::SessionStatusDto session;
    session.workflowAvailable = true;
    session.hasRealSave = true;
    view.setSessionStatus(session);
    view.setCatalog({
        makeItem("share-emblem", contracts::AssetOrigin::Share, "share"),
        makeItem("user-emblem", contracts::AssetOrigin::User, "user1"),
    });
    view.show();

    auto* emblemSourceFilter = view.findChild<QComboBox*>("emblemSourceFilterCombo");
    auto* emblemTable = view.findChild<QTableWidget*>("emblemTable");
    auto* bannerFrame = view.findChild<QFrame*>("inlineStatusFrame");
    auto* bannerDetailsButton = view.findChild<QPushButton*>("inlineStatusDetailsButton");
    ASSERT_NE(emblemSourceFilter, nullptr);
    ASSERT_NE(emblemTable, nullptr);
    ASSERT_NE(bannerFrame, nullptr);
    ASSERT_NE(bannerDetailsButton, nullptr);

    emblemSourceFilter->setCurrentIndex(5);
    QApplication::processEvents();
    EXPECT_EQ(emblemTable->rowCount(), 1);

    view.setInlineStatus("Imported selected emblem into User 1.", false, true);
    QApplication::processEvents();
    EXPECT_TRUE(bannerFrame->isVisible());
    EXPECT_TRUE(bannerDetailsButton->isVisible());

    view.clearInlineStatus();
    QApplication::processEvents();
    EXPECT_FALSE(bannerFrame->isVisible());
}

}  // namespace
}  // namespace ac6dm::app
