"""Compare HyperDB's supported write/query slice against SQLite on one fixture."""

import os
from pathlib import Path
import sqlite3
import subprocess
import tempfile


def hyperdb(snapshot: str, sql: str) -> list[list[str]]:
    result = subprocess.run(
        [os.environ["HYPERDB_CLI_BIN"], "--db", snapshot, sql],
        capture_output=True,
        text=True,
        check=True,
    )
    if not sql.startswith("SELECT"):
        assert result.stdout.startswith("OK ("), result.stdout
        return []
    lines = result.stdout.splitlines()
    return [line.split("\t") for line in lines[1:]]


def render(value: object) -> str:
    if isinstance(value, float):
        return format(value, "g")  # Match the CLI's default stream display.
    return "NULL" if value is None else str(value)


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="hyperdb-sqlite-test-") as directory:
        snapshot = str(Path(directory) / "fixture.hdb")
        subprocess.run([os.environ["HYPERDB_CLI_BIN"], "--init", snapshot], check=True)
        sqlite = sqlite3.connect(":memory:")
        statements = [
            "CREATE TABLE items (id INTEGER NOT NULL, name TEXT, amount REAL)",
            "INSERT INTO items VALUES (1, 'alpha', 2.5)",
            "INSERT INTO items (name, id) VALUES ('beta', 2)",
            "INSERT INTO items VALUES (3, 'gamma', 1.5)",
            "UPDATE items SET amount = 4.0 WHERE id = 2 OR name = 'missing'",
            "DELETE FROM items WHERE amount < 2",
        ]
        for sql in statements:
            sqlite.execute(sql)
            hyperdb(snapshot, sql)
        queries = [
            "SELECT id, name, amount FROM items WHERE amount IS NOT NULL ORDER BY id",
            "SELECT id FROM items WHERE amount IS NULL OR id = 2 ORDER BY id",
            "SELECT COUNT(*), SUM(amount) FROM items",
        ]
        for sql in queries:
            expected = [[render(value) for value in row] for row in sqlite.execute(sql)]
            actual = hyperdb(snapshot, sql)
            assert actual == expected, (sql, expected, actual)
        print("SQLite conformance fixture passed.")


if __name__ == "__main__":
    main()
