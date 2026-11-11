# Benchmark evidence

All C++ microbenchmarks use Google Benchmark v1.9.4. C++ tests use GoogleTest
v1.17.0; both are pinned through CMake FetchContent. Disable the frameworks
only for core-only or cross-compilation configurations with
`-DHYPERDB_BUILD_TESTS=OFF -DHYPERDB_BUILD_BENCHMARKS=OFF`.

Build Release workloads with:

```sh
cmake --preset release
cmake --build --preset release --target hyperdb_workloads hyperdb_benchmark hyperdb_file_io_benchmark
./build/release/hyperdb_workloads --benchmark_min_time=0.05s --benchmark_repetitions=7 \
  --benchmark_out=benchmarks/results/workloads.json --benchmark_out_format=json
./build/release/hyperdb_benchmark --benchmark_min_time=0.05s --benchmark_repetitions=7 \
  --benchmark_filter='BM_Equality.*100000' \
  --benchmark_out=benchmarks/results/equality.json --benchmark_out_format=json
```

Fixture creation is outside timed iterations, and each SQL benchmark validates
its result. The workload suite covers parsing, scans, sorting, grouping, index
construction and lookup, joins, vector search, writes, and snapshot I/O. The
equality target reports scan and indexed lookups; index construction is not
included in lookup timing. The file-I/O target measures positioned reads from
the POSIX adapter. These measurements do not represent full application
latency or a general comparison with SQLite.

The GitHub benchmark workflow stores Google Benchmark JSON, console output,
runner metadata, and `/usr/bin/time -v` output as a 90-day artifact. Nightly
artifacts are retained for 30 days. Performance regression gates are paused
until a reviewed Google Benchmark baseline is recorded. The existing
hand-timed measurements in [BENCHMARKS.md](BENCHMARKS.md) use a different
harness and should not be compared directly with new results.

`tools/benchmark_compare.py` runs two Google Benchmark binaries seven times
each and compares medians. Use it only when both binaries use the same workload
names and definitions. CI no longer compares its new output with the legacy
hand-timed parent binary.
