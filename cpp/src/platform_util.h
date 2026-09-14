#pragma once

#include <cstdint>
#include <string>

std::string GetAppDataDir();
std::string GetConfigPath();
std::string GetStatePath();
std::string ResolveConfigPath(const std::string& legacyAdjacentPath);

std::string ReadTextFile(const std::string& path);
bool WriteTextFile(const std::string& path, const std::string& contents);
bool EnsureDirectoryExists(const std::string& dir);
bool FileExists(const std::string& path);
bool CopyFileIfExists(const std::string& from, const std::string& to);
bool EnsureConfigFileAt(const std::string& path);

uint64_t CurrentTimeMs();
uint64_t MonotonicTimeMs();
std::string RandomHexNonce(size_t byteCount = 16);
std::string GetExeDirectory();

#ifdef _WIN32
std::wstring Utf8ToWide(const std::string& text);
std::string WideToUtf8(const std::wstring& text);
#endif
