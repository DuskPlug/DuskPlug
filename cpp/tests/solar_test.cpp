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
    const SunPosition morning = ComputeSunPosition(51.5, -0.1, 2026, 6, 21, 8, 0);
    const SunPosition midday = ComputeSunPosition(51.5, -0.1, 2026, 6, 21, 12, 0);
    const double morningExposure = WindowSunExposure(morning.azimuthDegrees, morning.elevationDegrees, 90.0);
    const double middayExposure = WindowSunExposure(midday.azimuthDegrees, midday.elevationDegrees, 90.0);
    EXPECT_TRUE(morningExposure > middayExposure);
}

TEST(WinterMiddayLowerExposureThanSummer) {
    const SunPosition winter = ComputeSunPosition(51.5, -0.1, 2026, 1, 15, 12, 0);
    const SunPosition summer = ComputeSunPosition(51.5, -0.1, 2026, 6, 21, 12, 0);
    const double winterExposure = WindowSunExposure(winter.azimuthDegrees, winter.elevationDegrees, 90.0);
    const double summerExposure = WindowSunExposure(summer.azimuthDegrees, summer.elevationDegrees, 90.0);
    EXPECT_TRUE(summerExposure > winterExposure);
}

TEST(NightExposureNearZero) {
    const SunPosition night = ComputeSunPosition(51.5, -0.1, 2026, 6, 21, 2, 0);
    const double exposure = WindowSunExposure(night.azimuthDegrees, night.elevationDegrees, 90.0);
    EXPECT_TRUE(exposure < 0.01);
}

void RunSolarTests() {
    std::printf("solar tests\n");
    RUN_TEST(ComputeSolarTimesReasonable);
    RUN_TEST(SolarWinterDayShorterThanSummer);
    RUN_TEST(EastWindowMorningExposureHigherThanMidday);
    RUN_TEST(WinterMiddayLowerExposureThanSummer);
    RUN_TEST(NightExposureNearZero);
}
