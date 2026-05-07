// ============================================================
// PROVIDED — you don't need to touch this file.
// It implements SHA-1, URL encoding, and a raw HTTP GET.
// ============================================================

#include "utils.hpp"
#include <openssl/sha.h>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <tuple>

// POSIX networking
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>

// ── SHA-1 ─────────────────────────────────────────────────────────────────────
std::string sha1_bytes(const std::string& data) {
    unsigned char hash[SHA_DIGEST_LENGTH]; // 20 bytes
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    SHA1(reinterpret_cast<const unsigned char*>(data.data()), data.size(), hash);
#pragma GCC diagnostic pop
    return {reinterpret_cast<char*>(hash), SHA_DIGEST_LENGTH};
}

// ── URL encoding ──────────────────────────────────────────────────────────────
std::string url_encode(const std::string& s) {
    std::ostringstream out;
    out << std::hex << std::uppercase;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out << c;
        } else {
            out << '%' << std::setw(2) << std::setfill('0') << (int)c;
        }
    }
    return out.str();
}

// ── HTTP GET ──────────────────────────────────────────────────────────────────

// Split "http://host:port/path?query" → {host, port, path+query}
static std::tuple<std::string, std::string, std::string>
parse_url(const std::string& url) {
    if (url.substr(0, 7) != "http://")
        throw std::runtime_error("http_get: only http:// is supported, got: " + url);

    size_t start = 7;
    size_t slash  = url.find('/', start);
    std::string authority = (slash == std::string::npos)
                            ? url.substr(start)
                            : url.substr(start, slash - start);
    std::string path = (slash == std::string::npos) ? "/" : url.substr(slash);

    std::string host, port = "80";
    size_t colon = authority.find(':');
    if (colon != std::string::npos) {
        host = authority.substr(0, colon);
        port = authority.substr(colon + 1);
    } else {
        host = authority;
    }
    return {host, port, path};
}

std::string http_get(const std::string& url) {
    auto [host, port, path] = parse_url(url);

    // Resolve hostname → connect TCP
    addrinfo hints{}, *res;
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0)
        throw std::runtime_error("http_get: DNS failed for " + host);

    int fd = -1;
    for (auto* p = res; p; p = p->ai_next) {
        fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        if (::connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
        ::close(fd); fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0)
        throw std::runtime_error("http_get: connect failed to " + host + ":" + port);

    // Send HTTP/1.0 GET (server closes connection when done — easy to read)
    std::string req = "GET " + path + " HTTP/1.0\r\n"
                      "Host: " + host + "\r\n"
                      "Connection: close\r\n\r\n";
    if (::send(fd, req.data(), req.size(), 0) < 0) {
        ::close(fd);
        throw std::runtime_error("http_get: send failed");
    }

    // Read until the server closes the connection
    std::string response;
    char buf[4096];
    ssize_t n;
    while ((n = ::recv(fd, buf, sizeof(buf), 0)) > 0)
        response.append(buf, n);
    ::close(fd);

    // Strip HTTP headers — body starts after the blank line
    size_t body_start = response.find("\r\n\r\n");
    if (body_start == std::string::npos)
        throw std::runtime_error("http_get: malformed HTTP response");
    return response.substr(body_start + 4);
}
