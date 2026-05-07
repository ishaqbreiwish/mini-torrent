#include "torrent.hpp"
#include "bencode.hpp"
#include "utils.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <arpa/inet.h>  // inet_ntop, ntohs

// ─────────────────────────────────────────────────────────────────────────────
// TODO (Phase 2): implement parse_torrent
//
// A .torrent file is a bencoded dict with at minimum these keys:
//
//   "announce"  →  string  (tracker URL)
//   "info"      →  dict    (the important nested dict)
//     "name"          →  string  (filename)
//     "piece length"  →  integer (bytes per piece, e.g. 262144 = 256 KiB)
//     "length"        →  integer (total file size — only present for single-file torrents)
//     "pieces"        →  string  (raw bytes: 20 bytes × num_pieces, each is a SHA-1)
//
// STEPS:
//
// 1. Read the file into a std::string:
//      std::ifstream f(path, std::ios::binary);
//      if (!f) throw std::runtime_error("cannot open " + path);
//      std::string raw((std::istreambuf_iterator<char>(f)), {});
//
// 2. Decode:
//      BencodeValue root = bencode::decode(raw);
//      auto& d = root.as_dict();
//
// 3. Extract fields:
//      tf.announce     = d.at("announce").as_str();
//      auto& info      = d.at("info").as_dict();
//      tf.name         = info.at("name").as_str();
//      tf.piece_length = info.at("piece length").as_int();
//      tf.total_length = info.at("length").as_int();
//
// 4. Split the raw "pieces" string into 20-byte chunks:
//      const std::string& pieces_raw = info.at("pieces").as_str();
//      for (size_t i = 0; i + 20 <= pieces_raw.size(); i += 20)
//          tf.piece_hashes.push_back(pieces_raw.substr(i, 20));
//
// 5. Compute info_hash (re-encode just the info dict, then SHA-1 it):
//      BencodeValue info_val;
//      info_val.data = info;   // copy the BencodeDict into a BencodeValue
//      tf.info_hash = sha1_bytes(bencode::encode(info_val));
//      // This is the standard BitTorrent info hash — 20 raw bytes.
//
// ─────────────────────────────────────────────────────────────────────────────
TorrentFile parse_torrent(const std::string& path) {
    // TODO: implement — delete the lines below and replace with your code
    (void)path;
    throw std::runtime_error("parse_torrent not implemented");
}


// ── PROVIDED: compact peer list parser ───────────────────────────────────────
// Tracker compact format: each peer = 4-byte IPv4 (BE) + 2-byte port (BE)
static std::vector<PeerAddress> parse_peers_compact(const std::string& compact) {
    if (compact.size() % 6 != 0)
        throw std::runtime_error("compact peer list has bad length: " + std::to_string(compact.size()));

    std::vector<PeerAddress> peers;
    for (size_t i = 0; i + 6 <= compact.size(); i += 6) {
        uint32_t ip_raw;
        uint16_t port_raw;
        std::memcpy(&ip_raw,   compact.data() + i,     4);
        std::memcpy(&port_raw, compact.data() + i + 4, 2);

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip_raw, ip_str, sizeof(ip_str));
        peers.push_back({std::string(ip_str), ntohs(port_raw)});
    }
    return peers;
}

// ─────────────────────────────────────────────────────────────────────────────
// TODO (Phase 3): implement get_peers
//
// Build the tracker URL by appending query parameters to tf.announce:
//
//   std::string url = tf.announce + "?"
//       + "info_hash="  + url_encode(tf.info_hash)
//       + "&peer_id="   + url_encode(peer_id)
//       + "&port=6881"
//       + "&uploaded=0"
//       + "&downloaded=0"
//       + "&left="      + std::to_string(tf.total_length)
//       + "&compact=1";
//
// Then:
//   std::string body  = http_get(url);
//   BencodeValue resp = bencode::decode(body);
//   auto& rd          = resp.as_dict();
//
//   // Check for tracker error
//   if (rd.count("failure reason"))
//       throw std::runtime_error("Tracker error: " + rd.at("failure reason").as_str());
//
//   std::string compact = rd.at("peers").as_str();
//   return parse_peers_compact(compact);
//
// ─────────────────────────────────────────────────────────────────────────────
std::vector<PeerAddress> get_peers(const TorrentFile& tf, const std::string& peer_id) {
    // TODO: implement — delete the lines below and replace with your code
    (void)tf; (void)peer_id;
    throw std::runtime_error("get_peers not implemented");
}
