# HyperDB optimization history

This is a record of changes made to reduce work in the existing in-memory engine. It separates measured results from changes that have not been benchmarked. The page-store modules are not yet connected to SQL execution, so their presence is not a measured SQL speedup.

## 2026-10-20: Equality index for simple predicates

Completed the in-memory B-tree and made the executor use it for a simple indexed equality term. The executor still checks the full predicate after obtaining candidate row IDs. Split, duplicate-key, and index-update tests were added. See [development log](docs/DEVELOPMENT_LOG.md) and [index decision](docs/DESIGN_DECISIONS.md).

The recorded Release workload used 100,000 rows and 200 lookups, excluding setup:

| HyperDB path | Total time for 200 lookups |
| --- | ---: |
| Scan | 520.982 ms |
| Indexed equality | 0.428201 ms |

These compare two query paths on one workload; they do not measure index build cost, writes, or a general database speedup. The accompanying Python/SQLite baseline used a different API boundary and cached prepared statements, so it is not a fair overall ranking.

## 2026-10-26: Reduce work in SELECT and DELETE

Commit `367d3e8` changed five execution paths. The local before/after measurements below are medians per operation from seven interleaved Release `-O3` runs on a 13th Gen Intel Core i7-1355U with GCC 15.2.0. Fixtures were created outside the timed loops. The full workload definitions and methodology are in [BENCHMARKS.md](docs/BENCHMARKS.md).

| Change | Why it helps | Workload | Before | After |
| --- | --- | --- | ---: | ---: |
| Bind predicate columns once and read scalar values by reference | Avoid name normalization, schema lookup, and value copying for every scanned row | Equality scan, 20,000 rows | 0.522 ms | 0.196 ms |
| Stop after an unordered `LIMIT` is filled | Avoid scanning and copying rows that cannot enter the result | `LIMIT 10`, 20,000 matching rows | 1.064 ms | 0.0017 ms |
| Sort row references and resolve the group key once | Avoid copying every row before ordering or grouping | `ORDER BY ... LIMIT 10`, 20,000 rows | 1.448 ms | 0.526 ms |
| Group row references | Avoid copying source rows into the grouping path | `GROUP BY`/`COUNT`, 20,000 rows | 1.599 ms | 0.492 ms |
| Compare join keys before combining rows | Construct joined rows only for matching pairs | Inner join, 150 × 150 candidate pairs | 0.581 ms | 0.110 ms |
| Find a matching row before staging a DELETE | Skip replacement-table allocation when nothing is deleted | Missing-row DELETE, about 5,100 rows | 0.279 ms | 0.113 ms |

The same run also measured paths without a clear improvement: SELECT parsing was 0.0020 → 0.0020 ms, indexed equality 0.0015 → 0.0013 ms, equality-index construction 3.320 → 3.464 ms, vector search 0.0334 → 0.0335 ms, row insertion 0.00012 → 0.00012 ms, one-row UPDATE 0.503 → 0.496 ms, snapshot save 1.513 → 1.517 ms, and snapshot load 1.787 → 1.817 ms. Those numbers are retained so the history does not imply a universal speedup.

## Changes without a recorded speedup

- Zero-match UPDATE and DELETE avoid rewriting the snapshot; their behavior is tested, but the change has no before/after performance measurement. See [development log](docs/DEVELOPMENT_LOG.md).
- The fixed-page disk manager and bounded buffer pool add storage structure and a WAL-before-data flush boundary. They are tested but not connected to SQL, and no throughput or latency improvement is claimed. See [storage limitations](docs/limitations.md).
- The Google Benchmark file-I/O target is a measurement scaffold. No result from it has been used to claim a speedup. See [benchmark evidence](docs/benchmarks.md).

## Reproduction limits

The historical comparison records the machine, compiler, build mode, workloads, and timing method, but does not retain raw benchmark JSON or an unambiguous pre-change commit for the archived before binary. Treat its timings as exploratory evidence. A future speedup claim should record exact before/after commits, commands, raw output, machine and compiler details, and profiling evidence in [docs/benchmarks.md](docs/benchmarks.md).
