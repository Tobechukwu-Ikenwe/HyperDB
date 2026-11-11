# ADR 0017: Standardize C++ tests and benchmarks

- Status: Accepted
- Date: 2026-11-11

## Context

Most C++ tests are separate `main()` programs using `assert`, which disappears
in Release unless each source manually undefines `NDEBUG`. A small GoogleTest
target already exists, as does a Google Benchmark file-I/O microbenchmark, but
they are optional behind `HYPERDB_FRAMEWORKS`. The main benchmark executables
use hand-written `chrono` loops and text output, requiring custom comparison
logic.

## Decision

Use pinned GoogleTest as the single C++ test framework and pinned Google
Benchmark as the single C++ microbenchmark framework in normal development and
CI profiles. Migrate every C++ test into GoogleTest-discovered test cases and
register them with CTest. Keep each test target/module boundary so CI can select
unit, integration, and storage groups by labels. Keep Python integration tests
under Python and CTest, and keep parser fuzzing under libFuzzer; neither is a
C++ unit-test framework.

Convert C++ workload executables to Google Benchmark registrations. Preserve
workload names and SQL result checks, move fixture creation outside timed
iterations, emit Google Benchmark JSON, and adapt CI comparison to those JSON
results. The standalone Python/SQLite comparison remains as an external-engine
baseline and is not represented as a Google Benchmark run.

Fetch GoogleTest v1.17.0 and Google Benchmark v1.9.4 through pinned CMake
FetchContent declarations. Both default to enabled; `HYPERDB_BUILD_TESTS=OFF`
and `HYPERDB_BUILD_BENCHMARKS=OFF` retain a core-only/offline or cross-compile
configuration without fetching either framework.

## Alternatives considered

- Keep standalone assertions and only use GTest for new tests: rejected because
  it leaves two C++ test conventions and Release assertion hazards.
- Use CTest alone: rejected because CTest runs programs but does not provide a
  common assertion, fixture, or reporting API.
- Keep custom chrono benchmarks and only add Google Benchmark for new work:
  rejected because the project would still have two benchmark formats and
  comparison pipelines.

## Risks and validation

- GTest expectations must not hide side effects previously performed inside
  `assert`; the migrated tests must run in Debug and Release.
- Benchmark setup and validation must be excluded from timed work, and each
  workload must retain a correctness check.
- JSON comparisons must handle Google Benchmark repetition fields and time
  units consistently.
- Validate the complete CTest suite, framework benchmark smoke runs, Release
  assertions, and CI workflows before declaring this decision implemented.
