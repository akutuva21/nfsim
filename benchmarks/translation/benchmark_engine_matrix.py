#!/usr/bin/env python3
"""Benchmark identical translation XML inputs across NFsim engines.

Each timed sample launches a fresh NFsim process.  Engine order reverses on
alternating repetitions to reduce drift.  The runner records exact input,
binary, and observable-output hashes plus wall time and sampled peak RSS.
"""

from __future__ import annotations

import argparse
import functools
import hashlib
import json
import os
import platform
import re
import statistics
import subprocess
import sys
import time
from pathlib import Path

try:
    import psutil
except ImportError:  # Peak RSS remains null without psutil.
    psutil = None


EVENT_RE = re.compile(r"You just simulated\s+(\d+)\s+reactions\s+in\s+([0-9.eE+-]+)s")


@functools.lru_cache(maxsize=None)
def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def mapping(value: str) -> tuple[str, str]:
    if "=" not in value:
        raise argparse.ArgumentTypeError("expected LABEL=VALUE")
    label, mapped = value.split("=", 1)
    if not label or not mapped:
        raise argparse.ArgumentTypeError("expected non-empty LABEL=VALUE")
    return label, mapped


def parse_model(value: str) -> tuple[str, Path, str]:
    label, mapped = mapping(value)
    if ":" not in mapped:
        raise argparse.ArgumentTypeError("model must be LABEL=/path/model.xml:SIM_TIME")
    path, sim_time = mapped.rsplit(":", 1)
    return label, Path(path).expanduser().resolve(), sim_time


def parse_engine(value: str) -> tuple[str, Path]:
    label, path = mapping(value)
    return label, Path(path).expanduser().resolve()


def parse_env(value: str) -> tuple[str, str, str]:
    engine, mapped = mapping(value)
    if "=" not in mapped:
        raise argparse.ArgumentTypeError("engine env must be ENGINE=NAME=VALUE")
    name, env_value = mapped.split("=", 1)
    return engine, name, env_value


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", action="append", type=parse_engine, required=True)
    parser.add_argument("--engine-env", action="append", type=parse_env, default=[])
    parser.add_argument("--reference-engine", help="engine used for correctness and attribution checks")
    parser.add_argument("--model", action="append", type=parse_model, required=True)
    parser.add_argument("--seeds", default="1339,1340,1341,1342,1343")
    parser.add_argument("--repeats", type=int, default=2)
    parser.add_argument("--modes", choices=("normal", "connect", "both"), default="both")
    parser.add_argument("--steps", type=int, default=3)
    parser.add_argument("--update-limit", type=int, default=20)
    parser.add_argument("--max-molecules", type=int, default=5_000_000)
    parser.add_argument("--max-cpu-time", type=int, default=600)
    parser.add_argument("--timeout", type=int, default=900)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--metadata", action="append", type=mapping, default=[])
    return parser.parse_args()


def validate(args: argparse.Namespace) -> tuple[list[int], list[str], dict[str, dict[str, str]]]:
    labels = [label for label, _ in args.engine]
    if len(set(labels)) != len(labels):
        raise ValueError("duplicate engine label")
    if args.reference_engine is not None and args.reference_engine not in labels:
        raise ValueError(f"unknown reference engine: {args.reference_engine}")
    for _, binary in args.engine:
        if not binary.is_file() or not os.access(binary, os.X_OK):
            raise ValueError(f"engine is not executable: {binary}")
    model_labels = [label for label, _, _ in args.model]
    if len(set(model_labels)) != len(model_labels):
        raise ValueError("duplicate model label")
    for _, model, _ in args.model:
        if not model.is_file():
            raise ValueError(f"model does not exist: {model}")
    if args.repeats < 1:
        raise ValueError("repeats must be positive")
    seeds = [int(item) for item in args.seeds.split(",") if item]
    if not seeds:
        raise ValueError("at least one seed is required")
    modes = ["normal", "connect"] if args.modes == "both" else [args.modes]
    envs: dict[str, dict[str, str]] = {label: {} for label in labels}
    for label, name, value in args.engine_env:
        if label not in envs:
            raise ValueError(f"environment supplied for unknown engine: {label}")
        envs[label][name] = value
    return seeds, modes, envs


def monitored_run(command: list[str], env: dict[str, str], cwd: Path, timeout: int) -> tuple[int, str, str, float, float | None]:
    start = time.perf_counter()
    process = subprocess.Popen(command, cwd=cwd, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    peak_rss = 0
    timed_out = False
    while process.poll() is None:
        if time.perf_counter() - start > timeout:
            timed_out = True
            process.kill()
            break
        if psutil is not None:
            try:
                root = psutil.Process(process.pid)
                peak_rss = max(peak_rss, root.memory_info().rss)
                for child in root.children(recursive=True):
                    peak_rss = max(peak_rss, child.memory_info().rss)
            except (OSError, psutil.NoSuchProcess, psutil.AccessDenied):
                pass
        time.sleep(0.01)
    stdout, stderr = process.communicate()
    if timed_out:
        stderr += f"\nBENCHMARK TIMEOUT after {timeout}s\n"
    return process.returncode, stdout, stderr, time.perf_counter() - start, (peak_rss / 1024 if peak_rss else None)


def run_sample(
    *, engine_label: str, binary: Path, engine_env: dict[str, str], model_label: str,
    model: Path, sim_time: str, seed: int, repeat: int, mode: str,
    args: argparse.Namespace, root: Path,
) -> dict[str, object]:
    run_dir = root / "runs" / model_label / mode / f"seed-{seed}" / f"repeat-{repeat}" / engine_label
    run_dir.mkdir(parents=True, exist_ok=True)
    output = run_dir / "simulation.gdat"
    command = [
        str(binary), "-xml", str(model), "-sim", sim_time, "-oSteps", str(args.steps),
        "-seed", str(seed), "-o", str(output), "-utl", str(args.update_limit),
        "-gml", str(args.max_molecules), "-maxcputime", str(args.max_cpu_time),
    ]
    if mode == "connect":
        command.append("-connect")
    env = os.environ.copy()
    env.update(engine_env)
    rc, stdout, stderr, wall, rss = monitored_run(command, env, root, args.timeout)
    (run_dir / "stdout.txt").write_text(stdout, encoding="utf-8")
    (run_dir / "stderr.txt").write_text(stderr, encoding="utf-8")
    matches = EVENT_RE.findall(stdout + "\n" + stderr)
    events, simulation_cpu = (int(matches[-1][0]), float(matches[-1][1])) if matches else (None, None)
    return {
        "model": model_label, "model_path": str(model), "model_sha256": sha256(model),
        "model_bytes": model.stat().st_size, "simulation_time": sim_time,
        "engine": engine_label, "engine_sha256": sha256(binary),
        "engine_env": engine_env, "seed": seed, "repeat": repeat, "mode": mode,
        "returncode": rc, "events": events, "simulation_cpu_seconds": simulation_cpu,
        "wall_seconds": wall, "peak_rss_kb": rss,
        "output_sha256": sha256(output) if output.is_file() else None,
        "command": command,
    }


def summarize(rows: list[dict[str, object]], engine_labels: list[str], reference: str) -> list[dict[str, object]]:
    summary: list[dict[str, object]] = []
    keys = sorted({(str(row["model"]), str(row["mode"])) for row in rows})
    baseline = engine_labels[0]
    for model, mode in keys:
        grouped = {
            engine: [row for row in rows if row["model"] == model and row["mode"] == mode and row["engine"] == engine and row["returncode"] == 0 and row["events"] is not None and row["output_sha256"] is not None]
            for engine in engine_labels
        }
        baseline_wall = statistics.median(float(row["wall_seconds"]) for row in grouped[baseline]) if grouped[baseline] else None
        baseline_cpu_values = [float(row["simulation_cpu_seconds"]) for row in grouped[baseline] if row["simulation_cpu_seconds"] is not None]
        baseline_cpu = statistics.median(baseline_cpu_values) if baseline_cpu_values else None
        reference_wall = statistics.median(float(row["wall_seconds"]) for row in grouped[reference]) if grouped[reference] else None
        reference_cpu_values = [float(row["simulation_cpu_seconds"]) for row in grouped[reference] if row["simulation_cpu_seconds"] is not None]
        reference_cpu = statistics.median(reference_cpu_values) if reference_cpu_values else None
        for engine in engine_labels:
            samples = grouped[engine]
            wall = statistics.median(float(row["wall_seconds"]) for row in samples) if samples else None
            cpu_values = [float(row["simulation_cpu_seconds"]) for row in samples if row["simulation_cpu_seconds"] is not None]
            cpu = statistics.median(cpu_values) if cpu_values else None
            rss_values = [float(row["peak_rss_kb"]) for row in samples if row["peak_rss_kb"] is not None]
            parity = []
            for row in samples:
                peers = [peer for peer in grouped[reference] if peer["seed"] == row["seed"] and peer["repeat"] == row["repeat"]]
                if peers and row["output_sha256"] is not None:
                    parity.append(row["output_sha256"] == peers[0]["output_sha256"] and row["events"] == peers[0]["events"])
            summary.append({
                "model": model, "mode": mode, "engine": engine, "successful_samples": len(samples),
                "median_wall_seconds": wall, "median_simulation_cpu_seconds": cpu,
                "median_peak_rss_kb": statistics.median(rss_values) if rss_values else None,
                "wall_speedup_vs_baseline": (baseline_wall / wall) if baseline_wall and wall else None,
                "simulation_cpu_speedup_vs_baseline": (baseline_cpu / cpu) if baseline_cpu and cpu else None,
                "wall_speedup_vs_reference": (reference_wall / wall) if reference_wall and wall else None,
                "simulation_cpu_speedup_vs_reference": (reference_cpu / cpu) if reference_cpu and cpu else None,
                "exact_pairs_vs_reference": sum(parity), "compared_pairs_vs_reference": len(parity),
            })
    return summary


def main() -> int:
    args = parse_args()
    seeds, modes, envs = validate(args)
    root = args.output_dir.expanduser().resolve()
    root.mkdir(parents=True, exist_ok=False)
    engines = list(args.engine)
    provenance = {
        "created_at_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "host": platform.uname()._asdict(), "python": sys.version,
        "psutil": getattr(psutil, "__version__", None),
        "engines": {label: {"path": str(path), "sha256": sha256(path), "environment": envs[label]} for label, path in engines},
        "models": {label: {"path": str(path), "sha256": sha256(path), "bytes": path.stat().st_size, "simulation_time": sim} for label, path, sim in args.model},
        "seeds": seeds, "repeats": args.repeats, "modes": modes,
        "steps": args.steps, "update_limit": args.update_limit,
        "max_molecules": args.max_molecules, "max_cpu_time": args.max_cpu_time,
        "baseline_engine": engines[0][0], "reference_engine": args.reference_engine or engines[0][0],
        "metadata": dict(args.metadata),
    }
    (root / "provenance.json").write_text(json.dumps(provenance, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    rows: list[dict[str, object]] = []
    results_path = root / "results.jsonl"
    with results_path.open("w", encoding="utf-8") as stream:
        sample_index = 0
        for model_label, model, sim_time in args.model:
            for mode in modes:
                for repeat in range(1, args.repeats + 1):
                    for seed in seeds:
                        ordered = engines if sample_index % 2 == 0 else list(reversed(engines))
                        for engine_label, binary in ordered:
                            print(f"{model_label} {mode} repeat={repeat} seed={seed} engine={engine_label}", flush=True)
                            row = run_sample(engine_label=engine_label, binary=binary, engine_env=envs[engine_label], model_label=model_label, model=model, sim_time=sim_time, seed=seed, repeat=repeat, mode=mode, args=args, root=root)
                            rows.append(row)
                            stream.write(json.dumps(row, sort_keys=True) + "\n")
                            stream.flush()
                        sample_index += 1
    reference = args.reference_engine or engines[0][0]
    summary = summarize(rows, [label for label, _ in engines], reference)
    (root / "summary.json").write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    failures = [row for row in rows if row["returncode"] != 0 or row["events"] is None or row["output_sha256"] is None]
    mismatches = [row for row in summary if row["engine"] != reference and row["compared_pairs_vs_reference"] != row["exact_pairs_vs_reference"]]
    print(f"samples={len(rows)} failures={len(failures)} summary_rows={len(summary)} mismatched_groups={len(mismatches)}")
    return 1 if failures else 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
