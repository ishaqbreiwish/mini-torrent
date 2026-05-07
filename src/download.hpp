#pragma once
#include "torrent.hpp"
#include <vector>
#include <string>
#include <queue>
#include <mutex>
#include <atomic>
#include <thread>

// One unit of downloadable work: a single piece
struct PieceWork {
    int         index;
    std::string expected_hash; // 20 raw bytes (SHA-1) — verify after download
    int64_t     length;        // exact byte count (last piece may differ from piece_length)
};

// ─────────────────────────────────────────────────────────────────────────────
// DownloadManager — parallel piece downloader
//
// THREADING MODEL:
//   - Main thread populates the work queue, spawns worker threads, then joins.
//   - Each worker thread owns one PeerConnection.
//   - Workers pull PieceWork items from the shared queue, download them,
//     verify SHA-1, and write results into a shared results vector.
//
// SHARED STATE and its guards:
//   work_queue_    ← protected by queue_mutex_
//   results_       ← protected by results_mutex_
//   pieces_done_   ← std::atomic, no mutex needed for simple increment/read
// ─────────────────────────────────────────────────────────────────────────────
class DownloadManager {
public:
    // Download tf using peers. Blocks until complete or throws on fatal error.
    void download(const TorrentFile&            tf,
                  const std::vector<PeerAddress>& peers,
                  const std::string&              peer_id,
                  const std::string&              output_path);

private:
    // ── Shared state ────────────────────────────────────────────────────────

    std::queue<PieceWork>             work_queue_;    // pieces yet to download
    std::mutex                        queue_mutex_;   // guards work_queue_

    std::vector<std::vector<uint8_t>> results_;       // results_[i] = data for piece i
    std::mutex                        results_mutex_; // guards results_

    std::atomic<int>                  pieces_done_{0};
    int                               total_pieces_ = 0;

    // ── Private helpers ──────────────────────────────────────────────────────

    // Worker thread entry point — loops until work_queue_ is empty.
    void worker_thread(PeerAddress        addr,
                       const std::string& info_hash,
                       const std::string& peer_id);

    // Returns true if sha1_bytes(data) == expected_hash
    bool verify_piece(const std::vector<uint8_t>& data,
                      const std::string&           expected_hash);

    // Write results_[0..n-1] in order to output_path (provided, no TODO)
    void assemble_file(const std::string& output_path, int64_t total_length);
};
