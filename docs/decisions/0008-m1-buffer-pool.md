# ADR 0008: Bounded page cache and WAL flush boundary

Status: accepted, 2026-10-29.

## Context

The disk manager validates and writes pages, but it does not know whether the corresponding WAL is durable. M1 needs a cache boundary that prevents dirty pages from reaching the file before their pageLSN is durable.

## Decision

Add a bounded single-user buffer pool with copy-based `read`, `write`, `flush`, and `flush_all` operations. Pages are validated on write, and a round-robin victim is written before reuse. A `WalDurability` interface supplies `flush_through(pageLSN)`; the pool calls it before every dirty page write and then syncs the page file. A failed WAL flush leaves the cached page dirty and prevents a data write. The pool owns no WAL implementation yet, so it cannot claim recovery.

## Tests and exit criterion for this step

Test cache reads, dirty writes, eviction, reopen persistence, and an injected WAL failure that leaves disk unchanged. Reproduce with `cmake --build --preset debug && ctest --preset debug --output-on-failure`.

## Consequences

The pool limits in-memory pages and provides a narrow WAL ordering seam. It does not provide transactions, atomic multi-page changes, or a recovery oracle. Those require the WAL format and replay policy in later steps.
