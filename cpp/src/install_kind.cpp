#include "install_kind.h"

#include "platform_util.h"

#include <algorithm>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>
#endif

namespace {

#ifdef _WIN32
bool ContainsIgnoreCase(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) {
        return true;
    }
    auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
    return std::search(
               haystack.begin(),
               haystack.end(),
               needle.begin(),
               needle.end(),
               [&](char a, char b) { return lower(static_cast<unsigned char>(a)) == lower(static_cast<unsigned char>(b)); })
        != haystack.end();
}

bool IsMsiInstalled() {
    constexpr const wchar_t* kUninstallKey =
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{89E986DB-8F9D-41AA-9F37-862A15944D0A}";
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, kUninstallKey, 0, KEY_READ | KEY_WOW64_64KEY, &key) == ERROR_SUCCESS) {
        RegCloseKey(key);
        return true;
    }
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kUninstallKey, 0, KEY_READ, &key) == ERROR_SUCCESS) {
        RegCloseKey(key);
        return true;
    }
    return false;
}

bool IsUnderProgramFiles(const std::string& path) {
    wchar_t programFiles[MAX_PATH]{};
    if (SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES, nullptr, SHGFP_TYPE_CURRENT, programFiles) != S_OK) {
        return false;
    }
    const std::string pf = WideToUtf8(programFiles);
    return path.rfind(pf, 0) == 0;
}
#endif

}  // namespace

InstallKind DetectInstallKind() {
#if defined(__APPLE__)
    return InstallKind::MacAppBundle;
#elif defined(__linux__)
    return InstallKind::LinuxTarball;
#else
    const std::string exeDir = GetExeDirectory();
    if (ContainsIgnoreCase(exeDir, "\\scoop\\apps\\duskplug\\")) {
        return InstallKind::Scoop;
    }
    if (IsMsiInstalled() && IsUnderProgramFiles(exeDir)) {
        return InstallKind::Msi;
    }
    if (IsUnderProgramFiles(exeDir)) {
        return InstallKind::Winget;
    }
    return InstallKind::Portable;
#endif
}

std::string PackageManagerUpdateHint(InstallKind kind) {
    switch (kind) {
    case InstallKind::Scoop:
        return "Update with: scoop update duskplug";
    case InstallKind::Winget:
        return "Update with: winget upgrade DuskPlug.DuskPlug";
    default:
        return {};
    }
}

bool SupportsInAppUpdate(InstallKind kind) {
    switch (kind) {
    case InstallKind::Scoop:
    case InstallKind::Winget:
        return false;
    case InstallKind::Msi:
    case InstallKind::Portable:
    case InstallKind::LinuxTarball:
    case InstallKind::MacAppBundle:
        return true;
    }
    return false;
}
