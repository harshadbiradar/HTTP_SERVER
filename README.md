# HTTP Server — Single Reactor + Single_Blocking_Queue(Shared)

> **Branch:** `sub`
> **Role in project:** Base / V1 architecture — the starting point from which later designs evolved.

---

## Overview

This branch implements a **Single Reactor** pattern: one main thread owns all I/O multiplexing, connection management, and task dispatch. A pool of worker threads handles CPU-bound response generation. Coordination between the main thread and workers happens through shared data structure built from scratch: a **Blocking Queue(BQ)** (inbound tasks) and a **Writable Queue** ((also a BQ variant)outbound responses), with an `eventfd` used to signal the reactor when responses are ready.

---

## Architecture

```
                        ┌──────────────────────────────────┐
                        │           MAIN THREAD            │
                        │         (Single Reactor)         │
                        │                                  │
                        │  epoll (ET)                      │
                        │   ├── accept() new connections   │
                        │   ├── recv()  incoming data      │
                        │   └── eventfd ──────────────┐    │
                        │                             │    │
                        │  Drain writable_queue ◄─────┘    │
                        │  send() responses                │
                        └────────────┬─────────────────────┘
                                     │ submit task
                                     ▼
                        ┌─────────────────────────┐
                        │    Blocking Queue (BQ)  │  ◄─── built from scratch
                        │   (mutex + condvar)     │        (mutex + condvar)
                        └────────────┬────────────┘
                                     │ take task
                         ┌───────────┼───────────┐
                         ▼           ▼           ▼
                    ┌─────────┐ ┌─────────┐ ┌─────────┐
                    │Worker T1│ │Worker T2│ │Worker TN│
                    │         │ │         │ │         │
                    │generate │ │generate │ │generate │
                    │response │ │response │ │response │
                    └────┬────┘ └────┬────┘ └────┬────┘
                         │           │           │
                         └───────────┴───────────┘
                                     │ push (fd, response)
                                     ▼
                        ┌────────────────────────┐
                        │   Writable_queue (BQ)  │  ◄─── shared, mutex-protected
                        └────────────┬───────────┘
                                     │ signal
                                     ▼
                                  eventfd
                                     │
                              (wakes main thread
                               via epoll)
```

---

## Component Breakdown

### Main Thread — The Reactor

Owns the single `epoll_fd` and runs the event loop. Responsible for:

| Responsibility | Detail |
|---|---|
| **Accept** | Detects `EPOLLIN` on the listen socket; calls `accept()`, registers new fd with ET epoll |
| **Read** | On client fd events, calls `recv()` in a loop until `EAGAIN` (non-blocking, ET semantics) |
| **Dispatch** | Wraps parsed request + fd into a `Task` and pushes it onto the Blocking Queue |
| **Response flush** | When `eventfd` fires, drains `writable_queue` and calls `send()` for each ready fd |

Edge-triggered mode means the reactor reads until `EAGAIN` on every activation — partial reads must be handled explicitly.

### Blocking Queue

A thread-safe, **bounded MPMC** queue built from scratch. In this architecture the main thread is the sole producer, but the implementation is generic and producer-count agnostic — any number of producers or consumers can use it safely.

Internals:
- `std::mutex` for mutual exclusion
- `std::condition_variable` for blocking both sides (producers when full, consumers when empty)
- `std::queue<T>` as the underlying container
- `max_size` default: 100 (configurable via `set_size(n)`)
- `std::atomic<bool> shutdown_flag` for clean teardown

```
Producer:  push(task)  → lock → block if full → enqueue → notify_one → unlock
Consumer:  pop()       → lock → block if empty → dequeue → notify_one → unlock
Consumer:  try_pop()   → lock → return T{} if empty (non-blocking) → unlock
```

**Shutdown:** `shutdown()` sets the atomic flag and calls `notify_all()`, unblocking all waiting producers and consumers so threads can exit cleanly. A consumer waking to an empty queue during shutdown returns `T{}` as a sentinel.

### Thread Pool

A fixed-size thread pool wrapping the BQ internally. Typed as `Blocking_Queue<std::function<void()>>` — tasks are submitted as callables.

**Key design points:**
- Pool is not live at construction — `create_pool(N, queue_size)` must be called explicitly to spawn threads and configure the BQ size
- `submit(task)` moves the callable into the BQ; the first available worker picks it up
- Destructor calls `shutdown()` — RAII, no manual teardown needed from the caller
- `shutdown()` is **idempotent** — guarded by a `shutdown_flag` check, safe to call multiple times
- `shutdown()` calls `BQ.shutdown()` (unblocks all waiting workers via `notify_all()`), then `close_pool()` which joins all threads — blocks until all workers have exited cleanly

**Worker loop:**
```cpp
while (true) {
    auto task = BQ.pop();
    if (!task) break;   // T{} sentinel → BQ shutdown, exit
    task();
}
```

`BQ.pop()` returns an empty `std::function` (`T{}`) when the queue is shutting down and empty — `!task` evaluates true, and the worker exits. No explicit poison-pill object needed; the BQ's shutdown sentinel doubles as the exit signal.

Each worker executes the submitted callable — generates the HTTP response, pushes to `writable_queue`, signals `eventfd`. Workers never touch the socket directly.

### Write-Ready Queue

A simple mutex-protected queue shared between all workers and the main thread. Workers push completed `{fd, response}` pairs; the main thread drains it after being notified via `eventfd`.

### eventfd

A single `eventfd` registered with epoll on the main thread. Workers write to it after pushing to `writable_queue`. The main thread wakes on `EPOLLIN`, reads the counter, then drains the queue.

---

## Data Flow — Request Lifecycle

```
Client connects
      │
      ▼
[epoll] EPOLLIN on listen_fd
      │
      ▼
accept() → register client_fd with ET epoll
      │
      ▼
[epoll] EPOLLIN on client_fd
      │
      ▼
recv() loop until EAGAIN → parse HTTP request
      │
      ▼
BQ.push({client_fd, parsed_request})
      │
      ▼
Worker thread wakes → generate response
      │
      ▼
writable_queue.push({client_fd, response})
eventfd.write(1)
      │
      ▼
[epoll] EPOLLIN on eventfd (main thread wakes)
      │
      ▼
drain writable_queue → send() each response
```

---

## Known Bottleneck — Why This Arch Was Superseded

All worker threads contend on a **single Blocking Queue** mutex. Under high concurrency this becomes a serialization point:

- `push()` and `pop()` both acquire the same lock
- High thread counts amplify lock contention
- Workers stall on `pop()` even when work is available, if another thread holds the lock

See [`mulqu`] branch — evolved from this arch with targeted changes to address the shared queue bottleneck.

---

## Build & Run

```bash
# Build
make

# Run (example: 8 worker threads)
./server --port 8080 --workers 8

# Benchmark (wrk)
wrk -t4 -c1000 -d30s http://localhost:8080/
```

---

## Dependencies

- Linux kernel ≥ 4.5 (epoll, eventfd)
- C++17
- pthreads (or `std::thread`)
- No external libraries

---

## File Structure

```
.
├── build/
├── include/
│   ├── Blocking_Queue.h        # Bounded MPMC queue (mutex + condvar)
│   ├── Thread_pool.h           # Thread pool interface
│   ├── Server.h                # Reactor / epoll event loop
│   ├── Connection.h            # Per-connection state
│   ├── Connection_manager.h    # Connection lifecycle management
│   ├── common_header.h         # Shared includes
│   └── config.h                # Server configuration constants
├── src/
│   ├── main.cpp                # Entry point
│   ├── server.cpp              # Reactor implementation
│   ├── thread_pool.cpp         # Thread pool implementation
│   ├── blocking_queue.cpp      # BQ implementation
│   ├── connection.cpp          # Connection state implementation
│   └── connection_manager.cpp  # Connection lifecycle implementation
├── tests/
├── CMakeLists.txt
└── .gitignore
```

---

## Position in Project Evolution

| Branch | Architecture |
|---|---|
| **this** | Single Reactor + Single Shared BQ (base) |
| [`mulqu`] | Evolved from this — see its README |
