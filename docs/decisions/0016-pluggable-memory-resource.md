# ADR 0016: Pluggable memory resource and optional HyperMalloc backend

## Status

Accepted for incremental implementation.

## Context

HyperDB's SQL engine uses the C++ system allocator directly through standard
containers and `new`. HyperMalloc is a separate Linux-only C allocator with
`hm_malloc`, `hm_free`, `hm_calloc`, `hm_realloc`, and `hm_memalign`; it has no
initialization function. It obtains memory through anonymous `mmap`, uses
pthread mutexes, assumes a 64-bit ABI, and provides process-wide allocator
statistics. It does not accept a caller-owned region or offer isolated heaps.

The HyperMalloc repository exports the optional CMake target
`HyperMalloc::hypermalloc`. Its use must not add a required dependency or make
the HyperDB core depend directly on HyperMalloc symbols.

## Decision

Introduce one HyperDB memory-resource interface, based on
`std::pmr::memory_resource`, with system, application-callback, and optional
HyperMalloc upstreams. Wrap each selected upstream with per-resource budget
accounting for requested bytes, current/peak bytes, successful allocations,
deallocations, and failures. A zero budget means unlimited. Allocation failure
is reported as `std::bad_alloc` at the C++ boundary and mapped to
`HDB_OUT_OF_MEMORY` at the C API boundary.

HyperMalloc allocations use `hm_malloc` for alignments up to its guaranteed
16-byte alignment and `hm_memalign` for larger valid power-of-two alignments;
all are released with `hm_free`. The backend is enabled only by an explicit
build option and successful `find_package(HyperMalloc CONFIG)` on supported
Linux 64-bit systems. HyperDB does not use `LD_PRELOAD`, replace global `new`,
or call HyperMalloc from database components directly.

Allocator-aware containers and objects will be migrated incrementally. The
public memory statistics must describe only allocations that have actually
been routed through the selected resource. No fixed-memory or whole-engine
budget guarantee is made while ordinary STL allocations remain outside it.
The system allocator remains the default. Application callbacks must provide
matching aligned allocation/deallocation and outlive the database and any
results allocated from them.

HyperMalloc's global statistics are not attributed to a particular HyperDB
instance. HyperDB's resource wrapper provides per-instance accounting for
requests routed through it. HyperMalloc's internal mapped-byte capacity cannot
be reported as per-instance available capacity.

## Alternatives considered

- **Global `operator new` override or `LD_PRELOAD`:** rejected because it is
  process-wide, prevents independent allocator selection per database, can
  capture unrelated libraries, and violates allocator ownership boundaries.
- **HyperMalloc calls scattered through the engine:** rejected because it
  couples the core to a Linux-only dependency and makes backend substitution
  and matching deallocation difficult.
- **Connect HyperMalloc only to `BufferPool`:** rejected as the final design;
  parser, SQL model, indexes, and query intermediates are major allocation
  sources and must be inventoried and migrated or explicitly reported.
- **Caller-provided fixed arena:** supported through application callbacks,
  but a built-in fixed-memory execution guarantee is deferred until all
  relevant allocations and exhaustion paths are covered by tests.

## Consequences and validation

The allocator interface can be tested independently with deterministic
callbacks and failure injection. Every converted subsystem must have tests for
resource selection, ownership, and allocation failure. HyperMalloc remains
optional and its Linux-only/pthread/mmap requirements are reported separately
from system and application allocator support. Changes to C API creation are
additive; the existing `hdb_database_create` continues selecting the system
allocator.
