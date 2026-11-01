# Benchmark baselines

The reviewed local before/after dataset and its full hardware/compiler/workload context are in `docs/BENCHMARKS.md`. It was recorded on an Intel i7-1355U workstation and must not be compared as a pass/fail threshold against GitHub-hosted runner timing.

`benchmarks.yml` records new raw observations and environment metadata as workflow artifacts. Add a numeric baseline and regression threshold only when repeated measurements run on the same controlled hardware and compiler configuration. Current workloads do not report latency percentiles, allocation counts, page-cache hit rates, or flash write amplification.
