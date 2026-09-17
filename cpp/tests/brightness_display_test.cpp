#include "../src/brightness.h"
#include "test_assert.h"

TEST(DisplayedBrightnessPrefersLastAppliedOverHardware) {
    EXPECT_EQ(ResolveDisplayedScreenBrightnessPercent(20, 70, true, 20), 20);
}

TEST(DisplayedBrightnessUsesHardwareWhenNothingApplied) {
    EXPECT_EQ(ResolveDisplayedScreenBrightnessPercent(-1, 70, false, 20), 70);
}

TEST(DisplayedBrightnessUsesAutomaticTargetWhenUnknown) {
    EXPECT_EQ(ResolveDisplayedScreenBrightnessPercent(-1, -1, true, 35), 35);
}

TEST(DisplayedBrightnessFallsBackWhenManualAndUnknown) {
    EXPECT_EQ(ResolveDisplayedScreenBrightnessPercent(-1, -1, false, 35), 50);
}

TEST(InterpolatedBrightnessStartsAtFromValue) {
    EXPECT_EQ(InterpolateBrightnessPercent(20, 80, 0.0), 20);
}

TEST(InterpolatedBrightnessEndsAtToValue) {
    EXPECT_EQ(InterpolateBrightnessPercent(20, 80, 1.0), 80);
}

TEST(InterpolatedBrightnessMidpointIsBetweenEndpoints) {
    const int midpoint = InterpolateBrightnessPercent(20, 80, 0.5);
    EXPECT_TRUE(midpoint > 20 && midpoint < 80);
}

TEST(BrightnessFadeDurationScalesWithDelta) {
    EXPECT_EQ(BrightnessFadeDurationMs(20, 20), 0ULL);
    EXPECT_TRUE(BrightnessFadeDurationMs(20, 80) > BrightnessFadeDurationMs(20, 40));
    EXPECT_TRUE(BrightnessFadeDurationMs(0, 100) <= 1500ULL);
}

TEST(BrightnessDriftDetectsMismatch) {
    EXPECT_TRUE(BrightnessDriftedFromTarget(100, 30));
    EXPECT_FALSE(BrightnessDriftedFromTarget(100, 99));
    EXPECT_FALSE(BrightnessDriftedFromTarget(100, -1));
}

void RunBrightnessDisplayTests() {
    std::printf("brightness display tests\n");
    RUN_TEST(DisplayedBrightnessPrefersLastAppliedOverHardware);
    RUN_TEST(DisplayedBrightnessUsesHardwareWhenNothingApplied);
    RUN_TEST(DisplayedBrightnessUsesAutomaticTargetWhenUnknown);
    RUN_TEST(DisplayedBrightnessFallsBackWhenManualAndUnknown);
    RUN_TEST(InterpolatedBrightnessStartsAtFromValue);
    RUN_TEST(InterpolatedBrightnessEndsAtToValue);
    RUN_TEST(InterpolatedBrightnessMidpointIsBetweenEndpoints);
    RUN_TEST(BrightnessFadeDurationScalesWithDelta);
    RUN_TEST(BrightnessDriftDetectsMismatch);
}
