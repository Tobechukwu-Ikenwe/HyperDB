# Limitations and non-goals

Current C++ SQL execution uses in-memory tables and whole-file `HDBSNAP1` snapshots. Its durability after power loss has not been fault-injection tested. That path has no transaction, multi-connection, WAL, page-checksum, or crash-recovery guarantees. The CLI serializes cooperating processes with a sidecar lock; that is not database isolation. The existing B-tree is an in-memory equality index. SQL syntax and type enforcement cover only the subset in README. The new page-store work is not a production path until integrated and tested end to end. The local 85% line-coverage gate passes, but CI has not run remotely and no general performance result is claimed.

The new page codec and disk manager verify checksums and page IDs, and an allocation sync-failure test passes. They are not wired into SQL execution. There is no free-space map, overflow storage, buffer pool, WAL-before-data enforcement, or directory sync on new file creation yet. A page write is not a transaction commit.

Explicit non-goals: stored procedures, triggers, views, window functions, recursive CTEs, full-text search, replication, authentication and authorization, full SQL-standard conformance, JSON types, Windows support, and distributed execution. Linux and macOS are the target platforms.
