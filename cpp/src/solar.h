#pragma once

#include "config.h"

struct SolarTimes {
    int sunriseMinutes = 0;
    int sunsetMinutes = 0;
};

struct SunPosition {
    double azimuthDegrees = 0.0;   // 0 = north, 90 = east, clockwise
    double elevationDegrees = 0.0;   // above horizon
};

SolarTimes ComputeSolarTimes(
    double latitude,
    double longitude,
    int year,
    int month,
    int day);

// Shift NOAA-formula solar minutes into civil local clock time.
SolarTimes ApplyLocalUtcOffset(SolarTimes times, int offsetMinutes);

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

SunPosition ComputeSunPosition(
    double latitude,
    double longitude,
    int year,
    int month,
    int day,
    int hour,
    int minute);

double WindowSunExposure(
    double sunAzimuth,
    double sunElevation,
    double windowAzimuthDegrees,
    double glareWeight = 0.6);
