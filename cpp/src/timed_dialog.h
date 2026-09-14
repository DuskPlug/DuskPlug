#pragma once

#ifndef _WIN32
#error "timed_dialog is Windows-only"
#endif

#include <windows.h>

bool PromptTimedMinutes(HWND owner, int& minutes);
