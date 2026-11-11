#!/usr/bin/env python3
"""Fail if CTest did not register integration tests required by CI."""

import argparse
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inventory", type=Path)
    parser.add_argument("required", nargs="+")
    args = parser.parse_args()

    data = json.loads(args.inventory.read_text(encoding="utf-8"))
    registered = {test["name"] for test in data.get("tests", [])}
    missing = sorted(
        required
        for required in args.required
        if not any(name == required or name.startswith(required + "/") for name in registered)
    )
    if missing:
        parser.error("CTest did not register required tests: " + ", ".join(missing))
    print("Required CTest entries registered: " + ", ".join(args.required))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
