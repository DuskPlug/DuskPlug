#pragma once

#include "config.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>

// Shows the DuskPlug settings dialog. Returns false if cancelled.
bool ShowSettingsDialog(HWND owner, const std::wstring& configPath, AppConfig& config);
