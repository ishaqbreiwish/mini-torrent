# Mini-Torrent Build Guide

## What You're Building

A simplified BitTorrent client that:
1. Parses a `.torrent` file (bencode format)
2. Contacts a tracker over HTTP to get a list of peer IPs
3. Opens TCP connections to peers and speaks the BitTorrent wire protocol
4. Downloads pieces from multiple peers **in parallel** using threads
5. Verifies each piece's SHA-1 hash and assembles the final file

**Why this project for a C++ infra internship:** it touches every concept that matters — threading primitives, lock discipline, network I/O, binary protocols, and systems-level C++. It is small enough to finish in a day but hits all the hard parts.

---

## Architecture

```
main.cpp
  ├── parse_torrent()          torrent.cpp
  │     └── bencode::decode()    bencode.cpp
  ├── get_peers()              torrent.cpp  →  HTTP GET  →  tracker  →  compact peer list
  └── DownloadManager          download.cpp
        │  (main thread: populate queue, spawn threads, join)
        └── worker_thread() × N    (one std::thread per peer)
              └── PeerConnection   peer.cpp
                    ├── connect()          POSIX TCP socket
                    ├── handshake()        68-byte protocol handshake
                    ├── express_interest() choke/unchoke negotiation
                    └── download_piece()   request/piece message loop
```

---

## Build & Run

```bash
# macOS: install OpenSSL if not already there
brew install openssl

# From the project root:
mkdir -p build && cd build
cmake ..
make -j4

# If CMake can't find OpenSSL:
cmake .. -DOPENSSL_ROOT_DIR=$(brew --prefix openssl)

# Run
./mini-torrent ../sample.torrent
./mini-torrent ../sample.torrent my_output.dat

# Run bencode tests (no network needed — do this after Phase 1)
./test_bencode
```

---

## Phase-by-Phase Guide

Work through these **in order**. Each phase builds on the previous. Run the binary after each phase to confirm it gets further.

### Phase 1 — Bencode Parser  `src/bencode.cpp`  (~30 min)

**Implement:** `bencode::decode()` and `bencode::encode()`

Bencode is the encoding used inside `.torrent` files and tracker responses. It has exactly 4 types:

| Type    | Wire format           | Example             | Decodes to          |
|---------|-----------------------|---------------------|---------------------|
| Integer | `i<decimal>e`         | `i42e`              | `int64_t` 42        |
| String  | `<length>:<bytes>`    | `4:spam`            | `std::string` "spam"|
| List    | `l<values...>e`       | `li1ei2ee`          | `[1, 2]`            |
| Dict    | `d<key><val>...e`     | `d3:fooi1ee`        | `{"foo": 1}`        |

The key insight: `decode()` calls itself recursively for lists and dicts. Pass `pos` by reference so each recursive call advances the position.

**Test gate:** `./test_bencode` must pass all tests before continuing.

---

### Phase 2 — Torrent Parsing  `src/torrent.cpp:parse_torrent`  (~20 min)

**Implement:** `parse_torrent()`

A `.torrent` file is a bencoded dict. Structure (simplified for single-file torrents):

```
d
  8:announce  <tracker URL string>
  4:info  d
    4:name          <filename string>
    12:piece length  <integer>
    6:length        <integer — total file size>
    6:pieces        <raw bytes: 20 bytes × num_pieces, each is a SHA-1 hash>
  e
e
```

**The info_hash:** `sha1_bytes(bencode::encode(info_dict))` — re-encode just the info dict, hash it. This 20-byte value is the torrent's unique identifier. You'll use it in the tracker URL and in the peer handshake.

**Test:** `./mini-torrent ../sample.torrent` should print the filename and size.

---

### Phase 3 — Tracker Request  `src/torrent.cpp:get_peers`  (~15 min)

**Implement:** `get_peers()`

The tracker is just an HTTP server. Send a GET request, get back a bencoded response with a peer list.

URL format:
```
http://tracker.example.com/announce
  ?info_hash=<url_encoded_20_bytes>
  &peer_id=<url_encoded_20_bytes>
  &port=6881
  &uploaded=0
  &downloaded=0
  &left=<total_length>
  &compact=1
```

`url_encode()` and `http_get()` are already in `utils.cpp` — just call them.

Compact peer format: the tracker returns `peers` as a raw byte string where every **6 bytes** = 4-byte IPv4 + 2-byte port (both big-endian). `parse_peers_compact()` in `torrent.cpp` handles this — it's already written.

**Test:** `./mini-torrent ../sample.torrent` should now print "Got N peers".

---

### Phase 4 — Peer Wire Protocol  `src/peer.cpp`  (~40 min)

**Implement:** `connect()`, `handshake()`, `express_interest()`, `download_piece()`

`send_msg()` and `recv_msg()` are already implemented — they handle the 4-byte length prefix framing.

See [BitTorrent Protocol Quick Reference](#bittorrent-protocol-quick-reference) below for exact byte layouts.

**Incremental testing:**
- After `connect()` + `handshake()`: connect to one peer manually in a test, check you get a valid handshake back.
- After `download_piece()`: download one piece, call `sha1_bytes()` on it, compare to `tf.piece_hashes[0]`.

---

### Phase 5 — Download Manager  `src/download.cpp`  (~45 min)  ← THE MAIN EVENT

**Implement:** `download()` and `worker_thread()`

This is the threading core. Read [C++ Threading Reference](#c-threading-reference) before starting.

**The pattern: work queue + thread pool**

```
Main thread                         Worker threads (one per peer, run concurrently)
─────────────────────               ──────────────────────────────────────────────
push all pieces to queue            loop:
spawn N threads                       lock → pop piece → unlock
join all threads                      connect + handshake + express_interest
assemble file                         download piece
                                      if fail → lock → push back → unlock → return
                                      verify SHA-1
                                      if fail → lock → push back → unlock → return
                                      lock → store result → unlock
                                      ++pieces_done  (atomic)
                                      print progress
```

**Critical rule:** release `queue_mutex_` before the network call. The download can take seconds — holding the lock that long blocks every other thread from getting work.

---

## C++ Threading Reference

### `std::thread`

```cpp
#include <thread>

void work(int x) { /* runs in new thread */ }

std::thread t(work, 42);   // spawn: calls work(42) in a new thread
t.join();                  // main thread blocks until t finishes — MUST be called

// Pointer-to-member-function (how worker_thread is spawned):
struct Foo { void bar(int x); };
Foo obj;
std::thread t2(&Foo::bar, &obj, 42);  // calls obj.bar(42) in new thread

// Store multiple threads:
std::vector<std::thread> threads;
threads.emplace_back(work, 1);  // construct thread directly inside vector
threads.emplace_back(work, 2);
for (auto& t : threads) t.join();
```

Rules:
- Every `std::thread` must be either `join()`ed or `detach()`ed before destruction — otherwise `std::terminate()`.
- After `join()`, the thread object is no longer joinable. Don't join twice.

---

### `std::mutex` and lock guards

```cpp
#include <mutex>

std::mutex m;

// lock_guard — RAII lock. Locks on construction, unlocks on destruction.
// Use when you want to hold the lock for the entire scope block.
{
    std::lock_guard<std::mutex> lock(m);
    // ... critical section ...
}  // ← unlocked here automatically (even if an exception is thrown)

// unique_lock — more flexible: can unlock early.
// Required when using condition_variable.
{
    std::unique_lock<std::mutex> lock(m);
    // ... do something ...
    lock.unlock();   // release early
    // ... no longer protected ...
    lock.lock();     // re-acquire if needed
}
```

**Common mistake:** holding the mutex while doing slow work (a network download, file I/O). This serializes all threads. Pop from the queue fast, drop the lock, *then* do the work.

---

### `std::atomic<T>`

```cpp
#include <atomic>

std::atomic<int> counter{0};

++counter;                  // atomic increment (thread-safe, no mutex needed)
counter.fetch_add(1);       // same, returns old value
int val = counter.load();   // read
counter.store(5);           // write
```

Use `atomic` when a single variable needs to be read or written atomically (like a progress counter). If you need to read-then-act on a value without interference (e.g., pop from a queue), use a mutex — atomics don't help with compound operations.

---

### `std::condition_variable` (optional enhancement)

A condvar lets one thread sleep until another thread signals it. Useful for: "main thread prints progress each time a piece finishes."

```cpp
#include <condition_variable>
#include <mutex>

std::mutex              cv_mutex;
std::condition_variable cv;
int done = 0;

// Thread A — wait for progress:
{
    std::unique_lock<std::mutex> lock(cv_mutex);
    cv.wait(lock, [&]{ return done > last_printed; });  // releases lock while sleeping
    // ... print progress ...
}

// Thread B — signal progress:
{
    std::lock_guard<std::mutex> lock(cv_mutex);
    ++done;
    cv.notify_one();   // wake up one waiter
}
```

`cv.wait(lock, pred)` is equivalent to `while (!pred()) cv.wait(lock)` — the predicate guards against **spurious wakeups** (the OS can wake a thread for no reason; the predicate re-checks before proceeding).

---

### Data races vs. deadlocks

| Problem    | Cause                                                    | Symptom                        | Fix                                  |
|------------|----------------------------------------------------------|--------------------------------|--------------------------------------|
| Data race  | Two threads access shared data, at least one writes, no sync | Corrupted data, UB, crashes | Protect with mutex or use atomic     |
| Deadlock   | Thread A holds mutex 1, waits for 2. Thread B holds 2, waits for 1 | Program hangs forever | Always acquire mutexes in the same order |

In this project: `queue_mutex_` and `results_mutex_` are independent — you never need both at once. Never nest one inside the other.

---

## BitTorrent Protocol Quick Reference

### Handshake (68 bytes — send then receive)

```
Byte offset   Length   Content
───────────   ──────   ───────────────────────────────────────
0             1        0x13  (= 19, length of the protocol string)
1             19       "BitTorrent protocol"
20            8        0x00 × 8  (reserved, all zeros)
28            20       info_hash  (raw bytes — the SHA-1 you computed)
48            20       peer_id    (raw bytes — your 20-byte ID)
```

After receiving the peer's 68-byte response, validate that bytes `[28..48)` match your `info_hash`. Mismatch means you connected to a peer for a different torrent.

---

### Message Format (all messages after the handshake)

```
[4 bytes, big-endian]   length   = 1 + payload.size()
[1 byte]                id       = message type
[length-1 bytes]        payload  = depends on message type
```

Keep-alive: `length = 0` (no id, no payload) — just ignore and read the next message.

---

### Message Types

| ID | Name       | Payload                                         | When sent                      |
|----|------------|-------------------------------------------------|--------------------------------|
| 0  | Choke      | —                                               | Peer stops serving you         |
| 1  | Unchoke    | —                                               | Peer agrees to serve you       |
| 2  | Interested | —                                               | You want data (you send this)  |
| 5  | Bitfield   | bitmask of which pieces the peer has            | First message after handshake  |
| 6  | Request    | `index(4B) + begin(4B) + length(4B)` all BE     | You request one 16 KiB block   |
| 7  | Piece      | `index(4B) + begin(4B) + data` all BE           | Peer sends block data          |

---

### Piece Request Flow

```
You                              Peer
────────────────────────         ────────────────────
→ Interested (id=2)
                                 ← Unchoke (id=1)
→ Request(idx=0, begin=0,    len=16384)
                                 ← Piece(idx=0, begin=0, data[16384])
→ Request(idx=0, begin=16384, len=16384)
                                 ← Piece(idx=0, begin=16384, data[16384])
... repeat until piece is complete ...
```

Block size is **16384 bytes** (16 KiB) — this is the standard max. The last block of a piece may be smaller.

---

### Info Hash

`info_hash = SHA1(bencode::encode(info_dict))`

- Raw 20 bytes — never hex-encode it when sending over the wire.
- URL-encode it (percent-encode every byte) for the tracker HTTP request.
- It is the torrent's global identifier — both the tracker and every peer use it to identify which torrent you want.

---

## What's Provided vs. What You Implement

| File              | Provided (no edits needed)            | You implement                   |
|-------------------|---------------------------------------|---------------------------------|
| `utils.cpp`       | `sha1_bytes`, `url_encode`, `http_get`| —                               |
| `bencode.cpp`     | —                                     | `decode`, `encode`              |
| `torrent.cpp`     | `parse_peers_compact`                 | `parse_torrent`, `get_peers`    |
| `peer.cpp`        | `send_msg`, `recv_msg`, `drain_one_msg`, constructor/destructor | `connect`, `handshake`, `express_interest`, `download_piece` |
| `download.cpp`    | `verify_piece`, `assemble_file`       | `worker_thread`, `download`     |
| `main.cpp`        | everything                            | —                               |

---

## Debugging Tips

**Build incrementally.** Implement bencode → test_bencode passes → implement torrent parsing → run binary → etc. Don't write all five phases before compiling.

**Set socket timeouts.** Dead peers will block a thread indefinitely without them. Add this right after `connect()` succeeds in `peer.cpp`:
```cpp
struct timeval tv{5, 0}; // 5-second timeout
setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
setsockopt(sockfd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
```

**Byte order is always big-endian on the wire.** Use `htonl()` before sending a 4-byte integer, `ntohl()` after receiving. `htonl` = host-to-network (big-endian), `ntohl` = the reverse. Both are in `<arpa/inet.h>`.

**SHA-1 mismatch on info_hash** usually means you're hashing the wrong bytes. You must hash the bencoded info dict — not the whole `.torrent` file, not the decoded struct.

**Garbled stdout from multiple threads** is expected — `std::cout` is not thread-safe. For a one-day project, accept it or add a simple print mutex:
```cpp
std::mutex print_mutex;
// in worker:
std::lock_guard<std::mutex> lock(print_mutex);
std::cout << "...\n";
```

**Compile with warnings and AddressSanitizer** to catch races and memory errors early:
```bash
cmake .. -DCMAKE_CXX_FLAGS="-Wall -Wextra -fsanitize=address,thread"
```
Note: `-fsanitize=address` and `-fsanitize=thread` cannot be used together — pick one.

**If the tracker returns no peers** for `sample.torrent`, the torrent might be dead. Test your networking code by manually hardcoding one well-known public tracker and a real torrent hash, or use Wireshark to inspect what your client sends.
