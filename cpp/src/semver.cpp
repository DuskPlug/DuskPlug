#include "semver.h"

#include <cctype>

std::optional<SemVer> ParseSemVer(const std::string& text) {
    SemVer version{};
    size_t pos = 0;
    if (!text.empty() && text[0] == 'v') {
        pos = 1;
    }

    auto readPart = [&](int& out) -> bool {
        if (pos >= text.size() || !std::isdigit(static_cast<unsigned char>(text[pos]))) {
            return false;
        }
        out = 0;
        while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) {
            out = out * 10 + (text[pos] - '0');
            ++pos;
        }
        return true;
    };

    if (!readPart(version.major)) {
        return std::nullopt;
    }
    if (pos >= text.size() || text[pos] != '.') {
        return std::nullopt;
    }
    ++pos;
    if (!readPart(version.minor)) {
        return std::nullopt;
    }
    if (pos >= text.size() || text[pos] != '.') {
        return std::nullopt;
    }
    ++pos;
    if (!readPart(version.patch)) {
        return std::nullopt;
    }

    return version;
}

int CompareSemVer(const SemVer& left, const SemVer& right) {
    if (left.major != right.major) {
        return left.major < right.major ? -1 : 1;
    }
    if (left.minor != right.minor) {
        return left.minor < right.minor ? -1 : 1;
    }
    if (left.patch != right.patch) {
        return left.patch < right.patch ? -1 : 1;
    }
    return 0;
}

bool IsNewerVersion(const std::string& remote, const std::string& current) {
    const auto remoteVersion = ParseSemVer(remote);
    const auto currentVersion = ParseSemVer(current);
    if (!remoteVersion || !currentVersion) {
        return false;
    }
    return CompareSemVer(*remoteVersion, *currentVersion) > 0;
}
