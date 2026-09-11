#include "config.h"
#include "json_util.h"

#include <cctype>
#include <cstdio>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <shlobj.h>
#include <windows.h>

#include <string>

std::wstring GetConfigPath() {
    wchar_t appData[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appData))) {
        return L"config.json";
    }

    std::wstring path(appData);
    path += L"\\SMART";
    CreateDirectoryW(path.c_str(), nullptr);
    path += L"\\config.json";
    return path;
}

std::wstring ResolveConfigPath(const std::wstring& legacyAdjacentPath) {
    const std::wstring appDataPath = GetConfigPath();

    if (GetFileAttributesW(appDataPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return appDataPath;
    }

    if (GetFileAttributesW(legacyAdjacentPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        CopyFileW(legacyAdjacentPath.c_str(), appDataPath.c_str(), FALSE);
        if (GetFileAttributesW(appDataPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return appDataPath;
        }
    }

    return appDataPath;
}

static std::string ReadTextFile(const std::wstring& path) {
    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return {};
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 1024 * 1024) {
        CloseHandle(file);
        return {};
    }

    std::string contents(static_cast<size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    const BOOL ok = ReadFile(file, contents.data(), static_cast<DWORD>(contents.size()), &read, nullptr);
    CloseHandle(file);
    if (!ok || read == 0) {
        return {};
    }
    contents.resize(read);

    if (contents.size() >= 3
        && static_cast<unsigned char>(contents[0]) == 0xEF
        && static_cast<unsigned char>(contents[1]) == 0xBB
        && static_cast<unsigned char>(contents[2]) == 0xBF) {
        contents.erase(0, 3);
    }

    return contents;
}

static bool WriteTextFileUtf8(const std::wstring& path, const std::string& contents) {
    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD written = 0;
    const BOOL ok = WriteFile(
        file,
        contents.data(),
        static_cast<DWORD>(contents.size()),
        &written,
        nullptr);
    CloseHandle(file);
    return ok && written == contents.size();
}

static const char* kDefaultConfigTemplate = R"({
  "ClientId": "",
  "ClientSecret": "",
  "DeviceId": "",
  "BaseUrl": "https://openapi.tuyaeu.com",
  "SwitchCode": "switch_1",
  "Latitude": 0,
  "Longitude": 0,
  "DarkOffsetMinutes": 0,
  "LightOffsetMinutes": 0,
  "LockOffSeconds": 30,
  "ScheduleOnTime": "18:00",
  "ScheduleOffTime": "23:00"
})";

bool EnsureConfigFile(const std::wstring& path) {
    if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return true;
    }

    const std::wstring dir = path.substr(0, path.find_last_of(L"\\/"));
    if (!dir.empty()) {
        CreateDirectoryW(dir.c_str(), nullptr);
    }

    return WriteTextFileUtf8(path, kDefaultConfigTemplate);
}

static bool LooksLikePlaceholder(const std::string& value) {
    if (value.empty()) {
        return true;
    }
    return value.rfind("your_", 0) == 0;
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

bool LoadConfig(const std::wstring& path, AppConfig& out, std::wstring& error, bool requireCredentials) {
    const std::string json = ReadTextFile(path);
    if (json.empty()) {
        error = L"Could not open your DuskPlug settings file.";
        return false;
    }

    auto clientId = JsonGetString(json, "ClientId");
    auto clientSecret = JsonGetString(json, "ClientSecret");
    auto deviceId = JsonGetString(json, "DeviceId");
    auto baseUrl = JsonGetString(json, "BaseUrl");
    auto switchCode = JsonGetString(json, "SwitchCode");

    if (!clientId || !clientSecret || !deviceId || !baseUrl) {
        error = L"Settings are missing plug connection details.";
        return false;
    }

    if (requireCredentials) {
        AppConfig required{};
        required.clientId = *clientId;
        required.clientSecret = *clientSecret;
        required.deviceId = *deviceId;
        required.baseUrl = *baseUrl;
        if (!IsConfigComplete(required)) {
            error = L"Finish plug connection details in Settings.";
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

bool SaveAppConfig(const std::wstring& path, const AppConfig& config) {
    std::string json = ReadTextFile(path);
    if (json.empty()) {
        json = kDefaultConfigTemplate;
    }

    if (!UpsertJsonString(json, "ClientId", config.clientId)) {
        return false;
    }
    if (!UpsertJsonString(json, "ClientSecret", config.clientSecret)) {
        return false;
    }
    if (!UpsertJsonString(json, "DeviceId", config.deviceId)) {
        return false;
    }
    if (!UpsertJsonString(json, "BaseUrl", config.baseUrl)) {
        return false;
    }
    if (!UpsertJsonString(json, "SwitchCode", config.switchCode)) {
        return false;
    }
    if (!UpsertJsonNumber(json, "Latitude", config.latitude)) {
        return false;
    }
    if (!UpsertJsonNumber(json, "Longitude", config.longitude)) {
        return false;
    }
    if (!UpsertJsonInt(json, "DarkOffsetMinutes", config.darkOffsetMinutes)) {
        return false;
    }
    if (!UpsertJsonInt(json, "LightOffsetMinutes", config.lightOffsetMinutes)) {
        return false;
    }
    if (!UpsertJsonInt(json, "LockOffSeconds", config.lockOffSeconds)) {
        return false;
    }
    if (!UpsertJsonString(json, "ScheduleOnTime", config.scheduleOnTime)) {
        return false;
    }
    if (!UpsertJsonString(json, "ScheduleOffTime", config.scheduleOffTime)) {
        return false;
    }

    return WriteTextFileUtf8(path, json);
}

bool SaveCoordinatesToConfig(const std::wstring& path, double latitude, double longitude) {
    std::string json = ReadTextFile(path);
    if (json.empty()) {
        return false;
    }

    if (!UpsertJsonNumber(json, "Latitude", latitude)) {
        return false;
    }
    if (!UpsertJsonNumber(json, "Longitude", longitude)) {
        return false;
    }

    return WriteTextFileUtf8(path, json);
}

bool SaveScheduleToConfig(const std::wstring& path, const std::string& onTime, const std::string& offTime) {
    std::string json = ReadTextFile(path);
    if (json.empty()) {
        return false;
    }

    if (!UpsertJsonString(json, "ScheduleOnTime", onTime)) {
        return false;
    }
    if (!UpsertJsonString(json, "ScheduleOffTime", offTime)) {
        return false;
    }

    return WriteTextFileUtf8(path, json);
}
