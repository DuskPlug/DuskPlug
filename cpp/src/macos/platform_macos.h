#pragma once

#include "../brightness.h"
#include "../config.h"
#include "../location_service.h"

#include "activity_macos.h"

ILocationService* CreateMacLocationService();
MacActivityTracker* CreateMacActivityTracker();
IBrightnessController* CreateMacBrightnessController();
bool ShowMacSettingsDialog(const std::string& configPath, AppConfig& config);
bool RequestMacLocation(double& latitude, double& longitude, std::string& error);
void OpenMacLocationSettings();
bool InstallMacAutostart();
bool RemoveMacAutostart();
