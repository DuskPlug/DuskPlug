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
#include <cwchar>
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

bool UninstallKeyLooksLikeDuskPlugMsi(HKEY parent, const wchar_t* subKeyName) {
    HKEY sub = nullptr;
    if (RegOpenKeyExW(parent, subKeyName, 0, KEY_READ, &sub) != ERROR_SUCCESS) {
        return false;
    }

    wchar_t displayName[256]{};
    DWORD displaySize = sizeof(displayName);
    DWORD type = 0;
    const bool isDuskPlug = RegQueryValueExW(
                                sub,
                                L"DisplayName",
                                nullptr,
                                &type,
                                reinterpret_cast<LPBYTE>(displayName),
                                &displaySize)
            == ERROR_SUCCESS
        && type == REG_SZ
        && _wcsicmp(displayName, L"DuskPlug") == 0;

    wchar_t uninstallString[512]{};
    DWORD uninstallSize = sizeof(uninstallString);
    const bool isMsi = RegQueryValueExW(
                           sub,
                           L"UninstallString",
                           nullptr,
                           &type,
                           reinterpret_cast<LPBYTE>(uninstallString),
                           &uninstallSize)
            == ERROR_SUCCESS
        && type == REG_SZ
        && (wcsstr(uninstallString, L"msiexec") != nullptr || wcsstr(uninstallString, L"MsiExec") != nullptr);

    RegCloseKey(sub);
    return isDuskPlug && isMsi;
}

bool HiveHasDuskPlugMsi(HKEY root, REGSAM wow) {
    HKEY uninstall = nullptr;
    if (RegOpenKeyExW(
            root,
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
            0,
            KEY_READ | wow,
            &uninstall)
        != ERROR_SUCCESS) {
        return false;
    }

    wchar_t name[256];
    for (DWORD i = 0;; ++i) {
        DWORD nameLen = 256;
        if (RegEnumKeyExW(uninstall, i, name, &nameLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) {
            break;
        }
        if (UninstallKeyLooksLikeDuskPlugMsi(uninstall, name)) {
            RegCloseKey(uninstall);
            return true;
        }
    }
    RegCloseKey(uninstall);
    return false;
}

bool IsMsiInstalled() {
    constexpr const wchar_t* kLegacyUninstallKey =
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{89E986DB-8F9D-41AA-9F37-862A15944D0A}";
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, kLegacyUninstallKey, 0, KEY_READ | KEY_WOW64_64KEY, &key) == ERROR_SUCCESS) {
        RegCloseKey(key);
        return true;
    }
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kLegacyUninstallKey, 0, KEY_READ, &key) == ERROR_SUCCESS) {
        RegCloseKey(key);
        return true;
    }
    return HiveHasDuskPlugMsi(HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY)
        || HiveHasDuskPlugMsi(HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY)
        || HiveHasDuskPlugMsi(HKEY_CURRENT_USER, 0);
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
