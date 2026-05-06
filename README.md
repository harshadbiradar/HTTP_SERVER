# C++ High-Performance Server Engine

A server engine built from scratch in C++ to explore how far a single-machine, multi-core architecture can push raw throughput. No frameworks, no libraries — just Linux syscalls, epoll, and threads.

**The project evolved through 3 distinct architectures**, each one a response to a bottleneck discovered in the previous. The branches are preserved as a record of that progression.

## Final Architecture (Shared-Nothing Multi-Reactor)

Each worker thread is a fully independent reactor — no shared state, no mutexes, no inter-thread signaling.

```
Thread 0:  socket(SO_REUSEPORT) → bind → listen → epoll_wait → accept → read → write
Thread 1:  socket(SO_REUSEPORT) → bind → listen → epoll_wait → accept → read → write
  ...
Thread N:  socket(SO_REUSEPORT) → bind → listen → epoll_wait → accept → read → write

(No coordination between threads. Kernel distributes connections via SO_REUSEPORT.)
```

| Component | Design |
|---|---|
| Load balancing | `SO_REUSEPORT` — kernel distributes connections across threads |
| Event model | Edge-triggered `epoll` (`EPOLLET`) per thread |
| Connection storage | Flat pre-allocated vector indexed by FD (O(1), cache-friendly) |
| Write strategy | Inline after read; `EPOLLOUT` registered only when `send()` returns `EAGAIN` |
| Synchronization | None — fully shared-nothing |

---

## Architectural Evolution

### Branch: `sub` — Single Reactor + Worker Pool

**The starting point.** One epoll thread handles all accepts and reads, dispatches work to a pool of worker threads, which signal back via `eventfd` when responses are ready.

```
Main Thread (epoll) → accept → read → Blocking_Queue → Worker Thread
Worker Thread → process → Writable_Queue → eventfd → Main Thread → send()
```

**Key technical decisions:**
- Edge-triggered `epoll` (`EPOLLET`) — chose the hardest mode from the start, requiring drain-until-`EAGAIN` loops
- `eventfd` for worker→main IPC — the correct Linux primitive for waking an epoll loop from another thread
- Hand-written `Blocking_Queue` with `std::mutex` + `std::condition_variable`, proper shutdown semantics, and `try_pop()` for non-blocking drain
- `shared_ptr<Connection>` for safe cross-thread lifetime management
- `std::move` on request buffers into worker lambdas to avoid copies in the hot path

**Bottleneck discovered:** The single `Blocking_Queue` becomes a contention point — N worker threads and 1 main thread all compete for one mutex.

---

### Branch: `mulqu` — Per-Worker Queues

**Eliminated the shared queue bottleneck** by partitioning into per-worker queues with per-worker `eventfd` signaling.

```
Main Thread (epoll) → accept → read → worker_queue[fd % N] → Worker[N]
Worker[N] → process → write_queue[N] → eventfd[N] → Main Thread → send()
```

**Key technical decisions:**
- **Per-worker queues + per-worker eventfd** — zero cross-worker contention on both task dispatch and response paths
- **Rewrote `Blocking_Queue` from scratch** — replaced `std::queue` (heap-allocated nodes) with a circular buffer (`head`/`tail`/`count`, modular arithmetic, backed by `unique_ptr<vector<T>>`)
- **`push()` returns `was_empty`** — caller skips the `eventfd` `write()` syscall if items were already queued. Eliminates redundant kernel transitions.
- **Batch drain with `MAX_BATCH=512` cap** — prevents response processing from starving accept/read
- **`fd % n_workers` routing** — deterministic connection-to-worker assignment with zero overhead
- **Connection buffers upgraded** — `std::string` → `std::vector<char>` with pre-allocated 16KB and direct `recv()` into buffer offset

**Bottleneck discovered:** The main thread is still a single point of serialization. All accepts, all reads, all writes funnel through one epoll loop. The worker threads help with "processing," but for a simple response there's nothing to process — the main thread is doing all the real work and the queues just add latency.

---

### Branch: `multireactor` / `master` — Shared-Nothing

**Eliminated all inter-thread coordination** by making each thread a fully independent server.

**What was deleted:**
- `Blocking_Queue` — gone (no queues needed)
- `eventfd` — gone (no inter-thread signaling needed)
- `Server` class — gone (each thread sets up its own server)
- `shared_ptr<Connection>` — gone (connections never cross thread boundaries)
- All mutexes and condition variables — gone
- Header count: 7 → 5. Source files: 6 → 4. ~300 lines of concurrency code removed.

**What replaced it:**
- `SO_REUSEPORT` — each thread creates its own socket on the same port; kernel distributes connections
- FD-indexed `vector<Connection*>(65536)` — replaced `unordered_map<int, shared_ptr<Connection>>` for O(1) lookups with zero hash overhead
- Inline read→write — response generated and sent in the same event loop iteration, no thread hop, no queue, no context switch
- Lazy `EPOLLOUT` — only registers write interest when `send()` can't complete; de-registers after drain to avoid spurious wake-ups
- `TCP_NODELAY` — disables Nagle's algorithm for immediate packet transmission in request-response patterns

---

## Performance

Benchmarked via `wrk` on loopback with a 120-configuration parameter sweep (threads=5–12, maxEvents=4096–8192, connections=5K–25K). Each test run captured `mpstat` (per-core CPU), `pidstat` (per-thread stats + context switches), and `vmstat` (system-wide).

| Metric | Result |
|---|---|
| Peak throughput | ~601,000 RPS (t=11, e=8192, c=5000) |
| Best avg latency | ~20ms (t=12, e=8192, c=5000) |
| Concurrency tested | 5,000 – 25,000 connections |

> **Note:** These numbers were measured with AddressSanitizer and UndefinedBehaviorSanitizer enabled in the build (typically 2–3x userspace overhead). Throughput without sanitizers is expected to be significantly higher.

**Key findings:**
- Linear RPS scaling up to t=11 (physical core count); t=12 dips slightly due to context switching
- `maxEvents` (4096 vs 8192) has <2% RPS impact — epoll batch size is not the bottleneck
- RPS scales well up to c=15k (~516k RPS); drops at higher connection counts due to kernel TCP stack overhead — verified via `mpstat` (%sys spikes to 97%+ at c=25k vs ~42% at c=15k)

---

## Build & Run

### Prerequisites
- Linux (kernel with `SO_REUSEPORT` support)
- CMake 3.10+
- G++ with C++17 support

### Build
```bash
mkdir -p build && cd build
cmake ..
cmake --build .
```

### Run
```bash
./concurrent_server --threads 11 --maxperthrd 8192
```

### Benchmark
```bash
wrk -t11 -c5000 -d20s --latency http://localhost:8080/
```

---

## Project Structure

```
├── include/
│   ├── common_header.h        # Shared includes, logging macros
│   ├── config.h               # Runtime configuration
│   ├── Connection.h           # Per-connection state (buffers, FD, epoll event)
│   ├── Connection_manager.h   # Accept, read, write, close logic
│   └── Thread_pool.h          # Thread lifecycle + per-thread server setup
├── src/
│   ├── main.cpp               # Entry point, CLI arg parsing
│   ├── thread_pool.cpp        # Thread function, server setup, event loop
│   └── connection_manager.cpp # Connection handling, I/O, epoll management
├── tests/
│   ├── test1.sh               # Basic parameter sweep
│   └── test2.sh               # Full 120-config stress test with system monitoring
└── CMakeLists.txt
```