# Portable HyperDB implementation progress

Updated: 2026-11-01. Source of truth for the portability program. Existing M0–M6 engineering status remains in [STATUS.md](../../STATUS.md). A stage is complete only after its stated acceptance checks pass.

| Stage | State | Evidence | Remaining acceptance work |
| --- | --- | --- | --- |
| 1. Audit | Complete | [Portability audit](PORTABILITY_AUDIT.md) covers repository inventory, subsystem states, allocation and platform risks, exact source locations, and a dependency-order plan. Native Linux Debug baseline passed 15/15 tests; 87 local audit links and line anchors validated. | Revisit findings as implementation reveals new evidence. |
| 2. Architecture | Complete | [Portable architecture](PORTABLE_ARCHITECTURE.md), [platform contract](PLATFORM_ABSTRACTION.md), [memory](MEMORY_ARCHITECTURE.md), [storage](STORAGE_ARCHITECTURE.md), [transaction](TRANSACTION_ARCHITECTURE.md), [RTOS integration](RTOS_INTEGRATION.md), [portability matrix](PORTABILITY_MATRIX.md), [performance gate](PERFORMANCE.md), and [ADR 0010](../decisions/0010-portable-core-contract.md) define the target and migration order. All 11 architecture/ADR files passed local-link validation. | Revisit contracts as tests expose gaps; proposed interfaces are not implemented yet. |
| 3. Foundation | In progress | `hyperdb_posix_file` and `hyperdb_snapshot_compat` build separately from `hyperdb_core`; Debug and Release each pass 17/17 tests, including C-compiled API coverage. The `single-threaded` preset builds and passes 17/17 tests; its core archive has no mutex lock symbols. Both in-memory embedding examples link without storage adapters and run. ADRs 0011–0013 and 0015 record boundaries and limits. | Move snapshot I/O behind caller-provided storage callbacks; remove `std::filesystem` declarations from portable interfaces; define/inject allocator and synchronization; add prepared statements. |
| 4. Storage | In progress | `DiskManager::open(std::unique_ptr<File>, ...)` accepts application storage without a path/filesystem. A test memory device initializes, allocates, closes, reopens, and validates a page. Debug and Release pass 17/17 tests. ADR 0014 records ownership and limitations. | Add storage geometry/capability reporting and fault cases for injected adapters; integrate pager/catalog/table/index state with SQL; add migration and recovery. |
| 5. Reliability | Not started | Legacy snapshot tests and page fault tests only. | Transaction protocol, recovery oracle, power-cut tests, concurrency contract. |
| 6. Portability | In progress | Linux Debug/Release and Linux single-threaded profile validated locally. Windows, embedded cross-build, restricted runtime, representative RTOS and hardware remain unvalidated. | Add runner jobs and complete no-filesystem, Windows, embedded cross-build, representative RTOS, and hardware validation. |
| 7. Optimization | Not started | Historical local in-memory workloads in `docs/BENCHMARKS.md`. | Reproducible baselines and before/after profiling on the new architecture. |
| 8. Final verification | Not started | None. | Full regression, portability, failure-injection, memory, and performance gates. |

## Current blockers and known defects

- P-01 through P-16 in [PORTABILITY_AUDIT.md](PORTABILITY_AUDIT.md) describe the migration risks and proposed tests. They are not yet resolved.
- Windows and bare-metal targets are unvalidated. POSIX snapshot I/O remains in `hyperdb_snapshot_compat`; the core still requires `std::mutex`, exceptions, and full STL, and has no caller-provided memory. The injected `File` path covers only the page prototype, not SQL persistence.
- The `HYPERDB_SINGLE_THREADED` build replaces the two core mutexes with no-op locks. The caller must ensure all shared handles are accessed from one thread; concurrent access in this profile is unsupported and unsafe.
- No WAL/recovery implementation exists. `WalDurability` is an interface used by buffer-pool tests.
- No memory peak has been measured for 32, 64, 128, or 256 KiB configurations. No such budget is claimed.
- The prior macOS CI run built but failed during CTest; the failed test is unidentified in local evidence.

## Next dependency-order step

Continue Stage 4 by defining storage capabilities and fault behavior, then integrate pages with the database model. In parallel, complete Stage 3 allocator, synchronization, and C statement contracts before adding no-filesystem/no-thread profiles.
