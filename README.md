# HTTP Server — Per-Worker Queues

> **Branch:** `mulqu`
> **Evolved from:** [`sub`] — read that README first for full arch context.

---

## What Changed

This branch is a targeted fix on `sub`. The core arch — Single Reactor, main thread owning epoll/accept/recv/dispatch, workers generating responses — is identical. Only the coordination layer between main and workers was changed.

| Component | `sub` | `mulqu` |
|---|---|---|
| Task queue | Single shared BQ (all workers contend) | One BQ per worker |
| Dispatch | Main pushes to single BQ | Main dispatches via Round-Robin across worker BQs |
| Write-ready queue | Single shared queue | One per worker |
| eventfd | Single, shared | One per worker |

---

## Why

In `sub`, all workers competed on one BQ mutex — a serialization point under load. High thread counts made it worse: workers stalling on `pop()` even when the queue had items, just waiting for the lock.

Sharding into per-worker BQs eliminates that contention entirely. Each worker owns its queue — no sharing, no lock competition between workers.

---

## Round-Robin Dispatch

Main thread maintains a counter, incremented on each task submission:

```
task arrives → counter % N → pick worker BQ → push
```

Simple, zero coordination overhead, distributes load uniformly.

---

## Code Impact

Minimal — no new classes, no structural changes. The existing `Blocking_Queue` and `Thread_Pool` implementations are reused as-is. Changes were isolated to dispatch logic in the reactor and wiring up per-worker write_ready_queue + eventfd.

---

## Next

See [`MultiReactor`] — eliminates centralized dispatch entirely. Each thread becomes its own reactor.