# C++ Multi-Reactor HTTP Server

High-performance HTTP/1.1 server implementing a **Shared-Nothing Multi-Reactor** architecture, designed to scale linearly with CPU cores and handle massive concurrency with minimal latency.

## Architecture

Each worker thread is a fully independent reactor — no shared state, no mutexes, no inter-thread signaling.

| Component | Design |
|---|---|
| Load balancing | `SO_REUSEPORT` — kernel distributes connections across threads |
| Event model | Edge-triggered `epoll` (`EPOLLET`) per thread |
| Connection storage | Flat pre-allocated vector per worker (cache-friendly) |
| Synchronization | None — fully shared-nothing |

## Performance

Benchmarked via `wrk` (loopback, 120-test parameter sweep — t=5–12, maxEvents=4096–8192, c=5k–25k):

| Metric | Result |
|---|---|
| Peak throughput | ~601,000 RPS (t=11, e=8192, c=5000) |
| Best avg latency | ~20ms (t=12, e=8192, c=5000) |
| Concurrency tested | 5,000 – 25,000 connections |

**Key findings:**
- Linear RPS scaling up to t=11 (physical core count); t=12 dips slightly due to context switching
- `maxEvents` (4096 vs 8192) has <2% RPS impact — epoll batch size is not the bottleneck
- - RPS scales well up to c=15k (~516k RPS); drops at higher connection counts due to kernel TCP stack overhead — verified via mpstat (%sys spikes to 97%+ at c=25k vs ~42% at c=15k)
## Prerequisites

- Linux (kernel with `SO_REUSEPORT` support)
- CMake 3.10+
- G++ with C++17 support

## Build
```bash
mkdir -p build && cd build
cmake ..
cmake --build .
```

## Run
```bash
./concurrent_server --threads 11 --maxperthrd 8192
```

## Benchmark
```bash
wrk -t11 -c5000 -d20s --latency http://localhost:8080/
```

## Project Evolution

| Branch | Architecture | Key characteristic |
|---|---|---|
| `sub` | Single-Reactor | Baseline |
| `mulqu` | Multi-Queue | Per-worker queues + eventfd signaling |
| `multireactor` | Shared-Nothing | SO_REUSEPORT, zero coordination |
| `master` | Shared-Nothing | Current main — merged from `multireactor` |