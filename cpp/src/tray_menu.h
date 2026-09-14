#pragma once

#include <cstddef>

enum class TrayPowerModeItem {
    On,
    Off,
    Smart,
    Schedule,
    Timed,
    LockOff,
};

bool IsTrayMenuItemChecked(
    TrayPowerModeItem item,
    bool smartActive,
    bool scheduleActive,
    bool timedActive,
    bool lockOffEnabled,
    bool hasKnownState,
    bool knownOn,
    size_t onCount,
    size_t totalDevices);
