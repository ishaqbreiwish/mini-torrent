#include "download.hpp"
#include "peer.hpp"
#include "utils.hpp"
#include <fstream>
#include <iostream>
#include <cstring>
#include <algorithm>

// ── PROVIDED: piece verification ─────────────────────────────────────────────
bool DownloadManager::verify_piece(const std::vector<uint8_t>& data,
                                   const std::string& expected_hash) {
    std::string actual = sha1_bytes(std::string(data.begin(), data.end()));
    return actual == expected_hash;
}

// ── PROVIDED: file assembly ───────────────────────────────────────────────────
// Writes all pieces in order to output_path, trimming to total_length bytes.
void DownloadManager::assemble_file(const std::string& output_path, int64_t total_length) {
    std::ofstream out(output_path, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot open output file: " + output_path);
    int64_t written = 0;
    for (auto& piece : results_) {
        int64_t to_write = std::min(static_cast<int64_t>(piece.size()), total_length - written);
        out.write(reinterpret_cast<const char*>(piece.data()), to_write);
        written += to_write;
    }
    std::cout << "[+] Wrote " << written << " bytes to " << output_path << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// TODO (Phase 5 — CORE): implement worker_thread
//
// Each std::thread runs this function. One thread per peer. They run concurrently.
//
// LOOP:
//   while (true) {
//     ── 1. POP A PIECE FROM THE QUEUE ────────────────────────────────────────
//       PieceWork work;
//       {
//           std::lock_guard<std::mutex> lock(queue_mutex_);
//           if (work_queue_.empty()) return;  // nothing left — this thread is done
//           work = work_queue_.front();
//           work_queue_.pop();
//       }  // ← LOCK RELEASED HERE.  Never hold it during the download below!
//
//     ── 2. CONNECT (first iteration only) ────────────────────────────────────
//       PeerConnection conn(addr);
//       if (!conn.connect() || !conn.handshake(info_hash, peer_id)
//                           || !conn.express_interest()) {
//           // Failed to connect — push piece back and give up on this peer
//           std::lock_guard<std::mutex> lock(queue_mutex_);
//           work_queue_.push(work);
//           return;
//       }
//       NOTE: in practice, connect once before the loop and reuse the connection.
//       If the connection drops mid-loop, push the piece back and return.
//
//     ── 3. DOWNLOAD ───────────────────────────────────────────────────────────
//       auto data = conn.download_piece(work.index, work.length);
//       if (data.empty()) {
//           // Failed — push piece back so another thread can retry
//           std::lock_guard<std::mutex> lock(queue_mutex_);
//           work_queue_.push(work);
//           return;   // or continue if you want to try the next piece with this peer
//       }
//
//     ── 4. VERIFY SHA-1 ───────────────────────────────────────────────────────
//       if (!verify_piece(data, work.expected_hash)) {
//           // Corrupted data — push piece back, this peer might be bad
//           std::lock_guard<std::mutex> lock(queue_mutex_);
//           work_queue_.push(work);
//           return;
//       }
//
//     ── 5. STORE RESULT ────────────────────────────────────────────────────────
//       {
//           std::lock_guard<std::mutex> lock(results_mutex_);
//           results_[work.index] = std::move(data);
//       }
//       ++pieces_done_;  // atomic: no mutex needed
//       std::cout << "[" << pieces_done_.load() << "/" << total_pieces_ << "] "
//                 << "piece " << work.index << "\n";
//   }
//
// THREADING RULES (DON'T SKIP THESE):
//   - Never hold two mutexes at the same time → deadlock risk.
//   - Lock queue_mutex_ only long enough to pop/push (not during download).
//   - Lock results_mutex_ only long enough to store (not during verification).
//   - pieces_done_ is atomic — increment with ++pieces_done_, not pieces_done_ = pieces_done_ + 1.
//   - std::cout from multiple threads can interleave — acceptable for a one-day project.
//     To fix: wrap prints in a separate mutex (optional).
// ─────────────────────────────────────────────────────────────────────────────
void DownloadManager::worker_thread(PeerAddress        addr,
                                    const std::string& info_hash,
                                    const std::string& peer_id) {
    // TODO: implement this function
    (void)addr; (void)info_hash; (void)peer_id;
}

// ─────────────────────────────────────────────────────────────────────────────
// TODO (Phase 5): implement download
//
// This is what the main thread calls. It sets up everything and waits.
//
// STEPS:
//
// 1. INITIALIZE:
//      total_pieces_ = tf.num_pieces();
//      results_.resize(total_pieces_);
//      pieces_done_ = 0;
//
// 2. POPULATE WORK QUEUE (no mutex needed — workers haven't started yet):
//      for (int i = 0; i < total_pieces_; ++i)
//          work_queue_.push({i, tf.piece_hashes[i], tf.piece_len(i)});
//
// 3. SPAWN THREADS — one per peer:
//      std::vector<std::thread> threads;
//      threads.reserve(peers.size());
//      for (const auto& peer : peers) {
//          threads.emplace_back(
//              &DownloadManager::worker_thread, this,
//              peer, tf.info_hash, peer_id
//          );
//      }
//      // emplace_back constructs the thread in-place.
//      // &DownloadManager::worker_thread is a pointer-to-member-function.
//      // 'this' is passed as the implicit first argument (the object to call it on).
//
// 4. JOIN THREADS — wait for all workers to finish:
//      for (auto& t : threads)
//          t.join();
//      // join() blocks until that thread returns.  Must be called before the
//      // thread object is destroyed (otherwise: std::terminate).
//
// 5. CHECK COMPLETION:
//      if (pieces_done_.load() != total_pieces_)
//          throw std::runtime_error("Download incomplete: got "
//              + std::to_string(pieces_done_.load()) + "/"
//              + std::to_string(total_pieces_) + " pieces");
//
// 6. ASSEMBLE:
//      assemble_file(output_path, tf.total_length);
// ─────────────────────────────────────────────────────────────────────────────
void DownloadManager::download(const TorrentFile&             tf,
                               const std::vector<PeerAddress>& peers,
                               const std::string&              peer_id,
                               const std::string&              output_path) {
    // TODO: implement this function
    (void)tf; (void)peers; (void)peer_id; (void)output_path;
}
