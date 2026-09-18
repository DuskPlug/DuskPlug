#include "update_apply.h"

#include "install_kind.h"
#include "platform_util.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>

#include <string>

namespace {

std::string GetTempDirectory() {
    wchar_t path[MAX_PATH]{};
    const DWORD len = GetTempPathW(MAX_PATH, path);
    if (len == 0 || len >= MAX_PATH) {
        return ".";
    }
    return WideToUtf8(std::wstring(path, len));
}

std::string GetUpdaterPath() {
    return GetExeDirectory() + "\\DuskPlugUpdate.exe";
}

bool LaunchUpdater(const std::wstring& arguments, bool elevated) {
    const std::wstring updater = Utf8ToWide(GetUpdaterPath());
    if (elevated) {
        const HINSTANCE rc = ShellExecuteW(
            nullptr,
            L"runas",
            updater.c_str(),
            arguments.c_str(),
            nullptr,
            SW_HIDE);
        return reinterpret_cast<INT_PTR>(rc) > 32;
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::wstring command = L"\"" + updater + L"\" " + arguments;
    if (!CreateProcessW(
            nullptr,
            command.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &si,
            &pi)) {
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

std::wstring BuildCommonArgs(DWORD parentPid, const std::string& launchExe) {
    return L"--wait-pid "
        + std::to_wstring(parentPid)
        + L" --launch \""
        + Utf8ToWide(launchExe)
        + L"\"";
}

ApplyUpdateResult ApplyMsiUpdate(const UpdateInfo& info) {
    ApplyUpdateResult result{};
    if (!FileExists(GetUpdaterPath())) {
        result.error = "DuskPlugUpdate.exe is missing. Download the latest installer from GitHub.";
        return result;
    }

    const std::string downloadPath = GetTempDirectory() + "\\DuskPlug-update.msi";
    if (!DownloadToFile(info.downloadUrl, downloadPath, result.error)) {
        return result;
    }
    if (!VerifyDownload(downloadPath, info.sha256)) {
        result.error = "Downloaded MSI failed hash verification.";
        return result;
    }

    const std::string exePath = GetExeDirectory() + "\\DuskPlug.exe";
    const std::wstring arguments =
        BuildCommonArgs(GetCurrentProcessId(), exePath)
        + L" --msi \""
        + Utf8ToWide(downloadPath)
        + L"\"";

    if (!LaunchUpdater(arguments, true)) {
        result.error = "Could not start elevated updater (UAC may have been cancelled).";
        return result;
    }

    result.success = true;
    result.restartScheduled = true;
    return result;
}

ApplyUpdateResult ApplyPortableUpdate(const UpdateInfo& info) {
    ApplyUpdateResult result{};
    if (!FileExists(GetUpdaterPath())) {
        result.error = "DuskPlugUpdate.exe is missing. Download the latest ZIP from GitHub.";
        return result;
    }

    const std::string zipPath = GetTempDirectory() + "\\DuskPlug-update.zip";
    const std::string appDir = GetExeDirectory();

    if (!DownloadToFile(info.downloadUrl, zipPath, result.error)) {
        return result;
    }
    if (!VerifyDownload(zipPath, info.sha256)) {
        result.error = "Downloaded ZIP failed hash verification.";
        return result;
    }

    const std::string targetExe = appDir + "\\DuskPlug.exe";
    const std::wstring arguments =
        BuildCommonArgs(GetCurrentProcessId(), targetExe)
        + L" --portable-zip \""
        + Utf8ToWide(zipPath)
        + L"\" --app-dir \""
        + Utf8ToWide(appDir)
        + L"\"";

    if (!LaunchUpdater(arguments, false)) {
        result.error = "Could not start update helper.";
        return result;
    }

    result.success = true;
    result.restartScheduled = true;
    return result;
}

}  // namespace

ApplyUpdateResult ApplyUpdate(const UpdateInfo& info, InstallKind kind) {
    ApplyUpdateResult result{};
    if (!info.available || info.downloadUrl.empty()) {
        result.error = "No update is available to install.";
        return result;
    }

    switch (kind) {
    case InstallKind::Msi:
    case InstallKind::Winget:
        return ApplyMsiUpdate(info);
    case InstallKind::Portable:
        return ApplyPortableUpdate(info);
    default:
        result.error = "In-app updates are not supported for this install type.";
        return result;
    }
}
