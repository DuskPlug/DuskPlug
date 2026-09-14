#include "http.h"

#include <curl/curl.h>

#include <sstream>

namespace {

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    const size_t total = size * nmemb;
    auto* out = static_cast<std::string*>(userp);
    out->append(static_cast<char*>(contents), total);
    return total;
}

}  // namespace

HttpResponse HttpRequest(
    const std::string& method,
    const std::string& url,
    const std::map<std::string, std::string>& headers,
    const std::string& body,
    unsigned long timeoutMs) {
    HttpResponse response;

    CURL* curl = curl_easy_init();
    if (!curl) {
        response.error = "curl_easy_init failed";
        return response;
    }

    struct curl_slist* headerList = nullptr;
    headerList = curl_slist_append(headerList, "Content-Type: application/json");
    for (const auto& [key, value] : headers) {
        headerList = curl_slist_append(headerList, (key + ": " + value).c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "DuskPlug/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    if (timeoutMs > 0) {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(timeoutMs));
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(timeoutMs));
    }

    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    } else {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    }

    const CURLcode code = curl_easy_perform(curl);
    if (code != CURLE_OK) {
        response.error = curl_easy_strerror(code);
    } else {
        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        response.statusCode = status;
    }

    curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);
    return response;
}
