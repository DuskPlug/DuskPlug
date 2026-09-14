#include "tray_menu.h"

bool IsTrayMenuItemChecked(
    TrayPowerModeItem item,
    bool smartActive,
    bool scheduleActive,
    bool timedActive,
    bool lockOffEnabled,
    bool hasKnownState,
    bool knownOn,
    size_t onCount,
    size_t totalDevices) {
    switch (item) {
    case TrayPowerModeItem::Smart:
        return smartActive;
    case TrayPowerModeItem::Schedule:
        return scheduleActive;
    case TrayPowerModeItem::Timed:
        return timedActive;
    case TrayPowerModeItem::LockOff:
        return lockOffEnabled;
    case TrayPowerModeItem::On:
        if (smartActive || scheduleActive || timedActive || !hasKnownState) {
            return false;
        }
        if (totalDevices <= 1) {
            return knownOn;
        }
        return onCount == totalDevices;
    case TrayPowerModeItem::Off:
        if (smartActive || scheduleActive || timedActive || !hasKnownState) {
            return false;
        }
        if (totalDevices <= 1) {
            return !knownOn;
        }
        return onCount == 0;
    }
    return false;
}
