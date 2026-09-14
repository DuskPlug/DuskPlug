#pragma once

#include <string>

enum class InstallKind {
    Scoop,
    Winget,
    Msi,
    Portable,
    LinuxTarball,
    MacAppBundle,
};

InstallKind DetectInstallKind();
std::string PackageManagerUpdateHint(InstallKind kind);
bool SupportsInAppUpdate(InstallKind kind);
