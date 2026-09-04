# NFsim translation benchmark results

## Protocol

All binaries were fresh Release builds (`-O3 -DNDEBUG`) produced with
AppleClang 21 as universal arm64/x86_64 executables. Every timing sample used a
new process. Engine order alternated. Identical XML bytes, seeds, simulation
horizons, output steps, update limit, molecule limit, and CPU limit were used
for all arms.

- Rasi-100 family, Rasi-500, and simple translation: five seeds, two repeats,
  normal and `-connect` modes (10 samples per engine/mode).
- uORF, nonstop circRNA, and promoter pausing: five seeds, normal and
  `-connect` modes (5 samples per engine/mode).
- Whole genome (529,228,331-byte XML): one bounded normal-mode smoke per
  engine; reported separately because clean master rejects this XML while
  returning exit code zero.
- Peak RSS: separate fresh-worker probes using per-child rusage.

The table reports median whole-process wall speedup in normal mode. Values
above 1 favor this branch.

| Model | Samples/engine | vs v1.14.3 | vs clean master | Exact vs clean master |
|---|---:|---:|---:|---:|
| Rasi-100 canonical | 10 | 2.72x | 3.86x | 10/10 |
| Rasi-100 TJ | 10 | 2.04x | 2.77x | 10/10 |
| Rasi-100 SAT | 10 | 2.18x | 2.64x | 10/10 |
| Rasi-100 SEC | 10 | 2.48x | 3.08x | 10/10 |
| Rasi-100 CAT | 10 | 2.35x | 3.10x | 10/10 |
| Rasi-100 CSAT | 10 | 2.32x | 2.78x | 10/10 |
| Rasi-100 CSEC | 10 | 2.34x | 3.16x | 10/10 |
| Rasi-500 original | 10 | 7.58x | 8.52x | 10/10 |
| Simple translation | 10 | 11.61x | 14.44x | 10/10 |
| uORF translation | 5 | 4.94x | 6.43x | 5/5 |
| Nonstop circRNA | 5 | 7.31x | 7.04x | 5/5 |
| Promoter pausing | 5 | 12.17x | 17.44x | 5/5 |

Across the reported 12-model matrix, optimized output hashes and event counts
match clean master for every paired condition (210/210 across normal and
`-connect`). Stock v1.14.3 often has a different seeded trajectory from clean
master, so release-vs-branch output identity is not used to attribute these
changes.

The Rasi-500 original input is primarily a load/startup stress (one reaction
in the bounded run). A separate event-heavy Rasi-500 oracle produced 3,489
reactions with the canonical output hash and measured kernel time of 6.662 s
on the recovered v3 baseline versus 0.0105 s on this branch (about 635x on
this host); whole-process CPU was 12.207 s versus 0.671 s (18.2x). Kernel
speedup depends strongly on horizon and state accumulation.

## Memory and large input

| Workload | v1.14.3 peak RSS | branch peak RSS | ratio | Wall speedup |
|---|---:|---:|---:|---:|
| Rasi-500 original | 1,878,816 KiB | 115,712 KiB | 16.24x lower | 7.94x |
| Whole genome original | 4,751,840 KiB | 2,587,120 KiB | 1.84x lower | 1.27x |

The 529 MB whole-genome XML completes on both v1.14.3 and this branch. Clean
master rejects it as invalid XML and produces no trajectory despite returning
zero; the benchmark harness treats missing events/output as failure.

## Connect-mode boundary

Against clean master, this branch is faster in every reported `-connect`
group (1.17x to 13.11x). Against v1.14.3, however, Rasi-100 `-connect` is
0.39x to 0.62x and Rasi-500 is 0.22x. This appears to be inherited current-
master behavior: the branch is 1.17x to 1.45x faster than its clean-master
control on those same conditions, but it does not recover the release's
older `-connect` performance.

## Validation

- 74/74 Python validation tests passed.
- 10/10 selected native component suites passed.
- Canonical Rasi hash matched
  `a1e661a995a80f5ec65267d0c907d637e30f137d6dad4eb47dcf959717921011`.
- Nine production-vs-differential comparisons were byte-identical with zero
  observed misses.
