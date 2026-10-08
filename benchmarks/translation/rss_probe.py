#!/usr/bin/env python3
"""Measure one NFsim process in a fresh worker using waitable child rusage."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import resource
import subprocess
import time
from pathlib import Path


def file_hash(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--model", required=True)
    parser.add_argument("--xml", type=Path, required=True)
    parser.add_argument("--sim-time", required=True)
    parser.add_argument("--seed", type=int, default=1339)
    parser.add_argument("--mode", choices=("normal", "connect"), default="normal")
    parser.add_argument("--env", action="append", default=[])
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()

    args.output_dir.mkdir(parents=True, exist_ok=False)
    output = args.output_dir / "simulation.gdat"
    command = [
        str(args.binary.resolve()), "-xml", str(args.xml.resolve()), "-sim", args.sim_time,
        "-oSteps", "3", "-seed", str(args.seed), "-o", str(output), "-utl", "3",
        "-gml", "5000000", "-maxcputime", "600",
    ]
    if args.mode == "connect":
        command.append("-connect")
    env = os.environ.copy()
    for item in args.env:
        name, value = item.split("=", 1)
        env[name] = value

    before = resource.getrusage(resource.RUSAGE_CHILDREN)
    started = time.perf_counter()
    result = subprocess.run(command, env=env, text=True, capture_output=True)
    wall = time.perf_counter() - started
    after = resource.getrusage(resource.RUSAGE_CHILDREN)
    (args.output_dir / "stdout.txt").write_text(result.stdout, encoding="utf-8")
    (args.output_dir / "stderr.txt").write_text(result.stderr, encoding="utf-8")
    # ru_maxrss is bytes on macOS and KiB on Linux.  This worker has one child.
    peak_rss_kb = after.ru_maxrss / 1024 if platform.system() == "Darwin" else after.ru_maxrss
    record = {
        "model": args.model, "engine": args.engine, "seed": args.seed, "mode": args.mode,
        "returncode": result.returncode, "wall_seconds": wall,
        "user_seconds": after.ru_utime - before.ru_utime,
        "system_seconds": after.ru_stime - before.ru_stime,
        "peak_rss_kb": peak_rss_kb,
        "binary_sha256": file_hash(args.binary), "xml_sha256": file_hash(args.xml),
        "output_sha256": file_hash(output) if output.is_file() else None,
        "environment": dict(item.split("=", 1) for item in args.env), "command": command,
    }
    (args.output_dir / "record.json").write_text(json.dumps(record, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(record, sort_keys=True))
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
