#include "update_checker.h"

#include "crypto.h"
#include "http.h"
#include "json_util.h"
#include "platform_util.h"
#include "semver.h"
#include "version.h"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace {

std::string NormalizeHex(const std::string& hex) {
    std::string out;
    out.reserve(hex.size());
    for (unsigned char c : hex) {
        if (std::isxdigit(c)) {
            out.push_back(static_cast<char>(std::tolower(c)));
        }
    }
    return out;
}

std::string ReadBinaryFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

}  // namespace

const char* AssetKeyForInstallKind(InstallKind kind) {
    switch (kind) {
    case InstallKind::Msi:
    case InstallKind::Winget:
        return "windows_msi";
    case InstallKind::Portable:
        return "windows_zip";
    case InstallKind::LinuxTarball:
        return "linux_tarball";
    case InstallKind::MacAppBundle:
        return "macos_zip";
    default:
        return nullptr;
    }
}

uint64_t GetLastUpdateCheckMs() {
    const std::string json = ReadTextFile(GetStatePath());
    if (json.empty()) {
        return 0;
    }
    const auto value = JsonGetNumber(json, "LastUpdateCheckMs");
    if (!value || *value < 0) {
        return 0;
    }
    return static_cast<uint64_t>(*value);
}

void SetLastUpdateCheckMs(uint64_t ms) {
    std::string json = ReadTextFile(GetStatePath());
    if (json.empty()) {
        json = "{}";
    }

    const std::string key = "\"LastUpdateCheckMs\":";
    const std::string replacement = key + std::to_string(ms);
    const size_t keyPos = json.find(key);
    if (keyPos != std::string::npos) {
        size_t valueStart = keyPos + key.size();
        size_t valueEnd = valueStart;
        while (valueEnd < json.size()
               && (std::isdigit(static_cast<unsigned char>(json[valueEnd])) || json[valueEnd] == '.')) {
            ++valueEnd;
        }
        json.replace(keyPos, valueEnd - keyPos, replacement);
    } else {
        const size_t close = json.rfind('}');
        if (close == std::string::npos) {
            json = "{" + replacement + "}";
        } else if (close == 0 || json[close - 1] == '{') {
            json.insert(close, replacement);
        } else {
            json.insert(close, "," + replacement);
        }
    }

    WriteTextFile(GetStatePath(), json);
}

bool ShouldCheckForUpdatesNow() {
    const uint64_t last = GetLastUpdateCheckMs();
    if (last == 0) {
        return true;
    }
    return CurrentTimeMs() - last >= kUpdateCheckIntervalMs;
}

UpdateInfo CheckForUpdates(InstallKind kind) {
    UpdateInfo info{};
    SetLastUpdateCheckMs(CurrentTimeMs());

    const HttpResponse response = HttpRequest("GET", kUpdateManifestUrl, {}, std::string(), 30000);
    if (response.statusCode == 404) {
        info.manifestMissing = true;
        return info;
    }
    if (response.statusCode != 200 || response.body.empty()) {
        info.error = response.error.empty()
            ? "Could not reach the update server. Check your internet connection."
            : response.error;
        return info;
    }

    const auto version = JsonGetString(response.body, "version");
    const auto notesUrl = JsonGetString(response.body, "notes_url");
    if (!version) {
        info.error = "Update manifest is missing version.";
        return info;
    }

    info.version = *version;
    if (notesUrl) {
        info.notesUrl = *notesUrl;
    }

    if (!IsNewerVersion(info.version, DUSKPLUG_VERSION)) {
        return info;
    }

    if (!SupportsInAppUpdate(kind)) {
        info.available = true;
        return info;
    }

    const char* assetKey = AssetKeyForInstallKind(kind);
    if (!assetKey) {
        info.error = "No update asset for this install type.";
        return info;
    }

    const auto downloadUrl = JsonGetAssetField(response.body, assetKey, "url");
    const auto sha256 = JsonGetAssetField(response.body, assetKey, "sha256");
    if (!downloadUrl || !sha256) {
        info.error = "Update manifest is missing download metadata.";
        return info;
    }

    info.downloadUrl = *downloadUrl;
    info.sha256 = NormalizeHex(*sha256);
    info.available = true;
    return info;
}

bool DownloadToFile(const std::string& url, const std::string& path, std::string& error) {
    const HttpResponse response = HttpRequest("GET", url, {}, std::string(), 120000);
    if (response.statusCode != 200 || response.body.empty()) {
        error = response.error.empty() ? "Download failed." : response.error;
        return false;
    }
    if (!WriteTextFile(path, response.body)) {
        error = "Could not write downloaded file.";
        return false;
    }
    return true;
}

bool VerifyDownload(const std::string& path, const std::string& sha256Hex) {
    const std::string bytes = ReadBinaryFile(path);
    if (bytes.empty()) {
        return false;
    }
    return NormalizeHex(Sha256HexLower(bytes)) == NormalizeHex(sha256Hex);
}
