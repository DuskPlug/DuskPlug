#include "coords.h"

#include <cctype>
#include <cmath>
#include <cstring>
#include <locale>
#include <sstream>
#include <string>

namespace {

void SkipSpace(const char*& p) {
    while (*p && std::isspace(static_cast<unsigned char>(*p))) {
        ++p;
    }
}

bool ParseCoordNumber(const char*& p, double& out) {
    SkipSpace(p);

    const char* start = p;
    if (*p == '+' || *p == '-') {
        ++p;
    }

    bool sawDigit = false;
    while (std::isdigit(static_cast<unsigned char>(*p))) {
        sawDigit = true;
        ++p;
    }
    if (*p == '.') {
        ++p;
        while (std::isdigit(static_cast<unsigned char>(*p))) {
            sawDigit = true;
            ++p;
        }
    }
    if (!sawDigit) {
        return false;
    }

    std::istringstream in(std::string(start, p));
    in.imbue(std::locale::classic());
    if (!(in >> out) || !std::isfinite(out)) {
        return false;
    }

    SkipSpace(p);
    // Optional degree symbol (UTF-8 ° or ISO-8859-1).
    if (static_cast<unsigned char>(*p) == 0xC2 && static_cast<unsigned char>(p[1]) == 0xB0) {
        p += 2;
        SkipSpace(p);
    } else if (static_cast<unsigned char>(*p) == 0xB0) {
        ++p;
        SkipSpace(p);
    }

    return true;
}

bool ParseTwoCoords(const char* p, double& latitude, double& longitude) {
    if (!p) {
        return false;
    }

    double lat = 0.0;
    double lon = 0.0;
    if (!ParseCoordNumber(p, lat)) {
        return false;
    }

    SkipSpace(p);
    if (*p == ',' || *p == ';' || *p == '/') {
        ++p;
    }

    if (!ParseCoordNumber(p, lon)) {
        return false;
    }

    if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
        return false;
    }

    latitude = lat;
    longitude = lon;
    return true;
}

const char* FindQueryValue(const std::string& text, const char* key) {
    const size_t pos = text.find(key);
    if (pos == std::string::npos) {
        return nullptr;
    }
    return text.c_str() + pos + std::char_traits<char>::length(key);
}

}  // namespace

bool ParseLatLonPair(const std::string& text, double& latitude, double& longitude) {
    const char* at = std::strchr(text.c_str(), '@');
    if (at && ParseTwoCoords(at + 1, latitude, longitude)) {
        return true;
    }

    if (const char* query = FindQueryValue(text, "q=")) {
        if (ParseTwoCoords(query, latitude, longitude)) {
            return true;
        }
    }
    if (const char* query = FindQueryValue(text, "query=")) {
        if (ParseTwoCoords(query, latitude, longitude)) {
            return true;
        }
    }

    return ParseTwoCoords(text.c_str(), latitude, longitude);
}
