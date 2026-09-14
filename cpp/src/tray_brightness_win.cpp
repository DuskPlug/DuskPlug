#include "tray_brightness_win.h"

#include <windowsx.h>
#include <commctrl.h>
#include <cstdio>

#pragma comment(lib, "comctl32.lib")

namespace {

constexpr wchar_t kPopupClass[] = L"DuskPlugBrightnessPopup";

constexpr int kTrackId = 3001;
constexpr int kLabelId = 3002;
constexpr int kPopupWidth = 220;
constexpr int kSliderSectionHeight = 54;
constexpr int kAutoRowHeight = 24;
constexpr int kPopupHeight = kSliderSectionHeight + kAutoRowHeight;
constexpr int kAutoRowTop = kSliderSectionHeight;
constexpr UINT WM_BRIGHTNESS_SYNC = WM_APP + 20;
constexpr UINT_PTR IDT_BRIGHTNESS_APPLY = 4001;
constexpr DWORD kBrightnessApplyDelayMs = 500;

TrayBrightnessCallbacks g_callbacks{};

HWND g_owner = nullptr;
HWND g_popup = nullptr;
HMENU g_brightnessSubMenu = nullptr;
HMENU g_pendingSubMenu = nullptr;

HHOOK g_llMouseHook = nullptr;
HHOOK g_menuFilterHook = nullptr;

HCURSOR g_handCursor = nullptr;
HCURSOR g_arrowCursor = nullptr;

bool g_classRegistered = false;
bool g_updatingTrack = false;
bool g_dragging = false;
bool g_popupVisible = false;

int g_pendingApplyPercent = -1;
int g_lastAppliedPercent = -1;

void UpdateLabelText();
void InstallInputHooks();
void UninstallInputHooks();
void ScheduleBrightnessApply(int percent);
void FlushBrightnessApply();
void PaintAutoRow(HDC dc, const RECT& clientRect);

int ClampPercent(int percent) {
    if (percent < 0) {
        return 0;
    }
    if (percent > 100) {
        return 100;
    }
    return percent;
}

bool PopupScreenRect(RECT& outRect) {
    if (!g_popup || !IsWindowVisible(g_popup)) {
        return false;
    }
    return GetWindowRect(g_popup, &outRect) != FALSE;
}

bool TrackScreenRect(RECT& outRect) {
    if (!g_popup) {
        return false;
    }

    HWND track = GetDlgItem(g_popup, kTrackId);
    if (!track) {
        return false;
    }

    return GetWindowRect(track, &outRect) != FALSE;
}

bool AutoRowScreenRect(RECT& outRect) {
    RECT popupRect{};
    if (!PopupScreenRect(popupRect)) {
        return false;
    }

    outRect.left = popupRect.left;
    outRect.right = popupRect.right;
    outRect.top = popupRect.top + kAutoRowTop;
    outRect.bottom = popupRect.top + kPopupHeight;
    return true;
}

int PercentFromTrackPoint(HWND track, int clientX) {
    RECT rc{};
    GetClientRect(track, &rc);
    const int width = rc.right - rc.left;
    if (width <= 0) {
        return 0;
    }
    return ClampPercent((clientX * 100) / width);
}

void ScheduleBrightnessApply(int percent) {
    g_pendingApplyPercent = ClampPercent(percent);
    if (g_owner) {
        SetTimer(g_owner, IDT_BRIGHTNESS_APPLY, kBrightnessApplyDelayMs, nullptr);
    }
}

void FlushBrightnessApply() {
    if (g_owner) {
        KillTimer(g_owner, IDT_BRIGHTNESS_APPLY);
    }

    if (g_pendingApplyPercent < 0 || !g_callbacks.setPercent) {
        return;
    }

    if (g_pendingApplyPercent != g_lastAppliedPercent) {
        g_callbacks.setPercent(g_pendingApplyPercent);
        g_lastAppliedPercent = g_pendingApplyPercent;
    }
    g_pendingApplyPercent = -1;
}

void ApplyTrackPercent(int percent, bool scheduleApply) {
    if (!g_popup) {
        return;
    }

    HWND track = GetDlgItem(g_popup, kTrackId);
    if (!track) {
        return;
    }

    percent = ClampPercent(percent);
    g_updatingTrack = true;
    SendMessageW(track, TBM_SETPOS, TRUE, percent);
    g_updatingTrack = false;
    UpdateLabelText();

    if (scheduleApply) {
        ScheduleBrightnessApply(percent);
    }
}

void UpdateLabelText() {
    if (!g_popup) {
        return;
    }

    HWND label = GetDlgItem(g_popup, kLabelId);
    HWND track = GetDlgItem(g_popup, kTrackId);
    if (!label || !track) {
        return;
    }

    const int percent = static_cast<int>(SendMessageW(track, TBM_GETPOS, 0, 0));
    wchar_t text[32];
    _snwprintf(text, 32, L"Brightness: %d%%", percent);
    SetWindowTextW(label, text);
}

void SetPercentFromScreenPoint(int screenX, int screenY, bool scheduleApply) {
    (void)screenY;
    HWND track = GetDlgItem(g_popup, kTrackId);
    if (!track) {
        return;
    }

    POINT pt{screenX, screenY};
    ScreenToClient(track, &pt);
    ApplyTrackPercent(PercentFromTrackPoint(track, pt.x), scheduleApply);
}

void PaintAutoRow(HDC dc, const RECT& clientRect) {
    RECT autoRow = clientRect;
    autoRow.top = kAutoRowTop;
    autoRow.bottom = kPopupHeight;
    FillRect(dc, &autoRow, GetSysColorBrush(COLOR_MENU));

    HPEN separator = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
    HPEN previousPen = reinterpret_cast<HPEN>(SelectObject(dc, separator));
    MoveToEx(dc, autoRow.left + 1, kAutoRowTop, nullptr);
    LineTo(dc, autoRow.right - 1, kAutoRowTop);
    SelectObject(dc, previousPen);
    DeleteObject(separator);

    const bool autoEnabled = g_callbacks.isAutoEnabled && g_callbacks.isAutoEnabled();
    const int checkSize = GetSystemMetrics(SM_CXMENUCHECK);
    const int checkX = autoRow.left + 12;
    const int checkY = autoRow.top + ((autoRow.bottom - autoRow.top - checkSize) / 2);
    RECT checkRect = {checkX, checkY, checkX + checkSize, checkY + checkSize};
    DrawFrameControl(
        dc,
        &checkRect,
        DFC_MENU,
        autoEnabled ? DFCS_MENUCHECK | DFCS_CHECKED : DFCS_MENUCHECK);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, GetSysColor(COLOR_MENUTEXT));
    RECT textRect = autoRow;
    textRect.left = checkX + checkSize + 8;
    DrawTextW(dc, L"Automatic", -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void UpdateHoverCursor(POINT screenPt) {
    RECT trackRect{};
    if (TrackScreenRect(trackRect) && PtInRect(&trackRect, screenPt) != FALSE) {
        SetCursor(g_handCursor ? g_handCursor : LoadCursorW(nullptr, IDC_HAND));
        return;
    }

    RECT autoRowRect{};
    if (AutoRowScreenRect(autoRowRect) && PtInRect(&autoRowRect, screenPt) != FALSE) {
        SetCursor(g_arrowCursor ? g_arrowCursor : LoadCursorW(nullptr, IDC_ARROW));
        return;
    }

    SetCursor(g_arrowCursor ? g_arrowCursor : LoadCursorW(nullptr, IDC_ARROW));
}

enum class MouseHandleResult {
    NotHandled,
    HandledPassThrough,
    HandledBlock,
};

MouseHandleResult HandlePanelMouse(UINT message, POINT screenPt) {
    RECT popupRect{};
    const bool hasPopup = PopupScreenRect(popupRect);
    const bool inPopup = hasPopup && PtInRect(&popupRect, screenPt) != FALSE;

    RECT trackRect{};
    const bool inTrack = TrackScreenRect(trackRect) && PtInRect(&trackRect, screenPt) != FALSE;

    RECT autoRowRect{};
    const bool inAutoRow = AutoRowScreenRect(autoRowRect) && PtInRect(&autoRowRect, screenPt) != FALSE;

    if (message == WM_MOUSEMOVE) {
        UpdateHoverCursor(screenPt);
    }

    if (message == WM_LBUTTONDOWN && inAutoRow) {
        if (g_callbacks.setAutoEnabled && g_callbacks.isAutoEnabled) {
            g_callbacks.setAutoEnabled(!g_callbacks.isAutoEnabled());
        }
        InvalidateRect(g_popup, nullptr, FALSE);
        return MouseHandleResult::HandledBlock;
    }

    if (message == WM_LBUTTONDOWN && inTrack) {
        g_dragging = true;
        SetPercentFromScreenPoint(screenPt.x, screenPt.y, true);
        return MouseHandleResult::HandledBlock;
    }

    if (g_dragging) {
        if (message == WM_MOUSEMOVE) {
            SetPercentFromScreenPoint(screenPt.x, screenPt.y, true);
            return MouseHandleResult::HandledPassThrough;
        }
        if (message == WM_LBUTTONUP) {
            SetPercentFromScreenPoint(screenPt.x, screenPt.y, false);
            FlushBrightnessApply();
            g_dragging = false;
            return MouseHandleResult::HandledBlock;
        }
    }

    if (message == WM_LBUTTONDOWN && inPopup && !inAutoRow && !inTrack) {
        return MouseHandleResult::HandledBlock;
    }

    return MouseHandleResult::NotHandled;
}

LRESULT CALLBACK LowLevelMouseProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code >= 0 && g_popupVisible) {
        const MSLLHOOKSTRUCT* info = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        const POINT screenPt = info->pt;

        switch (wParam) {
        case WM_LBUTTONDOWN:
        case WM_MOUSEMOVE:
        case WM_LBUTTONUP: {
            const MouseHandleResult handled = HandlePanelMouse(static_cast<UINT>(wParam), screenPt);
            if (handled == MouseHandleResult::HandledBlock) {
                return 1;
            }
            break;
        }
        default:
            break;
        }
    }

    return CallNextHookEx(g_llMouseHook, code, wParam, lParam);
}

LRESULT CALLBACK BrightnessMenuFilterProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code >= 0 && g_popupVisible) {
        const MSG* msg = reinterpret_cast<MSG*>(lParam);
        if (wParam == MSGF_MENU) {
            switch (msg->message) {
            case WM_LBUTTONDOWN:
            case WM_MOUSEMOVE:
            case WM_LBUTTONUP: {
                POINT screenPt = msg->pt;
                if ((screenPt.x | screenPt.y) == 0) {
                    const DWORD pos = GetMessagePos();
                    screenPt.x = GET_X_LPARAM(pos);
                    screenPt.y = GET_Y_LPARAM(pos);
                }
                const MouseHandleResult handled = HandlePanelMouse(msg->message, screenPt);
                if (handled != MouseHandleResult::NotHandled) {
                    return 1;
                }
                break;
            }
            default:
                break;
            }
        }
    }

    return CallNextHookEx(g_menuFilterHook, code, wParam, lParam);
}

void InstallInputHooks() {
    if (!g_llMouseHook) {
        g_llMouseHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, GetModuleHandleW(nullptr), 0);
    }
    if (!g_menuFilterHook) {
        g_menuFilterHook = SetWindowsHookExW(
            WH_MSGFILTER,
            BrightnessMenuFilterProc,
            GetModuleHandleW(nullptr),
            GetCurrentThreadId());
    }
}

void UninstallInputHooks() {
    if (g_llMouseHook) {
        UnhookWindowsHookEx(g_llMouseHook);
        g_llMouseHook = nullptr;
    }
    if (g_menuFilterHook) {
        UnhookWindowsHookEx(g_menuFilterHook);
        g_menuFilterHook = nullptr;
    }
}

LRESULT CALLBACK BrightnessPopupProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT clientRect{};
        GetClientRect(hwnd, &clientRect);
        PaintAutoRow(dc, clientRect);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND: {
        RECT rc{};
        GetClientRect(hwnd, &rc);
        FillRect(reinterpret_cast<HDC>(wParam), &rc, GetSysColorBrush(COLOR_MENU));
        return 1;
    }

    case WM_SETCURSOR:
        if (reinterpret_cast<HWND>(wParam) == GetDlgItem(hwnd, kTrackId)) {
            SetCursor(g_handCursor ? g_handCursor : LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        }
        break;

    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(lParam) == GetDlgItem(hwnd, kTrackId)) {
            if (!g_updatingTrack) {
                const int percent = static_cast<int>(SendMessageW(
                    reinterpret_cast<HWND>(lParam),
                    TBM_GETPOS,
                    0,
                    0));
                ScheduleBrightnessApply(percent);
            }
            UpdateLabelText();
        }
        return 0;

    case WM_BRIGHTNESS_SYNC: {
        const int percent = g_callbacks.getPercent ? g_callbacks.getPercent() : 50;
        ApplyTrackPercent(percent, false);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void EnsurePopupCreated() {
    if (g_popup) {
        return;
    }

    if (!g_classRegistered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = BrightnessPopupProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = kPopupClass;
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_MENU + 1);
        RegisterClassExW(&wc);
        g_classRegistered = true;
    }

    g_popup = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kPopupClass,
        L"",
        WS_POPUP,
        0,
        0,
        kPopupWidth,
        kPopupHeight,
        g_owner,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);
    if (!g_popup) {
        return;
    }

    CreateWindowExW(
        0,
        L"STATIC",
        L"Brightness: 50%",
        WS_CHILD | WS_VISIBLE,
        12,
        8,
        kPopupWidth - 24,
        18,
        g_popup,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kLabelId)),
        GetModuleHandleW(nullptr),
        nullptr);

    CreateWindowExW(
        0,
        TRACKBAR_CLASSW,
        L"",
        WS_CHILD | WS_VISIBLE | TBS_NOTICKS,
        12,
        28,
        kPopupWidth - 24,
        22,
        g_popup,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTrackId)),
        GetModuleHandleW(nullptr),
        nullptr);
    SendMessageW(GetDlgItem(g_popup, kTrackId), TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
}

}  // namespace

void InitTrayBrightnessUi(HWND owner, const TrayBrightnessCallbacks& callbacks) {
    g_owner = owner;
    g_callbacks = callbacks;
    g_handCursor = LoadCursorW(nullptr, IDC_HAND);
    g_arrowCursor = LoadCursorW(nullptr, IDC_ARROW);

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);
    EnsurePopupCreated();
}

void ShutdownTrayBrightnessUi() {
    HideBrightnessPanel();
    if (g_popup) {
        DestroyWindow(g_popup);
        g_popup = nullptr;
    }
    g_owner = nullptr;
    g_callbacks = {};
    g_handCursor = nullptr;
    g_arrowCursor = nullptr;
}

void OnTrayContextMenuOpening() {
    g_dragging = false;
    g_popupVisible = false;
    InstallInputHooks();
}

void ShowBrightnessPanelAtRect(const RECT& itemRect) {
    if (!g_callbacks.isAvailable || !g_callbacks.isAvailable()) {
        return;
    }

    EnsurePopupCreated();
    if (!g_popup) {
        return;
    }

    SendMessageW(g_popup, WM_BRIGHTNESS_SYNC, 0, 0);
    g_pendingApplyPercent = -1;
    g_lastAppliedPercent = g_callbacks.getPercent ? ClampPercent(g_callbacks.getPercent()) : 50;

    int x = itemRect.left;
    int y = itemRect.top;

    const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    if (x + kPopupWidth > screenWidth) {
        x = (screenWidth > kPopupWidth) ? (screenWidth - kPopupWidth) : 0;
    }
    if (y + kPopupHeight > screenHeight) {
        y = (screenHeight > kPopupHeight) ? (screenHeight - kPopupHeight) : 0;
    }

    SetWindowPos(
        g_popup,
        HWND_TOPMOST,
        x,
        y,
        kPopupWidth,
        kPopupHeight,
        SWP_SHOWWINDOW | SWP_NOACTIVATE);
    SetWindowPos(g_popup, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);

    g_popupVisible = true;
    g_dragging = false;
    InstallInputHooks();
}

void ShowBrightnessPanel(HWND owner, HMENU brightnessSubMenu) {
    if (!brightnessSubMenu) {
        return;
    }

    g_brightnessSubMenu = brightnessSubMenu;

    RECT itemRect{};
    if (!GetMenuItemRect(owner, brightnessSubMenu, CMD_SCREEN_BRIGHTNESS_PLACEHOLDER, &itemRect)) {
        return;
    }

    ShowBrightnessPanelAtRect(itemRect);
}

void RequestShowBrightnessPanel(HWND owner, HMENU brightnessSubMenu) {
    g_pendingSubMenu = brightnessSubMenu;
    PostMessageW(owner, WM_TRAY_BRIGHTNESS_SHOW, 0, 0);
}

void HideBrightnessPanel() {
    FlushBrightnessApply();
    g_dragging = false;
    g_popupVisible = false;
    g_brightnessSubMenu = nullptr;
    g_pendingSubMenu = nullptr;
    if (g_popup) {
        ShowWindow(g_popup, SW_HIDE);
    }
}

void MeasureBrightnessPlaceholderItem(MEASUREITEMSTRUCT* measure) {
    if (!measure || measure->itemID != CMD_SCREEN_BRIGHTNESS_PLACEHOLDER) {
        return;
    }

    measure->itemWidth = kPopupWidth;
    measure->itemHeight = kPopupHeight;
}

void DrawBrightnessPlaceholderItem(const DRAWITEMSTRUCT* draw) {
    if (!draw || draw->itemID != CMD_SCREEN_BRIGHTNESS_PLACEHOLDER) {
        return;
    }

    FillRect(draw->hDC, &draw->rcItem, GetSysColorBrush(COLOR_MENU));
    ShowBrightnessPanelAtRect(draw->rcItem);
}

void SyncBrightnessPanelAutoState() {
    if (g_popup && IsWindowVisible(g_popup)) {
        InvalidateRect(g_popup, nullptr, FALSE);
    }
}

bool HandleTrayBrightnessMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT& result) {
    if (msg == WM_TRAY_BRIGHTNESS_SHOW) {
        if (g_pendingSubMenu) {
            ShowBrightnessPanel(hwnd, g_pendingSubMenu);
        }
        result = 0;
        return true;
    }

    if (msg == WM_TIMER && wParam == IDT_BRIGHTNESS_APPLY) {
        FlushBrightnessApply();
        result = 0;
        return true;
    }

    if (msg == WM_EXITMENULOOP || msg == WM_CANCELMODE) {
        HideBrightnessPanel();
        UninstallInputHooks();
    }

    result = 0;
    return false;
}
