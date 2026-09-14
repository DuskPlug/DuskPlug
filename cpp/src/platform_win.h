#pragma once

#include "activity_tracker.h"
#include "brightness.h"
#include "location_service.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#ifdef DeviceCapabilities
#undef DeviceCapabilities
#endif

#include <string>

ILocationService* CreateWinLocationService(HWND hwnd, const std::wstring& appDir);
IActivityTracker* CreateWinActivityTracker(HWND hwnd);
IBrightnessController* CreateWinBrightnessController();
