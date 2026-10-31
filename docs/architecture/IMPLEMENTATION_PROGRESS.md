# Portable HyperDB implementation progress

Updated: 2026-10-31. Source of truth for the portability program. Existing M0–M6 engineering status remains in [STATUS.md](../../STATUS.md). A stage is complete only after its stated acceptance checks pass.

| Stage | State | Evidence | Remaining acceptance work |
| --- | --- | --- | --- |
| 1. Audit | Complete | [Portability audit](PORTABILITY_AUDIT.md) covers repository inventory, subsystem states, allocation and platform risks, exact source locations, and a dependency-order plan. Native Linux Debug baseline passed 15/15 tests; 87 local audit links and line anchors validated. | Revisit findings as implementation reveals new evidence. |
| 2. Architecture | Complete | [Portable architecture](PORTABLE_ARCHITECTURE.md), [platform contract](PLATFORM_ABSTRACTION.md), [memory](MEMORY_ARCHITECTURE.md), [storage](STORAGE_ARCHITECTURE.md), [transaction](TRANSACTION_ARCHITECTURE.md), [RTOS integration](RTOS_INTEGRATION.md), [portability matrix](PORTABILITY_MATRIX.md), [performance gate](PERFORMANCE.md), and [ADR 0010](../decisions/0010-portable-core-contract.md) define the target and migration order. All 11 architecture/ADR files passed local-link validation. | Revisit contracts as tests expose gaps; proposed interfaces are not implemented yet. |
| 3. Foundation | Not started | Existing C++ API, POSIX file adapter, and status type are partial starting points. | One core/adapters build split, portable storage/memory/sync contracts, stable C API, C/C++ embedding examples and tests. |
| 4. Storage | Not started | Checksummed fixed page codec and disk manager tested separately from SQL. | Pager lifecycle, page-backed heap/catalog/indexes, migration, adapter-backed persistence. |
| 5. Reliability | Not started | Legacy snapshot tests and page fault tests only. | Transaction protocol, recovery oracle, power-cut tests, concurrency contract. |
| 6. Portability | Not started | Linux native builds only in the current session. | Windows, embedded cross-build, restricted runtime, representative RTOS and hardware validation. |
| 7. Optimization | Not started | Historical local in-memory workloads in `docs/BENCHMARKS.md`. | Reproducible baselines and before/after profiling on the new architecture. |
| 8. Final verification | Not started | None. | Full regression, portability, failure-injection, memory, and performance gates. |

## Current blockers and known defects

- P-01 through P-16 in [PORTABILITY_AUDIT.md](PORTABILITY_AUDIT.md) describe the migration risks and proposed tests. They are not yet resolved.
- Windows and bare-metal targets are unvalidated; the POSIX adapter is currently compiled into `hyperdb_core`.
- No WAL/recovery implementation exists. `WalDurability` is an interface used by buffer-pool tests.
- No memory peak has been measured for 32, 64, 128, or 256 KiB configurations. No such budget is claimed.
- The prior macOS CI run built but failed during CTest; the failed test is unidentified in local evidence.

## Next dependency-order step

Start Stage 3 with a tested build split between the common database target and POSIX adapters while keeping native Linux SQL and snapshot tests green. Then introduce the narrow logical storage and memory interfaces and embedding API.
