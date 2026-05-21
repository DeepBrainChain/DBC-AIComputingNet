// docker_http_client.cpp — see docker_http_client.h
//
// libcurl with CURLOPT_UNIX_SOCKET_PATH. Hostname in the URL is irrelevant when
// the unix-socket option is set, but we still need *something* so we use the
// Docker convention "http://localhost/v1.41/...".

#include "docker_http_client.h"

#include "log/log.h"
#include "util/utils.h"

#include <curl/curl.h>

#include <utility>

namespace dbc::container {

namespace {

constexpr const char* kApiBase = "http://localhost/v1.41";

size_t WriteCb(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* out = static_cast<std::string*>(userp);
    out->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

struct CurlEasyGuard {
    CURL* h;
    explicit CurlEasyGuard(CURL* p) : h(p) {}
    ~CurlEasyGuard() { if (h) curl_easy_cleanup(h); }
    CurlEasyGuard(const CurlEasyGuard&) = delete;
    CurlEasyGuard& operator=(const CurlEasyGuard&) = delete;
};

struct CurlSlistGuard {
    curl_slist* h = nullptr;
    ~CurlSlistGuard() { if (h) curl_slist_free_all(h); }
};

/// Performs the request after the per-method options have been set. Manages
/// header list lifetime and result extraction.
int32_t Perform(
    CURL* curl,
    const std::string& socket_path,
    const std::string& path,
    const std::string& content_type,
    HttpResponse& out) {

    const std::string url = std::string(kApiBase) + path;
    out.body.clear();

    curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, socket_path.c_str());
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    CurlSlistGuard headers;
    if (!content_type.empty()) {
        std::string ct = "Content-Type: " + content_type;
        headers.h = curl_slist_append(headers.h, ct.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.h);
    }

    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        LOG_ERROR << "docker engine curl failed: " << curl_easy_strerror(rc)
                  << " (path=" << path << ")";
        return E_DEFAULT;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &out.status_code);
    return ERR_SUCCESS;
}

}  // namespace

DockerHttpClient::DockerHttpClient(std::string socket_path)
    : socket_path_(std::move(socket_path)) {
    // libcurl init is idempotent and global; called once at first use is fine.
    static const bool kInit = [] {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        return true;
    }();
    (void)kInit;
}

DockerHttpClient::~DockerHttpClient() = default;

int32_t DockerHttpClient::Get(const std::string& path, HttpResponse& out) {
    CURL* curl = curl_easy_init();
    if (!curl) return E_DEFAULT;
    CurlEasyGuard g(curl);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    return Perform(curl, socket_path_, path, "", out);
}

int32_t DockerHttpClient::PostJson(
    const std::string& path,
    const std::string& json,
    HttpResponse& out) {
    CURL* curl = curl_easy_init();
    if (!curl) return E_DEFAULT;
    CurlEasyGuard g(curl);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(json.size()));
    return Perform(curl, socket_path_, path, "application/json", out);
}

int32_t DockerHttpClient::Delete(const std::string& path, HttpResponse& out) {
    CURL* curl = curl_easy_init();
    if (!curl) return E_DEFAULT;
    CurlEasyGuard g(curl);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    return Perform(curl, socket_path_, path, "", out);
}

int32_t DockerHttpClient::PutRaw(
    const std::string& path,
    const std::vector<uint8_t>& body,
    const std::string& content_type,
    HttpResponse& out) {
    CURL* curl = curl_easy_init();
    if (!curl) return E_DEFAULT;
    CurlEasyGuard g(curl);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    return Perform(curl, socket_path_, path, content_type, out);
}

}  // namespace dbc::container
