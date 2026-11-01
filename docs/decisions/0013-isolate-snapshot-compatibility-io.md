# ADR 0013: Isolate legacy snapshot I/O from the shared engine target

Status: Accepted, 2026-11-01.

## Context

After separating `posix_file.cpp`, `hyperdb_core` still compiled `database/persistence.cpp`, which contains POSIX `open`/`fsync` and uses `std::filesystem` and file streams. This prevented an application from linking the shared SQL engine without snapshot I/O. Existing CLI, workload, statement, and persistence tests rely on the legacy `Database::save/load(path)` behavior.

## Decision

Move `persistence.cpp` into `hyperdb_snapshot_compat`, a separately linked compatibility target that depends on `hyperdb_core`. Link only snapshot consumers against it. Keep snapshot bytes, path API, and behavior unchanged. The core target must continue to build and link through the C and C++ in-memory examples without either POSIX storage target.

This step removes OS calls from the shared core target but does not yet inject storage into snapshot serialization. The compatibility target remains unsuitable for bare-metal and does not define a crash-recovery guarantee.

## Alternatives and trade-offs

- Keeping persistence in core retains convenient linkage but forces snapshot/runtime dependencies onto every embedding application.
- Rewriting snapshot serialization into callbacks in one step increases format-regression risk.
- Isolating the existing implementation first preserves its tests and enables a future portable storage interface without changing SQL execution.

## Validation

Build and run all Debug and Release tests. Build and run C/C++ in-memory embedding examples linked only to the shared core. Keep snapshot round-trip/corruption tests linked to the compatibility target.
