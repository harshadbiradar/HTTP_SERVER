# HTTP Server — Multi Reactor

> **Branch:** `MultiReactor`
> **Evolved from:** [`mulqu`] — read that README (and [`sub`]) for full arch context.

---

## What Changed

`mulqu` still had a central bottleneck — the main thread was the sole owner of accept, epoll, and dispatch. Workers were fast, but everything funneled through one reactor. RR dispatch, per-worker BQs, per-worker eventfd — all of that coordination exists only because of that centralization.

`MultiReactor` eliminates the problem at the root.

| Component | `mulqu` | `MultiReactor` |
|---|---|---|
| Reactors | 1 main thread | N threads, each a full reactor |
| Accept | Main thread only | Each thread accepts on its own socket |
| epoll | Single shared `epoll_fd` | One `epoll_fd` per thread |
| Connection ownership | Main assigns to worker | Each thread owns its connections |
| Task queue (BQ) | Per-worker BQ | Gone |
| write_ready_queue | Per-worker | Gone |
| eventfd | Per-worker | Gone |
| Dispatch | Round-Robin from main | Kernel (SO_REUSEPORT) |

---

## SO_REUSEPORT

Each thread creates its own server socket bound to the same port using `SO_REUSEPORT`. The kernel distributes incoming connections across sockets — no userspace dispatch, no RR counter, no coordination between threads whatsoever.

```
Thread 1: socket() → bind(:8080, SO_REUSEPORT) → listen() → own epoll → own connections
Thread 2: socket() → bind(:8080, SO_REUSEPORT) → listen() → own epoll → own connections
...
Thread N: socket() → bind(:8080, SO_REUSEPORT) → listen() → own epoll → own connections

Kernel → distributes incoming connections across threads
```

Each thread runs the full reactor loop independently — accept, recv, generate response, send. No shared state, no locks between threads.

---

## Why This Is The Right End State

The coordination overhead in `sub` and `mulqu` — BQ, write_ready_queue, eventfd, RR dispatch — existed entirely to manage the boundary between the single reactor and the worker pool. `MultiReactor` removes that boundary. There is no boundary. Each thread *is* the reactor.

The bottleneck is now hardware: core count and NIC throughput.

---

## Result

~600k RPS on loopback, across a 120-test parameter sweep (threads, epoll maxEvents, connection counts).

---

## Code Impact

Significant restructuring compared to `mulqu` — the reactor loop, connection handling, and response generation all collapse into a single per-thread flow. `Blocking_Queue`, `write_ready_queue`, and eventfd wiring are removed entirely.