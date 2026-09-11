#include "activity_win.h"
#include "config.h"
#include "location_cli.h"
#include "location_win.h"
#include "schedule.h"
#include "settings_dialog.h"
#include "smart_mode.h"
#include "tuya_client.h"

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

#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "comctl32.lib")

namespace {

constexpr UINT WM_TRAYICON = WM_APP + 1;
constexpr UINT WM_SHOW_EXISTING = WM_APP + 3;
constexpr UINT WM_DEFER_STARTUP = WM_APP + 4;
constexpr UINT IDT_POLL = 1001;
constexpr UINT IDT_SMART = 1002;
constexpr UINT IDT_LOCK = 1003;
constexpr UINT CMD_ON = 10001;
constexpr UINT CMD_OFF = 10002;
constexpr UINT CMD_REFRESH = 10003;
constexpr UINT CMD_EXIT = 10004;
constexpr UINT CMD_RESTART = 10005;
constexpr UINT CMD_SMART = 10006;
constexpr UINT CMD_SET_LOCATION = 10007;
constexpr UINT CMD_SCHEDULE = 10008;
constexpr UINT CMD_SETTINGS = 10009;

struct AppState {
    HWND hwnd = nullptr;
    NOTIFYICONDATAW nid{};
    HMENU menu = nullptr;
    HICON iconOn = nullptr;
    HICON iconOff = nullptr;
    HICON iconSmartOn = nullptr;
    HICON iconSmartOff = nullptr;
    HBITMAP menuTick = nullptr;
    bool busy = false;
    bool hasKnownState = false;
    bool knownOn = false;
    std::wstring appDir;
    AppConfig config;
    std::unique_ptr<TuyaClient> client;
    SmartModeController smart;
    HPOWERNOTIFY suspendNotify = nullptr;
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
void RunPlugAction(bool toggle, bool setOn, bool statusOnly);
void UpdateContextMenuChecks();
void ToggleScheduleMode();
void RunSettings();
void ApplySettingsReload();

void FinishStartup() {
    g_app.smart.LoadPersistedState();
    if (g_app.smart.IsAutomationEnabled()) {
        UpdateContextMenuChecks();
        StartAutomationTimers();
        g_app.smart.Evaluate();
    } else {
        RunPlugAction(false, false, true);
    }
}

std::wstring GetExeDirectory() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring full(path);
    const size_t pos = full.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"." : full.substr(0, pos);
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

void UpdateTrayDisplay(bool on) {
    HICON icon = g_app.iconOff;
    const wchar_t* tip = L"Plug: OFF (click to toggle)";

    if (g_app.smart.IsScheduleEnabled()) {
        if (on) {
            icon = g_app.iconSmartOn ? g_app.iconSmartOn : g_app.iconOn;
        } else {
            icon = g_app.iconSmartOff ? g_app.iconSmartOff : g_app.iconOff;
        }
        tip = on ? L"Plug: SCHEDULE — ON" : L"Plug: SCHEDULE — OFF";
    } else if (g_app.smart.IsEnabled()) {
        if (on) {
            icon = g_app.iconSmartOn ? g_app.iconSmartOn : g_app.iconOn;
        } else {
            icon = g_app.iconSmartOff ? g_app.iconSmartOff : g_app.iconOff;
        }
        tip = on ? L"Plug: SMART — ON" : L"Plug: SMART — OFF";
    } else {
        icon = on ? g_app.iconOn : g_app.iconOff;
        tip = on ? L"Plug: ON (click to toggle)" : L"Plug: OFF (click to toggle)";
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

    g_app.hasKnownState = true;
    g_app.knownOn = on;
    g_app.smart.SetKnownPlugState(on);
    UpdateContextMenuChecks();
}

void RestoreTrayState() {
    if (g_app.hasKnownState) {
        UpdateTrayDisplay(g_app.knownOn);
        return;
    }

    wcscpy_s(g_app.nid.szTip, L"Plug: status unknown");
    g_app.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_GUID;
    g_app.nid.guidItem = kTrayIconGuid;
    Shell_NotifyIconW(NIM_MODIFY, &g_app.nid);
}

void ExitAutomationMode() {
    if (!g_app.smart.IsAutomationEnabled()) {
        return;
    }

    KillTimer(g_app.hwnd, IDT_SMART);
    KillTimer(g_app.hwnd, IDT_LOCK);
    g_app.smart.Disable(g_app.hwnd);
    UpdateContextMenuChecks();
}

void StartAutomationTimers() {
    SetTimer(g_app.hwnd, IDT_SMART, 10000, nullptr);
    SetTimer(g_app.hwnd, IDT_LOCK, 1000, nullptr);
}

void RunPlugAction(bool toggle, bool setOn, bool statusOnly) {
    if (g_app.busy || !g_app.client) {
        return;
    }

    if (!statusOnly) {
        ExitAutomationMode();
    }

    g_app.busy = true;
    if (!statusOnly) {
        wcscpy_s(g_app.nid.szTip, L"Plug: working...");
        g_app.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_GUID;
        g_app.nid.guidItem = kTrayIconGuid;
        Shell_NotifyIconW(NIM_MODIFY, &g_app.nid);
    }

    std::string error;
    bool on = false;
    bool ok = false;

    if (statusOnly) {
        ok = g_app.client->GetSwitchState(on, error);
    } else if (toggle) {
        ok = g_app.client->Toggle(error, on);
    } else {
        ok = g_app.client->SetSwitch(setOn, error);
        on = setOn;
    }

    if (ok) {
        UpdateTrayDisplay(on);
    } else if (!g_app.smart.IsAutomationEnabled()) {
        RestoreTrayState();
    }

    g_app.busy = false;
}

void ReloadConfigFromDisk() {
    const std::wstring configPath = ResolveConfigPath(JoinPath(g_app.appDir, L"config.json"));
    std::wstring configError;
    AppConfig config{};
    if (LoadConfig(configPath, config, configError)) {
        g_app.config = config;
        g_app.smart.UpdateConfig(config);
    }
}

void ApplySettingsReload() {
    ReloadConfigFromDisk();
    g_app.client = std::make_unique<TuyaClient>(g_app.config);
    g_app.smart.UpdateConfig(g_app.config);
    g_app.smart.UpdateClient(g_app.client.get());
    if (g_app.smart.IsAutomationEnabled()) {
        g_app.smart.Evaluate();
    }
}

void RunSettings() {
    const std::wstring configPath = ResolveConfigPath(JoinPath(g_app.appDir, L"config.json"));
    EnsureConfigFile(configPath);
    if (!ShowSettingsDialog(g_app.hwnd, configPath, g_app.config)) {
        return;
    }

    ApplySettingsReload();
    ShowSetupBalloon(L"Settings saved.");
}

void ToggleSmartMode() {
    if (g_app.smart.IsEnabled()) {
        ExitAutomationMode();
        RestoreTrayState();
        return;
    }

    std::wstring error;
    if (!g_app.smart.Enable(error, true)) {
        if (!error.empty()) {
            ShowSetupBalloon(error.c_str());
        }
        return;
    }

    UpdateContextMenuChecks();
    StartAutomationTimers();
    g_app.smart.Evaluate();
}

bool HasValidSchedule(const AppConfig& config) {
    int onMinutes = 0;
    int offMinutes = 0;
    if (!ParseTimeHHMM(config.scheduleOnTime, onMinutes) || !ParseTimeHHMM(config.scheduleOffTime, offMinutes)) {
        return false;
    }
    return onMinutes != offMinutes;
}

void ToggleScheduleMode() {
    if (g_app.smart.IsScheduleEnabled()) {
        ExitAutomationMode();
        RestoreTrayState();
        return;
    }

    if (!HasValidSchedule(g_app.config)) {
        MessageBoxW(
            g_app.hwnd,
            L"Set a daily ON and OFF time in Settings first.",
            L"DuskPlug — Schedule Mode",
            MB_ICONINFORMATION | MB_OK);
        RunSettings();
        if (!HasValidSchedule(g_app.config)) {
            return;
        }
    }

    std::wstring error;
    if (!g_app.smart.EnableSchedule(error)) {
        if (!error.empty()) {
            ShowSetupBalloon(error.c_str());
        }
        return;
    }

    UpdateContextMenuChecks();
    StartAutomationTimers();
    g_app.smart.Evaluate();
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

void SetMenuCommandCheck(UINT commandId, bool checked) {
    if (!g_app.menu) {
        return;
    }

    MENUITEMINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = MIIM_STATE;
    if (!GetMenuItemInfoW(g_app.menu, commandId, FALSE, &info)) {
        return;
    }

    if (checked) {
        info.fState |= MFS_CHECKED;
    } else {
        info.fState &= ~MFS_CHECKED;
    }

    SetMenuItemInfoW(g_app.menu, commandId, FALSE, &info);
}

void UpdateContextMenuChecks() {
    const bool automation = g_app.smart.IsAutomationEnabled();
    const bool smart = g_app.smart.IsEnabled();
    const bool schedule = g_app.smart.IsScheduleEnabled();
    SetMenuCommandCheck(CMD_SMART, smart);
    SetMenuCommandCheck(CMD_SCHEDULE, schedule);
    SetMenuCommandCheck(CMD_ON, !automation && g_app.hasKnownState && g_app.knownOn);
    SetMenuCommandCheck(CMD_OFF, !automation && g_app.hasKnownState && !g_app.knownOn);
}

void ShowContextMenu() {
    UpdateContextMenuChecks();
    POINT pt{};
    GetCursorPos(&pt);
    SetForegroundWindow(g_app.hwnd);
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

void RestartApp(HWND hwnd) {
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    g_app.smart.SavePersistedState();

    if (g_mutex) {
        CloseHandle(g_mutex);
        g_mutex = nullptr;
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(
            exePath,
            nullptr,
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            g_app.appDir.c_str(),
            &si,
            &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    DestroyWindow(hwnd);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
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
            if (!g_app.smart.IsAutomationEnabled()) {
                RunPlugAction(false, false, true);
            }
        } else if (wParam == IDT_SMART) {
            g_app.smart.Evaluate();
        } else if (wParam == IDT_LOCK) {
            g_app.smart.OnLockTimerTick();
        }
        return 0;

    case WM_WTSSESSION_CHANGE:
        g_app.smart.OnSessionChange(wParam);
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
            RunPlugAction(true, false, false);
        } else if (LOWORD(lParam) == WM_RBUTTONUP) {
            ShowContextMenu();
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case CMD_ON:
            RunPlugAction(false, true, false);
            break;
        case CMD_OFF:
            RunPlugAction(false, false, false);
            break;
        case CMD_SMART:
            ToggleSmartMode();
            break;
        case CMD_SCHEDULE:
            ToggleScheduleMode();
            break;
        case CMD_SETTINGS:
            RunSettings();
            break;
        case CMD_REFRESH:
            RunPlugAction(false, false, true);
            break;
        case CMD_RESTART:
            RestartApp(hwnd);
            break;
        case CMD_EXIT:
            g_app.smart.SavePersistedState();
            DestroyWindow(hwnd);
            break;
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, IDT_POLL);
        KillTimer(hwnd, IDT_SMART);
        KillTimer(hwnd, IDT_LOCK);
        if (g_app.suspendNotify) {
            UnregisterSuspendResumeNotification(g_app.suspendNotify);
            g_app.suspendNotify = nullptr;
        }
        g_app.smart.Shutdown(hwnd);
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
        if (g_app.menu) {
            DestroyMenu(g_app.menu);
            g_app.menu = nullptr;
        }
        if (g_app.menuTick) {
            DeleteObject(g_app.menuTick);
            g_app.menuTick = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            if (_wcsicmp(argv[i], L"--get-location") == 0) {
                const int rc = RunGetLocationMode();
                LocalFree(argv);
                return rc;
            }
        }
        LocalFree(argv);
    }

    SetAppUserModelId();

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

    g_app.appDir = GetExeDirectory();
    const std::wstring configPath = ResolveConfigPath(JoinPath(g_app.appDir, L"config.json"));
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

    g_app.menu = CreatePopupMenu();
    MENUINFO menuInfo{};
    menuInfo.cbSize = sizeof(menuInfo);
    menuInfo.fMask = MIM_STYLE;
    menuInfo.dwStyle = MNS_CHECKORBMP;
    SetMenuInfo(g_app.menu, &menuInfo);

    AppendMenuW(g_app.menu, MF_STRING | MF_UNCHECKED, CMD_ON, L"Turn On");
    AppendMenuW(g_app.menu, MF_STRING | MF_UNCHECKED, CMD_OFF, L"Turn Off");
    AppendMenuW(g_app.menu, MF_STRING | MF_UNCHECKED, CMD_SMART, L"Smart Mode");
    AppendMenuW(g_app.menu, MF_STRING | MF_UNCHECKED, CMD_SCHEDULE, L"Schedule Mode");
    AppendMenuW(g_app.menu, MF_STRING, CMD_SETTINGS, L"Settings...");
    AppendMenuW(g_app.menu, MF_STRING, CMD_REFRESH, L"Refresh Status");
    AppendMenuW(g_app.menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(g_app.menu, MF_STRING, CMD_RESTART, L"Restart");
    AppendMenuW(g_app.menu, MF_STRING, CMD_EXIT, L"Exit");

    g_app.menuTick = CreateMenuTickBitmap();
    if (g_app.menuTick) {
        SetMenuItemBitmaps(g_app.menu, CMD_ON, MF_BYCOMMAND, nullptr, g_app.menuTick);
        SetMenuItemBitmaps(g_app.menu, CMD_OFF, MF_BYCOMMAND, nullptr, g_app.menuTick);
        SetMenuItemBitmaps(g_app.menu, CMD_SMART, MF_BYCOMMAND, nullptr, g_app.menuTick);
        SetMenuItemBitmaps(g_app.menu, CMD_SCHEDULE, MF_BYCOMMAND, nullptr, g_app.menuTick);
    }

    if (!IsConfigComplete(g_app.config)) {
        if (!ShowSettingsDialog(g_app.hwnd, configPath, g_app.config) || !IsConfigComplete(g_app.config)) {
            MessageBoxW(
                g_app.hwnd,
                L"DuskPlug needs your plug connection details before it can run.\n\n"
                L"Open Settings from the tray menu when you are ready to finish setup.",
                L"DuskPlug",
                MB_ICONINFORMATION | MB_OK);
            DestroyWindow(g_app.hwnd);
            if (g_mutex) {
                CloseHandle(g_mutex);
            }
            return 1;
        }
    }

    g_app.client = std::make_unique<TuyaClient>(g_app.config);

    SmartModeCallbacks callbacks{};
    callbacks.updateTray = UpdateTrayDisplay;
    callbacks.showSetupBalloon = ShowSetupBalloon;
    callbacks.isBusy = []() { return g_app.busy; };
    g_app.smart.Initialize(g_app.hwnd, g_app.config, g_app.appDir, g_app.client.get(), callbacks);

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

    return static_cast<int>(message.wParam);
}
