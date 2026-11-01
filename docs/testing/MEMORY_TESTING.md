# Memory allocator testing

Status: HyperMalloc's existing C test suite passed 13/13 at inspected commit
`3258efd0742351a1045d51442ab5d10bee3d578c`. HyperDB Debug and optional
HyperMalloc configurations each pass 19/19 CTest cases locally. Allocator
tests cover the same basic create/insert/index/select workload with system,
application callbacks, and HyperMalloc when enabled, plus accounting, cleanup,
and a budget failure during table creation. The GitHub workflow has been added
but has not yet run remotely. The broader matrix below remains incomplete.

## Required allocator matrix

| Profile | Required checks |
| --- | --- |
| System allocator | Existing SQL regression/integration suite; allocator counters and normal cleanup. |
| Application callbacks | Same SQL workload/result oracle; assert allocate/deallocate balance, alignment, callback context, and no cross-backend frees. |
| HyperMalloc | Linux optional package build; same SQL workload/result oracle; check allocator invariants after cleanup and multiple database handles. |

Every allocator profile should exercise CREATE/INSERT/UPDATE/DELETE/SELECT,
NULL and strings/blobs, equality index build/lookup/update, page-buffer reads
and flushes, simultaneous independent database lifetimes, query result
lifetime, and database destruction. Results must match the system-backend
oracle.

## Failure injection and budgets

A deterministic application allocator must fail after each allocation index
across table creation, row insertion, index construction/update, parser/AST
construction, query execution, C API result construction, and cleanup. Verify
that the failure is reported, allocations are freed by the correct callback,
and preexisting database state remains valid. Once transaction support exists,
also verify rollback after every allocation failure during a transaction;
today HyperDB has no transaction API, so transaction rollback coverage is
unsupported and must not be marked as skipped success.

Run finite budgets below and above observed workload peaks. Assert that tracked
current/peak bytes and allocation/failure counts match the callback ledger.
Report raw peak memory separately from tracked allocator bytes. A workload
does not establish a supported 32/64/128/256 KiB tier unless process/stack use,
all core allocations, and all failure paths have been measured at that limit.

## Sanitizers and CI

Run allocator ownership and failure tests in the existing ASan, UBSan, and
TSan profiles where the backend is supported. HyperMalloc itself requires
Linux/pthreads and uses mmap; do not include it in Windows or no-thread builds.
HyperMalloc's ASan interaction must be verified because its internal mmap
allocator is not itself instrumented like the system allocator. Benchmarks
must run equivalent HyperDB workloads with identical SQL and data, record
compiler/runner/backend and raw results, and compare latency and memory
accounting. Do not infer allocator speed from HyperMalloc's standalone tests.

GitHub workflow status names and actual completed runs belong in
[`docs/CI_CD.md`](../CI_CD.md); do not mark an allocator workflow operational
based only on YAML parsing or a successful configure step.
