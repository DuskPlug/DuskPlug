#include "http_win.h"

#include <windows.h>
#include <winhttp.h>

#include <sstream>

#pragma comment(lib, "winhttp.lib")

static std::wstring Utf8ToWide(const std::string& text) {
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

static bool ParseUrl(const std::wstring& url, URL_COMPONENTS& components, std::wstring& host, std::wstring& path) {
    ZeroMemory(&components, sizeof(components));
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &components)) {
        return false;
    }

    host.assign(components.lpszHostName, components.dwHostNameLength);
    path.assign(components.lpszUrlPath, components.dwUrlPathLength);
    if (components.dwExtraInfoLength > 0) {
        path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    }
    return true;
}

HttpResponse HttpRequest(
    const std::wstring& method,
    const std::wstring& url,
    const std::map<std::string, std::string>& headers,
    const std::string& body,
    unsigned long timeoutMs) {
    HttpResponse response;

    URL_COMPONENTS components{};
    std::wstring host;
    std::wstring path;
    if (!ParseUrl(url, components, host, path)) {
        response.error = L"Invalid URL";
        return response;
    }

    const bool secure = components.nScheme == INTERNET_SCHEME_HTTPS;
    const INTERNET_PORT port = components.nPort;

    HINTERNET session = WinHttpOpen(L"DuskPlug/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        response.error = L"WinHttpOpen failed";
        return response;
    }

    if (timeoutMs > 0) {
        WinHttpSetTimeouts(
            session,
            timeoutMs,
            timeoutMs,
            timeoutMs,
            timeoutMs);
    }

    HINTERNET connect = WinHttpConnect(session, host.c_str(), port, 0);
    if (!connect) {
        response.error = L"WinHttpConnect failed";
        WinHttpCloseHandle(session);
        return response;
    }

    HINTERNET request = WinHttpOpenRequest(
        connect,
        method.c_str(),
        path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        secure ? WINHTTP_FLAG_SECURE : 0);
    if (!request) {
        response.error = L"WinHttpOpenRequest failed";
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return response;
    }

    std::wstring headerBlock;
    for (const auto& [key, value] : headers) {
        headerBlock += Utf8ToWide(key) + L": " + Utf8ToWide(value) + L"\r\n";
    }
    headerBlock += L"Content-Type: application/json\r\n";

    const BOOL sendOk = WinHttpSendRequest(
        request,
        headerBlock.c_str(),
        static_cast<DWORD>(-1),
        body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data()),
        static_cast<DWORD>(body.size()),
        static_cast<DWORD>(body.size()),
        0);

    if (!sendOk || !WinHttpReceiveResponse(request, nullptr)) {
        response.error = L"HTTP request failed";
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return response;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(
        request,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &statusSize,
        WINHTTP_NO_HEADER_INDEX);
    response.statusCode = static_cast<long>(statusCode);

    std::string responseBody;
    DWORD available = 0;
    while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
        std::string chunk(available, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(request, chunk.data(), available, &read)) {
            break;
        }
        chunk.resize(read);
        responseBody += chunk;
    }

    response.body = std::move(responseBody);
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return response;
}
