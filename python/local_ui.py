"""Dependency-free local SQL interface for HyperDB."""

import argparse
import json
import re
import subprocess
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PAGE = Path(__file__).resolve().parent / "ui" / "index.html"
DATABASES = ROOT / "data" / "ui"
CLI = ROOT / "build" / "hyperdb"
NAME = re.compile(r"[A-Za-z0-9_-]{1,64}\Z")


def snapshot(name: str) -> Path:
    if not isinstance(name, str) or not NAME.fullmatch(name):
        raise ValueError("Database name must use 1–64 letters, digits, _ or -")
    return DATABASES / f"{name}.hdb"


def command(*args: str, timeout: int = 30) -> tuple[str, float]:
    start = time.perf_counter()
    try:
        result = subprocess.run(args, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired as error:
        raise ValueError("Operation timed out") from error
    elapsed = (time.perf_counter() - start) * 1000
    if result.returncode:
        raise ValueError(result.stderr.strip() or "HyperDB command failed")
    return result.stdout, elapsed


def ensure_database(path: Path) -> None:
    if path.exists():
        return
    DATABASES.mkdir(parents=True, exist_ok=True)
    try:
        command(str(CLI), "--init", str(path))
    except ValueError:
        # Another local request may have initialized the same database first.
        if not path.exists():
            raise


class Handler(BaseHTTPRequestHandler):
    def send_json(self, status: int, data: dict) -> None:
        body = json.dumps(data).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:
        if self.path == "/":
            body = PAGE.read_bytes()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        elif self.path == "/api/databases":
            names = sorted(path.stem for path in DATABASES.glob("*.hdb")) if DATABASES.exists() else []
            self.send_json(200, {"databases": names})
        else:
            self.send_json(404, {"error": "Unknown page"})

    def do_POST(self) -> None:
        try:
            if self.headers.get("Content-Type", "").split(";", 1)[0] != "application/json":
                raise ValueError("Expected application/json")
            length = int(self.headers.get("Content-Length", "0"))
            if length < 1 or length > 65536:
                raise ValueError("Request must be between 1 and 65536 bytes")
            data = json.loads(self.rfile.read(length))
            if not isinstance(data, dict):
                raise ValueError("Expected a JSON object")
            name = data.get("database", "")
            path = snapshot(name)
            if not CLI.is_file():
                raise ValueError("Build HyperDB first: cmake --build build")
            if self.path == "/api/query":
                sql = data.get("sql", "")
                if not isinstance(sql, str) or not sql.strip() or len(sql) > 16384:
                    raise ValueError("Enter one SQL statement of at most 16384 characters")
                ensure_database(path)
                output, elapsed = command(str(CLI), "--db-json", str(path), sql)
                self.send_json(200, {"result": json.loads(output), "elapsed_ms": elapsed})
            else:
                self.send_json(404, {"error": "Unknown action"})
        except (ValueError, OSError, json.JSONDecodeError) as error:
            self.send_json(400, {"error": str(error)})


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    print(f"HyperDB workbench: http://127.0.0.1:{server.server_port}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
