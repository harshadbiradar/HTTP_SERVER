# C++ Multi-Queue HTTP Server (Reactor + Worker Pool)

This branch implements a **Reactor + Worker Pool** architecture with **Per-Worker Queues**, designed to parallelize business logic while centralizing connection management.

## Architecture: Multi-Queue Reactor
- **Dedicated I/O Thread:** A single main thread handles `epoll_wait` and all connection acceptance/reading.
- **Per-Worker Queues:** Instead of a single global queue, each worker thread has its own task queue to minimize lock contention.
- **Eventfd Signaling:** Uses Linux `eventfd` to signal workers when new tasks are available and to signal the I/O thread when responses are ready to be sent.
- **Concurrency:** Effectively uses multiple cores for request processing (Business Logic) but is bounded by the single-thread I/O performance.

## Performance
- **Throughput:** ~60,000–65,000 Requests Per Second (RPS).
- **Latency:** Higher than Shared-Nothing due to inter-thread context switching (~300ms avg under heavy load).
- **Scalability:** Scales logic well, but I/O becomes a bottleneck on very high core counts.

## Prerequisites
- Linux OS
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
./concurrent_server --threads 11 --queue 16384
```

## Project Evolution
This project evolved through several architectural stages:
1. **master/sub:** Baseline Single-Reactor pattern.
2. **mulqu:** (This branch) Multi-Queue design with dedicated worker pools and eventfd signaling.
3. **MultiReactor:** Optimized Shared-Nothing architecture for maximum performance.
