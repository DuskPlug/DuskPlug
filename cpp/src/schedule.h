#pragma once

#include <string>

bool ParseTimeHHMM(const std::string& text, int& minutesOut);
bool FormatTimeHHMM(int minutes, std::string& out);
bool ShouldBeOnForSchedule(int onMinutes, int offMinutes, int nowMinutes);
int GetLocalMinutesNow();
