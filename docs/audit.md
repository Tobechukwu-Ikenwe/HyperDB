# Repository audit (2026-10-28)

## Baseline

`cmake --build build` had previously produced binaries; `ctest --test-dir build --output-on-failure` passed 10/10 tests on this host. The worktree was clean before this audit. The existing CMake build uses C++20, one monolithic `hyperdb_core` target, seven C++ test executables with `assert`, three Python integration tests, a CLI, a SQLite bridge, a vector demo, and two ad hoc benchmark executables. It does not yet use GoogleTest or Google Benchmark.

## Current behavior and target mapping

| Existing files | Working role | Target home |
| --- | --- | --- |
| `src/parser/*` | Lexer with source locations and recursive-descent subset parser | `src/sql/` |
| `src/database/TableData/*`, `src/database/database.*` | In-memory tables, schema, index ownership | `src/common/`, `src/catalog/`, `src/api/` |
| `src/database/persistence.cpp` | Versioned whole-file snapshot; temporary-file sync and rename | Compatibility path; new `src/storage/` page store |
| `src/database/Indexes/btree.*` | In-memory equality index | New `src/index/` persistent concurrent B+ tree |
| `src/QueryExecutor/*`, `src/filter*`, `src/select.cpp`, `src/from/*`, `src/OrderBy/*`, `src/Limit/*` | SQL subset execution | `src/execution/`, `src/planner/` |
| `src/main.cpp` | Single-statement CLI and snapshot modes | `src/shell/` |
| `src/bridge/*`, `python/*`, `hyperdb-text2sql/` | SQLite import and optional text-to-SQL experiment | Keep separate from engine core |
| `tests/*` | Seven C++ and three Python tests | Preserve, then reorganize by unit/integration/recovery/etc. |
| `benchmarks/*` | Manual workload timers and SQLite baseline | Preserve, add pinned Google Benchmark and reproducible harness |

## Gaps

The engine executes in memory and rewrites a full snapshot after CLI writes. There are no 8 KB slotted pages, checksums, free-space map, overflow pages, abstract file I/O, buffer pool, WAL, recovery, transactions, isolation levels, or multi-connection API. Indexes are not persistent page structures. SQL grammar, type checking, optimizer, memory budgeting, spill operators, and diagnostic coverage are far below the target. The snapshot path directly uses `std::fstream` and POSIX sync calls, and the CLI directly uses POSIX locking; both must eventually route through the file abstraction. Tests are fast but do not establish the requested durability, fuzzing, concurrency, coverage, or differential-testing thresholds. Current benchmark results cannot be used as evidence for the requested comparison protocol.

The snapshot file has an `HDBSNAP1` magic and is a legacy format. A new page format should use a different magic and an explicit migration/import path; silently interpreting legacy snapshots as page files would be unsafe.
