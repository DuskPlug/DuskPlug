#pragma once
#include <optional>
#include <string>

std::optional<std::string> JsonGetString(const std::string& json, const std::string& key);
std::optional<double> JsonGetNumber(const std::string& json, const std::string& key);
bool JsonGetBool(const std::string& json, const std::string& key, bool& out);
bool JsonGetSuccess(const std::string& json);
std::optional<std::string> JsonGetNestedString(const std::string& json, const std::string& parentKey, const std::string& childKey);
std::optional<std::string> JsonGetObjectSlice(const std::string& json, const std::string& key);
std::optional<std::string> JsonGetAssetField(
    const std::string& json,
    const std::string& assetKey,
    const std::string& fieldKey);
bool JsonStatusValueForCode(const std::string& json, const std::string& code, bool& out);
