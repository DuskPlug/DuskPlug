#pragma once

#include "config.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>

// Shows the DuskPlug settings dialog. Returns false if cancelled.
// If settings is already open, brings the existing window to the front and returns false.
bool ShowSettingsDialog(HWND owner, const std::wstring& configPath, AppConfig& config);

bool IsSettingsDialogOpen();
void FocusSettingsDialog();
