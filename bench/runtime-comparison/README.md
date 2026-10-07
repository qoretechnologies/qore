# IR/AOT runtime comparison

Copyright (c) 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT.

This suite compares Qore IR, Qore AOT, Java interpreted execution (`-Xint`), and
warmed HotSpot execution. It is intended to locate optimization opportunities,
not produce a single language ranking. Running the suite does not modify
installed runtime configuration or production code.

## Running

Use a release Qore build with LLVM/AOT support, Python 3.9 or newer, and a JDK
with `java` and `javac`. Maven is not required. The runner downloads JMH 1.37
and three dependencies from Maven Central into the output directory, verifying
pinned SHA-256 hashes on every run. Dependencies and generated classes/binaries
are build artifacts, not installed or committed modules.

```sh
# Validate the runner's reference, parsers, and argument checks without runtimes.
python3 bench/runtime-comparison/test_runner.py

# Compile and check all implementations against an independent reference.
python3 bench/runtime-comparison/run.py --check-only

# Short integration smoke run; insufficient warmup for performance conclusions.
python3 bench/runtime-comparison/run.py \
    --forks 1 --warmup 1 --samples 2 --time-ms 50 --startup-samples 2

# Default baseline: 3 independent processes per kernel/mode,
# 3 warmup and 5 measurement windows of 500 ms each.
python3 bench/runtime-comparison/run.py

# Focused run, also usable when no Java installation is available.
python3 bench/runtime-comparison/run.py --modes qore-ir qore-aot \
    --kernels direct_call method_call --count 4096
```

`--qore`, `--qcc`, `--java`, and `--javac` select other installations. The Qore
defaults are `build/qore` and `build/qcc`. The runner prepends the Qore binary's
directory to `DYLD_LIBRARY_PATH` on macOS or `LD_LIBRARY_PATH` elsewhere; use
compatible binaries/libraries. The default output root is
`build/bench-runtime-comparison`; every run creates a unique `run-*` directory.
Run `--help` for all settings.

For an interleaved before/after IR and/or AOT comparison, preserve the original shared
library and its `libqore.dylib`/`libqore.so` link in a separate directory before
rebuilding. Pass `--modes qore-ir qore-aot --baseline-libdir /path/to/saved-library`.
The runner adds corresponding `-before` jobs, using the same tools, source snapshot,
settings, and shuffled schedule with the saved library selected by its loader
path. AOT is compiled twice: once against each library, then run against that
same library. This measures compiler optimizations as well as runtime changes;
it does not run a newly optimized executable against an old runtime and call
that a baseline. The two libraries must be ABI-compatible with the tools. Library
and executable hashes and both compile times are recorded. Preserve the tools
too if a future change makes them incompatible; this option is for compatible
shared-library comparisons only.

## Workloads and units

Each invocation processes `--count` elements, normally 1024. The reported unit
is nanoseconds per element, including all passes in that workload. One list
build therefore means appending one element plus its share of the final
checksum traversal; it is not a measurement of an isolated append instruction.

|!Kernel|!Work
|arithmetic|Dependent integer recurrence and checksum
|direct_call|Same recurrence through a small function
|method_call|Same recurrence through a method on a final class; receiver created outside timing
|indirect_call|Typed Qore function reference / Java LongUnaryOperator; callback created outside timing
|indirect_wide|Same low-bit recurrence with bit 50 retained across the callback boundary; checksum uses low 16 bits
|indirect_float|Double callback with bounded addition/subtraction and an integer checksum
|indirect_tagged_float|Double callback alternating -3e307 and -6e307, which require heap encoding when boxed in Qore
|indirect_branch_float|Comparison/branching float target that retains generic Qore dispatch; fallback control
|polymorphic_call|Three receiver types selected cyclically, with different recurrence increments
|dispatch_control|Same receiver-selection arithmetic and increments without method calls
|list_read|Indexed traversal of a list prepared outside timing; XOR each element with the invocation seed
|list_build|Build a growing list, then read every element into the checksum
|list_mutate|Build a growing list, then increment and read every element
|temporary_list|Construct two elements per iteration and select with a runtime index
|temporary_hash|Construct a two-key hash per iteration and select with a runtime key

Qore integers are compared to Java `long`; the original recurrence keeps values in
0..65535, while the wide callback retains bit 50. All intermediates and checksums
fit signed 64-bit integers. A changing seed prevents
constant inputs across invocations. The loop-carried recurrence also avoids
the trivial closed-form sum used by some older loop benchmarks.

Java uses `ArrayList<Long>` for the general list kernels, `long[2]` for the
temporary fixed-size pair, and `HashMap<String, Long>` for the hash kernel.
These are equivalent observable computations, **not identical representations
or language semantics**. In particular, Qore's ownership, copy-on-write,
NOTHING, and cancellation semantics differ from Java's. Java's boxed elements
can allocate while Qore's small integers are inline. A compiler may eliminate
temporary aggregates; this is an intended optimization opportunity. Do not
interpret a temporary-pair result as a general list throughput measurement.

The original direct/final-method kernels permit inlining. The new indirect and
polymorphic kernels retain calls in Qore AOT; Java's target methods explicitly
use JMH `CompilerControl(DONT_INLINE)`. These are retained-call comparisons,
not a claim that default HotSpot would leave the calls uninlined. A Qore typed
call reference and a Java functional interface have different runtime contracts.
The polymorphic control measures the selection/recurrence work, but subtracting
it does not isolate an exact virtual-call cost.

## Measurement contract

- Each selected mode first passes 225 checks: every kernel at sizes 0, 1, 17, 1024,
  and 4096, each with seeds 0, 7, and 65535. Python independently computes the
  expected results. Missing, duplicate, extra, or incorrect records fail.
- Kernel definitions are identical between Qore IR and AOT. There are no
  imported user modules that could execute as AOT during an IR measurement.
- Compilation and data setup are outside the steady-state timer. Qore uses
  `clock_get_monotonic_us()`; JMH supplies Java's measurement loop. Every
  process warms up before its own measured samples.
- Qore returns and prints checksum reductions after each window; JMH consumes
  Java return values. Qore includes one kernel dispatch, checksum XOR, seed
  update, and clock query per batch. Java includes the JMH harness and seed
  update. This overhead is amortized over `count`, not subtracted. Check
  sensitivity at several counts before interpreting very small differences.
- JMH uses one worker thread, one child JVM per job, a 256 MiB initial/maximum
  heap, and the GC profiler. Java allocation bytes per element include profiler
  noise. Qore allocation rates are not inferred from Java's profiler.
- Jobs are shuffled reproducibly and run sequentially. Independent process
  means are summarized by their median; raw within-process samples and
  between-process ranges are retained. Samples within one process are not
  treated as independent forks.
- Startup is measured separately as process launch to exit on the `startup`
  command. It includes runtime initialization, loading/parsing the benchmark,
  a ready line, and teardown. It is **not** a cold-disk or time-to-first-result
  measurement. Filesystem caches are uncontrolled. Java startup does not
  launch JMH or use the steady-state heap flags.
- Sources are copied into the run directory before compilation and execution,
  so concurrent edits cannot change kernels halfway through a measurement.
- Failures propagate; stderr is saved, not suppressed. Results include exact
  commands, source/binary hashes, versions, relevant environment, compile time,
  startup samples, JMH JSON, and all raw output.

The default duration is a starting point. Inspect warmup traces, variance, and
generated code; increase warmup and forks when necessary. Keep the host quiet
and record background load. Use Qore before/after measurements with the same
inputs and settings to establish improvements; a Java ratio alone is not
evidence that a particular Qore change is safe or effective.

## Thread-pool and sandbox context

`thread_pool.py` measures Qore IR/AOT task delivery with a persistent one-worker
pool, both without a sandbox and with a sandbox attached to the submitting
`Program`. No Java installation or downloaded dependency is needed:

```sh
python3 bench/runtime-comparison/thread_pool.py --check-only
python3 bench/runtime-comparison/thread_pool.py \
    --baseline-libdir /path/to/saved-library --forks 3 --time-ms 300
```

Each task either does no container work or builds, increments, and sums 1024
integers. Every result is checked. Batch size 1 measures serial round-trip
cost; batch size 64 measures amortized delivery/throughput cost. Both report
**nanoseconds per task**, including submission, worker execution/recycling,
queue delivery, and validation. Neither reports an individual-task latency
percentile. Divide one billion by ns/task to obtain completed tasks/second.

The submitting wrapper has the same call-stack shape in both contexts. It is
entered before timing, and the worker closure belongs to the trusted parent
Program, like work supplied by a library. The sandbox context must therefore
propagate from submission to the worker. This is a synthetic throughput probe,
not a sandbox enforcement test or an HTTP-server benchmark; the existing
thread-pool and HTTP sandbox regression tests check policy enforcement.

Compilation, worker startup, and the first batch are outside timing. Queue
reads have a 30-second timeout; task exceptions are reported as failed results.
As with `run.py`, jobs are shuffled and sequential, results summarize process
means, and sources, hashes, commands, raw output, and settings are saved in a
unique ignored `pool-*` directory. Before/after libraries must be ABI-compatible;
AOT code is compiled and run against its corresponding library.

## Program-boundary calls

`program_calls.py` is a separate **IR-only** probe, not a Java or AOT comparison:

```sh
python3 bench/runtime-comparison/program_calls.py --check-only
python3 bench/runtime-comparison/program_calls.py \
    --baseline-libdir /path/to/saved-library --forks 3 --time-ms 300
```

Two programs explicitly pinned to IR parse identical code. A typed function-reference loop runs in one
program and calls either its own function or the second program's function.
The outer `Program::callFunction` enters the measuring program before timing;
it is not invoked once per measured call. Both cases retain the same outer call
stack and keep both programs alive. This measures the whole callback workload
in ns/call, not an isolated context-switch instruction. It does not test sandbox
policy overhead, module loading, or cross-program AOT-to-AOT calls.

Thirty checks cover both cases, five sizes, and three seeds against the Python
reference before timing. Independent processes warm up separately; a seeded
shuffled schedule interleaves scenarios and compatible before/after libraries.
The `calls-*` output preserves sources, library hashes, commands, raw records,
and medians of fork means. No external modules or Java dependencies are needed.

## Investigating a gap

The function-reference optimization can be isolated with
`QORE_DISABLE_FUNCREF_FAST_CALL=1` at execution (disables both new paths), or
`QORE_DISABLE_AOT_FUNCREF_NATIVE_ENTRY=1` while compiling (suppresses native
adapters but retains guarded runtime dispatch). Native adapters require AOT
recompilation; old binaries still work with the updated runtime. The
`indirect_call` kernel keeps its target opaque inside the loop, so the measured
call is not replaced by a direct inlined recurrence. See the
[calling-convention contract](../../design/qore-jit-aot-current-state.md#opaque-function-reference-calls)
for the conservative scalar-body eligibility and remaining fallbacks.

`QORE_DISABLE_AOT_TYPED_FUNCREF_CALL=1` at compilation isolates the additional
zero/one-argument scalar ABI while retaining the boxed adapter and cached
signature metadata. References constructed before parse finalization are not
cached. Runtime state and actual argument types remain guarded. The typed ABI
does not remove diagnostic frames. Exact native-local consumers can additionally
use the unboxed continuation below.

`QORE_DISABLE_AOT_FUNCREF_NATIVE_CONTINUATION=1` at compilation disables the
single-use callback-result continuation into an exact same-typed native scalar
local. The enabled native edge carries its result without boxing; the generic
edge retains owned boxed values and full assignment checking. Optional/soft,
captured/reference and multiple-use destinations keep ordinary lowering, as do
invoke-form calls. This changes AOT code generation only, not the runtime ABI.
The `indirect_wide` kernel keeps values above int48 range, `indirect_float`
uses exactly representable bounded doubles, and `indirect_tagged_float`
alternates large negative doubles requiring heap encoding when boxed in Qore.
The latter uses subtraction and a threshold checksum so rounding around the
decimal constants does not change the intended alternating checksum.
`indirect_branch_float` retains generic dispatch as a fallback control. Their
Java targets use DONT_INLINE like the original `indirect_call`; checksums are
validated independently before timing. These are separate workloads, not
interchangeable measurements of one call latency. Native-local argument loads
reuse the existing assigned-state proof instead of boxing values solely to
recheck their tags; this also removes the old int48-only bottleneck for wide
arguments. Generic immediate scalar returns keep their inline assignment guard.

`QORE_DISABLE_NATIVE_SCALAR_ASSIGN_GUARD=1` at compilation disables inline
assignment checks for matching immediate values stored into exact native
`int`, `float`, or `bool` locals. The enabled JIT/AOT path avoids general
coercion/type resolution for those values, including boxed callback results.
Nonmatching and heap-encoded values still use the original coercion path;
typed callable declarations alone are not accepted as proof of actual results.

`QORE_DISABLE_LLVM_SCALAR_DECREF_GUARD=1` at compilation disables the late
LLVM cleanup guards for immediate int48, bool and NOTHING values in AOT functions
containing typed native callback calls. Other functions are unchanged: broad
guard insertion regressed direct-call and dispatch-control loops. The enabled
pass runs after the normal optimizer and only simplifies locally afterward;
inserting these guards before the full pipeline caused excessive code growth.
Opaque references, heap numbers and other representations retain the original
release operation. `QORE_IR_OPT_STATS=1` reports the number of inserted guards.
This trades additional native code for fewer runtime cleanup calls; compare
code size and compilation time as well as steady-state performance.

`QORE_DISABLE_LLVM_CANCEL_RUNTIME_CONFIG=1` at compilation selects legacy TLS
polling for generated JIT/AOT loops. The enabled path acquires a nullable,
borrowed live `RuntimeConfig*` once per invocation and passes it to
`qore_rt_check_cancel_v2`. Null selects the legacy path, including during
thread-data teardown. Polling frequency, deferral, cancellation priority,
Program scope and atomic ordering are unchanged. Uncommon exception/manager
work is outlined for both polling paths; this switch isolates context passing,
not that shared runtime improvement. Old AOT artifacts remain supported.

`QORE_DISABLE_AOT_FUNCREF_RUNTIME_CONFIG=1`
at compilation selects the original typed callback helpers instead of v2,
which acquires an explicit live execution context once per generated invocation.
Use separate shuffled runs with the same settings to measure this switch;
do not combine disabled-path measurements with before/after library labels.
V2 preserves diagnostic frames, policy overrides, stack-reader synchronization,
and lazy context materialization. It is not an end-to-end TLS-free native ABI.
Compiler-proven scalar leaves retain their local/closure frame markers but
avoid general variable-cleanup bookkeeping for those empty frames. The native
closure-marker push handles an available slot inline and outlines full-block
growth/reuse; the entry fields and publication ordering are unchanged. V1 remains
available as a functional control; this does not broaden native eligibility.

Use the `kernels-aot` executable in a run directory, or invoke `kernels.qr`
directly. Arguments are `KERNEL COUNT WARMUP SAMPLES MS SEED`:

```sh
build/qore --exec-mode=ir bench/runtime-comparison/kernels.qr direct_call 1024 2 5 1000 7
```

Profile a separate run so sampling does not affect the baseline. On macOS,
`sample PID 5 1 -file PROFILE.txt` records a five-second stack sample of a
running benchmark. On Linux, use `perf record -g -- COMMAND ...` followed by
`perf report`. A longer measurement window keeps the process alive for attach.

Inspect Qore's lowering with `QORE_AOT_DUMP_IR=1` or
`QORE_DUMP_LLVM_IR=1` during a separate `qcc` compilation. The latter dump is
before final LLVM optimization, so inspect the emitted executable's
disassembly too. `QORE_IR_OPT_STATS=1` reports optimization coverage. Existing
`QORE_DISABLE_*` switches allow controlled experiments; set compile-time
switches while running `qcc`, not only while executing its output.

Useful source references:

- [Qore IR design](../../design/qore-ir-spec.md)
- [Ownership and copy-on-write constraints](../../design/lvalue-loads-in-ir.md)
- [JMH project and benchmarking guidance](https://github.com/openjdk/jmh)
- [HotSpot optimization overview](https://docs.oracle.com/en/java/javase/25/vm/java-hotspot-virtual-machine-performance-enhancements.html)

See [RESULTS.md](RESULTS.md) for the initial measurements and follow-up targets.
