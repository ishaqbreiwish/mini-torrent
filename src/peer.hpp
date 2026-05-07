#pragma once
#include "torrent.hpp"
#include <string>
#include <vector>
#include <cstdint>

// BitTorrent peer wire protocol message IDs
enum class MsgId : uint8_t {
    Choke         = 0,
    Unchoke       = 1,
    Interested    = 2,
    NotInterested = 3,
    Have          = 4,
    Bitfield      = 5,
    Request       = 6,
    Piece         = 7,
};

// A TCP connection to one BitTorrent peer.
// The socket is managed with RAII — destructor closes it automatically.
class PeerConnection {
public:
    explicit PeerConnection(const PeerAddress& addr);
    ~PeerConnection();

    // Non-copyable: socket ownership would be ambiguous
    PeerConnection(const PeerConnection&)            = delete;
    PeerConnection& operator=(const PeerConnection&) = delete;

    // Step 1: open TCP socket and connect. Returns false on failure.
    bool connect();

    // Step 2: exchange the 68-byte BitTorrent handshake.
    // info_hash and peer_id are raw 20-byte strings.
    // Returns false if connection fails or the peer returns a wrong info_hash.
    bool handshake(const std::string& info_hash, const std::string& peer_id);

    // Step 3: send Interested, wait for Unchoke.
    // Returns false if the peer chokes us or the connection drops.
    bool express_interest();

    // Step 4: download all blocks of piece piece_idx and return assembled bytes.
    // piece_len is the exact size of this piece (last piece may be < piece_length).
    // Returns empty vector on failure.
    std::vector<uint8_t> download_piece(int piece_idx, int64_t piece_len);

    bool is_connected() const { return sockfd_ >= 0; }

private:
    PeerAddress addr_;
    int         sockfd_ = -1;

    // ── PROVIDED: low-level message framing (see peer.cpp) ──────────────────
    //
    // BitTorrent message format:
    //   [4 bytes big-endian]  length  (= 1 + payload.size())
    //   [1 byte]              message id
    //   [length-1 bytes]      payload
    //
    // send_msg and recv_msg handle this framing for you.

    bool send_msg(MsgId id, const std::vector<uint8_t>& payload = {});
    bool recv_msg(MsgId& id, std::vector<uint8_t>& payload);

    // Consume one message that we don't need (e.g. Bitfield)
    void drain_one_msg();
};
