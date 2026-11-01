# ADR 0012: Add an initial C embedding API

Status: Accepted, 2026-11-01.

## Context

The engine exposes C++ implementation headers under `src/` and SQL execution can throw C++ exceptions. There is no C embedding boundary. A portable header must not expose `Database`, `Value`, STL containers, or exceptions.

## Decision

Add an opaque in-memory database handle and an opaque owned result handle in a C-compatible header. Expose a single-statement execute call, result metadata, typed values, and caller-owned error buffers. Catch all C++ exceptions before returning through the ABI. Result pointers remain valid until the result handle is destroyed. The initial implementation uses the system allocator and is single-threaded per handle.

## Trade-offs and follow-up

This creates a usable C boundary without changing the SQL parser or claiming bounded allocation. It does not add prepared statements, persistent open, custom allocators, or synchronization; those need separate designs and tests. OOM status is returned where allocation failure is caught. Error-buffer truncation is explicit and always null-terminates a non-empty buffer.

## Validation

Compile and link a C translation unit against the C ABI, execute CREATE/INSERT/SELECT, read typed result values, and verify malformed SQL returns an error without crossing the ABI as an exception. Build and run the full Debug and Release suites.
