"""Inspect the local insurance SQLite import without modifying or scanning all rows."""

import sqlite3
from pathlib import Path


DB = Path(__file__).resolve().parents[1] / "data/hyperdb.db"


def main() -> None:
    with sqlite3.connect(f"file:{DB}?mode=ro", uri=True) as connection:
        connection.execute("PRAGMA query_only = ON")
        for table in ("plan_attributes", "rates"):
            columns = [row[1] for row in connection.execute(f'PRAGMA table_info("{table}")')]
            count = connection.execute(f'SELECT COUNT(*) FROM "{table}"').fetchone()[0]
            print(f"{table}: {count:,} rows, {len(columns)} columns")
            print("  example fields:", ", ".join(columns[:12]))


if __name__ == "__main__":
    main()
