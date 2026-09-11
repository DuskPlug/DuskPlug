#pragma once
#include <string>

std::string Sha256HexLower(const std::string& text);
std::string HmacSha256HexUpper(const std::string& key, const std::string& message);
