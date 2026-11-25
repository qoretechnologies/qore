# Runtime comparison harness

Copyright (c) 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT.

This harness measures the same small computations ("kernels") in each Qore execution tier and, for reference, in Java.
It locates optimization opportunities in the tiers and measures the effect of a change against a baseline; it is not a
language ranking.  It was written for the performance work of qore#5578 (`feature/performance`); `RESULTS.md` holds that
work's final per-kernel numbers as the target for the changes ported from it.

## What it measures

`kernels.qr` holds 37 kernels: integer arithmetic, direct, method, monomorphic, polymorphic, optional and boxed method
calls, call references with integer, float and wide results (`indirect_*`), closures, list reads, builds and mutations,
and temporary lists and hashes.  `RuntimeBench.java` holds the same computations as JMH benchmarks.  Each kernel runs
`COUNT` items per invocation and is timed in fixed windows; the result is **nanoseconds per item** of the whole kernel,
including its loop and per-invocation work, not an isolated call latency.  Lower is better.

`run.py` runs these modes:

|!Mode|!What runs
|`qore-ast`|`qore --exec-mode=ast kernels.qr`
|`qore-ir`|`qore --exec-mode=ir kernels.qr`
|`qore-jit`|`qore --exec-mode=jit kernels.qr`
|`qore-aot`|an executable that `qcc -O3` compiles from `kernels.qr`
|`java-int`|`RuntimeBench` under JMH with `-Xint` (interpreter only)
|`java-jit`|`RuntimeBench` under JMH with HotSpot C2

Before any timing, every mode prints the results of every kernel for counts 0, 1, 17, 1024 and 4096 and seeds 0, 7 and
65535, and `run.py` compares them with an independent Python reference (`reference()`); a mismatch stops the run.
Jobs (mode, kernel, fork) then run in a shuffled order, each in a process of its own.  The table it prints is the
median, over the forks, of each process's mean ns/item.  Process start-up is measured separately.

## Prerequisites

- A Release (`-O3 -DNDEBUG`) Qore build with LLVM (JIT and `qcc`); by default `build/qore` and `build/qcc`.  The
  harness puts the directory of `--qore` first in `DYLD_LIBRARY_PATH` (macOS) or `LD_LIBRARY_PATH`, so the build's own
  `libqore` is used without installing it.
- Python 3.9 or newer.
- For the Java modes only: a JDK (`java`, `javac`).  The runner downloads JMH 1.37 and its three dependencies from Maven
  Central into the output root and checks their pinned SHA-256 digests on every run.  Select only Qore modes to run
  without Java or network access.

## Running

```sh
# the harness's own tests: no Qore, Java or network access needed
python3 -B -W error -m unittest discover -s bench/runtime-comparison -p "test_*.py"

# compile and check every mode against the reference, without timing
python3 bench/runtime-comparison/run.py --modes qore-ast qore-ir qore-jit qore-aot --check-only

# the baseline that BASELINE-*.md records: 4096 items, 3 forks, 3 warmup and 3 measured windows of 300 ms
python3 bench/runtime-comparison/run.py --modes qore-ast qore-ir qore-jit qore-aot \
    --count 4096 --forks 3 --warmup 3 --samples 3 --time-ms 300 --startup-samples 3

# a focused run
python3 bench/runtime-comparison/run.py --modes qore-ir qore-aot --kernels direct_call indirect_call --count 4096
```

`--qore`, `--qcc`, `--java` and `--javac` select other binaries; `--help` lists every setting.  Each run writes a new
`run-*` directory under `--output-root` (default `build/bench-runtime-comparison`) holding the source snapshot, every
command line, each process's output, `metadata.json` (binary and library digests, versions, settings), `samples.json`
(every window) and `summary.json`.  Measure on a quiet host: on a shared build host run it in a heavy-job slot
(`qbuild --heavy python3 bench/runtime-comparison/run.py ...`).

### Before/after comparisons

Save the shared library of the baseline (`libqore.dylib` or `libqore.so` and its link) in a directory before rebuilding,
and pass `--baseline-libdir <dir>` with the Qore modes.  The runner adds a `<mode>-before` job for each Qore mode, in
the same shuffled schedule, with the saved library first in the loader path; AOT is compiled once against each library,
so the comparison includes compiler changes.  For runtime-only changes, `--reuse-aot-for-baseline` runs the one
executable against both libraries.  The two libraries must be ABI-compatible with the tools.

## Other scripts

|!Script|!Measures|!Check
|`program_calls.py` / `program_calls.qr`|typed calls in the same Program and across Programs (IR), ns/call|`program_calls.py --check-only`
|`thread_pool.py` / `thread_pool.qr`|end-to-end task cost of a thread pool with and without a submitting sandbox (IR, AOT)|`thread_pool.py --check-only`
|`mixed_workloads.qr`|four synthetic processing workloads: CSV report, object orders, callback pipeline, text index|`mixed_reference.validate_checks()`
|`short_callbacks.qr`|batched short callbacks (integer and float, zero and one argument)|`short_callbacks.py <check output>`
|`opaque_callbacks.qr`|callbacks the compiler cannot see through|`opaque_callbacks.py <check output>`
|`ordinary_params.qr`|ordinary (non-native) function entries with int, float, string, list, hash and object parameters|`qore ordinary_params.qr check`
|`ShortCallbackBench.java`, `OpaqueCallbackBench.java`|the Java counterparts of the two callback scripts|-

Each Qore script takes `check` or `NAME COUNT WARMUP SAMPLES MS SEED` (see the script) and prints one record per
window.  The Qore scripts are `%modern`, run in one process and start no processes of their own.

## Child processes

Every process that the Python scripts start (`qore`, `qcc`, the AOT executables, `java`, `javac`, `git`) goes through
`run.run_bounded()`: it runs in a process group of its own with a timeout, and when the timeout passes or the run is
interrupted, the whole group is killed, so that a JVM that JMH forks ends too.  No script runs itself as an interpreter.
`examples/test/runner/test_runtime_comparison_bench.py` runs the harness's tests and checks these rules; it runs with
the other runner tests (`python3 -B -W error -m unittest discover -s examples/test/runner -p "test_*.py"`).

## Caveats

Host load, thermal state, frequency and CPU placement are not controlled.  The Qore and Java versions compute the same
results, but their runtimes differ in value representation, ownership and optimization opportunities.  Compare changes
(before/after on one host, in one run) rather than absolute numbers across hosts or revisions.
