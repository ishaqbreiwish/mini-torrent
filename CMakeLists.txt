cmake_minimum_required(VERSION 3.16)
project(mini-torrent CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# macOS Homebrew: hint toward its OpenSSL
if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH
        "/opt/homebrew/opt/openssl@3"
        "/opt/homebrew/opt/openssl"
        "/usr/local/opt/openssl"
    )
endif()

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

set(SOURCES
    src/main.cpp
    src/utils.cpp
    src/bencode.cpp
    src/torrent.cpp
    src/peer.cpp
    src/download.cpp
)

add_executable(mini-torrent ${SOURCES})
target_include_directories(mini-torrent PRIVATE src)
target_link_libraries(mini-torrent OpenSSL::SSL OpenSSL::Crypto Threads::Threads)

# Bencode unit tests — no networking needed, fast to iterate on
add_executable(test_bencode
    tests/test_bencode.cpp
    src/bencode.cpp
)
target_include_directories(test_bencode PRIVATE src)
target_link_libraries(test_bencode OpenSSL::Crypto)
