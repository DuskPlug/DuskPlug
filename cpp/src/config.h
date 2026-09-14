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

bool EnsureConfigFile(const std::string& path);
bool IsConfigComplete(const AppConfig& config);
bool LoadConfig(const std::string& path, AppConfig& out, std::string& error, bool requireCredentials = true);
bool SaveAppConfig(const std::string& path, const AppConfig& config);
bool SaveCoordinatesToConfig(const std::string& path, double latitude, double longitude);
bool SaveScheduleToConfig(const std::string& path, const std::string& onTime, const std::string& offTime);
int BaseUrlToDataCenterIndex(const std::string& baseUrl);
std::string DataCenterIndexToBaseUrl(int index);

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
