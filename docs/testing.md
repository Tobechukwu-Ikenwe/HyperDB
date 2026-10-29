# Testing and reproduction

Baseline and the new foundation/I/O tests run with:

```sh
cmake --preset debug
cmake --build --preset debug --parallel 4
ctest --preset debug --output-on-failure
```

The same three commands work with `release`, `asan`, `ubsan`, `tsan`, `coverage`, and `framework` in place of `debug` after each preset is configured. The framework preset fetches pinned GoogleTest and Google Benchmark versions on first configure. `fuzz` uses Clang and builds `hyperdb_parser_fuzz`; run `build/fuzz/hyperdb_parser_fuzz build/fuzz/corpus/parser -max_total_time=60` for a local smoke run. The source corpus is copied into the build directory so fuzzing does not modify committed seeds. The required one-hour M1 fuzz exit has not been run.

For the coverage gate, run `python3 tools/coverage_report.py --min-line 0.85` after the coverage preset's tests. This uses GCC's JSON gcov output, counts executable lines in `src/` excluding `src/shell/`, and exits nonzero below 85%. The CI coverage job runs the same command. Instrumentation flags are set before `hyperdb_core` is declared, so core code is included in sanitizer and coverage builds.

Local result on 2026-10-28 after 14/14 coverage tests: 1,618 of 1,776 executable `src/` lines covered (91.1%) with GCC 15.2.0. This is a line metric; it does not establish branch coverage or the milestone exit workloads.

The existing C++ tests are `assert` programs; each undefines `NDEBUG` so assertions run in Release. Python integration tests use the CLI and bridge. The file test uses a temporary path, tests reopen, EOF, invalid offsets, and deterministic operation failures. It does not simulate torn writes, lost unsynced writes, or crashes. No sqllogictest runner, 100,000-query differential run, coverage threshold, or recovery oracle exists yet.

On this development host, LeakSanitizer fails under the sandbox's process tracing before any test executes. For local AddressSanitizer memory checks use `ASAN_OPTIONS=detect_leaks=0 ctest --preset asan --output-on-failure`; CI runs the default LeakSanitizer settings on its runner.
