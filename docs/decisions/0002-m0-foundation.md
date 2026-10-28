# ADR 0002: Foundation interfaces and build modes

Status: accepted, 2026-10-28.

## Context

The existing code throws internally, has no stable status-returning API, and has one CMake configuration. Replacing all APIs immediately would risk regressions.

## Decision

Add `hyperdb::Status` and `hyperdb::Result<T>` as the error boundary for new modules. Preserve legacy throwing interfaces during migration. Add CMake presets for debug, release, sanitizers, coverage, and fuzz development. Add a small dependency-free logging interface; use optional test/benchmark dependencies only once pinned versions and their builds are verified. Keep existing tests active throughout.

## Consequences

M0 remains open until all named presets, pinned test/benchmark scaffolding, formatting/lint gates, and the full CI matrix are verified. A local successful build does not establish CI status.
