"""Exercise the persisted CLI with concurrent writers."""

import os
from pathlib import Path
import subprocess
import tempfile


def run(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [os.environ["HYPERDB_CLI_BIN"], *args],
        text=True,
        capture_output=True,
        check=True,
    )


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="hyperdb-cli-test-") as directory:
        snapshot = str(Path(directory) / "people.hdb")
        run("--init", snapshot)
        duplicate = subprocess.run(
            [os.environ["HYPERDB_CLI_BIN"], "--init", snapshot],
            text=True,
            capture_output=True,
        )
        assert duplicate.returncode != 0 and "already exists" in duplicate.stderr
        run("--db", snapshot, "CREATE TABLE people (id INTEGER NOT NULL)")
        run("--index", snapshot, "people", "id")
        processes = [
            subprocess.Popen(
                [os.environ["HYPERDB_CLI_BIN"], "--db", snapshot,
                 f"INSERT INTO people VALUES ({i})"],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            for i in range(16)
        ]
        for process in processes:
            out, err = process.communicate(timeout=15)
            assert process.returncode == 0, (out, err)
        result = run("--db", snapshot, "SELECT id FROM people ORDER BY id")
        assert result.stdout.splitlines() == ["id", *map(str, range(16))], result.stdout
        print("CLI concurrency tests passed.")


if __name__ == "__main__":
    main()
