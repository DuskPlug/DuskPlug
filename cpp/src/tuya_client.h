#pragma once
#include "config.h"
#include <cstdint>
#include <string>

class TuyaClient {
public:
    explicit TuyaClient(AppConfig config);

    bool GetSwitchState(bool& on, std::string& error);
    bool SetSwitch(bool on, std::string& error, unsigned long timeoutMs = 0);
    bool SetCountdownOff(int seconds, std::string& error, unsigned long timeoutMs = 0);
    bool Toggle(std::string& error, bool& newState);

private:
    AppConfig config_;
    std::string cachedAccessToken_;
    uint64_t tokenValidUntilMs_ = 0;

    std::string GetAccessToken(std::string& error, unsigned long timeoutMs = 0);
    std::string ApiRequest(
        const std::string& method,
        const std::string& path,
        const std::string& accessToken,
        const std::string& body,
        std::string& error,
        unsigned long timeoutMs = 0);
    bool SendCommand(const std::string& code, const std::string& valueJson, std::string& error, unsigned long timeoutMs);
    static std::string CountdownCodeFromSwitch(const std::string& switchCode);
    static std::string NewNonce();
    static std::string CurrentTimestampMs();
    static std::string BuildUrl(const std::string& baseUrl, const std::string& path);
};
