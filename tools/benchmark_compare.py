#!/usr/bin/env python3
"""Run two HyperDB workload binaries repeatedly and compare per-operation medians."""

import argparse
import json
import math
import statistics
import subprocess
import tempfile
from pathlib import Path

REPETITIONS = 7
MIN_BASELINE_MS = 0.01


def _milliseconds(value: float, unit: str) -> float:
    scales = {"ns": 1e-6, "us": 1e-3, "ms": 1.0, "s": 1e3}
    if unit not in scales:
        raise RuntimeError(f"Unsupported Google Benchmark time unit: {unit}")
    return value * scales[unit]


def run(binary: Path) -> dict[str, dict[str, list[float | int]]]:
    samples: dict[str, dict[str, list[float | int]]] = {}
    with tempfile.TemporaryDirectory(prefix="hyperdb-gbench-") as temporary:
        for sample in range(REPETITIONS):
            output = Path(temporary) / f"sample-{sample}.json"
            subprocess.run(
                [
                    str(binary),
                    "--benchmark_min_time=0.05s",
                    f"--benchmark_out={output}",
                    "--benchmark_out_format=json",
                ],
                check=True,
                capture_output=True,
                text=True,
            )
            document = json.loads(output.read_text(encoding="utf-8"))
            current = {}
            for result in document.get("benchmarks", []):
                if result.get("run_type") == "aggregate":
                    continue
                if result.get("error_occurred"):
                    raise RuntimeError(
                        f"Google Benchmark failed for {result.get('name', 'unknown')}: "
                        f"{result.get('error_message', 'unknown error')}"
                    )
                name = result["name"]
                current[name] = {
                    "ms_per_operation": _milliseconds(
                        result["real_time"], result["time_unit"]
                    ),
                    "iterations": int(result.get("iterations", 0)),
                }
            if not current:
                raise RuntimeError(f"No Google Benchmark measurements found in {output}")
            if not samples:
                samples = {
                    name: {"values": [], "iterations": []} for name in current
                }
            if set(current) != set(samples):
                raise RuntimeError(f"Workload set changed during benchmark: {binary}")
            for name, result in current.items():
                samples[name]["values"].append(result["ms_per_operation"])
                samples[name]["iterations"].append(result["iterations"])
    return samples


def summarize(samples: dict[str, dict[str, list[float]]]) -> dict[str, dict[str, float]]:
    summary = {}
    for name, row in samples.items():
        values = sorted(row["values"])
        summary[name] = {
            "iterations_per_sample": row["iterations"],
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
