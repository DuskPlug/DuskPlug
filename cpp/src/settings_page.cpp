#include "settings_page.h"

#include "coords.h"
#include "json_util.h"
#include "platform_util.h"
#include "schedule.h"
#include "tuya_client.h"
#include "version.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace {

const char* DataCenterLabel(int index) {
    switch (index) {
    case 1:
        return "Western Europe";
    case 2:
        return "Western America";
    case 3:
        return "Eastern America";
    case 4:
        return "Singapore";
    case 5:
        return "India";
    default:
        return "Central Europe (UK / most EU)";
    }
}

std::string FindAssetPath(const char* filename) {
    const std::string exeDir = GetExeDirectory();
    const std::vector<std::string> candidates = {
#ifdef __APPLE__
        exeDir + "/../Resources/" + filename,
#endif
        exeDir + "/assets/" + filename,
        exeDir + "/../assets/" + filename,
        exeDir + "/../../assets/" + filename,
        std::string("assets/") + filename,
    };

    for (const auto& path : candidates) {
        if (FileExists(path)) {
            return path;
        }
    }
    return {};
}

std::string FindSettingsHtmlPath() {
    return FindAssetPath("settings.html");
}

std::string NormalizeScheduleTime(const std::string& text) {
    if (text.size() >= 5 && text[2] == ':') {
        return text.substr(0, 5);
    }
    return text;
}

bool ReadRequiredString(const std::string& json, const char* key, std::string& out) {
    if (auto value = JsonGetString(json, key)) {
        out = *value;
        return true;
    }
    out.clear();
    return false;
}

bool ReadIntField(const std::string& json, const char* key, int& out) {
    if (auto value = JsonGetNumber(json, key)) {
        out = static_cast<int>(*value);
        return true;
    }
    return false;
}

bool ReadDoubleField(const std::string& json, const char* key, double& out) {
    if (auto value = JsonGetNumber(json, key)) {
        out = *value;
        return true;
    }
    return false;
}

bool ReadBoolField(const std::string& json, const char* key, bool& out) {
    return JsonGetBool(json, key, out);
}

std::string BuildDevicesBootJson(const AppConfig& config) {
    std::string json = "[";
    for (size_t i = 0; i < config.devices.size(); ++i) {
        if (i > 0) {
            json += ",";
        }
        const DeviceConfig& device = config.devices[i];
        json += "{";
        json += "\"id\":\"" + JsonEscape(device.id) + "\",";
        json += "\"name\":\"" + JsonEscape(device.name) + "\",";
        json += "\"type\":\"" + DeviceTypeToString(device.type) + "\",";
        json += "\"enabled\":" + std::string(device.enabled ? "true" : "false") + ",";
        json += "\"switchCode\":\"" + JsonEscape(device.capabilities.switchCode) + "\",";
        json += "\"brightnessCode\":\"" + JsonEscape(device.capabilities.brightnessCode) + "\",";
        json += "\"brightnessMin\":" + std::to_string(device.capabilities.brightnessMin) + ",";
        json += "\"brightnessMax\":" + std::to_string(device.capabilities.brightnessMax) + ",";
        json += "\"mode\":\"" + DeviceAutomationModeToString(device.automation.mode) + "\",";
        json += "\"scheduleOnTime\":\"" + JsonEscape(device.automation.scheduleOnTime) + "\",";
        json += "\"scheduleOffTime\":\"" + JsonEscape(device.automation.scheduleOffTime) + "\",";
        json += "\"darkOffsetMinutes\":" + std::to_string(device.automation.darkOffsetMinutes) + ",";
        json += "\"lightOffsetMinutes\":" + std::to_string(device.automation.lightOffsetMinutes) + ",";
        json += "\"nightBrightness\":" + std::to_string(device.automation.nightBrightness) + ",";
        json += "\"dayBrightness\":" + std::to_string(device.automation.dayBrightness) + ",";
        json += "\"useBrightness\":" + std::string(device.automation.useBrightness ? "true" : "false");
        json += "}";
    }
    json += "]";
    return json;
}

bool ParseDevicePayload(const std::string& json, DeviceConfig& device) {
    if (!ReadRequiredString(json, "id", device.id) || device.id.empty()) {
        return false;
    }
    ReadRequiredString(json, "name", device.name);
    if (device.name.empty()) {
        device.name = device.id;
    }
    if (auto type = JsonGetString(json, "type")) {
        device.type = DeviceTypeFromString(*type);
    }
    bool enabled = true;
    if (ReadBoolField(json, "enabled", enabled)) {
        device.enabled = enabled;
    }
    ReadRequiredString(json, "switchCode", device.capabilities.switchCode);
    if (device.capabilities.switchCode.empty()) {
        device.capabilities.switchCode = device.type == DeviceType::Bulb ? "switch_led" : "switch_1";
    }
    if (auto brightnessCode = JsonGetString(json, "brightnessCode")) {
        device.capabilities.brightnessCode = *brightnessCode;
    }
    ReadIntField(json, "brightnessMin", device.capabilities.brightnessMin);
    ReadIntField(json, "brightnessMax", device.capabilities.brightnessMax);
    if (auto mode = JsonGetString(json, "mode")) {
        device.automation.mode = DeviceAutomationModeFromString(*mode);
    }
    ReadRequiredString(json, "scheduleOnTime", device.automation.scheduleOnTime);
    ReadRequiredString(json, "scheduleOffTime", device.automation.scheduleOffTime);
    ReadIntField(json, "darkOffsetMinutes", device.automation.darkOffsetMinutes);
    ReadIntField(json, "lightOffsetMinutes", device.automation.lightOffsetMinutes);
    int nightBrightness = device.automation.nightBrightness;
    int dayBrightness = device.automation.dayBrightness;
    ReadIntField(json, "nightBrightness", nightBrightness);
    ReadIntField(json, "dayBrightness", dayBrightness);
    device.automation.nightBrightness = ClampDeviceBrightnessPercent(nightBrightness);
    device.automation.dayBrightness = ClampDeviceBrightnessPercent(dayBrightness);
    bool useBrightness = device.automation.useBrightness;
    if (ReadBoolField(json, "useBrightness", useBrightness)) {
        device.automation.useBrightness = useBrightness;
    }
    return true;
}

std::string JsCallDiscoveryResult(bool ok, const std::string& payloadJson) {
    if (!ok) {
        return "window.duskplugShowError(\"" + JsonEscape(payloadJson) + "\", false);";
    }
    return "window.duskplugApplyDiscovery(" + payloadJson + ");";
}

}  // namespace

std::string JsonEscape(const std::string& text) {
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
        case '<':
            out += "\\u003c";
            break;
        default:
            if (c < 0x20) {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            } else {
                out += static_cast<char>(c);
            }
            break;
        }
    }
    return out;
}

std::string LoadSettingsHtml() {
    const std::string path = FindSettingsHtmlPath();
    if (path.empty()) {
        return {};
    }
    return ReadTextFile(path);
}

std::string BuildSettingsBootJson(const AppConfig& config, const char* platform) {
    const int centerIndex = BaseUrlToDataCenterIndex(config.baseUrl);
    std::string json = "{";
    json += "\"version\":\"" + JsonEscape(DUSKPLUG_VERSION) + "\",";
    json += "\"platform\":\"" + JsonEscape(platform ? platform : "") + "\",";
    json += "\"dataCenters\":[";
    for (int i = 0; i < 6; ++i) {
        if (i > 0) {
            json += ",";
        }
        json += "{\"id\":" + std::to_string(i) + ",\"label\":\"" + JsonEscape(DataCenterLabel(i)) + "\"}";
    }
    json += "],";
    json += "\"config\":{";
    json += "\"clientId\":\"" + JsonEscape(config.clientId) + "\",";
    json += "\"clientSecret\":\"" + JsonEscape(config.clientSecret) + "\",";
    json += "\"dataCenterIndex\":" + std::to_string(centerIndex) + ",";
    json += "\"devices\":" + BuildDevicesBootJson(config) + ",";

    char number[64];
    snprintf(number, sizeof(number), "%.6f", config.latitude);
    json += "\"latitude\":" + std::string(number) + ",";
    snprintf(number, sizeof(number), "%.6f", config.longitude);
    json += "\"longitude\":" + std::string(number) + ",";
    json += "\"lockOffSeconds\":" + std::to_string(config.lockOffSeconds) + ",";
    json += "\"screenBrightnessNight\":" + std::to_string(config.screenBrightnessNight) + ",";
    json += "\"screenBrightnessDay\":" + std::to_string(config.screenBrightnessDay) + ",";
    json += "\"windowAzimuthDegrees\":" + std::to_string(config.windowAzimuthDegrees) + ",";
    char glareNumber[32];
    snprintf(glareNumber, sizeof(glareNumber), "%.2f", config.windowGlareWeight);
    json += "\"windowGlareWeight\":" + std::string(glareNumber) + ",";
    json += "\"darkOffsetMinutes\":" + std::to_string(config.darkOffsetMinutes) + ",";
    json += "\"lightOffsetMinutes\":" + std::to_string(config.lightOffsetMinutes);
    json += "}}";
    return json;
}

std::string InjectBrandMark(const std::string& html) {
    const std::string token = "/*__DUSKPLUG_MARK__*/";
    const size_t pos = html.find(token);
    if (pos == std::string::npos) {
        return html;
    }

    const std::string path = FindAssetPath("brand-mark.png");
    const std::string bytes = path.empty() ? std::string{} : ReadBinaryFile(path);
    if (bytes.empty()) {
        return html;
    }

    std::string out = html;
    out.replace(pos, token.size(), "data:image/png;base64," + Base64Encode(bytes));
    return out;
}

std::string InjectSettingsBoot(const std::string& html, const std::string& bootJson) {
    const std::string token = "/*__DUSKPLUG_BOOT__*/null";
    const size_t pos = html.find(token);
    if (pos == std::string::npos) {
        return html;
    }
    std::string out = html;
    out.replace(pos, token.size(), bootJson);
    return out;
}

std::string PrepareSettingsHtml(const std::string& html, const std::string& bootJson) {
    return InjectSettingsBoot(InjectBrandMark(html), bootJson);
}

std::string JsCallSetLocation(double latitude, double longitude) {
    char script[160];
    snprintf(
        script,
        sizeof(script),
        "window.duskplugSetLocation(%.6f, %.6f);",
        latitude,
        longitude);
    return script;
}

std::string JsCallShowError(const std::string& message, bool openLocationSettings) {
    return "window.duskplugShowError(\"" + JsonEscape(message) + "\", {openLocationSettings: "
        + (openLocationSettings ? "true" : "false") + "});";
}

bool ApplySettingsFromJson(const std::string& json, AppConfig& config, std::string& error) {
    if (!ReadRequiredString(json, "clientId", config.clientId)
        || !ReadRequiredString(json, "clientSecret", config.clientSecret)) {
        error = "Access ID and Access Secret are required.";
        return false;
    }

    if (config.clientId.empty() || config.clientSecret.empty()) {
        error = "Access ID and Access Secret are required.";
        return false;
    }

    int centerIndex = 0;
    if (!ReadIntField(json, "dataCenterIndex", centerIndex)) {
        centerIndex = BaseUrlToDataCenterIndex(config.baseUrl);
    }
    config.baseUrl = DataCenterIndexToBaseUrl(centerIndex);

    config.devices.clear();
    const auto deviceSlices = JsonGetArrayObjectSlices(json, "devices");
    if (deviceSlices.empty()) {
        error = "Add at least one device.";
        return false;
    }
    for (const auto& slice : deviceSlices) {
        DeviceConfig device{};
        if (!ParseDevicePayload(slice, device)) {
            error = "Each device needs an ID and valid settings.";
            return false;
        }
        int onMinutes = 0;
        int offMinutes = 0;
        device.automation.scheduleOnTime = NormalizeScheduleTime(device.automation.scheduleOnTime);
        device.automation.scheduleOffTime = NormalizeScheduleTime(device.automation.scheduleOffTime);
        if (device.automation.mode == DeviceAutomationMode::Schedule) {
            if (!ParseTimeHHMM(device.automation.scheduleOnTime, onMinutes)
                || !ParseTimeHHMM(device.automation.scheduleOffTime, offMinutes)) {
                error = "Choose valid ON and OFF times for each scheduled device.";
                return false;
            }
            if (onMinutes == offMinutes) {
                error = "ON and OFF times cannot be the same for a scheduled device.";
                return false;
            }
        }
        config.devices.push_back(std::move(device));
    }

    double latitude = 0.0;
    double longitude = 0.0;
    const auto paste = JsonGetString(json, "pasteCoords");
    if (paste && !paste->empty() && ParseLatLonPair(*paste, latitude, longitude)) {
        config.latitude = latitude;
        config.longitude = longitude;
    } else if (auto latText = JsonGetString(json, "latitudeText")) {
        if (!latText->empty() && latText->find(',') != std::string::npos
            && ParseLatLonPair(*latText, latitude, longitude)) {
            config.latitude = latitude;
            config.longitude = longitude;
        } else if (ReadDoubleField(json, "latitude", latitude) && ReadDoubleField(json, "longitude", longitude)) {
            config.latitude = latitude;
            config.longitude = longitude;
        } else {
            error = "Enter valid latitude and longitude, paste a Google Maps pair, or use Detect Location.";
            return false;
        }
    } else if (ReadDoubleField(json, "latitude", latitude) && ReadDoubleField(json, "longitude", longitude)) {
        config.latitude = latitude;
        config.longitude = longitude;
    } else {
        error = "Enter valid latitude and longitude, paste a Google Maps pair, or use Detect Location.";
        return false;
    }
    config.hasLatitude = true;
    config.hasLongitude = true;

    int brightnessNight = config.screenBrightnessNight;
    int brightnessDay = config.screenBrightnessDay;
    if (!ReadIntField(json, "lockOffSeconds", config.lockOffSeconds)
        || !ReadIntField(json, "screenBrightnessNight", brightnessNight)
        || !ReadIntField(json, "screenBrightnessDay", brightnessDay)) {
        error = "Lock-off and screen brightness values must be whole numbers.";
        return false;
    }

    if (config.lockOffSeconds < 0) {
        error = "Lock-off seconds cannot be negative.";
        return false;
    }

    config.screenBrightnessNight = ClampScreenBrightnessPercent(brightnessNight);
    config.screenBrightnessDay = ClampScreenBrightnessPercent(brightnessDay);

    int windowAzimuth = config.windowAzimuthDegrees;
    if (!ReadIntField(json, "windowAzimuthDegrees", windowAzimuth)) {
        windowAzimuth = -1;
    }
    config.windowAzimuthDegrees = windowAzimuth;
    config.screenBrightnessAdaptive = windowAzimuth >= 0;

    double glareWeight = config.windowGlareWeight;
    if (ReadDoubleField(json, "windowGlareWeight", glareWeight)) {
        config.windowGlareWeight = std::clamp(glareWeight, 0.0, 1.0);
    }

    SyncLegacyFieldsFromDevices(config);
    error.clear();
    return true;
}

SettingsWebResult HandleSettingsWebMessage(
    const std::string& message,
    const std::string& configPath,
    AppConfig& config) {
    SettingsWebResult result{};
    const auto type = JsonGetString(message, "type");
    if (!type) {
        result.kind = SettingsWebResult::Kind::RunScript;
        result.script = JsCallShowError("Could not read that settings message.", false);
        return result;
    }

    if (*type == "cancel") {
        result.kind = SettingsWebResult::Kind::Cancel;
        return result;
    }

    if (*type == "detectLocation") {
        result.kind = SettingsWebResult::Kind::DetectLocation;
        return result;
    }

    if (*type == "openLocationSettings") {
        result.kind = SettingsWebResult::Kind::OpenLocationSettings;
        return result;
    }

    if (*type == "discoverDevice") {
        const auto deviceId = JsonGetString(message, "deviceId");
        if (!deviceId || deviceId->empty()) {
            result.kind = SettingsWebResult::Kind::RunScript;
            result.script = JsCallDiscoveryResult(false, "Device ID is required.");
            return result;
        }

        TuyaClient client(config);
        DiscoveredCapabilities discovered{};
        std::string error;
        if (!client.DiscoverFunctions(*deviceId, discovered, error)) {
            result.kind = SettingsWebResult::Kind::RunScript;
            result.script = JsCallDiscoveryResult(false, error.empty() ? "Could not discover device functions." : error);
            return result;
        }

        std::string payload = "{";
        payload += "\"deviceId\":\"" + JsonEscape(*deviceId) + "\",";
        payload += "\"type\":\"" + DeviceTypeToString(discovered.suggestedType) + "\",";
        payload += "\"switchCode\":\"" + JsonEscape(discovered.capabilities.switchCode) + "\",";
        payload += "\"brightnessCode\":\"" + JsonEscape(discovered.capabilities.brightnessCode) + "\",";
        payload += "\"brightnessMin\":" + std::to_string(discovered.capabilities.brightnessMin) + ",";
        payload += "\"brightnessMax\":" + std::to_string(discovered.capabilities.brightnessMax);
        payload += "}";
        result.kind = SettingsWebResult::Kind::RunScript;
        result.script = JsCallDiscoveryResult(true, payload);
        return result;
    }

    if (*type == "parseCoords") {
        const auto text = JsonGetString(message, "text");
        bool reportError = false;
        JsonGetBool(message, "reportError", reportError);
        double latitude = 0.0;
        double longitude = 0.0;
        if (text && ParseLatLonPair(*text, latitude, longitude)) {
            result.kind = SettingsWebResult::Kind::SetLocation;
            result.latitude = latitude;
            result.longitude = longitude;
            return result;
        }
        if (reportError) {
            result.kind = SettingsWebResult::Kind::RunScript;
            result.script = JsCallShowError(
                "Paste a Google Maps pair such as 51.4809, -3.2092, or a Google Maps link.",
                false);
        }
        return result;
    }

    if (*type == "save") {
        AppConfig updated = config;
        std::string error;
        if (!ApplySettingsFromJson(message, updated, error)) {
            result.kind = SettingsWebResult::Kind::RunScript;
            result.script = JsCallShowError(error, false);
            return result;
        }
        if (!SaveAppConfig(configPath, updated)) {
            result.kind = SettingsWebResult::Kind::RunScript;
            result.script = JsCallShowError("Could not save your settings.", false);
            return result;
        }
        config = updated;
        result.kind = SettingsWebResult::Kind::Saved;
        return result;
    }

    result.kind = SettingsWebResult::Kind::RunScript;
    result.script = JsCallShowError("Unknown settings action.", false);
    return result;
}
