#include "update_apply.h"

#include "platform_util.h"

#include <cstdlib>
#include <string>
#include <unistd.h>

namespace {

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
    return std::system(command.c_str()) == 0;
}

ApplyUpdateResult ApplyLinuxTarballUpdate(const UpdateInfo& info) {
    ApplyUpdateResult result{};
    const std::string tempDir = "/tmp/duskplug-update";
    const std::string archivePath = tempDir + "/DuskPlug-Linux-x64.tar.gz";
    const std::string stagingDir = tempDir + "/staging";
    const std::string appDir = GetExeDirectory();

    RunShell("rm -rf " + ShellQuote(tempDir));
    EnsureDirectoryExists(tempDir);
    EnsureDirectoryExists(stagingDir);

    if (!DownloadToFile(info.downloadUrl, archivePath, result.error)) {
        return result;
    }
    if (!VerifyDownload(archivePath, info.sha256)) {
        result.error = "Downloaded tarball failed hash verification.";
        return result;
    }

    if (!RunShell("tar -xzf " + ShellQuote(archivePath) + " -C " + ShellQuote(stagingDir))) {
        result.error = "Could not extract update archive.";
        return result;
    }

    const std::string updatedBinary = stagingDir + "/duskplug";
    const std::string updatedAssets = stagingDir + "/assets";
    if (!FileExists(updatedBinary)) {
        result.error = "Update archive did not contain duskplug binary.";
        return result;
    }

    if (!RunShell("cp " + ShellQuote(updatedBinary) + " " + ShellQuote(appDir + "/duskplug"))) {
        result.error = "Could not replace duskplug binary.";
        return result;
    }
    RunShell("chmod +x " + ShellQuote(appDir + "/duskplug"));
    if (FileExists(updatedAssets)) {
        RunShell("rm -rf " + ShellQuote(appDir + "/assets"));
        if (!RunShell("cp -R " + ShellQuote(updatedAssets) + " " + ShellQuote(appDir + "/assets"))) {
            result.error = "Could not update assets folder.";
            return result;
        }
    }

    const std::string relaunch = appDir + "/duskplug";
    if (fork() == 0) {
        execl(relaunch.c_str(), relaunch.c_str(), static_cast<char*>(nullptr));
        _exit(1);
    }

    result.success = true;
    result.restartScheduled = true;
    return result;
}

}  // namespace

ApplyUpdateResult ApplyUpdate(const UpdateInfo& info, InstallKind kind) {
    ApplyUpdateResult result{};
    if (kind != InstallKind::LinuxTarball) {
        result.error = "In-app updates are not supported for this install type.";
        return result;
    }
    return ApplyLinuxTarballUpdate(info);
}
