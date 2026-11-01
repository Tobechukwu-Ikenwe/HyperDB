# HyperMalloc integration audit

Status: source audit and initial integration slice complete; allocation
coverage remains partial. HyperDB is the
only repository being changed. HyperMalloc was inspected at commit
`3258efd0742351a1045d51442ab5d10bee3d578c`; its Release build installed to
`/tmp/hypermalloc-prefix` and its 13 CTest tests passed.

## HyperMalloc API and constraints verified from source

The public header is `HyperMalloc/include/hypermalloc.h`. It exports a C ABI:

| API | Contract relevant to HyperDB |
| --- | --- |
| `hm_malloc(size)` | Returns at least `size` bytes aligned to 16; zero size is a one-byte allocation; returns null and sets `errno=ENOMEM` on failure. |
| `hm_free(ptr)` | Releases only HyperMalloc pointers; null is accepted. Debug invalid frees abort. |
| `hm_calloc(n, size)` | Checks multiplication overflow and returns zero-filled storage. |
| `hm_realloc(ptr, size)` | Failure preserves the old pointer; zero size frees and returns null. |
| `hm_memalign(alignment, size)` | Supports valid power-of-two alignment, used by the adapter when C++ requests more than 16 bytes. |
| `hm_usable_size(ptr)` | Reports usable size for a live allocation. |
| `hm_get_stats(out)` | Reports aggregate allocator statistics across the process, not an individual HyperDB. |

There is no public `hm_init` or shutdown API. Static arena metadata and mutexes
are initialized in the allocator; a constructor registers `pthread_atfork`
handlers. `src/core.c` uses anonymous `mmap`/`munmap` for 256 KiB chunks and
large allocations; there is no `sbrk`. The source requires a 64-bit ABI,
16-byte-or-less `max_align_t`, Linux memory mapping, and pthreads. It is not a
caller-provided-memory allocator and does not expose per-instance heaps, a
capacity limit, or a per-heap statistics handle. `HM_ARENAS` changes the
process-global arena count at build time (1–8).

The CMake project exports `HyperMalloc::hypermalloc` and declares
`Threads::Threads` as a transitive dependency through its installed package.
The installed package was verified with CMake. HyperDB can consume it using
`find_package(HyperMalloc CONFIG REQUIRED)` only when its optional backend is
enabled; the normal build must not include the header or link that target.

## HyperDB allocation inventory

The inventory is based on the current `src/` and `include/` source. “External”
means allocated by the system STL/API and not yet routed through a HyperDB
resource. There is no transaction workspace because transactions are not
implemented.

| Area | Current allocation sites/types | Ownership and status |
| --- | --- | --- |
| Lexer | `Scanner::tokens_`, copied `Token::text`, static keyword `unordered_set`, uppercase strings (`src/parser/lexer.cpp`, `lexer.h`) | Per parse plus process-lifetime keyword set; system allocator. Input/token counts have no budget. |
| Parser and AST | SQL/token copies, AST strings/vectors, recursive `unique_ptr<Expression>`, `std::stod` temporaries (`src/parser/parser.cpp`, `ast.h`) | Per statement; system allocator. Recursion/input sizes have no explicit limits. |
| Database/table model | Outer table/index/vector-index maps; schema vectors/maps; `Table::rows`; each `Row`; strings/blobs in `Value` (`database.h`, `TableData/*`) | Outer registry maps and nested index-map nodes/buckets use the configured resource. Schema, table rows, strings, and blobs remain system allocated. |
| B-tree | Node storage and node entry/child vectors, copied index keys, duplicate row-ID vectors (`Indexes/btree*`, `index_entry.h`) | Node and entry/child container allocations use the configured resource. Key payloads and row-ID vectors remain system allocated. Mutations can allocate during insertion. |
| Vector index | Embedding map/vectors and full candidate result vector (`Indexes/vector_index.*`) | Database lifetime plus per-search scratch; system allocator. Search materializes every eligible candidate before sorting. |
| Query execution | Bindings, table-name maps, index candidates, row-pointer lists, join tables, grouping maps/vectors, result columns/rows (`QueryExecutor/*`, `aggregate.cpp`) | Per query; system allocator. Intermediate materialization can grow with input/result size. |
| Writes | Assignment maps/vectors, survivor rows, row-ID maps, staged `Database` and rebuilt indexes (`statement_executor.cpp`, `database.cpp`) | Per statement; system allocator. Allocation failures can occur after work begins; no transaction rollback facility exists. |
| Page/storage prototype | Buffer-pool frame vector, `unique_ptr` managers/adapters, status-message strings (`storage/*`) | Pool/manager lifetime; system allocator. The page store is not connected to SQL tables. |
| Snapshot/CLI | Deserialization strings/blobs/embeddings, table/column/index temporary vectors, streams/filesystem, CLI result formatting (`persistence.cpp`, `main.cpp`) | Per save/load/process; system allocator. Snapshot load is bounded by file/schema caps but peak memory is not measured. |
| C embedding API | `new`/`delete` for opaque database and result handles; result data uses standard containers (`bridge/c_api.cpp`) | Handle/result lifetime; system allocator. Error reporting uses a caller buffer. |
| Other dependencies | GoogleTest/Google Benchmark allocate their own state; Python, SQLite, PyTorch/Transformers and Streamlit allocate outside the C++ engine | External runtime allocations; must not be included in HyperDB allocator statistics. |

## Integration boundary

The `std::pmr::memory_resource` adapter supports system, application callback,
and optional HyperMalloc backends. Database creation chooses a resource; the C
API retains its original constructor and adds an allocator-config constructor
and statistics query. Counters and byte budgets only cover allocations routed
through the resource. Registry containers and B-tree nodes/entry/child vectors
are routed and exercised by common SQL/index tests. Budget failures during
table creation are verified to leave the database empty. Other subsystems
remain external until migrated and separately tested.

### Application callback example (C)

Initialize a config with `HDB_ALLOCATOR_CONFIG_INIT`, then set `kind`,
`context`, `allocate`, `deallocate`, and optionally `budget_bytes`. Callbacks
must return storage aligned to the requested alignment and release each pointer
using the same context. The context and backing pool must outlive the database.
A failed allocation returns `NULL` and is reported as `HDB_OUT_OF_MEMORY` when
it occurs in a routed path.

```c
hdb_allocator_config config = HDB_ALLOCATOR_CONFIG_INIT;
config.kind = HDB_ALLOCATOR_APPLICATION;
config.context = &my_pool;
config.allocate = pool_allocate;
config.deallocate = pool_deallocate;
config.budget_bytes = 64 * 1024;

hdb_database *db = NULL;
hdb_status status = hdb_database_create_with_allocator(&config, &db);
```

The 64 KiB setting limits only resource-routed requests. It does not claim the
process or a HyperDB query fits in 64 KiB.

The following remain unsupported until separately implemented and tested:

- Passing a static memory region directly to HyperMalloc (not supported by
  HyperMalloc); applications can provide their own region-based callbacks.
- Isolating HyperMalloc heap/statistics per database (HyperMalloc owns one
  process-global allocator state).
- Using HyperMalloc on non-Linux or non-64-bit targets, or in a no-pthread
  profile.
- Claiming a complete database byte budget while any required SQL path still
  allocates outside the resource.
- Claiming zero-allocation statements or fixed-memory execution.
