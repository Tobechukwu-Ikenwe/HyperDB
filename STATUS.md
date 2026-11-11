# HyperDB status

Updated: 2026-11-01. Baseline commit: `367d3e8`. Foundation and file I/O boundary: `19c998a`.

Testing and benchmark framework update (2026-11-11): all C++ test executables
now register GoogleTest cases through CTest discovery; test expectations remain
active in Release. The C API behavior test is compiled as C++ against the C
header, while the C example still validates C compilation. All C++ workload
benchmarks use pinned Google Benchmark v1.9.4 and emit JSON. The hand-timed
parent benchmark output is not comparable to this new format, so CI currently
records seven Google Benchmark repetitions without a regression gate pending a
reviewed baseline. Pinned GoogleTest v1.17.0 and Google Benchmark v1.9.4 are
enabled by default through FetchContent and can both be disabled for core-only
cross builds. Debug and Release each pass 22/22 CTest cases locally; the
Google Benchmark smoke ran all 14 SQL workload registrations and produced JSON.
The same-binary benchmark comparison utility also parsed repeated JSON samples.
GitHub workflows have not yet run on this migration; the allocator branch is
pushed, but PR creation is blocked by the connected GitHub API's 403.

Allocator milestone update (2026-11-01): inspected HyperMalloc at pinned commit
`3258efd0742351a1045d51442ab5d10bee3d578c`; its standalone suite passed 13/13
tests. Added a HyperDB PMR adapter for system, application callback, and
optional HyperMalloc backends; per-database requested-byte accounting/budgets;
an additive C API for backend selection and stats; and routing for database
registry maps plus B-tree node/entry/child containers. The default Debug and
optional HyperMalloc configurations each build and pass 19/19 CTest tests
locally. `.github/workflows/hypermalloc.yml` is added but has not run on GitHub.
Coverage remains partial: parser/AST, table rows and payloads, query allocations,
vector-index internals, returned result objects, C handles, and snapshot/CLI
temporaries remain outside the configured allocator. The byte budget is not a
whole-process cap; fixed-memory operation and allocator speed claims are
unsupported. Transaction rollback tests do not apply because HyperDB has no
transaction API.

Next: run the allocator workflow remotely when repository CI is available, then
expand allocator-aware routing and deterministic failure injection. Do not mark
the memory-budget or portability milestones complete based on this slice.

Source audit on 2026-10-29 removed five unreferenced files and two CMake source entries (ADR 0009). Debug and Release each passed 15/15 tests after the cleanup. The older `Limit` and `filter_table` helpers remain because tests exercise them; `filter.cpp` also serves vector metadata search.

The optimization history was consolidated in `optimizatoons.md` on 2026-10-31. It distinguishes recorded local timings from unmeasured storage work and notes gaps in the historical benchmark record. No new performance claim or engine behavior change was made in this documentation session.

The portability audit and target contracts were recorded in `docs/architecture/PORTABILITY_AUDIT.md` and related architecture documents. Stage 1 (audit) and Stage 2 (architecture) are complete. In Stage 3, `src/storage/posix_file.cpp` was moved out of `hyperdb_core` into `hyperdb_posix_file`; Debug and Release each built successfully and passed 15/15 CTest cases. This is only a partial build boundary: snapshot persistence still contains POSIX file calls, so the engine is not yet OS independent. README links now point to the audit and progress plan.

Stage 3 now also has an initial opaque C API in `include/hyperdb/c_api.h`, C and C++ examples, and a C-compiled API test. It supports in-memory handles and single-statement execution with typed result access; it uses the system allocator and does not yet support prepared statements, persistence, allocator injection, or storage injection. The POSIX file adapter and legacy snapshot compatibility implementation are separate CMake targets; the C and C++ in-memory examples link without them. `nm -u` found no direct POSIX I/O symbols in the core archive. Debug and Release each built and passed 16/16 CTest cases on 2026-11-01. Both examples ran successfully. The core still uses `std::mutex`, exceptions, and full STL facilities. This does not complete Stage 3 or establish any new platform target.

Stage 4 has begun with `DiskManager::open(std::unique_ptr<File>, create)`, allowing a caller-provided random-access storage object without a path or filesystem. The memory-backed adapter test initializes a page file, allocates and writes a page, destroys the manager, reopens the same backing bytes, and validates the record. Debug and Release each pass 17/17 tests. This tests the page prototype only; SQL persistence still uses whole-file snapshots and raw flash is unsupported.

The `single-threaded` CMake preset now selects a no-op `Mutex` implementation for the core. The Linux Debug profile built and passed 17/17 tests, and `nm -u build/single-threaded/libhyperdb_core.a` reported no mutex lock symbols. Shared handles must remain confined to one thread in this mode. Windows and embedded toolchains are still unverified.

The test tree now separates unit, integration, regression, storage, fuzz, transaction, and fault-injection documentation; benchmark workloads, methodology, and results are likewise separated. Eight GitHub Actions workflows cover PR/main checks, storage and integration, Linux single-threaded and Windows/AArch64 compile profiles, sanitizers, parser fuzzing, benchmark comparison, nightly stress, and release packaging. The benchmark workflow builds the parent revision and candidate on one runner, repeats 7 samples, and applies a broad 35% threshold only above a 0.01 ms baseline. Local Debug, Release, single-threaded, and coverage profiles each passed 17/17 tests; coverage was 1,701/1,874 lines (90.8%, above the 85% floor). Commit `765eae9` passed all four push workflows on GitHub: CI, Sanitizers, Portability, and Benchmarks. PR #1 passed all 13 required checks, including the parser fuzz smoke. Strict `main` branch protection requires the core, portability, sanitizer, and parser-fuzz checks. The `release` environment has an owner reviewer and disables administrator bypass. Nightly and release publication have not yet been exercised. Historical run 40803141644 still has an unidentified macOS CTest failure.

## Milestones

| Milestone | State | Reproducible evidence |
| --- | --- | --- |
| M0 foundation | In progress | Full-core `-Wall -Wextra -Wpedantic -Werror` Debug and Release builds passed locally; GCC 15 Release has the narrow `-Wfree-nonheap-object` exemption documented in ADR 0007. GitHub Actions run 40803141644 passed Linux GCC/Clang, Release, sanitizers, frameworks, coverage, fuzz smoke, and style, but its macOS test job failed. Coverage gate: 1,618/1,776 executable lines (91.1%, GCC 15.2.0). |
| M1 durable single-user engine | In progress | File I/O boundary, versioned page codec, disk manager, and bounded buffer-pool tests pass locally in Debug and Release (15/15). Tests cover checksums, slotted record bounds, page allocation, reopen, injected disk sync failure, and WAL-flush gating. The WAL durability interface has no implementation. Legacy snapshot tests pass; the new storage path is not integrated with SQL and no full M1 exit criterion has been met. |
| M2 indexes | Not started | Existing equality index is in memory. |
| M3 transactions and recovery | Not started | No WAL. |
| M4 concurrency | Not started | No database-level multi-connection isolation. |
| M5 optimizer and large data | Not started | No cost model or spill operators. |
| M6 performance and hardening | Not started | Existing measurements are exploratory. |

## Open risks and issues

- HDB-1: Legacy snapshot and CLI use direct file operations. Migrate them through the FileSystem boundary after new storage tests.
- HDB-2: Existing C++ tests use `assert` rather than a pinned framework; release builds must retain assertions or migrate tests.
- HDB-3: Current format does not provide page checksums or WAL, so crash guarantees are unverified.
- HDB-4: No controlled CI result is available yet across GCC, Clang, and macOS.
- HDB-5: M0 has pinned GoogleTest/Google Benchmark targets, a coverage gate, committed parser corpus, and full-core warning gates. GitHub Actions run 40803141644 passed all jobs except macOS tests; investigate and rerun that job before closing M0.
- HDB-6: GCC 15 Release emits `-Wfree-nonheap-object` in libstdc++ `std::variant` copy assignment while compiling legacy query and statement code. ADR 0007 suppresses this diagnostic only for GCC 15+ Release. Compare compiler versions and investigate the source path before removing the exemption.
- HDB-7: LeakSanitizer cannot start inside this development sandbox because of process tracing; local ASan evidence excludes leak checking. GitHub Actions run 40803141644 passed its ASan job with default settings.
- HDB-8: macOS CI built successfully but `ctest` returned exit code 8 in run 40803141644. The public job metadata does not identify the failed test; retrieve the step log or reproduce on macOS before changing platform behavior.
- HDB-9: Nightly scenarios and release publication remain unexecuted. The PR fuzz smoke passed on PR #1. The release environment approval gate is configured, but no tag was created to exercise it. The historical macOS failure from run 40803141644 remains unidentified; current portability validation covers Windows and AArch64 instead.

## Next steps

Run nightly and PR fuzz checks on schedule/PR, and exercise release publication only when a real release tag is authorized. Continue M1 with a WAL format and recovery path before replacing legacy snapshot persistence; migrate snapshot/CLI I/O through the `FileSystem` boundary with compatibility tests. Do not close M1 without its stated long-running differential, fuzz, and restart criteria.

For the portability program, continue Stage 3 by moving snapshot persistence behind injected storage, then add allocator/synchronization contracts and the tested C embedding API. Do not claim Windows, RTOS, bare-metal, or fixed-memory support until their stated validation gates pass.
