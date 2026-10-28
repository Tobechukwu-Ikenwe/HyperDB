# ADR 0001: Preserve the snapshot engine while building the page engine

Status: accepted, 2026-10-28.

## Context

The existing engine runs a SQL subset and passes ten baseline tests. Its in-memory table representation and whole-file snapshot format are incompatible with the target page store and WAL architecture. Replacing them at once would erase tested behavior and make failures hard to isolate.

## Options

1. Rewrite the engine in place.
2. Introduce narrow page-store modules alongside the snapshot path, migrate callers one boundary at a time, then remove the old path after equivalent behavior is tested.

## Decision

Use option 2. Start with API error types and file I/O interfaces, then page codecs and disk manager, buffer pool, heap, catalog, SQL migration, and CLI migration. Keep `HDBSNAP1` readable until a separately tested migration path exists. Give the new page format its own magic/version and tests. Existing Python demos remain optional and outside the engine core. Introduce CMake targets at module boundaries as code moves; prevent new cyclic dependencies.

## Consequences

There will temporarily be two storage paths. Documentation must identify which path each test exercises. M1's exit criteria apply only to the new durable path and cannot be inferred from legacy snapshot tests.
