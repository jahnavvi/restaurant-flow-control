# Restaurant Flow Control

A concurrent producer-consumer simulation in C, modeling a restaurant seating system using POSIX threads, mutexes, condition variables, and semaphores.

## Overview

Two producer threads generate seating requests (General Table and VIP Room) and insert them into a bounded shared queue. Two consumer threads (T-X and Rev-9 robots) remove and process requests concurrently. The simulation enforces a VIP request cap, a total queue capacity limit, and a global request count, then prints a final consumption report.

## Demo

```
Request queue: 0 GT + 0 VIP = 0. Added General table request. Produced: 1 GT + 0 VIP = 1 in 0.001 s.
Request queue: 1 GT + 0 VIP = 1. T-X consumed General table request.  T-X totals: 1 GT + 0 VIP = 1 consumed in 0.002 s.
...

REQUEST REPORT
----------------------------------------
General table request producer generated 88 requests
VIP room request producer generated 22 requests
T-X consumed 44 GT + 11 VIP = 55 total
REV-9 consumed 44 GT + 11 VIP = 55 total
Elapsed time 0.014 s
```

## Technical Highlights

- **Bounded circular buffer** with configurable capacity (default: 20 slots)
- **VIP cap enforcement** — no more than 6 VIP requests in the queue at once, enforced via `pthread_cond_wait` on `not_full`
- **Semaphore barrier** — main thread blocks on `sem_wait` until all requests are consumed, avoiding a busy-wait join
- **Graceful shutdown** — `mark_producer_finished()` decrements a producer counter; when it hits zero, consumers are woken via `pthread_cond_broadcast` and drain the remaining queue before exiting
- **Thread-safe logging** — a dedicated semaphore (`sem_print`) serializes all console output

## Architecture

```
main.c
├── Spawns 2 producer threads (GeneralTable, VIPRoom)
├── Spawns 2 consumer threads (T-X, Rev-9)
├── Waits on barrier semaphore until queue is drained
└── Prints final report via output_production_history()

request_queue.c        # Core queue: init, add, remove, destroy, producer-done signaling
producers.c            # Producer thread: generates requests until total_limit reached
consumers.c            # Consumer thread: removes requests until producers done + queue empty
log.c                  # Thread-safe logging with timestamps and queue state snapshots
seating.h              # Enums: RequestType (GeneralTable/VIPRoom), ConsumerType (TX/Rev9)
```

## Build & Run

Requirements: GCC, POSIX threads (`-lpthread`)

```bash
make
./dineseating [-s total_requests] [-x tx_ms] [-r rev9_ms] [-g general_ms] [-v vip_ms]
```

| Flag | Description | Default |
|------|-------------|---------|
| `-s N` | Total requests to generate | 110 |
| `-x N` | T-X processing delay (ms) | 0 |
| `-r N` | Rev-9 processing delay (ms) | 0 |
| `-g N` | General producer delay (ms) | 0 |
| `-v N` | VIP producer delay (ms) | 0 |

Example:

```bash
./dineseating -s 50 -x 5 -r 5
```

## Synchronization Design

| Primitive | Purpose |
|-----------|---------|
| `pthread_mutex_t mutex` | Guards all queue state |
| `pthread_cond_t not_full` | Producers block when queue full or VIP cap hit |
| `pthread_cond_t not_empty` | Consumers block when queue empty |
| `sem_t barrier` | Main thread waits until last request consumed |
| `sem_t sem_print` | Serializes log output across threads |

## Files

| File | Description |
|------|-------------|
| `main.c` | Entry point, thread creation, argument parsing |
| `request_queue.c` / `.h` | Bounded queue with full sync primitive management |
| `producers.c` / `.h` | Producer thread logic |
| `consumers.c` / `.h` | Consumer thread logic |
| `log.c` / `.h` | Thread-safe timestamped logging |
| `seating.h` | Shared enums for request and consumer types |
| `Makefile` | Build configuration |
