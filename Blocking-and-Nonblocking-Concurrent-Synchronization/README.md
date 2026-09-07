# Blocking-and-Nonblocking-Concurrent-Synchronization

Implementation and empirical evaluation of concurrent synchronization primitives across the progress-guarantee spectrum — from **blocking mutual exclusion** (locks) to **non-blocking snapshot algorithms** (obstruction-free / wait-free) — implemented in C++ using `std::thread`, `std::atomic`, and `std::mutex`.

## Motivation

When many threads share the same data, something has to stop them from stepping on each other. The usual fix is a lock: only one thread is allowed in at a time, everyone else waits. This works, but it has a real cost — if the thread holding the lock is slow, gets paused, or crashes, every other thread waiting on that lock is stuck too, and this gets worse as more threads are added.

This project looks at that trade-off from two sides. First, it implements and benchmarks two classic lock-based algorithms, Filter Lock and Bakery Lock, to see how badly performance drops as more threads compete for the same lock. Second, it moves away from locks entirely and implements non-blocking algorithms — obstruction-free and wait-free atomic snapshots — that let a thread read a consistent view of shared data without ever waiting on another thread to finish.

The aim is to actually measure this, not just describe it: run real code with a varying number of threads, record how long threads wait and how much work gets done, and see whether removing locks is genuinely worth the extra complexity.

## Structure

```
Blocking-and-Nonblocking-Concurrent-Synchronization/
├── locking-algorithms/            Blocking mutual exclusion
│   ├── Implementation/
│   │   ├── filter_lock.cpp
│   │   └── bakery_lock.cpp
│   ├── Input-Output/
│   │   ├── input_params.txt
│   │   ├── output-filter.txt
│   │   └── output-bakery.txt
│   ├── Analysis/
│   │   └── performance_graphs.ipynb
│   └── Report.pdf
└── wait-free-atomic-snapshot/     Non-blocking snapshot algorithms
    ├── Implementation/
    │   ├── obstruction_free_snapshot.cpp
    │   └── wait_free_snapshot.cpp
    ├── Input-Output/
    │   ├── input_params.txt
    │   ├── output-obstruction-free.txt
    │   └── output-wait-free.txt
    └── Report.pdf
```

Each source file resolves its input/output paths relative to `Implementation/` (`../Input-Output/...`), so build and run from inside that folder.

---

## 1. Locking Algorithms (`locking-algorithms/`)

Two classic mutual-exclusion algorithms, implemented and benchmarked against each other:

- **Filter Lock** — threads climb through *N−1* levels (N = thread count), each level eliminating at least one contender via a level/victim scheme, until only one thread reaches the critical section.
- **Bakery Lock** — each thread draws a ticket number and waits until it holds the lowest number among all threads currently wanting to enter, like a queue at a bakery counter.

Both algorithms guarantee **mutual exclusion**, **bounded waiting**, and are **deadlock-free** and **starvation-free** — the benchmarks compare them purely on *performance* under contention (throughput and lock-acquisition time), not correctness.

### Build & run

```bash
cd locking-algorithms/Implementation
g++ -std=c++17 -pthread -O2 -o filter filter_lock.cpp   && ./filter
g++ -std=c++17 -pthread -O2 -o bakery bakery_lock.cpp    && ./bakery
```

### Input (`Input-Output/input_params.txt`)

Whitespace-separated: `totalThreads  numberOfTimeInCS  lambda1  lambda2`

| Field | Meaning |
|---|---|
| `totalThreads` | number of concurrent threads competing for the lock |
| `numberOfTimeInCS` | number of times each thread re-enters the critical section |
| `lambda1` | rate parameter for the (exponential) delay spent *inside* the CS |
| `lambda2` | rate parameter for the (exponential) delay spent *outside* the CS |

### Output

`output-filter.txt` / `output-bakery.txt` log each thread's CS entry-request, entry, exit-request, and exit timestamps, followed by total execution time and measured throughput.

### Results

`Analysis/performance_graphs.ipynb` plots throughput and average/worst-case lock-acquisition time against both thread count and CS-iteration count (see the notebook for full write-up). Headline finding: **Filter Lock consistently outperforms Bakery Lock**, with the gap widening sharply under higher contention — e.g. at 64 threads, Bakery Lock's worst-case entry time (~7.9s) is roughly 11× Filter Lock's (~0.7s). This tracks the algorithms' designs: Bakery Lock's ticket scheme requires comparing against *every other thread* on each acquisition attempt (O(N) per lock), while Filter Lock's level/victim scheme does not carry the same cost. Full methodology and narrative in `Report.pdf`.

---

## 2. Wait-Free / Obstruction-Free Atomic Snapshot (`wait-free-atomic-snapshot/`)

Two non-blocking implementations of an **atomic snapshot** over an array of multi-reader/multi-writer (MRMW) registers — a classic building block from concurrent algorithm design (Herlihy & Shavit) for reading a consistent, linearizable view of shared state while writers keep updating it concurrently, without ever taking a lock:

- **Obstruction-Free** — a snapshot is guaranteed to complete only if the collecting thread eventually runs without interference from writers (no starvation-freedom guarantee).
- **Wait-Free** — every snapshot attempt is guaranteed to complete in a bounded number of steps regardless of other threads' progress, via the classic "collect twice, retry on change" technique with per-register move tracking.

Each register slot is a `StampedValue` (a value + monotonically increasing timestamp), letting a reader detect whether a slot changed between two collects without locking it.

### Build & run

```bash
cd wait-free-atomic-snapshot/Implementation
g++ -std=c++17 -pthread -O2 -o obs obstruction_free_snapshot.cpp   && ./obs
g++ -std=c++17 -pthread -O2 -o wfs wait_free_snapshot.cpp          && ./wfs
```

### Input (`Input-Output/input_params.txt`)

Whitespace-separated: `numberOfWriterThreads  numberOfReaderThreads  sizeOfRegister  waitTimeForNextWrite  waitTimeForNextRead  totalNumberOfSnapshots`

| Field | Meaning |
|---|---|
| `numberOfWriterThreads` | threads continuously writing random values to random register slots |
| `numberOfReaderThreads` | threads collecting snapshots |
| `sizeOfRegister` | number of register slots (MRMW array size) |
| `waitTimeForNextWrite` | delay between writes, in **microseconds** |
| `waitTimeForNextRead` | delay between snapshot attempts, in **microseconds** |
| `totalNumberOfSnapshots` | consistent snapshots a reader thread tries to collect before stopping |

The program runs for a fixed 10-second window (writers keep going for the full window regardless of `totalNumberOfSnapshots`).

### Output

`output-wait-free.txt` / `output-obstruction-free.txt` log every write (`Thr N's write of V on location L at <time>`) and every completed snapshot (`ThrN's snapshot: L1-.. L2-.. ... which finished at <time>`), interleaved as they occur.




