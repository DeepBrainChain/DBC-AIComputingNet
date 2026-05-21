// docker_http_client.h
//
// Thin libcurl-based HTTP client that speaks to the Docker Engine API over its
// Unix socket (/var/run/docker.sock). Used by ContainerClient.

#ifndef DBC_DOCKER_HTTP_CLIENT_H
#define DBC_DOCKER_HTTP_CLIENT_H

#include <cstdint>
#include <string>
#include <vector>

namespace dbc::container {

struct HttpResponse {
    long status_code = 0;
    std::string body;
};

class DockerHttpClient {
public:
    /// `socket_path` defaults to "/var/run/docker.sock".
    explicit DockerHttpClient(std::string socket_path = "/var/run/docker.sock");
    ~DockerHttpClient();

    /// HTTP GET. `path` like "/containers/abc123/json".
    int32_t Get(const std::string& path, HttpResponse& out);

    /// HTTP POST with JSON body.
    int32_t PostJson(const std::string& path, const std::string& json, HttpResponse& out);

    /// HTTP DELETE.
    int32_t Delete(const std::string& path, HttpResponse& out);

    /// HTTP PUT with raw binary body (used for /containers/{id}/archive tar streams).
    int32_t PutRaw(
        const std::string& path,
        const std::vector<uint8_t>& body,
        const std::string& content_type,
        HttpResponse& out);

private:
    std::string socket_path_;
};

}  // namespace dbc::container

#endif  // DBC_DOCKER_HTTP_CLIENT_H
