# C++ Baseline HTTP Server (Single-Reactor Pattern)

This branch is part of the foundational phase of the project, implementing a classic **Single-Reactor** pattern using `epoll`.

## Architecture: Single-Reactor
- **Single-Threaded I/O:** A single loop handles all `epoll_wait`, `accept`, `read`, and `write` operations.
- **Worker Hand-off:** Requests are processed by a thread pool using a shared global queue.
- **Clean Abstractions:** Demonstrates the core concepts of non-blocking I/O and event-driven programming in modern C++.

## Performance
- **Throughput:** ~5,000–10,000 Requests Per Second (RPS).
- **Scalability:** Limited by the single-thread reactor and lock contention on the global queue.

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

## Project Evolution
This project evolved through several architectural stages:
1. **master/sub:** (This branch) Baseline Single-Reactor pattern.
2. **mulqu:** Multi-Queue design with dedicated worker pools and eventfd signaling.
3. **MultiReactor:** Optimized Shared-Nothing architecture for maximum performance.
