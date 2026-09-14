#include "json_util.h"

#include <cctype>
#include <vector>

static void SkipJsonWhitespace(const std::string& json, size_t& pos) {
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
}

static std::optional<std::string> ReadJsonString(const std::string& json, size_t& pos) {
    if (pos >= json.size() || json[pos] != '"') {
        return std::nullopt;
    }

    ++pos;
    std::string value;
    while (pos < json.size()) {
        const char c = json[pos++];
        if (c == '"') {
            return value;
        }
        if (c == '\\' && pos < json.size()) {
            value.push_back(json[pos++]);
            continue;
        }
        value.push_back(c);
    }

    return std::nullopt;
}

static std::optional<size_t> FindJsonKey(const std::string& json, const std::string& key) {
    const std::string quoted = "\"" + key + "\"";
    size_t pos = 0;
    while ((pos = json.find(quoted, pos)) != std::string::npos) {
        size_t after = pos + quoted.size();
        SkipJsonWhitespace(json, after);
        if (after < json.size() && json[after] == ':') {
            return pos;
        }
        ++pos;
    }
    return std::nullopt;
}

std::optional<std::string> JsonGetString(const std::string& json, const std::string& key) {
    const auto keyPos = FindJsonKey(json, key);
    if (!keyPos) {
        return std::nullopt;
    }

    size_t pos = *keyPos + key.size() + 2;
    SkipJsonWhitespace(json, pos);
    if (pos >= json.size() || json[pos] != ':') {
        return std::nullopt;
    }
    ++pos;
    SkipJsonWhitespace(json, pos);
    return ReadJsonString(json, pos);
}

std::optional<double> JsonGetNumber(const std::string& json, const std::string& key) {
    const auto keyPos = FindJsonKey(json, key);
    if (!keyPos) {
        return std::nullopt;
    }

    size_t pos = *keyPos + key.size() + 2;
    SkipJsonWhitespace(json, pos);
    if (pos >= json.size() || json[pos] != ':') {
        return std::nullopt;
    }
    ++pos;
    SkipJsonWhitespace(json, pos);

    size_t end = pos;
    while (end < json.size()) {
        const char c = json[end];
        if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.') {
            ++end;
            continue;
        }
        break;
    }

    if (end == pos) {
        return std::nullopt;
    }

    try {
        return std::stod(json.substr(pos, end - pos));
    } catch (...) {
        return std::nullopt;
    }
}

bool JsonGetBool(const std::string& json, const std::string& key, bool& out) {
    const auto keyPos = FindJsonKey(json, key);
    if (!keyPos) {
        return false;
    }

    size_t pos = *keyPos + key.size() + 2;
    SkipJsonWhitespace(json, pos);
    if (pos >= json.size() || json[pos] != ':') {
        return false;
    }
    ++pos;
    SkipJsonWhitespace(json, pos);

    if (json.compare(pos, 4, "true") == 0) {
        out = true;
        return true;
    }
    if (json.compare(pos, 5, "false") == 0) {
        out = false;
        return true;
    }
    return false;
}

bool JsonGetSuccess(const std::string& json) {
    bool success = false;
    if (JsonGetBool(json, "success", success)) {
        return success;
    }
    return false;
}

std::optional<std::string> JsonGetObjectSlice(const std::string& json, const std::string& key) {
    const auto keyPos = FindJsonKey(json, key);
    if (!keyPos) {
        return std::nullopt;
    }

    size_t pos = *keyPos + key.size() + 2;
    SkipJsonWhitespace(json, pos);
    if (pos >= json.size() || json[pos] != ':') {
        return std::nullopt;
    }
    ++pos;
    SkipJsonWhitespace(json, pos);
    if (pos >= json.size() || json[pos] != '{') {
        return std::nullopt;
    }

    size_t depth = 0;
    const size_t start = pos;
    for (; pos < json.size(); ++pos) {
        if (json[pos] == '{') {
            ++depth;
        } else if (json[pos] == '}') {
            --depth;
            if (depth == 0) {
                break;
            }
        }
    }

    if (pos >= json.size()) {
        return std::nullopt;
    }

    return json.substr(start, pos - start + 1);
}

std::optional<std::string> JsonGetAssetField(
    const std::string& json,
    const std::string& assetKey,
    const std::string& fieldKey) {
    const auto assets = JsonGetObjectSlice(json, "assets");
    if (!assets) {
        return std::nullopt;
    }
    return JsonGetNestedString(*assets, assetKey, fieldKey);
}

std::optional<std::string> JsonGetNestedString(const std::string& json, const std::string& parentKey, const std::string& childKey) {
    const auto keyPos = FindJsonKey(json, parentKey);
    if (!keyPos) {
        return std::nullopt;
    }

    size_t pos = *keyPos + parentKey.size() + 2;
    SkipJsonWhitespace(json, pos);
    if (pos >= json.size() || json[pos] != ':') {
        return std::nullopt;
    }
    ++pos;
    SkipJsonWhitespace(json, pos);
    if (pos >= json.size() || json[pos] != '{') {
        return std::nullopt;
    }

    size_t depth = 0;
    const size_t start = pos;
    for (; pos < json.size(); ++pos) {
        if (json[pos] == '{') {
            ++depth;
        } else if (json[pos] == '}') {
            --depth;
            if (depth == 0) {
                break;
            }
        }
    }

    if (pos >= json.size()) {
        return std::nullopt;
    }

    const std::string slice = json.substr(start, pos - start + 1);
    return JsonGetString(slice, childKey);
}

std::vector<std::string> JsonGetArrayObjectSlices(const std::string& json, const std::string& key) {
    std::vector<std::string> items;
    const auto keyPos = FindJsonKey(json, key);
    if (!keyPos) {
        return items;
    }

    size_t pos = *keyPos + key.size() + 2;
    SkipJsonWhitespace(json, pos);
    if (pos >= json.size() || json[pos] != ':') {
        return items;
    }
    ++pos;
    SkipJsonWhitespace(json, pos);
    if (pos >= json.size() || json[pos] != '[') {
        return items;
    }
    ++pos;
    SkipJsonWhitespace(json, pos);

    while (pos < json.size() && json[pos] != ']') {
        SkipJsonWhitespace(json, pos);
        if (pos >= json.size() || json[pos] == ']') {
            break;
        }
        if (json[pos] != '{') {
            break;
        }

        size_t depth = 0;
        const size_t start = pos;
        for (; pos < json.size(); ++pos) {
            if (json[pos] == '{') {
                ++depth;
            } else if (json[pos] == '}') {
                --depth;
                if (depth == 0) {
                    break;
                }
            }
        }
        if (pos >= json.size()) {
            break;
        }

        items.push_back(json.substr(start, pos - start + 1));
        ++pos;
        SkipJsonWhitespace(json, pos);
        if (pos < json.size() && json[pos] == ',') {
            ++pos;
        }
    }

    return items;
}

bool JsonStatusIntForCode(const std::string& json, const std::string& code, int& out) {
    const std::string quoted = "\"code\"";
    size_t pos = 0;
    while ((pos = json.find(quoted, pos)) != std::string::npos) {
        size_t after = pos + quoted.size();
        SkipJsonWhitespace(json, after);
        if (after < json.size() && json[after] == ':') {
            ++after;
            SkipJsonWhitespace(json, after);
            auto value = ReadJsonString(json, after);
            if (value && *value == code) {
                const std::string tail = json.substr(after, 120);
                if (const auto number = JsonGetNumber(tail, "value")) {
                    out = static_cast<int>(*number);
                    return true;
                }
            }
        }
        ++pos;
    }
    return false;
}

bool JsonStatusValueForCode(const std::string& json, const std::string& code, bool& out) {
    const std::string quoted = "\"code\"";
    size_t pos = 0;
    while ((pos = json.find(quoted, pos)) != std::string::npos) {
        size_t after = pos + quoted.size();
        SkipJsonWhitespace(json, after);
        if (after < json.size() && json[after] == ':') {
            ++after;
            SkipJsonWhitespace(json, after);
            auto value = ReadJsonString(json, after);
            if (value && *value == code) {
                const std::string tail = json.substr(after, 120);
                return JsonGetBool(tail, "value", out);
            }
        }
        ++pos;
    }
    return false;
}
