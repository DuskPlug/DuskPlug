#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <functional>

struct TrayBrightnessCallbacks {
    std::function<bool()> isAvailable;
    std::function<int()> getPercent;
    std::function<void(int)> setPercent;
    std::function<bool()> isAutoEnabled;
    std::function<void(bool)> setAutoEnabled;
};

void InitTrayBrightnessUi(HWND owner, const TrayBrightnessCallbacks& callbacks);
void ShutdownTrayBrightnessUi();
void ShowTrayBrightnessSlider(HWND owner, HMENU parentMenu, UINT brightnessMenuItemId);
void HideTrayBrightnessSlider();
bool HandleTrayBrightnessMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT& result);
