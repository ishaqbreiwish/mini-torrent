#pragma once
#include <string>

// SHA-1 of data. Returns raw 20-byte string (NOT hex-encoded).
// Used to hash the bencoded info dict → info_hash.
std::string sha1_bytes(const std::string& data);

// Percent-encode a string for a URL query parameter.
// Encodes everything except unreserved chars: A-Z a-z 0-9 - _ . ~
std::string url_encode(const std::string& s);

// Issue an HTTP GET to url and return the response body.
// Only supports http:// (no TLS). Throws std::runtime_error on failure.
std::string http_get(const std::string& url);
