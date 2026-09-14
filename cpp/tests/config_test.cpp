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

TEST(LoadAndSaveScreenBrightnessFields) {
    const std::string path = "./duskplug_brightness_config_test.json";
    const std::string json = R"({
  "ClientId": "id",
  "ClientSecret": "secret",
  "DeviceId": "12345678901234567890",
  "BaseUrl": "https://openapi.tuyaeu.com",
  "ScreenBrightnessNight": 15,
  "ScreenBrightnessDay": 90
})";

    EXPECT_TRUE(WriteTextFile(path, json));

    AppConfig config{};
    std::string error;
    EXPECT_TRUE(LoadConfig(path, config, error));
    EXPECT_EQ(config.screenBrightnessNight, 15);
    EXPECT_EQ(config.screenBrightnessDay, 90);

    config.screenBrightnessNight = 150;
    config.screenBrightnessDay = -5;
    EXPECT_TRUE(SaveAppConfig(path, config));

    AppConfig reloaded{};
    EXPECT_TRUE(LoadConfig(path, reloaded, error));
    EXPECT_EQ(reloaded.screenBrightnessNight, 100);
    EXPECT_EQ(reloaded.screenBrightnessDay, 0);

    remove(path.c_str());
}

TEST(ClampScreenBrightnessPercentValues) {
    EXPECT_EQ(ClampScreenBrightnessPercent(-10), 0);
    EXPECT_EQ(ClampScreenBrightnessPercent(0), 0);
    EXPECT_EQ(ClampScreenBrightnessPercent(50), 50);
    EXPECT_EQ(ClampScreenBrightnessPercent(100), 100);
    EXPECT_EQ(ClampScreenBrightnessPercent(140), 100);
}

TEST(MigrateLegacyDeviceConfig) {
    const std::string path = "./duskplug_legacy_device_config_test.json";
    const std::string json = R"({
  "ClientId": "id",
  "ClientSecret": "secret",
  "DeviceId": "12345678901234567890",
  "BaseUrl": "https://openapi.tuyaeu.com",
  "SwitchCode": "switch_1",
  "ScheduleOnTime": "18:00",
  "ScheduleOffTime": "23:00",
  "DarkOffsetMinutes": 5,
  "LightOffsetMinutes": 10
})";

    EXPECT_TRUE(WriteTextFile(path, json));

    AppConfig config{};
    std::string error;
    EXPECT_TRUE(LoadConfig(path, config, error));
    EXPECT_EQ(config.devices.size(), 1u);
    EXPECT_EQ(config.devices[0].id, "12345678901234567890");
    EXPECT_EQ(config.devices[0].capabilities.switchCode, "switch_1");
    EXPECT_EQ(config.devices[0].automation.darkOffsetMinutes, 5);
    EXPECT_EQ(config.devices[0].automation.lightOffsetMinutes, 10);
    EXPECT_EQ(config.deviceId, "12345678901234567890");

    remove(path.c_str());
}

TEST(LoadAndSaveMultiDeviceConfig) {
    const std::string path = "./duskplug_multi_device_config_test.json";
    const std::string json = R"({
  "ClientId": "id",
  "ClientSecret": "secret",
  "BaseUrl": "https://openapi.tuyaeu.com",
  "Devices": [
    {
      "Id": "plug123456789012345678",
      "Name": "Hall plug",
      "Type": "plug",
      "Enabled": true,
      "Capabilities": { "switch": "switch_1" },
      "Automation": {
        "mode": "schedule",
        "scheduleOnTime": "17:30",
        "scheduleOffTime": "22:30",
        "darkOffsetMinutes": 0,
        "lightOffsetMinutes": 0,
        "nightBrightness": 20,
        "dayBrightness": 80,
        "useBrightness": false
      }
    },
    {
      "Id": "bulb123456789012345678",
      "Name": "Living room",
      "Type": "bulb",
      "Enabled": true,
      "Capabilities": {
        "switch": "switch_led",
        "brightness": "bright_value_v2",
        "brightnessMin": 10,
        "brightnessMax": 1000
      },
      "Automation": {
        "mode": "smart",
        "scheduleOnTime": "18:00",
        "scheduleOffTime": "23:00",
        "darkOffsetMinutes": 15,
        "lightOffsetMinutes": 5,
        "nightBrightness": 25,
        "dayBrightness": 90,
        "useBrightness": true
      }
    }
  ],
  "Latitude": 53.48,
  "Longitude": -2.24
})";

    EXPECT_TRUE(WriteTextFile(path, json));

    AppConfig config{};
    std::string error;
    EXPECT_TRUE(LoadConfig(path, config, error));
    EXPECT_EQ(config.devices.size(), 2u);
    EXPECT_EQ(config.devices[1].type, DeviceType::Bulb);
    EXPECT_EQ(config.devices[1].capabilities.brightnessCode, "bright_value_v2");
    EXPECT_EQ(config.devices[1].automation.mode, DeviceAutomationMode::Smart);

    EXPECT_TRUE(SaveAppConfig(path, config));

    AppConfig reloaded{};
    EXPECT_TRUE(LoadConfig(path, reloaded, error));
    EXPECT_EQ(reloaded.devices.size(), 2u);
    EXPECT_EQ(reloaded.devices[1].name, "Living room");
    EXPECT_EQ(reloaded.devices[1].automation.nightBrightness, 25);

    remove(path.c_str());
}

void RunConfigTests() {
    std::printf("config tests\n");
    RUN_TEST(LoadConfigWithScheduleFields);
    RUN_TEST(LoadAndSaveScreenBrightnessFields);
    RUN_TEST(ClampScreenBrightnessPercentValues);
    RUN_TEST(MigrateLegacyDeviceConfig);
    RUN_TEST(LoadAndSaveMultiDeviceConfig);
}
