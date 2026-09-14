#include "../src/tray_menu.h"
#include "test_assert.h"

TEST(TrayMenuSingleDeviceManualOn) {
    EXPECT_TRUE(IsTrayMenuItemChecked(
        TrayPowerModeItem::On, false, false, false, true, true, 1, 1));
    EXPECT_FALSE(IsTrayMenuItemChecked(
        TrayPowerModeItem::Off, false, false, false, true, true, 1, 1));
    EXPECT_FALSE(IsTrayMenuItemChecked(
        TrayPowerModeItem::Smart, false, false, false, true, true, 1, 1));
}

TEST(TrayMenuSingleDeviceManualOff) {
    EXPECT_FALSE(IsTrayMenuItemChecked(
        TrayPowerModeItem::On, false, false, false, true, false, 0, 1));
    EXPECT_TRUE(IsTrayMenuItemChecked(
        TrayPowerModeItem::Off, false, false, false, true, false, 0, 1));
}

TEST(TrayMenuSmartModeExclusive) {
    EXPECT_TRUE(IsTrayMenuItemChecked(
        TrayPowerModeItem::Smart, true, false, false, true, true, 1, 1));
    EXPECT_FALSE(IsTrayMenuItemChecked(
        TrayPowerModeItem::On, true, false, false, true, true, 1, 1));
    EXPECT_FALSE(IsTrayMenuItemChecked(
        TrayPowerModeItem::Off, true, false, false, true, false, 0, 1));
}

TEST(TrayMenuMultiDeviceAllOn) {
    EXPECT_TRUE(IsTrayMenuItemChecked(
        TrayPowerModeItem::On, false, false, false, true, true, 2, 2));
    EXPECT_FALSE(IsTrayMenuItemChecked(
        TrayPowerModeItem::Off, false, false, false, true, true, 2, 2));
}

TEST(TrayMenuMultiDeviceAllOff) {
    EXPECT_TRUE(IsTrayMenuItemChecked(
        TrayPowerModeItem::Off, false, false, false, true, false, 0, 2));
}

TEST(TrayMenuScheduleMode) {
    EXPECT_TRUE(IsTrayMenuItemChecked(
        TrayPowerModeItem::Schedule, false, true, false, true, true, 1, 1));
    EXPECT_FALSE(IsTrayMenuItemChecked(
        TrayPowerModeItem::Smart, false, true, false, true, true, 1, 1));
}

void RunTrayMenuTests() {
    std::printf("tray menu tests\n");
    RUN_TEST(TrayMenuSingleDeviceManualOn);
    RUN_TEST(TrayMenuSingleDeviceManualOff);
    RUN_TEST(TrayMenuSmartModeExclusive);
    RUN_TEST(TrayMenuMultiDeviceAllOn);
    RUN_TEST(TrayMenuMultiDeviceAllOff);
    RUN_TEST(TrayMenuScheduleMode);
}
