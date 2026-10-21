"""Load the tables named by a query from read-only SQLite into the C++ engine.

The binary protocol is local and versioned. It transfers values; SQL execution
and result production happen in HyperDB. No SQLite writes are permitted here.
"""

import json
import os
from contextlib import closing
from pathlib import Path
import sqlite3
import struct
import subprocess


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BINARY = ROOT / "build/hyperdb_bridge"
MAX_ROWS = 10_000


def _binary() -> str:
    return os.environ.get("HYPERDB_BRIDGE_BIN", str(DEFAULT_BINARY))


def _run(mode: str, sql: str, payload: bytes | None = None) -> str:
    try:
        process = subprocess.run(
            [_binary(), mode, sql], input=payload, capture_output=True,
            timeout=60, check=False,
        )
    except subprocess.TimeoutExpired as error:
        raise RuntimeError("HyperDB bridge timed out") from error
    if process.returncode:
        raise RuntimeError(process.stderr.decode("utf-8", "replace").strip())
    return process.stdout.decode("utf-8")


def _u32(value: int) -> bytes:
    return struct.pack("<I", value)


def _text(value: str | bytes) -> bytes:
    data = value.encode("utf-8") if isinstance(value, str) else value
    return _u32(len(data)) + data


def _value(value: object) -> bytes:
    if value is None:
        return b"\x00"
    if isinstance(value, int):
        return b"\x01" + struct.pack("<q", value)
    if isinstance(value, float):
        return b"\x02" + struct.pack("<d", value)
    if isinstance(value, str):
        return b"\x03" + _text(value)
    if isinstance(value, bytes):
        return b"\x04" + _text(value)
    raise TypeError(f"Unsupported SQLite value type: {type(value).__name__}")


def execute_sqlite_query(db_path: str | Path, sql: str) -> dict:
    """Execute supported SQL in HyperDB using up to two SQLite source tables."""
    names = json.loads(_run("--tables", sql))
    if not 1 <= len(names) <= 2:
        raise RuntimeError("HyperDB bridge supports one or two tables")
    payload = bytearray(b"HDB1" + _u32(len(names)))
    path = Path(db_path).resolve()
    with closing(sqlite3.connect(path.as_uri() + "?mode=ro", uri=True)) as connection:
        connection.execute("PRAGMA query_only = ON")
        for name in names:
            quoted = '"' + name.replace('"', '""') + '"'
            cursor = connection.execute(f"SELECT * FROM {quoted}")
            columns = [item[0] for item in cursor.description]
            rows = cursor.fetchmany(MAX_ROWS + 1)
            if len(rows) > MAX_ROWS:
                raise RuntimeError(f"Table {name} exceeds the {MAX_ROWS:,}-row bridge limit")
            payload += _text(name) + _u32(len(columns)) + _u32(len(rows))
            for column in columns:
                payload += _text(column)
            for row in rows:
                for value in row:
                    payload += _value(value)
    return json.loads(_run("--execute", sql, bytes(payload)))
