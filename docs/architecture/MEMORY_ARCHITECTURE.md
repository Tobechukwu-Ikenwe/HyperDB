# Memory architecture and budgets

Status: design. No 32, 64, 128, or 256 KiB deployment profile has been measured or validated. The current allocation inventory is in [PORTABILITY_AUDIT.md](PORTABILITY_AUDIT.md#4-memory-allocations-and-ownership).

## Ownership and policies

The caller owns the allocator or static region and must keep it alive until all database handles close. A database owns its catalog, cache, active connections, and transaction state. A prepared statement owns its AST and operator state; row views returned by `step` remain valid only until the next step/reset/finalize. A page guard owns a pin and releases it on destruction. These lifetimes must be enforced by tests and the C ABI.

The first implementation should support the system allocator and a caller allocator with a byte budget. A fixed arena can be supplied through the same callback contract; it may return exhaustion when full. Scratch arenas may reset at statement completion, while persistent catalog/cache allocations require individual deallocation. Do not replace every STL container merely for appearance: use measured allocation profiles and an allocator-aware container path where the embedded toolchain supports it. Toolchains lacking required C++20 runtime features need an explicit unsupported configuration until a tested alternative exists.

| Budget | Current behavior | Target bound and failure |
| --- | --- | --- |
| SQL input | Arbitrary `std::string`; token text copied | `max_sql_bytes`, `max_tokens`, `max_ast_nodes`, `max_expression_depth`; reject before deep recursion or excess allocation. |
| Database cache | `capacity × Page` plus copies; fixed 8192-byte page | `cache_bytes` and pin count; no eviction of pinned pages; return `RESOURCE_EXHAUSTED` when all frames are pinned. |
| Table storage | All rows live in `std::vector<Row>` | Page-backed table with bounded row/page cursor; maximum record size and overflow policy versioned in storage format. |
| Result | Whole `QueryResult` materialized | `step` yields one row view; optional collect helper has a caller-specified row/byte limit. |
| Sort/group/join | Intermediates proportional to rows or join pairs | Operator memory budget; spill through temporary storage when supported, otherwise explicit resource error without semantic change. |
| Index/search | B-tree nodes and vector candidates grow with rows | Page-backed equality index; vector result heap bounded by requested limit, with exact scoring semantics. |
| Transaction | No transaction workspace | Explicit dirty-page, log, and savepoint limits; fail before commit if capacity cannot be reserved. |

Peak-memory acceptance measures allocator high-water mark and stack use for representative SQL, paging, recovery, and failure paths. Run each workload at configured budgets of 256, 128, 64, and 32 KiB; record pass, explicit resource failure, or unsupported toolchain separately. An allocation failure must never become `std::terminate`, an inconsistent schema, or a partial durable commit. No budget tier is supported until its measured peak and workload suite fit.

## Current hazards to remove first

`Status` messages allocate inside `noexcept` paths; parser/SQL APIs throw; `Schema::add_column` can leave its lookup map ahead of its column vector on allocation failure; B-tree insertion and staged UPDATE/DELETE can allocate after mutating state. These require fail-after-N regression tests before allocator migration. The current `BufferPool` frame allocation catches `bad_alloc` but not every allocation-related exception, and its page-by-value API increases transient stack/heap use.
