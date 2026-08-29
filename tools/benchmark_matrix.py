#!/usr/bin/env python3
"""Run reproducible NFsim baseline/candidate benchmark matrices.

The driver keeps performance runs separate from output-equivalence checks,
alternates variant order, records exact inputs and commands, and can measure
independent trajectories in separate operating-system processes.
"""

from __future__ import print_function

import argparse
import csv
import datetime
import hashlib
import json
import os
import platform
import re
import shlex
import statistics
import subprocess
import sys
import time

try:
    import concurrent.futures
except ImportError:  # pragma: no cover - Python 2 is not supported.
    concurrent = None

try:
    import resource
except ImportError:  # pragma: no cover - Windows has no resource module.
    resource = None


EVENT_RE = re.compile(r"You just simulated\s+([0-9]+)\s+reactions")
RESERVED_FLAGS = {"-xml", "-sim", "-oSteps", "-seed", "-o"}


def utc_now():
    return datetime.datetime.utcnow().replace(microsecond=0).isoformat() + "Z"


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def command_string(command):
    try:
        return shlex.join(command)
    except AttributeError:  # pragma: no cover - Python 3.7 compatibility.
        return " ".join(shlex.quote(str(part)) for part in command)


def safe_name(value):
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", value)


def parse_workload(value):
    name, separator, xml_path = value.partition("=")
    if not separator or not name or not xml_path:
        raise argparse.ArgumentTypeError("workload must use NAME=XML_PATH")
    xml_path = os.path.abspath(xml_path)
    if not os.path.isfile(xml_path):
        raise argparse.ArgumentTypeError("XML path does not exist: %s" % xml_path)
    return {"name": name, "xml": xml_path}


def parse_condition(value):
    name, separator, argument_text = value.partition("=")
    if not name:
        raise argparse.ArgumentTypeError("condition name cannot be empty")
    arguments = shlex.split(argument_text) if separator else []
    invalid = RESERVED_FLAGS.intersection(arguments)
    if invalid:
        raise argparse.ArgumentTypeError(
            "condition cannot set reserved flags: %s" % sorted(invalid)
        )
    return {"name": name, "args": arguments}


def parse_integer_list(value, label):
    values = []
    for item in value.split(","):
        item = item.strip()
        if not item:
            continue
        try:
            parsed = int(item)
        except ValueError:
            raise argparse.ArgumentTypeError(
                "%s must contain integers: %s" % (label, value)
            )
        if parsed <= 0:
            raise argparse.ArgumentTypeError(
                "%s values must be positive: %s" % (label, value)
            )
        values.append(parsed)
    if not values:
        raise argparse.ArgumentTypeError("%s cannot be empty" % label)
    return values


def validate_extra_arguments(arguments):
    invalid = RESERVED_FLAGS.intersection(arguments)
    if invalid:
        raise ValueError(
            "extra arguments cannot set reserved flags: %s" % sorted(invalid)
        )


def percentile(values, fraction):
    """Linear percentile, with no dependency beyond the standard library."""
    if not values:
        return None
    ordered = sorted(values)
    if len(ordered) == 1:
        return float(ordered[0])
    position = (len(ordered) - 1) * fraction
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    weight = position - lower
    return float(ordered[lower]) + weight * (ordered[upper] - ordered[lower])


def numeric_summary(values):
    values = [float(value) for value in values if value is not None]
    if not values:
        return {
            "n": 0,
            "median": None,
            "p10": None,
            "p90": None,
            "min": None,
            "max": None,
        }
    return {
        "n": len(values),
        "median": float(statistics.median(values)),
        "p10": percentile(values, 0.10),
        "p90": percentile(values, 0.90),
        "min": min(values),
        "max": max(values),
    }


def git_metadata(repo_root):
    metadata = {"commit": None, "status": None}
    try:
        metadata["commit"] = subprocess.check_output(
            ["git", "-C", repo_root, "rev-parse", "HEAD"],
            stderr=subprocess.DEVNULL,
            universal_newlines=True,
        ).strip()
        metadata["status"] = subprocess.check_output(
            ["git", "-C", repo_root, "status", "--short"],
            stderr=subprocess.DEVNULL,
            universal_newlines=True,
        ).splitlines()
    except (OSError, subprocess.CalledProcessError):
        pass
    return metadata


def normalize_peak_rss_kb(value):
    if value is None:
        return None
    # Linux reports KiB; macOS reports bytes.
    if sys.platform == "darwin":
        return float(value) / 1024.0
    return float(value)


def parse_event_count(stdout_path):
    try:
        with open(stdout_path, "r", errors="replace") as handle:
            text = handle.read()
    except (IOError, OSError):
        return None
    matches = EVENT_RE.findall(text)
    return int(matches[-1]) if matches else None


def child_main(argv):
    parser = argparse.ArgumentParser(description="Internal NFsim benchmark child runner")
    parser.add_argument("--metrics-path", required=True)
    parser.add_argument("--stdout-path", required=True)
    parser.add_argument("--stderr-path", required=True)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    arguments = parser.parse_args(argv)
    command = list(arguments.command)
    if command and command[0] == "--":
        command = command[1:]
    if not command:
        parser.error("child command cannot be empty")

    start = time.perf_counter()
    with open(arguments.stdout_path, "w") as stdout_handle:
        with open(arguments.stderr_path, "w") as stderr_handle:
            completed = subprocess.run(
                command,
                stdout=stdout_handle,
                stderr=stderr_handle,
                universal_newlines=True,
            )
    wall_seconds = time.perf_counter() - start

    user_seconds = None
    system_seconds = None
    peak_rss_kb = None
    if resource is not None:
        usage = resource.getrusage(resource.RUSAGE_CHILDREN)
        user_seconds = float(usage.ru_utime)
        system_seconds = float(usage.ru_stime)
        peak_rss_kb = normalize_peak_rss_kb(usage.ru_maxrss)

    metrics = {
        "returncode": completed.returncode,
        "wall_seconds": wall_seconds,
        "cpu_user_seconds": user_seconds,
        "cpu_system_seconds": system_seconds,
        "peak_rss_kb": peak_rss_kb,
        "event_count": parse_event_count(arguments.stdout_path),
    }
    with open(arguments.metrics_path, "w") as metrics_handle:
        json.dump(metrics, metrics_handle, indent=2, sort_keys=True)
        metrics_handle.write("\n")
    return 0


def read_child_metrics(metrics_path):
    with open(metrics_path, "r") as handle:
        return json.load(handle)


def run_one(
    script_path,
    executable,
    workload,
    condition,
    sim,
    osteps,
    actual_seed,
    extra_arguments,
    logs_dir,
    run_id,
    output_path=None,
):
    stdout_path = os.path.join(logs_dir, run_id + ".stdout.log")
    stderr_path = os.path.join(logs_dir, run_id + ".stderr.log")
    helper_stderr_path = os.path.join(logs_dir, run_id + ".runner.log")
    metrics_path = os.path.join(logs_dir, run_id + ".metrics.json")

    command = [
        executable,
        "-xml",
        workload["xml"],
        "-sim",
        str(sim),
        "-oSteps",
        str(osteps),
        "-seed",
        str(actual_seed),
    ]
    command.extend(condition["args"])
    command.extend(extra_arguments)
    command.extend(["-o", output_path if output_path else os.devnull])

    child_command = [
        sys.executable,
        script_path,
        "--child",
        "--metrics-path",
        metrics_path,
        "--stdout-path",
        stdout_path,
        "--stderr-path",
        stderr_path,
        "--",
    ] + command

    start = time.perf_counter()
    with open(helper_stderr_path, "w") as helper_stderr:
        helper = subprocess.run(
            child_command,
            stdout=subprocess.DEVNULL,
            stderr=helper_stderr,
            universal_newlines=True,
        )
    driver_wall_seconds = time.perf_counter() - start

    if os.path.isfile(metrics_path):
        metrics = read_child_metrics(metrics_path)
    else:
        metrics = {
            "returncode": helper.returncode,
            "wall_seconds": driver_wall_seconds,
            "cpu_user_seconds": None,
            "cpu_system_seconds": None,
            "peak_rss_kb": None,
            "event_count": None,
        }

    event_count = metrics.get("event_count")
    child_wall = metrics.get("wall_seconds")
    return {
        "run_id": run_id,
        "returncode": metrics.get("returncode", helper.returncode),
        "actual_seed": actual_seed,
        "wall_seconds": child_wall,
        "driver_wall_seconds": driver_wall_seconds,
        "cpu_user_seconds": metrics.get("cpu_user_seconds"),
        "cpu_system_seconds": metrics.get("cpu_system_seconds"),
        "cpu_seconds": (
            (metrics.get("cpu_user_seconds") or 0.0)
            + (metrics.get("cpu_system_seconds") or 0.0)
            if metrics.get("cpu_user_seconds") is not None
            and metrics.get("cpu_system_seconds") is not None
            else None
        ),
        "peak_rss_kb": metrics.get("peak_rss_kb"),
        "event_count": event_count,
        "events_per_second": (
            float(event_count) / child_wall
            if event_count is not None and child_wall and child_wall > 0
            else None
        ),
        "stdout_path": stdout_path,
        "stderr_path": stderr_path,
        "command": command,
        "output_path": output_path,
    }


def run_variant(
    script_path,
    executable,
    workload,
    condition,
    sim,
    osteps,
    group_seed,
    workers,
    extra_arguments,
    logs_dir,
    group_id,
    variant,
):
    start = time.perf_counter()

    def run_worker(worker_index):
        actual_seed = group_seed + worker_index
        run_id = "%s-%s-%s-w%d-p%d" % (
            group_id,
            variant,
            safe_name(workload["name"]),
            workers,
            worker_index,
        )
        return run_one(
            script_path,
            executable,
            workload,
            condition,
            sim,
            osteps,
            actual_seed,
            extra_arguments,
            logs_dir,
            run_id,
        )

    if workers == 1:
        records = [run_worker(0)]
    else:
        with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as executor:
            records = list(executor.map(run_worker, range(workers)))
    driver_wall_seconds = time.perf_counter() - start
    return records, driver_wall_seconds


def write_csv(path, rows, fieldnames):
    with open(path, "w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def write_json(path, value):
    with open(path, "w") as handle:
        json.dump(value, handle, indent=2, sort_keys=True)
        handle.write("\n")


def build_parser():
    parser = argparse.ArgumentParser(
        description="Run an alternating, multi-seed NFsim benchmark matrix."
    )
    parser.add_argument("--baseline", required=True, help="baseline NFsim executable")
    parser.add_argument("--candidate", required=True, help="candidate NFsim executable")
    parser.add_argument("--output-dir", required=True, help="new result directory")
    parser.add_argument(
        "--workload",
        action="append",
        required=True,
        type=parse_workload,
        metavar="NAME=XML",
        help="repeat for each XML workload",
    )
    parser.add_argument(
        "--seeds",
        required=True,
        help="comma-separated fixed seeds, e.g. 17,18,19,...,26",
    )
    parser.add_argument(
        "--condition",
        action="append",
        type=parse_condition,
        metavar="NAME[=ARGS]",
        help="repeat condition, e.g. normal or truncated=-utl 1",
    )
    parser.add_argument("--workers", default="1", help="comma-separated process counts")
    parser.add_argument("--sim", default="20", help="NFsim -sim value")
    parser.add_argument("--osteps", default="1", help="NFsim -oSteps value")
    parser.add_argument(
        "--extra-arg",
        action="append",
        default=[],
        help="additional NFsim argument; repeat for each token",
    )
    parser.add_argument(
        "--check-output",
        action="store_true",
        help="run same-seed baseline/candidate output digest checks",
    )
    parser.add_argument("--compiler", default=None, help="compiler/build label")
    parser.add_argument("--hardware", default=None, help="hardware label")
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="allow an existing result directory with metadata.json",
    )
    return parser


def driver_main(argv):
    parser = build_parser()
    arguments = parser.parse_args(argv)

    try:
        seeds = parse_integer_list(arguments.seeds, "seeds")
        workers = parse_integer_list(arguments.workers, "workers")
        validate_extra_arguments(arguments.extra_arg)
    except (argparse.ArgumentTypeError, ValueError) as error:
        parser.error(str(error))

    conditions = arguments.condition or [{"name": "normal", "args": []}]
    baseline = os.path.abspath(arguments.baseline)
    candidate = os.path.abspath(arguments.candidate)
    for executable in (baseline, candidate):
        if not os.path.isfile(executable):
            parser.error("executable does not exist: %s" % executable)

    output_dir = os.path.abspath(arguments.output_dir)
    metadata_path = os.path.join(output_dir, "metadata.json")
    if os.path.exists(metadata_path) and not arguments.overwrite:
        parser.error(
            "result directory already contains metadata.json; use --overwrite or choose a new directory"
        )
    os.makedirs(output_dir, exist_ok=True)
    logs_dir = os.path.join(output_dir, "logs")
    os.makedirs(logs_dir, exist_ok=True)

    script_path = os.path.abspath(__file__)
    repo_root = os.path.abspath(os.path.join(os.path.dirname(script_path), os.pardir))
    started_at = utc_now()
    metadata = {
        "schema_version": 1,
        "started_at_utc": started_at,
        "argv": [script_path] + list(argv),
        "parameters": {
            "sim": arguments.sim,
            "osteps": arguments.osteps,
            "seeds": seeds,
            "workers": workers,
            "conditions": conditions,
            "extra_arguments": arguments.extra_arg,
            "check_output": arguments.check_output,
            "compiler": arguments.compiler,
            "hardware": arguments.hardware,
        },
        "host": {
            "platform": platform.platform(),
            "machine": platform.machine(),
            "processor": platform.processor(),
            "python": platform.python_version(),
        },
        "git": git_metadata(repo_root),
        "executables": {
            "baseline": {"path": baseline, "sha256": sha256_file(baseline)},
            "candidate": {"path": candidate, "sha256": sha256_file(candidate)},
        },
        "workloads": [
            {
                "name": workload["name"],
                "xml": workload["xml"],
                "sha256": sha256_file(workload["xml"]),
            }
            for workload in arguments.workload
        ],
    }

    run_rows = []
    group_rows = []
    output_check_rows = []
    failures = []
    group_number = 0

    for workload_index, workload in enumerate(arguments.workload):
        for condition_index, condition in enumerate(conditions):
            for seed_index, group_seed in enumerate(seeds):
                for worker_index, worker_count in enumerate(workers):
                    group_number += 1
                    group_id = "g%04d" % group_number
                    alternate_index = (
                        workload_index + condition_index + seed_index + worker_index
                    )
                    order = (
                        ["baseline", "candidate"]
                        if alternate_index % 2 == 0
                        else ["candidate", "baseline"]
                    )
                    executables = {"baseline": baseline, "candidate": candidate}
                    for variant in order:
                        records, driver_wall = run_variant(
                            script_path,
                            executables[variant],
                            workload,
                            condition,
                            arguments.sim,
                            arguments.osteps,
                            group_seed,
                            worker_count,
                            arguments.extra_arg,
                            logs_dir,
                            group_id,
                            variant,
                        )
                        for record in records:
                            row = {
                                "run_kind": "benchmark",
                                "group_id": group_id,
                                "variant": variant,
                                "workload": workload["name"],
                                "condition": condition["name"],
                                "group_seed": group_seed,
                                "workers": worker_count,
                                "actual_seed": record["actual_seed"],
                                "returncode": record["returncode"],
                                "wall_seconds": record["wall_seconds"],
                                "driver_wall_seconds": record["driver_wall_seconds"],
                                "cpu_user_seconds": record["cpu_user_seconds"],
                                "cpu_system_seconds": record["cpu_system_seconds"],
                                "cpu_seconds": record["cpu_seconds"],
                                "peak_rss_kb": record["peak_rss_kb"],
                                "event_count": record["event_count"],
                                "events_per_second": record["events_per_second"],
                                "stdout_path": record["stdout_path"],
                                "stderr_path": record["stderr_path"],
                                "output_path": record["output_path"],
                                "command": json.dumps(record["command"]),
                            }
                            run_rows.append(row)
                            if record["returncode"] != 0:
                                failures.append(row)

                        event_counts = [record["event_count"] for record in records]
                        total_events = (
                            sum(event_counts)
                            if all(event_count is not None for event_count in event_counts)
                            else None
                        )
                        total_cpu = [
                            record["cpu_seconds"]
                            for record in records
                            if record["cpu_seconds"] is not None
                        ]
                        peak_rss = [
                            record["peak_rss_kb"]
                            for record in records
                            if record["peak_rss_kb"] is not None
                        ]
                        group_rows.append(
                            {
                                "group_id": group_id,
                                "variant": variant,
                                "workload": workload["name"],
                                "condition": condition["name"],
                                "group_seed": group_seed,
                                "workers": worker_count,
                                "first_variant": order[0],
                                "driver_wall_seconds": driver_wall,
                                "sum_cpu_seconds": sum(total_cpu) if total_cpu else None,
                                "total_event_count": total_events,
                                "max_peak_rss_kb": max(peak_rss) if peak_rss else None,
                                "throughput_events_per_second": (
                                    float(total_events) / driver_wall
                                    if total_events is not None and driver_wall > 0
                                    else None
                                ),
                                "returncode": max(
                                    record["returncode"] for record in records
                                ),
                            }
                        )

    if arguments.check_output:
        check_number = 0
        for workload in arguments.workload:
            for condition in conditions:
                for seed in seeds:
                    check_number += 1
                    check_id = "check%04d" % check_number
                    digests = {}
                    returncodes = {}
                    for variant, executable in (
                        ("baseline", baseline),
                        ("candidate", candidate),
                    ):
                        output_path = os.path.join(
                            output_dir,
                            "outputs",
                            "%s-%s-%s-seed%d.gdat"
                            % (
                                check_id,
                                variant,
                                safe_name(condition["name"]),
                                seed,
                            ),
                        )
                        os.makedirs(os.path.dirname(output_path), exist_ok=True)
                        record = run_one(
                            script_path,
                            executable,
                            workload,
                            condition,
                            arguments.sim,
                            arguments.osteps,
                            seed,
                            arguments.extra_arg,
                            logs_dir,
                            "%s-%s-%s" % (check_id, variant, safe_name(workload["name"])),
                            output_path=output_path,
                        )
                        returncodes[variant] = record["returncode"]
                        if os.path.isfile(output_path):
                            digests[variant] = sha256_file(output_path)
                        else:
                            digests[variant] = None
                        if record["returncode"] != 0:
                            failures.append(
                                {
                                    "run_kind": "output_check",
                                    "group_id": check_id,
                                    "variant": variant,
                                    "workload": workload["name"],
                                    "condition": condition["name"],
                                    "group_seed": seed,
                                    "returncode": record["returncode"],
                                }
                            )
                    output_check_rows.append(
                        {
                            "check_id": check_id,
                            "workload": workload["name"],
                            "condition": condition["name"],
                            "seed": seed,
                            "baseline_sha256": digests["baseline"],
                            "candidate_sha256": digests["candidate"],
                            "baseline_returncode": returncodes["baseline"],
                            "candidate_returncode": returncodes["candidate"],
                            "exact_match": (
                                returncodes["baseline"] == 0
                                and returncodes["candidate"] == 0
                                and digests["baseline"] is not None
                                and digests["baseline"] == digests["candidate"]
                            ),
                        }
                    )

    summary_rows = []
    groups_by_key = {}
    for row in group_rows:
        key = (row["variant"], row["workload"], row["condition"], row["workers"])
        groups_by_key.setdefault(key, []).append(row)
    for key, rows in sorted(groups_by_key.items()):
        variant, workload, condition, worker_count = key
        metrics = {
            "driver_wall_seconds": numeric_summary(
                [row["driver_wall_seconds"] for row in rows]
            ),
            "sum_cpu_seconds": numeric_summary(
                [row["sum_cpu_seconds"] for row in rows]
            ),
            "total_event_count": numeric_summary(
                [row["total_event_count"] for row in rows]
            ),
            "max_peak_rss_kb": numeric_summary(
                [row["max_peak_rss_kb"] for row in rows]
            ),
            "throughput_events_per_second": numeric_summary(
                [row["throughput_events_per_second"] for row in rows]
            ),
        }
        summary_rows.append(
            {
                "variant": variant,
                "workload": workload,
                "condition": condition,
                "workers": worker_count,
                "groups": len(rows),
                "wall_median_seconds": metrics["driver_wall_seconds"]["median"],
                "wall_p10_seconds": metrics["driver_wall_seconds"]["p10"],
                "wall_p90_seconds": metrics["driver_wall_seconds"]["p90"],
                "cpu_median_seconds": metrics["sum_cpu_seconds"]["median"],
                "peak_rss_median_kb": metrics["max_peak_rss_kb"]["median"],
                "events_median": metrics["total_event_count"]["median"],
                "throughput_median_events_per_second": metrics[
                    "throughput_events_per_second"
                ]["median"],
                "throughput_p10_events_per_second": metrics[
                    "throughput_events_per_second"
                ]["p10"],
                "throughput_p90_events_per_second": metrics[
                    "throughput_events_per_second"
                ]["p90"],
            }
        )

    run_fields = [
        "run_kind",
        "group_id",
        "variant",
        "workload",
        "condition",
        "group_seed",
        "workers",
        "actual_seed",
        "returncode",
        "wall_seconds",
        "driver_wall_seconds",
        "cpu_user_seconds",
        "cpu_system_seconds",
        "cpu_seconds",
        "peak_rss_kb",
        "event_count",
        "events_per_second",
        "stdout_path",
        "stderr_path",
        "output_path",
        "command",
    ]
    group_fields = [
        "group_id",
        "variant",
        "workload",
        "condition",
        "group_seed",
        "workers",
        "first_variant",
        "driver_wall_seconds",
        "sum_cpu_seconds",
        "total_event_count",
        "max_peak_rss_kb",
        "throughput_events_per_second",
        "returncode",
    ]
    summary_fields = [
        "variant",
        "workload",
        "condition",
        "workers",
        "groups",
        "wall_median_seconds",
        "wall_p10_seconds",
        "wall_p90_seconds",
        "cpu_median_seconds",
        "peak_rss_median_kb",
        "events_median",
        "throughput_median_events_per_second",
        "throughput_p10_events_per_second",
        "throughput_p90_events_per_second",
    ]

    write_csv(os.path.join(output_dir, "runs.csv"), run_rows, run_fields)
    write_csv(os.path.join(output_dir, "groups.csv"), group_rows, group_fields)
    write_csv(os.path.join(output_dir, "summary.csv"), summary_rows, summary_fields)
    if arguments.check_output:
        write_csv(
            os.path.join(output_dir, "output_checks.csv"),
            output_check_rows,
            [
                "check_id",
                "workload",
                "condition",
                "seed",
                "baseline_sha256",
                "candidate_sha256",
                "baseline_returncode",
                "candidate_returncode",
                "exact_match",
            ],
        )

    metadata["finished_at_utc"] = utc_now()
    metadata["counts"] = {
        "benchmark_runs": len(run_rows),
        "benchmark_groups": len(group_rows),
        "output_checks": len(output_check_rows),
        "failures": len(failures),
    }
    write_json(metadata_path, metadata)

    print("benchmark_matrix result_dir=%s" % output_dir)
    print("benchmark_matrix runs=%d groups=%d" % (len(run_rows), len(group_rows)))
    if output_check_rows:
        exact = sum(1 for row in output_check_rows if row["exact_match"])
        print(
            "benchmark_matrix output_checks=%d exact_matches=%d"
            % (len(output_check_rows), exact)
        )
    if failures:
        print("benchmark_matrix failures=%d" % len(failures), file=sys.stderr)
        return 1
    if output_check_rows and not all(row["exact_match"] for row in output_check_rows):
        print("benchmark_matrix output mismatch", file=sys.stderr)
        return 1
    return 0


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    if argv and argv[0] == "--child":
        return child_main(argv[1:])
    return driver_main(argv)


if __name__ == "__main__":
    sys.exit(main())
