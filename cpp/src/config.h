#pragma once
#include <string>

struct AppConfig {
    std::string clientId;
    std::string clientSecret;
    std::string deviceId;
    std::string baseUrl;
    std::string switchCode = "switch_1";
    bool hasLatitude = false;
    bool hasLongitude = false;
    double latitude = 0.0;
    double longitude = 0.0;
    int darkOffsetMinutes = 0;
    int lightOffsetMinutes = 0;
    int lockOffSeconds = 30;
    std::string scheduleOnTime = "18:00";
    std::string scheduleOffTime = "23:00";
    bool hasScheduleTimes = false;
};

std::wstring GetConfigPath();
std::wstring ResolveConfigPath(const std::wstring& legacyAdjacentPath);
bool EnsureConfigFile(const std::wstring& path);
bool IsConfigComplete(const AppConfig& config);
bool LoadConfig(const std::wstring& path, AppConfig& out, std::wstring& error, bool requireCredentials = true);
bool SaveAppConfig(const std::wstring& path, const AppConfig& config);
bool SaveCoordinatesToConfig(const std::wstring& path, double latitude, double longitude);
bool SaveScheduleToConfig(const std::wstring& path, const std::string& onTime, const std::string& offTime);
int BaseUrlToDataCenterIndex(const std::string& baseUrl);
std::string DataCenterIndexToBaseUrl(int index);
