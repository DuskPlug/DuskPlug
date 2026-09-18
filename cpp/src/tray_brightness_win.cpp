#include "tray_brightness_win.h"

#include <windowsx.h>
#include <cstdio>

#pragma comment(lib, "gdi32.lib")

namespace {

constexpr wchar_t kPopupClass[] = L"DuskPlugBrightnessPopup";

constexpr int kPopupWidth = 248;
constexpr int kPad = 14;
constexpr int kSliderTop = 40;
constexpr int kSliderHeight = 20;
constexpr int kAutoRowTop = 78;
constexpr int kAutoRowHeight = 28;
constexpr int kPopupHeight = kAutoRowTop + kAutoRowHeight + 6;
constexpr UINT WM_BRIGHTNESS_SYNC = WM_APP + 20;
constexpr UINT_PTR IDT_BRIGHTNESS_APPLY = 4001;
constexpr DWORD kBrightnessApplyDelayMs = 500;

constexpr COLORREF kBgBottom = RGB(12, 16, 24);
constexpr COLORREF kCardBorder = RGB(48, 54, 66);
constexpr COLORREF kText = RGB(238, 243, 248);
constexpr COLORREF kMuted = RGB(147, 160, 180);
constexpr COLORREF kAccent = RGB(231, 163, 90);
constexpr COLORREF kAccent2 = RGB(139, 183, 255);
constexpr COLORREF kTrackBg = RGB(15, 21, 32);

TrayBrightnessCallbacks g_callbacks{};

HWND g_owner = nullptr;
HWND g_popup = nullptr;
HMENU g_brightnessSubMenu = nullptr;
HMENU g_pendingSubMenu = nullptr;

HHOOK g_llMouseHook = nullptr;
HHOOK g_menuFilterHook = nullptr;

HCURSOR g_handCursor = nullptr;
HFONT g_titleFont = nullptr;
HFONT g_valueFont = nullptr;
HFONT g_bodyFont = nullptr;

bool g_classRegistered = false;
bool g_dragging = false;
bool g_popupVisible = false;

int g_currentPercent = 50;
int g_pendingApplyPercent = -1;
int g_lastAppliedPercent = -1;
RECT g_lastPanelScreenRect{};

bool RectsNearEqual(const RECT& a, const RECT& b) {
    return a.left == b.left
        && a.top == b.top
        && a.right == b.right
        && a.bottom == b.bottom;
}

void UpdateLabelText();
void InstallInputHooks();
void UninstallInputHooks();
void ScheduleBrightnessApply(int percent);
void FlushBrightnessApply();
void SyncAutoCheckboxState();
void ApplyTrackPercent(int percent, bool scheduleApply);
void SyncPanelTrackFromSystem();
int InitialPanelPercent();
void ToggleAutomaticFromPanel();
void EnsureThemeResources();
void ReleaseThemeResources();
RECT TrackClientRect(const RECT& clientRect);
RECT AutoRowClientRect(const RECT& clientRect);
bool TrackScreenRect(RECT& outRect);
bool AutoRowScreenRect(RECT& outRect);
void PaintPanel(HDC dc, const RECT& clientRect);

int ClampPercent(int percent) {
    if (percent < 0) {
        return 0;
    }
    if (percent > 100) {
        return 100;
    }
    return percent;
}

void EnsureThemeResources() {
    if (g_titleFont) {
        return;
    }

    g_titleFont = CreateFontW(
        -12,
        0,
        0,
        0,
        FW_BOLD,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI");
    g_valueFont = CreateFontW(
        -18,
        0,
        0,
        0,
        FW_SEMIBOLD,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI");
    g_bodyFont = CreateFontW(
        -13,
        0,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI");
}

void ReleaseThemeResources() {
    if (g_titleFont) {
        DeleteObject(g_titleFont);
        g_titleFont = nullptr;
    }
    if (g_valueFont) {
        DeleteObject(g_valueFont);
        g_valueFont = nullptr;
    }
    if (g_bodyFont) {
        DeleteObject(g_bodyFont);
        g_bodyFont = nullptr;
    }
}

bool PopupScreenRect(RECT& outRect) {
    if (!g_popup || !IsWindowVisible(g_popup)) {
        return false;
    }
    return GetWindowRect(g_popup, &outRect) != FALSE;
}

RECT TrackClientRect(const RECT& clientRect) {
    RECT rc{};
    rc.left = kPad;
    rc.right = clientRect.right - kPad;
    rc.top = kSliderTop;
    rc.bottom = kSliderTop + kSliderHeight;
    return rc;
}

RECT AutoRowClientRect(const RECT& clientRect) {
    RECT rc{};
    rc.left = kPad;
    rc.right = clientRect.right - kPad;
    rc.top = kAutoRowTop;
    rc.bottom = kAutoRowTop + kAutoRowHeight;
    return rc;
}

bool TrackScreenRect(RECT& outRect) {
    if (!g_popup) {
        return false;
    }

    RECT clientRect{};
    GetClientRect(g_popup, &clientRect);
    outRect = TrackClientRect(clientRect);
    MapWindowPoints(g_popup, nullptr, reinterpret_cast<LPPOINT>(&outRect), 2);
    return true;
}

bool AutoRowScreenRect(RECT& outRect) {
    if (!g_popup) {
        return false;
    }

    RECT clientRect{};
    GetClientRect(g_popup, &clientRect);
    outRect = AutoRowClientRect(clientRect);
    MapWindowPoints(g_popup, nullptr, reinterpret_cast<LPPOINT>(&outRect), 2);
    return true;
}

int PercentFromTrackPoint(const RECT& trackRect, int clientX) {
    const int width = trackRect.right - trackRect.left;
    if (width <= 0) {
        return 0;
    }
    const int localX = clientX - trackRect.left;
    return ClampPercent((localX * 100) / width);
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

void SyncAutoCheckboxState() {
    if (g_popup) {
        InvalidateRect(g_popup, nullptr, FALSE);
    }
}

int InitialPanelPercent() {
    if (g_callbacks.isAutoEnabled && g_callbacks.isAutoEnabled() && g_callbacks.getPercent) {
        return ClampPercent(g_callbacks.getPercent());
    }

    if (g_callbacks.getHardwarePercent) {
        const int hardware = g_callbacks.getHardwarePercent();
        if (hardware >= 0) {
            return ClampPercent(hardware);
        }
    }

    if (g_callbacks.getPercent) {
        return ClampPercent(g_callbacks.getPercent());
    }

    return 50;
}

void SyncPanelTrackFromSystem() {
    if (!g_popup || g_dragging) {
        return;
    }

    const int percent = g_callbacks.getPercent ? ClampPercent(g_callbacks.getPercent()) : 50;
    ApplyTrackPercent(percent, false);
    g_lastAppliedPercent = percent;
    g_pendingApplyPercent = -1;
}

void ToggleAutomaticFromPanel() {
    const bool enabled = g_callbacks.isAutoEnabled && g_callbacks.isAutoEnabled();
    if (g_callbacks.setAutoEnabled) {
        g_callbacks.setAutoEnabled(!enabled);
    }
    SyncBrightnessPanelAutoState();
}

void DisableAutomaticIfEnabled() {
    if (g_callbacks.isAutoEnabled && g_callbacks.isAutoEnabled() && g_callbacks.setAutoEnabled) {
        g_callbacks.setAutoEnabled(false);
        SyncAutoCheckboxState();
    }
}

int CurrentTrackPercent() {
    return g_currentPercent;
}

void ApplyTrackPercent(int percent, bool scheduleApply) {
    percent = ClampPercent(percent);
    g_currentPercent = percent;
    UpdateLabelText();

    if (scheduleApply) {
        DisableAutomaticIfEnabled();
        ScheduleBrightnessApply(percent);
    }
}

bool AdjustBrightnessByWheel(POINT screenPt, short wheelDelta) {
    if (!g_popup || wheelDelta == 0) {
        return false;
    }

    RECT popupRect{};
    if (!PopupScreenRect(popupRect) || PtInRect(&popupRect, screenPt) == FALSE) {
        return false;
    }

    constexpr int kWheelStep = 5;
    int percent = CurrentTrackPercent();
    if (wheelDelta > 0) {
        percent += kWheelStep;
    } else {
        percent -= kWheelStep;
    }

    ApplyTrackPercent(percent, true);
    return true;
}

void UpdateLabelText() {
    if (g_popup) {
        InvalidateRect(g_popup, nullptr, FALSE);
    }
}

void SetPercentFromScreenPoint(int screenX, int screenY, bool scheduleApply) {
    (void)screenY;
    if (!g_popup) {
        return;
    }

    RECT clientRect{};
    GetClientRect(g_popup, &clientRect);
    const RECT trackRect = TrackClientRect(clientRect);
    POINT pt{screenX, screenY};
    ScreenToClient(g_popup, &pt);
    ApplyTrackPercent(PercentFromTrackPoint(trackRect, pt.x), scheduleApply);
}

void FillRoundedRect(HDC dc, const RECT& rc, int radius, HBRUSH brush) {
    HBRUSH previous = reinterpret_cast<HBRUSH>(SelectObject(dc, brush));
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    SelectObject(dc, previous);
}

void PaintPanel(HDC dc, const RECT& clientRect) {
    EnsureThemeResources();

    HBRUSH background = CreateSolidBrush(kBgBottom);
    FillRect(dc, &clientRect, background);
    DeleteObject(background);

    HPEN borderPen = CreatePen(PS_SOLID, 1, kCardBorder);
    HPEN previousPen = reinterpret_cast<HPEN>(SelectObject(dc, borderPen));
    HBRUSH previousBrush = reinterpret_cast<HBRUSH>(SelectObject(dc, GetStockObject(NULL_BRUSH)));
    Rectangle(dc, clientRect.left, clientRect.top, clientRect.right, clientRect.bottom);
    SelectObject(dc, previousBrush);
    SelectObject(dc, previousPen);
    DeleteObject(borderPen);

    SetBkMode(dc, TRANSPARENT);

    HFONT previousFont = reinterpret_cast<HFONT>(SelectObject(dc, g_titleFont));
    SetTextColor(dc, kAccent);
    RECT titleRect{clientRect.left + kPad, clientRect.top + 10, clientRect.right - kPad, clientRect.top + 28};
    DrawTextW(dc, L"SCREEN BRIGHTNESS", -1, &titleRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    SelectObject(dc, g_valueFont);
    SetTextColor(dc, kText);
    wchar_t valueText[16];
    _snwprintf(valueText, 16, L"%d%%", g_currentPercent);
    RECT valueRect{clientRect.right - 72, clientRect.top + 8, clientRect.right - kPad, clientRect.top + 32};
    DrawTextW(dc, valueText, -1, &valueRect, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);

    const RECT trackRect = TrackClientRect(clientRect);
    HBRUSH trackBg = CreateSolidBrush(kTrackBg);
    FillRoundedRect(dc, trackRect, 10, trackBg);
    DeleteObject(trackBg);

    const int trackWidth = trackRect.right - trackRect.left;
    const int fillWidth = (trackWidth * g_currentPercent) / 100;
    if (fillWidth > 0) {
        RECT fillRect = trackRect;
        fillRect.right = fillRect.left + fillWidth;
        HBRUSH fillBrush = CreateSolidBrush(kAccent);
        FillRoundedRect(dc, fillRect, 10, fillBrush);
        DeleteObject(fillBrush);
    }

    const int thumbX = trackRect.left + (trackWidth * g_currentPercent) / 100;
    const int thumbY = (trackRect.top + trackRect.bottom) / 2;
    HBRUSH thumbBrush = CreateSolidBrush(kAccent2);
    HBRUSH previousThumbBrush = reinterpret_cast<HBRUSH>(SelectObject(dc, thumbBrush));
    HPEN thumbPen = CreatePen(PS_SOLID, 1, kAccent2);
    HPEN previousThumbPen = reinterpret_cast<HPEN>(SelectObject(dc, thumbPen));
    Ellipse(dc, thumbX - 7, thumbY - 7, thumbX + 7, thumbY + 7);
    SelectObject(dc, previousThumbPen);
    SelectObject(dc, previousThumbBrush);
    DeleteObject(thumbPen);
    DeleteObject(thumbBrush);

    HPEN separator = CreatePen(PS_SOLID, 1, kCardBorder);
    previousPen = reinterpret_cast<HPEN>(SelectObject(dc, separator));
    MoveToEx(dc, clientRect.left + kPad, kAutoRowTop, nullptr);
    LineTo(dc, clientRect.right - kPad, kAutoRowTop);
    SelectObject(dc, previousPen);
    DeleteObject(separator);

    const bool autoEnabled = g_callbacks.isAutoEnabled && g_callbacks.isAutoEnabled();
    const RECT autoRow = AutoRowClientRect(clientRect);
    const int boxSize = 16;
    const int boxTop = autoRow.top + ((autoRow.bottom - autoRow.top - boxSize) / 2);
    RECT boxRect{autoRow.left, boxTop, autoRow.left + boxSize, boxTop + boxSize};

    HBRUSH boxFill = CreateSolidBrush(autoEnabled ? kAccent : kTrackBg);
    FillRoundedRect(dc, boxRect, 4, boxFill);
    DeleteObject(boxFill);

    HPEN boxPen = CreatePen(PS_SOLID, 1, autoEnabled ? kAccent : kCardBorder);
    previousPen = reinterpret_cast<HPEN>(SelectObject(dc, boxPen));
    previousBrush = reinterpret_cast<HBRUSH>(SelectObject(dc, GetStockObject(NULL_BRUSH)));
    RoundRect(dc, boxRect.left, boxRect.top, boxRect.right, boxRect.bottom, 4, 4);
    SelectObject(dc, previousBrush);
    SelectObject(dc, previousPen);
    DeleteObject(boxPen);

    if (autoEnabled) {
        HPEN checkPen = CreatePen(PS_SOLID, 2, RGB(27, 18, 8));
        previousPen = reinterpret_cast<HPEN>(SelectObject(dc, checkPen));
        MoveToEx(dc, boxRect.left + 3, boxRect.top + 8, nullptr);
        LineTo(dc, boxRect.left + 7, boxRect.bottom - 4);
        LineTo(dc, boxRect.right - 2, boxRect.top + 4);
        SelectObject(dc, previousPen);
        DeleteObject(checkPen);
    }

    SelectObject(dc, g_bodyFont);
    SetTextColor(dc, kText);
    RECT labelRect{boxRect.right + 10, autoRow.top, autoRow.right, autoRow.bottom};
    DrawTextW(dc, L"Automatic", -1, &labelRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    SelectObject(dc, previousFont);
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

    if ((message == WM_LBUTTONDOWN || message == WM_LBUTTONUP) && inAutoRow) {
        if (message == WM_LBUTTONUP) {
            ToggleAutomaticFromPanel();
        }
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
    if (code < 0 || !g_popupVisible) {
        return CallNextHookEx(g_llMouseHook, code, wParam, lParam);
    }

    const MSLLHOOKSTRUCT* info = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
    const POINT screenPt = info->pt;

    if (!g_dragging) {
        RECT popupRect{};
        if (!PopupScreenRect(popupRect) || PtInRect(&popupRect, screenPt) == FALSE) {
            return CallNextHookEx(g_llMouseHook, code, wParam, lParam);
        }
    }

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
        case WM_MOUSEWHEEL: {
            const short delta = static_cast<short>(HIWORD(info->mouseData));
            if (AdjustBrightnessByWheel(screenPt, delta)) {
                return 1;
            }
            break;
        }
        default:
            break;
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
                const DWORD pos = GetMessagePos();
                POINT screenPt{
                    GET_X_LPARAM(pos),
                    GET_Y_LPARAM(pos),
                };
                const MouseHandleResult handled = HandlePanelMouse(msg->message, screenPt);
                if (handled == MouseHandleResult::HandledBlock) {
                    return 1;
                }
                break;
            }
            case WM_MOUSEWHEEL: {
                POINT screenPt = msg->pt;
                if ((screenPt.x | screenPt.y) == 0) {
                    const DWORD pos = GetMessagePos();
                    screenPt.x = GET_X_LPARAM(pos);
                    screenPt.y = GET_Y_LPARAM(pos);
                }
                const short delta = static_cast<short>(HIWORD(msg->wParam));
                if (AdjustBrightnessByWheel(screenPt, delta)) {
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
        PaintPanel(dc, clientRect);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND: {
        RECT rc{};
        GetClientRect(hwnd, &rc);
        PaintPanel(reinterpret_cast<HDC>(wParam), rc);
        return 1;
    }

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            POINT screenPt{};
            GetCursorPos(&screenPt);
            RECT trackRect{};
            if (TrackScreenRect(trackRect) && PtInRect(&trackRect, screenPt) != FALSE) {
                SetCursor(g_handCursor ? g_handCursor : LoadCursorW(nullptr, IDC_HAND));
                return TRUE;
            }
        }
        break;

    case WM_MOUSEWHEEL: {
        POINT screenPt{};
        GetCursorPos(&screenPt);
        if (AdjustBrightnessByWheel(screenPt, GET_WHEEL_DELTA_WPARAM(wParam))) {
            return 0;
        }
        break;
    }

    case WM_BRIGHTNESS_SYNC: {
        SyncAutoCheckboxState();
        if (g_callbacks.isAutoEnabled && g_callbacks.isAutoEnabled()) {
            SyncPanelTrackFromSystem();
        }
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
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
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
}

bool GetPlaceholderScreenRect(RECT& outRect) {
    if (!g_brightnessSubMenu) {
        return false;
    }

    if (g_owner && GetMenuItemRect(g_owner, g_brightnessSubMenu, 0, &outRect)) {
        return true;
    }
    return GetMenuItemRect(nullptr, g_brightnessSubMenu, 0, &outRect) != FALSE;
}

bool ScreenRectFromDrawItem(const DRAWITEMSTRUCT* draw, RECT& outRect) {
    if (!draw) {
        return false;
    }

    outRect = draw->rcItem;
    const HWND menuWnd = WindowFromDC(draw->hDC);
    if (menuWnd) {
        MapWindowPoints(menuWnd, nullptr, reinterpret_cast<LPPOINT>(&outRect), 2);
        return true;
    }

    return GetPlaceholderScreenRect(outRect);
}

void PaintPlaceholderBackground(HDC dc, const RECT& rcItem) {
    EnsureThemeResources();
    PaintPanel(dc, rcItem);
}

}  // namespace

void InitTrayBrightnessUi(HWND owner, const TrayBrightnessCallbacks& callbacks) {
    g_owner = owner;
    g_callbacks = callbacks;
    g_handCursor = LoadCursorW(nullptr, IDC_HAND);
    EnsureThemeResources();
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
    ReleaseThemeResources();
}

void OnTrayContextMenuOpening() {
    g_dragging = false;
    g_popupVisible = false;
}

void ShowBrightnessPanelAtRect(const RECT& itemRect) {
    if (!g_callbacks.isAvailable || !g_callbacks.isAvailable()) {
        return;
    }

    EnsurePopupCreated();
    if (!g_popup) {
        return;
    }

    const bool firstShow = !g_popupVisible;
    if (g_popupVisible && RectsNearEqual(g_lastPanelScreenRect, itemRect)) {
        return;
    }

    int x = itemRect.left;
    int y = itemRect.top;
    int width = itemRect.right - itemRect.left;
    int height = itemRect.bottom - itemRect.top;
    if (width < kPopupWidth) {
        width = kPopupWidth;
    }
    if (height < kPopupHeight) {
        height = kPopupHeight;
    }

    const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    if (x + width > screenWidth) {
        x = (screenWidth > width) ? (screenWidth - width) : 0;
    }
    if (y + height > screenHeight) {
        y = (screenHeight > height) ? (screenHeight - height) : 0;
    }

    if (firstShow) {
        const int percent = InitialPanelPercent();
        ApplyTrackPercent(percent, false);
        g_lastAppliedPercent = percent;
        g_pendingApplyPercent = -1;
        SyncAutoCheckboxState();
    }

    SetWindowPos(
        g_popup,
        HWND_TOPMOST,
        x,
        y,
        width,
        height,
        SWP_SHOWWINDOW | SWP_NOACTIVATE);
    SetWindowPos(g_popup, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);

    g_lastPanelScreenRect = itemRect;
    g_popupVisible = true;
    if (firstShow) {
        g_dragging = false;
    }
    InstallInputHooks();
    InvalidateRect(g_popup, nullptr, TRUE);
}

void ShowBrightnessPanel(HWND owner, HMENU brightnessSubMenu) {
    if (!brightnessSubMenu) {
        return;
    }

    g_brightnessSubMenu = brightnessSubMenu;

    RECT itemRect{};
    if (!GetPlaceholderScreenRect(itemRect)) {
        return;
    }

    ShowBrightnessPanelAtRect(itemRect);
}

void RequestShowBrightnessPanel(HWND owner, HMENU brightnessSubMenu) {
    g_pendingSubMenu = brightnessSubMenu;
    g_brightnessSubMenu = brightnessSubMenu;
    PostMessageW(owner, WM_TRAY_BRIGHTNESS_SHOW, 0, 0);
}

void HideBrightnessPanel() {
    FlushBrightnessApply();
    g_dragging = false;
    g_popupVisible = false;
    g_lastPanelScreenRect = {};
    g_brightnessSubMenu = nullptr;
    g_pendingSubMenu = nullptr;
    UninstallInputHooks();
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

    PaintPlaceholderBackground(draw->hDC, draw->rcItem);

    RECT screenRect{};
    if (ScreenRectFromDrawItem(draw, screenRect)) {
        ShowBrightnessPanelAtRect(screenRect);
    }
}

void SyncBrightnessPanelAutoState() {
    if (!g_popup || !IsWindowVisible(g_popup) || g_dragging) {
        return;
    }
    SendMessageW(g_popup, WM_BRIGHTNESS_SYNC, 0, 0);
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
