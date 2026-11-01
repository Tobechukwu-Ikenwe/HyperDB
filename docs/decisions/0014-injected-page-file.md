# ADR 0014: Allow direct application storage injection for page files

Status: Accepted, 2026-11-01.

## Context

The `File` interface is already a random-access byte contract, but `DiskManager::open` only accepted a `FileSystem` plus `std::filesystem::path`. Device-backed applications therefore had to implement a filesystem-shaped path layer even when they already had an open block or flash translation device.

## Decision

Add an overload that takes ownership of `std::unique_ptr<File>` and initializes or validates the page header using the existing logic. Keep the filesystem overload as a convenience that opens a file and delegates. The application adapter remains responsible for translating logical byte reads/writes and sync into its device semantics.

## Limits and trade-offs

This permits page-store tests and integrations without paths, but does not make SQL tables page-backed, eliminate dynamic allocation, report flash geometry, or provide wear leveling. The current `File` contract still requires complete-or-error random access, file size, and durable `sync`; raw flash that cannot offer these semantics must use an application FTL or is unsupported. The implementation takes ownership of the adapter object until the disk manager is destroyed.

## Validation

Use a test-only memory-backed `File` with no `FileSystem` or path. Initialize, allocate, close, reopen over the same backing bytes, and validate page content. Retain existing POSIX and fault-injection regression tests.
