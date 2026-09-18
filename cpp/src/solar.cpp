#include "solar.h"

#include "config.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>

namespace {

constexpr double kPi = 3.14159265358979323846;

struct LocalDateTime {
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int utcOffsetMinutes = 0;
};

int GetLocalUtcOffsetMinutes(const std::tm& local) {
#ifdef _WIN32
    TIME_ZONE_INFORMATION tzi{};
    if (GetTimeZoneInformation(&tzi) == TIME_ZONE_ID_INVALID) {
        return 0;
    }
    const LONG activeBias = (local.tm_isdst > 0) ? tzi.DaylightBias : tzi.StandardBias;
    return static_cast<int>(-(tzi.Bias + activeBias));
#elif defined(__APPLE__)
    return static_cast<int>(local.tm_gmtoff / 60);
#else
#if defined(_GNU_SOURCE) || defined(__USE_MISC)
    return static_cast<int>(local.tm_gmtoff / 60);
#else
    return 0;
#endif
#endif
}

LocalDateTime GetLocalDateTime() {
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    return {
        local.tm_year + 1900,
        local.tm_mon + 1,
        local.tm_mday,
        local.tm_hour,
        local.tm_min,
        GetLocalUtcOffsetMinutes(local),
    };
}

int DayOfYear(int year, int month, int day) {
    static const int daysBeforeMonth[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int doy = daysBeforeMonth[month - 1] + day;
    const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    if (month > 2 && leap) {
        ++doy;
    }
    return doy;
}

double Rad(double degrees) {
    return degrees * kPi / 180.0;
}

double FixAngle(double degrees) {
    double angle = std::fmod(degrees, 360.0);
    if (angle < 0.0) {
        angle += 360.0;
    }
    return angle;
}

int NormalizeMinutes(double minutes) {
    while (minutes < 0.0) {
        minutes += 24.0 * 60.0;
    }
    while (minutes >= 24.0 * 60.0) {
        minutes -= 24.0 * 60.0;
    }
    return static_cast<int>(minutes + 0.5);
}

}  // namespace

int GetCurrentUtcOffsetMinutes() {
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    return GetLocalUtcOffsetMinutes(local);
}

SolarTimes ComputeSolarTimes(
    double latitude,
    double longitude,
    int year,
    int month,
    int day) {
    SolarTimes result{};

    const int doy = DayOfYear(year, month, day);
    const double latRad = Rad(latitude);
    const double lngHour = longitude / 15.0;

    const double t = static_cast<double>(doy) + ((6.0 - lngHour) / 24.0);
    const double m = (0.9856 * t) - 3.289;
    const double l = m + (1.916 * std::sin(Rad(m))) + (0.020 * std::sin(Rad(2.0 * m))) + 282.634;
    const double lNorm = FixAngle(l);

    double raDegrees = std::atan(0.91764 * std::tan(Rad(lNorm))) * 180.0 / kPi;
    raDegrees = FixAngle(raDegrees);
    if (lNorm >= 90.0 && lNorm < 270.0) {
        raDegrees += 180.0;
    }
    raDegrees = FixAngle(raDegrees);

    const double eqTimeMinutes = 4.0 * (lNorm - raDegrees);

    const double sinDec = 0.39782 * std::sin(Rad(lNorm));
    const double cosDec = std::cos(std::asin(sinDec));
    const double cosH = (std::cos(Rad(90.833)) - (sinDec * std::sin(latRad))) / (cosDec * std::cos(latRad));
    if (cosH > 1.0 || cosH < -1.0) {
        result.sunriseMinutes = 6 * 60;
        result.sunsetMinutes = 18 * 60;
        return result;
    }

    const double hourAngleDegrees = std::acos(cosH) * 180.0 / kPi;
    result.sunriseMinutes = NormalizeMinutes(720.0 - (4.0 * (longitude + hourAngleDegrees)) - eqTimeMinutes);
    result.sunsetMinutes = NormalizeMinutes(720.0 - (4.0 * (longitude - hourAngleDegrees)) - eqTimeMinutes);
    return result;
}

SolarTimes ApplyLocalUtcOffset(SolarTimes times, int offsetMinutes) {
    times.sunriseMinutes = NormalizeMinutes(static_cast<double>(times.sunriseMinutes + offsetMinutes));
    times.sunsetMinutes = NormalizeMinutes(static_cast<double>(times.sunsetMinutes + offsetMinutes));
    return times;
}

bool IsDarkWithOffsets(
    double latitude,
    double longitude,
    int darkOffsetMinutes,
    int lightOffsetMinutes,
    SolarTimes* outTimes) {
    const LocalDateTime now = GetLocalDateTime();

    SolarTimes times = ComputeSolarTimes(
        latitude,
        longitude,
        now.year,
        now.month,
        now.day);
    times = ApplyLocalUtcOffset(times, now.utcOffsetMinutes);

    if (outTimes) {
        *outTimes = times;
    }

    const int nowMinutes = now.hour * 60 + now.minute;
    const int darkStart = times.sunsetMinutes + darkOffsetMinutes;
    const int lightStart = times.sunriseMinutes - lightOffsetMinutes;

    if (lightStart <= darkStart) {
        return nowMinutes >= darkStart || nowMinutes < lightStart;
    }

    return nowMinutes >= darkStart && nowMinutes < lightStart;
}

bool IsDark(
    double latitude,
    double longitude,
    const AppConfig& config,
    SolarTimes* outTimes) {
    return IsDarkWithOffsets(
        latitude,
        longitude,
        config.darkOffsetMinutes,
        config.lightOffsetMinutes,
        outTimes);
}

SunPosition ComputeSunPosition(
    double latitude,
    double longitude,
    int year,
    int month,
    int day,
    int hour,
    int minute,
    int utcOffsetMinutes) {
    SunPosition result{};

    int civilMinutes = hour * 60 + minute;
    int utcMinutes = civilMinutes - utcOffsetMinutes;
    while (utcMinutes < 0) {
        utcMinutes += 24 * 60;
    }
    while (utcMinutes >= 24 * 60) {
        utcMinutes -= 24 * 60;
    }
    hour = utcMinutes / 60;
    minute = utcMinutes % 60;

    const int doy = DayOfYear(year, month, day);
    const double latRad = Rad(latitude);
    const double lngHour = longitude / 15.0;
    const double timeDecimal = static_cast<double>(hour) + static_cast<double>(minute) / 60.0;

    const double t = static_cast<double>(doy) + ((timeDecimal / 24.0) - (lngHour / 24.0));
    const double m = (0.9856 * t) - 3.289;
    const double l = m + (1.916 * std::sin(Rad(m))) + (0.020 * std::sin(Rad(2.0 * m))) + 282.634;
    const double lNorm = FixAngle(l);

    double raDegrees = std::atan(0.91764 * std::tan(Rad(lNorm))) * 180.0 / kPi;
    raDegrees = FixAngle(raDegrees);
    if (lNorm >= 90.0 && lNorm < 270.0) {
        raDegrees += 180.0;
    }
    raDegrees = FixAngle(raDegrees);

    const double eqTimeMinutes = 4.0 * (lNorm - raDegrees);
    const double sinDec = 0.39782 * std::sin(Rad(lNorm));
    const double cosDec = std::cos(std::asin(sinDec));

    double trueSolarMinutes = timeDecimal * 60.0 + eqTimeMinutes + 4.0 * longitude;
    trueSolarMinutes = std::fmod(trueSolarMinutes, 1440.0);
    if (trueSolarMinutes < 0.0) {
        trueSolarMinutes += 1440.0;
    }

    const double hourAngleDeg = (trueSolarMinutes / 4.0) - 180.0;
    const double hourAngleRad = Rad(hourAngleDeg);

    const double cosZenith = sinDec * std::sin(latRad) + cosDec * std::cos(latRad) * std::cos(hourAngleRad);
    const double zenithRad = std::acos(std::clamp(cosZenith, -1.0, 1.0));
    result.elevationDegrees = 90.0 - (zenithRad * 180.0 / kPi);

    const double sinAz = -std::sin(hourAngleRad) * cosDec;
    const double cosAz = (std::cos(hourAngleRad) * cosDec * std::sin(latRad)) - (sinDec * std::cos(latRad));
    result.azimuthDegrees = FixAngle(std::atan2(sinAz, cosAz) * 180.0 / kPi);

    return result;
}

double WindowSunExposure(
    double sunAzimuth,
    double sunElevation,
    double windowAzimuthDegrees,
    double glareWeight) {
    if (sunElevation <= 0.0) {
        return 0.0;
    }

    glareWeight = std::clamp(glareWeight, 0.0, 1.0);

    double angleDiff = std::fabs(FixAngle(sunAzimuth - windowAzimuthDegrees));
    if (angleDiff > 180.0) {
        angleDiff = 360.0 - angleDiff;
    }

    // cos(0)=1 when the sun shines into the window; cos(180)=-1 when it is behind.
    const double cosDiff = std::cos(Rad(angleDiff));
    const double intoWindow = (cosDiff + 1.0) * 0.5;

    // Fade only when the sun is barely above the horizon.
    constexpr double kMinElevationForFullExposure = 5.0;
    const double lowSunFade = std::clamp(sunElevation / kMinElevationForFullExposure, 0.0, 1.0);

    (void)glareWeight;
    return std::clamp(intoWindow * lowSunFade, 0.0, 1.0);
}
