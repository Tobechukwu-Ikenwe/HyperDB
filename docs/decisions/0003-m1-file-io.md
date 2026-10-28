# ADR 0003: M1 file I/O boundary

Status: accepted, 2026-10-28.

## Context

The legacy snapshot engine uses streams and raw POSIX calls. The target page engine needs deterministic short-I/O behavior and fault injection before building a disk manager.

## Options

An abstract `File` returning statuses can use positioned operations, while a stream abstraction would hide offsets and make partial I/O hard to model. A global syscall hook would couple tests to process state.

## Decision

New storage code will depend on `FileSystem::open` and `File::{read_at,write_at,sync,size}`. `read_at` and `write_at` complete the requested byte count or return an error. Reads past EOF return corruption. A POSIX implementation loops over EINTR and short transfers, bounds-checks offsets, and closes with RAII. A fault wrapper fails an assigned operation number before it reaches the delegate. File creation is explicit. The page manager will own file layout, checksums, and write ordering; the file layer does not promise transaction durability.

## Tests and exit criterion for this step

Test create/open/round-trip, EOF rejection, invalid offset, fault injection for write and sync, and reopen persistence. One-command verification: `cmake --build --preset debug && ctest --preset debug --output-on-failure`. This verifies the I/O boundary only, not M1.

## Consequences

The old snapshot and CLI still bypass this boundary under HDB-1. Future storage modules must not use direct POSIX calls outside `src/storage/posix_file.cpp`.
