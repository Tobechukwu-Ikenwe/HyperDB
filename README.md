# HyperDB

HyperDB is an educational C++ database engine with in-memory query execution, versioned file snapshots, and an experimental text-to-SQL Python demo. The C++ engine parses and executes a documented SQL subset, including `SELECT` and single-table writes. The storage API provides snapshots. It also has a B-tree equality index, exact cosine vector search over caller-supplied embeddings, and a local Python bridge that imports small read-only SQLite tables for C++ execution. The model demo can choose SQLite or HyperDB for generated SQL.

## Build and run the C++ engine

Requires a C++20 compiler and CMake 3.16 or newer.

```sh
cmake -S . -B build
cmake --build build -j 4
ctest --test-dir build --output-on-failure
./build/hyperdb
./build/hyperdb 'SELECT Name FROM Department WHERE Budget_in_Billions > 5 ORDER BY Name ASC LIMIT 1'
./build/hyperdb_vector_demo
./build/hyperdb --save-demo /tmp/hyperdb-demo.hdb
./build/hyperdb --db /tmp/hyperdb-demo.hdb 'SELECT Name FROM Department ORDER BY Name'
./build/hyperdb --init /tmp/hyperdb-people.hdb
./build/hyperdb --db /tmp/hyperdb-people.hdb 'CREATE TABLE people (id INTEGER NOT NULL, name TEXT)'
./build/hyperdb --db /tmp/hyperdb-people.hdb "INSERT INTO people VALUES (1, 'Ada')"
./build/hyperdb --db /tmp/hyperdb-people.hdb "UPDATE people SET name = 'Ada Lovelace' WHERE id = 1"
./build/hyperdb --db /tmp/hyperdb-people.hdb 'SELECT id, name FROM people'
./build/hyperdb --index /tmp/hyperdb-people.hdb people id
```

The executable creates a three-row `Department` table for demonstration. Pass one SQL statement as a shell argument; errors print to stderr and return a nonzero exit code. Use `--init` to create an empty snapshot, `--save-demo` to save the sample table, and `--db` to execute one statement per process. `--init` refuses to overwrite an existing file; successful writes through `--db` save the changed snapshot.

The C++ API exposes `execute_statement`, `Database::save(path)`, and `Database::load(path)` for other tables. Snapshots include schemas, rows, equality-index definitions, and attached embeddings. UPDATE and DELETE rebuild equality indexes and remap surviving embeddings; each CLI write rewrites the full snapshot. The current format caps a file at 1 GiB, each table at 1 million rows and 512 columns, each text/blob field at 16 MiB, and vector dimensions at 100,000.

## Local SQL workbench

Run `cmake --build build -j 4`, then `python3 python/local_ui.py` and open `http://127.0.0.1:8765`. Enter a database name, enter one SQL statement, and run it. New databases are created on first use; HyperDB saves and reloads their state behind the interface. Results and errors appear as readable text. The UI runs locally without extra Python packages. The CLI's `--db-json` mode supplies typed results to the UI. Run `./build/hyperdb_workloads` in a terminal for the developer benchmark suite. Use `python3 tests/ui_smoke.py` while the server is running to check the UI API.

Snapshot CLI modes (`--init`, `--save-demo`, `--db`, and `--index`) require POSIX file locking and leave a `.lock` sidecar file. The CLI holds the lock across load, execution, and save, so concurrent CLI writers serialize. The C++ library itself has no cross-process lock. Saving writes and syncs a temporary sibling on POSIX systems, renames it into place, then syncs the directory. There is no write-ahead log or multi-statement transaction; power-loss recovery has not been fault-injection tested, and guarantees depend on the filesystem and hardware.

Supported grammar: `SELECT *` or column and aggregate projections, `FROM` one table with an optional two-table `INNER JOIN ... ON a = b`, optional `WHERE` comparisons joined by `AND`, `OR`, `NOT` and parentheses, optional one-column `GROUP BY` and aggregate `HAVING`, optional one-column `ORDER BY` with `ASC` or `DESC`, and optional nonnegative integer `LIMIT`. Aggregates are `COUNT`, `SUM`, `AVG`, `MIN`, and `MAX`; `COUNT(*)` is supported. Comparisons support `=`, `==`, `!=`, `<>`, `<`, `<=`, `>`, `>=`; `IS NULL` and `IS NOT NULL` test for missing values. Table and column lookup is ASCII case-insensitive. `NULL` comparisons yield unknown and do not select rows. Quoted identifiers and strings use the existing lexer. This is not full SQL or SQLite compatibility. The write grammar supports `CREATE TABLE name (column type [NOT NULL], ...)`, one-row `INSERT INTO name [(columns)] VALUES (literals)`, `UPDATE name SET column = literal [, ...] [WHERE predicate]`, and `DELETE FROM name [WHERE predicate]`. Omitted INSERT columns take their schema defaults (NULL unless set through C++). Types are metadata; no coercion or primary-key uniqueness is enforced. Table aliases, outer joins, multiple group keys, arithmetic, SQL index DDL, and multi-statement transactions are not implemented. The C++ engine executes on in-memory tables; the bridge imports a fresh copy per query and does not use snapshots.

`Database::create_index(table, column)` builds a B-tree; `hyperdb --index SNAPSHOT TABLE COLUMN` creates and saves one through the CLI. Simple equality predicates can use it. `Database::create_vector_index`, `set_embedding`, and `vector_search` provide exact cosine search and a simple metadata filter. Embeddings are supplied by callers; HyperDB does not generate them or provide approximate nearest-neighbor search.

## Python experiment

`python/app.py` is a Streamlit demo using a local checkpoint under `hyperdb-text2sql/` and the `department_management` Spider SQLite fixture. It needs compatible Python packages (`streamlit`, `transformers`, `torch`, `pandas`, and tokenizer dependencies) in the active environment. Run from the repository root with `streamlit run python/app.py`. Choose SQLite or HyperDB C++ as the execution engine. HyperDB uses `build/hyperdb_bridge`, imports at most 10,000 rows per referenced table, and rejects joins above one million candidate pairs. Generate SQL, review or edit it, then click **Run SQL**. Generated SQL may be wrong or outside HyperDB's grammar; the app reports errors and shows up to 100 rows. The checkpoint loads and the tested review/run flow works, but model accuracy has not been measured.

For a model-free bridge example, build the project and run `PYTHONPATH=python python3 -c "from hyperdb_bridge import execute_sqlite_query; print(execute_sqlite_query('data/text2sql/spider_data/database/department_management/department_management.sqlite', 'SELECT Name FROM department LIMIT 2'))"`.

## Benchmarks

Build a Release configuration and run the same deterministic equality workload in both engines:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j 4
./build-release/hyperdb_benchmark 100000 200
python3 benchmarks/sqlite_baseline.py 100000 200
```

These report elapsed milliseconds for repeated scan and indexed lookups. Python/SQLite uses cached prepared statements while HyperDB parses each query, and setup time is excluded. Treat results as workload-specific measurements, not a general engine ranking.

Run `./build/hyperdb_workloads` for a broader HyperDB suite covering parsing, scans, sorting, grouping, index construction and lookup, joins, vector search, writes, and snapshot I/O. See [benchmark notes](docs/BENCHMARKS.md) for measurements and the executor changes they motivated.

## Project documents

- [Roadmap](docs/ROADMAP.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Design decisions](docs/DESIGN_DECISIONS.md)
- [Learning guide](docs/LEARNING_GUIDE.md)
- [Development log](docs/DEVELOPMENT_LOG.md)
- [Insurance data provenance](docs/DATA_PROVENANCE.md)

The [development log](docs/DEVELOPMENT_LOG.md) records one local benchmark run and its limits.
