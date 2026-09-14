#include "update_apply.h"

#include "install_kind.h"
#include "platform_util.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

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

bool RunCommand(const std::wstring& command) {
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::wstring mutableCommand = command;
    if (!CreateProcessW(
            nullptr,
            mutableCommand.data(),
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
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return exitCode == 0;
}

bool CopyFileOverwrite(const std::string& from, const std::string& to) {
    return CopyFileW(Utf8ToWide(from).c_str(), Utf8ToWide(to).c_str(), FALSE) != FALSE;
}

bool CopyDirectoryRecursive(const std::string& fromDir, const std::string& toDir) {
    EnsureDirectoryExists(toDir);
    const std::wstring search = Utf8ToWide(fromDir + "\\*");
    WIN32_FIND_DATAW data{};
    const HANDLE handle = FindFirstFileW(search.c_str(), &data);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    bool ok = true;
    do {
        const std::wstring name = data.cFileName;
        if (name == L"." || name == L"..") {
            continue;
        }
        const std::string childFrom = fromDir + "\\" + WideToUtf8(name);
        const std::string childTo = toDir + "\\" + WideToUtf8(name);
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!CopyDirectoryRecursive(childFrom, childTo)) {
                ok = false;
            }
        } else if (!CopyFileOverwrite(childFrom, childTo)) {
            ok = false;
        }
    } while (FindNextFileW(handle, &data));

    FindClose(handle);
    return ok;
}

bool LaunchUpdatedExe(const std::string& exePath) {
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::wstring command = L"\"" + Utf8ToWide(exePath) + L"\"";
    if (!CreateProcessW(
            nullptr,
            command.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            Utf8ToWide(GetExeDirectory()).c_str(),
            &si,
            &pi)) {
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

ApplyUpdateResult ApplyMsiUpdate(const UpdateInfo& info) {
    ApplyUpdateResult result{};
    const std::string downloadPath = GetTempDirectory() + "\\DuskPlug-update.msi";
    if (!DownloadToFile(info.downloadUrl, downloadPath, result.error)) {
        return result;
    }
    if (!VerifyDownload(downloadPath, info.sha256)) {
        result.error = "Downloaded MSI failed hash verification.";
        return result;
    }

    const std::wstring args = L"/i \"" + Utf8ToWide(downloadPath) + L"\" /quiet /norestart";
    const HINSTANCE rc = ShellExecuteW(
        nullptr,
        L"runas",
        L"msiexec.exe",
        args.c_str(),
        nullptr,
        SW_HIDE);
    if (reinterpret_cast<INT_PTR>(rc) <= 32) {
        result.error = "Could not start elevated installer (UAC may have been cancelled).";
        return result;
    }

    result.success = true;
    result.restartScheduled = true;
    return result;
}

ApplyUpdateResult ApplyPortableUpdate(const UpdateInfo& info) {
    ApplyUpdateResult result{};
    const std::string tempDir = GetTempDirectory();
    const std::string zipPath = tempDir + "\\DuskPlug-update.zip";
    const std::string stagingDir = tempDir + "\\DuskPlug-update-staging";
    const std::string appDir = GetExeDirectory();

    if (!DownloadToFile(info.downloadUrl, zipPath, result.error)) {
        return result;
    }
    if (!VerifyDownload(zipPath, info.sha256)) {
        result.error = "Downloaded ZIP failed hash verification.";
        return result;
    }

    const std::wstring extractCommand =
        L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \""
        L"Expand-Archive -LiteralPath '" + Utf8ToWide(zipPath)
        + L"' -DestinationPath '" + Utf8ToWide(stagingDir) + L"' -Force\"";
    if (!RunCommand(extractCommand)) {
        result.error = "Could not extract update archive.";
        return result;
    }

    const std::string updatedExe = stagingDir + "\\DuskPlug.exe";
    const std::string updatedLoader = stagingDir + "\\WebView2Loader.dll";
    const std::string updatedAssets = stagingDir + "\\assets";
    if (!FileExists(updatedExe)) {
        result.error = "Update archive did not contain DuskPlug.exe.";
        return result;
    }

    const std::string targetExe = appDir + "\\DuskPlug.exe";
    if (!CopyFileOverwrite(updatedExe, targetExe)) {
        result.error = "Could not replace DuskPlug.exe.";
        return result;
    }
    if (FileExists(updatedLoader) && !CopyFileOverwrite(updatedLoader, appDir + "\\WebView2Loader.dll")) {
        result.error = "Could not update WebView2Loader.dll.";
        return result;
    }
    if (FileExists(updatedAssets) && !CopyDirectoryRecursive(updatedAssets, appDir + "\\assets")) {
        result.error = "Could not update assets folder.";
        return result;
    }

    if (!LaunchUpdatedExe(targetExe)) {
        result.error = "Update installed but restart failed.";
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
        return ApplyMsiUpdate(info);
    case InstallKind::Portable:
        return ApplyPortableUpdate(info);
    default:
        result.error = "In-app updates are not supported for this install type.";
        return result;
    }
}
