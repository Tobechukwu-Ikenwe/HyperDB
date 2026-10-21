# Development log

## 2026-10-20 — Insurance artifact provenance

Read the local PUF-like ZIPs, CSVs, and SQLite import without modification. Added `python/verify_insurance.py` to hash loose CSVs and ZIP members and compare every CSV row with the corresponding read-only SQLite table. The full verification passed for 22,059 plan rows and 2,235,761 rate rows; hashes and limitations are in `docs/DATA_PROVENANCE.md`. CMS publishes matching 2026 PUF descriptions and field dictionaries, but the original local download URL and release revision are not recorded, so exact external identity remains unverified. The third ZIP is invalid.

## 2026-10-20 — POSIX snapshot sync

Added `fsync` for the completed temporary snapshot and its containing directory after rename on POSIX systems. All ten normal CTest targets passed. This narrows the power-loss gap, but recovery has not been fault-injection tested and depends on filesystem and hardware behavior. A directory-sync failure after rename can report an error even though the new snapshot is present.

## 2026-10-20 — CLI index creation

Added `hyperdb --index SNAPSHOT TABLE COLUMN`, using the existing B-tree API and snapshot lock. The concurrent CLI test now creates an index before launching inserts; all ten normal CTest targets pass. SQL `CREATE INDEX` remains outside the supported grammar.

## 2026-10-20 — Differential SQL check and final write verification

Added a Python fixture that executes the same supported CREATE, INSERT, UPDATE, DELETE, and SELECT statements in HyperDB and SQLite. Its first run found a CLI display difference for a floating value (`4` versus `4.0`); the fixture now normalizes SQLite floating display before comparing rows. Fixed zero-match UPDATE/DELETE reporting so they return `0 rows affected` without rewriting the snapshot. All ten CTest targets pass in normal GCC, Release GCC, AddressSanitizer/UndefinedBehaviorSanitizer, and Release Clang builds. Leak detection was disabled under ptrace. GCC 15 with extra warnings still reports `-Wfree-nonheap-object` in inlined `std::variant` code; Clang with the same warning level is clean, and no sanitizer error appeared. This warning remains unresolved rather than being treated as proven harmless.

## 2026-10-20 — Concurrent CLI writers

Added a POSIX advisory `.lock` sidecar around snapshot initialization, demo save, and the CLI load/execute/save cycle. A new test launched 16 concurrent INSERT processes and verified all rows survived; nine normal CTest targets passed. The lock is not a library-wide concurrency mechanism and does not add power-loss durability.

## 2026-10-20 — SQL writes across restart

Extended the existing parser with single-table CREATE TABLE, INSERT, UPDATE, and DELETE statements. Added `execute_statement` and staged `Database::replace_rows`, which rebuilds equality indexes and remaps embeddings after deletes. The CLI can initialize an empty snapshot and saves successful writes. Eight CTest targets pass in the normal build. Separate CLI processes created a table, inserted a row, updated it, selected it, deleted it, and observed the empty table after reload. Remaining: write performance is unmeasured; no multi-statement transactions or concurrent writer locking.

## 2026-10-20 — SQL NULL predicates

Added `IS NULL` and `IS NOT NULL` to the SELECT expression AST, parser, and executor. Parser tests cover precedence and malformed input; engine tests cover NULL, non-NULL, and interaction with three-valued `OR`. All seven normal CTest targets passed. A GCC 15 Release build still emits `-Wfree-nonheap-object` while inlining `std::variant` copy assignment in both executor and test code; this is not yet proven to be a compiler false positive.

## 2026-10-20 — Versioned snapshots and CLI reload

Added `Database::save/load` with typed rows, schema metadata, equality-index definitions, and vector embeddings. Loading checks bounded fields and rebuilds indexes; a truncated snapshot is rejected. Added `--save-demo` and `--db` CLI modes to exercise save/reload in separate processes. The persistence round-trip test, including index updates after load and vector search, passed with all seven CTest targets in normal, Release, and AddressSanitizer/UndefinedBehaviorSanitizer builds. Leak detection was disabled under ptrace. The Release build with extra warnings emitted a GCC `std::variant` free-nonheap warning in the query executor; sanitizer tests did not report an error, and the warning needs separate investigation. At that milestone, SQL writes and large-file performance evidence remained.

## 2026-10-20 — Read-only query slice

Found a working lexer, in-memory table model, disconnected relational helpers, and an executor stub. The pre-existing working tree already contained substantial uncommitted restructuring and removed group-by files; none were reverted. Added an owning SELECT AST and parser, connected execution with validation, filtering, ordering, limit, and projection, and made the CLI execute a real query. Added parser and end-to-end tests and corrected CMake source wiring. `cmake --build build -j 4`, `ctest --test-dir build --output-on-failure`, and a CLI query passed. Remaining: limited SQL grammar, no persistence or joins, and no Python/C++ bridge.

## 2026-10-20 — Registration checks and demo repair

Added duplicate-column and table-registration checks; kept stored rows unchanged during queries. Numeric ordering now compares integer and real values numerically and rejects non-finite sort keys. Fixed the Streamlit result variable, limited displayed rows, and opened its SQLite database read-only. C++ build and three CTest targets passed; the Python app was not run because its model/dependency runtime was not verified. Inspected local insurance tables and archive listings, but external provenance is not established.

Added `python/inspect_insurance.py` as a read-only local schema/count example. An AddressSanitizer/UndefinedBehaviorSanitizer build passed its three CTest targets with leak detection disabled. LeakSanitizer could not run in the traced environment; this is an environment limitation rather than a reported program leak. Python modules passed syntax compilation.

Expanded the end-to-end test to cover mixed integer/real ordering, NULL sort position, and a malformed row introduced after registration. The first expanded run failed because the test still expected an empty database after adding a second table; dropping the test table fixed the setup. Normal and sanitizer CTest runs then passed all three targets.

## 2026-10-20 — Relational breadth and controlled writes

Changed registered table lookup to const and added validated `insert_row`. Added one two-table inner join with qualified/ambiguous name handling, and `COUNT`, `SUM`, `AVG`, `MIN`, `MAX`, one-column `GROUP BY`, and aggregate `HAVING`. Parser and end-to-end tests passed; representative aggregate and join outputs matched the same fixtures queried in SQLite. Remaining: aliases, outer joins, multiple grouping keys, persistence, and scalable join planning.

## 2026-10-20 — B-tree and measured equality lookup

Completed the existing B-tree declaration, connected an equality index to table registration/insertion and simple equality predicates, and added split/duplicate/index-update tests. Reorganized CMake around `hyperdb_core`. A deterministic Release workload of 100,000 rows and 200 lookups measured HyperDB scan 520.982 ms and indexed 0.428201 ms; a Python/SQLite baseline measured scan 463.048 ms and indexed 0.307 ms in the same local environment. Setup was excluded; Python/SQLite can cache prepared statements, while HyperDB reparses SQL. These numbers are one local run, not a broad speed claim. Release and sanitizer tests passed. GCC 15 initially warned about a `std::variant` assignment in aggregate extrema; retaining a pointer to the selected input value removed that assignment and the optimized rebuild had no warning.

## 2026-10-20 — Vector retrieval and Python bridge

Added exact cosine search with caller-supplied embeddings and an optional metadata filter, plus an executable example. Because SQLite C development headers are absent, added a local Python-to-C++ bridge that reads small SQLite source tables read-only and runs the parsed query in HyperDB. Streamlit now offers an explicit engine choice. The bridge fixture test passed, and a live `department_management` Spider query returned rows through C++. That live query exposed case-sensitive identifier lookup; table and column resolution was then made ASCII case-insensitive and retested. The model/Streamlit runtime and model accuracy remain unverified. The bridge is bounded to 10,000 rows per table and one million join pairs; it is not persistent storage.

## 2026-10-20 — AI flow verification

Loaded the local 60.5-million-parameter checkpoint with the available fast tokenizer files and ran `python/inference.py`; it generated `SELECT count(*) FROM head WHERE age > 56`. HyperDB returned `5` through the bridge, matching SQLite on the same Spider fixture. Streamlit's test runner loaded the page without an exception. In the full schema prompt, the model generated `SELECT count(*) FROM department WHERE age > 56`, which is invalid because `age` belongs to `head`. Changed the app to separate generation from execution and expose editable SQL. A Streamlit test then corrected the SQL and displayed the C++ result `5` without an exception. This verifies one flow, not model accuracy.
