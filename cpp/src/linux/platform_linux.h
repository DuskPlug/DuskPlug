#pragma once

#include "../config.h"
#include "../location_service.h"

#include "activity_linux.h"

ILocationService* CreateLinuxLocationService();

bool ShowLinuxSettingsDialog(const std::string& configPath, AppConfig& config);

bool InstallLinuxAutostart(const std::string& execPath);
bool RemoveLinuxAutostart();
