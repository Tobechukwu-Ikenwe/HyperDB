"""Smoke-test the simple local SQL UI; pass its base URL as argv[1]."""

import json
from pathlib import Path
import sys
from urllib.error import HTTPError
from urllib.request import Request, urlopen
from uuid import uuid4


ROOT = Path(__file__).resolve().parents[1]
BASE = sys.argv[1] if len(sys.argv) > 1 else "http://127.0.0.1:8765"


def post(path: str, data: dict) -> dict:
    request = Request(BASE + path, json.dumps(data).encode(),
                      {"Content-Type": "application/json"}, method="POST")
    with urlopen(request, timeout=125) as response:
        return json.load(response)


def main() -> None:
    name = "smoke_" + uuid4().hex
    snapshot = ROOT / "data" / "ui" / f"{name}.hdb"
    try:
        with urlopen(BASE, timeout=5) as response:
            page = response.read()
            assert b"Choose a database, run SQL" in page
            assert b"snapshot" not in page.lower()
        post("/api/query", {"database": name,
                            "sql": "CREATE TABLE people (id INTEGER NOT NULL, name TEXT)"})
        post("/api/query", {"database": name,
                            "sql": "INSERT INTO people VALUES (1, 'Ada\tLovelace')"})
        result = post("/api/query", {"database": name,
                                     "sql": "SELECT id, name FROM people WHERE id = 1"})["result"]
        assert result == {"kind": "query", "columns": ["id", "name"],
                          "rows": [[1, "Ada\tLovelace"]]}, result
        result = post("/api/query", {"database": name,
                                     "sql": "UPDATE people SET name = 'Grace' WHERE id = 1"})["result"]
        assert result == {"kind": "write", "rows_affected": 1, "modified": True}, result
        try:
            post("/api/query", {"database": name, "sql": "SELECT missing FROM people"})
            raise AssertionError("Expected SQL error")
        except HTTPError as error:
            assert "Unknown column" in error.read().decode()
        print("UI smoke test passed")
    finally:
        snapshot.unlink(missing_ok=True)
        Path(str(snapshot) + ".lock").unlink(missing_ok=True)


if __name__ == "__main__":
    main()
