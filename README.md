 # ToyDB – DBS Project
 
 A simplified database storage engine with buffer management, variable-length record storage, and indexing.
 
 ## Overview
 
 ToyDB is a simplified educational database system containing the lower layers of a DBMS:
 
 - **PF Layer (Paged File Layer)** — Handles page-level storage and buffer management.
 - **Slotted Page Layer** — Stores variable-length records efficiently within fixed-size pages.
 - **AM Layer (Access Method Layer)** — Implements B+ Tree indexing over PF pages for efficient record lookup.
 
 This project extends the original ToyDB by adding:
 
 - Configurable buffer pool
 - LRU / MRU page replacement
 - Dirty-page tracking
 - Page-level I/O statistics
 - Slotted-page store for variable-length student records
 - Performance benchmarks
 - Index-building comparison: normal build, incremental inserts, and bulk-loading
 
 ## Directory Structure
 
 ```
 DB_Proj/
 │
 ├── pflayer/                # Paged File layer (PF)
 │   ├── pf.c, buf.c, hash.c
 │   ├── pftypes.h
 │   ├── benchpf.c           # Benchmark for PF stats
 │   ├── slotted.c           # Slotted-page implementation
 │   ├── slotted_bench.c     # Slotted store benchmark
 │   ├── testpf.c            # PF correctness & stress test
 │   └── Makefile
 │
 ├── amlayer/                # Access Method (AM) + B+ Tree
 │   ├── am.c
 │   ├── indexbench.c
 │   └── Makefile
 │
 └── data/
		 └── student.txt         # Dummy student dataset for Objective 2
 ```
 
 ## Project Objectives
 
 ### Objective 1 — Page Buffering in PF Layer
 
 Implementations include:
 
 - **Configurable buffer pool size**
 - **Two replacement policies:**
	 - LRU (default)
	 - MRU (optimized for sequential scans)
 - **Dirty page tracking**
	 - Explicit `PF_MarkDirty()` for updates
	 - Per-file replacement policy via `PF_SetReplPolicy()`
 - **Detailed statistics collection**
	 - Logical reads/writes
	 - Physical reads/writes
	 - Buffer hits/misses
 
 #### Benchmark Program: `benchpf.c`
 
 Produces CSV files for graph plotting:
 
 ```bash
 cd pflayer
 make
 # pool=20, ops=10k, writes=10%, LRU
 ./benchpf 20 10000 10 0 100 stats_lru.csv
 # same but MRU
 ./benchpf 20 10000 10 1 100 stats_mru.csv
 ```
 
 You can plot these statistics to compare read/write workloads.
 
 ### Objective 2 — Slotted-Page Storage for Variable-Length Records
 
 Implemented in: `slotted.c` + `slotted_bench.c`
 
 #### Features:
 
 - Insert variable-length student records
 - Delete records and reuse space
 - Page-directory layout with slot array
 - Sequential scan support
 - Space-utilization analysis
 - Comparison with static, fixed-size page layouts
 
 #### Run Benchmark:
 
 ```bash
 cd pflayer
 make
 ./slotted_bench
 ```
 
 #### Sample Output:
 
 ```
 Slotted: records=17814 pages=116 bytes=399768 util=0.8414
 Static(size=64): pages=279 util=0.9976
 Static(size=128): pages=557 util=0.9994
 Static(size=256): pages=1114 util=0.9994
 Static(size=512): pages=2227 util=0.9999
 Scan checked 100 records
 ```
 
 Shows slotted pages significantly reduce pages used vs static layouts.
 
 ### Objective 3 — B+ Tree Indexing over the Student File
 
 Implemented in: `amlayer/am.c` + `amlayer/indexbench.c`
 
 #### Three indexing methods demonstrated:
 
 1. **Build index on an existing file:**
		- Reads all student records and constructs the B+ tree in a single operation.
 
 2. **Incremental insert index:**
		- Start empty → insert entries one-by-one.
 
 3. **Bulk-loading (sorted data):**
		- If records are pre-sorted by key, use bulk build:
			- Minimizes page splits
			- Provides fastest index creation
			- Lowest I/O count
 
 #### Run Index Benchmarks:
 
 ```bash
 cd amlayer
 make
 ./indexbench
 ```
 
 #### Outputs comparisons of:
 
 - Build time
 - Number of pages accessed
 - Number of splits
 - Query completion time
 
 ## How to Build & Run
 
 ### Compile PF Layer
 
 ```bash
 cd pflayer
 make
 ```
 
 ### Run PF Correctness Test
 
 ```bash
 ./testpf
 ```
 
 ### Run PF Benchmark
 
 ```bash
 ./benchpf <pool> <ops> <write%> <policy> <pages> <outfile>
 ```
 
 ### Run Slotted-Page Benchmark
 
 ```bash
 ./slotted_bench
 ```
 
 ### Compile + Run AM Layer
 
 ```bash
 cd ../amlayer
 make
 ./indexbench
 ```
 
 ## Performance Metrics Collected
 
 ### PF Layer
 
 - Logical reads/writes
 - Physical reads/writes
 - Buffer hits/misses
 - Hit ratio
 - Replacement count
 - Dirty flush count
 
 ### Slotted Page Layer
 
 - Pages used
 - Bytes used
 - Utilization (bytes / pages × page_size)
 - Records inserted
 - Space comparison with static layouts
 
 ### AM Layer
 
 - Insert time
 - Bulk-build time
 - Height of B+ tree
 - Node splits
 - Query I/O count
 
 ## Data Format (`student.txt`)
 
 Each line contains semicolon-separated fields:
 
 ```
 Roll;RegNo;Name;Gender;...;DepartmentToken;...;
 ```
 
 `slotted_bench.c` extracts:
 
 - `roll_no`
 - `name`
 - `department token`
 - `UG/PG level` (derived)
 
 ## Configuration Options
 
 ### Set buffer pool size:
 
 ```c
 PF_SetBufferPoolSize(n);
 ```
 
 ### Choose replacement policy:
 
 ```c
 PF_SetReplPolicy(fd, PF_REPL_LRU);
 PF_SetReplPolicy(fd, PF_REPL_MRU);
 ```
 
 ### Limit records for benchmarking:
 
 ```bash
 export MAX_REC=10000
 ```
 
 ### Delete every k-th record:
 
 ```bash
 export DEL_EVERY=5
 ```
 
 ## Testing Summary
 
 - `testpf.c` validates PF correctness (alloc, dispose, dirty, scanning, error paths)
 - `benchpf.c` stresses buffer manager under mixed read/write
 - `slotted_bench.c` validates slotted page insert/delete/scan and utilization
 - `indexbench.c` tests three B+ tree building strategies
 
 **All tests pass successfully.**
 
 ## Conclusion
 
 This project demonstrates the essential storage-engine components of a DBMS:
 
 - Page-level buffer management with configurable pool and replacement strategies
 - Efficient variable-length record storage via slotted pages
 - B+ tree indexing with multiple build strategies and performance comparison
 
 It provides a complete, modular educational DBMS core suitable for learning or extension.