# ToyDB Project

This repo contains a Paged File (PF) layer with a buffer manager and a slotted-page storage layer (SP) for variable-length records. Additions include:

- Buffer pool with selectable replacement policies:
  - LRU (default) and MRU (good for sequential access)
  - Set per-file via `PF_SetReplPolicy(fd, policy)`
- Runtime-configurable buffer pool size via `PF_SetBufferPoolSize(n)` or env `TOYDB_PF_BUFS`.
- Explicit dirty marking `PF_MarkDirty(fd, pageno)`.
- PF statistics: logical/physical IO and buffer hits/misses; CSV writer and plotting.
- Benchmarks: `benchpf` (PF cache) and `slotted_bench` (slotted pages with student data).
- AM index benchmark: `amlayer/indexbench` builds a B+ tree on student roll_no and reports PF stats for two build modes (incremental vs sorted) and simple queries.

Quick start

- Build PF tests and benches:
  - cd pflayer
  - make tests benchpf slots

- Run PF benchmark across mixes and policies (creates CSVs and plots):
  - ./benchpf                # default LRU, runs several mixes
  - python3 plot_pf_stats.py pf_combined.csv

- Run slotted-page loader on dataset and see utilization:
  - ./slotted_bench ../data/student.txt students.spf
  - Environment:
    - MAX_REC=N limits loaded rows for quick runs
    - TOYDB_PF_BUFS=N sets buffer pool size

- Build and run AM index benchmark:
  - cd amlayer && make indexbench
  - ./indexbench ../pflayer/students.spf student 0    # 0=incremental, 1=sorted
  - CSV output: set CSV_OUT=../pflayer/index_stats.csv and CSV_HEADER=1 to write header
  - Example:
    - CSV_OUT=../pflayer/index_stats.csv CSV_HEADER=1 ./indexbench ../pflayer/students.spf student 1
    - CSV_OUT=../pflayer/index_stats.csv ./indexbench ../pflayer/students.spf student 0
  - Options (env):
    - MAX_REC=N to cap records used from slotted file
    - POLICY=LRU|MRU to set index file replacement policy
    - QNUM=N number of random point queries (default 100)
    - RNUM=N number of range queries (default 50)
    - RANGEPCT=P percent of key domain for each range (default 10)
  - Plot: python3 amlayer/plot_index_stats.py ../pflayer/index_stats.csv index

Notes

- Slotted-page records serialize fields: roll_no (int32), name, dept, level.
- Scan API: `SP_ScanOpen/Next/Close` to iterate records.
- See `pflayer/IMP.DOC` for PF API semantics and on-disk format.
