#pragma once

#include "install_kind.h"
#include "update_checker.h"

#include <string>

struct ApplyUpdateResult {
    bool success = false;
    std::string error;
    bool restartScheduled = false;
};

ApplyUpdateResult ApplyUpdate(const UpdateInfo& info, InstallKind kind);
