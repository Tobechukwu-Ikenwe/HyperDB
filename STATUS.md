# HyperDB status

Updated: 2026-10-31. Baseline commit: `367d3e8`. Foundation and file I/O boundary: `19c998a`.

Source audit on 2026-10-29 removed five unreferenced files and two CMake source entries (ADR 0009). Debug and Release each passed 15/15 tests after the cleanup. The older `Limit` and `filter_table` helpers remain because tests exercise them; `filter.cpp` also serves vector metadata search.

The optimization history was consolidated in `optimizatoons.md` on 2026-10-31. It distinguishes recorded local timings from unmeasured storage work and notes gaps in the historical benchmark record. No new performance claim or engine behavior change was made in this documentation session.

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

## Next steps

Resolve the macOS CI test failure to complete M0. Continue M1 with a WAL format and recovery path before replacing legacy snapshot persistence; migrate snapshot/CLI I/O through the `FileSystem` boundary with compatibility tests. Do not close M1 without its stated long-running differential, fuzz, and restart criteria.
