# ADR 0006: Disk manager allocation and page validation

Status: accepted, 2026-10-28.

## Context

The page codec verifies bytes, while `File` supplies positioned I/O. A disk manager must bind logical page IDs to file offsets and update the file-header page count without exposing partially initialized pages.

## Options

The file header can be written before or after a new page. Writing it first could make a crash expose an uninitialized page. Writing the new page and syncing it first leaves an orphan tail if the process stops before the header update.

## Decision

Treat the checksummed page-zero count as authoritative. To allocate page N, write and sync the initialized page at `N * 8192`, then update, write, and sync page zero with count N+1. Reopen accepts physical trailing bytes beyond the authoritative count, including an incomplete orphan page, and overwrites them on the next allocation. It rejects a file shorter than the committed count. A failed header write or sync poisons further allocations on that manager instance; reopening inspects the actual header. Reads validate checksum and page ID. Writes validate both before I/O. File access goes only through `File`.

## Tests and exit criterion for this step

Test create, append, write, sync, reopen, logical page bounds, corrupted page/header detection, and an injected sync failure that leaves an orphan tail and can be retried after reopen. Reproduce with `cmake --build --preset debug && ctest --preset debug --output-on-failure`.

## Consequences

This is a single-user page allocator, not a transaction manager. Page writes have no WAL-before-data enforcement yet, and directory-entry durability on database creation is not yet guaranteed. The buffer pool and WAL layers must supply those guarantees before the new path can replace snapshot persistence.
