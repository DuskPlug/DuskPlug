#include "tuya_client.h"

#include "crypto.h"
#include "http.h"
#include "json_util.h"
#include "platform_util.h"

namespace {
constexpr const char* kEmptyBodySha256 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

bool JsonContainsCode(const std::string& json, const std::string& code) {
    return json.find("\"code\":\"" + code + "\"") != std::string::npos
        || json.find("\"code\": \"" + code + "\"") != std::string::npos;
}

std::optional<std::string> FindFirstCodeMatching(const std::string& json, const char* prefix) {
    const std::string needle = "\"code\":\"";
    size_t pos = 0;
    while ((pos = json.find(needle, pos)) != std::string::npos) {
        pos += needle.size();
        size_t end = json.find('"', pos);
        if (end == std::string::npos) {
            break;
        }
        const std::string code = json.substr(pos, end - pos);
        if (code.rfind(prefix, 0) == 0) {
            return code;
        }
        ++pos;
    }
    return std::nullopt;
}

}  // namespace

TuyaClient::TuyaClient(AppConfig config) : config_(std::move(config)) {}

const DeviceConfig* TuyaClient::LegacyDevice() const {
    return GetPrimaryDevice(config_);
}

std::string TuyaClient::CurrentTimestampMs() {
    return std::to_string(CurrentTimeMs());
}

std::string TuyaClient::NewNonce() {
    return RandomHexNonce(16);
}

std::string TuyaClient::BuildUrl(const std::string& baseUrl, const std::string& path) {
    std::string url = baseUrl;
    while (!url.empty() && url.back() == '/') {
        url.pop_back();
    }
    url += path;
    return url;
}

std::string TuyaClient::CountdownCodeFromSwitch(const std::string& switchCode) {
    if (switchCode.size() >= 6 && switchCode.compare(0, 6, "switch") == 0) {
        return "countdown" + switchCode.substr(6);
    }
    return "countdown_1";
}

int TuyaClient::BrightnessPercentToRaw(const DeviceConfig& device, int percent) {
    const int clamped = ClampDeviceBrightnessPercent(percent);
    const int minValue = device.capabilities.brightnessMin;
    const int maxValue = device.capabilities.brightnessMax;
    if (maxValue <= minValue) {
        return clamped * 10;
    }
    return minValue + ((maxValue - minValue) * clamped) / 100;
}

int TuyaClient::BrightnessRawToPercent(const DeviceConfig& device, int raw) {
    const int minValue = device.capabilities.brightnessMin;
    const int maxValue = device.capabilities.brightnessMax;
    if (maxValue <= minValue) {
        return ClampDeviceBrightnessPercent(raw / 10);
    }
    const int scaled = ((raw - minValue) * 100) / (maxValue - minValue);
    return ClampDeviceBrightnessPercent(scaled);
}

std::string TuyaClient::ApiRequest(
    const std::string& method,
    const std::string& path,
    const std::string& accessToken,
    const std::string& body,
    std::string& error,
    unsigned long timeoutMs) {
    const std::string timestamp = CurrentTimestampMs();
    const std::string nonce = NewNonce();
    const std::string contentSha256 = body.empty() ? kEmptyBodySha256 : Sha256HexLower(body);
    const std::string stringToSign = method + "\n" + contentSha256 + "\n\n" + path;

    std::string signMessage;
    if (accessToken.empty()) {
        signMessage = config_.clientId + timestamp + nonce + stringToSign;
    } else {
        signMessage = config_.clientId + accessToken + timestamp + nonce + stringToSign;
    }

    const std::string sign = HmacSha256HexUpper(config_.clientSecret, signMessage);

    std::map<std::string, std::string> headers{
        {"client_id", config_.clientId},
        {"sign", sign},
        {"sign_method", "HMAC-SHA256"},
        {"t", timestamp},
        {"nonce", nonce},
        {"lang", "en"},
    };
    if (!accessToken.empty()) {
        headers["access_token"] = accessToken;
    }

    const std::string url = BuildUrl(config_.baseUrl, path);
    const HttpResponse response = HttpRequest(method, url, headers, body, timeoutMs);

    if (!response.error.empty()) {
        error = "Network error";
        return {};
    }

    if (response.body.empty()) {
        error = "Empty response";
        return {};
    }

    return response.body;
}

std::string TuyaClient::GetAccessToken(std::string& error, unsigned long timeoutMs) {
    const uint64_t now = MonotonicTimeMs();
    if (!cachedAccessToken_.empty() && now < tokenValidUntilMs_) {
        return cachedAccessToken_;
    }

    const std::string json = ApiRequest("GET", "/v1.0/token?grant_type=1", "", "", error, timeoutMs);
    if (json.empty()) {
        return {};
    }
    if (!JsonGetSuccess(json)) {
        error = "Token request failed";
        cachedAccessToken_.clear();
        tokenValidUntilMs_ = 0;
        return {};
    }
    const auto token = JsonGetNestedString(json, "result", "access_token");
    if (!token) {
        error = "Token missing in response";
        cachedAccessToken_.clear();
        tokenValidUntilMs_ = 0;
        return {};
    }

    cachedAccessToken_ = *token;
    tokenValidUntilMs_ = now + 50ULL * 60 * 1000;
    return cachedAccessToken_;
}

bool TuyaClient::SendCommand(
    const std::string& deviceId,
    const std::string& code,
    const std::string& valueJson,
    std::string& error,
    unsigned long timeoutMs) {
    const std::string token = GetAccessToken(error, timeoutMs);
    if (token.empty()) {
        return false;
    }

    const std::string body =
        std::string(R"({"commands":[{"code":")") + code + R"(","value":)" + valueJson + "}]}";
    const std::string path = "/v1.0/devices/" + deviceId + "/commands";
    const std::string json = ApiRequest("POST", path, token, body, error, timeoutMs);
    if (json.empty()) {
        return false;
    }
    if (!JsonGetSuccess(json)) {
        error = "Device command failed";
        return false;
    }
    return true;
}

bool TuyaClient::GetDeviceState(const DeviceConfig& device, DeviceState& out, std::string& error) {
    out = DeviceState{};

    const std::string token = GetAccessToken(error);
    if (token.empty()) {
        return false;
    }

    const std::string path = "/v1.0/devices/" + device.id + "/status";
    const std::string json = ApiRequest("GET", path, token, "", error);
    if (json.empty()) {
        return false;
    }
    if (!JsonGetSuccess(json)) {
        error = "Status request failed";
        return false;
    }

    bool switchOn = false;
    if (JsonStatusValueForCode(json, device.capabilities.switchCode, switchOn)) {
        out.hasSwitch = true;
        out.switchOn = switchOn;
    } else {
        error = "Switch code not found in status";
        return false;
    }

    if (!device.capabilities.brightnessCode.empty()) {
        int raw = 0;
        if (JsonStatusIntForCode(json, device.capabilities.brightnessCode, raw)) {
            out.hasBrightness = true;
            out.brightnessRaw = raw;
            out.brightnessPercent = BrightnessRawToPercent(device, raw);
        }
    }

    return true;
}

bool TuyaClient::SetSwitch(const DeviceConfig& device, bool on, std::string& error, unsigned long timeoutMs) {
    if (!SendCommand(device.id, device.capabilities.switchCode, on ? "true" : "false", error, timeoutMs)) {
        if (error.empty()) {
            error = "Switch command failed";
        }
        return false;
    }
    return true;
}

bool TuyaClient::SetBrightness(const DeviceConfig& device, int percent, std::string& error, unsigned long timeoutMs) {
    if (device.capabilities.brightnessCode.empty()) {
        error = "Device has no brightness capability";
        return false;
    }

    const int raw = BrightnessPercentToRaw(device, percent);
    if (!SendCommand(device.id, device.capabilities.brightnessCode, std::to_string(raw), error, timeoutMs)) {
        if (error.empty()) {
            error = "Brightness command failed";
        }
        return false;
    }
    return true;
}

bool TuyaClient::SetCountdownOff(const DeviceConfig& device, int seconds, std::string& error, unsigned long timeoutMs) {
    if (seconds < 1) {
        return false;
    }
    if (seconds > 86400) {
        seconds = 86400;
    }

    const std::string code = CountdownCodeFromSwitch(device.capabilities.switchCode);
    if (!SendCommand(device.id, code, std::to_string(seconds), error, timeoutMs)) {
        if (error.empty()) {
            error = "Countdown command failed";
        }
        return false;
    }
    return true;
}

bool TuyaClient::Toggle(const DeviceConfig& device, std::string& error, bool& newState) {
    DeviceState state{};
    if (!GetDeviceState(device, state, error)) {
        return false;
    }
    newState = !state.switchOn;
    return SetSwitch(device, newState, error);
}

bool TuyaClient::DiscoverFunctions(const std::string& deviceId, DiscoveredCapabilities& out, std::string& error) {
    out = DiscoveredCapabilities{};

    const std::string token = GetAccessToken(error);
    if (token.empty()) {
        return false;
    }

    const std::string path = "/v1.0/devices/" + deviceId + "/functions";
    const std::string json = ApiRequest("GET", path, token, "", error);
    if (json.empty()) {
        return false;
    }
    if (!JsonGetSuccess(json)) {
        error = "Functions request failed";
        return false;
    }

    if (auto switchCode = FindFirstCodeMatching(json, "switch")) {
        out.capabilities.switchCode = *switchCode;
    } else {
        error = "No switch function found on device";
        return false;
    }

    if (auto brightnessCode = FindFirstCodeMatching(json, "bright_value")) {
        out.capabilities.brightnessCode = *brightnessCode;
        out.suggestedType = DeviceType::Bulb;
    } else if (JsonContainsCode(json, "bright_value_v2")) {
        out.capabilities.brightnessCode = "bright_value_v2";
        out.suggestedType = DeviceType::Bulb;
    } else if (JsonContainsCode(json, "bright_value")) {
        out.capabilities.brightnessCode = "bright_value";
        out.suggestedType = DeviceType::Bulb;
    } else if (out.capabilities.switchCode.find("switch_led") != std::string::npos) {
        out.suggestedType = DeviceType::Bulb;
    }

    out.capabilities.brightnessMin = 10;
    out.capabilities.brightnessMax = 1000;
    return true;
}

bool TuyaClient::GetSwitchState(bool& on, std::string& error) {
    const DeviceConfig* device = LegacyDevice();
    if (!device) {
        error = "No device configured";
        return false;
    }
    DeviceState state{};
    if (!GetDeviceState(*device, state, error)) {
        return false;
    }
    on = state.switchOn;
    return true;
}

bool TuyaClient::SetSwitch(bool on, std::string& error, unsigned long timeoutMs) {
    const DeviceConfig* device = LegacyDevice();
    if (!device) {
        error = "No device configured";
        return false;
    }
    return SetSwitch(*device, on, error, timeoutMs);
}

bool TuyaClient::SetCountdownOff(int seconds, std::string& error, unsigned long timeoutMs) {
    const DeviceConfig* device = LegacyDevice();
    if (!device) {
        error = "No device configured";
        return false;
    }
    return SetCountdownOff(*device, seconds, error, timeoutMs);
}

bool TuyaClient::Toggle(std::string& error, bool& newState) {
    const DeviceConfig* device = LegacyDevice();
    if (!device) {
        error = "No device configured";
        return false;
    }
    return Toggle(*device, error, newState);
}
