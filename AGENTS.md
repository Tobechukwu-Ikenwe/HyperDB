# HyperDB engineering agreement

1. Audit existing code before changes. Preserve working behavior and refactor in small, tested steps. Keep the build green.
2. Work in milestone order. For each milestone, write a short ADR, define interfaces, write tests, implement, run the one-command exit criteria, and record results. Do not mark a milestone done before its exit criteria pass.
3. Do not claim functionality without tests or performance without a reproducible recorded benchmark. Put unverified behavior in `docs/limitations.md`.
4. Leave no stubs, dead code, or commented-out code. Every TODO in merged code needs a linked STATUS.md issue.
5. Make small atomic conventional commits (`feat:`, `fix:`, `test:`, `perf:`, `docs:`, `refactor:`); keep CI green at each commit.
6. Prefer simple correct designs. Optimize only with profiling evidence and record before/after numbers in `docs/benchmarks.md`.
7. Ask the user only when blocked. Otherwise decide, record the decision in `docs/decisions/`, and continue.
8. The engine core has no runtime dependencies beyond C++20 and POSIX. Allowed test/build dependencies are GoogleTest or Catch2, Google Benchmark, optionally fmt/spdlog, and compiler-provided libFuzzer, pinned through CMake FetchContent.
9. Use RAII, narrow module interfaces, no exceptions across the public API boundary, no cyclic CMake dependencies, strict warnings in CI, and documented invariants. Every correctness bug gets a regression test.
10. Update STATUS.md at the end of every work session with milestone progress, open risks, and next steps.
