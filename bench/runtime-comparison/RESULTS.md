# Qore IR and AOT performance findings

Copyright (c) 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT.

Java is useful here as two reference points: `-Xint` for interpreter costs and
warmed HotSpot for optimized native execution. The comparison identified an IR
return-path cost that was removed, reducing direct-call kernel time by 23.3%
and method-call time by 19.5% in an interleaved before/after run. The AOT gaps
point primarily to runtime helpers, not simply a lack of function inlining.

These are equivalent computations, not equivalent runtime contracts. Qore IR
is an [internal value-based compiler representation](../../design/qore-ir-spec.md);
Java bytecode is the JVM's [portable, primarily stack-based instruction format](https://docs.oracle.com/javase/specs/jvms/se25/html/jvms-2.html).
Their ability to feed
interpreted and compiled execution is the useful analogy. Comparing Qore AOT
with HotSpot also compares ahead-of-time optimization with runtime profiling
and speculation; it does not isolate the effect of compilation strategy.

## Environment and method

Measured 2026-10-06 on Apple M4 Pro, macOS 27.0.1 arm64. Qore 3.0.0 was built
in release mode (`-O3 -DNDEBUG`) with LLVM 22.1.8; AOT used `qcc -O3`. Java was
Temurin 25.0.2+10-LTS, JMH 1.37, with a 256 MiB fixed heap. The unchanged Qore
revision was `564d868fd8cc258797035a3ed419624853168d2f`.

The host was a shared workstation with background activity, including a busy
Steam process and browser processes. CPU placement, frequency, thermal state,
and filesystem caches were not controlled. Treat these results as optimization
evidence on this host, not production guarantees or a general language ranking.

The baseline used three processes per kernel/mode, three warmup windows and
five measured windows of 500 ms, with 1024 elements per invocation. All 480
pre-measurement results matched an independent integer reference. Jobs were
shuffled and executed sequentially; compilation, setup, and startup were timed
separately. See [README.md](README.md) for workload and harness differences.

[results-macos-arm64.json](results-macos-arm64.json) retains binary/source
hashes, settings, raw runtime samples, startup samples, and between-process
summaries for both experiments. It excludes local paths and environment data.
The runner gained source snapshotting and metadata hardening after the first
baseline; the kernel computations did not change between the two experiments.

## Baseline comparison

Numbers are nanoseconds per element, median of three process means; lower is
better. Each list construction/mutation result includes all its workload's
passes, not just an isolated append or assignment. These IR numbers precede
the tracing-cache change.

|!Kernel|!Qore IR|!Qore AOT|!Java interpreted|!Java warmed JIT
|arithmetic|103.36|7.08|7.14|0.99
|direct_call|493.84|7.25|17.02|1.00
|method_call|551.61|7.40|17.50|0.98
|list_read|222.17|8.77|84.54|0.30
|list_build|315.44|109.55|178.15|2.23
|list_mutate|218.63|207.35|389.39|6.04
|temporary_list|380.94|138.48|53.84|1.25
|temporary_hash|696.42|329.66|685.64|16.20

Launch-to-exit startup medians over five processes were 38.28 ms (IR),
34.26 ms (AOT), 26.05 ms (Java interpreted), and 27.09 ms (Java normal mode).
These include loading the benchmark and shutdown, but not compilation or JMH;
they are not cold-start or time-to-first-useful-result measurements.

The results distinguish several questions:

- AOT direct and method calls cost almost the same as arithmetic. Disassembly
  confirms the tiny recurrence callees are inlined; call inlining is already
  effective in these cases.
- AOT has little advantage over IR on `list_mutate`, suggesting common runtime
  helper work dominates. Different kernels can take different optimized paths;
  do not subtract `list_build` from `list_mutate` to estimate assignment cost.
- HotSpot's GC profiler still reports about 32 bytes per element for the
  temporary pair and 239.91 for the temporary hash. Its faster results do not
  prove those allocations were eliminated. General list build/mutate allocate
  about 38.60/62.56 bytes per element. Java uses boxed general lists but a
  primitive array for the temporary pair; Qore's representations differ.

## Implemented IR optimization

A separate five-second stack sample of IR `direct_call` attributed 740 of
3548 main-thread leaf samples (20.9%) to `__findenv_locked`. Normal IR returns
checked `QORE_IR_TRACE_EXCEPTIONS` with `getenv()` even when tracing was off.

[QoreIRInterpreter.cpp](../../lib/QoreIRInterpreter.cpp) now reads the flag once
into a thread-safe function-local `static const bool`, as existing IR trace
settings do. All eight exception/return diagnostic sites share that value.
No ownership, exception propagation, dispatch, or cancellation behavior changed.
The diagnostic configuration intentionally changes: enabling or disabling this
environment variable after the first IR execution has no effect. Presence,
including an empty value or `0`, still enables it. This is documented in the
environment-variable reference and release notes.

The original shared library was preserved before rebuilding. Both libraries
were then tested using the same executable and source snapshot, with shuffled
before/after jobs: three processes, three warmup and five measured windows of
300 ms. Both implementations passed all 120 reference checks.

|!Kernel|!Before ns/element|!After ns/element|!Time reduction
|arithmetic|102.89|102.78|0.1%
|direct_call|488.18|374.61|23.3%
|method_call|545.37|438.98|19.5%
|list_read|221.74|211.99|4.4%
|list_mutate|215.05|216.39|-0.6%
|temporary_list|377.05|375.71|0.4%

Direct-call process means ranged from 478.91 to 492.72 ns before and 372.44
to 378.89 ns after; method calls ranged from 541.12 to 551.26 before and
437.51 to 446.52 after. The control kernels' small differences have overlapping
ranges; the list-read ranges also overlap. Do not interpret them as established
improvements or regressions. A follow-up sample had 3683 main-thread samples
and no `getenv`/`__findenv_locked` stacks. These measurements establish a benefit
for the call-heavy kernels, not a 20% improvement for arbitrary programs.

Reproduce the interleaved run after preserving a compatible baseline library:

```sh
python3 bench/runtime-comparison/run.py --modes qore-ir \
    --baseline-libdir build/bench-runtime-comparison/before-lib \
    --kernels arithmetic direct_call method_call list_read list_mutate temporary_list \
    --forks 3 --warmup 3 --samples 5 --time-ms 300
```

## Original prioritized follow-up work

These are profile-backed targets, not implemented optimizations or predicted
speedups. Profiles were collected separately from measurements with macOS
`sample` at 1 ms intervals for five seconds. Counts below exclude the idle
signal-wait thread and use leaf samples, avoiding recursive double counting.

The cancellation and path-metadata portions were subsequently implemented;
see the measured follow-up below. The original profiling rationale is retained here.

### 1. Lower AOT cancellation polling cost

On AOT arithmetic, 1648 of 3697 main-thread samples were in sandbox-manager
lookup and 1089 in `qore_check_cancel` itself: 74.0% combined. TLS lookup and
the cancellation wrapper/stub account for additional samples. Disassembly
shows a `qore_rt_check_cancel` call on every loop backedge, matching the `Br`
lowering in [QoreIRToLLVM.cpp](../../lib/QoreIRToLLVM.cpp).

Investigate a cheaper negative-path check/context lookup in
[thread.cpp](../../lib/thread.cpp), retaining every existing poll initially.
The current code already avoids the old program mutex in common cases; the
remaining cost is not evidence that that earlier fix is absent. Preserve
thread cancellation, program interruption, sandbox selection, cancellation
scopes, deferral, and manager lifetime across nested/foreign calls. Test all
those cases before considering changes to polling frequency. Compare both
no-sandbox and sandboxed loops, including cancellation latency.

### 2. Specialize local typed-list mutation

AOT `list_mutate` spends substantial time in `LValueHelper` navigation and
cleanup, value type/discard helpers, path preparation, and allocation.
`qore_rt_lv_path_compound` in [JITRuntime.cpp](../../lib/JITRuntime.cpp) copies
and patches the lvalue path on each call before navigating it.

Investigate predecoded path metadata or an allocation-free single-index path,
then a guarded fast path for a proven unaliased local typed list. Preserve
copy-on-write, references, captured/shared variables, locking, bounds/NOTHING
behavior, type enforcement, and exception cleanup. Validate unique, aliased,
reference-bound, and closure-captured variants separately. This target can
benefit both IR and AOT because they share runtime helpers.

### 3. Extend aggregate scalar replacement and simplify construction

AOT `temporary_list` still calls `qore_rt_make_list_by_type_path_cached`,
local assignment, reference-counting, indexing, and cleanup helpers per
iteration. Samples include allocation/free, type validation, complex-list
type lookup/locking, and cached type-path resolution. Existing caches are
present; the issue is remaining per-operation work, not an absent cache.

The aggregate scalarizer in [QoreIRAnalysis.cpp](../../lib/QoreIRAnalysis.cpp)
currently requires a constant integer/string selector. It rejects this pair's
runtime `seed & 1` index. Extend it to a proven bounded selector lowered to a
select between scalar elements, with a fallback for unsupported escapes or
index semantics. Finite-key hash selection is a related experiment. Preserve
operand evaluation order, type checks, cleanup, and exception behavior.

For aggregates that cannot be eliminated, investigate prevalidated typed
construction and stable per-context type metadata. Measure actual allocation
counts before claiming scalar replacement; Java's surviving allocation here
is a useful warning against drawing that conclusion from timing alone.

### 4. Reduce remaining IR frame and value bookkeeping

After removing `getenv`, direct-call samples show interpreter execution,
allocation/free, frame reset, value discard/conversion, and local-variable
bookkeeping. Extend existing frame reuse or typed fast-call paths based on
that profile. First determine which reusable frame buffers still allocate and
why the recurrence does not use a narrower existing leaf path. Preserve
debugger visibility, recursive calls, reentrancy, and exception unwinding.

For each candidate, use an interleaved Qore before/after comparison and repeat
with several batch sizes and real application workloads. Java helps reveal
cost categories; Qore's own unchanged baseline determines whether a change
actually improves Qore.

## Validation

The release build, all-mode benchmark reference checks and smoke run, six
runner tests, and five focused IR/call/exception test files passed. The new
tracing regression has four cases and 41 assertions covering unset, enabled,
empty, and zero values plus later environment changes. It fails on all four
cases against the saved old library, confirming the intended behavioral
difference. See [AUDIT.md](AUDIT.md) for the complete pre-commit audit.

## Measured runtime follow-up (2026-10-06)

The baseline for this follow-up is `125ca95e48cb648460397bcf6f1ed891d2b94ede`,
which already includes the tracing-cache optimization above. These percentages
must not be added to the earlier tracing results. The platform, release build,
kernel computations, and shared-workstation caveats are unchanged. Java was
used to prioritize the work; unchanged Qore is the before/after control.

Each candidate's library and compatible tools were saved before the next edit.
IR and AOT before/after jobs are shuffled and interleaved. The harness compiles
an AOT executable with each library and runs it against that same library, so
the baseline never receives the candidate compiler's optimizations. Each mode
passes 120 independent reference checks before timing; steady-state timings
exclude build, startup and setup. Reported reductions are `100 * (1 - after/before)`.

### Retained changes

1. **Inactive cancellation checks:** a conservative sequentially consistent
   process-wide pending-interrupt count skips manager/context resolution when
   zero. Thread cancellation and deferral still run first. A nonzero count
   uses full governing-manager lookup. Every existing cancellation point and
   blocking-wait handshake is preserved. This also skips lookup for a sandboxed
   context with no pending interruption, although these kernel timings are
   unsandboxed. No cancellation-latency speedup is claimed.
2. **Native indexed compound mutation:** the JIT/AOT helper passes a stack-local
   index override for a two-step root/index path instead of copying vectors and
   root strings. The existing navigation still handles references, locks, COW,
   auto-vivification, negative indices, type checks and exception cleanup. It is
   not an unchecked direct write into a list.

### Isolated experiments and rejected candidates

The cancellation-only experiment used three forks per mode/kernel, three warmup
and five measured 300 ms windows, with 1024 elements per invocation:

|!Kernel/mode|!Before ns/element|!Cancellation only|!Time reduction
|arithmetic AOT|6.92|3.83|44.7%
|direct_call AOT|6.93|3.84|44.6%
|method_call AOT|7.12|4.02|43.5%
|list_read AOT|8.53|5.28|38.1%
|list_mutate AOT|197.63|192.81|2.4%
|direct_call IR|368.21|368.31|-0.03%

An initial indexed-path candidate applied the shortcut in both IR and native
execution. Against the cancellation-only runtime it reduced `list_mutate`
from 196.30 to 180.16 ns/element in IR (8.2%) and 189.10 to 165.01 in AOT
(12.7%), using the same three-fork settings. However, a five-fork cumulative
repeat found several IR controls slower: arithmetic 96.64 to 99.95 (3.4%),
list reads 208.26 to 219.07 (5.2%), temporary lists 368.69 to 381.61 (3.5%),
and temporary hashes 674.45 to 694.80 (3.0%). The interpreter specialization
was therefore removed and the narrower native change remeasured. Code-layout
effects are plausible, but the exact cause of those control regressions was
not established; they are not dismissed as noise.

A bounded integer-pair scalar-replacement prototype was also rejected. It
initially changed a full-width boxed integer result to zero after a statement
temporary drain: `NativeInt` does not prove reference-free storage in Qore.
Restricting elements to proven inline integer ranges passed all-mode semantic
checks, but the available facts could not prove the benchmark's elements safe,
so its list was still materialized (`scalar-lists=0`). No scalarizer change is
committed. `IRBoundedPair.qr` retains the edge cases. The next step is sound
range/lifetime propagation or explicit scalar ownership, not merely a bounded
index check. Dynamic hash scalarization and IR frame reuse also remain future work.

### Final retained revision

Five independent processes per mode/kernel, three warmup and five measured
300 ms windows per process, 1024 elements per batch (160 jobs, 800 timing
samples, 480 reference checks). Values are medians of process means in
ns/element; positive reductions mean less time.

|!Kernel|!IR before|!IR after|!IR reduction|!AOT before|!AOT after|!AOT reduction
|arithmetic|99.53|93.25|6.3%|7.02|3.83|45.4%
|direct_call|366.76|364.07|0.7%|7.00|3.86|44.9%
|method_call|422.89|420.22|0.6%|7.17|4.03|43.8%
|list_read|208.59|205.91|1.3%|8.47|5.28|37.6%
|list_build|295.43|289.26|2.1%|103.15|96.99|6.0%
|list_mutate|205.66|184.73|10.2%|198.48|173.47|12.6%
|temporary_list|366.90|366.02|0.2%|132.54|128.51|3.0%
|temporary_hash|673.88|676.17|-0.3%|315.91|315.66|0.1%

The largest AOT gains are clearly separated across processes: arithmetic
ranged from 6.93-7.11 before versus 3.79-3.84 after; list reads from 8.38-8.99
versus 5.21-5.36; list mutation from 197.77-201.77 versus 169.92-174.84.
IR list mutation ranged from 202.78-208.77 versus 181.92-189.47. These are
cumulative measurements of the retained runtime, not proof that every IR gain
comes from the native-only path shortcut. The interpreter's private path copy
is unchanged.

IR direct/method calls and temporary-list/hash differences have overlapping
process ranges and do not establish a benefit. Temporary hashes were essentially
unchanged in both modes at this batch size; the larger-batch result below is less favorable.
Startup was also effectively unchanged: IR 33.26 to 33.09 ms, AOT 29.60 to
29.60 ms. These are warm-cache launch-to-exit measurements, not cold-start claims.

### Larger-batch confirmation

At 4096 elements, three forks and the same 300 ms windows (48 jobs, 240 timing
samples, 480 reference checks):

|!Kernel|!IR before|!IR after|!IR reduction|!AOT before|!AOT after|!AOT reduction
|arithmetic|97.27|91.80|5.6%|6.64|3.43|48.3%
|list_read|206.19|200.61|2.7%|8.11|4.92|39.3%
|list_mutate|206.51|182.57|11.6%|196.11|171.31|12.6%
|temporary_hash|687.35|671.41|2.3%|306.45|313.84|-2.4%

The AOT hash result prompted a focused repeat at 4096 elements with five forks
and longer 500 ms windows. That repeat was 307.40 before versus 305.72 after
(0.5% less time), with broad overlapping ranges: 300.33-319.11 before and
303.62-316.30 after. Thus the data show neither a consistent hash improvement
nor a consistently reproduced regression. The 2.4% slower observation is retained
in the evidence; allocation-heavy hash workloads still need dedicated profiling.

The practical result is a substantial reduction in tight-loop AOT overhead and
about 10-12% lower IR / 13% lower AOT time for this list-mutation workload.
It is not a general 45% Qore speedup. Application-level measurements, sandboxed
throughput/latency measurements and other platforms remain necessary before
extrapolating these results.

Reproduce after saving a compatible baseline library as described in the README:

```sh
python3 bench/runtime-comparison/run.py --modes qore-ir qore-aot \
    --baseline-libdir /path/to/saved-library \
    --forks 5 --warmup 3 --samples 5 --time-ms 300
python3 bench/runtime-comparison/run.py --modes qore-ir qore-aot \
    --kernels arithmetic list_read list_mutate temporary_hash \
    --baseline-libdir /path/to/saved-library --count 4096 \
    --forks 3 --warmup 3 --samples 5 --time-ms 300
python3 bench/runtime-comparison/run.py --modes qore-aot \
    --kernels temporary_hash --baseline-libdir /path/to/saved-library \
    --count 4096 --forks 5 --warmup 3 --samples 5 --time-ms 500
```

[results-followup-macos-arm64.json](results-followup-macos-arm64.json) contains
all six comparison experiments: source/library/executable hashes, settings,
startup samples, raw timing samples and process summaries, without local paths
or environment values. Full local commands/output remain in the runner's build
directories. [FOLLOWUP-AUDIT.md](FOLLOWUP-AUDIT.md) records the complete
`audit-changes` checklist, 175 passing QUnit cases (746 assertions), four
standalone regression scripts, and the measurement/validation scope.
