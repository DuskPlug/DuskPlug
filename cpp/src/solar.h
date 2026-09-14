#pragma once

#include "config.h"

struct SolarTimes {
    int sunriseMinutes = 0;
    int sunsetMinutes = 0;
};

SolarTimes ComputeSolarTimes(
    double latitude,
    double longitude,
    int year,
    int month,
    int day);

bool IsDark(
    double latitude,
    double longitude,
    const AppConfig& config,
    SolarTimes* outTimes = nullptr);

bool IsDarkWithOffsets(
    double latitude,
    double longitude,
    int darkOffsetMinutes,
    int lightOffsetMinutes,
    SolarTimes* outTimes = nullptr);
