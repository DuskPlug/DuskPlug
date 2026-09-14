#include "../src/config.h"
#include "../src/platform_util.h"
#include "../src/schedule.h"
#include "test_assert.h"

#include <cstdio>
#include <string>

TEST(LoadConfigWithScheduleFields) {
    const std::string path = "./duskplug_config_test.json";
    const std::string json = R"({
  "ClientId": "id",
  "ClientSecret": "secret",
  "DeviceId": "12345678901234567890",
  "BaseUrl": "https://openapi.tuyaeu.com",
  "ScheduleOnTime": "18:00",
  "ScheduleOffTime": "23:00"
})";

    EXPECT_TRUE(WriteTextFile(path, json));

    AppConfig config{};
    std::string error;
    EXPECT_TRUE(LoadConfig(path, config, error));
    EXPECT_EQ(config.scheduleOnTime, "18:00");
    EXPECT_EQ(config.scheduleOffTime, "23:00");
    EXPECT_TRUE(config.hasScheduleTimes);

    int onMinutes = 0;
    EXPECT_TRUE(ParseTimeHHMM(config.scheduleOnTime, onMinutes));

    remove(path.c_str());
}

void RunConfigTests() {
    std::printf("config tests\n");
    RUN_TEST(LoadConfigWithScheduleFields);
}
