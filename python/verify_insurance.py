"""Read-only integrity check for the local insurance CSV, ZIP, and SQLite copies."""

import csv
import hashlib
from itertools import zip_longest
from pathlib import Path
import sqlite3
from zipfile import ZipFile


DATA = Path(__file__).resolve().parents[1] / "data"
TABLES = {
    "plan-attributes-puf": "plan_attributes",
    "rate-puf": "rates",
}


def sha256(stream) -> str:
    digest = hashlib.sha256()
    while chunk := stream.read(1024 * 1024):
        digest.update(chunk)
    return digest.hexdigest()


def verify(stem: str, table: str, connection: sqlite3.Connection) -> None:
    csv_path = DATA / f"{stem}.csv"
    zip_path = DATA / f"{stem}.zip"
    with csv_path.open("rb") as stream:
        loose_hash = sha256(stream)
    with ZipFile(zip_path) as archive:
        with archive.open(f"{stem}.csv") as stream:
            archived_hash = sha256(stream)
    if loose_hash != archived_hash:
        raise ValueError(f"{stem}: loose CSV differs from ZIP member")

    columns = [row[1] for row in connection.execute(f'PRAGMA table_info("{table}")')]
    with csv_path.open(newline="", encoding="utf-8-sig") as stream:
        reader = csv.reader(stream)
        header = next(reader)
        if header != columns:
            raise ValueError(f"{stem}: CSV header differs from SQLite columns")
        database_rows = connection.execute(f'SELECT * FROM "{table}" ORDER BY rowid')
        count = 0
        for count, (csv_row, database_row) in enumerate(
            zip_longest(reader, database_rows), 1
        ):
            if csv_row is None or database_row is None or tuple(csv_row) != database_row:
                raise ValueError(f"{stem}: CSV/SQLite mismatch at data row {count}")
    print(f"{stem}: {count:,} rows, {len(header)} columns, SHA-256 {loose_hash}; copies match")


def main() -> None:
    database = DATA / "hyperdb.db"
    with sqlite3.connect(f"file:{database}?mode=ro", uri=True) as connection:
        connection.execute("PRAGMA query_only = ON")
        for stem, table in TABLES.items():
            verify(stem, table, connection)


if __name__ == "__main__":
    main()
