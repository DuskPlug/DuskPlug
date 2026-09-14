#include "config.h"
#include "json_util.h"
#include "platform_util.h"

#include <cctype>
#include <cstdio>

namespace {

int ClampScreenBrightnessPercentValue(int percent) {
    if (percent < 0) {
        return 0;
    }
    if (percent > 100) {
        return 100;
    }
    return percent;
}

bool LooksLikePlaceholder(const std::string& value) {
    if (value.empty()) {
        return true;
    }
    return value.rfind("your_", 0) == 0;
}

bool UpsertJsonString(std::string& json, const std::string& key, const std::string& value) {
    const std::string quoted = "\"" + key + "\"";
    const std::string escaped = "\"" + value + "\"";
    const size_t keyPos = json.find(quoted);

    if (keyPos != std::string::npos) {
        size_t colon = json.find(':', keyPos + quoted.size());
        if (colon == std::string::npos) {
            return false;
        }
        ++colon;
        while (colon < json.size() && std::isspace(static_cast<unsigned char>(json[colon]))) {
            ++colon;
        }
        if (colon >= json.size() || json[colon] != '"') {
            return false;
        }
        size_t end = colon + 1;
        while (end < json.size() && json[end] != '"') {
            ++end;
        }
        if (end >= json.size()) {
            return false;
        }
        ++end;
        json.replace(colon, end - colon, escaped);
        return true;
    }

    const size_t close = json.rfind('}');
    if (close == std::string::npos) {
        return false;
    }

    const std::string insertion = std::string(",\r\n  \"") + key + "\": \"" + value + "\"";
    json.insert(close, insertion);
    return true;
}

bool UpsertJsonNumber(std::string& json, const std::string& key, double value) {
    const std::string quoted = "\"" + key + "\"";
    const size_t keyPos = json.find(quoted);
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%.4f", value);
    const std::string number = buffer;

    if (keyPos != std::string::npos) {
        size_t colon = json.find(':', keyPos + quoted.size());
        if (colon == std::string::npos) {
            return false;
        }
        ++colon;
        while (colon < json.size() && std::isspace(static_cast<unsigned char>(json[colon]))) {
            ++colon;
        }
        size_t end = colon;
        while (end < json.size()) {
            const char c = json[end];
            if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.') {
                ++end;
                continue;
            }
            break;
        }
        if (end == colon) {
            return false;
        }
        json.replace(colon, end - colon, number);
        return true;
    }

    const size_t close = json.rfind('}');
    if (close == std::string::npos) {
        return false;
    }

    const std::string insertion = std::string(",\r\n  \"") + key + "\": " + number;
    json.insert(close, insertion);
    return true;
}

bool UpsertJsonInt(std::string& json, const std::string& key, int value) {
    const std::string quoted = "\"" + key + "\"";
    const size_t keyPos = json.find(quoted);
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", value);
    const std::string number = buffer;

    if (keyPos != std::string::npos) {
        size_t colon = json.find(':', keyPos + quoted.size());
        if (colon == std::string::npos) {
            return false;
        }
        ++colon;
        while (colon < json.size() && std::isspace(static_cast<unsigned char>(json[colon]))) {
            ++colon;
        }
        size_t end = colon;
        while (end < json.size()) {
            const char c = json[end];
            if ((c >= '0' && c <= '9') || c == '-' || c == '+') {
                ++end;
                continue;
            }
            break;
        }
        if (end == colon) {
            return false;
        }
        json.replace(colon, end - colon, number);
        return true;
    }

    const size_t close = json.rfind('}');
    if (close == std::string::npos) {
        return false;
    }

    const std::string insertion = std::string(",\r\n  \"") + key + "\": " + number;
    json.insert(close, insertion);
    return true;
}

}  // namespace

namespace {

std::string JsonEscapeConfig(const std::string& text) {
    std::string out;
    out.reserve(text.size() + 8);
    for (unsigned char c : text) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out += static_cast<char>(c);
            break;
        }
    }
    return out;
}

bool ParseDeviceConfigJson(const std::string& json, DeviceConfig& out) {
    if (auto id = JsonGetString(json, "Id")) {
        out.id = *id;
    } else if (auto legacyId = JsonGetString(json, "id")) {
        out.id = *legacyId;
    } else {
        return false;
    }

    if (auto name = JsonGetString(json, "Name")) {
        out.name = *name;
    } else if (auto legacyName = JsonGetString(json, "name")) {
        out.name = *legacyName;
    } else {
        out.name = out.id;
    }

    if (auto type = JsonGetString(json, "Type")) {
        out.type = DeviceTypeFromString(*type);
    } else if (auto legacyType = JsonGetString(json, "type")) {
        out.type = DeviceTypeFromString(*legacyType);
    }

    bool enabled = true;
    if (JsonGetBool(json, "Enabled", enabled) || JsonGetBool(json, "enabled", enabled)) {
        out.enabled = enabled;
    }

    if (const auto capabilities = JsonGetObjectSlice(json, "Capabilities")) {
        if (auto switchCode = JsonGetString(*capabilities, "switch")) {
            out.capabilities.switchCode = *switchCode;
        }
        if (auto brightness = JsonGetString(*capabilities, "brightness")) {
            out.capabilities.brightnessCode = *brightness;
        }
        if (auto minValue = JsonGetNumber(*capabilities, "brightnessMin")) {
            out.capabilities.brightnessMin = static_cast<int>(*minValue);
        }
        if (auto maxValue = JsonGetNumber(*capabilities, "brightnessMax")) {
            out.capabilities.brightnessMax = static_cast<int>(*maxValue);
        }
    } else if (auto switchCode = JsonGetString(json, "SwitchCode")) {
        out.capabilities.switchCode = *switchCode;
    }

    if (const auto automation = JsonGetObjectSlice(json, "Automation")) {
        if (auto mode = JsonGetString(*automation, "mode")) {
            out.automation.mode = DeviceAutomationModeFromString(*mode);
        }
        if (auto onTime = JsonGetString(*automation, "scheduleOnTime")) {
            out.automation.scheduleOnTime = *onTime;
        }
        if (auto offTime = JsonGetString(*automation, "scheduleOffTime")) {
            out.automation.scheduleOffTime = *offTime;
        }
        if (auto darkOffset = JsonGetNumber(*automation, "darkOffsetMinutes")) {
            out.automation.darkOffsetMinutes = static_cast<int>(*darkOffset);
        }
        if (auto lightOffset = JsonGetNumber(*automation, "lightOffsetMinutes")) {
            out.automation.lightOffsetMinutes = static_cast<int>(*lightOffset);
        }
        if (auto night = JsonGetNumber(*automation, "nightBrightness")) {
            out.automation.nightBrightness = ClampDeviceBrightnessPercent(static_cast<int>(*night));
        }
        if (auto day = JsonGetNumber(*automation, "dayBrightness")) {
            out.automation.dayBrightness = ClampDeviceBrightnessPercent(static_cast<int>(*day));
        }
        bool useBrightness = true;
        if (JsonGetBool(*automation, "useBrightness", useBrightness)) {
            out.automation.useBrightness = useBrightness;
        }
    }

    if (out.capabilities.switchCode.empty()) {
        out.capabilities.switchCode = out.type == DeviceType::Bulb ? "switch_led" : "switch_1";
    }

    return !out.id.empty();
}

std::string SerializeDeviceConfigJson(const DeviceConfig& device) {
    std::string json = "    {\r\n";
    json += "      \"Id\": \"" + JsonEscapeConfig(device.id) + "\",\r\n";
    json += "      \"Name\": \"" + JsonEscapeConfig(device.name) + "\",\r\n";
    json += "      \"Type\": \"" + DeviceTypeToString(device.type) + "\",\r\n";
    json += "      \"Enabled\": " + std::string(device.enabled ? "true" : "false") + ",\r\n";
    json += "      \"Capabilities\": {\r\n";
    json += "        \"switch\": \"" + JsonEscapeConfig(device.capabilities.switchCode) + "\"";
    if (!device.capabilities.brightnessCode.empty()) {
        json += ",\r\n        \"brightness\": \"" + JsonEscapeConfig(device.capabilities.brightnessCode) + "\",\r\n";
        json += "        \"brightnessMin\": " + std::to_string(device.capabilities.brightnessMin) + ",\r\n";
        json += "        \"brightnessMax\": " + std::to_string(device.capabilities.brightnessMax);
    }
    json += "\r\n      },\r\n";
    json += "      \"Automation\": {\r\n";
    json += "        \"mode\": \"" + DeviceAutomationModeToString(device.automation.mode) + "\",\r\n";
    json += "        \"scheduleOnTime\": \"" + JsonEscapeConfig(device.automation.scheduleOnTime) + "\",\r\n";
    json += "        \"scheduleOffTime\": \"" + JsonEscapeConfig(device.automation.scheduleOffTime) + "\",\r\n";
    json += "        \"darkOffsetMinutes\": " + std::to_string(device.automation.darkOffsetMinutes) + ",\r\n";
    json += "        \"lightOffsetMinutes\": " + std::to_string(device.automation.lightOffsetMinutes) + ",\r\n";
    json += "        \"nightBrightness\": " + std::to_string(device.automation.nightBrightness) + ",\r\n";
    json += "        \"dayBrightness\": " + std::to_string(device.automation.dayBrightness) + ",\r\n";
    json += "        \"useBrightness\": " + std::string(device.automation.useBrightness ? "true" : "false") + "\r\n";
    json += "      }\r\n";
    json += "    }";
    return json;
}

std::string SerializeAppConfig(const AppConfig& config) {
    AppConfig normalized = config;
    SyncLegacyFieldsFromDevices(normalized);

    std::string json = "{\r\n";
    json += "  \"ClientId\": \"" + JsonEscapeConfig(normalized.clientId) + "\",\r\n";
    json += "  \"ClientSecret\": \"" + JsonEscapeConfig(normalized.clientSecret) + "\",\r\n";
    json += "  \"BaseUrl\": \"" + JsonEscapeConfig(normalized.baseUrl) + "\",\r\n";
    json += "  \"Devices\": [\r\n";
    for (size_t i = 0; i < normalized.devices.size(); ++i) {
        if (i > 0) {
            json += ",\r\n";
        }
        json += SerializeDeviceConfigJson(normalized.devices[i]);
    }
    json += "\r\n  ],\r\n";
    json += "  \"DeviceId\": \"" + JsonEscapeConfig(normalized.deviceId) + "\",\r\n";
    json += "  \"SwitchCode\": \"" + JsonEscapeConfig(normalized.switchCode) + "\",\r\n";
    char number[64];
    snprintf(number, sizeof(number), "%.4f", normalized.latitude);
    json += "  \"Latitude\": ";
    json += number;
    json += ",\r\n";
    snprintf(number, sizeof(number), "%.4f", normalized.longitude);
    json += "  \"Longitude\": ";
    json += number;
    json += ",\r\n";
    json += "  \"DarkOffsetMinutes\": " + std::to_string(normalized.darkOffsetMinutes) + ",\r\n";
    json += "  \"LightOffsetMinutes\": " + std::to_string(normalized.lightOffsetMinutes) + ",\r\n";
    json += "  \"LockOffSeconds\": " + std::to_string(normalized.lockOffSeconds) + ",\r\n";
    json += "  \"ScreenBrightnessNight\": " + std::to_string(ClampScreenBrightnessPercentValue(normalized.screenBrightnessNight)) + ",\r\n";
    json += "  \"ScreenBrightnessDay\": " + std::to_string(ClampScreenBrightnessPercentValue(normalized.screenBrightnessDay)) + ",\r\n";
    json += "  \"ScheduleOnTime\": \"" + JsonEscapeConfig(normalized.scheduleOnTime) + "\",\r\n";
    json += "  \"ScheduleOffTime\": \"" + JsonEscapeConfig(normalized.scheduleOffTime) + "\"\r\n";
    json += "}\r\n";
    return json;
}

void MigrateLegacyDeviceFields(AppConfig& config) {
    if (!config.devices.empty()) {
        SyncLegacyFieldsFromDevices(config);
        return;
    }

    if (config.deviceId.empty()) {
        return;
    }

    config.devices.push_back(MakeLegacyDeviceFromAppConfig(config));
    SyncLegacyFieldsFromDevices(config);
}

}  // namespace

int ClampScreenBrightnessPercent(int percent) {
    return ClampScreenBrightnessPercentValue(percent);
}

int ClampDeviceBrightnessPercent(int percent) {
    return ClampScreenBrightnessPercentValue(percent);
}

std::string DeviceTypeToString(DeviceType type) {
    return type == DeviceType::Bulb ? "bulb" : "plug";
}

DeviceType DeviceTypeFromString(const std::string& text) {
    if (text == "bulb") {
        return DeviceType::Bulb;
    }
    return DeviceType::Plug;
}

std::string DeviceAutomationModeToString(DeviceAutomationMode mode) {
    switch (mode) {
    case DeviceAutomationMode::Smart:
        return "smart";
    case DeviceAutomationMode::Schedule:
        return "schedule";
    case DeviceAutomationMode::Manual:
    default:
        return "manual";
    }
}

DeviceAutomationMode DeviceAutomationModeFromString(const std::string& text) {
    if (text == "smart") {
        return DeviceAutomationMode::Smart;
    }
    if (text == "schedule") {
        return DeviceAutomationMode::Schedule;
    }
    return DeviceAutomationMode::Manual;
}

const DeviceConfig* GetPrimaryDevice(const AppConfig& config) {
    for (const auto& device : config.devices) {
        if (device.enabled && !device.id.empty()) {
            return &device;
        }
    }
    if (!config.devices.empty()) {
        return &config.devices.front();
    }
    return nullptr;
}

std::vector<const DeviceConfig*> GetEnabledDevices(const AppConfig& config) {
    std::vector<const DeviceConfig*> devices;
    for (const auto& device : config.devices) {
        if (device.enabled && !device.id.empty()) {
            devices.push_back(&device);
        }
    }
    return devices;
}

DeviceConfig MakeLegacyDeviceFromAppConfig(const AppConfig& config) {
    DeviceConfig device{};
    device.id = config.deviceId;
    device.name = config.deviceId.empty() ? "Device" : config.deviceId;
    device.type = DeviceType::Plug;
    device.enabled = true;
    device.capabilities.switchCode = config.switchCode.empty() ? "switch_1" : config.switchCode;
    device.automation.darkOffsetMinutes = config.darkOffsetMinutes;
    device.automation.lightOffsetMinutes = config.lightOffsetMinutes;
    device.automation.scheduleOnTime = config.scheduleOnTime;
    device.automation.scheduleOffTime = config.scheduleOffTime;
    device.automation.nightBrightness = config.screenBrightnessNight;
    device.automation.dayBrightness = config.screenBrightnessDay;
    if (config.hasScheduleTimes) {
        device.automation.mode = DeviceAutomationMode::Schedule;
    }
    return device;
}

void SyncLegacyFieldsFromDevices(AppConfig& config) {
    const DeviceConfig* primary = GetPrimaryDevice(config);
    if (!primary) {
        return;
    }

    config.deviceId = primary->id;
    config.switchCode = primary->capabilities.switchCode;
    config.darkOffsetMinutes = primary->automation.darkOffsetMinutes;
    config.lightOffsetMinutes = primary->automation.lightOffsetMinutes;
    config.scheduleOnTime = primary->automation.scheduleOnTime;
    config.scheduleOffTime = primary->automation.scheduleOffTime;
    config.hasScheduleTimes = true;
}

bool EnsureConfigFile(const std::string& path) {
    return EnsureConfigFileAt(path);
}

bool IsConfigComplete(const AppConfig& config) {
    if (LooksLikePlaceholder(config.clientId)
        || LooksLikePlaceholder(config.clientSecret)
        || config.baseUrl.empty()) {
        return false;
    }

    for (const auto& device : config.devices) {
        if (device.enabled && !LooksLikePlaceholder(device.id)) {
            return true;
        }
    }

    return !LooksLikePlaceholder(config.deviceId);
}

int BaseUrlToDataCenterIndex(const std::string& baseUrl) {
    if (baseUrl == "https://openapi.tuyaeu.com") {
        return 0;
    }
    if (baseUrl == "https://openapi-weaz.tuyaeu.com") {
        return 1;
    }
    if (baseUrl == "https://openapi.tuyaus.com") {
        return 2;
    }
    if (baseUrl == "https://openapi-ueaz.tuyaus.com") {
        return 3;
    }
    if (baseUrl == "https://openapi-sg.iotbing.com") {
        return 4;
    }
    if (baseUrl == "https://openapi.tuyain.com") {
        return 5;
    }
    return 0;
}

std::string DataCenterIndexToBaseUrl(int index) {
    switch (index) {
    case 1:
        return "https://openapi-weaz.tuyaeu.com";
    case 2:
        return "https://openapi.tuyaus.com";
    case 3:
        return "https://openapi-ueaz.tuyaus.com";
    case 4:
        return "https://openapi-sg.iotbing.com";
    case 5:
        return "https://openapi.tuyain.com";
    case 0:
    default:
        return "https://openapi.tuyaeu.com";
    }
}

bool LoadConfig(const std::string& path, AppConfig& out, std::string& error, bool requireCredentials) {
    const std::string json = ReadTextFile(path);
    if (json.empty()) {
        error = "Could not open your DuskPlug settings file.";
        return false;
    }

    auto clientId = JsonGetString(json, "ClientId");
    auto clientSecret = JsonGetString(json, "ClientSecret");
    auto baseUrl = JsonGetString(json, "BaseUrl");
    auto deviceId = JsonGetString(json, "DeviceId");
    auto switchCode = JsonGetString(json, "SwitchCode");

    if (!clientId || !clientSecret || !baseUrl) {
        error = "Settings are missing plug connection details.";
        return false;
    }

    out = AppConfig{};
    out.clientId = *clientId;
    out.clientSecret = *clientSecret;
    out.baseUrl = *baseUrl;
    if (deviceId) {
        out.deviceId = *deviceId;
    }
    if (switchCode && !switchCode->empty()) {
        out.switchCode = *switchCode;
    }

    const auto deviceSlices = JsonGetArrayObjectSlices(json, "Devices");
    for (const auto& slice : deviceSlices) {
        DeviceConfig device{};
        if (ParseDeviceConfigJson(slice, device)) {
            out.devices.push_back(std::move(device));
        }
    }

    if (auto latitude = JsonGetNumber(json, "Latitude")) {
        out.latitude = *latitude;
        out.hasLatitude = true;
    }
    if (auto longitude = JsonGetNumber(json, "Longitude")) {
        out.longitude = *longitude;
        out.hasLongitude = true;
    }
    if (auto darkOffset = JsonGetNumber(json, "DarkOffsetMinutes")) {
        out.darkOffsetMinutes = static_cast<int>(*darkOffset);
    }
    if (auto lightOffset = JsonGetNumber(json, "LightOffsetMinutes")) {
        out.lightOffsetMinutes = static_cast<int>(*lightOffset);
    }
    if (auto lockOff = JsonGetNumber(json, "LockOffSeconds")) {
        out.lockOffSeconds = static_cast<int>(*lockOff);
    }
    if (auto nightBrightness = JsonGetNumber(json, "ScreenBrightnessNight")) {
        out.screenBrightnessNight = ClampScreenBrightnessPercentValue(static_cast<int>(*nightBrightness));
    }
    if (auto dayBrightness = JsonGetNumber(json, "ScreenBrightnessDay")) {
        out.screenBrightnessDay = ClampScreenBrightnessPercentValue(static_cast<int>(*dayBrightness));
    }
    if (auto scheduleOn = JsonGetString(json, "ScheduleOnTime")) {
        out.scheduleOnTime = *scheduleOn;
        out.hasScheduleTimes = true;
    }
    if (auto scheduleOff = JsonGetString(json, "ScheduleOffTime")) {
        out.scheduleOffTime = *scheduleOff;
        out.hasScheduleTimes = true;
    }

    MigrateLegacyDeviceFields(out);

    if (requireCredentials && out.devices.empty() && out.deviceId.empty()) {
        error = "Finish plug connection details in Settings.";
        return false;
    }

    if (requireCredentials && !IsConfigComplete(out)) {
        error = "Finish plug connection details in Settings.";
        return false;
    }

    return true;
}

bool SaveAppConfig(const std::string& path, const AppConfig& config) {
    AppConfig normalized = config;
    SyncLegacyFieldsFromDevices(normalized);
    return WriteTextFile(path, SerializeAppConfig(normalized));
}

bool SaveCoordinatesToConfig(const std::string& path, double latitude, double longitude) {
    AppConfig config{};
    std::string error;
    if (!LoadConfig(path, config, error, false)) {
        return false;
    }
    config.latitude = latitude;
    config.longitude = longitude;
    config.hasLatitude = true;
    config.hasLongitude = true;
    return SaveAppConfig(path, config);
}

bool SaveScheduleToConfig(const std::string& path, const std::string& onTime, const std::string& offTime) {
    AppConfig config{};
    std::string error;
    if (!LoadConfig(path, config, error, false)) {
        return false;
    }
    config.scheduleOnTime = onTime;
    config.scheduleOffTime = offTime;
    config.hasScheduleTimes = true;
    if (!config.devices.empty()) {
        config.devices[0].automation.scheduleOnTime = onTime;
        config.devices[0].automation.scheduleOffTime = offTime;
        config.devices[0].automation.mode = DeviceAutomationMode::Schedule;
    }
    return SaveAppConfig(path, config);
}

#ifdef _WIN32
std::wstring GetConfigPathWide() {
    return Utf8ToWide(GetConfigPath());
}

std::wstring ResolveConfigPathWide(const std::wstring& legacyAdjacentPath) {
    return Utf8ToWide(ResolveConfigPath(WideToUtf8(legacyAdjacentPath)));
}

bool LoadConfig(const std::wstring& path, AppConfig& out, std::wstring& error, bool requireCredentials) {
    std::string utf8Error;
    const bool ok = LoadConfig(WideToUtf8(path), out, utf8Error, requireCredentials);
    error = Utf8ToWide(utf8Error);
    return ok;
}

bool SaveAppConfig(const std::wstring& path, const AppConfig& config) {
    return SaveAppConfig(WideToUtf8(path), config);
}

bool SaveCoordinatesToConfig(const std::wstring& path, double latitude, double longitude) {
    return SaveCoordinatesToConfig(WideToUtf8(path), latitude, longitude);
}

bool SaveScheduleToConfig(const std::wstring& path, const std::string& onTime, const std::string& offTime) {
    return SaveScheduleToConfig(WideToUtf8(path), onTime, offTime);
}

bool EnsureConfigFile(const std::wstring& path) {
    return EnsureConfigFile(WideToUtf8(path));
}
#endif
