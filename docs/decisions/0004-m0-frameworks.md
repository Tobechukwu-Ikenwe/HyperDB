# ADR 0004: Pinned test and benchmark frameworks

Status: accepted, 2026-10-28.

## Context

Legacy tests use `assert`, and the existing benchmark programs use manual timers. M0 calls for GoogleTest and Google Benchmark scaffolding fetched through CMake with pinned versions. Making network access a requirement for every default build would break offline use of the already working engine.

## Options

1. Require framework downloads on every configure.
2. Put framework targets behind a named CMake preset and build that preset in CI, while default presets remain usable offline.

## Decision

Use option 2. Pin GoogleTest to `v1.17.0` and Google Benchmark to `v1.9.4` through `FetchContent`. Add a GoogleTest unit target for the new status API and a Google Benchmark microbenchmark for positioned reads. The benchmark is scaffolding; no throughput claim follows from its existence. Keep legacy `assert` tests until migrated. CI must configure, build, and test the framework preset. Framework code is not linked into the engine core.

## Consequences

The framework preset needs network access at first configure. Default builds remain offline. Benchmarks require deliberate execution and recorded machine details before results can be compared.
