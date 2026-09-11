#pragma once
#include <map>
#include <string>

struct HttpResponse {
    long statusCode = 0;
    std::string body;
    std::wstring error;
};

HttpResponse HttpRequest(
    const std::wstring& method,
    const std::wstring& url,
    const std::map<std::string, std::string>& headers,
    const std::string& body = std::string(),
    unsigned long timeoutMs = 0);
