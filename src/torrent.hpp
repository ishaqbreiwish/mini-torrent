#pragma once
#include <string>
#include <vector>
#include <cstdint>

// Everything extracted from a .torrent file
struct TorrentFile {
    std::string announce;       // tracker URL  e.g. "http://tracker.example.com/announce"
    std::string name;           // suggested output filename
    int64_t     piece_length;   // bytes per piece (uniform, except possibly the last)
    int64_t     total_length;   // total file size in bytes
    std::vector<std::string> piece_hashes; // 20 raw bytes per piece (SHA-1)

    // SHA-1 of the bencoded "info" dict — used in tracker requests + peer handshakes
    std::string info_hash; // raw 20 bytes (NOT hex)

    int     num_pieces() const { return static_cast<int>(piece_hashes.size()); }

    // Piece i may be shorter than piece_length (the last piece usually is)
    int64_t piece_len(int i) const {
        if (i < num_pieces() - 1) return piece_length;
        return total_length - piece_length * (num_pieces() - 1);
    }
};

// IP + port of one BitTorrent peer
struct PeerAddress {
    std::string ip;
    uint16_t    port;
};

// Parse a .torrent file at path. Throws std::runtime_error on malformed input.
TorrentFile parse_torrent(const std::string& path);

// Contact the tracker listed in tf and return the list of peers.
// peer_id must be exactly 20 bytes (see main.cpp for how it's generated).
std::vector<PeerAddress> get_peers(const TorrentFile& tf, const std::string& peer_id);
