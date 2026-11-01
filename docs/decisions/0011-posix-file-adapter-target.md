# ADR 0011: Separate the POSIX file adapter target

Status: Accepted, 2026-10-31.

## Context

`src/storage/posix_file.cpp` uses `open`, `pread`, `pwrite`, `fsync`, and `fstat`, but CMake compiles it into `hyperdb_core`. The disk manager consumes the abstract `FileSystem` interface and does not require these calls. Existing file, disk-manager, and buffer-pool tests use the POSIX factory.

## Decision

Compile `posix_file.cpp` in a distinct `hyperdb_posix_file` target. Link only consumers that call `make_posix_file_system()` against that target. Keep the existing `FileSystem` contract and SQL behavior during this step. Validate the build and all existing tests in Debug and Release.

This is a build boundary, not a claim that `hyperdb_core` is OS independent: snapshot persistence still calls POSIX APIs and its public path type is `std::filesystem::path`.

## Alternatives and trade-offs

- Keeping the adapter inside the core preserves today's build but blocks a clean platform link boundary.
- Moving snapshot persistence simultaneously would enlarge this change and complicate attribution of regressions.
- Splitting the adapter first keeps the existing file integration tests as a direct gate. It adds one CMake target and makes link requirements explicit.

## Follow-up

Move snapshot I/O behind the portable storage contract, then allow non-POSIX builds to omit all POSIX sources. Add a no-POSIX compile/link gate only after that migration.
