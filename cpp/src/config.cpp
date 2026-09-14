#include "config.h"
#include "json_util.h"
#include "platform_util.h"

#include <cctype>
#include <cstdio>

namespace {

static bool LooksLikePlaceholder(const std::string& value) {
    if (value.empty()) {
        return true;
    }
    return value.rfind("your_", 0) == 0;
}

static bool UpsertJsonString(std::string& json, const std::string& key, const std::string& value) {
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

static bool UpsertJsonNumber(std::string& json, const std::string& key, double value) {
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

static bool UpsertJsonInt(std::string& json, const std::string& key, int value) {
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

bool EnsureConfigFile(const std::string& path) {
    return EnsureConfigFileAt(path);
}

bool IsConfigComplete(const AppConfig& config) {
    return !LooksLikePlaceholder(config.clientId)
        && !LooksLikePlaceholder(config.clientSecret)
        && !LooksLikePlaceholder(config.deviceId)
        && !config.baseUrl.empty();
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
    auto deviceId = JsonGetString(json, "DeviceId");
    auto baseUrl = JsonGetString(json, "BaseUrl");
    auto switchCode = JsonGetString(json, "SwitchCode");

    if (!clientId || !clientSecret || !deviceId || !baseUrl) {
        error = "Settings are missing plug connection details.";
        return false;
    }

    if (requireCredentials) {
        AppConfig required{};
        required.clientId = *clientId;
        required.clientSecret = *clientSecret;
        required.deviceId = *deviceId;
        required.baseUrl = *baseUrl;
        if (!IsConfigComplete(required)) {
            error = "Finish plug connection details in Settings.";
            return false;
        }
    }

    out.clientId = *clientId;
    out.clientSecret = *clientSecret;
    out.deviceId = *deviceId;
    out.baseUrl = *baseUrl;
    if (switchCode && !switchCode->empty()) {
        out.switchCode = *switchCode;
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
    if (auto scheduleOn = JsonGetString(json, "ScheduleOnTime")) {
        out.scheduleOnTime = *scheduleOn;
        out.hasScheduleTimes = true;
    }
    if (auto scheduleOff = JsonGetString(json, "ScheduleOffTime")) {
        out.scheduleOffTime = *scheduleOff;
        out.hasScheduleTimes = true;
    }

    return true;
}

bool SaveAppConfig(const std::string& path, const AppConfig& config) {
    std::string json = ReadTextFile(path);
    if (json.empty()) {
        EnsureConfigFile(path);
        json = ReadTextFile(path);
    }

    if (!UpsertJsonString(json, "ClientId", config.clientId)
        || !UpsertJsonString(json, "ClientSecret", config.clientSecret)
        || !UpsertJsonString(json, "DeviceId", config.deviceId)
        || !UpsertJsonString(json, "BaseUrl", config.baseUrl)
        || !UpsertJsonString(json, "SwitchCode", config.switchCode)
        || !UpsertJsonNumber(json, "Latitude", config.latitude)
        || !UpsertJsonNumber(json, "Longitude", config.longitude)
        || !UpsertJsonInt(json, "DarkOffsetMinutes", config.darkOffsetMinutes)
        || !UpsertJsonInt(json, "LightOffsetMinutes", config.lightOffsetMinutes)
        || !UpsertJsonInt(json, "LockOffSeconds", config.lockOffSeconds)
        || !UpsertJsonString(json, "ScheduleOnTime", config.scheduleOnTime)
        || !UpsertJsonString(json, "ScheduleOffTime", config.scheduleOffTime)) {
        return false;
    }

    return WriteTextFile(path, json);
}

bool SaveCoordinatesToConfig(const std::string& path, double latitude, double longitude) {
    std::string json = ReadTextFile(path);
    if (json.empty()) {
        return false;
    }
    if (!UpsertJsonNumber(json, "Latitude", latitude) || !UpsertJsonNumber(json, "Longitude", longitude)) {
        return false;
    }
    return WriteTextFile(path, json);
}

bool SaveScheduleToConfig(const std::string& path, const std::string& onTime, const std::string& offTime) {
    std::string json = ReadTextFile(path);
    if (json.empty()) {
        return false;
    }
    if (!UpsertJsonString(json, "ScheduleOnTime", onTime) || !UpsertJsonString(json, "ScheduleOffTime", offTime)) {
        return false;
    }
    return WriteTextFile(path, json);
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
