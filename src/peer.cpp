#include "peer.hpp"
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <stdexcept>

// BitTorrent pieces are requested in blocks of at most 16 KiB
static constexpr int BLOCK_SIZE = 16384;

// ── Constructor / Destructor ──────────────────────────────────────────────────
PeerConnection::PeerConnection(const PeerAddress& addr) : addr_(addr) {}

PeerConnection::~PeerConnection() {
    if (sockfd_ >= 0) {
        ::close(sockfd_);
        sockfd_ = -1;
    }
}

// ── PROVIDED: socket I/O helpers ──────────────────────────────────────────────

// Read exactly n bytes into buf. Returns false on EOF or error.
static bool recv_exact(int fd, void* buf, size_t n) {
    auto* p = reinterpret_cast<char*>(buf);
    while (n > 0) {
        ssize_t r = ::recv(fd, p, n, 0);
        if (r <= 0) return false;
        p += r;
        n -= r;
    }
    return true;
}

// Send exactly n bytes from buf. Returns false on error.
static bool send_exact(int fd, const void* buf, size_t n) {
    auto* p = reinterpret_cast<const char*>(buf);
    while (n > 0) {
        ssize_t s = ::send(fd, p, n, 0);
        if (s <= 0) return false;
        p += s;
        n -= s;
    }
    return true;
}

// ── PROVIDED: message framing ─────────────────────────────────────────────────

bool PeerConnection::send_msg(MsgId id, const std::vector<uint8_t>& payload) {
    uint32_t len_net = htonl(1 + static_cast<uint32_t>(payload.size()));
    if (!send_exact(sockfd_, &len_net, 4)) return false;
    uint8_t id_byte = static_cast<uint8_t>(id);
    if (!send_exact(sockfd_, &id_byte, 1)) return false;
    if (!payload.empty())
        if (!send_exact(sockfd_, payload.data(), payload.size())) return false;
    return true;
}

bool PeerConnection::recv_msg(MsgId& id, std::vector<uint8_t>& payload) {
    uint32_t len_net;
    if (!recv_exact(sockfd_, &len_net, 4)) return false;
    uint32_t len = ntohl(len_net);
    if (len == 0) {
        // keep-alive message (length == 0, no id) — just recurse to get the next real message
        return recv_msg(id, payload);
    }
    uint8_t id_byte;
    if (!recv_exact(sockfd_, &id_byte, 1)) return false;
    id = static_cast<MsgId>(id_byte);
    payload.resize(len - 1);
    if (len > 1 && !recv_exact(sockfd_, payload.data(), len - 1)) return false;
    return true;
}

void PeerConnection::drain_one_msg() {
    MsgId id;
    std::vector<uint8_t> payload;
    recv_msg(id, payload); // consume and discard
}

// ─────────────────────────────────────────────────────────────────────────────
// TODO (Phase 4): implement connect
//
// Open a TCP socket and connect to addr_.ip : addr_.port.
//
// STEPS:
//   addrinfo hints{}, *res;
//   hints.ai_family   = AF_UNSPEC;    // accept IPv4 or IPv6
//   hints.ai_socktype = SOCK_STREAM;  // TCP
//   std::string port_str = std::to_string(addr_.port);
//   if (getaddrinfo(addr_.ip.c_str(), port_str.c_str(), &hints, &res) != 0)
//       return false;
//
//   int fd = -1;
//   for (auto* p = res; p; p = p->ai_next) {
//       fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
//       if (fd < 0) continue;
//       if (::connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
//       ::close(fd); fd = -1;
//   }
//   freeaddrinfo(res);
//
//   if (fd < 0) return false;
//   sockfd_ = fd;
//
//   // Optional but strongly recommended: set a 5-second timeout so slow/dead
//   // peers don't block a thread indefinitely.
//   struct timeval tv{5, 0};
//   setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
//   setsockopt(sockfd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
//
//   return true;
// ─────────────────────────────────────────────────────────────────────────────
bool PeerConnection::connect() {
    // TODO
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// TODO (Phase 4): implement handshake
//
// Send exactly 68 bytes, then receive exactly 68 bytes, then validate.
//
// HANDSHAKE LAYOUT (build a single buffer and send it in one shot):
//   [1 byte]   0x13                      = 19, length of "BitTorrent protocol"
//   [19 bytes] "BitTorrent protocol"
//   [8 bytes]  0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00  (reserved, all zeros)
//   [20 bytes] info_hash                 (raw bytes — NOT hex)
//   [20 bytes] peer_id                   (raw bytes)
//   Total = 68 bytes
//
// BUILD TIP:
//   std::string hs;
//   hs.reserve(68);
//   hs += '\x13';
//   hs += "BitTorrent protocol";
//   hs += std::string(8, '\0');
//   hs += info_hash;
//   hs += peer_id;
//   send_exact(sockfd_, hs.data(), hs.size())
//
// RECEIVE:
//   char resp[68];
//   recv_exact(sockfd_, resp, 68)
//
// VALIDATE:
//   The peer's info_hash is at bytes [28..48).
//   Compare: std::string(resp + 28, 20) == info_hash
//   If it doesn't match, return false (connected to the wrong torrent).
// ─────────────────────────────────────────────────────────────────────────────
bool PeerConnection::handshake(const std::string& info_hash, const std::string& peer_id) {
    // TODO
    (void)info_hash; (void)peer_id;
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// TODO (Phase 4): implement express_interest
//
// After a successful handshake, peers typically send a Bitfield message
// telling us which pieces they have. We consume it (drain_one_msg), then
// express that we want data.
//
// STEPS:
//   1. drain_one_msg()  — eat the Bitfield (or whatever the first message is)
//   2. send_msg(MsgId::Interested)
//   3. Loop:
//        MsgId id; std::vector<uint8_t> payload;
//        if (!recv_msg(id, payload)) return false;
//        if (id == MsgId::Unchoke) return true;
//        if (id == MsgId::Choke)   return false;
//        // ignore anything else (Have, etc.) and keep waiting
// ─────────────────────────────────────────────────────────────────────────────
bool PeerConnection::express_interest() {
    // TODO
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// TODO (Phase 4): implement download_piece
//
// A piece is divided into blocks of BLOCK_SIZE (16384) bytes.
// Request each block, receive it, copy into the result buffer.
//
// PRE-ALLOCATE result:
//   std::vector<uint8_t> result(piece_len);
//
// FOR EACH BLOCK (offset = 0, BLOCK_SIZE, 2*BLOCK_SIZE, ...):
//   int64_t block_len = std::min((int64_t)BLOCK_SIZE, piece_len - offset);
//
//   a) BUILD AND SEND REQUEST payload (13 bytes total):
//        std::vector<uint8_t> req(12);
//        uint32_t idx_n   = htonl(piece_idx);
//        uint32_t begin_n = htonl(offset);
//        uint32_t len_n   = htonl(block_len);
//        memcpy(req.data() + 0, &idx_n,   4);
//        memcpy(req.data() + 4, &begin_n, 4);
//        memcpy(req.data() + 8, &len_n,   4);
//        if (!send_msg(MsgId::Request, req)) return {};
//
//   b) LOOP UNTIL WE GET THE PIECE RESPONSE:
//        MsgId id; std::vector<uint8_t> payload;
//        while (true) {
//            if (!recv_msg(id, payload)) return {};
//            if (id == MsgId::Choke)     return {};   // peer changed its mind
//            if (id == MsgId::Piece)     break;       // got what we want
//            // else: ignore (Have, Unchoke, etc.)
//        }
//
//   c) PARSE PIECE PAYLOAD:
//        Piece payload layout:  [4B index][4B begin][data...]
//        uint32_t recv_idx, recv_begin;
//        memcpy(&recv_idx,   payload.data() + 0, 4);  recv_idx   = ntohl(recv_idx);
//        memcpy(&recv_begin, payload.data() + 4, 4);  recv_begin = ntohl(recv_begin);
//        size_t data_len = payload.size() - 8;
//        // Sanity check:
//        if ((int)recv_idx != piece_idx || (int)recv_begin != offset) return {};
//        // Copy into result:
//        memcpy(result.data() + recv_begin, payload.data() + 8, data_len);
//
// After all blocks: return result;
// ─────────────────────────────────────────────────────────────────────────────
std::vector<uint8_t> PeerConnection::download_piece(int piece_idx, int64_t piece_len) {
    // TODO
    (void)piece_idx; (void)piece_len;
    return {};
}
