#pragma once

#include "../brightness.h"
#include "../config.h"
#include "../location_service.h"

#include "activity_linux.h"

ILocationService* CreateLinuxLocationService();
IBrightnessController* CreateLinuxBrightnessController();

bool ShowLinuxSettingsDialog(const std::string& configPath, AppConfig& config);
bool RequestLinuxLocation(double& latitude, double& longitude, std::string& error);
void OpenLinuxLocationSettings();

bool InstallLinuxAutostart(const std::string& execPath);
bool RemoveLinuxAutostart();
