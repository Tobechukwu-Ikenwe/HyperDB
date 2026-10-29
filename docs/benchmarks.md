# Benchmark evidence

The existing manual benchmark executables and historical measurements are described in [BENCHMARKS.md](BENCHMARKS.md). They do not meet the target's reproducibility and fairness protocol. No new performance result is claimed for the foundation or file I/O boundary. Add raw JSON, machine and compiler details, exact workload commands, and before/after profiling evidence before claiming a speedup.

The pinned Google Benchmark scaffold builds with `cmake --preset framework && cmake --build --preset framework`. Its `hyperdb_file_io_benchmark` target measures repeated positioned reads of an 8 KB page with setup outside the timed loop. It is a development microbenchmark, not a storage-engine throughput result. Capture JSON using `build/framework/hyperdb_file_io_benchmark --benchmark_out=results.json --benchmark_out_format=json` before analyzing any numbers.
