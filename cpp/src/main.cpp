#include "activity_tracker.h"
#include "config.h"
#include "location_cli.h"
#include "location_service.h"
#include "platform_util.h"
#include "platform_win.h"
#include "schedule.h"
#include "settings_dialog.h"
#include "smart_mode.h"
#include "tuya_client.h"
#include "install_kind.h"
#include "update_apply.h"
#include "update_checker.h"
#include "timed_dialog.h"
#include "tray_brightness_win.h"
#include "tray_menu.h"
#include "version.h"

#include <objbase.h>

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wtsapi32.h>

#include <memory>
#include <string>
#include <vector>

#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "comctl32.lib")

namespace {

constexpr UINT WM_TRAYICON = WM_APP + 1;
constexpr UINT WM_SHOW_EXISTING = WM_APP + 3;
constexpr UINT WM_DEFER_STARTUP = WM_APP + 4;
constexpr UINT IDT_POLL = 1001;
constexpr UINT IDT_SMART = 1002;
constexpr UINT IDT_LOCK = 1003;
constexpr UINT IDT_TIMED = 1004;
constexpr UINT ID_MENU_TITLE = 10000;
constexpr UINT CMD_ON = 10001;
constexpr UINT CMD_OFF = 10002;
constexpr UINT CMD_EXIT = 10004;
constexpr UINT CMD_SETTINGS = 10009;
constexpr UINT CMD_LOCK_OFF = 10010;
constexpr UINT CMD_CHECK_UPDATES = 10011;
constexpr UINT CMD_ON_ALL = 10014;
constexpr UINT CMD_OFF_ALL = 10015;
constexpr UINT CMD_SMART_MODE = 10016;
constexpr UINT CMD_SCHEDULE_MODE = 10017;
constexpr UINT CMD_DEVICE_ON_BASE = 11000;
constexpr UINT CMD_DEVICE_OFF_BASE = 11100;
constexpr UINT CMD_DEVICE_MODE_MANUAL_BASE = 11200;
constexpr UINT CMD_DEVICE_MODE_SMART_BASE = 11300;
constexpr UINT CMD_DEVICE_MODE_SCHEDULE_BASE = 11400;
constexpr UINT kMaxDeviceMenuSlots = 50;
constexpr UINT CMD_TIMED_BASE = 10300;
constexpr UINT CMD_TIMED_CUSTOM = 10308;
constexpr int kTimedPresetCount = 8;
constexpr int kTimedPresetMinutes[kTimedPresetCount] = {5, 10, 30, 60, 120, 360, 720, 1440};

struct AppState {
    HWND hwnd = nullptr;
    NOTIFYICONDATAW nid{};
    HMENU menu = nullptr;
    HMENU brightnessSubMenu = nullptr;
    HMENU timedSubMenu = nullptr;
    std::vector<HMENU> deviceSubMenus;
    UINT brightnessMenuIndex = 0;
    UINT timedMenuIndex = 0;
    HICON iconOn = nullptr;
    HICON iconOff = nullptr;
    HICON iconSmartOn = nullptr;
    HICON iconSmartOff = nullptr;
    HBITMAP menuTick = nullptr;
    HICON menuAppIcon = nullptr;
    bool busy = false;
    bool hasKnownState = false;
    bool knownOn = false;
    std::wstring appDir;
    AppConfig config;
    std::unique_ptr<TuyaClient> client;
    SmartModeController smart;
    ILocationService* locationService = nullptr;
    IActivityTracker* activityTracker = nullptr;
    IBrightnessController* brightnessController = nullptr;
    HPOWERNOTIFY suspendNotify = nullptr;
    InstallKind installKind = InstallKind::Portable;
    UpdateInfo pendingUpdate;
};

AppState g_app;
HANDLE g_mutex = nullptr;
UINT g_taskbarCreatedMsg = 0;

// {A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
constexpr GUID kTrayIconGuid = {
    0xa1b2c3d4, 0xe5f6, 0x7890, {0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x90}};

void SetAppUserModelId() {
    using SetAppIdFn = HRESULT(WINAPI*)(PCWSTR);
    HMODULE shell32 = GetModuleHandleW(L"shell32.dll");
    if (!shell32) {
        shell32 = LoadLibraryW(L"shell32.dll");
    }
    if (!shell32) {
        return;
    }
    const auto setAppId = reinterpret_cast<SetAppIdFn>(
        GetProcAddress(shell32, "SetCurrentProcessExplicitAppUserModelID"));
    if (setAppId) {
        setAppId(L"DuskPlug.Tray");
    }
}

bool AddTrayIcon() {
    g_app.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_GUID;
    g_app.nid.guidItem = kTrayIconGuid;
    return Shell_NotifyIconW(NIM_ADD, &g_app.nid) != FALSE;
}

void RefreshTrayIcon() {
    g_app.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_GUID;
    g_app.nid.guidItem = kTrayIconGuid;
    Shell_NotifyIconW(NIM_MODIFY, &g_app.nid);
}

void NotifyAlreadyRunning() {
    HWND existing = FindWindowW(L"DuskPlugWindow", nullptr);
    if (existing) {
        PostMessageW(existing, WM_SHOW_EXISTING, 0, 0);
    }
}

void StartAutomationTimers();
void RunPlugAction(bool toggle, bool setOn, bool statusOnly, int deviceIndex = -1);
void UpdateContextMenuChecks();
void RebuildTrayMenu();
void RunSettings();
void ApplySettingsReload();

void HandleUpdateCheckResult(const UpdateInfo& info, bool showNoUpdateMessage);
void RunUpdateCheck(bool manual);
void RunApplyUpdate(bool confirmPrompt);
void MaybeBackgroundUpdateCheck();

void FinishStartup() {
    g_app.smart.LoadPersistedState();
    RebuildTrayMenu();
    if (g_app.smart.HasAutomatedDevices()) {
        UpdateContextMenuChecks();
        StartAutomationTimers();
        g_app.smart.Evaluate();
    } else {
        if (g_app.smart.IsScreenBrightnessEnabled()) {
            g_app.smart.Evaluate();
        }
        RunPlugAction(false, false, true);
    }
    MaybeBackgroundUpdateCheck();
}

std::wstring JoinPath(const std::wstring& dir, const wchar_t* file) {
    if (dir.empty()) {
        return file;
    }
    if (dir.back() == L'\\' || dir.back() == L'/') {
        return dir + file;
    }
    return dir + L"\\" + file;
}

void ShowSetupBalloon(const wchar_t* text) {
    NOTIFYICONDATAW info = g_app.nid;
    info.uFlags = NIF_INFO | NIF_GUID;
    info.guidItem = kTrayIconGuid;
    wcscpy_s(info.szInfoTitle, L"DuskPlug");
    wcsncpy_s(info.szInfo, text, _TRUNCATE);
    info.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &info);
}

void SetEnabledDevicesAutomationMode(DeviceAutomationMode mode);
void SetTimedModeFromTray(int minutes);

void UpdateTrayDisplay(bool anyOn, size_t onCount, size_t totalCount) {
    const bool automated = g_app.smart.HasAutomatedDevices() || g_app.smart.IsTimedModeActive();
    HICON icon = anyOn ? g_app.iconOn : g_app.iconOff;
    wchar_t tip[128] = L"DuskPlug";

    if (automated) {
        icon = anyOn
            ? (g_app.iconSmartOn ? g_app.iconSmartOn : g_app.iconOn)
            : (g_app.iconSmartOff ? g_app.iconSmartOff : g_app.iconOff);
    }

    if (totalCount <= 1) {
        swprintf_s(
            tip,
            automated
                ? (anyOn ? L"Light: AUTO — ON" : L"Light: AUTO — OFF")
                : (anyOn ? L"Light: ON (click to toggle)" : L"Light: OFF (click to toggle)"));
    } else {
        swprintf_s(tip, L"DuskPlug: %zu of %zu on", onCount, totalCount);
    }

    if (!icon) {
        return;
    }

    if (g_app.nid.hIcon) {
        DestroyIcon(g_app.nid.hIcon);
    }

    g_app.nid.hIcon = CopyIcon(icon);
    wcscpy_s(g_app.nid.szTip, tip);
    g_app.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_GUID;
    g_app.nid.guidItem = kTrayIconGuid;
    Shell_NotifyIconW(NIM_MODIFY, &g_app.nid);

    g_app.hasKnownState = totalCount > 0;
    g_app.knownOn = anyOn;
    UpdateContextMenuChecks();
}

void RestoreTrayState() {
    const size_t totalCount = g_app.smart.GetEnabledDeviceCount();
    if (g_app.hasKnownState) {
        UpdateTrayDisplay(g_app.knownOn, g_app.smart.GetKnownOnCount(), totalCount);
        return;
    }

    wcscpy_s(g_app.nid.szTip, L"DuskPlug: status unknown");
    g_app.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_GUID;
    g_app.nid.guidItem = kTrayIconGuid;
    Shell_NotifyIconW(NIM_MODIFY, &g_app.nid);
}

void StartAutomationTimers() {
    SetTimer(g_app.hwnd, IDT_SMART, 10000, nullptr);
    SetTimer(g_app.hwnd, IDT_LOCK, 1000, nullptr);
}

void RunPlugAction(bool toggle, bool setOn, bool statusOnly, int deviceIndex) {
    if (g_app.busy || !g_app.client) {
        return;
    }

    const auto enabledDevices = GetEnabledDevices(g_app.config);
    if (enabledDevices.empty()) {
        return;
    }

    g_app.busy = true;
    if (!statusOnly) {
        wcscpy_s(g_app.nid.szTip, L"DuskPlug: working...");
        g_app.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_GUID;
        g_app.nid.guidItem = kTrayIconGuid;
        Shell_NotifyIconW(NIM_MODIFY, &g_app.nid);
    }

    std::string error;
    size_t successCount = 0;

    const auto operateDevice = [&](const DeviceConfig& device) {
        if (statusOnly) {
            DeviceState state{};
            if (g_app.client->GetDeviceState(device, state, error)) {
                g_app.smart.SetKnownDeviceState(device.id, state.switchOn);
                ++successCount;
            }
            return;
        }

        if (toggle) {
            bool newState = false;
            if (g_app.smart.ToggleDevice(device, error, newState)) {
                ++successCount;
            }
            return;
        }

        if (g_app.smart.SetDeviceSwitch(device, setOn, error)) {
            ++successCount;
        }
    };

    if (deviceIndex >= 0 && static_cast<size_t>(deviceIndex) < enabledDevices.size()) {
        operateDevice(*enabledDevices[static_cast<size_t>(deviceIndex)]);
    } else {
        for (const auto* device : enabledDevices) {
            operateDevice(*device);
        }
    }

    if (successCount > 0) {
        const size_t onCount = g_app.smart.GetKnownOnCount();
        UpdateTrayDisplay(onCount > 0, onCount, enabledDevices.size());
    } else if (!g_app.smart.HasAutomatedDevices()) {
        RestoreTrayState();
    }

    g_app.busy = false;
}

std::wstring ActiveConfigPath() {
    return ResolveConfigPathWide(JoinPath(g_app.appDir, L"config.json"));
}

void ReloadConfigFromDisk() {
    const std::wstring configPath = ActiveConfigPath();
    std::wstring configError;
    AppConfig config{};
    if (LoadConfig(configPath, config, configError)) {
        g_app.config = config;
        g_app.smart.UpdateConfig(config);
    }
}

void PersistConfigToDisk() {
    SaveAppConfig(ActiveConfigPath(), g_app.config);
}

bool AnyEnabledDeviceUsesMode(DeviceAutomationMode mode) {
    for (const auto& device : g_app.config.devices) {
        if (device.enabled && device.automation.mode == mode) {
            return true;
        }
    }
    return false;
}

int ResolveTimedMenuMinutes() {
    const int activeMinutes = g_app.smart.GetTimedDurationMinutes();
    if (activeMinutes > 0) {
        return activeMinutes;
    }

    for (const auto& device : g_app.config.devices) {
        if (device.enabled && device.automation.mode == DeviceAutomationMode::Timed) {
            return device.automation.timedDurationMinutes;
        }
    }
    return 0;
}

bool IsTimedModeMenuActive() {
    return g_app.smart.IsTimedModeActive() || AnyEnabledDeviceUsesMode(DeviceAutomationMode::Timed);
}

void SetDeviceAutomationMode(size_t deviceIndex, DeviceAutomationMode mode) {
    const auto enabledDevices = GetEnabledDevices(g_app.config);
    if (deviceIndex >= enabledDevices.size()) {
        return;
    }

    const std::string& deviceId = enabledDevices[deviceIndex]->id;
    bool changed = false;
    for (auto& device : g_app.config.devices) {
        if (device.id == deviceId && device.enabled) {
            if (device.automation.mode != mode) {
                device.automation.mode = mode;
                changed = true;
            }
            break;
        }
    }
    if (!changed) {
        return;
    }

    SyncLegacyFieldsFromDevices(g_app.config);
    PersistConfigToDisk();
    g_app.smart.UpdateConfig(g_app.config);

    if (g_app.smart.HasAutomatedDevices()) {
        StartAutomationTimers();
        g_app.smart.Evaluate();
    } else {
        UpdateTrayDisplay(g_app.knownOn, g_app.smart.GetKnownOnCount(), g_app.smart.GetEnabledDeviceCount());
    }
    RebuildTrayMenu();
    UpdateContextMenuChecks();
}

void SetEnabledDevicesAutomationMode(DeviceAutomationMode mode) {
    if (mode != DeviceAutomationMode::Timed) {
        g_app.smart.CancelTimedMode();
        KillTimer(g_app.hwnd, IDT_TIMED);
    }

    bool changed = false;
    for (auto& device : g_app.config.devices) {
        if (!device.enabled) {
            continue;
        }
        if (device.automation.mode != mode) {
            device.automation.mode = mode;
            changed = true;
        }
    }
    if (!changed) {
        return;
    }

    SyncLegacyFieldsFromDevices(g_app.config);
    PersistConfigToDisk();
    g_app.smart.UpdateConfig(g_app.config);

    if (g_app.smart.HasAutomatedDevices()) {
        StartAutomationTimers();
        g_app.smart.Evaluate();
    } else {
        UpdateTrayDisplay(g_app.knownOn, g_app.smart.GetKnownOnCount(), g_app.smart.GetEnabledDeviceCount());
    }
    RebuildTrayMenu();
    UpdateContextMenuChecks();
}

void ToggleSmartModeFromTray() {
    if (AnyEnabledDeviceUsesMode(DeviceAutomationMode::Smart)) {
        SetEnabledDevicesAutomationMode(DeviceAutomationMode::Manual);
        return;
    }
    SetEnabledDevicesAutomationMode(DeviceAutomationMode::Smart);
}

void ToggleScheduleModeFromTray() {
    if (AnyEnabledDeviceUsesMode(DeviceAutomationMode::Schedule)) {
        SetEnabledDevicesAutomationMode(DeviceAutomationMode::Manual);
        return;
    }
    SetEnabledDevicesAutomationMode(DeviceAutomationMode::Schedule);
}

void EnsureManualModeFromTray() {
    if (!g_app.smart.HasAutomatedDevices() && !g_app.smart.IsTimedModeActive()) {
        return;
    }
    SetEnabledDevicesAutomationMode(DeviceAutomationMode::Manual);
}

void SetTimedModeFromTray(int minutes) {
    const int clamped = ClampTimedDurationMinutes(minutes);
    for (auto& device : g_app.config.devices) {
        if (!device.enabled) {
            continue;
        }
        device.automation.mode = DeviceAutomationMode::Timed;
        device.automation.timedDurationMinutes = clamped;
    }

    SyncLegacyFieldsFromDevices(g_app.config);
    PersistConfigToDisk();
    g_app.smart.UpdateConfig(g_app.config);
    g_app.smart.StartTimedMode(clamped);
    SetTimer(g_app.hwnd, IDT_TIMED, 1000, nullptr);
    RebuildTrayMenu();
    UpdateContextMenuChecks();

    wchar_t tip[96];
    swprintf_s(tip, L"Timed mode: on for %d minute(s), then off.", clamped);
    ShowSetupBalloon(tip);
}

void ApplySettingsReload() {
    ReloadConfigFromDisk();
    g_app.client = std::make_unique<TuyaClient>(g_app.config);
    g_app.smart.UpdateConfig(g_app.config);
    g_app.smart.UpdateClient(g_app.client.get());
    if (g_app.smart.HasAutomatedDevices() || g_app.smart.IsScreenBrightnessEnabled()) {
        StartAutomationTimers();
        g_app.smart.Evaluate();
    }
    RebuildTrayMenu();
}

void RunSettings() {
    const std::wstring configPath = ResolveConfigPathWide(JoinPath(g_app.appDir, L"config.json"));
    EnsureConfigFile(configPath);
    if (!ShowSettingsDialog(g_app.hwnd, configPath, g_app.config)) {
        return;
    }

    ApplySettingsReload();
    ShowSetupBalloon(L"Settings saved.");
}

int CurrentManualBrightnessPercent() {
    if (!g_app.brightnessController) {
        return 50;
    }
    return g_app.smart.GetScreenBrightnessPercent();
}

void RebuildTrayMenu() {
    if (g_app.menu) {
        DestroyMenu(g_app.menu);
        g_app.menu = nullptr;
        g_app.brightnessSubMenu = nullptr;
        g_app.timedSubMenu = nullptr;
    }

    g_app.menu = CreatePopupMenu();
    MENUINFO menuInfo{};
    menuInfo.cbSize = sizeof(menuInfo);
    menuInfo.fMask = MIM_STYLE;
    menuInfo.dwStyle = MNS_CHECKORBMP;
    SetMenuInfo(g_app.menu, &menuInfo);

    AppendMenuW(g_app.menu, MF_OWNERDRAW | MF_GRAYED | MF_DISABLED, ID_MENU_TITLE, nullptr);
    AppendMenuW(g_app.menu, MF_SEPARATOR, 0, nullptr);

    const auto enabledDevices = GetEnabledDevices(g_app.config);
    if (enabledDevices.size() <= 1) {
        AppendMenuW(g_app.menu, MF_STRING | MF_UNCHECKED, CMD_ON, L"Turn On");
        AppendMenuW(g_app.menu, MF_STRING | MF_UNCHECKED, CMD_OFF, L"Turn Off");
    } else {
        AppendMenuW(g_app.menu, MF_STRING | MF_UNCHECKED, CMD_ON_ALL, L"Turn all on");
        AppendMenuW(g_app.menu, MF_STRING | MF_UNCHECKED, CMD_OFF_ALL, L"Turn all off");
    }
    AppendMenuW(g_app.menu, MF_STRING | MF_UNCHECKED, CMD_SMART_MODE, L"Smart Mode");

    g_app.deviceSubMenus.clear();
    if (enabledDevices.size() > 1) {
        AppendMenuW(g_app.menu, MF_SEPARATOR, 0, nullptr);
        for (size_t i = 0; i < enabledDevices.size() && i < kMaxDeviceMenuSlots; ++i) {
            const DeviceConfig& device = *enabledDevices[i];
            HMENU deviceMenu = CreatePopupMenu();
            g_app.deviceSubMenus.push_back(deviceMenu);
            AppendMenuW(deviceMenu, MF_STRING | MF_UNCHECKED, CMD_DEVICE_ON_BASE + static_cast<UINT>(i), L"Turn On");
            AppendMenuW(deviceMenu, MF_STRING | MF_UNCHECKED, CMD_DEVICE_OFF_BASE + static_cast<UINT>(i), L"Turn Off");
            AppendMenuW(deviceMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(
                deviceMenu,
                MF_STRING | MF_UNCHECKED,
                CMD_DEVICE_MODE_MANUAL_BASE + static_cast<UINT>(i),
                L"Manual");
            AppendMenuW(
                deviceMenu,
                MF_STRING | MF_UNCHECKED,
                CMD_DEVICE_MODE_SMART_BASE + static_cast<UINT>(i),
                L"Smart");
            AppendMenuW(
                deviceMenu,
                MF_STRING | MF_UNCHECKED,
                CMD_DEVICE_MODE_SCHEDULE_BASE + static_cast<UINT>(i),
                L"Schedule");
            AppendMenuW(
                g_app.menu,
                MF_STRING | MF_POPUP,
                reinterpret_cast<UINT_PTR>(deviceMenu),
                Utf8ToWide(device.name).c_str());
        }
    }

    AppendMenuW(g_app.menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(g_app.menu, MF_STRING | MF_UNCHECKED, CMD_SCHEDULE_MODE, L"Schedule Mode");
    g_app.timedSubMenu = CreatePopupMenu();
    AppendMenuW(g_app.timedSubMenu, MF_STRING | MF_UNCHECKED, CMD_TIMED_BASE + 0, L"5 minutes");
    AppendMenuW(g_app.timedSubMenu, MF_STRING | MF_UNCHECKED, CMD_TIMED_BASE + 1, L"10 minutes");
    AppendMenuW(g_app.timedSubMenu, MF_STRING | MF_UNCHECKED, CMD_TIMED_BASE + 2, L"30 minutes");
    AppendMenuW(g_app.timedSubMenu, MF_STRING | MF_UNCHECKED, CMD_TIMED_BASE + 3, L"1 hour");
    AppendMenuW(g_app.timedSubMenu, MF_STRING | MF_UNCHECKED, CMD_TIMED_BASE + 4, L"2 hours");
    AppendMenuW(g_app.timedSubMenu, MF_STRING | MF_UNCHECKED, CMD_TIMED_BASE + 5, L"6 hours");
    AppendMenuW(g_app.timedSubMenu, MF_STRING | MF_UNCHECKED, CMD_TIMED_BASE + 6, L"12 hours");
    AppendMenuW(g_app.timedSubMenu, MF_STRING | MF_UNCHECKED, CMD_TIMED_BASE + 7, L"24 hours");
    AppendMenuW(g_app.timedSubMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(g_app.timedSubMenu, MF_STRING, CMD_TIMED_CUSTOM, L"Custom...");
    g_app.timedMenuIndex = GetMenuItemCount(g_app.menu);
    AppendMenuW(
        g_app.menu,
        MF_STRING | MF_POPUP,
        reinterpret_cast<UINT_PTR>(g_app.timedSubMenu),
        L"Timed Mode");
    AppendMenuW(g_app.menu, MF_STRING | MF_UNCHECKED, CMD_LOCK_OFF, L"Off when locked or sleeping");
    AppendMenuW(g_app.menu, MF_SEPARATOR, 0, nullptr);
    g_app.brightnessSubMenu = CreatePopupMenu();
    AppendMenuW(
        g_app.brightnessSubMenu,
        MF_OWNERDRAW,
        CMD_SCREEN_BRIGHTNESS_PLACEHOLDER,
        L"");
    g_app.brightnessMenuIndex = GetMenuItemCount(g_app.menu);
    AppendMenuW(
        g_app.menu,
        MF_STRING | MF_POPUP,
        reinterpret_cast<UINT_PTR>(g_app.brightnessSubMenu),
        L"Screen brightness");
    AppendMenuW(g_app.menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(g_app.menu, MF_STRING, CMD_SETTINGS, L"Settings...");
    AppendMenuW(g_app.menu, MF_STRING, CMD_CHECK_UPDATES, L"Check for updates...");
    {
        const std::wstring versionLabel = L"v" + Utf8ToWide(DUSKPLUG_VERSION);
        AppendMenuW(g_app.menu, MF_STRING | MF_GRAYED | MF_DISABLED, 0, versionLabel.c_str());
    }
    AppendMenuW(g_app.menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(g_app.menu, MF_STRING, CMD_EXIT, L"Exit");

    if (g_app.menuTick) {
        SetMenuItemBitmaps(g_app.menu, CMD_ON, MF_BYCOMMAND, nullptr, g_app.menuTick);
        SetMenuItemBitmaps(g_app.menu, CMD_OFF, MF_BYCOMMAND, nullptr, g_app.menuTick);
        SetMenuItemBitmaps(g_app.menu, CMD_ON_ALL, MF_BYCOMMAND, nullptr, g_app.menuTick);
        SetMenuItemBitmaps(g_app.menu, CMD_OFF_ALL, MF_BYCOMMAND, nullptr, g_app.menuTick);
        SetMenuItemBitmaps(g_app.menu, CMD_SMART_MODE, MF_BYCOMMAND, nullptr, g_app.menuTick);
        SetMenuItemBitmaps(g_app.menu, CMD_SCHEDULE_MODE, MF_BYCOMMAND, nullptr, g_app.menuTick);
        SetMenuItemBitmaps(
            g_app.menu,
            static_cast<UINT>(reinterpret_cast<UINT_PTR>(g_app.timedSubMenu)),
            MF_BYCOMMAND,
            nullptr,
            g_app.menuTick);
        SetMenuItemBitmaps(g_app.menu, CMD_LOCK_OFF, MF_BYCOMMAND, nullptr, g_app.menuTick);
        for (int i = 0; i < kTimedPresetCount; ++i) {
            SetMenuItemBitmaps(
                g_app.timedSubMenu,
                CMD_TIMED_BASE + static_cast<UINT>(i),
                MF_BYCOMMAND,
                nullptr,
                g_app.menuTick);
        }
        SetMenuItemBitmaps(
            g_app.timedSubMenu,
            CMD_TIMED_CUSTOM,
            MF_BYCOMMAND,
            nullptr,
            g_app.menuTick);
    }
}

HBITMAP CreateMenuTickBitmap() {
    int cx = GetSystemMetrics(SM_CXMENUCHECK);
    int cy = GetSystemMetrics(SM_CYMENUCHECK);
    if (cx <= 0) {
        cx = 13;
    }
    if (cy <= 0) {
        cy = 13;
    }

    const HDC screen = GetDC(nullptr);
    const HDC dc = CreateCompatibleDC(screen);
    const HBITMAP bmp = CreateCompatibleBitmap(screen, cx, cy);
    const HGDIOBJ oldBmp = SelectObject(dc, bmp);

    RECT rc{0, 0, cx, cy};
    FillRect(dc, &rc, GetSysColorBrush(COLOR_MENU));
    DrawFrameControl(dc, &rc, DFC_MENU, DFCS_MENUCHECK);

    SelectObject(dc, oldBmp);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
    return bmp;
}

void SetMenuItemCheck(HMENU menu, UINT commandId, bool checked) {
    if (!menu) {
        return;
    }

    CheckMenuItem(menu, commandId, MF_BYCOMMAND | (checked ? MF_CHECKED : MF_UNCHECKED));
}

void SetMenuCommandCheck(UINT commandId, bool checked) {
    SetMenuItemCheck(g_app.menu, commandId, checked);
}

void HandleUpdateCheckResult(const UpdateInfo& info, bool showNoUpdateMessage) {
    g_app.pendingUpdate = info;

    if (!info.error.empty()) {
        if (showNoUpdateMessage) {
            MessageBoxW(
                g_app.hwnd,
                Utf8ToWide(info.error).c_str(),
                L"DuskPlug — Updates",
                MB_ICONWARNING | MB_OK);
        }
        return;
    }

    if (info.manifestMissing) {
        if (showNoUpdateMessage) {
            MessageBoxW(
                g_app.hwnd,
                L"No update manifest is published yet for this release.\n\n"
                L"See https://github.com/DuskPlug/DuskPlug/releases for downloads.",
                L"DuskPlug — Updates",
                MB_ICONINFORMATION | MB_OK);
        }
        return;
    }

    if (!info.available) {
        if (showNoUpdateMessage) {
            const std::wstring message = L"You have the latest version (v" + Utf8ToWide(DUSKPLUG_VERSION) + L").";
            MessageBoxW(g_app.hwnd, message.c_str(), L"DuskPlug — Updates", MB_ICONINFORMATION | MB_OK);
        }
        return;
    }

    if (!SupportsInAppUpdate(g_app.installKind)) {
        if (showNoUpdateMessage) {
            std::string message = "DuskPlug " + info.version + " is available.\n\n" + PackageManagerUpdateHint(g_app.installKind);
            MessageBoxW(
                g_app.hwnd,
                Utf8ToWide(message).c_str(),
                L"DuskPlug — Updates",
                MB_ICONINFORMATION | MB_OK);
        }
        return;
    }

    if (showNoUpdateMessage) {
        const std::wstring prompt = L"DuskPlug " + Utf8ToWide(info.version)
            + L" is available.\n\nDownload and install now?";
        if (MessageBoxW(g_app.hwnd, prompt.c_str(), L"DuskPlug — Updates", MB_YESNO | MB_ICONQUESTION)
            == IDYES) {
            RunApplyUpdate(false);
        }
        return;
    }

    const std::wstring balloon = L"DuskPlug " + Utf8ToWide(info.version)
        + L" is available. Right-click and choose Check for updates.";
    ShowSetupBalloon(balloon.c_str());
}

void RunUpdateCheck(bool manual) {
    HandleUpdateCheckResult(CheckForUpdates(g_app.installKind), manual);
}

void RunApplyUpdate(bool confirmPrompt) {
    if (!g_app.pendingUpdate.available || !SupportsInAppUpdate(g_app.installKind)) {
        return;
    }

    if (confirmPrompt) {
        const std::wstring prompt = L"Download and install DuskPlug "
            + Utf8ToWide(g_app.pendingUpdate.version) + L"?";
        if (MessageBoxW(g_app.hwnd, prompt.c_str(), L"DuskPlug — Updates", MB_YESNO | MB_ICONQUESTION)
            != IDYES) {
            return;
        }
    }

    wcscpy_s(g_app.nid.szTip, L"DuskPlug: updating...");
    g_app.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_GUID;
    g_app.nid.guidItem = kTrayIconGuid;
    Shell_NotifyIconW(NIM_MODIFY, &g_app.nid);

    const ApplyUpdateResult result = ApplyUpdate(g_app.pendingUpdate, g_app.installKind);
    if (!result.success) {
        MessageBoxW(
            g_app.hwnd,
            Utf8ToWide(result.error).c_str(),
            L"DuskPlug — Updates",
            MB_ICONERROR | MB_OK);
        RestoreTrayState();
        return;
    }

    if (result.restartScheduled) {
        g_app.smart.SavePersistedState();
        DestroyWindow(g_app.hwnd);
    }
}

void MaybeBackgroundUpdateCheck() {
    if (!ShouldCheckForUpdatesNow()) {
        return;
    }
    HandleUpdateCheckResult(CheckForUpdates(g_app.installKind), false);
}

void UpdateContextMenuChecks() {
    const bool smartActive = AnyEnabledDeviceUsesMode(DeviceAutomationMode::Smart);
    const bool scheduleActive = AnyEnabledDeviceUsesMode(DeviceAutomationMode::Schedule);
    const bool timedActive = IsTimedModeMenuActive();
    const int timedMinutes = ResolveTimedMenuMinutes();
    const auto enabledDevices = GetEnabledDevices(g_app.config);
    const size_t onCount = g_app.smart.GetKnownOnCount();
    const bool lockOffEnabled = g_app.smart.IsLockOffEnabled();

    if (enabledDevices.size() <= 1) {
        SetMenuCommandCheck(
            CMD_ON,
            IsTrayMenuItemChecked(
                TrayPowerModeItem::On,
                smartActive,
                scheduleActive,
                timedActive,
                lockOffEnabled,
                g_app.hasKnownState,
                g_app.knownOn,
                onCount,
                enabledDevices.size()));
        SetMenuCommandCheck(
            CMD_OFF,
            IsTrayMenuItemChecked(
                TrayPowerModeItem::Off,
                smartActive,
                scheduleActive,
                timedActive,
                lockOffEnabled,
                g_app.hasKnownState,
                g_app.knownOn,
                onCount,
                enabledDevices.size()));
    } else {
        SetMenuCommandCheck(
            CMD_ON_ALL,
            IsTrayMenuItemChecked(
                TrayPowerModeItem::On,
                smartActive,
                scheduleActive,
                timedActive,
                lockOffEnabled,
                g_app.hasKnownState,
                g_app.knownOn,
                onCount,
                enabledDevices.size()));
        SetMenuCommandCheck(
            CMD_OFF_ALL,
            IsTrayMenuItemChecked(
                TrayPowerModeItem::Off,
                smartActive,
                scheduleActive,
                timedActive,
                lockOffEnabled,
                g_app.hasKnownState,
                g_app.knownOn,
                onCount,
                enabledDevices.size()));
    }
    SetMenuCommandCheck(
        CMD_SMART_MODE,
        IsTrayMenuItemChecked(
            TrayPowerModeItem::Smart,
            smartActive,
            scheduleActive,
            timedActive,
            lockOffEnabled,
            g_app.hasKnownState,
            g_app.knownOn,
            onCount,
            enabledDevices.size()));
    SetMenuCommandCheck(
        CMD_SCHEDULE_MODE,
        IsTrayMenuItemChecked(
            TrayPowerModeItem::Schedule,
            smartActive,
            scheduleActive,
            timedActive,
            lockOffEnabled,
            g_app.hasKnownState,
            g_app.knownOn,
            onCount,
            enabledDevices.size()));
    if (g_app.timedSubMenu) {
        bool presetMatched = false;
        for (int i = 0; i < kTimedPresetCount; ++i) {
            const bool checked = timedActive && timedMinutes == kTimedPresetMinutes[i];
            SetMenuItemCheck(
                g_app.timedSubMenu,
                CMD_TIMED_BASE + static_cast<UINT>(i),
                checked);
            if (checked) {
                presetMatched = true;
            }
        }
        SetMenuItemCheck(
            g_app.timedSubMenu,
            CMD_TIMED_CUSTOM,
            timedActive && !presetMatched);
        SetMenuItemCheck(
            g_app.menu,
            static_cast<UINT>(reinterpret_cast<UINT_PTR>(g_app.timedSubMenu)),
            IsTrayMenuItemChecked(
                TrayPowerModeItem::Timed,
                smartActive,
                scheduleActive,
                timedActive,
                lockOffEnabled,
                g_app.hasKnownState,
                g_app.knownOn,
                onCount,
                enabledDevices.size()));
    }
    SetMenuCommandCheck(
        CMD_LOCK_OFF,
        IsTrayMenuItemChecked(
            TrayPowerModeItem::LockOff,
            smartActive,
            scheduleActive,
            timedActive,
            lockOffEnabled,
            g_app.hasKnownState,
            g_app.knownOn,
            onCount,
            enabledDevices.size()));
    EnableMenuItem(g_app.menu, CMD_LOCK_OFF, (smartActive || scheduleActive) ? MF_ENABLED : MF_GRAYED);
    const bool brightnessAvailable = g_app.brightnessController
        && g_app.brightnessController->AnyControllable();
    EnableMenuItem(
        g_app.menu,
        g_app.brightnessMenuIndex,
        MF_BYPOSITION | (brightnessAvailable ? MF_ENABLED : MF_GRAYED));
    SyncBrightnessPanelAutoState();

    for (size_t i = 0; i < g_app.deviceSubMenus.size(); ++i) {
        HMENU deviceMenu = g_app.deviceSubMenus[i];
        if (!deviceMenu || i >= enabledDevices.size()) {
            continue;
        }

        const DeviceConfig& device = *enabledDevices[i];
        const bool deviceKnownOn = g_app.smart.GetDeviceKnownOn(device.id);
        const bool deviceHasState = g_app.smart.HasDeviceKnownState(device.id);
        const DeviceAutomationMode mode = device.automation.mode;

        SetMenuItemCheck(
            deviceMenu,
            CMD_DEVICE_ON_BASE + static_cast<UINT>(i),
            deviceHasState && deviceKnownOn && mode == DeviceAutomationMode::Manual);
        SetMenuItemCheck(
            deviceMenu,
            CMD_DEVICE_OFF_BASE + static_cast<UINT>(i),
            deviceHasState && !deviceKnownOn && mode == DeviceAutomationMode::Manual);
        SetMenuItemCheck(
            deviceMenu,
            CMD_DEVICE_MODE_MANUAL_BASE + static_cast<UINT>(i),
            mode == DeviceAutomationMode::Manual);
        SetMenuItemCheck(
            deviceMenu,
            CMD_DEVICE_MODE_SMART_BASE + static_cast<UINT>(i),
            mode == DeviceAutomationMode::Smart);
        SetMenuItemCheck(
            deviceMenu,
            CMD_DEVICE_MODE_SCHEDULE_BASE + static_cast<UINT>(i),
            mode == DeviceAutomationMode::Schedule);
    }
}

void ShowContextMenu() {
    RunPlugAction(false, false, true);
    UpdateContextMenuChecks();
    POINT pt{};
    GetCursorPos(&pt);
    SetForegroundWindow(g_app.hwnd);
    OnTrayContextMenuOpening();
    TrackPopupMenu(
        g_app.menu,
        TPM_RIGHTBUTTON | TPM_RIGHTALIGN | TPM_BOTTOMALIGN,
        pt.x,
        pt.y,
        0,
        g_app.hwnd,
        nullptr);
    PostMessageW(g_app.hwnd, WM_NULL, 0, 0);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    LRESULT brightnessResult = 0;
    if (HandleTrayBrightnessMessage(hwnd, msg, wParam, lParam, brightnessResult)) {
        return brightnessResult;
    }

    if (msg == g_taskbarCreatedMsg && g_taskbarCreatedMsg != 0) {
        AddTrayIcon();
        RestoreTrayState();
        return 0;
    }

    switch (msg) {
    case WM_DEFER_STARTUP:
        FinishStartup();
        return 0;

    case WM_SHOW_EXISTING:
        RefreshTrayIcon();
        ShowSetupBalloon(L"DuskPlug is already running in the system tray.");
        return 0;

    case WM_TIMER:
        if (wParam == IDT_POLL) {
            if (g_app.smart.IsScreenBrightnessEnabled()) {
                g_app.smart.Evaluate();
            }
            if (!g_app.smart.HasAutomatedDevices()) {
                RunPlugAction(false, false, true);
            }
        } else if (wParam == IDT_SMART) {
            g_app.smart.Evaluate();
        } else if (wParam == IDT_LOCK) {
            g_app.smart.OnLockTimerTick();
        } else if (wParam == IDT_TIMED) {
            g_app.smart.OnTimedModeTick();
        }
        return 0;

    case WM_WTSSESSION_CHANGE:
        if (wParam == WTS_SESSION_UNLOCK) {
            g_app.smart.OnSessionChangeEvent(true);
        } else if (wParam == WTS_SESSION_LOCK) {
            g_app.smart.OnSessionChangeEvent(false);
        }
        return 0;

    case WM_POWERBROADCAST:
        if (wParam == PBT_APMSUSPEND) {
            g_app.smart.OnPowerSuspend();
            return TRUE;
        }
        if (wParam == PBT_APMRESUMEAUTOMATIC
            || wParam == PBT_APMRESUMESUSPEND
            || wParam == PBT_APMRESUMECRITICAL) {
            g_app.smart.OnPowerResume();
            return TRUE;
        }
        break;

    case WM_QUERYENDSESSION:
        g_app.smart.OnPowerSuspend();
        return TRUE;

    case WM_TRAYICON:
        if (LOWORD(lParam) == WM_LBUTTONUP) {
            EnsureManualModeFromTray();
            RunPlugAction(true, false, false);
        } else if (LOWORD(lParam) == WM_RBUTTONUP) {
            ShowContextMenu();
        }
        return 0;

    case WM_MEASUREITEM: {
        auto* measure = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        if (!measure || measure->CtlType != ODT_MENU) {
            break;
        }
        if (measure->itemID == ID_MENU_TITLE) {
            measure->itemWidth = 220;
            measure->itemHeight = 36;
            return TRUE;
        }
        if (measure->itemID == CMD_SCREEN_BRIGHTNESS_PLACEHOLDER) {
            MeasureBrightnessPlaceholderItem(measure);
            return TRUE;
        }
        break;
    }

    case WM_DRAWITEM: {
        auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (!draw || draw->CtlType != ODT_MENU) {
            break;
        }
        if (draw->itemID == ID_MENU_TITLE) {
            const RECT& rc = draw->rcItem;
            FillRect(draw->hDC, &rc, reinterpret_cast<HBRUSH>(COLOR_MENU + 1));

            const int iconSize = 16;
            const int iconY = rc.top + ((rc.bottom - rc.top - iconSize) / 2);
            if (g_app.menuAppIcon) {
                DrawIconEx(draw->hDC, rc.left + 12, iconY, g_app.menuAppIcon, iconSize, iconSize, 0, nullptr, DI_NORMAL);
            }

            SetBkMode(draw->hDC, TRANSPARENT);
            SetTextColor(draw->hDC, GetSysColor(COLOR_MENUTEXT));
            RECT textRect = rc;
            textRect.left += 12 + iconSize + 8;
            DrawTextW(draw->hDC, L"DuskPlug", -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        if (draw->itemID == CMD_SCREEN_BRIGHTNESS_PLACEHOLDER) {
            DrawBrightnessPlaceholderItem(draw);
            return TRUE;
        }
        break;
    }

    case WM_INITMENUPOPUP: {
        const HMENU menu = reinterpret_cast<HMENU>(wParam);
        if (menu == g_app.brightnessSubMenu
            && g_app.brightnessController
            && g_app.brightnessController->AnyControllable()) {
            RequestShowBrightnessPanel(g_app.hwnd, g_app.brightnessSubMenu);
        } else if (menu != g_app.menu && menu != g_app.brightnessSubMenu) {
            HideBrightnessPanel();
        }
        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case CMD_ON:
            EnsureManualModeFromTray();
            RunPlugAction(false, true, false);
            break;
        case CMD_OFF:
            EnsureManualModeFromTray();
            RunPlugAction(false, false, false);
            break;
        case CMD_ON_ALL:
            EnsureManualModeFromTray();
            RunPlugAction(false, true, false);
            break;
        case CMD_OFF_ALL:
            EnsureManualModeFromTray();
            RunPlugAction(false, false, false);
            break;
        case CMD_SMART_MODE:
            ToggleSmartModeFromTray();
            break;
        case CMD_SCHEDULE_MODE:
            ToggleScheduleModeFromTray();
            break;
        case CMD_TIMED_CUSTOM: {
            int minutes = g_app.smart.GetTimedDurationMinutes();
            if (minutes <= 0) {
                minutes = 30;
            }
            if (PromptTimedMinutes(g_app.hwnd, minutes)) {
                SetTimedModeFromTray(minutes);
            }
            break;
        }
        case CMD_LOCK_OFF:
            g_app.smart.ToggleLockOffEnabled();
            UpdateContextMenuChecks();
            break;
        case CMD_SETTINGS:
            RunSettings();
            break;
        case CMD_CHECK_UPDATES:
            RunUpdateCheck(true);
            break;
        case CMD_EXIT:
            g_app.smart.SavePersistedState();
            DestroyWindow(hwnd);
            break;
        default:
            if (LOWORD(wParam) >= CMD_TIMED_BASE
                && LOWORD(wParam) < CMD_TIMED_BASE + static_cast<UINT>(kTimedPresetCount)) {
                const int index = static_cast<int>(LOWORD(wParam) - CMD_TIMED_BASE);
                SetTimedModeFromTray(kTimedPresetMinutes[index]);
            } else if (LOWORD(wParam) >= CMD_DEVICE_ON_BASE
                && LOWORD(wParam) < CMD_DEVICE_ON_BASE + kMaxDeviceMenuSlots) {
                const int deviceIndex = static_cast<int>(LOWORD(wParam) - CMD_DEVICE_ON_BASE);
                SetDeviceAutomationMode(static_cast<size_t>(deviceIndex), DeviceAutomationMode::Manual);
                RunPlugAction(false, true, false, deviceIndex);
            } else if (LOWORD(wParam) >= CMD_DEVICE_OFF_BASE
                && LOWORD(wParam) < CMD_DEVICE_OFF_BASE + kMaxDeviceMenuSlots) {
                const int deviceIndex = static_cast<int>(LOWORD(wParam) - CMD_DEVICE_OFF_BASE);
                SetDeviceAutomationMode(static_cast<size_t>(deviceIndex), DeviceAutomationMode::Manual);
                RunPlugAction(false, false, false, deviceIndex);
            } else if (LOWORD(wParam) >= CMD_DEVICE_MODE_MANUAL_BASE
                && LOWORD(wParam) < CMD_DEVICE_MODE_MANUAL_BASE + kMaxDeviceMenuSlots) {
                SetDeviceAutomationMode(
                    static_cast<size_t>(LOWORD(wParam) - CMD_DEVICE_MODE_MANUAL_BASE),
                    DeviceAutomationMode::Manual);
            } else if (LOWORD(wParam) >= CMD_DEVICE_MODE_SMART_BASE
                && LOWORD(wParam) < CMD_DEVICE_MODE_SMART_BASE + kMaxDeviceMenuSlots) {
                SetDeviceAutomationMode(
                    static_cast<size_t>(LOWORD(wParam) - CMD_DEVICE_MODE_SMART_BASE),
                    DeviceAutomationMode::Smart);
            } else if (LOWORD(wParam) >= CMD_DEVICE_MODE_SCHEDULE_BASE
                && LOWORD(wParam) < CMD_DEVICE_MODE_SCHEDULE_BASE + kMaxDeviceMenuSlots) {
                SetDeviceAutomationMode(
                    static_cast<size_t>(LOWORD(wParam) - CMD_DEVICE_MODE_SCHEDULE_BASE),
                    DeviceAutomationMode::Schedule);
            }
            break;
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, IDT_POLL);
        KillTimer(hwnd, IDT_SMART);
        KillTimer(hwnd, IDT_LOCK);
        KillTimer(hwnd, IDT_TIMED);
        if (g_app.suspendNotify) {
            UnregisterSuspendResumeNotification(g_app.suspendNotify);
            g_app.suspendNotify = nullptr;
        }
        g_app.smart.Shutdown();
        delete g_app.locationService;
        g_app.locationService = nullptr;
        delete g_app.activityTracker;
        g_app.activityTracker = nullptr;
        delete g_app.brightnessController;
        g_app.brightnessController = nullptr;
        Shell_NotifyIconW(NIM_DELETE, &g_app.nid);
        if (g_app.nid.hIcon) {
            DestroyIcon(g_app.nid.hIcon);
        }
        if (g_app.iconOn) {
            DestroyIcon(g_app.iconOn);
        }
        if (g_app.iconOff) {
            DestroyIcon(g_app.iconOff);
        }
        if (g_app.iconSmartOn) {
            DestroyIcon(g_app.iconSmartOn);
        }
        if (g_app.iconSmartOff) {
            DestroyIcon(g_app.iconSmartOff);
        }
        if (g_app.menuAppIcon) {
            DestroyIcon(g_app.menuAppIcon);
        }
        if (g_app.menu) {
            DestroyMenu(g_app.menu);
            g_app.menu = nullptr;
        }
        if (g_app.menuTick) {
            DeleteObject(g_app.menuTick);
            g_app.menuTick = nullptr;
        }
        ShutdownTrayBrightnessUi();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    bool openSettingsOnStart = false;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            if (_wcsicmp(argv[i], L"--self-test") == 0) {
                LocalFree(argv);
                return 0;
            }
            if (_wcsicmp(argv[i], L"--get-location") == 0) {
                const int rc = RunGetLocationMode();
                LocalFree(argv);
                return rc;
            }
            if (_wcsicmp(argv[i], L"--settings") == 0) {
                openSettingsOnStart = true;
            }
        }
        LocalFree(argv);
    }

    SetAppUserModelId();

    const HRESULT comHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool ownsCom = comHr == S_OK;

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    g_mutex = CreateMutexW(nullptr, FALSE, L"Global\\DuskPlug-SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        NotifyAlreadyRunning();
        return 0;
    }

    g_taskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");

    g_app.appDir = Utf8ToWide(GetExeDirectory());
    g_app.installKind = DetectInstallKind();
    const std::wstring configPath = ResolveConfigPathWide(JoinPath(g_app.appDir, L"config.json"));
    EnsureConfigFile(configPath);
    std::wstring configError;
    if (!LoadConfig(configPath, g_app.config, configError, false)) {
        MessageBoxW(
            nullptr,
            configError.c_str(),
            L"DuskPlug",
            MB_ICONERROR | MB_OK);
        return 1;
    }

    bool resetTimedMode = false;
    for (auto& device : g_app.config.devices) {
        if (device.automation.mode == DeviceAutomationMode::Timed) {
            device.automation.mode = DeviceAutomationMode::Manual;
            resetTimedMode = true;
        }
    }
    if (resetTimedMode) {
        SyncLegacyFieldsFromDevices(g_app.config);
        SaveAppConfig(configPath, g_app.config);
    }

    const std::wstring iconOnPath = JoinPath(g_app.appDir, L"assets\\light-on.ico");
    const std::wstring iconOffPath = JoinPath(g_app.appDir, L"assets\\light-off.ico");
    const std::wstring iconSmartOnPath = JoinPath(g_app.appDir, L"assets\\light-smart-on.ico");
    const std::wstring iconSmartOffPath = JoinPath(g_app.appDir, L"assets\\light-smart-off.ico");
    g_app.iconOn = static_cast<HICON>(LoadImageW(nullptr, iconOnPath.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE));
    g_app.iconOff = static_cast<HICON>(LoadImageW(nullptr, iconOffPath.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE));
    g_app.iconSmartOn = static_cast<HICON>(LoadImageW(nullptr, iconSmartOnPath.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE));
    g_app.iconSmartOff = static_cast<HICON>(LoadImageW(nullptr, iconSmartOffPath.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE));
    if (!g_app.iconOn || !g_app.iconOff) {
        MessageBoxW(nullptr, L"Missing assets\\light-on.ico or assets\\light-off.ico next to DuskPlug.exe", L"DuskPlug", MB_ICONERROR | MB_OK);
        return 1;
    }
    if (!g_app.iconSmartOn) {
        g_app.iconSmartOn = CopyIcon(g_app.iconOn);
    }
    if (!g_app.iconSmartOff) {
        g_app.iconSmartOff = CopyIcon(g_app.iconOff);
    }

    const std::wstring appIconPath = JoinPath(g_app.appDir, L"assets\\app.ico");
    g_app.menuAppIcon = static_cast<HICON>(LoadImageW(
        nullptr,
        appIconPath.c_str(),
        IMAGE_ICON,
        16,
        16,
        LR_LOADFROMFILE));
    if (!g_app.menuAppIcon) {
        g_app.menuAppIcon = CopyIcon(g_app.iconOff);
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"DuskPlugWindow";
    if (!RegisterClassExW(&wc)) {
        MessageBoxW(nullptr, L"RegisterClassEx failed", L"DuskPlug", MB_ICONERROR | MB_OK);
        return 1;
    }

    g_app.hwnd = CreateWindowExW(
        0,
        L"DuskPlugWindow",
        L"DuskPlug",
        WS_OVERLAPPEDWINDOW,
        0, 0, 0, 0,
        nullptr,
        nullptr,
        instance,
        nullptr);
    if (!g_app.hwnd) {
        MessageBoxW(nullptr, L"CreateWindowEx failed", L"DuskPlug", MB_ICONERROR | MB_OK);
        return 1;
    }

    ShowWindow(g_app.hwnd, SW_HIDE);

    g_app.suspendNotify = RegisterSuspendResumeNotification(g_app.hwnd, DEVICE_NOTIFY_WINDOW_HANDLE);

    ZeroMemory(&g_app.nid, sizeof(g_app.nid));
    g_app.nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_app.nid.hWnd = g_app.hwnd;
    g_app.nid.uID = 1;
    g_app.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_GUID;
    g_app.nid.guidItem = kTrayIconGuid;
    g_app.nid.uCallbackMessage = WM_TRAYICON;
    g_app.nid.hIcon = CopyIcon(g_app.iconOff);
    g_app.nid.guidItem = kTrayIconGuid;
    wcscpy_s(g_app.nid.szTip, L"Plug: starting...");
    if (!AddTrayIcon()) {
        MessageBoxW(nullptr, L"Could not create tray icon", L"DuskPlug", MB_ICONERROR | MB_OK);
        return 1;
    }

    g_app.menuTick = CreateMenuTickBitmap();
    RebuildTrayMenu();

    TrayBrightnessCallbacks brightnessCallbacks{};
    brightnessCallbacks.isAvailable = []() {
        return g_app.brightnessController && g_app.brightnessController->AnyControllable();
    };
    brightnessCallbacks.getPercent = []() { return CurrentManualBrightnessPercent(); };
    brightnessCallbacks.setPercent = [](int percent) {
        g_app.smart.SetScreenBrightnessPercent(percent);
    };
    brightnessCallbacks.isAutoEnabled = []() {
        return g_app.smart.IsScreenBrightnessEnabled();
    };
    brightnessCallbacks.setAutoEnabled = [](bool enabled) {
        if (enabled == g_app.smart.IsScreenBrightnessEnabled()) {
            return;
        }
        g_app.smart.SetScreenBrightnessEnabled(enabled);
        UpdateContextMenuChecks();
        if (enabled) {
            ShowSetupBalloon(L"Automatic screen brightness is now on.");
        }
    };
    if (!IsConfigComplete(g_app.config)) {
        if (openSettingsOnStart) {
            RunSettings();
        } else {
            ShowSetupBalloon(
                L"DuskPlug needs your plug connection details.\n"
                L"Right-click the tray icon and choose Settings...");
        }
    }

    g_app.locationService = CreateWinLocationService(g_app.hwnd, g_app.appDir);
    g_app.activityTracker = CreateWinActivityTracker(g_app.hwnd);
    g_app.brightnessController = CreateWinBrightnessController();
    InitTrayBrightnessUi(g_app.hwnd, brightnessCallbacks);

    if (IsConfigComplete(g_app.config)) {
        g_app.client = std::make_unique<TuyaClient>(g_app.config);

        SmartModeCallbacks callbacks{};
        callbacks.updateTray = [](bool anyOn, size_t onCount, size_t totalCount) {
            UpdateTrayDisplay(anyOn, onCount, totalCount);
        };
        callbacks.showSetupMessage = [](const std::string& text) {
            ShowSetupBalloon(Utf8ToWide(text).c_str());
        };
        callbacks.isBusy = []() { return g_app.busy; };
        callbacks.onTimedModeExpired = []() {
            KillTimer(g_app.hwnd, IDT_TIMED);
            SetEnabledDevicesAutomationMode(DeviceAutomationMode::Manual);
            RebuildTrayMenu();
            UpdateContextMenuChecks();
            ShowSetupBalloon(L"Timed mode finished — devices turned off.");
        };
        g_app.smart.Initialize(
            g_app.config,
            g_app.client.get(),
            g_app.locationService,
            g_app.activityTracker,
            g_app.brightnessController,
            callbacks);
    }

    SetTimer(g_app.hwnd, IDT_POLL, 30000, nullptr);

    PostMessageW(g_app.hwnd, WM_DEFER_STARTUP, 0, 0);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (g_mutex) {
        CloseHandle(g_mutex);
    }

    if (ownsCom) {
        CoUninitialize();
    }

    return static_cast<int>(message.wParam);
}
