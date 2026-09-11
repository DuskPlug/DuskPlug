#include "../src/json_util.h"
#include "test_assert.h"

TEST(JsonGetStringValue) {
    const std::string json = R"({"ClientId":"abc","DeviceId":"dev123"})";
    auto value = JsonGetString(json, "ClientId");
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(*value, "abc");
}

TEST(JsonGetNumberValue) {
    const std::string json = R"({"Latitude":53.48,"LockOffSeconds":30})";
    auto lat = JsonGetNumber(json, "Latitude");
    EXPECT_TRUE(lat.has_value());
    EXPECT_EQ(*lat, 53.48);
}

TEST(JsonGetBoolValue) {
    const std::string json = R"({"SmartMode":true})";
    bool out = false;
    EXPECT_TRUE(JsonGetBool(json, "SmartMode", out));
    EXPECT_TRUE(out);
}

void RunJsonTests() {
    std::printf("json tests\n");
    RUN_TEST(JsonGetStringValue);
    RUN_TEST(JsonGetNumberValue);
    RUN_TEST(JsonGetBoolValue);
}
