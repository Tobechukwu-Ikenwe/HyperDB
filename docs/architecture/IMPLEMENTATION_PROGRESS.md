# Portable HyperDB implementation progress

Updated: 2026-11-01. Source of truth for the portability program. Existing M0–M6 engineering status remains in [STATUS.md](../../STATUS.md). A stage is complete only after its stated acceptance checks pass.

| Stage | State | Evidence | Remaining acceptance work |
| --- | --- | --- | --- |
| 1. Audit | Complete | [Portability audit](PORTABILITY_AUDIT.md) covers repository inventory, subsystem states, allocation and platform risks, exact source locations, and a dependency-order plan. Native Linux Debug baseline passed 15/15 tests; 87 local audit links and line anchors validated. | Revisit findings as implementation reveals new evidence. |
| 2. Architecture | Complete | [Portable architecture](PORTABLE_ARCHITECTURE.md), [platform contract](PLATFORM_ABSTRACTION.md), [memory](MEMORY_ARCHITECTURE.md), [storage](STORAGE_ARCHITECTURE.md), [transaction](TRANSACTION_ARCHITECTURE.md), [RTOS integration](RTOS_INTEGRATION.md), [portability matrix](PORTABILITY_MATRIX.md), [performance gate](PERFORMANCE.md), and [ADR 0010](../decisions/0010-portable-core-contract.md) define the target and migration order. All 11 architecture/ADR files passed local-link validation. | Revisit contracts as tests expose gaps; proposed interfaces are not implemented yet. |
| 3. Foundation | In progress | `hyperdb_posix_file` and `hyperdb_snapshot_compat` build separately from `hyperdb_core`; the single-threaded profile and C/C++ embedding examples were validated earlier. ADR 0016 and the allocator docs record an additive system/application/optional-HyperMalloc PMR resource, C API selection/stats, and partial routing for registry and B-tree node containers. Debug and optional HyperMalloc configurations each pass 19/19 tests locally; the HyperMalloc workflow is added but not yet remotely run. | Route parser, table/value storage, query scratch/results, vector-index internals, API-owned results, and persistence allocations; add broad allocation-failure tests; complete synchronization and prepared statement contracts. |
| 4. Storage | In progress | `DiskManager::open(std::unique_ptr<File>, ...)` accepts application storage without a path/filesystem. A test memory device initializes, allocates, closes, reopens, and validates a page. Debug and Release pass 17/17 tests. ADR 0014 records ownership and limitations. | Add storage geometry/capability reporting and fault cases for injected adapters; integrate pager/catalog/table/index state with SQL; add migration and recovery. |
| 5. Reliability | Not started | Legacy snapshot tests and page fault tests only. | Transaction protocol, recovery oracle, power-cut tests, concurrency contract. |
| 6. Portability | In progress | Linux Debug/Release and Linux single-threaded profile validated locally. GitHub run [41296112034](https://github.com/Tobechukwu-Ikenwe/HyperDB/actions/runs/41296112034) passed Windows shared-core/C API/injected-storage tests and AArch64 embedded-Linux core cross-compilation. AArch64 was compile-only. | Complete no-filesystem, restricted runtime, representative RTOS, and hardware validation. |
| 7. Optimization | Not started | Historical local in-memory workloads in `docs/BENCHMARKS.md`. | Reproducible baselines and before/after profiling on the new architecture. |
| 8. Final verification | Not started | None. | Full regression, portability, failure-injection, memory, and performance gates. |

## Current blockers and known defects

- P-01 through P-16 in [PORTABILITY_AUDIT.md](PORTABILITY_AUDIT.md) describe the migration risks and proposed tests. They are not yet resolved.
- Windows and bare-metal targets are unvalidated. POSIX snapshot I/O remains in `hyperdb_snapshot_compat`; the core still requires `std::mutex`, exceptions, and full STL, and has no caller-provided memory. The injected `File` path covers only the page prototype, not SQL persistence.
- The `HYPERDB_SINGLE_THREADED` build replaces the two core mutexes with no-op locks. The caller must ensure all shared handles are accessed from one thread; concurrent access in this profile is unsupported and unsafe.
- No WAL/recovery implementation exists. `WalDurability` is an interface used by buffer-pool tests.
- Allocator counters cover only routed allocations. No total memory peak has been measured for 32, 64, 128, or 256 KiB configurations. No such capacity claim is made.
- The prior macOS CI run built but failed during CTest; the failed test is unidentified in local evidence.
- New GitHub workflows, release protection, and benchmark threshold behavior have not yet been validated by an actual run. See [CI/CD runbook](../CI_CD.md).

## Next dependency-order step

Continue Stage 4 by defining storage capabilities and fault behavior, then integrate pages with the database model. Continue Stage 3 by routing remaining database/query allocations, adding systematic failure injection, and completing synchronization and prepared-statement contracts before adding no-filesystem/no-thread profiles.
