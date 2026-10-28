# HyperDB status

Updated: 2026-10-28. Baseline commit: `367d3e8`. Foundation and first M1 step: `19c998a`.

## Milestones

| Milestone | State | Reproducible evidence |
| --- | --- | --- |
| M0 foundation | In progress | Debug, release, UBSan, TSan, and coverage builds and 12/12 tests passed locally. ASan build passed; 12/12 tests passed with `ASAN_OPTIONS=detect_leaks=0`. Fuzz preset built and 5-second parser smoke ran 108,669 inputs without a crash. CI is configured but has not run remotely. |
| M1 durable single-user engine | In progress | File I/O boundary tests pass for create, reopen, offset checks, EOF, and injected read/write/sync failures. Legacy snapshot tests pass; no M1 exit criterion has been met. |
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
- HDB-5: M0 still needs pinned GoogleTest/Google Benchmark targets, full warning gates across legacy code, coverage reporting, and a verified CI run. `HYPERDB_FUZZ` has a parser target but no committed corpus.
- HDB-6: GCC 15 Release emits `-Wfree-nonheap-object` warnings in legacy variant/statement code. Investigate whether this is a false positive before enabling `-Werror` for the entire target.
- HDB-7: LeakSanitizer cannot start inside this development sandbox because of process tracing; CI must verify default ASan settings. Local ASan evidence excludes leak checking.

## Next steps

Complete M0 gates and one-command verification, then implement the M1 versioned page codec and disk manager on the tested `FileSystem`/`File` interface. Move legacy snapshot/CLI I/O behind that interface later, with compatibility tests. Do not close M1 without its stated long-running differential, fuzz, and restart criteria.
