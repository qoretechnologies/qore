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

## Post-merge container follow-up (2026-10-06)

The next baseline is the clean `feature/performance` merge
`6c87c18cf17599bcafd62b1240c08fdd551b39b4`, including `origin/develop` at
`6d94b1848`. It already contains the earlier performance changes and the merged
sandbox/thread-pool behavior. These measurements do **not** isolate the cost of
that merge: the new optimizations are compared against this post-merge baseline.

### Refreshed Java comparison

Same machine, release build, LLVM 22.1.8 and Temurin 25.0.2/JMH 1.37 as above.
Three independent processes, three warmup and five measured 300 ms windows,
1024 elements. All 480 reference checks passed. Values are ns/element:

|!Kernel|!Qore IR|!Qore AOT|!Java interpreted|!Java warmed JIT
|arithmetic|92.81|3.80|6.88|0.89
|direct_call|364.36|3.82|16.36|0.88
|method_call|418.35|4.01|16.56|0.89
|list_read|203.47|5.25|78.24|0.29
|list_build|281.53|95.27|165.43|2.09
|list_mutate|177.73|171.78|359.89|5.84
|temporary_list|355.16|126.73|50.07|1.16
|temporary_hash|664.08|311.75|638.47|15.28

Launch-to-exit medians were 39.31/34.62/27.70/27.48 ms for IR/AOT/Java
interpreted/Java normal mode. These are not cold-start measurements.

The remaining AOT gap is largest in container work: about 46 times Java's
warmed time for growing lists, 29 times for mutation, 109 times for temporary
pairs, and 20 times for hashes. These compare different representations and
runtime contracts, not identical implementations. The pair still allocates
about 32 bytes/element under JMH's GC profiler and the hash about 240; Java's
advantage here is not evidence that either allocation disappeared.

IR direct/method calls take about 22/25 times Java interpreted execution in
these kernels. In contrast, tiny AOT calls are almost as cheap as AOT arithmetic.
That makes IR frame/value bookkeeping and shared container helpers useful
separate targets; implementing more tiny-function inlining is not the first
priority for this AOT workload.

The workstation was not isolated. Backup/sync, desktop, and browser processes
were active at different points; they were not stopped. Runtime builds, Qore
regression suites, and stack sampling were kept out of timing runs; lightweight
report/harness validation and desktop activity were not eliminated. Small changes and cross-run comparisons
require caution; the direct before/after experiments below are interleaved.

### Profile-driven changes

Separate five-second macOS stack samples of AOT list construction, mutation,
and temporary pairs showed runtime assignment/cleanup, allocations, type
comparisons, and type lookup in hot paths. Two limited changes were retained:

- `QoreTypeInfo::superSetOf()` skips its ordered structural comparison when both
  operands have concrete type metadata and are the same pointer. Null/auto
  semantics, unequal-type comparisons, conversions, and assignment validation
  are unchanged.
- `LValueHelper` holds its first deferred old reference inline, with vector
  overflow. The old code heap-allocated that vector even for a single reference.
  Lock release, recursive scans, release order, exception cleanup, and semantic
  alias ownership are preserved. This removes a temporary bookkeeping allocation,
  not the actual list/hash allocation. The helper's internal size grows by one
  pointer; public class layouts and helper ABI are unchanged.

Both apply to shared runtime operations, including IR and native/AOT paths;
neither bypasses COW or introduces a special language-semantic shortcut.

Isolated comparisons used three forks and 200 ms windows (three warmup, five
measured). The first compares metadata identity alone with the merged baseline;
the second adds inline reference storage against a saved identity-only library:

|!Experiment and kernel|!IR before|!IR after|!AOT before|!AOT after|!AOT time reduction
|Identity: list_build|285.60|281.23|96.28|94.14|2.2%
|Identity: list_mutate|180.83|178.98|173.57|171.49|1.2%
|Identity: temporary_list|363.22|359.00|127.03|120.51|5.1%
|Identity: temporary_hash|663.64|670.11|319.21|305.35|4.3%
|Inline reference: list_build|279.73|281.52|95.31|81.17|14.8%
|Inline reference: list_mutate|178.20|165.21|171.17|155.64|9.1%
|Inline reference: temporary_list|360.82|367.48|121.05|121.36|-0.3%
|Inline reference: temporary_hash|668.96|665.60|308.15|309.06|-0.3%

These effects must not be added across runs. The identity-only arithmetic
controls were 3.2% slower in IR and 1.6% slower in AOT; the inline-storage
experiment was 3.3% faster in IR and 1.2% slower in AOT. Mixed controls are
retained in the evidence, not omitted in favor of successful targets.

### Combined retained changes

The final comparison uses the unchanged merged library and the rebuilt library
containing both changes: five independent forks, three warmup and five measured
300 ms windows, 1024 elements. All 480 reference checks passed; 160 shuffled jobs
produced 800 timing samples. Positive reduction means less elapsed time:

|!Kernel|!IR before|!IR after|!IR reduction|!AOT before|!AOT after|!AOT reduction
|arithmetic|94.42|94.21|0.2%|3.80|3.83|-0.7%
|direct_call|363.43|361.44|0.5%|3.80|3.84|-1.2%
|method_call|414.29|411.75|0.6%|4.04|4.03|0.1%
|list_read|204.50|203.63|0.4%|5.31|5.30|0.2%
|list_build|285.39|284.22|0.4%|96.63|82.36|14.8%
|list_mutate|182.13|167.65|8.0%|171.59|156.69|8.7%
|temporary_list|358.64|360.57|-0.5%|127.31|122.25|4.0%
|temporary_hash|662.74|665.48|-0.4%|330.26|304.71|7.7%

AOT list-build process means ranged from 94.22-99.22 ns before to 81.65-82.65 ns
after; list mutation from 167.68-174.40 to 155.98-157.47 ns. The corresponding IR
mutation ranges were 178.58-182.28 and 164.97-168.46 ns. These targeted reductions
exceed the observed between-process spread in this run.

Other IR kernels are essentially flat here. AOT arithmetic/direct-call controls
are slightly slower (roughly 0.02/0.04 ns per element); no improvement is claimed
for them. Hash speedup magnitude varies between experiments. Do not generalize
the best kernel's percentage to all Qore programs.

### Larger batches and IR controls

At 4096 elements, three forks and the same 300 ms windows produced 72 jobs and
360 timing samples, with another 480 reference checks:

|!Kernel|!IR before|!IR after|!IR reduction|!AOT before|!AOT after|!AOT reduction
|arithmetic|93.83|92.62|1.3%|3.40|3.43|-0.9%
|direct_call|362.41|364.02|-0.4%|3.41|3.44|-0.8%
|list_build|279.27|287.11|-2.8%|96.28|82.70|14.1%
|list_mutate|181.09|164.17|9.3%|171.86|158.47|7.8%
|temporary_list|357.55|362.99|-1.5%|129.12|122.47|5.2%
|temporary_hash|662.64|669.62|-1.1%|317.23|306.07|3.5%

AOT container improvements and the IR mutation improvement repeat at the larger
batch size. The 2.8% slower IR construction result has overlapping process ranges
(278.82-288.10 before, 282.91-292.67 after); the temporary-list result also
overlaps (351.91-363.83 before, 362.67-367.62 after). They are retained as possible
small tradeoffs, not reclassified as improvements.

A focused five-fork repeat with longer 500 ms windows measured IR construction
at 281.58 before versus 286.70 after (1.8% slower), and temporary lists at
361.25 versus 363.65 (0.7% slower). Ranges again overlap: 274.46-292.05 versus
279.67-295.18 for construction and 358.33-362.30 versus 359.58-367.25 for pairs.
The slower construction median repeats at 4096 elements, so it remains a
potential cost of the retained changes, even though the 1024-element run was
flat. The changes are retained for the larger AOT/container-mutation benefits,
not presented as uniformly beneficial to IR. This repeat adds 20 jobs, 100
timing samples, and 240 reference checks.

### Thread-pool delivery with sandbox context

Three forks, three warmup and five measured 300 ms windows; 96 shuffled jobs,
480 timing samples, 32 reference checks. Both variants use a persistent
single-worker pool. The container task builds, increments, and sums 1024
integers. Values below are **microseconds per completed task**, not ns/element:

|!Context / work / batch|!IR before|!IR after|!AOT before|!AOT after
|Plain / no-op / 1|11.83|11.72|11.44|11.46
|Sandbox / no-op / 1|11.83|11.77|11.43|11.60
|Plain / no-op / 64|8.50|8.44|8.62|8.48
|Sandbox / no-op / 64|8.47|8.54|8.60|8.44
|Plain / container / 1|198.83|184.40|184.75|171.73
|Sandbox / container / 1|194.29|182.95|189.83|169.88
|Plain / container / 64|196.00|182.01|189.08|168.84
|Sandbox / container / 64|196.32|177.06|186.12|169.81

The container tasks take 5.8-9.8% less time in IR and 7.0-10.7% less in AOT.
For sandboxed AOT, serial round trips improve by 10.5% and batched delivery by
8.8%; the latter corresponds to about 5373 to 5889 completed tasks/second.
No-op delivery changes are small and mixed, within about 2% in either direction,
including a 1.5% slower sandboxed AOT serial result. This is not a general
thread-pool scheduling improvement.

The sandbox is attached to the submitting Program while the worker closure
belongs to a trusted parent Program. Thus this probes the merged context-
propagation path as well as container work. It does not measure the cost of
introducing that security behavior: both before and after already include it.
Small differences between plain and sandboxed rows do not establish zero sandbox
overhead. Results include submission, queue delivery, and checksum validation;
they are not individual-task latency percentiles or an HTTP benchmark.

### Remaining priorities

1. Profile the remaining typed-container helper paths: repeated coercion,
   metadata lookup, and owned-value bookkeeping still dominate native container
   kernels. A typed fast path must preserve soft conversions, optional values,
   semantic aliases, full-width integer ownership, cancellation, and destructor
   timing. The identity shortcut is only a first step. A separate post-change
   five-second AOT list-build sample still has approximately 32% of main-thread
   samples in the local-assignment dispatch branch and 22% in coercion. These
   are inclusive sampled branches, not exact per-instruction or allocation counts.
2. Extend aggregate elimination only after boundedness and ownership are proved.
   Temporary pairs with a runtime index and temporary hashes remain expensive.
   The retained full-width/alias/exception fixtures are guards for this work;
   Java's result alone is not proof that Qore can erase an allocation safely.
3. Profile IR call-frame reset, owned-slot cleanup, dispatch, and value handling.
   A thread-local frame pool already exists; adding another pool is not the
   proposal. The roughly 360/420 ns direct/method-call kernels still warrant
   separate work even though their AOT equivalents inline well.
4. Repeat on Linux and a representative application under sustained concurrency.
   The task benchmark provides a reproducible sandbox-context probe, not
   request-latency percentiles or a production throughput prediction.

No new scalar-replacement or IR frame optimization is included in this change.

### Evidence and reproduction

[results-containers-macos-arm64.json](results-containers-macos-arm64.json)
contains all seven experiments: the refreshed Java baseline, each isolated
change, combined changes, larger batches, the focused IR repeat, and thread-pool
delivery. There are 600 unique jobs and 3000 positive timing samples. Process
means, medians, ranges, fork counts, and sample counts were independently
validated. Metadata preserves source/library/executable hashes and settings,
but excludes local paths and environment values. Full local output, commands,
and profiles remain in ignored build directories.

After saving the unchanged compatible library as described in the README:

```sh
python3 bench/runtime-comparison/run.py --modes qore-ir qore-aot \
    --baseline-libdir /path/to/saved-library \
    --forks 5 --warmup 3 --samples 5 --time-ms 300
python3 bench/runtime-comparison/run.py --modes qore-ir qore-aot \
    --kernels arithmetic direct_call list_build list_mutate temporary_list temporary_hash \
    --baseline-libdir /path/to/saved-library --count 4096 \
    --forks 3 --warmup 3 --samples 5 --time-ms 300
python3 bench/runtime-comparison/thread_pool.py \
    --baseline-libdir /path/to/saved-library \
    --forks 3 --warmup 3 --samples 5 --time-ms 300
```

[FOLLOWUP-AUDIT.md](FOLLOWUP-AUDIT.md#container-follow-up-audit-2026-10-06)
records every `audit-changes` item, release/debug coverage, and the existing
macOS linker-warning-sensitive test failure reproduced on the baseline.

## Retained calls and program boundaries (2026-10-06)

Baseline revision: `b7de8f1f92274e4d5ce9e5ab76983c4b318d772f`, release
library SHA-256 `ef6505c2e610ab54c7cd2f6da3c0153414bd2473e3abc4b460adae921ac8832a`.
The benchmark now includes typed callable references, three runtime receiver
types, and a selection/recurrence control. Java's retained-call targets use
JMH `CompilerControl(DONT_INLINE)`; the generated `META-INF/CompilerHints`
contains all four directives. This is deliberately distinct from the existing
direct/final-method tests, which allow inlining. It is not a comparison with
the best default, freely inlined HotSpot result.
An additional diagnostic JVM run with `PrintInlining` confirms that inlining
`boundaryStep` is disallowed by the compile command.

Same-program baseline, ns/element (three fork means, median; 1024 elements,
three warmup and five measured windows of 300 ms):

|!Kernel|!Qore IR|!Qore AOT|!Java interpreted|!Java warmed JIT
|arithmetic|93.82|4.10|6.86|0.90
|direct_call|366.66|4.11|16.38|0.88
|method_call|414.29|4.32|16.55|0.88
|indirect_call|308.46|212.23|27.23|1.65
|polymorphic_call|429.32|293.65|22.32|2.54
|dispatch_control|275.12|5.68|10.58|0.92

The large retained-call gap exists **without** a program crossing. These are
whole workloads, including Qore ownership, argument/return validation, and
loop machinery, not ABI-only call latencies. Do not subtract the control to
claim an exact dispatch cost. Qore function references and Java functional
interfaces do not have identical semantics.

Final AOT disassembly confirms calls to `qore_rt_call_closure_fast` inside the
indirect loop and `qore_rt_dot_eval_object_method_direct_aot` inside the
polymorphic loop. It also exposes per-iteration coercion, boxing/unboxing,
reference cleanup, and (for the polymorphic receiver) local-stack helpers.
The function-reference branch of `qore_rt_call_closure_fast_impl` in
`lib/JITRuntime.cpp` builds a `QoreListNode` and calls `execValue`; the special
closure fast path applies to `NT_RUNTIME_CLOSURE`, not `NT_FUNCREF`. The helper's
name therefore does not mean this function-reference case has a fast ABI.
An independent five-second AOT sample confirms the generic route through
`QoreFunction::evalFunction(..., RuntimeConfig&, ...)`, `UserVariantExecHelper`,
and `evalTiered`, with TLS parameter instantiation and `qore_rt_load_local_aot`
before the tiny native callee's arithmetic. This is direct evidence that an
explicit context argument already exists on part of the route but does not yet
remove the surrounding generic-call and TLS machinery.

### Existing explicit context and safe optimization boundaries

- `RuntimeConfig&` already passes context through AST evaluation and
  `CodeEvaluationHelper`. It has not removed all TLS consumers. In particular,
  `rc_get_current_ref()` refreshes object/class fields from `ThreadData`, while
  `ObjectSubstitutionHelper` saves/restores both stores. Passing a pointer alone
  cannot safely eliminate those writes while legacy callbacks still use TLS.
- `IRDirectParams` already bypasses the TLS parameter stack for eligible IR
  callees. The interpreter's cached direct-call shortcut requires the caller
  and callee to share a program and retains general fallbacks for ineligible
  signatures, specialization, AST-visible locals, and native callees.
- `ProgramThreadCountContextHelper::set()` already returns immediately when
  the current program matches and thread-local program data exists. Same-program
  calls are not paying the full cross-program switch on every invocation.
- Cross-program calls still need program lifetime/thread accounting, the right
  local stacks and parse options, sandbox/cancellation context, and correct
  restoration on exceptions. Same-program method calls still need the callee's
  lexical class and receiver, not merely the caller's current context.
- An eventual compact explicit-context native entry should have a guarded
  same-program path plus a full transition path. TLS synchronization can be
  elided only where all consumers are proven compatible; AST/builtin callbacks,
  reentry, debugging/stack inspection, and deoptimization need a defined bridge.
  Copying the owning public `RuntimeConfig` is not an allocation-free solution:
  its copy constructor allocates its implementation.

### Narrow runtime change

The IR direct-call sample showed repeated ownership-set hash-node allocation
and freeing inside `IRCallFrame::reset`, plus per-invocation vector storage.
The retained change uses a pooled dense byte array for local-owned value IDs
and moves the scope-handler watermark stack into the existing per-depth,
thread-local frame pool. It changes no program transition, object/class
context, argument binding, cancellation poll, or public/native ABI.

Every ownership mark/erase/lookup keeps its original logical meaning. The array
is reset to the current function's value-slot count; invalid initialization
sentinels are bounds-checked. Scope-exit guards still run before the frame is
returned to the pool, and nested calls use distinct frames. Other node-based
sets remain, so this is not a claim of allocation-free IR calls.

The next native-call target is the generic function-reference route: resolve
and guard the target/signature/program, pass native arguments to its fast entry
without constructing an argument list, and retain the general path for overloads,
coercions, references/defaults, deleted receivers, and cross-program transitions.
That is likely more valuable for this AOT workload than removing a program
switch that is already skipped. The benchmark makes that hypothesis testable;
the new native entry itself is not implemented in this change.

### Before/after measurements

The saved baseline and rebuilt libraries are ABI-compatible; AOT executables
are compiled separately against the corresponding libraries. The interleaved
1024-element run covers all eleven kernels, three forks per mode, three warmup
and five measured windows of 300 ms. All 660 reference checks passed.

|!Kernel|!IR before|!IR after|!IR time reduction|!AOT before|!AOT after
|arithmetic|94.26|94.51|-0.3%|4.14|4.13
|direct_call|369.18|356.34|3.5%|4.14|4.14
|method_call|419.45|407.35|2.9%|4.38|4.37
|indirect_call|318.14|309.24|2.8%|215.56|218.52
|polymorphic_call|437.43|428.97|1.9%|296.45|293.53
|dispatch_control|277.96|274.76|1.2%|5.58|5.89
|list_read|206.54|196.15|5.0%|5.68|5.67
|list_build|283.64|273.69|3.5%|82.34|82.51
|list_mutate|168.72|169.36|-0.4%|158.73|158.97
|temporary_list|362.05|352.94|2.5%|122.99|122.64
|temporary_hash|668.92|655.23|2.0%|327.93|316.60

The IR-only change does not consistently improve AOT. Its indirect-call median
was 1.4% slower, and the selection-control median was 5.5% slower, with overlapping
fork ranges (after 5.20-5.90 ns, before 5.38-5.73 ns). Conversely, the AOT hash
median improved without a corresponding native-helper change. These mixed
movements are not evidence of an AOT optimization; native code layout and
workstation noise remain uncontrolled.

A 4096-element repeat used the same fork/window counts and a different shuffle
seed. All 660 reference checks passed:

|!Kernel|!IR before|!IR after|!IR time reduction|!AOT before|!AOT after
|arithmetic|94.56|92.79|1.9%|3.48|3.48
|direct_call|357.40|356.56|0.2%|3.49|3.48
|method_call|424.72|395.27|6.9%|3.53|3.52
|indirect_call|309.31|304.40|1.6%|215.84|215.08
|polymorphic_call|430.21|426.94|0.8%|290.17|297.70
|dispatch_control|273.82|265.74|3.0%|5.59|5.43

Method and indirect-call improvements recur, but the direct-call difference
shrinks to noise at the larger batch size (before fork range 353.64-365.44 ns,
after 356.22-358.62 ns). AOT's earlier indirect/control slowdowns do not recur;
its polymorphic median instead slows by 2.6%. Retain the narrower conclusion:
less IR allocation/bookkeeping with modest workload-dependent gains, not a
stable universal call speedup and not an AOT-call improvement.

### Same-program versus cross-program IR

The companion probe explicitly pins both child programs to `EM_IR` before
parsing and checks their modes afterward. A typed callback created by the loop's
program is compared with one created by the other program from identical source.
Program creation, parsing, callback lookup, and the outer `Program::callFunction`
entry are outside timing. Five forks per library/scenario, 1024 calls per batch,
three warmup and five measured 300-ms windows; all 60 reference checks passed.

|!Scenario|!Before ns/call|!After ns/call|!Time reduction|!After fork range
|Same program|306.10|299.85|2.0%|296.59-302.36
|Cross program|340.60|334.43|1.8%|332.38-338.27

The cross-program premium is about 34.5 ns before and after (11.3% / 11.5%).
This supports the hypothesis that crossings cost extra, but most of the cost
in this workload is already present in a same-program call. The frame change
does not optimize program switching. This probe does not measure sandbox policy,
foreign-language calls, module loading, or cross-program AOT-to-AOT execution.

### Evidence and reproduction

[results-calls-macos-arm64.json](results-calls-macos-arm64.json) contains the
sanitized metadata, fork means, and all 1480 timing samples from 296 jobs across
the Java baseline, two interleaved Qore comparisons, and program-boundary probe.
Summary medians and expected fork/sample counts were independently recomputed.
No local paths or environment values are included. The after-library SHA-256 is
`f73c47949e65a9f17de09585b1360475fbeb26abe7620cadbf67f3339555fcb3`.

```sh
python3 bench/runtime-comparison/run.py \
    --kernels arithmetic direct_call method_call indirect_call polymorphic_call dispatch_control \
    --forks 3 --time-ms 300 --startup-samples 3
python3 bench/runtime-comparison/run.py --modes qore-ir qore-aot \
    --baseline-libdir /path/to/saved-library \
    --forks 3 --time-ms 300 --startup-samples 3
python3 bench/runtime-comparison/run.py --modes qore-ir qore-aot \
    --kernels arithmetic direct_call method_call indirect_call polymorphic_call dispatch_control \
    --baseline-libdir /path/to/saved-library --count 4096 \
    --forks 3 --time-ms 300 --startup-samples 3 --order-seed 20261007
python3 bench/runtime-comparison/program_calls.py \
    --baseline-libdir /path/to/saved-library --forks 5 --time-ms 300
```

Builds, runtime regression suites, and sampling/JVM diagnostics were separate
from timing. Lightweight evidence inspection and ordinary workstation activity
were not eliminated. See the [call audit](FOLLOWUP-AUDIT.md#call-frame-and-call-boundary-audit-2026-10-06)
for checks, skips, and the baseline-reproduced linker-warning wrapper failure.

## Guarded function-reference calling convention (2026-10-06)

Baseline: `54e3d929bb0de97e06706c41f0e48020d80e6009`, including the previous
IR call-frame storage improvement. The saved release library/compiler was
preserved before changing runtime dispatch or LLVM lowering. Kernel sources,
workstation, LLVM version, and optimization level are unchanged.

Same-program opaque function references can now bypass argument-list creation
and generic binding when their unique user variant and exact argument types
are eligible. IR reuses the existing direct-parameter path where possible.
AOT additionally has an internal adapter ABI:

```cpp
uint64_t entry(const uint64_t* borrowed_args,
               QoreAOTContext* callee_context, ExceptionSink* xsink);
```

The owned boxed result and borrowed boxed arguments bridge the runtime to an
existing typed native entry, without TLS parameter installation. Creation and
invocation validate the normal native entry's identity, not merely its name.
No existing artifact descriptor or public ABI changes. Old AOT binaries remain
usable; newly compiled adapters require the updated runtime.

Native eligibility is intentionally restricted to compiler-proven non-throwing
scalar expressions. Testing found that context-independent bodies can still
need an explicit context for eager source locations, and that the lazy PC
registry does not yet cover adapter frames. Potentially throwing bodies,
boxed returns, defaults/coercion/overloads, context-dependent code, method
references and cross-program calls therefore retain the appropriate existing
runtime path. This is not a general removal of Qore's runtime context.

### Guard ordering and controls

The first complete, safety-correct version checked RTTI before program
identity. Five forks, three warmups, five 300 ms samples, 1024 iterations gave:

|!Kernel|!IR before ns|!IR after ns|!AOT before ns|!AOT after ns
|arithmetic|93.70|95.69|4.14|4.13
|direct_call|354.23|357.11|4.15|4.13
|method_call|406.09|411.14|4.36|4.37
|indirect_call|305.82|238.63|217.35|88.73
|polymorphic_call|423.80|425.31|298.19|296.55
|dispatch_control|274.08|275.06|5.68|5.87
|list_read|194.73|200.29|5.66|5.67
|temporary_hash|661.75|664.45|324.57|301.39

The 4096-iteration, three-fork repeat measured indirect calls at
308.68 -> 239.07 ns (IR, 22.6% less time) and 214.03 -> 88.92 ns
(AOT, 58.5% less time). Unrelated controls moved in both directions; these
results do not establish a general interpreter/compiler speedup. A separate
five-fork, 500 ms control repeat measured IR arithmetic at 92.50 -> 93.48 ns
and list reads at 193.30 -> 196.55 ns: small slowdowns remained on this host.

The IR Program probe confirmed the intended same-program benefit but exposed
a negative-path cost. At 1024 iterations, same-program calls went
302.79 -> 234.98 ns, while cross-program calls went 333.59 -> 344.91 ns.
At 4096 iterations with 500 ms windows, the corresponding values were
303.54 -> 233.41 ns and 334.18 -> 341.93 ns. The cross-program slowdown
(2.3-3.4%) motivated moving the cheap program-identity check ahead of RTTI.
These intermediate results are retained, not presented as final measurements.

Moving the program guard first, before the final stack-error/restoration
correction, gave 305.29 -> 238.69 ns for IR indirect calls and
214.57 -> 88.34 ns for AOT. The 4096-iteration repeat gave
304.61 -> 234.38 ns and 212.07 -> 87.90 ns, respectively. The Program
probe's cross-program cost narrowed to 339.62 -> 343.68 ns (+1.2%), while
same-program calls went 300.22 -> 235.45 ns. Compiling with native adapters
disabled gave 213.13 -> 152.20 ns for AOT indirect calls, isolating the
benefit of bypassing generic binding. IR arithmetic and list controls were
94.86 -> 96.02 ns and 194.97 -> 199.21 ns. These are also intermediate
results, included in the evidence rather than silently discarded.

### Final implementation

After the final stack-error check and non-fallible context restoration,
the five-fork, 1024-iteration run measured:

|!Kernel|!IR before ns|!IR after ns|!IR time reduction|!AOT before ns|!AOT after ns|!AOT time reduction
|indirect_call|311.10|240.34|22.7%|215.67|87.62|59.4%
|arithmetic|93.21|95.42|-2.4%|4.13|4.14|-0.3%
|list_read|198.49|200.05|-0.8%|5.68|5.68|approximately 0%

These are medians of process-fork means, in nanoseconds per element; for
`indirect_call`, one element includes one opaque callback plus the loop and
checksum work. Startup is excluded. Negative reductions are slowdowns, not
benefits. The arithmetic/list controls do not enter the new reference path,
but their observed slowdowns must not be hidden or generalized away as noise.

The three-fork, 4096-iteration repeat measured 306.70 -> 236.09 ns for IR
(23.0% less time) and 215.54 -> 86.25 ns for AOT (60.0% less time).
Thus the same-program callback benefit survives a larger batch size and
separate process forks; this is a workload-specific result, not a speedup
claim for arbitrary Qore applications.

The final Program probe measured same-program calls at 306.11 -> 235.36 ns
(23.1% less time), but cross-program calls at 337.03 -> 343.50 ns
(1.9% more time). Guard reordering reduced the earlier negative-path cost,
but did not eliminate it. Cross-program execution is not optimized by this
change and remains a follow-up target.

With `QORE_DISABLE_AOT_FUNCREF_NATIVE_ENTRY=1` during compilation, the final
three-fork AOT run measured 215.35 -> 152.32 ns (29.3% less time). The
ordinary argument-binding bypass therefore explains only part of the full
AOT benefit: the native adapter reduces the remaining cost from roughly
152 ns to 88 ns in separate runs. Those figures are not an additive
decomposition or a paired experiment between the two changed binaries.

[Sanitized measurement evidence](results-funcref-macos-arm64.json) contains
13 experiments, 464 process/kernel jobs and 2320 measured samples. The final
implementation accounts for 98 jobs / 490 samples; the rest explicitly
record superseded guard/error-handling revisions. Each experiment includes
fork-level samples, summary statistics, source and binary hashes, and settings.
Every kernel mode passed 165 independent reference checks before timing;
each Program-probe mode passed 30 checks. The native adapter's symbol and
binding-helper reference were also verified in the final executable.

Reproduce the final runs after saving the baseline `qore`, `qcc`, and compatible
`libqore` files, then building the changed release runtime/compiler:

```sh
python3 bench/runtime-comparison/run.py --modes qore-ir qore-aot \
    --kernels indirect_call arithmetic list_read \
    --baseline-libdir /path/to/saved-baseline --forks 5 --time-ms 300
python3 bench/runtime-comparison/run.py --modes qore-ir qore-aot \
    --kernels indirect_call --count 4096 \
    --baseline-libdir /path/to/saved-baseline --forks 3 --time-ms 300
python3 bench/runtime-comparison/program_calls.py \
    --baseline-libdir /path/to/saved-baseline --forks 5 --time-ms 300
QORE_DISABLE_AOT_FUNCREF_NATIVE_ENTRY=1 python3 bench/runtime-comparison/run.py \
    --modes qore-aot --kernels indirect_call \
    --baseline-libdir /path/to/saved-baseline --forks 3 --time-ms 300
```

All runs used three warmups and five measured windows per fork. These are
single-workstation microbenchmarks, with no CPU/frequency isolation. Builds
and regression tests did not overlap timing; ordinary workstation activity
and lightweight inspection were not eliminated. Java was not remeasured in
this step, so historical Java results are not a fresh matched comparison.
See the [full audit and test record](FOLLOWUP-AUDIT.md#function-reference-native-entry-audit-2026-10-06).

Next priorities are reducing per-call eligibility/type-check overhead and the
foreign-reference rejection cost, registering adapter PC maps before admitting
throwing bodies, and then extending explicit call context to class/object-aware
entries with legacy TLS bridges. The small IR-control regressions also need
broader platform and code-layout investigation before claiming a general win.

A separate five-second, 1 ms macOS sampling run confirmed the hot callback
stack enters `tryExecFunctionRef`, `execFunctionRefNative`, and the generated
scalar adapter. It still spends work in RTTI, exact-binding/static-eligibility
checks, diagnostic frames, parse-option/location setup, and scalar conversion.
Generic binding and TLS parameter setup remain elsewhere in the harness, not
in that sampled native callback stack. This is qualitative evidence for the
next targets, not an allocation count or an additional timing experiment.

## Cached metadata and typed scalar callback ABI (2026-10-06)

Follow-up to 766a5ce07, measured against its saved Release runtime/compiler
library. The host remains Apple M4 Pro / arm64 / macOS 27.0.1, LLVM 22.1.8,
-O3 -DNDEBUG. This phase caches finalized signature eligibility at reference
creation, then adds native scalar dispatch for zero/one-argument AOT callbacks.
No Java-equivalent semantic or universal speedup claim is intended.

Median of independent process means, ns/element (callback plus loop/checksum
work); five forks, three warmups and five 300 ms measurement windows, count
1024. Java is freshly measured with Temurin 25.0.2 and JMH 1.37. Its indirect
and polymorphic targets retain DONT_INLINE controls.

|!Kernel|!IR before|!IR after|!AOT before|!AOT after|!Java JIT
|arithmetic|95.58|95.91|4.15|4.14|0.89
|direct_call|361.45|357.81|4.15|4.13|0.89
|method_call|414.48|414.86|4.39|4.36|0.89
|indirect_call|238.07|229.03|86.90|70.37|1.67
|polymorphic_call|431.98|421.97|297.68|296.82|2.54
|dispatch_control|279.87|270.11|5.71|5.65|0.96

The callback reductions are 3.8% for IR and
19.0% for AOT. A three-fork count-4096 repeat gave
233.08 -> 224.86 ns for IR and
87.24 -> 69.29 ns for AOT.
Controls do not establish improvements outside the optimized path: IR
arithmetic and method calls were 0.3% and 0.1% slower, while the other
unrelated controls varied by up to 3.5% in the faster direction.

Separate five-fork Program-boundary measurements gave
235.29 -> 223.96 ns/call within one Program
and 338.53 -> 342.16 across Programs. The latter is
1.1% slower; this optimization does not accelerate Program crossings.

The separately saved metadata-only intermediate improved the indirect kernel
from 240.59 -> 227.56 ns in IR and 86.18 -> 77.79 ns in AOT (five forks).
An initial typed-ABI pilot compared to that intermediate gave 80.93 -> 73.99 ns
(three forks); it precedes final exact-type and experimental-EH audit fixes and
is not the final combined result. Repeating the typed-only comparison with the
final corrected implementation gave 78.32 -> 70.16 ns (five forks), a further
10.4% reduction versus the saved metadata-only intermediate.

AOT callbacks are still 42.0 times the matched Java result; polymorphic AOT
calls are 117.1 times Java. The direct/final-method kernels allow inlining and
must not be read as isolated call overhead. The typed boundary retains RTTI,
diagnostic frames, Program/context setup and caller-result boxing. It is not
yet a fully unboxed loop. Reference construction cost and allocations were
not quantified; caching adds reference storage and a bounded creation scan.
The benchmark AOT artifact grew from 129408 to 129752 bytes (0.27%).

All implementations passed 165 independent checks per kernel-suite mode;
Program probes passed 30 checks per mode. Builds and tests were separate from
timing. Ordinary workstation activity and lightweight inspection were not
eliminated. No full-suite, sanitizer or production-latency claim is made.

Reproduce with the saved baseline library selected through --baseline-libdir,
using run.py --modes qore-ir qore-aot java-jit --kernels arithmetic direct_call
method_call indirect_call polymorphic_call dispatch_control --forks 5
--time-ms 300. Repeat indirect_call with --count 4096 --forks 3; run
program_calls.py --forks 5 --time-ms 300 for Program boundaries. Every AOT
comparison compiles separately against the selected runtime. The compile-time
QORE_DISABLE_AOT_TYPED_FUNCREF_CALL flag retains the boxed adapter for isolation.

[Raw samples and sanitized metadata](results-native-calls-macos-arm64.json)
retain separate intermediate/final phases, source and binary hashes, and
process-level dispersion. The interrupted pre-EH-correction broad run is
excluded. [Full audit](FOLLOWUP-AUDIT.md#cached-metadata-and-typed-scalar-reference-abi-audit-2026-10-06)
records all checklist items and regression coverage.

A separate five-second profile confirmed the typed helper and still sampled
RTTI, diagnostic stack-location construction, runtime location/parse-option
updates, and context/PC-map readiness. These are candidates for the next
measured optimization, not costs removed by this commit.

## Post-merge exact-reference and diagnostic-name optimization (2026-10-06)

The metadata/scalar-ABI work was audited and committed as d3e546088, then
origin was fetched and origin/develop (e88224334) merged cleanly in 2fd14f43d.
The merged release/debug runtimes were rebuilt and integration-tested;
QoreAgentStrategies-qmod was rebuilt before its changed module tests.
The merged release library was preserved as the baseline for this follow-up.

The executed plan was to remove two costs observed in the prior profile:
general RTTI inheritance lookup for the exact internal function-reference node,
and per-call copies of the immutable diagnostic name. The runtime now uses
an exact type check before a static downcast, with other/derived nodes falling
back. Native reference frames borrow the stable name; ordinary frames still
own synthesized names. All diagnostic frames, Program guards, policy checks,
location updates, cancellation behavior and exception boundaries remain.

Same host/build/methodology as above. Five forks per mode/kernel, count 1024,
three warmups and five 300 ms measurement windows. Java was remeasured in the
same shuffled schedule. Values are median fork means in ns/element.

|!Kernel|!IR before|!IR after|!AOT before|!AOT after|!Java JIT
|indirect_call|227.29|224.89|69.63|61.69|1.66
|arithmetic|94.62|94.75|4.10|4.09|0.89
|polymorphic_call|420.07|423.92|294.63|291.38|2.53

The AOT callback reduction is 11.4%.
The count-4096, three-fork repeat gave
68.92 -> 61.51 ns
(10.8% less time).
The IR callback changes are only 1.1% and
0.8%, with overlapping fork ranges.
They do not establish a strong independent IR benefit for this second round.
Arithmetic was essentially unchanged; polymorphic controls moved 0.9% slower
in IR and 1.1% faster in AOT, not evidence of optimized method dispatch.

The five-fork Program probe gave
226.90 -> 221.72 ns/call in one Program and
338.03 -> 340.70 across Programs.
The cross-program median is 0.8% slower, with overlapping ranges. All raw
forks are retained, including the slow same-program baseline fork; no outlier
was discarded.

A separate interleaved three-fork comparison measured the final runtime
directly against the original 766a5ce07 baseline, including both optimization
rounds and the develop merge:

|!Mode|!Original baseline ns/element|!Final ns/element|!Time reduction
|IR|238.80|227.53|4.7%
|AOT|86.38|62.91|27.2%

These are directly measured overall changes, not multiplied percentages from
different runs. AOT remains about 37.1 times Java in the matched callback
comparison, and polymorphic AOT dispatch about 115.0 times Java. Future larger
gains need guarded native method dispatch, less per-call context work and
compiler-visible unboxed/inlinable callback paths, with explicit ownership,
debugger and diagnostic semantics. Those broader changes are not implemented
by this follow-up.

A separate five-second profile retained the typed helper and context/location
work; the general dynamic cast and owning-name constructor were no longer
sampled in that callback stack. Source review confirms those operations were
removed. This is qualitative evidence, not an allocation count. Long names
are covered by a four-thread regression but were not separately timed.

Reproduce with run.py --modes qore-ir qore-aot java-jit --kernels indirect_call
arithmetic polymorphic_call --forks 5 --time-ms 300 and the merged saved library
as --baseline-libdir. Repeat indirect_call with --count 4096 --forks 3, and
program_calls.py --forks 5 --time-ms 300. The overall comparison selects the
original saved library with --modes qore-ir qore-aot --kernels indirect_call
--forks 3 --time-ms 300.

[Sanitized evidence](results-native-frame-macos-arm64.json) contains four
experiments, 119 jobs / 595 measured samples, source/library hashes and raw
dispersion. All modes passed their independent reference checks. Timing,
builds, regression tests and profiling were separate; background workstation
activity and lightweight inspection were not eliminated.
[Full audit](FOLLOWUP-AUDIT.md#exact-reference-recognition-and-borrowed-diagnostic-names-audit-2026-10-06)
records integration recovery and 338 release cases / 13333 assertions plus five
standalone AOT scripts, 15 debug cases / 11317 assertions and ten harness tests.
No full-suite, sanitizer or production performance claim is made.

## Explicit RuntimeConfig callback context (2026-10-07)

Measured against the saved Release runtime/compiler at 1501146e7. The retained
change adds a v2 typed-callback helper that receives RuntimeConfig explicitly.
Generated functions acquire its live thread-state view once, after their entry
stack guard, and reuse it at callback sites. Frame/context setup then accesses
that state directly. Existing acquire/release readiness flags bypass repeated
slow AOT-context and PC-map helpers once ready; first materialization remains
lazy. Program, debugger, signature, stack, diagnostic and ownership safeguards
remain. Old v1 helpers and previously compiled AOT executables remain supported.

Same Apple M4 Pro / arm64 / macOS 27.0.1 host, LLVM 22.1.8, Release -O3
-DNDEBUG. Five independent forks, count 1024, three warmups and five 300 ms
measurement windows, with before/after and freshly measured Java jobs
interleaved in a shuffled schedule. Values are median fork means in ns/element,
including loop/checksum work, not isolated call latency. Java callback and
polymorphic targets retain DONT_INLINE controls.

|!Kernel|!IR before|!IR after|!AOT before|!AOT after|!Java JIT
|indirect_call|229.11|225.07|61.65|53.98|1.66
|arithmetic|96.22|95.48|4.08|4.08|0.87
|polymorphic_call|425.06|420.44|293.39|292.98|2.53

The AOT callback uses 12.4% less time. Its before fork means span
61.07-62.02 ns and after means 52.53-54.99 ns, without overlap. A three-fork
count-4096 repeat gives 61.06 -> 53.60 ns, a 12.2% reduction. The IR callback
changes (229.11 -> 225.07 and 223.64 -> 223.02 ns) have overlapping fork
ranges; this change does not establish an IR speedup.

Controls were mixed. In the count-4096 repeat, IR polymorphic calls were
416.26 -> 427.83 ns (2.8% slower). A longer, differently shuffled five-fork
500 ms repeat gave 415.53 -> 412.40 ns (0.8% faster), while IR arithmetic
was 93.64 -> 94.89 ns (1.3% slower). The polymorphic slowdown was therefore
not consistently reproduced; no universal no-regression claim is made.
The five-fork IR Program-boundary probe gave 217.82 -> 219.90 ns/call within
one Program and 334.73 -> 339.96 across Programs: 1.0% and 1.6% slower,
respectively, with overlapping fork ranges. Neither probe uses the new AOT
helper; these are controls, not evidence of accelerated Program crossings.

An earlier combined prototype also refactored the generic fast executor to
consume prepared reference metadata. Its five-fork callback results were
225.90 -> 226.59 ns for IR and 62.09 -> 54.87 ns for AOT; IR polymorphic
controls were 417.62 -> 430.12 ns. A count-4096 repeat gave IR callbacks
222.28 -> 216.10 ns, AOT callbacks 60.03 -> 54.04 ns, and IR polymorphic
calls 412.64 -> 427.29 ns. Given the mixed IR benefit and slower controls,
the executor refactor was removed. The retained RuntimeConfig field was also
moved to the end of its private implementation, preserving existing offsets.
The combined experiment cannot attribute the control shift to either edit
individually. Raw results for both rejected runs and the initial baseline
are retained, not discarded as outliers.

AOT callbacks remain 32.6 times the matched Java result. This is not a full
TLS migration or a fully unboxed/inlinable callback loop. A separate five-second,
1 ms sampling run still shows frame/context construction and destruction,
local/closure frame-boundary bookkeeping, and generated callee stack guards.
Sampling is qualitative, not a per-component cost or allocation count.
The next stages are compiler-produced diagnostic/frame metadata, an unboxed
compiler-visible callback path, and guarded native method dispatch; broader
adapter eligibility must preserve exception, debugger and ownership semantics.

Reproduce with run.py --modes qore-ir qore-aot java-jit --kernels indirect_call
arithmetic polymorphic_call --forks 5 --time-ms 300 and the saved 1501146e7
library as --baseline-libdir. Repeat without Java using --count 4096 --forks 3.
The longer IR control run selects polymorphic_call arithmetic, --count 4096
--forks 5 --time-ms 500 --order-seed 20261007. Program controls use program_calls.py
--forks 5 --time-ms 300. Each AOT comparison compiles separately against its
selected runtime. QORE_DISABLE_AOT_FUNCREF_RUNTIME_CONFIG=1 at compilation
selects the v1 typed helper for functional isolation.

[Sanitized raw evidence](results-explicit-context-macos-arm64.json) contains
seven experiments, 289 jobs / 1445 measured samples, source/library hashes
and process-level dispersion. Every suite mode passed 165 independent checks;
Program probes passed 30 per mode. All forks are retained. Compilation,
regression testing and profiling were separate from timing; ordinary host
activity and lightweight inspection were not eliminated.
[Full audit](FOLLOWUP-AUDIT.md#explicit-callback-execution-context-audit-2026-10-07)
records 311 release cases / 13223 assertions plus the standalone AOT location
script, 13 debug cases / 11279 assertions, ten harness unit tests, old-AOT
compatibility and generated-code inspection. No full-suite, sanitizer or
production-performance claim is made.

## Compiler-proven native leaf frames (2026-10-07)

Fetched origin and merged origin/develop at 0122225d0 in 089ab8e44. The two
incoming commits fix DPQL Unicode handling and do not change call-runtime
sources. Rebuilt DataProvider-qmod and verified DPQL plus callback integration
before preserving the merged Release runtime/compiler as the baseline.

The executed optimization uses the existing whole-body scalar-expression proof
to specialize empty local/closure frames around typed native callbacks.
Diagnostic frames, local/closure markers and frame counts remain visible, but
the local stack no longer appends a general variable-cleanup record, and both
markers are popped directly. Ordinary calls retain their general cleanup path.
Program/debugger/type/entry-identity guards, stack checks, reader synchronization,
policy overrides, lazy materialization and helper ABI are unchanged.

Same Apple M4 Pro / arm64 / macOS 27.0.1 host, LLVM 22.1.8, Release -O3
-DNDEBUG. Five forks, count 1024, three warmups and five 300 ms measurement
windows. Before/after and Java jobs are shuffled/interleaved. Values are median
fork means in ns/element, including callback and loop/checksum work. They are
not isolated call latency. Java retained-call targets use DONT_INLINE.

|!Kernel|!IR before|!IR after|!AOT before|!AOT after|!Java JIT
|indirect_call|226.24|228.03|53.87|50.14|1.67
|arithmetic|97.28|96.29|4.12|4.13|0.88
|polymorphic_call|426.60|427.71|296.57|294.61|2.53

AOT callbacks use 6.9% less time, with nonoverlapping fork ranges:
53.47-54.48 ns before and 49.61-50.33 ns after. A three-fork count-4096 repeat
gives 53.61 -> 49.79 ns, a 7.1% reduction. AOT arithmetic is essentially
unchanged; polymorphic AOT controls vary 0.7% faster/slower across the two runs,
not evidence of optimized method dispatch.

IR callbacks were 0.8% slower in the first comparison, and 223.03 -> 227.95 ns
(2.2% slower) in the larger-count repeat. A longer five-fork count-4096 run,
500 ms windows and a different shuffle, gives 224.54 -> 225.26 ns (0.3% slower)
with overlapping ranges; IR arithmetic is 94.88 -> 95.31 ns (0.4% slower).
The larger shift did not persist, but no IR speedup or universal no-regression
claim is made. Every fork is retained.

The five-fork IR Program probe gives 222.49 -> 224.13 ns/call within one Program
(0.7% slower) and 339.19 -> 338.32 across Programs (0.3% faster), both with
overlapping ranges. Neither probe uses the new AOT frame helper. The separate
fresh pre-edit baseline is also retained, but percentages above come from
matched interleaved comparisons, not from that earlier run.

AOT callbacks still take 30.1 times the matched Java result. Separate five-second
sampling no longer shows the general variable-frame push/pop helpers in the
native callback stack; the source confirms their replacement. It still shows
scalar coercion/type-path resolution, typed-reference guards, context setup,
closure-marker insertion and stack guards. This is qualitative evidence, not a
component-cost or allocation count. The next optimization should keep the
typed success path compiler-visible and unboxed, while preserving validation
on generic fallback. Adapter diagnostic metadata and guarded native method
dispatch remain separate subsequent stages.

Reproduce with run.py --modes qore-ir qore-aot java-jit --kernels indirect_call
arithmetic polymorphic_call --forks 5 --time-ms 300 and the saved 089ab8e44
library as --baseline-libdir. Repeat without Java using --count 4096 --forks 3.
The longer IR control selects indirect_call arithmetic, --count 4096 --forks 5
--time-ms 500 --order-seed 20261008. Program controls use program_calls.py
--forks 5 --time-ms 300. Each AOT artifact is compiled against its selected
runtime. Compilation, tests and profiling are separate from timing; ordinary
host activity and lightweight inspection were not eliminated.

[Sanitized evidence](results-native-leaf-macos-arm64.json) contains five
experiments, 178 jobs / 890 measured samples, source/library hashes and all raw
fork values. Suite modes pass 165 independent reference checks each; Program
probes pass 30 per mode. [Full audit](FOLLOWUP-AUDIT.md#native-leaf-frame-audit-2026-10-07)
records 367 release cases / 16011 assertions plus the standalone AOT location
script, 14 debug cases / 11282 assertions, ten harness tests and old-AOT
compatibility. No full-suite, sanitizer or production-performance claim is made.

Test development also found a pre-existing recursive per-frame capture failure
in JIT mode, reproduced on the saved baseline and final runtime while AST/IR
pass. It is not fixed here; the evidence retains the complete reproducer and
the audit explains the distinction from the passing mixed-frame regression.

## Exact native scalar assignment guards (2026-10-07)

Baseline: 1e974dc50, including the native leaf-frame optimization above.
The new LLVM lowering accepts matching immediate values directly when assigning
boxed results to exact native int/float/bool locals. Other values retain the
original coercion, ownership and exception behavior. This optimizes callback
continuations and other dynamic scalar assignments; it does not change the
IR interpreter or the callback ABI.

Five-fork matched results, count 1024, three warmup and five measured 300 ms
windows per fork; ns/element includes loop, call and checksum:

|!Kernel|!IR before|!IR after|!AOT before|!AOT after|!AOT time reduction|!Java JIT
|indirect_call|226.39|225.63|50.25|27.91|44.4%|1.67
|arithmetic|95.24|95.94|4.13|4.12|0.2%|0.89
|polymorphic_call|425.11|411.04|295.56|273.30|7.5%|2.53

The callback fork-mean ranges are 49.66–50.43 ns before and 27.48–28.10 ns
after, with no overlap. Java's callback target still uses DONT_INLINE; Qore's
native callback remains an indirect call. The remaining workload ratio is
16.69x, not Java parity and not an isolated call-latency ratio.

A second matched three-fork run at count 4096 confirms AOT callback
49.79 → 27.08 ns (45.6% less time) and polymorphic calls 290.14 → 272.15 ns
(6.2% less time). Arithmetic is effectively unchanged, 3.450 → 3.456 ns.
IR callbacks are 220.72 → 224.64 ns in this repeat, with overlapping ranges;
IR arithmetic is 94.09 → 94.43 ns and polymorphic calls 425.68 → 414.29 ns.
No interpreter speedup is attributed to this LLVM-only change; the IR controls
also reflect host variation and whole-library layout effects.

A fresh pre-edit baseline run is retained separately and is not used for
the improvement percentages. Before/after jobs are shuffled and sequential;
each AOT artifact is compiled and executed with its matching library. No
outliers were removed. Builds, tests and the five-second sampling run are
separate from timing; ordinary host activity and light inspection remain
uncontrolled. Platform and harness match the preceding experiment.

The new sample no longer shows general coercion as a major cost in the
callback loop. It still shows native frame/context publication, reference
recognition, scalar cleanup calls, stack checks and per-iteration cancellation.
These are the next optimization targets; samples are qualitative, not precise
per-component cost or allocation counts.

[Sanitized evidence](results-scalar-assignment-macos-arm64.json) retains all
138 jobs / 690 samples across three experiments, source/library hashes and
recomputed summaries. [Full audit](FOLLOWUP-AUDIT.md#native-scalar-assignment-audit-2026-10-07)
records release/debug validation, code-generation checks, baseline parity,
old-AOT compatibility and limitations. The audit added explicit generated-guard
checks and kept non-strict destinations and owned heap values on the slow path.

## Native callback scalar cleanup (2026-10-07)

Baseline: `ebe191d30`, including the preceding native assignment guards.
The accepted change inserts immediate int48/bool/NOTHING cleanup guards only
in AOT functions containing typed native callback helpers, after the main
LLVM pipeline. Only local simplification follows. Heap and opaque values keep
their release calls, ordering and exception sinks. IR and ordinary JIT do not
emit these typed helpers and are unchanged by the final pass.

The five-fork, 1024-element comparison used shuffled sequential jobs, three
warmups and five 300 ms samples per fork. Units include loop, computation and
checksum, not isolated call latency. Java retains the callback target through
JMH's DONT_INLINE annotation. Builds, tests and profiling were separate from
timing; ordinary host activity and light inspection were not eliminated.

|!Kernel|!AOT before ns/element|!AOT after ns/element|!Java JIT ns/element
|Retained callback|27.74|21.74|1.67
|Arithmetic control|4.10|4.09|0.89
|Direct call|4.12|4.13|0.89
|Dispatch control|5.48|5.60|0.92
|Polymorphic method|272.25|268.96|2.53

The callback takes **21.7% less time**, with nonoverlapping fork-mean ranges:
27.39-28.08 ns before and 21.67-21.90 ns after. Java remains **13.0x faster**
on this retained-call kernel. Direct-call and dispatch-control ranges overlap;
their small median changes are not gains or established regressions. Direct
calls add little over Qore's own arithmetic control, but Qore's loop throughput
still trails Java. This is not a Java-parity result.

A separate three-fork, 4096-element run repeats the callback benefit and
checks the other kernels:

|!Kernel|!AOT before ns/element|!AOT after ns/element
|Retained callback|26.74|21.04
|Arithmetic|3.44|3.46
|Direct call|3.47|3.46
|Final method|3.51|3.50
|Polymorphic method|269.75|269.45
|Dispatch control|5.38|5.40
|List read|4.94|4.92
|List build|81.26|81.60
|List mutation|159.99|157.00
|Temporary list|119.76|120.08
|Temporary hash|316.80|304.93

The repeated callback reduction is **21.3%**. Unchanged kernels are controls,
not attributed improvements; the final pass does not transform their bodies.
In particular, the earlier direct-call regression is absent in both final runs.

### Rejected variants and code-size tradeoff

Two broader implementations were measured, then rejected:

|!Variant|!Native text bytes|!Finding
|Baseline|49856|Reference implementation
|Before the main LLVM pipeline|160500|Callback 28.19 to 21.70 ns, but 3.2x native text
|After the pipeline, all functions|93180|Callback improvement retained, but wider run regressed direct calls 16.6% and dispatch control 14.3%
|After the pipeline, typed callback callers only|51112|Accepted; callback improvement retained, direct-call controls restored

The final native-text increase is **2.5%**. Whole executable sizes were
129872 and 129864 bytes, effectively unchanged at Mach-O page granularity.
One final-run compilation took 218.01 ms versus 216.52 ms for the baseline;
these are single observations, not a compile-time performance distribution.
All rejected and accepted samples are retained in the evidence file.

### Correctness and remaining work

Code growth in the first trial exposed a pre-existing parallel-codegen source
location bug: Mach-O `ld -r` discarded the DWARF line tables before Qore built
its PC maps. The saved baseline reproduces the forced-split failure (line 30
instead of line 33 in IRFunctionRefCalls). The fix accumulates maps, including
literal inlined-callee locations, before the partial link. The final object
still carries the standard linker-surviving section and trailer. Forced-split
tests now pass with and without the cleanup optimization.

A separate five-second sampling run finds little scalar cleanup traffic in
the callback loop. Callable validation, frame setup/restoration, closure frame
markers, stack checks and cancellation remain. Next: use explicit live runtime
context for cancellation polling without weakening its frequency or scope,
then evaluate native result propagation and frame costs. Sampling is qualitative.

Two baseline correctness findings remain distinct from this change: the
previously recorded recursive JIT capture issue, and nested direct-call AOT
exceptions reporting caller lines 7/10 instead of throwing line 4. The latter
reproduces on baseline and final O0/O3, with single-part, source/debug-stripped
compilation; its complete reproducer is retained in the evidence. Neither is
counted as a passing regression or used to broaden native-call eligibility.

[Sanitized evidence](results-callback-cleanup-macos-arm64.json) contains five
experiments, all 357 jobs / 1785 samples, source/library hashes, rejected
variants and independently recomputed medians.
[Full audit](FOLLOWUP-AUDIT.md#native-callback-cleanup-audit-2026-10-07) records
330 release cases / 12616 assertions and 34 debug cases / 11484 assertions,
plus standalone scripts, old-AOT compatibility, baseline fixtures and the
10 harness tests. No full-suite, sanitizer, Linux validation or Qore allocation
count claim is made. The audit added compiler cancellation and explicit
code-generation coverage; measurements narrowed the optimization's scope.
