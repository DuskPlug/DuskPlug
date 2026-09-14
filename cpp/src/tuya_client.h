#pragma once

#include "config.h"

// wingdi.h (via windows.h) defines DeviceCapabilities as a Win32 API macro.
#ifdef DeviceCapabilities
#undef DeviceCapabilities
#endif

#include <cstdint>
#include <string>

struct DeviceState {
    bool hasSwitch = false;
    bool switchOn = false;
    bool hasBrightness = false;
    int brightnessRaw = 0;
    int brightnessPercent = 0;
};

struct DiscoveredCapabilities {
    DeviceType suggestedType = DeviceType::Plug;
    DeviceCapabilities capabilities;
};

class TuyaClient {
public:
    explicit TuyaClient(AppConfig config);

    bool GetDeviceState(const DeviceConfig& device, DeviceState& out, std::string& error);
    bool SetSwitch(const DeviceConfig& device, bool on, std::string& error, unsigned long timeoutMs = 0);
    bool SetBrightness(const DeviceConfig& device, int percent, std::string& error, unsigned long timeoutMs = 0);
    bool SetCountdownOff(const DeviceConfig& device, int seconds, std::string& error, unsigned long timeoutMs = 0);
    bool Toggle(const DeviceConfig& device, std::string& error, bool& newState);
    bool DiscoverFunctions(const std::string& deviceId, DiscoveredCapabilities& out, std::string& error);

    bool GetSwitchState(bool& on, std::string& error);
    bool SetSwitch(bool on, std::string& error, unsigned long timeoutMs = 0);
    bool SetCountdownOff(int seconds, std::string& error, unsigned long timeoutMs = 0);
    bool Toggle(std::string& error, bool& newState);

    static int BrightnessPercentToRaw(const DeviceConfig& device, int percent);
    static int BrightnessRawToPercent(const DeviceConfig& device, int raw);

private:
    AppConfig config_;
    std::string cachedAccessToken_;
    uint64_t tokenValidUntilMs_ = 0;

    const DeviceConfig* LegacyDevice() const;

    std::string GetAccessToken(std::string& error, unsigned long timeoutMs = 0);
    std::string ApiRequest(
        const std::string& method,
        const std::string& path,
        const std::string& accessToken,
        const std::string& body,
        std::string& error,
        unsigned long timeoutMs = 0);
    bool SendCommand(
        const std::string& deviceId,
        const std::string& code,
        const std::string& valueJson,
        std::string& error,
        unsigned long timeoutMs);
    static std::string CountdownCodeFromSwitch(const std::string& switchCode);
    static std::string NewNonce();
    static std::string CurrentTimestampMs();
    static std::string BuildUrl(const std::string& baseUrl, const std::string& path);
};
