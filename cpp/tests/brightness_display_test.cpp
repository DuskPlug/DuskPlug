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

void RunBrightnessDisplayTests() {
    std::printf("brightness display tests\n");
    RUN_TEST(DisplayedBrightnessPrefersLastAppliedOverHardware);
    RUN_TEST(DisplayedBrightnessUsesHardwareWhenNothingApplied);
    RUN_TEST(DisplayedBrightnessUsesAutomaticTargetWhenUnknown);
    RUN_TEST(DisplayedBrightnessFallsBackWhenManualAndUnknown);
}
