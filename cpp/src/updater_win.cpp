#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>

#include <string>
#include <vector>

namespace {

struct Options {
    DWORD parentPid = 0;
    std::wstring launchExe;
    std::wstring msiPath;
    std::wstring portableZip;
    std::wstring appDir;
};

void PrintUsage() {
    // Intentionally minimal output; this process is normally hidden.
}

bool ParseArgs(int argc, wchar_t** argv, Options& options) {
    const std::vector<std::wstring> args(argv + 1, argv + argc);
    for (size_t i = 0; i < args.size(); ++i) {
        const std::wstring& key = args[i];
        if (key == L"--wait-pid" && i + 1 < args.size()) {
            options.parentPid = static_cast<DWORD>(wcstoul(args[++i].c_str(), nullptr, 10));
        } else if (key == L"--launch" && i + 1 < args.size()) {
            options.launchExe = args[++i];
        } else if (key == L"--msi" && i + 1 < args.size()) {
            options.msiPath = args[++i];
        } else if (key == L"--portable-zip" && i + 1 < args.size()) {
            options.portableZip = args[++i];
        } else if (key == L"--app-dir" && i + 1 < args.size()) {
            options.appDir = args[++i];
        } else {
            return false;
        }
    }

    if (options.parentPid == 0 || options.launchExe.empty()) {
        return false;
    }
    const bool msiMode = !options.msiPath.empty();
    const bool portableMode = !options.portableZip.empty() && !options.appDir.empty();
    if (msiMode == portableMode) {
        return false;
    }
    return true;
}

bool WaitForProcess(DWORD pid) {
    for (;;) {
        HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!process) {
            return true;
        }
        const DWORD wait = WaitForSingleObject(process, 300);
        CloseHandle(process);
        if (wait == WAIT_OBJECT_0) {
            return true;
        }
        Sleep(300);
    }
}

bool RunProcess(
    const std::wstring& application,
    const std::wstring& commandLine,
    const wchar_t* workingDirectory,
    DWORD* exitCode,
    bool wait) {
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::wstring mutableCommand = commandLine;
    if (!CreateProcessW(
            application.empty() ? nullptr : application.c_str(),
            mutableCommand.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            workingDirectory,
            &si,
            &pi)) {
        return false;
    }

    if (wait) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD code = 1;
        GetExitCodeProcess(pi.hProcess, &code);
        if (exitCode) {
            *exitCode = code;
        }
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

bool ExtractZipWithTar(const std::wstring& zipPath, const std::wstring& destDir) {
    CreateDirectoryW(destDir.c_str(), nullptr);
    const std::wstring command =
        L"tar.exe -xf \"" + zipPath + L"\" -C \"" + destDir + L"\"";
    DWORD exitCode = 1;
    if (!RunProcess(L"C:\\Windows\\System32\\tar.exe", command, nullptr, &exitCode, true)) {
        return false;
    }
    return exitCode == 0;
}

bool CopyFileOverwrite(const std::wstring& from, const std::wstring& to) {
    return CopyFileW(from.c_str(), to.c_str(), FALSE) != FALSE;
}

bool CopyDirectoryRecursive(const std::wstring& fromDir, const std::wstring& toDir) {
    CreateDirectoryW(toDir.c_str(), nullptr);
    const std::wstring search = fromDir + L"\\*";
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
        const std::wstring childFrom = fromDir + L"\\" + name;
        const std::wstring childTo = toDir + L"\\" + name;
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

bool ApplyPortableUpdate(const Options& options) {
    wchar_t tempPath[MAX_PATH]{};
    const DWORD tempLen = GetTempPathW(MAX_PATH, tempPath);
    if (tempLen == 0 || tempLen >= MAX_PATH) {
        return false;
    }

    const std::wstring stagingDir = std::wstring(tempPath) + L"DuskPlug-update-staging";
    if (!ExtractZipWithTar(options.portableZip, stagingDir)) {
        return false;
    }

    const std::wstring updatedExe = stagingDir + L"\\DuskPlug.exe";
    if (GetFileAttributesW(updatedExe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return false;
    }

    if (!CopyFileOverwrite(updatedExe, options.appDir + L"\\DuskPlug.exe")) {
        return false;
    }

    const std::wstring updatedLoader = stagingDir + L"\\WebView2Loader.dll";
    if (GetFileAttributesW(updatedLoader.c_str()) != INVALID_FILE_ATTRIBUTES) {
        if (!CopyFileOverwrite(updatedLoader, options.appDir + L"\\WebView2Loader.dll")) {
            return false;
        }
    }

    const std::wstring updatedHelper = stagingDir + L"\\DuskPlugUpdate.exe";
    if (GetFileAttributesW(updatedHelper.c_str()) != INVALID_FILE_ATTRIBUTES) {
        if (!CopyFileOverwrite(updatedHelper, options.appDir + L"\\DuskPlugUpdate.exe")) {
            return false;
        }
    }

    const std::wstring updatedAssets = stagingDir + L"\\assets";
    if (GetFileAttributesW(updatedAssets.c_str()) != INVALID_FILE_ATTRIBUTES) {
        if (!CopyDirectoryRecursive(updatedAssets, options.appDir + L"\\assets")) {
            return false;
        }
    }

    return true;
}

bool ApplyMsiUpdate(const std::wstring& msiPath) {
    const std::wstring command = L"msiexec.exe /i \"" + msiPath + L"\" /quiet /norestart";
    DWORD exitCode = 1;
    if (!RunProcess(L"C:\\Windows\\System32\\msiexec.exe", command, nullptr, &exitCode, true)) {
        return false;
    }
    return exitCode == 0 || exitCode == 3010 || exitCode == 1641;
}

bool LaunchExe(const std::wstring& exePath) {
    const size_t slash = exePath.find_last_of(L"\\/");
    const std::wstring workDir = slash == std::wstring::npos ? L"." : exePath.substr(0, slash);
    std::wstring command = L"\"" + exePath + L"\"";
    return RunProcess(L"", command, workDir.c_str(), nullptr, false);
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    Options options{};
    if (!ParseArgs(argc, argv, options)) {
        PrintUsage();
        return 2;
    }

    if (!WaitForProcess(options.parentPid)) {
        return 3;
    }

    if (!options.msiPath.empty()) {
        if (!ApplyMsiUpdate(options.msiPath)) {
            return 4;
        }
    } else if (!ApplyPortableUpdate(options)) {
        return 5;
    }

    if (!LaunchExe(options.launchExe)) {
        return 6;
    }

    return 0;
}
