#pragma once

#include <map>
#include <string>

struct HttpResponse {
    long statusCode = 0;
    std::string body;
    std::string error;
};

HttpResponse HttpRequest(
    const std::string& method,
    const std::string& url,
    const std::map<std::string, std::string>& headers,
    const std::string& body = std::string(),
    unsigned long timeoutMs = 0);
