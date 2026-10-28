#!/usr/bin/env python3
"""Report line coverage for hyperdb_core using GCC's JSON gcov output."""

import argparse
import gzip
import json
import subprocess
import sys
import tempfile
from pathlib import Path


def collect(build_dir: Path, source_root: Path) -> dict[Path, dict[int, int]]:
    objects = build_dir / "CMakeFiles" / "hyperdb_core.dir"
    data_files = sorted(objects.rglob("*.gcda"))
    if not data_files:
        raise RuntimeError("No core .gcda files found; build and run the coverage preset first")

    counts: dict[Path, dict[int, int]] = {}
    with tempfile.TemporaryDirectory(prefix="hyperdb-gcov-") as temporary:
        temp_path = Path(temporary)
        for data_file in data_files:
            relative = data_file.relative_to(objects)
            source = source_root / str(relative).removesuffix(".gcda")
            subprocess.run(
                ["gcov", "--json-format", "--object-directory", str(data_file), str(source)],
                cwd=temp_path,
                stdout=subprocess.DEVNULL,
                check=True,
            )
            report_files = list(temp_path.glob("*.gcov.json.gz"))
            if len(report_files) != 1:
                raise RuntimeError(f"Expected one gcov report for {source}")
            report_file = report_files[0]
            with gzip.open(report_file, "rt", encoding="utf-8") as stream:
                report = json.load(stream)
            report_file.unlink()
            for item in report["files"]:
                path = Path(item["file"]).resolve()
                if not path.is_relative_to(source_root / "src"):
                    continue
                if path.is_relative_to(source_root / "src" / "shell"):
                    continue
                lines = counts.setdefault(path, {})
                for line in item["lines"]:
                    number = line["line_number"]
                    lines[number] = lines.get(number, 0) + line["count"]
    return counts


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=Path("build/coverage"))
    parser.add_argument("--min-line", type=float, default=0.0)
    args = parser.parse_args()
    source_root = Path(__file__).resolve().parents[1]
    counts = collect(args.build.resolve(), source_root)
    total_lines = sum(len(lines) for lines in counts.values())
    covered_lines = sum(sum(count > 0 for count in lines.values()) for lines in counts.values())
    if total_lines == 0:
        raise RuntimeError("No instrumented source lines found")
    ratio = covered_lines / total_lines
    print(f"src/ line coverage: {covered_lines}/{total_lines} ({ratio:.1%})")
    for path, lines in sorted(counts.items()):
        if not lines:
            continue
        covered = sum(count > 0 for count in lines.values())
        print(f"  {path.relative_to(source_root)}: {covered}/{len(lines)}")
    if ratio < args.min_line:
        print(f"Line coverage is below the required {args.min_line:.1%}", file=sys.stderr)
        raise SystemExit(1)


if __name__ == "__main__":
    main()
