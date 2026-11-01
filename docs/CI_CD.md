# GitHub Actions and automated validation

The workflow definitions in `.github/workflows/` are the validation architecture. They are split by purpose, use least-privilege token permissions, pin third-party actions to full commit SHAs, and do not use dependency caches.

## Workflow schedule

| Workflow | Pull request | Push to `main` | Nightly/manual | Release tag | Coverage |
| --- | --- | --- | --- | --- | --- |
| `ci.yml` | Unit and SQL regression tests with GCC and Clang; formatting and clang-tidy; 85% line coverage floor; framework tests; calls the integration workflow | Same | — | — | Required PR checks after repository branch protection is configured |
| `integration-tests.yml` | Called by CI | Called by CI | Reusable workflow only | — | C API, snapshot, bridge, storage, fault-injection cases, and CLI concurrency |
| `portability.yml` | Linux no-mutex, Windows core-only, AArch64 cross-compile | Same | — | — | Windows core build and tests; AArch64 compile only; no hardware claims |
| `sanitizers.yml` | ASan, UBSan, and TSan full suites | Same | — | — | Correctness under supported desktop sanitizers |
| `fuzzing.yml` | 30-second parser fuzz smoke plus tests | — | Reusable; nightly calls with 3,600 seconds | — | Parser only; does not imply storage/recovery fuzz coverage |
| `benchmarks.yml` | — | Release workloads, same-runner parent-commit comparison, and raw artifact | Reusable/manual | — | Seven paired samples; 35% slowdown threshold for workloads with baseline median at least 0.01 ms; artifacts include raw data and JSON comparison |
| `nightly.yml` | — | — | Daily stress/repetition, larger equality workload, one-hour parser fuzz, benchmark artifacts | — | Reports unsupported memory-budget, power-loss, flash, and hardware scenarios in the run summary |
| `release.yml` | — | — | — | Build and full Release suite, package source and Linux x86_64 files, then publish behind the `release` environment | Release assets are uploaded only after verification and environment approval |

The current CTest labels are `unit`, `integration`, `storage`, `regression`, and `concurrency`. Storage fault tests remain alongside the storage modules they exercise; `tests/fault_injection/README.md` maps them. `tests/transactions/README.md` states that transaction tests do not exist because transaction/recovery behavior is not implemented.

## Required repository settings

GitHub does not allow a workflow file to make its own check required. A repository administrator must enable branch protection or a ruleset for `main` and require these check contexts:

- `unit and SQL regression (gcc)`
- `unit and SQL regression (clang)`
- `formatting and static analysis`
- `coverage gate`
- `pinned test and benchmark frameworks`
- `integration and storage tests`
- `Linux single-threaded profile`
- `Windows shared core without POSIX or snapshot targets`
- `AArch64 embedded-Linux core cross-compilation`
- `asan`, `ubsan`, and `tsan`
- `parser fuzz smoke`

The `release` workflow also requires a GitHub environment named `release` with at least one required reviewer. Configure that protection in repository settings. The release job has `contents: write`; all other workflows have read-only contents permission.

## Benchmark policy

`benchmarks/baselines/` stores reviewed methodology. Each main/nightly run builds the parent commit and candidate on the same hosted runner, repeats each workload seven times, and compares median per-operation latency. It gates only workloads whose baseline median is at least 0.01 ms, with a 35% slowdown threshold; this avoids comparing the historical i7-1355U workstation numbers to a different runner and ignores sub-noise-floor operations. Results include every sample and the baseline commit in JSON, plus runner metadata and `/usr/bin/time -v` output. Shared runners remain variable, so this gate is intentionally broad and should be treated as a regression signal, not a precise performance claim. Current workloads do not measure allocation counts, page-cache hit rates, storage write amplification, or full latency distributions.

`benchmarks/results/` is an artifact destination description, not a place to commit raw runner output. The workflow retains artifacts for 90 days; nightly artifacts are retained for 30 days.

## Portability boundaries

The Windows job builds and executes only shared-core, C API, and injected-memory-page tests. It does not build the POSIX CLI or claim Windows snapshot semantics. The AArch64 job compiles `hyperdb_core` and does not execute the result. The no-mutex profile is tested on Linux, but no RTOS adapter, bare-metal build, physical flash device, or hardware power-cut test exists. The nightly job reports missing checks explicitly rather than silently skipping or claiming them.

## Existing run investigation and operational status

Historical GitHub run [40803141644](https://github.com/Tobechukwu-Ikenwe/HyperDB/actions/runs/40803141644) had a successful Linux GCC/Clang build, release job, sanitizers, coverage, and style job. The macOS test step failed after its build succeeded. GitHub's current job metadata identifies the failed step as `Test` but the log endpoint returned no test output, so the specific failing CTest remains unidentified. This workflow set replaces the macOS job with the explicitly required Windows and AArch64 configurations.

Workflow YAML is locally syntax-checked before commit. A workflow is not called operational until the new revision has run on GitHub and all required jobs pass. Branch protection and the release environment are repository settings and must be verified separately.
