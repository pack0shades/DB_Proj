# ToyDB Project

A simplified educational database storage engine composed of a paged file layer with buffer management, a slotted-page storage layer for variable-length records, and a B+ tree access-method (AM) layer.

This repository implements:
- PF Layer: page-level storage, buffer manager, replacement policies, dirty tracking, statistics.
- Slotted Page Layer: variable-length record storage with slot array, reuse and scans.
- AM Layer: B+ tree indexing with incremental and bulk (sorted) build modes.
- Benchmarks and utilities for measuring performance and space utilization.

---

## Quick start

Build PF tests and benches:
- cd pflayer
- make tests benchpf slots

Run PF benchmark across mixes and policies (creates CSVs and plots):
- ./benchpf                # default LRU, runs several mixes
- python3 plot_pf_stats.py pf_combined.csv

Run slotted-page loader on dataset and inspect utilization:
- ./slotted_bench ../data/student.txt students.spf

Environment:
- MAX_REC=N limits loaded rows for quicker runs
- TOYDB_PF_BUFS=N sets buffer pool size

Build and run AM index benchmark:
- cd amlayer && make indexbench
- ./indexbench ../pflayer/students.spf student 0    # 0=incremental, 1=sorted

CSV output: set `CSV_OUT=../pflayer/index_stats.csv` and `CSV_HEADER=1` to write header.
Plot: `python3 amlayer/plot_index_stats.py ../pflayer/index_stats.csv index`

---

## Overview

ToyDB demonstrates core storage-engine components:

- PF Layer (Paged File Layer)
  - Page-level file access and buffer management.
  - Configurable buffer pool size and replacement policy (LRU, MRU).
  - Dirty-page tracking and explicit `PF_MarkDirty()`.
  - Per-file replacement policy via `PF_SetReplPolicy(fd, policy)`.
  - Runtime buffer sizing via `PF_SetBufferPoolSize(n)` or env `TOYDB_PF_BUFS`.
  - Detailed IO and buffer statistics (logical/physical reads/writes, hits/misses).
  - CSV writer and plotting helpers.

- Slotted Page Layer
  - Stores variable-length student records in fixed-size pages using a slot array.
  - Supports insert, delete (space reuse), and sequential scanning.
  - Space-utilization analysis vs static fixed-size layouts.

- AM Layer (Access Method / B+ Tree)
  - B+ tree index over PF pages with:
    - Build from file (scan then insert)
    - Incremental inserts
    - Bulk-loading (sorted input) for efficient construction
  - Benchmarks measure I/O, splits, height, and query performance.

---

## Directory Structure

DB_Proj/
- pflayer/                # Paged File layer (PF)
  - pf.c, buf.c, hash.c
  - pftypes.h
  - benchpf.c              # Benchmark for PF stats
  - slotted.c              # Slotted-page implementation
  - slotted_bench.c        # Slotted store benchmark
  - testpf.c               # PF correctness & stress test
  - Makefile
- amlayer/                # Access Method (AM) + B+ Tree
  - am.c
  - indexbench.c
  - Makefile
- data/
  - student.txt            # Dummy student dataset for benchmarks

---

## Project Objectives & Features

### Objective 1 — Page Buffering (PF Layer)
- Configurable buffer pool size.
- Replacement policies:
  - LRU (default)
  - MRU (useful for sequential access workloads)
- Per-file replacement policy with `PF_SetReplPolicy(fd, PF_REPL_LRU|PF_REPL_MRU)`.
- Explicit dirty marking via `PF_MarkDirty(fd, pageno)`.
- PF statistics:
  - Logical reads/writes
  - Physical reads/writes
  - Buffer hits/misses
  - Replacement counts
  - Dirty flush counts
- Benchmarks:
  - `benchpf.c` generates CSVs suitable for plotting and policy comparison.

Example:
- Run bench with pool=20, ops=10k, 10% writes:
  - `./benchpf 20 10000 10 0 100 stats_lru.csv`  # LRU
  - `./benchpf 20 10000 10 1 100 stats_mru.csv`  # MRU

### Objective 2 — Slotted-Page Storage
- Implemented in `slotted.c` and `slotted_bench.c`.
- Features:
  - Insert variable-length student records (roll_no, name, dept, level).
  - Delete records and reuse freed space.
  - Page directory with slot array; sequential scan API: `SP_ScanOpen/Next/Close`.
  - Space-utilization analysis compared to fixed-size pages.
- Example output (typical):
  - Slotted: records=17814 pages=116 bytes=399768 util=0.8414
  - Static(size=64): pages=279 util=0.9976
  - Scan checked 100 records

Run:
- `./slotted_bench ../data/student.txt students.spf`

### Objective 3 — B+ Tree Indexing (AM Layer)
- Implemented in `amlayer/am.c` and `amlayer/indexbench.c`.
- Index-building strategies:
  - Build from existing file (scan then insert).
  - Incremental insert (one-by-one).
  - Bulk-loading with pre-sorted keys (minimizes splits and IO).
- Bench outputs:
  - Build time, page accesses, node splits, query I/O.
- Run:
  - `cd amlayer && make`
  - `./indexbench ../pflayer/students.spf student 1`

Environment options:
- `MAX_REC=N` — cap records used from slotted file
- `POLICY=LRU|MRU` — set index file replacement policy
- `QNUM=N` — number of random point queries (default 100)
- `RNUM=N` — number of range queries (default 50)
- `RANGEPCT=P` — percent of key domain used for range queries (default 10)

---

## How to Build & Run

Compile PF layer:
- cd pflayer
- make

Run PF correctness test:
- ./testpf

Run PF benchmark:
- ./benchpf <pool> <ops> <write%> <policy> <pages> <outfile>

Run slotted-page benchmark:
- ./slotted_bench

Compile & run AM layer:
- cd amlayer
- make
- ./indexbench

---

## Performance Metrics Collected

PF Layer:
- Logical reads/writes
- Physical reads/writes
- Buffer hits/misses
- Hit ratio
- Replacement count
- Dirty flush count

Slotted Page Layer:
- Pages used
- Bytes used
- Utilization (bytes / pages × page_size)
- Records inserted
- Comparison vs static layouts

AM Layer:
- Insert time
- Bulk-build time
- B+ tree height
- Node splits
- Query I/O count

---

## Data Format (student.txt)

Each line contains semicolon-separated fields (sample):
Roll;RegNo;Name;Gender;...;DepartmentToken;...;

`slotted_bench.c` extracts:
- roll_no (int)
- name
- department token
- UG/PG level (derived)

---

## Configuration & Environment

- Set buffer pool size:
  - `PF_SetBufferPoolSize(n)` or `TOYDB_PF_BUFS=n`
- Set replacement policy:
  - `PF_SetReplPolicy(fd, PF_REPL_LRU)` or `PF_SetReplPolicy(fd, PF_REPL_MRU)`
- Limit records during benchmarks:
  - `export MAX_REC=10000`
- Delete every k-th record in slotted bench:
  - `export DEL_EVERY=5`

---

## Testing Summary

- `testpf.c` validates PF correctness (allocation, dispose, dirty, scanning, error paths).
- `benchpf.c` stresses buffer manager under mixed read/write workloads.
- `slotted_bench.c` validates slotted insert/delete/scan and utilization.
- `indexbench.c` compares three B+ tree build strategies.
- All tests pass successfully in the development environment.

---

## Conclusion

ToyDB demonstrates essential DBMS storage-engine components:
- Configurable page-level buffer management with multiple replacement strategies.
- Efficient variable-length record storage via slotted pages.
- B+ tree indexing with incremental and bulk-loading strategies.
- Benchmarks and metrics for performance comparison and analysis.

This repository is suitable for learning, experiments, and further extension.