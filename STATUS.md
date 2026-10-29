# HyperDB status

Updated: 2026-10-28. Baseline commit: `367d3e8`. Foundation and file I/O boundary: `19c998a`.

## Milestones

| Milestone | State | Reproducible evidence |
| --- | --- | --- |
| M0 foundation | In progress | Full-core `-Wall -Wextra -Wpedantic -Werror` Debug and Release builds passed with 14/14 tests locally. GCC 15 Release has the narrow `-Wfree-nonheap-object` exemption documented in ADR 0007. Coverage gate: 1,618/1,776 executable lines (91.1%, GCC 15.2.0). Parser fuzz smoke: 106,494 inputs in 11 seconds without a crash. Fuzz tests used `ASAN_OPTIONS=detect_leaks=0` because LeakSanitizer cannot start in this sandbox. Prior UBSan and TSan runs passed. CI is configured but has not run remotely. |
| M1 durable single-user engine | In progress | File I/O boundary, versioned page codec, and disk manager tests pass in Debug, Release, coverage, and fuzz presets. They cover checksums, slotted record bounds, page allocation, reopen, and an injected sync failure. Legacy snapshot tests pass; the new storage path is not integrated with SQL and no full M1 exit criterion has been met. |
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
- HDB-5: M0 has pinned GoogleTest/Google Benchmark targets, a coverage gate, committed parser corpus, and full-core warning gates. It still needs a verified remote CI run.
- HDB-6: GCC 15 Release emits `-Wfree-nonheap-object` in libstdc++ `std::variant` copy assignment while compiling legacy query and statement code. ADR 0007 suppresses this diagnostic only for GCC 15+ Release. Compare compiler versions and investigate the source path before removing the exemption.
- HDB-7: LeakSanitizer cannot start inside this development sandbox because of process tracing; CI must verify default ASan settings. Local ASan evidence excludes leak checking.

## Next steps

Verify remote CI to complete the remaining M0 gate. Continue M1 with a buffer pool and WAL design before replacing legacy snapshot persistence; migrate snapshot/CLI I/O through the `FileSystem` boundary with compatibility tests. Do not close M1 without its stated long-running differential, fuzz, and restart criteria.
