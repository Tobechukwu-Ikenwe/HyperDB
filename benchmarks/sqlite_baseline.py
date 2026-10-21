"""SQLite baseline for the benchmark.cpp deterministic equality workload."""

import sqlite3
import sys
import time


def main() -> None:
    rows = int(sys.argv[1]) if len(sys.argv) > 1 else 100_000
    iterations = int(sys.argv[2]) if len(sys.argv) > 2 else 200
    if rows <= 0 or iterations <= 0:
        raise ValueError("Rows and iterations must be positive")
    connection = sqlite3.connect(":memory:")
    connection.execute("CREATE TABLE Items (id INTEGER, payload TEXT)")
    connection.executemany(
        "INSERT INTO Items VALUES (?, ?)",
        ((i, f"item{i}") for i in range(rows)),
    )

    def measure() -> float:
        start = time.perf_counter()
        for _ in range(iterations):
            result = connection.execute(
                "SELECT payload FROM Items WHERE id = ?", (rows - 1,)
            ).fetchall()
            if result != [(f"item{rows - 1}",)]:
                raise RuntimeError("Benchmark result mismatch")
        return (time.perf_counter() - start) * 1000

    scan_ms = measure()
    connection.execute("CREATE INDEX items_id_idx ON Items(id)")
    index_ms = measure()
    print(f"rows={rows} iterations={iterations}")
    print(f"scan_ms={scan_ms:.3f} index_ms={index_ms:.3f}")


if __name__ == "__main__":
    main()
