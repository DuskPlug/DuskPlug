#pragma once

#include "config.h"

#include <string>

struct SettingsWebResult {
    enum class Kind {
        None,
        Saved,
        Cancel,
        DetectLocation,
        OpenLocationSettings,
        SetLocation,
        RunScript
    };

    Kind kind = Kind::None;
    std::string script;
    double latitude = 0.0;
    double longitude = 0.0;
};

std::string LoadSettingsHtml();
std::string BuildSettingsBootJson(const AppConfig& config, const char* platform);
std::string InjectBrandMark(const std::string& html);
std::string InjectSettingsBoot(const std::string& html, const std::string& bootJson);
std::string PrepareSettingsHtml(const std::string& html, const std::string& bootJson);
std::string JsCallSetLocation(double latitude, double longitude);
std::string JsCallShowError(const std::string& message, bool openLocationSettings);
std::string JsonEscape(const std::string& text);

bool ApplySettingsFromJson(const std::string& json, AppConfig& config, std::string& error);
SettingsWebResult HandleSettingsWebMessage(
    const std::string& message,
    const std::string& configPath,
    AppConfig& config);
