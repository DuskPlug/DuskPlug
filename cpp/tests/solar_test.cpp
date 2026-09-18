#include "../src/solar.h"
#include "test_assert.h"

TEST(ComputeSolarTimesReasonable) {
    const SolarTimes times = ComputeSolarTimes(51.5, -0.1, 2026, 6, 21);
    EXPECT_TRUE(times.sunriseMinutes > 3 * 60);
    EXPECT_TRUE(times.sunsetMinutes < 22 * 60);
    EXPECT_TRUE(times.sunsetMinutes > times.sunriseMinutes);
}

TEST(SolarWinterDayShorterThanSummer) {
    const SolarTimes winter = ComputeSolarTimes(51.5, -0.1, 2026, 1, 15);
    const SolarTimes summer = ComputeSolarTimes(51.5, -0.1, 2026, 6, 21);
    const int winterDaylight = winter.sunsetMinutes - winter.sunriseMinutes;
    const int summerDaylight = summer.sunsetMinutes - summer.sunriseMinutes;
    EXPECT_TRUE(summerDaylight > winterDaylight);
}

TEST(EastWindowMorningExposureHigherThanMidday) {
    const SunPosition morning = ComputeSunPosition(51.5, -0.1, 2026, 6, 21, 8, 0, 60);
    const SunPosition midday = ComputeSunPosition(51.5, -0.1, 2026, 6, 21, 12, 0, 60);
    const double morningExposure = WindowSunExposure(morning.azimuthDegrees, morning.elevationDegrees, 90.0);
    const double middayExposure = WindowSunExposure(midday.azimuthDegrees, midday.elevationDegrees, 90.0);
    EXPECT_TRUE(morningExposure > middayExposure);
}

TEST(SunBehindWindowLowerExposureThanIntoWindow) {
    const SunPosition morning = ComputeSunPosition(51.5, -0.1, 2026, 9, 18, 9, 50, 60);
    const SunPosition evening = ComputeSunPosition(51.5, -0.1, 2026, 9, 18, 18, 0, 60);
    const double intoWindow = WindowSunExposure(morning.azimuthDegrees, morning.elevationDegrees, 90.0);
    const double behindWindow = WindowSunExposure(evening.azimuthDegrees, evening.elevationDegrees, 90.0);
    EXPECT_TRUE(intoWindow > behindWindow);
}

TEST(EastWindowMorningNearDayBrightness) {
    constexpr double kCardiffLat = 51.4816;
    constexpr double kCardiffLng = -3.1791;
    const SunPosition morning = ComputeSunPosition(kCardiffLat, kCardiffLng, 2026, 9, 18, 9, 50, 60);
    const double exposure = WindowSunExposure(
        morning.azimuthDegrees,
        morning.elevationDegrees,
        90.0);
    EXPECT_TRUE(exposure > 0.85);
}

TEST(NightExposureNearZero) {
    const SunPosition night = ComputeSunPosition(51.5, -0.1, 2026, 6, 21, 2, 0);
    const double exposure = WindowSunExposure(night.azimuthDegrees, night.elevationDegrees, 90.0);
    EXPECT_TRUE(exposure < 0.01);
}

TEST(CardiffSunsetSeptemberWithBstOffset) {
    constexpr double kCardiffLat = 51.4816;
    constexpr double kCardiffLng = -3.1791;
    SolarTimes times = ComputeSolarTimes(kCardiffLat, kCardiffLng, 2026, 9, 15);
    times = ApplyLocalUtcOffset(times, 60);
    EXPECT_TRUE(times.sunsetMinutes >= 19 * 60 + 12);
    EXPECT_TRUE(times.sunsetMinutes <= 19 * 60 + 40);
}

TEST(CardiffSunsetJanuaryWithoutDstOffset) {
    constexpr double kCardiffLat = 51.4816;
    constexpr double kCardiffLng = -3.1791;
    const SolarTimes times = ComputeSolarTimes(kCardiffLat, kCardiffLng, 2026, 1, 15);
    EXPECT_TRUE(times.sunsetMinutes >= 16 * 60 + 20);
    EXPECT_TRUE(times.sunsetMinutes <= 16 * 60 + 45);
}

void RunSolarTests() {
    std::printf("solar tests\n");
    RUN_TEST(ComputeSolarTimesReasonable);
    RUN_TEST(SolarWinterDayShorterThanSummer);
    RUN_TEST(EastWindowMorningExposureHigherThanMidday);
    RUN_TEST(SunBehindWindowLowerExposureThanIntoWindow);
    RUN_TEST(EastWindowMorningNearDayBrightness);
    RUN_TEST(NightExposureNearZero);
    RUN_TEST(CardiffSunsetSeptemberWithBstOffset);
    RUN_TEST(CardiffSunsetJanuaryWithoutDstOffset);
}
