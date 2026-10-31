# Portable performance evidence and gates

Status: baseline plan. No performance improvement from portability work is claimed. Existing measurements are in [BENCHMARKS.md](../BENCHMARKS.md), [benchmark evidence](../benchmarks.md), and [optimization history](../../optimizatoons.md). Those local in-memory figures are not a cross-platform storage baseline.

## Before/after protocol

For each architectural slice, record exact before/after commit IDs, compiler and flags, OS/board, allocator, storage adapter/device and geometry, page/cache sizes, data fixture and checksum, query text, iterations, warm/cold state, raw machine-readable results, medians and dispersion, and peak allocated/stack bytes. Run correctness and fault tests before reporting timing. Compare equivalent semantics and exclude or include setup consistently. Record regressions as well as wins in `docs/benchmarks.md`.

| Workload | Metrics to capture |
| --- | --- |
| Insert and transaction commit | Rows/s, commit latency distribution, writes and synced bytes, power-loss outcome. |
| Point/range/index lookups | Latency, page reads, cache hit rate, result equality. |
| Scan/filter/projection | Rows/s, query latency, peak temporary memory. |
| Join/group/sort | Candidate count, result rows, spills, latency, peak memory. |
| Page cache and storage | Hit/miss/eviction counts, I/O bytes, sync count, write amplification. |
| Flash-backed device | Erases/programs per transaction, endurance assumptions, interrupted-write behavior. |
| Memory tiers | Allocator high-water mark, stack high-water mark, explicit exhaustion outcomes. |

Desktop comparisons with SQLite can be useful only when both engines execute equivalent supported queries through comparable APIs with the same dataset, indexing, durability setting, and cache/warmup policy. The existing Python/SQLite and C++ HyperDB measurements do not establish an overall speed ranking. A portability change is accepted on correctness first; any material regression gets a profile and workload analysis before optimization.
