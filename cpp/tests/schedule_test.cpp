#include "../src/schedule.h"
#include "test_assert.h"

TEST(ScheduleSameDayWindow) {
    EXPECT_TRUE(ShouldBeOnForSchedule(18 * 60, 23 * 60, 19 * 60));
    EXPECT_FALSE(ShouldBeOnForSchedule(18 * 60, 23 * 60, 17 * 60));
    EXPECT_FALSE(ShouldBeOnForSchedule(18 * 60, 23 * 60, 23 * 60));
}

TEST(ScheduleOvernightWindow) {
    EXPECT_TRUE(ShouldBeOnForSchedule(22 * 60, 6 * 60, 23 * 60));
    EXPECT_TRUE(ShouldBeOnForSchedule(22 * 60, 6 * 60, 3 * 60));
    EXPECT_FALSE(ShouldBeOnForSchedule(22 * 60, 6 * 60, 12 * 60));
}

TEST(ParseTimeHHMMValid) {
    int minutes = 0;
    EXPECT_TRUE(ParseTimeHHMM("18:00", minutes));
    EXPECT_EQ(minutes, 18 * 60);
    EXPECT_TRUE(ParseTimeHHMM("06:30", minutes));
    EXPECT_EQ(minutes, 6 * 60 + 30);
}

TEST(ParseTimeHHMMInvalid) {
    int minutes = 0;
    EXPECT_FALSE(ParseTimeHHMM("invalid", minutes));
    EXPECT_FALSE(ParseTimeHHMM("25:00", minutes));
}

TEST(FormatTimeHHMMRoundTrip) {
    std::string formatted;
    EXPECT_TRUE(FormatTimeHHMM(18 * 60 + 30, formatted));
    EXPECT_EQ(formatted, "18:30");
    int minutes = 0;
    EXPECT_TRUE(ParseTimeHHMM(formatted, minutes));
    EXPECT_EQ(minutes, 18 * 60 + 30);
}

void RunScheduleTests() {
    std::printf("schedule tests\n");
    RUN_TEST(ScheduleSameDayWindow);
    RUN_TEST(ScheduleOvernightWindow);
    RUN_TEST(ParseTimeHHMMValid);
    RUN_TEST(ParseTimeHHMMInvalid);
    RUN_TEST(FormatTimeHHMMRoundTrip);
}
