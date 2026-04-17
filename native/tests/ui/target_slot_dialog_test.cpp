#include "app/target_slot_dialog.hpp"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>

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

TEST(TargetSlotDialogTest, UsesSuggestedTargetAndEnglishActions) {
    ensureApp();
    contracts::CatalogItemDto item;
    item.assetKind = contracts::AssetKind::Emblem;
    item.origin = contracts::AssetOrigin::Package;
    item.shareCode = "ABCD1234";

    contracts::ImportPlanDto plan;
    plan.summary = "Import package emblem into user slot.";
    plan.suggestedTargetUserSlotIndex = 2;
    plan.targetChoices = {
        {.code = 0, .label = "User 1", .note = "12 free slots"},
        {.code = 2, .label = "User 3", .note = "recommended"},
    };

    TargetSlotDialog dialog(item, plan);
    auto* combo = dialog.findChild<QComboBox*>("targetSlotCombo");
    auto* buttons = dialog.findChild<QDialogButtonBox*>();
    auto* introLabel = dialog.findChild<QLabel*>("targetSlotIntroLabel");
    ASSERT_NE(combo, nullptr);
    ASSERT_NE(buttons, nullptr);
    ASSERT_NE(introLabel, nullptr);

    EXPECT_EQ(dialog.windowTitle(), QString("Import to User Slot"));
    EXPECT_EQ(combo->count(), 2);
    EXPECT_EQ(combo->currentData().toInt(), 2);
    EXPECT_EQ(buttons->button(QDialogButtonBox::Ok)->text(), QString("Import"));
    EXPECT_EQ(buttons->button(QDialogButtonBox::Cancel)->text(), QString("Cancel"));
    EXPECT_TRUE(introLabel->text().contains(QString("exchange file")));
}

TEST(TargetSlotDialogTest, DisablesConfirmWhenBlocked) {
    ensureApp();
    contracts::CatalogItemDto item;
    item.assetKind = contracts::AssetKind::Ac;
    item.origin = contracts::AssetOrigin::Share;
    item.archiveName = "Archive";
    item.machineName = "Machine";

    contracts::ImportPlanDto plan;
    plan.summary = "Import selected share AC.";
    plan.blockers = {"No writable user slot is available."};

    TargetSlotDialog dialog(item, plan);
    auto* buttons = dialog.findChild<QDialogButtonBox*>();
    auto* blockerLabel = dialog.findChild<QLabel*>("targetSlotBlockerLabel");
    ASSERT_NE(buttons, nullptr);
    ASSERT_NE(blockerLabel, nullptr);

    EXPECT_FALSE(buttons->button(QDialogButtonBox::Ok)->isEnabled());
    EXPECT_FALSE(blockerLabel->isHidden());
    EXPECT_TRUE(blockerLabel->text().contains(QString("No writable user slot")));
}

}  // namespace
}  // namespace ac6dm::app
