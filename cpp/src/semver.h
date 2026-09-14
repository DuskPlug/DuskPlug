#pragma once

#include <optional>
#include <string>

struct SemVer {
    int major = 0;
    int minor = 0;
    int patch = 0;
};

std::optional<SemVer> ParseSemVer(const std::string& text);
int CompareSemVer(const SemVer& left, const SemVer& right);
bool IsNewerVersion(const std::string& remote, const std::string& current);
