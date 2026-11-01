# HyperDB memory management

Status: the allocator interface, C++ selection, additive C selection, optional
HyperMalloc adapter, accounting, and a partial set of PMR-backed database/index
containers are implemented. They are not a whole-engine allocation budget.
See the
[allocation audit](ALLOCATOR_INTEGRATION.md) and [ADR 0016](../decisions/0016-pluggable-memory-resource.md).

## Intended configuration model

Each database selects a HyperDB memory resource at initialization. The default
is the C++ system allocator. An application may instead provide aligned
allocate/deallocate callbacks plus an opaque context. A build may opt into the
HyperMalloc backend when its installed CMake package is available on supported
Linux 64-bit targets. The engine core depends on the resource interface, not on
HyperMalloc headers or symbols.

The configured budget limits requested bytes routed through that resource.
Statistics include current and peak requested bytes, successful allocations,
deallocations, failures, and the configured budget. A finite budget reports
remaining tracked budget as `budget - current`; an unlimited budget or
HyperMalloc's internal available capacity is unknown. These values do not
include system STL allocations, allocator metadata, stack use, process memory,
or unrelated libraries unless those paths are migrated through the resource.

Allocation exhaustion is an explicit `std::bad_alloc` in C++ and
`HDB_OUT_OF_MEMORY` at the C API. Callers must destroy partial objects with
their creating allocator. Application callbacks must obey the documented
alignment and matching-deallocation contract and remain alive until every
database/result using them is destroyed.

## Backend properties

| Backend | Platform / threading | Memory source | Capacity and statistics |
| --- | --- | --- | --- |
| System | Platforms supported by the C++ runtime | `std::pmr::new_delete_resource` | Per-database accounting for routed requests; no system free-capacity query. |
| Application callbacks | Application-defined | Caller-controlled pool, arena, heap, or device memory | HyperDB tracks routed requests; actual capacity depends on callback contract. |
| HyperMalloc | Linux 64-bit, mmap, pthreads | HyperMalloc process-global arenas | HyperDB wrapper can track each database's routed requests; HyperMalloc stats/capacity are process-wide and cannot be attributed to one database. No caller-region initialization exists. |

HyperMalloc is optional. Its `hm_malloc`/`hm_free` pairs support ordinary
alignment; higher alignments use `hm_memalign`/`hm_free`. HyperDB does not use
its preload library or replace global `new`.

## Current allocation coverage

The outer table/index/vector-index registries and B-tree node/entry/child
containers use the selected resource. This is verified through SQL workloads
with the system and application callbacks, and with HyperMalloc when enabled.
Nested index maps also use the resource. B-tree key payloads and duplicate
row-ID vectors still use ordinary `Value` and `std::vector` allocations.

The lexer/parser/AST, schema and table rows, SQL value payloads, query executor,
vector-index internals and search scratch, returned results, C API wrappers,
snapshot temporaries, and CLI formatting still allocate through standard C++
containers or `new`. Those bytes are not included in the per-database counters.
The page-cache prototype is not an end-to-end SQL allocator path. See the audit
for exact source paths and ownership.

Fixed-memory operation is not currently supported. Providing a fixed region to
an application callback only bounds allocations that actually use that
callback. It does not prevent allocations by standard containers that remain
external. Zero-allocation-after-initialization is not claimed.
