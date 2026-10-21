# Design decisions

## 1. Bounded SQL grammar with an owning AST

Problem: the original lexer and table model had no end-to-end executor. We parse an explicit `SELECT` subset into an owning AST, then resolve names against the database. This separates syntax from semantic validation and makes precedence testable. A broad SQLite-compatible parser or an external parser dependency would have added scope before execution semantics were settled. Revisit when a concrete conformance suite demands more grammar.

## 2. Controlled table mutation with dynamic values

`Database` returns only `const Table*` and validates `create_table`/`insert_row`. This prevents routine post-registration row corruption while keeping table construction simple. Declared type and affinity strings remain metadata: integer and real can coexist, and there is no implicit coercion. A private `Table` storage class and stronger type constraints may be appropriate if SQL compatibility or concurrent mutation requirements grow. Empty/malformed rows and NULL in required columns are rejected now.

## 3. Explicit relational subset

The join path supports two tables and one equality condition, using a nested loop. This is simple and correct for small fixtures and keeps name resolution visible; a hash/index join would matter for larger relations. Aggregation supports one grouping key and standard aggregates, with NULL skipped by column aggregates. `SUM` and `AVG` return doubles. Arbitrary expressions, aliases, outer joins, and multiple group keys need a more general planner and semantic model before implementation.

## 4. B-tree only for compatible equality predicates

The existing B-tree header was a declaration without implementation. We completed insertion/search. Inserts append row IDs; UPDATE preserves them, while DELETE compacts rows and rebuilds indexes. The executor uses it for a simple equality term, then checks the full predicate. An indexed column must have one non-NULL category to avoid changing type-error behavior between scan and index paths. Range scanning, composite indexes, cost-based selection, and join indexing are future work. A deterministic Release benchmark and SQLite baseline are provided; no general speed ranking follows from one workload.

## 5. Exact vector search with caller-owned embeddings

Embeddings attach to database row IDs in a fixed-dimension `VectorIndex`. Cosine search scans embeddings exactly and can prefilter by one metadata condition. This demonstrates retrieval without inventing a model or hiding external costs. Approximate nearest-neighbor structures are alternatives only after data scale and recall/latency targets are measured. Embeddings persist in whole-database snapshots.

## 6. Local Python-to-C++ bridge while SQLite remains available

The saved T5 checkpoint was trained on Spider SQL, which is broader than HyperDB's grammar. The app therefore offers both engines, displays generated SQL for review/edit before execution, and reports unsupported C++ queries. A live test showed the model choosing an `age` column from the wrong table; the review step lets a user correct such errors. Python reads referenced SQLite tables in read-only mode and sends typed values through a versioned local process protocol; C++ executes and returns JSON. This avoids an unavailable SQLite C development dependency and makes the engine boundary testable. It copies data per query and caps table/join sizes, so a persistent binding or service may be warranted later. Model accuracy and training provenance remain unverified.

## 7. Stable ordering and explicit errors

Queries sort local result copies, preserving stored row order. Numeric integers and doubles compare by numeric value; non-finite values are rejected where ordering, grouping, or arithmetic would be undefined. `IS NULL` and `IS NOT NULL` return a Boolean even for NULL, while ordinary comparisons with NULL remain unknown. Parser errors carry one-based source positions; schema/type errors use exceptions. Returning empty rows for unknown names or incompatible values was considered but would hide mistakes.

## 8. Versioned whole-database snapshots

The in-memory engine needed restartable state before SQL writes and a storage manager. A bounded binary snapshot stores schemas, typed values, equality-index definitions, and embeddings; loading rebuilds B-trees to keep their internal layout out of the file format. Saving writes a temporary sibling, syncs the file on POSIX systems, renames it, then syncs the containing directory. Ordinary failures before rename leave the old snapshot in place; if directory sync fails after rename, the caller gets an error but must inspect which snapshot is present. A WAL or page store would support incremental writes and stronger transaction semantics, but adds recovery complexity before the mutation API is settled. The format is versioned and rejects malformed input. The file and directory sync reduce power-loss risk on POSIX filesystems, but crash fault testing and hardware-level guarantees remain unverified. Revisit when transactions, large data, or concurrent writers are required.

## 9. Minimal SQL writes through the existing parser

The engine needed a real create → insert → query → restart workflow. `parse_statement` extends the existing lexer and parser with single-table `CREATE TABLE`, `INSERT`, `UPDATE`, and `DELETE`; `execute_statement` applies semantic checks using `Database` and reuses the SELECT predicate evaluator. Literal-only assignments and one-row INSERT keep mutation semantics explicit. `Database::replace_rows` stages a validated replacement table, rebuilds equality indexes, remaps surviving embeddings, then swaps state so a failed update does not leave partial changes. DELETE compacts row IDs. An incremental index-maintenance path would reduce update cost but is more complex with the current B-tree, which has no delete operation. The CLI saves after each successful write. There are no multi-statement transactions, library-level concurrent writer locks, SQL index DDL, type coercions, or primary-key uniqueness guarantees; revisit when workloads and transactional requirements are defined.

## 10. POSIX CLI snapshot lock

Separate CLI processes could otherwise read the same snapshot and silently overwrite each other. Each snapshot command now holds an exclusive advisory lock on a persistent `.lock` sidecar during the load/execute/save cycle. A concurrency test launches 16 inserts and verifies that all survive. The lock is CLI-only, advisory, and POSIX-specific; library callers and non-cooperating programs must coordinate separately. The sidecar remains after use because deleting it can create a race between processes locking different inodes. A transactional storage manager could replace this whole-file serialization when the project needs concurrent readers and writers.
