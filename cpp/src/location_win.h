#pragma once

#include "config.h"
#include "location_service.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>

enum class LocationPromptChoice {
    OpenSettings,
    UseConfigFallback,
    RunSetupLocation,
    Cancelled,
};

enum class LocationResolveResult {
    Success,
    OpenedSettings,
    Cancelled,
    Failed,
};

bool ResolveLocation(
    HWND hwnd,
    const AppConfig& config,
    const std::wstring& appDir,
    GeoLocation& out,
    std::wstring& error);

LocationResolveResult ResolveLocationWithPrompt(
    HWND hwnd,
    const AppConfig& config,
    const std::wstring& appDir,
    GeoLocation& out,
    std::wstring& error);

void OpenWindowsLocationSettings();
void RunSetupLocationScript(const std::wstring& appDir);
bool RequestWindowsLocation(HWND hwnd, double& latitude, double& longitude);
