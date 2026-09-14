#pragma once

#include "install_kind.h"

#include <cstdint>
#include <string>

struct UpdateInfo {
    std::string version;
    std::string downloadUrl;
    std::string sha256;
    std::string notesUrl;
    std::string error;
    bool available = false;
};

constexpr const char* kUpdateManifestUrl =
    "https://github.com/DuskPlug/DuskPlug/releases/latest/download/latest.json";

constexpr uint64_t kUpdateCheckIntervalMs = 24ULL * 60 * 60 * 1000;

UpdateInfo CheckForUpdates(InstallKind kind);
bool DownloadToFile(const std::string& url, const std::string& path, std::string& error);
bool VerifyDownload(const std::string& path, const std::string& sha256Hex);
uint64_t GetLastUpdateCheckMs();
void SetLastUpdateCheckMs(uint64_t ms);
bool ShouldCheckForUpdatesNow();
const char* AssetKeyForInstallKind(InstallKind kind);
