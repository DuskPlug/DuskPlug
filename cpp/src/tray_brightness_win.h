#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <functional>

struct TrayBrightnessCallbacks {
    std::function<bool()> isAvailable;
    std::function<int()> getPercent;
    std::function<int()> getHardwarePercent;
    std::function<void(int)> setPercent;
    std::function<bool()> isAutoEnabled;
    std::function<void(bool)> setAutoEnabled;
};

constexpr UINT WM_TRAY_BRIGHTNESS_SHOW = WM_APP + 21;
constexpr UINT CMD_SCREEN_BRIGHTNESS_PLACEHOLDER = 10012;

void InitTrayBrightnessUi(HWND owner, const TrayBrightnessCallbacks& callbacks);
void ShutdownTrayBrightnessUi();
void OnTrayContextMenuOpening();
void MeasureBrightnessPlaceholderItem(MEASUREITEMSTRUCT* measure);
void DrawBrightnessPlaceholderItem(const DRAWITEMSTRUCT* draw);
void RequestShowBrightnessPanel(HWND owner, HMENU brightnessSubMenu);
void HideBrightnessPanel();
void SyncBrightnessPanelAutoState();
bool HandleTrayBrightnessMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT& result);
