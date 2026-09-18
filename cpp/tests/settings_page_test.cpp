#include "../src/settings_page.h"
#include "../src/config.h"
#include "test_assert.h"

#include <cstdio>
#include <string>

namespace {

const char* kSampleDeviceJson = R"(
      "id":"device",
      "name":"Test plug",
      "type":"plug",
      "enabled":true,
      "switchCode":"switch_1",
      "mode":"schedule",
      "scheduleOnTime":"22:00",
      "scheduleOffTime":"06:00",
      "darkOffsetMinutes":5,
      "lightOffsetMinutes":10,
      "nightBrightness":20,
      "dayBrightness":80,
      "useBrightness":false
)";

AppConfig MakeSampleConfig() {
    AppConfig config{};
    config.clientId = "id";
    config.clientSecret = "sec";
    config.deviceId = "dev";
    config.baseUrl = "https://openapi.tuyaeu.com";
    config.latitude = 51.5;
    config.longitude = -3.2;
    config.scheduleOnTime = "18:00";
    config.scheduleOffTime = "23:00";

    DeviceConfig device{};
    device.id = "dev";
    device.name = "Test plug";
    device.type = DeviceType::Plug;
    device.enabled = true;
    device.capabilities.switchCode = "switch_1";
    device.automation.mode = DeviceAutomationMode::Schedule;
    device.automation.scheduleOnTime = "18:00";
    device.automation.scheduleOffTime = "23:00";
    config.devices.push_back(device);
    return config;
}

}  // namespace

TEST(JsonEscapeQuotesAndTags) {
    EXPECT_EQ(JsonEscape("a\"b"), "a\\\"b");
    EXPECT_EQ(JsonEscape("<script>"), "\\u003cscript>");
}

TEST(InjectSettingsBootReplacesToken) {
    const std::string html = "boot(/*__DUSKPLUG_BOOT__*/null);";
    EXPECT_EQ(InjectSettingsBoot(html, "{\"ok\":true}"), "boot({\"ok\":true});");
}

TEST(InjectBrandMarkLeavesHtmlWhenTokenMissing) {
    const std::string html = "<img src=\"placeholder\">";
    EXPECT_EQ(InjectBrandMark(html), html);
}

TEST(PrepareSettingsHtmlAppliesBootAfterBrandMark) {
    const std::string html = "<img src=\"/*__DUSKPLUG_MARK__*/\">boot(/*__DUSKPLUG_BOOT__*/null);";
    const std::string prepared = PrepareSettingsHtml(html, "{\"ok\":true}");
    EXPECT_TRUE(prepared.find("boot({\"ok\":true});") != std::string::npos);
    EXPECT_TRUE(prepared.find("/*__DUSKPLUG_BOOT__*/") == std::string::npos);
}

TEST(BuildSettingsBootJsonContainsFields) {
    const AppConfig config = MakeSampleConfig();
    const std::string json = BuildSettingsBootJson(config, "windows");
    EXPECT_TRUE(json.find("\"platform\":\"windows\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"clientId\":\"id\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"dataCenterIndex\":0") != std::string::npos);
    EXPECT_TRUE(json.find("\"devices\":[") != std::string::npos);
    EXPECT_TRUE(json.find("\"id\":\"dev\"") != std::string::npos);
}

TEST(ApplySettingsFromJsonRequiresCredentials) {
    AppConfig config{};
    std::string error;
    EXPECT_FALSE(ApplySettingsFromJson("{\"type\":\"save\"}", config, error));
    EXPECT_TRUE(error.find("required") != std::string::npos);
}

TEST(ApplySettingsFromJsonAcceptsValidPayload) {
    AppConfig config{};
    std::string error;
    const std::string json = std::string(
        "{\"type\":\"save\",\"clientId\":\"id\",\"clientSecret\":\"secret\","
        "\"dataCenterIndex\":1,\"latitude\":51.48,\"longitude\":-3.21,"
        "\"lockOffSeconds\":30,\"screenBrightnessNight\":15,\"screenBrightnessDay\":90,"
        "\"devices\":[{") + kSampleDeviceJson + "}]}";
    EXPECT_TRUE(ApplySettingsFromJson(json, config, error));
    EXPECT_EQ(config.clientId, "id");
    EXPECT_EQ(config.baseUrl, "https://openapi-weaz.tuyaeu.com");
    EXPECT_EQ(config.devices.size(), 1u);
    EXPECT_EQ(config.devices[0].automation.scheduleOnTime, "22:00");
    EXPECT_EQ(config.devices[0].automation.scheduleOffTime, "06:00");
    EXPECT_EQ(config.screenBrightnessNight, 15);
    EXPECT_TRUE(config.hasLatitude);
}

TEST(ApplySettingsFromJsonRejectsSameScheduleTimes) {
    AppConfig config{};
    std::string error;
    const std::string json =
        "{\"clientId\":\"id\",\"clientSecret\":\"secret\",\"dataCenterIndex\":0,"
        "\"latitude\":1,\"longitude\":2,\"lockOffSeconds\":30,"
        "\"screenBrightnessNight\":20,\"screenBrightnessDay\":80,"
        "\"devices\":[{"
        "\"id\":\"device\",\"name\":\"Test plug\",\"type\":\"plug\",\"enabled\":true,"
        "\"switchCode\":\"switch_1\",\"mode\":\"schedule\","
        "\"scheduleOnTime\":\"18:00\",\"scheduleOffTime\":\"18:00\","
        "\"darkOffsetMinutes\":0,\"lightOffsetMinutes\":0,"
        "\"nightBrightness\":20,\"dayBrightness\":80,\"useBrightness\":false"
        "}]}";
    EXPECT_FALSE(ApplySettingsFromJson(json, config, error));
    EXPECT_TRUE(error.find("cannot be the same") != std::string::npos);
}

TEST(HandleSettingsWebMessageParseCoords) {
    AppConfig config{};
    const auto result = HandleSettingsWebMessage(
        R"({"type":"parseCoords","text":"51.4809, -3.2092","reportError":false})",
        "unused.json",
        config);
    EXPECT_TRUE(result.kind == SettingsWebResult::Kind::SetLocation);
    EXPECT_TRUE(result.latitude > 51.0 && result.latitude < 52.0);
}

void RunSettingsPageTests() {
    std::printf("settings page tests\n");
    RUN_TEST(JsonEscapeQuotesAndTags);
    RUN_TEST(InjectSettingsBootReplacesToken);
    RUN_TEST(InjectBrandMarkLeavesHtmlWhenTokenMissing);
    RUN_TEST(PrepareSettingsHtmlAppliesBootAfterBrandMark);
    RUN_TEST(BuildSettingsBootJsonContainsFields);
    RUN_TEST(ApplySettingsFromJsonRequiresCredentials);
    RUN_TEST(ApplySettingsFromJsonAcceptsValidPayload);
    RUN_TEST(ApplySettingsFromJsonRejectsSameScheduleTimes);
    RUN_TEST(HandleSettingsWebMessageParseCoords);
}
