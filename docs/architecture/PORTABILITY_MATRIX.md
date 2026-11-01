# Portability and feature matrix

Updated: 2026-11-01. `Verified` means a runnable target executed the listed tests. `Planned` is not support. Resource exhaustion must be explicit and must not alter SQL semantics. See [audit](PORTABILITY_AUDIT.md).

| Target/profile | Build/runtime evidence | SQL and storage today | Memory/dependencies | Status |
| --- | --- | --- | --- | --- |
| Linux native C++ and C API | Local GCC Debug 16/16 CTest targets on 2026-11-01; earlier CI Linux GCC/Clang jobs passed on older commit | Documented SQL subset and `HDBSNAP1` snapshots work; page path separate; no transaction recovery. C API is in-memory and single-threaded per handle. | Full C++20 STL, exceptions, system heap, filesystem, POSIX; no measured peak or allocator injection | Partially verified |
| macOS native C++ | Earlier CI built; CTest failed with unknown test | Same intended code path; behavior not fully verified | Full C++20 STL, exceptions, heap, POSIX-like services | Blocked by test failure |
| Windows native | No build or run | POSIX file source is compiled unconditionally; CLI lock unavailable | Windows adapter absent | Unsupported |
| Embedded Linux | No cross-build or device test | Linux-like adapter may be possible; no resource guarantee | Full runtime currently needed | Planned |
| 256/128/64/32 KiB memory tiers | No peak-memory measurement or budget tests | Whole in-memory table/result model conflicts with fixed budgets | No caller allocator or global budget | Unsupported |
| FreeRTOS / Zephyr / other RTOS | No representative adapter or target test | No portable no-thread/no-filesystem core | `std::mutex`, filesystem, exceptions currently required | Unsupported |
| Bare metal | No target build or run | No storage/memory service injection across full engine | Full STL/heap/exceptions currently required | Unsupported |
| Raw flash device | No adapter or power-cut test | Current byte-addressable file assumptions do not describe erase/program rules | Needs application driver/FTL and verified barrier | Unsupported |

## SQL feature compatibility target

The following features exist on the current Linux in-memory engine and must retain identical semantics in every future supported profile. A profile may reject a query because a documented resource limit is exceeded, but it may not silently omit a clause or change NULL/type behavior.

| Feature | Current state | Future cross-profile contract |
| --- | --- | --- |
| NULL, integer, real, text, blob | Implemented in `Value`; declared type affinity is metadata | Preserve value and comparison behavior; document unsupported coercions separately. |
| Filtering, projection, `LIMIT` | Implemented subset | Same predicate truth table and output columns; bounded iterator execution. |
| One-column sort/group and aggregates | Implemented subset | Same ordering, NULL aggregation, and `HAVING` semantics; spill or explicit memory error. |
| Two-table inner join | Implemented subset | Same name resolution and matched rows; bounded execution. |
| Equality index | In-memory B-tree | Index choice must not change results or type errors. |
| Exact vector search | Implemented over supplied embeddings | Same similarity/order/filter semantics where enabled; resource failure explicit. |
| Transactions, rollback, recovery | Missing | Add a shared transaction contract; mark unsupported until tested on each storage backend. |
| SQL aliases, outer joins, range index scans, general expressions | Missing in all profiles | Product feature gaps, not platform-specific exclusions. |

Next validation evidence must include toolchain version, target board/OS, adapter, test command, passed/failed targets, memory high-water mark, and durability test conditions. Compiling a binary is not a durability or hardware-support result.
