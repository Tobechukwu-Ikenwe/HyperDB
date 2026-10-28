# Testing and reproduction

Baseline and the new foundation/I/O tests run with:

```sh
cmake --preset debug
cmake --build --preset debug --parallel 4
ctest --preset debug --output-on-failure
```

The same three commands work with `release`, `asan`, `ubsan`, `tsan`, and `coverage` in place of `debug` after each preset is configured. `fuzz` uses Clang and builds `hyperdb_parser_fuzz`; run `build/fuzz/hyperdb_parser_fuzz -max_total_time=60` for a local smoke run. The required one-hour M1 fuzz exit has not been run.

The existing C++ tests are `assert` programs; each undefines `NDEBUG` so assertions run in Release. Python integration tests use the CLI and bridge. The file test uses a temporary path, tests reopen, EOF, invalid offsets, and deterministic operation failures. It does not simulate torn writes, lost unsynced writes, or crashes. No sqllogictest runner, 100,000-query differential run, coverage threshold, or recovery oracle exists yet.

On this development host, LeakSanitizer fails under the sandbox's process tracing before any test executes. For local AddressSanitizer memory checks use `ASAN_OPTIONS=detect_leaks=0 ctest --preset asan --output-on-failure`; CI runs the default LeakSanitizer settings on its runner.
