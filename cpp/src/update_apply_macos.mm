#include "update_apply.h"

#include "platform_util.h"

#include <string>
#include <unistd.h>

namespace {

std::string GetMacAppBundlePath() {
    std::string dir = GetExeDirectory();
    const std::string suffix = "/Contents/MacOS";
    if (dir.size() >= suffix.size() && dir.compare(dir.size() - suffix.size(), suffix.size(), suffix) == 0) {
        return dir.substr(0, dir.size() - suffix.size());
    }
    return dir;
}

std::string ShellQuote(const std::string& value) {
    std::string quoted = "'";
    for (char c : value) {
        if (c == '\'') {
            quoted += "'\\''";
        } else {
            quoted.push_back(c);
        }
    }
    quoted.push_back('\'');
    return quoted;
}

bool RunShell(const std::string& command) {
    return system(command.c_str()) == 0;
}

ApplyUpdateResult ApplyMacZipUpdate(const UpdateInfo& info) {
    ApplyUpdateResult result{};
    const std::string tempDir = "/tmp/duskplug-update";
    const std::string zipPath = tempDir + "/DuskPlug-macOS.zip";
    const std::string stagingDir = tempDir + "/staging";
    const std::string bundlePath = GetMacAppBundlePath();
    const std::string bundleDir = bundlePath.substr(0, bundlePath.find_last_of('/'));

    RunShell("rm -rf " + ShellQuote(tempDir));
    EnsureDirectoryExists(tempDir);
    EnsureDirectoryExists(stagingDir);

    if (!DownloadToFile(info.downloadUrl, zipPath, result.error)) {
        return result;
    }
    if (!VerifyDownload(zipPath, info.sha256)) {
        result.error = "Downloaded ZIP failed hash verification.";
        return result;
    }

    if (!RunShell("ditto -xk " + ShellQuote(zipPath) + " " + ShellQuote(stagingDir))) {
        result.error = "Could not extract update archive.";
        return result;
    }

    const std::string updatedBundle = stagingDir + "/DuskPlug.app";
    if (!FileExists(updatedBundle + "/Contents/MacOS/DuskPlug")) {
        result.error = "Update archive did not contain DuskPlug.app.";
        return result;
    }

    RunShell("rm -rf " + ShellQuote(bundlePath));
    if (!RunShell("ditto " + ShellQuote(updatedBundle) + " " + ShellQuote(bundlePath))) {
        result.error = "Could not replace DuskPlug.app.";
        return result;
    }

    if (fork() == 0) {
        execl("/usr/bin/open", "open", "-a", bundlePath.c_str(), static_cast<char*>(nullptr));
        _exit(1);
    }

    result.success = true;
    result.restartScheduled = true;
    return result;
}

}  // namespace

ApplyUpdateResult ApplyUpdate(const UpdateInfo& info, InstallKind kind) {
    ApplyUpdateResult result{};
    if (kind != InstallKind::MacAppBundle) {
        result.error = "In-app updates are not supported for this install type.";
        return result;
    }
    return ApplyMacZipUpdate(info);
}
