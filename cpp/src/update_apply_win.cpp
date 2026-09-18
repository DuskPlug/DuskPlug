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

bool LaunchDetachedPowerShell(const std::wstring& arguments) {
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::wstring command = L"powershell.exe " + arguments;
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

bool LaunchElevatedPowerShell(const std::wstring& arguments) {
    const HINSTANCE rc = ShellExecuteW(
        nullptr,
        L"runas",
        L"powershell.exe",
        arguments.c_str(),
        nullptr,
        SW_HIDE);
    return reinterpret_cast<INT_PTR>(rc) > 32;
}

bool WriteMsiRestartHelperScript(const std::string& scriptPath) {
    const std::string script =
        "param(\r\n"
        "  [Parameter(Mandatory=$true)][int] $ParentPid,\r\n"
        "  [Parameter(Mandatory=$true)][string] $MsiPath,\r\n"
        "  [Parameter(Mandatory=$true)][string] $ExePath\r\n"
        ")\r\n"
        "while (Get-Process -Id $ParentPid -ErrorAction SilentlyContinue) {\r\n"
        "  Start-Sleep -Milliseconds 300\r\n"
        "}\r\n"
        "$proc = Start-Process -FilePath 'msiexec.exe' -ArgumentList @('/i', $MsiPath, '/quiet', '/norestart') -PassThru -Wait\r\n"
        "if ($proc.ExitCode -eq 0 -or $proc.ExitCode -eq 3010 -or $proc.ExitCode -eq 1641) {\r\n"
        "  Start-Process -FilePath $ExePath\r\n"
        "}\r\n";

    return WriteTextFile(scriptPath, script);
}

bool WritePortableRestartHelperScript(const std::string& scriptPath) {
    const std::string script =
        "param(\r\n"
        "  [Parameter(Mandatory=$true)][int] $ParentPid,\r\n"
        "  [Parameter(Mandatory=$true)][string] $StagingDir,\r\n"
        "  [Parameter(Mandatory=$true)][string] $AppDir,\r\n"
        "  [Parameter(Mandatory=$true)][string] $ExePath\r\n"
        ")\r\n"
        "while (Get-Process -Id $ParentPid -ErrorAction SilentlyContinue) {\r\n"
        "  Start-Sleep -Milliseconds 300\r\n"
        "}\r\n"
        "$exe = Join-Path $StagingDir 'DuskPlug.exe'\r\n"
        "if (-not (Test-Path -LiteralPath $exe)) { exit 1 }\r\n"
        "Copy-Item -LiteralPath $exe -Destination (Join-Path $AppDir 'DuskPlug.exe') -Force\r\n"
        "$loader = Join-Path $StagingDir 'WebView2Loader.dll'\r\n"
        "if (Test-Path -LiteralPath $loader) {\r\n"
        "  Copy-Item -LiteralPath $loader -Destination (Join-Path $AppDir 'WebView2Loader.dll') -Force\r\n"
        "}\r\n"
        "$assets = Join-Path $StagingDir 'assets'\r\n"
        "$targetAssets = Join-Path $AppDir 'assets'\r\n"
        "if (Test-Path -LiteralPath $assets) {\r\n"
        "  New-Item -ItemType Directory -Force -Path $targetAssets | Out-Null\r\n"
        "  Copy-Item -LiteralPath (Join-Path $assets '*') -Destination $targetAssets -Recurse -Force\r\n"
        "}\r\n"
        "Start-Process -FilePath $ExePath\r\n";

    return WriteTextFile(scriptPath, script);
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

    const std::string scriptPath = GetTempDirectory() + "\\DuskPlug-apply-update.ps1";
    const std::string exePath = GetExeDirectory() + "\\DuskPlug.exe";
    if (!WriteMsiRestartHelperScript(scriptPath)) {
        result.error = "Could not prepare update restart helper.";
        return result;
    }

    const std::wstring arguments =
        L"-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \""
        + Utf8ToWide(scriptPath)
        + L"\" -ParentPid "
        + std::to_wstring(GetCurrentProcessId())
        + L" -MsiPath \""
        + Utf8ToWide(downloadPath)
        + L"\" -ExePath \""
        + Utf8ToWide(exePath)
        + L"\"";

    if (!LaunchElevatedPowerShell(arguments)) {
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

    if (!FileExists(stagingDir + "\\DuskPlug.exe")) {
        result.error = "Update archive did not contain DuskPlug.exe.";
        return result;
    }

    const std::string scriptPath = tempDir + "\\DuskPlug-apply-portable-update.ps1";
    const std::string targetExe = appDir + "\\DuskPlug.exe";
    if (!WritePortableRestartHelperScript(scriptPath)) {
        result.error = "Could not prepare update restart helper.";
        return result;
    }

    const std::wstring arguments =
        L"-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \""
        + Utf8ToWide(scriptPath)
        + L"\" -ParentPid "
        + std::to_wstring(GetCurrentProcessId())
        + L" -StagingDir \""
        + Utf8ToWide(stagingDir)
        + L"\" -AppDir \""
        + Utf8ToWide(appDir)
        + L"\" -ExePath \""
        + Utf8ToWide(targetExe)
        + L"\"";

    if (!LaunchDetachedPowerShell(arguments)) {
        result.error = "Could not start update restart helper.";
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
