#pragma once

#include "config.h"

#include <string>

struct GeoLocation {
    double latitude = 0.0;
    double longitude = 0.0;
    bool fromOs = false;
};

enum class LocationPromptResult {
    Success,
    OpenedSettings,
    Cancelled,
    Failed,
};

class ILocationService {
public:
    virtual ~ILocationService() = default;

    virtual bool TryResolve(const AppConfig& config, GeoLocation& out, std::string& error) = 0;
    virtual LocationPromptResult ResolveWithPrompt(const AppConfig& config, GeoLocation& out, std::string& error) = 0;
    virtual void OpenSettings() = 0;
};
