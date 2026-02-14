# C++ Multi-Reactor HTTP Server (Shared-Nothing Architecture)

This is the high-performance branch of the project, implementing a **Shared-Nothing Multi-Reactor** architecture. It is designed to scale linearly with CPU cores and handle massive concurrency with minimal latency.

## Architecture: Shared-Nothing Multi-Reactor
- **Zero Contention:** Every thread in the pool is its own independent reactor. There are no shared connection maps, no global mutexes, and no inter-thread signaling.
- **Kernel Load Balancing:** Utilizes `SO_REUSEPORT` to allow the Linux kernel to distribute incoming TCP connections directly across all worker threads.
- **Edge-Triggered Epoll:** Uses `EPOLLET` for the most efficient event notification, enabling a "Zero-Syscall" write path where responses are attempted immediately after reads.
- **Data Locality:** Connection data is stored in a flat pre-allocated array (vector), maximizing CPU cache hits and eliminating pointer-chasing overhead.

## Performance
- **Throughput:** ~585,000+ Requests Per Second (RPS) on standard multicore hardware.
- **Latency:** Sub-millisecond internal latency with very low tail latency (p99).
- **Benchmark:** Verified using `wrk` with 5,000+ concurrent connections.

## Prerequisites
- Linux OS (Kernel supports `SO_REUSEPORT`)
- CMake 3.10+
- G++ (Support for C++17)

## Building the Project
```bash
mkdir -p build && cd build
cmake ..
cmake --build .
```

## Running the Server
```bash
./concurrent_server --threads 11 --events 8192
```

## Benchmarking
To see the full potential of this architecture, use `wrk`:
```bash
wrk -t11 -c5000 -d10s --latency http://localhost:8080/
```

## Project Evolution
This project evolved through several architectural stages:
1. **master/sub:** Baseline Single-Reactor pattern.
2. **mulqu:** Multi-Queue design with dedicated worker pools and eventfd signaling.
3. **MultiReactor:** (This branch) Optimized Shared-Nothing architecture for maximum performance.
