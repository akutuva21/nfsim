#!/usr/bin/env python3
import os
import argparse
import sys
import tempfile
import unittest


TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)

import benchmark_matrix


class BenchmarkMatrixTests(unittest.TestCase):
    def test_parse_condition_preserves_argument_boundaries(self):
        condition = benchmark_matrix.parse_condition("truncated=-utl 1")
        self.assertEqual(condition["name"], "truncated")
        self.assertEqual(condition["args"], ["-utl", "1"])

    def test_parse_condition_rejects_output_override(self):
        with self.assertRaises(argparse.ArgumentTypeError):
            benchmark_matrix.parse_condition("bad=-o result.gdat")

    def test_percentile_and_summary_are_deterministic(self):
        self.assertEqual(benchmark_matrix.percentile([1, 2, 3], 0.5), 2.0)
        summary = benchmark_matrix.numeric_summary([1, 2, 3, 4])
        self.assertEqual(summary["n"], 4)
        self.assertEqual(summary["median"], 2.5)
        self.assertEqual(summary["p10"], 1.3)
        self.assertEqual(summary["p90"], 3.7)

    def test_child_metrics_are_written(self):
        with tempfile.TemporaryDirectory() as directory:
            metrics = os.path.join(directory, "metrics.json")
            stdout = os.path.join(directory, "stdout.log")
            stderr = os.path.join(directory, "stderr.log")
            returncode = benchmark_matrix.child_main(
                [
                    "--metrics-path",
                    metrics,
                    "--stdout-path",
                    stdout,
                    "--stderr-path",
                    stderr,
                    "--",
                    sys.executable,
                    "-c",
                    "print('You just simulated 7 reactions in 0.01s')",
                ]
            )
            self.assertEqual(returncode, 0)
            child_metrics = benchmark_matrix.read_child_metrics(metrics)
            self.assertEqual(child_metrics["returncode"], 0)
            self.assertEqual(child_metrics["event_count"], 7)
            self.assertGreaterEqual(child_metrics["wall_seconds"], 0)


if __name__ == "__main__":
    unittest.main()
