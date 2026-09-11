#include "../src/config.h"
#include "../src/schedule.h"
#include "test_assert.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>

TEST(LoadConfigWithScheduleFields) {
    const std::wstring path = L"./smarttray_config_test.json";
    const std::string json = R"({
  "ClientId": "id",
  "ClientSecret": "secret",
  "DeviceId": "12345678901234567890",
  "BaseUrl": "https://openapi.tuyaeu.com",
  "ScheduleOnTime": "18:00",
  "ScheduleOffTime": "23:00"
})";

    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    EXPECT_TRUE(file != INVALID_HANDLE_VALUE);
    DWORD written = 0;
    WriteFile(file, json.data(), static_cast<DWORD>(json.size()), &written, nullptr);
    CloseHandle(file);

    AppConfig config{};
    std::wstring error;
    EXPECT_TRUE(LoadConfig(path, config, error));
    EXPECT_EQ(config.scheduleOnTime, "18:00");
    EXPECT_EQ(config.scheduleOffTime, "23:00");
    EXPECT_TRUE(config.hasScheduleTimes);

    int onMinutes = 0;
    EXPECT_TRUE(ParseTimeHHMM(config.scheduleOnTime, onMinutes));
    DeleteFileW(path.c_str());
}

void RunConfigTests() {
    std::printf("config tests\n");
    RUN_TEST(LoadConfigWithScheduleFields);
}
