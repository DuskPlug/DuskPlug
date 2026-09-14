#include "schedule.h"

#include <cctype>
#include <cstdio>
#include <ctime>

bool FormatTimeHHMM(int minutes, std::string& out) {
    if (minutes < 0 || minutes >= 24 * 60) {
        return false;
    }

    char buffer[8];
    snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes / 60, minutes % 60);
    out = buffer;
    return true;
}

bool ParseTimeHHMM(const std::string& text, int& minutesOut) {
    if (text.size() < 4) {
        return false;
    }

    const size_t colon = text.find(':');
    if (colon == std::string::npos || colon == 0 || colon >= text.size() - 1) {
        return false;
    }

    int hour = 0;
    int minute = 0;
    for (size_t i = 0; i < colon; ++i) {
        if (!std::isdigit(static_cast<unsigned char>(text[i]))) {
            return false;
        }
        hour = hour * 10 + (text[i] - '0');
    }
    for (size_t i = colon + 1; i < text.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(text[i]))) {
            return false;
        }
        minute = minute * 10 + (text[i] - '0');
    }

    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        return false;
    }

    minutesOut = hour * 60 + minute;
    return true;
}

bool ShouldBeOnForSchedule(int onMinutes, int offMinutes, int nowMinutes) {
    if (onMinutes == offMinutes) {
        return false;
    }

    if (onMinutes < offMinutes) {
        return nowMinutes >= onMinutes && nowMinutes < offMinutes;
    }

    return nowMinutes >= onMinutes || nowMinutes < offMinutes;
}

int GetLocalMinutesNow() {
    const std::time_t now = std::time(nullptr);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    return localTime.tm_hour * 60 + localTime.tm_min;
}
