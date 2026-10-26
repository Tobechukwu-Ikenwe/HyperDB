# HyperDB workload measurements

Run `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`, `cmake --build build -j 4`, and `./build/hyperdb_workloads`. The suite measures one in-memory operation at a time, except `snapshot_save` and `snapshot_load`, which include file I/O and sync. Setup and fixture creation are outside the timed loops. SQL run through the local UI includes process startup and snapshot loading or saving.

These measurements were taken on a 13th Gen Intel Core i7-1355U with GCC 15.2.0, Release `-O3`, on 2026-10-26. The "before" binary came from `git archive HEAD` with only `benchmarks/workloads.cpp` and its CMake target copied in. The "after" binary contains the executor changes. Each value is the median milliseconds per operation from seven interleaved runs of each binary. CPU frequency, cache state, and filesystem activity can change the numbers; this is a local workload comparison, not a general SQLite comparison.

| Workload | Fixture | Before ms | After ms |
| --- | --- | ---: | ---: |
| Parse SELECT | One statement | 0.0020 | 0.0020 |
| Scan equality | 20,000 rows, match at end | 0.522 | 0.196 |
| Scan with LIMIT 10 | 20,000 matching rows | 1.064 | 0.0017 |
| ORDER BY with LIMIT 10 | 20,000 rows | 1.448 | 0.526 |
| GROUP BY / COUNT | 20,000 rows, 10 groups | 1.599 | 0.492 |
| Build equality index | 20,000 rows | 3.320 | 3.464 |
| Indexed equality | 20,000 rows | 0.0015 | 0.0013 |
| Inner join | 150 × 150 candidate pairs | 0.581 | 0.110 |
| Vector search | 2,000 vectors, 8 dimensions | 0.0334 | 0.0335 |
| Insert row | 5,000-row starting table | 0.00012 | 0.00012 |
| Update one row | About 5,100 rows | 0.503 | 0.496 |
| Delete missing row | About 5,100 rows | 0.279 | 0.113 |
| Snapshot save | About 5,100 rows | 1.513 | 1.517 |
| Snapshot load | About 5,100 rows | 1.787 | 1.817 |

## What changed and why

1. **Bind predicate columns once.** A `WHERE id = 7` expression previously normalized and looked up `id` in the schema for every row. The executor now resolves each column node once per SELECT and looks up its numeric position while scanning. Scalar evaluation also reads values by reference instead of copying them. This improved the full equality scan.
2. **Stop work when an unordered LIMIT is satisfied.** A query returning ten rows previously copied all 20,000 matching rows into a temporary table, then kept ten. Without `ORDER BY` or aggregation, the executor now projects rows directly into the result and stops after ten matches. With `ORDER BY`, it must still inspect all candidates to know which ten come first.
3. **Sort and group row references.** Ordered and aggregate SELECT paths now collect pointers to existing rows instead of copying every full row. Sorting reorders pointers; grouping uses those pointers. Only final output values are copied. The GROUP BY column position is resolved before its loop.
4. **Compare join keys before constructing joined rows.** The nested-loop join previously concatenated both rows for every candidate pair. It now reads the two join keys first and constructs a combined row only for a matching pair.
5. **Avoid copying rows for a no-op DELETE.** The DELETE path finds the first matching row before allocating a survivor table. If no row matches, it returns immediately. A DELETE that removes rows still uses `Database::replace_rows` for validation and index/embedding maintenance.

The unchanged or slightly slower index-build, vector, snapshot, and update measurements show where these changes did not help. In particular, each CLI write still reloads and rewrites a whole snapshot, and updates still stage replacement rows and rebuild indexes. Those are larger storage-design costs rather than costs in the SELECT loop.

For context, the repository's original equality-only comparison was also run at 20,000 rows and 30 iterations: `hyperdb_benchmark` reported 5.986 ms total for scans and 0.0417 ms for indexed queries; `sqlite_baseline.py` reported 11.609 ms and 0.059 ms. This is a single, narrow in-memory workload. The SQLite script uses bound parameters and a cached prepared statement, while HyperDB parses SQL on each call; the Python API boundary also differs. These numbers do not establish an overall ranking.
