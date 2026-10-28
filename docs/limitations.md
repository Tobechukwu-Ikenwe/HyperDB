# Limitations and non-goals

Current C++ execution uses in-memory tables and whole-file `HDBSNAP1` snapshots. Its durability after power loss has not been fault-injection tested. The C++ library has no transaction, multi-connection, WAL, page-checksum, or crash-recovery guarantees. The CLI serializes cooperating processes with a sidecar lock; that is not database isolation. The existing B-tree is an in-memory equality index. SQL syntax and type enforcement cover only the subset in README. No coverage threshold, sanitizer-clean claim, or general performance claim has been established. The new page-store work is not a production path until integrated and tested end to end.

Explicit non-goals: stored procedures, triggers, views, window functions, recursive CTEs, full-text search, replication, authentication and authorization, full SQL-standard conformance, JSON types, Windows support, and distributed execution. Linux and macOS are the target platforms.
