# Storage architecture

Updated: 2026-11-01. The legacy `Database::save/load(path)` implementation is isolated in `hyperdb_snapshot_compat`; it is not part of the page store or portable core. It still uses filesystem streams, paths, and POSIX sync on Unix. A callback-based storage contract and page-backed SQL persistence remain unimplemented.

Status: target design. The active SQL path remains the `HDBSNAP1` whole-file snapshot. The page path is experimental and not a production database.

## Logical and physical layers

The shared engine will operate on logical pages and a capability-checked random-access device. A POSIX or Windows file adapter can supply those operations. An embedded application can supply a filesystem-backed device or an FTL-backed logical device. A raw erase-before-write flash chip is not accepted as a normal byte-addressable device. The application driver owns wear leveling, bad-block policy, and any flash translation not explicitly implemented by HyperDB.

The existing page format uses fixed 8192-byte pages, explicit little-endian fields, CRC32C, version 1, a page-zero header, and append-only slotted records. See [ADR 0005](../decisions/0005-m1-page-format.md). A future format version must persist logical page size and reject unsupported sizes at open. Page size is selected at database creation from a tested supported set, not silently inferred from physical erase geometry. The format must define record/overflow pages, free-space and reclamation metadata, catalog roots, index pages, checksums, pageLSN, and compatibility/migration rules before replacing snapshots. Raw C++ object layouts are never persisted.

| Component | Current | Required next invariant |
| --- | --- | --- |
| File/device I/O | POSIX positioned file adapter and `FileSystem::open(path)` | Core-level logical device callback; adapters handle paths, alignment, capacity, and durable barriers. |
| Page codec | Checksummed fixed slotted pages | Versioned size, page types, bounded record/overflow validation, golden format fixtures. |
| Allocation | Append page then publish count; orphan tail tolerated | Free-page tracking, reclamation, and crash-safe metadata updates. |
| Pager | Copy cache with dirty flag, round-robin victim | Pin guards, bounded cache bytes, eviction policy, write ordering, and error propagation. |
| Tables/catalog | Whole in-memory vectors and snapshot serialization | Page-backed rows/schema with stable IDs and explicit ownership. |
| B-tree | In-memory equality index rebuilt on load | Persistent page-backed index with split/merge and maintenance tests. |

## Flash integration contract

The adapter reports program alignment, erase-block size, overwrite support, atomic unit, capacity, and sync durability. HyperDB does not assume page size equals erase size. A device lacking random overwrite must have a translation layer that exposes the logical contract. If the adapter cannot establish durable ordering through interrupted power, the engine refuses durable mode. Flash write amplification and endurance must be measured on the actual stack before acceptance; a filesystem adapter atop flash does not automatically prove raw-device suitability.

## Compatibility and migration

Existing `HDBSNAP1` files remain readable during migration. New page files retain distinct magic/version. A migration utility reads a legacy snapshot, writes a new database to a separate destination, verifies schema/rows/index definitions/embeddings, syncs it, and only then offers replacement. Unsupported versions fail explicitly. Recovery is defined in [TRANSACTION_ARCHITECTURE.md](TRANSACTION_ARCHITECTURE.md).
