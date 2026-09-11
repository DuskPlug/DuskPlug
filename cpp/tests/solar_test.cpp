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

void RunSolarTests() {
    std::printf("solar tests\n");
    RUN_TEST(ComputeSolarTimesReasonable);
    RUN_TEST(SolarWinterDayShorterThanSummer);
}
