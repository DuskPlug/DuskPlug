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

constexpr int kPopupHeight = 56;
constexpr UINT WM_BRIGHTNESS_SYNC = WM_APP + 20;

TrayBrightnessCallbacks g_callbacks{};

HWND g_owner = nullptr;

HWND g_popup = nullptr;

bool g_classRegistered = false;

bool g_updatingTrack = false;

bool g_dragging = false;

void UpdateLabelText();

int PercentFromTrackPoint(HWND track, int clientX) {

    RECT rc{};

    GetClientRect(track, &rc);

    const int width = rc.right - rc.left;

    if (width <= 0) {

        return 0;

    }

    int percent = (clientX * 100) / width;

    if (percent < 0) {

        percent = 0;

    } else if (percent > 100) {

        percent = 100;

    }

    return percent;

}



void ApplyTrackPercent(int percent, bool notify) {

    if (!g_popup) {

        return;

    }

    HWND track = GetDlgItem(g_popup, kTrackId);

    if (!track) {

        return;

    }



    g_updatingTrack = true;

    SendMessageW(track, TBM_SETPOS, TRUE, percent);

    g_updatingTrack = false;



    if (notify && g_callbacks.setPercent) {

        g_callbacks.setPercent(percent);

    }

    UpdateLabelText();

}



void UpdateLabelText() {

    if (!g_popup) {

        return;

    }

    HWND label = GetDlgItem(g_popup, kLabelId);

    if (!label) {

        return;

    }

    const int percent = static_cast<int>(SendMessageW(GetDlgItem(g_popup, kTrackId), TBM_GETPOS, 0, 0));

    wchar_t text[32];

    _snwprintf(text, 32, L"Brightness: %d%%", percent);

    SetWindowTextW(label, text);

}



bool TrackContainsPoint(int x, int y) {

    HWND track = GetDlgItem(g_popup, kTrackId);

    if (!track) {

        return false;

    }

    RECT rc{};

    GetWindowRect(track, &rc);

    POINT pt{x, y};

    return PtInRect(&rc, pt) != FALSE;

}



void SetPercentFromScreenPoint(int screenX, int screenY, bool notify) {

    HWND track = GetDlgItem(g_popup, kTrackId);

    if (!track) {

        return;

    }

    POINT pt{screenX, screenY};

    ScreenToClient(track, &pt);

    ApplyTrackPercent(PercentFromTrackPoint(track, pt.x), notify);

}



LRESULT CALLBACK BrightnessPopupProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {

    switch (msg) {

    case WM_HSCROLL:

        if (reinterpret_cast<HWND>(lParam) == GetDlgItem(hwnd, kTrackId)) {

            if (!g_updatingTrack && g_callbacks.setPercent) {

                const int percent = static_cast<int>(SendMessageW(

                    reinterpret_cast<HWND>(lParam),

                    TBM_GETPOS,

                    0,

                    0));

                g_callbacks.setPercent(percent);

            }

            UpdateLabelText();

        }

        return 0;

    case WM_LBUTTONDOWN: {

        const int x = GET_X_LPARAM(lParam);

        const int y = GET_Y_LPARAM(lParam);

        POINT screenPt{x, y};

        ClientToScreen(hwnd, &screenPt);

        if (TrackContainsPoint(screenPt.x, screenPt.y)) {

            g_dragging = true;

            SetCapture(hwnd);

            SetPercentFromScreenPoint(screenPt.x, screenPt.y, true);

        }

        return 0;

    }

    case WM_MOUSEMOVE:

        if (g_dragging && (wParam & MK_LBUTTON)) {

            const int x = GET_X_LPARAM(lParam);

            const int y = GET_Y_LPARAM(lParam);

            POINT screenPt{x, y};

            ClientToScreen(hwnd, &screenPt);

            SetPercentFromScreenPoint(screenPt.x, screenPt.y, true);

        }

        return 0;

    case WM_LBUTTONUP:

        if (g_dragging) {

            g_dragging = false;

            ReleaseCapture();

        }

        return 0;

    case WM_CAPTURECHANGED:

        g_dragging = false;

        return 0;

    case WM_ACTIVATE:

        if (LOWORD(wParam) == WA_INACTIVE) {

            ShowWindow(hwnd, SW_HIDE);

        }

        return 0;

    case WM_BRIGHTNESS_SYNC:

        if (g_popup) {

            const int percent = g_callbacks.getPercent ? g_callbacks.getPercent() : 50;

            ApplyTrackPercent(percent, false);

        }

        return 0;

    default:

        return DefWindowProcW(hwnd, msg, wParam, lParam);

    }

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

        WS_POPUP | WS_BORDER,

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

}



}  // namespace



void InitTrayBrightnessUi(HWND owner, const TrayBrightnessCallbacks& callbacks) {

    g_owner = owner;

    g_callbacks = callbacks;

    INITCOMMONCONTROLSEX icc{};

    icc.dwSize = sizeof(icc);

    icc.dwICC = ICC_BAR_CLASSES;

    InitCommonControlsEx(&icc);

    EnsurePopupCreated();

}



void ShutdownTrayBrightnessUi() {

    HideTrayBrightnessSlider();

    if (g_popup) {

        DestroyWindow(g_popup);

        g_popup = nullptr;

    }

    g_owner = nullptr;

    g_callbacks = {};

}



void ShowTrayBrightnessSlider(HWND owner, HMENU parentMenu, UINT brightnessMenuItemId) {

    if (!g_callbacks.isAvailable || !g_callbacks.isAvailable()) {

        return;

    }



    EnsurePopupCreated();

    if (!g_popup || !parentMenu) {

        return;

    }



    RECT itemRect{};

    if (!GetMenuItemRect(owner, parentMenu, brightnessMenuItemId, &itemRect)) {

        return;

    }



    SendMessageW(g_popup, WM_BRIGHTNESS_SYNC, 0, 0);



    const int x = itemRect.left - kPopupWidth - 8;

    const int y = itemRect.top;

    SetWindowPos(g_popup, HWND_TOPMOST, x, y, kPopupWidth, kPopupHeight, SWP_SHOWWINDOW | SWP_NOACTIVATE);

}



void HideTrayBrightnessSlider() {

    if (g_popup) {

        g_dragging = false;

        ShowWindow(g_popup, SW_HIDE);

    }

}



bool HandleTrayBrightnessMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT& result) {

    (void)hwnd;

    (void)wParam;

    (void)lParam;

    (void)result;

    if (msg == WM_EXITMENULOOP || msg == WM_CANCELMODE) {

        HideTrayBrightnessSlider();

    }

    return false;

}


