#include "platform_util.h"

#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <random>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <shlobj.h>
#include <windows.h>
#else
#include <pwd.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#endif

namespace {

#ifndef _WIN32
std::string GetHomeDir() {
    if (const char* home = getenv("HOME")) {
        return home;
    }
    if (passwd* pw = getpwuid(getuid())) {
        return pw->pw_dir;
    }
    return ".";
}
#endif

bool IsDir(const std::string& path) {
#ifdef _WIN32
    const DWORD attrs = GetFileAttributesW(Utf8ToWide(path).c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st{};
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

}  // namespace

#ifdef _WIN32
std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return L"";
    }
    const int len = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (len <= 1) {
        return L"";
    }
    std::wstring out(static_cast<size_t>(len - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), len);
    return out;
}

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    const int len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1) {
        return {};
    }
    std::string out(static_cast<size_t>(len - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, out.data(), len, nullptr, nullptr);
    return out;
}
#endif

std::string GetAppDataDir() {
#ifdef _WIN32
    wchar_t appData[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appData))) {
        return "SMART";
    }
    std::string path = WideToUtf8(appData) + "\\SMART";
#else
#if defined(__APPLE__)
    const std::string path = GetHomeDir() + "/Library/Application Support/DuskPlug";
#else
    const std::string path = GetHomeDir() + "/.config/duskplug";
#endif
#endif
    EnsureDirectoryExists(path);
    return path;
}

std::string GetConfigPath() {
    return GetAppDataDir() + (
#ifdef _WIN32
        "\\config.json"
#else
        "/config.json"
#endif
    );
}

std::string GetStatePath() {
    return GetAppDataDir() + (
#ifdef _WIN32
        "\\state.json"
#else
        "/state.json"
#endif
    );
}

std::string ResolveConfigPath(const std::string& legacyAdjacentPath) {
    const std::string appDataPath = GetConfigPath();
    if (FileExists(appDataPath)) {
        return appDataPath;
    }
    if (CopyFileIfExists(legacyAdjacentPath, appDataPath) && FileExists(appDataPath)) {
        return appDataPath;
    }
    return appDataPath;
}

bool EnsureDirectoryExists(const std::string& dir) {
    if (dir.empty()) {
        return false;
    }
    if (IsDir(dir)) {
        return true;
    }
#ifdef _WIN32
    return CreateDirectoryW(Utf8ToWide(dir).c_str(), nullptr) != FALSE || GetLastError() == ERROR_ALREADY_EXISTS;
#else
    return mkdir(dir.c_str(), 0755) == 0 || errno == EEXIST;
#endif
}

bool FileExists(const std::string& path) {
#ifdef _WIN32
    return GetFileAttributesW(Utf8ToWide(path).c_str()) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st{};
    return stat(path.c_str(), &st) == 0;
#endif
}

bool CopyFileIfExists(const std::string& from, const std::string& to) {
    if (!FileExists(from)) {
        return false;
    }
#ifdef _WIN32
    return CopyFileW(Utf8ToWide(from).c_str(), Utf8ToWide(to).c_str(), FALSE) != FALSE;
#else
    std::ifstream in(from, std::ios::binary);
    std::ofstream out(to, std::ios::binary);
    if (!in || !out) {
        return false;
    }
    out << in.rdbuf();
    return out.good();
#endif
}

std::string ReadTextFile(const std::string& path) {
#ifdef _WIN32
    const std::wstring widePath = Utf8ToWide(path);
    HANDLE file = CreateFileW(
        widePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return {};
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 1024 * 1024) {
        CloseHandle(file);
        return {};
    }

    std::string contents(static_cast<size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    const BOOL ok = ReadFile(file, contents.data(), static_cast<DWORD>(contents.size()), &read, nullptr);
    CloseHandle(file);
    if (!ok || read == 0) {
        return {};
    }
    contents.resize(read);
#else
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
#endif

    if (contents.size() >= 3
        && static_cast<unsigned char>(contents[0]) == 0xEF
        && static_cast<unsigned char>(contents[1]) == 0xBB
        && static_cast<unsigned char>(contents[2]) == 0xBF) {
        contents.erase(0, 3);
    }
    return contents;
}

bool WriteTextFile(const std::string& path, const std::string& contents) {
    const size_t slash = path.find_last_of("/\\");
    if (slash != std::string::npos) {
        EnsureDirectoryExists(path.substr(0, slash));
    }

#ifdef _WIN32
    HANDLE file = CreateFileW(
        Utf8ToWide(path).c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written = 0;
    const BOOL ok = WriteFile(
        file,
        contents.data(),
        static_cast<DWORD>(contents.size()),
        &written,
        nullptr);
    CloseHandle(file);
    return ok && written == contents.size();
#else
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    return file.good();
#endif
}

bool EnsureConfigFileAt(const std::string& path) {
    if (FileExists(path)) {
        return true;
    }
    static const char* kDefaultConfigTemplate = R"({
  "ClientId": "",
  "ClientSecret": "",
  "DeviceId": "",
  "BaseUrl": "https://openapi.tuyaeu.com",
  "SwitchCode": "switch_1",
  "Latitude": 0,
  "Longitude": 0,
  "DarkOffsetMinutes": 0,
  "LightOffsetMinutes": 0,
  "LockOffSeconds": 30,
  "ScheduleOnTime": "18:00",
  "ScheduleOffTime": "23:00"
})";
    return WriteTextFile(path, kDefaultConfigTemplate);
}

uint64_t CurrentTimeMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}

uint64_t MonotonicTimeMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

std::string RandomHexNonce(size_t byteCount) {
    static const char* hex = "0123456789abcdef";
    std::vector<unsigned char> bytes(byteCount);

#ifdef _WIN32
    HCRYPTPROV prov = 0;
    if (CryptAcquireContextW(&prov, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        CryptGenRandom(prov, static_cast<DWORD>(bytes.size()), bytes.data());
        CryptReleaseContext(prov, 0);
    }
#else
    std::random_device rd;
    for (size_t i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<unsigned char>(rd());
    }
#endif

    std::string nonce(byteCount * 2, '0');
    for (size_t i = 0; i < bytes.size(); ++i) {
        nonce[i * 2] = hex[(bytes[i] >> 4) & 0xF];
        nonce[i * 2 + 1] = hex[bytes[i] & 0xF];
    }
    return nonce;
}

std::string GetExeDirectory() {
#ifdef _WIN32
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring full(path);
    const size_t pos = full.find_last_of(L"\\/");
    return pos == std::wstring::npos ? "." : WideToUtf8(full.substr(0, pos));
#else
    char path[4096]{};
#if defined(__APPLE__)
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0) {
        return ".";
    }
#else
    const ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len <= 0) {
        return ".";
    }
    path[len] = '\0';
#endif
    std::string full(path);
    const size_t pos = full.find_last_of("/\\");
    return pos == std::string::npos ? "." : full.substr(0, pos);
#endif
}
