# Translation benchmark

This directory contains the reproducible engine-vs-engine benchmark used for
the Rasi translation optimization branch.

- `benchmark_engine_matrix.py` launches a fresh NFsim process per sample,
  alternates engine order, hashes binaries/inputs/outputs, and records wall
  time, simulation CPU, event count, and peak RSS when the host permits it.
- `rss_probe.py` runs exactly one NFsim child per worker so macOS
  `RUSAGE_CHILDREN.ru_maxrss` is not contaminated by earlier processes.
- `RESULTS.md` is the compact interpretation. Raw JSONL, summaries, and
  provenance are retained under `results/`.

The comparison uses stock release `v1.14.3` (`8ead2baa`), clean fork master
(`c51c7a3`), and this branch with `NFSIM_MEMFILTER=1`. The clean-master arm is
the correctness and attribution control; the release arm is the requested
published-release performance baseline.
