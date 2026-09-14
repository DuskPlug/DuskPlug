#include "tuya_client.h"
#include "crypto.h"
#include "http.h"
#include "json_util.h"
#include "platform_util.h"

namespace {
constexpr const char* kEmptyBodySha256 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
}

TuyaClient::TuyaClient(AppConfig config) : config_(std::move(config)) {}

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
    const std::string path = "/v1.0/devices/" + config_.deviceId + "/commands";
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

bool TuyaClient::GetSwitchState(bool& on, std::string& error) {
    const std::string token = GetAccessToken(error);
    if (token.empty()) {
        return false;
    }

    const std::string path = "/v1.0/devices/" + config_.deviceId + "/status";
    const std::string json = ApiRequest("GET", path, token, "", error);
    if (json.empty()) {
        return false;
    }
    if (!JsonGetSuccess(json)) {
        error = "Status request failed";
        return false;
    }
    if (!JsonStatusValueForCode(json, config_.switchCode, on)) {
        error = "Switch code not found in status";
        return false;
    }
    return true;
}

bool TuyaClient::SetSwitch(bool on, std::string& error, unsigned long timeoutMs) {
    if (!SendCommand(config_.switchCode, on ? "true" : "false", error, timeoutMs)) {
        if (error.empty()) {
            error = "Switch command failed";
        }
        return false;
    }
    return true;
}

bool TuyaClient::SetCountdownOff(int seconds, std::string& error, unsigned long timeoutMs) {
    if (seconds < 1) {
        return false;
    }
    if (seconds > 86400) {
        seconds = 86400;
    }

    const std::string code = CountdownCodeFromSwitch(config_.switchCode);
    if (!SendCommand(code, std::to_string(seconds), error, timeoutMs)) {
        if (error.empty()) {
            error = "Countdown command failed";
        }
        return false;
    }
    return true;
}

bool TuyaClient::Toggle(std::string& error, bool& newState) {
    bool current = false;
    if (!GetSwitchState(current, error)) {
        return false;
    }
    newState = !current;
    return SetSwitch(newState, error);
}
