#include "solar.h"

#include "config.h"

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
};

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

bool IsDarkWithOffsets(
    double latitude,
    double longitude,
    int darkOffsetMinutes,
    int lightOffsetMinutes,
    SolarTimes* outTimes) {
    const LocalDateTime now = GetLocalDateTime();

    const SolarTimes times = ComputeSolarTimes(
        latitude,
        longitude,
        now.year,
        now.month,
        now.day);

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
