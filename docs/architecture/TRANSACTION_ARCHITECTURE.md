# Transaction and recovery architecture

Status: design only. Current SQL writes mutate an in-memory `Database` and the CLI then rewrites a snapshot. There is no `BEGIN`/`COMMIT`/`ROLLBACK`, isolation, WAL, or page recovery.

## Decision under evaluation

The first proposed page-backed protocol is single-writer, no-steal redo WAL on a logical device with a verified sync barrier. A transaction stages dirty pages in memory; no uncommitted page is written to the data file. It appends versioned, length-checked, checksummed page records and a transaction commit marker, then syncs the log. Only after successful log sync may commit return success. Checkpointing writes committed pages to data storage, syncs data, then advances a durable log-truncation point. Recovery ignores incomplete/torn trailing records and replays only committed records with valid checksums and page IDs. This is a design target, not an implemented guarantee.

| Property | Proposed first scope | Unresolved acceptance work |
| --- | --- | --- |
| Atomicity | All page changes in one transaction become visible together after a valid synced commit marker. | Failure injection at every log/data/sync step; compare full recovered state. |
| Consistency | Existing schema/row/index invariants checked before commit. | Persistent catalog/index invariants and corruption oracle. |
| Isolation | Single writer; readers use a documented snapshot/locking rule before multi-connection claims. | Reader/writer scheduling and visibility tests. |
| Durability | Success only after the adapter's durable barrier completes. | Target adapter contract and real power-cut validation. |

Rollback journaling could avoid redo log replay but still requires overwrite ordering and extra flash writes. Copy-on-write root switching can suit flash but requires reclamation, bounded free-space accounting, and an atomic root protocol. WAL matches the current pageLSN/buffer seam, yet log append and checkpoint traffic may amplify flash wear. The chosen approach must be revisited if actual device measurements or sync capabilities contradict these assumptions. An adapter without a verified durable barrier cannot claim durable commits.

The current `DiskManager::write_page` is callable outside the buffer pool; that bypass must be closed before page-backed SQL is enabled. A failed sync may leave an ambiguous on-device state; the API returns an error and requires recovery/inspection, never a success. Multi-statement SQL transactions and savepoints are future work after single-transaction recovery passes. Thread and lock policies are in [RTOS_INTEGRATION.md](RTOS_INTEGRATION.md).
