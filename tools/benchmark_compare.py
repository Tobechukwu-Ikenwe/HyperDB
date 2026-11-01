#!/usr/bin/env python3
"""Run two HyperDB workload binaries repeatedly and compare per-operation medians."""

import argparse
import json
import math
import statistics
import subprocess
from pathlib import Path

REPETITIONS = 7
MIN_BASELINE_MS = 0.01


def run(binary: Path) -> dict[str, dict[str, float]]:
    samples: dict[str, dict[str, float]] = {}
    for _ in range(REPETITIONS):
        completed = subprocess.run(
            [str(binary)], check=True, capture_output=True, text=True
        )
        lines = completed.stdout.splitlines()
        if not lines or lines[0] != "workload\titerations\tms_per_operation":
            raise RuntimeError(f"Unexpected benchmark output from {binary}")
        current: dict[str, dict[str, float]] = {}
        for line in lines[1:]:
            name, iterations, milliseconds = line.split("\t")
            current[name] = {
                "iterations": int(iterations),
                "ms_per_operation": float(milliseconds),
            }
        if not samples:
            samples = {
                name: {"iterations": row["iterations"], "values": []}
                for name, row in current.items()
            }
        if set(current) != set(samples):
            raise RuntimeError(f"Workload set changed during benchmark: {binary}")
        for name, row in current.items():
            if row["iterations"] != samples[name]["iterations"]:
                raise RuntimeError(f"Iteration count changed for {name}: {binary}")
            samples[name]["values"].append(row["ms_per_operation"])
    return samples


def summarize(samples: dict[str, dict[str, float]]) -> dict[str, dict[str, float]]:
    summary = {}
    for name, row in samples.items():
        values = sorted(row["values"])
        summary[name] = {
            "iterations": row["iterations"],
            "samples_ms": values,
            "p50_ms": statistics.median(values),
            "p90_ms": values[math.ceil(0.9 * len(values)) - 1],
        }
    return summary


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--baseline-commit", required=True)
    parser.add_argument("--threshold", type=float, default=0.35)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.threshold < 0:
        parser.error("threshold must be nonnegative")

    baseline = summarize(run(args.baseline.resolve()))
    candidate = summarize(run(args.candidate.resolve()))
    if set(baseline) != set(candidate):
        parser.error("Baseline and candidate workload sets differ; update the benchmark schema")

    regressions = []
    for name in sorted(baseline):
        old = baseline[name]["p50_ms"]
        new = candidate[name]["p50_ms"]
        comparable = old >= MIN_BASELINE_MS
        ratio = (new / old) if old > 0 else None
        if comparable and ratio is not None and ratio > 1.0 + args.threshold:
            regressions.append(name)
        classification = "gated" if comparable else "below-noise-floor"
        ratio_text = f"{ratio:.2f}x" if ratio is not None else "n/a"
        print(
            f"{name:20} baseline={old:.6f}ms candidate={new:.6f}ms "
            f"ratio={ratio_text} status={classification}"
        )

    report = {
        "baseline_commit": args.baseline_commit,
        "candidate_binary": str(args.candidate),
        "repetitions": REPETITIONS,
        "slowdown_threshold": args.threshold,
        "minimum_baseline_ms_for_gate": MIN_BASELINE_MS,
        "baseline": baseline,
        "candidate": candidate,
        "regressions": regressions,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if regressions:
        print("Performance regressions exceed threshold: " + ", ".join(regressions))
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
