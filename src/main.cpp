// ============================================================
// PROVIDED — no changes needed here.
// Wires together parse → tracker → download.
// ============================================================

#include "torrent.hpp"
#include "download.hpp"
#include <iostream>
#include <string>
#include <random>

// Generate a 20-byte peer ID.
// Convention: "-" + client_tag + version + "-" + random bytes
static std::string make_peer_id() {
    std::string id = "-MT0001-"; // MT = Mini Torrent, 0001 = version
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, 255);
    for (int i = 0; i < 12; ++i)
        id += static_cast<char>(dist(rng));
    return id; // exactly 20 bytes
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <file.torrent> [output_file]\n";
        return 1;
    }

    const std::string torrent_path = argv[1];
    const std::string output_path  = (argc >= 3) ? argv[2] : "output.dat";
    const std::string peer_id      = make_peer_id();

    try {
        // Phase 2
        std::cout << "[*] Parsing " << torrent_path << "\n";
        TorrentFile tf = parse_torrent(torrent_path);
        std::cout << "[*] File:   " << tf.name           << "\n"
                  << "[*] Size:   " << tf.total_length   << " bytes\n"
                  << "[*] Pieces: " << tf.num_pieces()   << " x "
                                    << tf.piece_length   << " bytes\n";

        // Phase 3
        std::cout << "[*] Contacting tracker: " << tf.announce << "\n";
        auto peers = get_peers(tf, peer_id);
        std::cout << "[*] Got " << peers.size() << " peers\n";
        if (peers.empty()) throw std::runtime_error("Tracker returned no peers");

        // Phase 5
        std::cout << "[*] Starting download with " << peers.size() << " threads...\n";
        DownloadManager mgr;
        mgr.download(tf, peers, peer_id, output_path);

        std::cout << "[+] Done! -> " << output_path << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[-] Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
