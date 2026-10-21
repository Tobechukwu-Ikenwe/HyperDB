# Architecture

## C++ data and query path

`Database` owns named in-memory `Table` values. A table contains a `Schema` and rows of `Value` variants: NULL, 64-bit integer, double, text, or blob. `create_table`, `insert_row`, and `replace_rows` check row width and non-nullable columns. Registered tables are exposed through `const Table*`, so callers cannot mutate stored rows directly. Values otherwise use dynamic storage classes; declared type and affinity strings are metadata, not enforced conversion rules. Table and column lookup is ASCII case-insensitive; result labels keep the spelling requested in SQL.

`tokenize` yields located tokens. `parse_select` owns a `SelectStatement` AST and checks syntax without reading the database. `execute_query` resolves names, optionally builds a two-table inner join, filters rows with SQL-style three-valued boolean logic and explicit `IS NULL` tests, then executes either a plain projection or grouped aggregates. Ordering precedes limit. Results are `QueryResult {columns, rows}`. Syntax errors and most semantic errors throw exceptions; CLI and bridge callers present them. `parse_statement` also produces CREATE/INSERT/UPDATE/DELETE ASTs. `execute_statement` dispatches writes through `Database`, and the CLI saves a snapshot after a successful write.

```text
SQL → lexer → SELECT AST → table/name resolution → optional INNER JOIN → WHERE
                                                    → GROUP BY / aggregates / HAVING
                                                    → ORDER BY → LIMIT → projection → rows
Database → Table / Schema / Rows ──────────────────────────────────────────────↑
```

`QueryExecutor/aggregate.cpp` implements `COUNT`, `SUM`, `AVG`, `MIN`, and `MAX`, one grouping key, and aggregate-to-literal `HAVING`. Numeric `SUM` and `AVG` return doubles; NULLs are skipped except by `COUNT(*)`. `INNER JOIN` currently uses a nested loop and qualified column names. Ambiguous unqualified names are rejected. These choices keep the supported subset explicit; they are not a general SQL planner.

`Database::create_index` builds a B-tree from the current rows. Inserts update it. The executor uses an index for a simple equality comparison or an equality term under `AND`; it still evaluates the full predicate on candidate rows. Indexes require one compatible non-NULL value category per column (numeric, text, or blob) and do not accelerate range or join predicates. Inserts append stable row IDs. UPDATE preserves IDs; DELETE compacts rows and remaps surviving embeddings. Both rebuild equality indexes from validated replacement rows. `OrderBy/order_by.cpp` sorts copies of query rows, so SQL reads do not reorder stored tables.

`Database::create_vector_index`, `set_embedding`, and `vector_search` attach caller-supplied fixed-dimension embeddings to table row IDs. Search computes exact cosine similarity and can apply one metadata `Condition` before scoring. It does not generate embeddings and has no approximate index. `examples/vector_search.cpp` demonstrates the API with illustrative vectors.

`Database::save` writes a versioned binary snapshot to a temporary sibling, syncs it on POSIX systems, renames it into place, then syncs the containing directory. `Database::load` validates bounded fields and indexes before returning a database; B-trees are rebuilt from rows. The CLI can initialize an empty snapshot, execute one statement against it, create an equality index with `--index`, and save successful writes. It can also save its demo table. This is a whole-database checkpoint without a WAL or multi-statement transaction; crash recovery has not been fault-injection tested and depends on filesystem and hardware behavior. On POSIX systems, the CLI holds an advisory sidecar file lock across load, execution, and save; C++ API users must coordinate their own writers. Each CLI write reloads and rewrites the entire file. The loader caps files at 1 GiB, tables at 1 million rows and 512 columns, fields at 16 MiB, and vector dimensions at 100,000. The Python bridge still imports SQLite tables per query.

`filter.cpp`, `filter_table.cpp`, `select.cpp`, `from/from.cpp`, and `Limit/limit.cpp` are older helper APIs, tested separately. The AST executor uses its own predicate and projection path; the vector metadata filter calls the simple `matches` helper. `GroupBy/database.h` is a stale invalid header excluded from CMake. The previously tracked group-by files were already deleted in the initial working tree; the active aggregate implementation is `QueryExecutor/aggregate.cpp`.

## Python and model boundary

`python/model.py` is a Spider/T5 training script; `python/inference.py` loads a checkpoint. `python/app.py` generates SQL from a question and Spider schema, asks the user to review or edit it, then lets the user choose SQLite or HyperDB C++ for execution. SQLite is opened read-only. The HyperDB option calls `python/hyperdb_bridge.py`, which asks `hyperdb_bridge` which tables the parsed query uses, copies those tables from read-only SQLite through a versioned local binary protocol, and receives JSON result rows from the C++ executor. The bridge accepts at most two tables, 10,000 rows per table, and one million candidate join pairs. It is a per-query import, not durable C++ storage or a network service. Unsupported model SQL fails with an explicit error. The local checkpoint and Streamlit review/run path were exercised, including a corrected model query; model accuracy has not been measured.

## Data and provenance

`data/hyperdb.db` contains `trips`, `plan_attributes`, and `rates`. The two insurance tables have 22,059 and 2,235,761 rows with 151 and 20 columns. `python/verify_insurance.py` confirms that each loose CSV matches its ZIP member byte for byte and that every CSV row matches the corresponding SQLite row in rowid order. The source download revision and original import command remain unrecorded. All imported insurance fields are SQLite `TEXT`, including numeric-looking rates. See [DATA_PROVENANCE.md](DATA_PROVENANCE.md) for hashes, CMS documentation links, scope, and caveats. The C++ CLI does not load these large tables.
