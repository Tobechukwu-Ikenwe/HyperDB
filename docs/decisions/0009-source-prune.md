# ADR 0009: Remove unreferenced source files

Status: accepted, 2026-10-29.

## Context

The source tree contains files left from the early in-memory prototype. Some are not part of the build and have no callers. Others are legacy helpers with tests or live call sites and must be retained to preserve behavior.

## Decision

Remove `GroupBy/database.h` (invalid, unreferenced header), `query.h` (empty), `select.cpp` (uncalled `select_column` function), and `from/from.h` plus `from/from.cpp` (an uncalled wrapper around `Database::find_table`). Remove the two unused `.cpp` entries from CMake. Keep the tested `Limit` and `filter_table` APIs; keep `filter.cpp` because vector metadata search calls `matches`.

## Verification

Search source, tests, examples, fuzz targets, and benchmarks for call sites and includes; then run the Debug and Release builds and test suites. No SQL behavior or public Database API changes are intended.
