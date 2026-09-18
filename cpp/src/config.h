#pragma once

#include <string>
#include <vector>

enum class DeviceType {
    Plug,
    Bulb,
};

enum class DeviceAutomationMode {
    Manual,
    Smart,
    Schedule,
    Timed,
};

struct DeviceCapabilities {
    std::string switchCode = "switch_1";
    std::string brightnessCode;
    int brightnessMin = 10;
    int brightnessMax = 1000;
};

struct DeviceAutomation {
    DeviceAutomationMode mode = DeviceAutomationMode::Manual;
    std::string scheduleOnTime = "18:00";
    std::string scheduleOffTime = "23:00";
    int darkOffsetMinutes = -30;
    int lightOffsetMinutes = -30;
    int nightBrightness = 20;
    int dayBrightness = 80;
    bool useBrightness = true;
    int timedDurationMinutes = 30;
};

struct DeviceConfig {
    std::string id;
    std::string name;
    DeviceType type = DeviceType::Plug;
    bool enabled = true;
    DeviceCapabilities capabilities;
    DeviceAutomation automation;
};

struct AppConfig {
    std::string clientId;
    std::string clientSecret;
    std::string baseUrl;
    std::vector<DeviceConfig> devices;

    // Legacy top-level fields kept in sync with the first device for older scripts.
    std::string deviceId;
    std::string switchCode = "switch_1";

    bool hasLatitude = false;
    bool hasLongitude = false;
    double latitude = 0.0;
    double longitude = 0.0;
    int darkOffsetMinutes = -30;
    int lightOffsetMinutes = -30;
    int lockOffSeconds = 30;
    int screenBrightnessNight = 20;
    int screenBrightnessDay = 80;
    bool screenBrightnessAdaptive = false;
    int windowAzimuthDegrees = -1;
    double windowGlareWeight = 0.5;
    std::string scheduleOnTime = "18:00";
    std::string scheduleOffTime = "23:00";
    bool hasScheduleTimes = false;
};

int ClampScreenBrightnessPercent(int percent);
int ClampDeviceBrightnessPercent(int percent);
int ClampTimedDurationMinutes(int minutes);

const DeviceConfig* GetPrimaryDevice(const AppConfig& config);
std::vector<const DeviceConfig*> GetEnabledDevices(const AppConfig& config);
void SyncLegacyFieldsFromDevices(AppConfig& config);
DeviceConfig MakeLegacyDeviceFromAppConfig(const AppConfig& config);

bool EnsureConfigFile(const std::string& path);
bool IsConfigComplete(const AppConfig& config);
bool LoadConfig(const std::string& path, AppConfig& out, std::string& error, bool requireCredentials = true);
bool SaveAppConfig(const std::string& path, const AppConfig& config);
bool SaveCoordinatesToConfig(const std::string& path, double latitude, double longitude);
bool SaveScheduleToConfig(const std::string& path, const std::string& onTime, const std::string& offTime);
int BaseUrlToDataCenterIndex(const std::string& baseUrl);
std::string DataCenterIndexToBaseUrl(int index);

std::string DeviceTypeToString(DeviceType type);
DeviceType DeviceTypeFromString(const std::string& text);
std::string DeviceAutomationModeToString(DeviceAutomationMode mode);
DeviceAutomationMode DeviceAutomationModeFromString(const std::string& text);

#ifdef _WIN32
#include <string>

std::wstring GetConfigPathWide();
std::wstring ResolveConfigPathWide(const std::wstring& legacyAdjacentPath);

bool LoadConfig(const std::wstring& path, AppConfig& out, std::wstring& error, bool requireCredentials = true);
bool SaveAppConfig(const std::wstring& path, const AppConfig& config);
bool SaveCoordinatesToConfig(const std::wstring& path, double latitude, double longitude);
bool SaveScheduleToConfig(const std::wstring& path, const std::string& onTime, const std::string& offTime);
bool EnsureConfigFile(const std::wstring& path);
#endif
