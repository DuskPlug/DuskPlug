#pragma once

#include "../config.h"
#include "../location_service.h"

#include "activity_macos.h"

ILocationService* CreateMacLocationService();
MacActivityTracker* CreateMacActivityTracker();
bool ShowMacSettingsDialog(const std::string& configPath, AppConfig& config);
bool InstallMacAutostart();
bool RemoveMacAutostart();
