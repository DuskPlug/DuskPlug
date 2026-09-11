#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// Shows a daily on/off time picker. Returns false if cancelled.
bool ShowScheduleDialog(HWND owner, int& onMinutes, int& offMinutes);
