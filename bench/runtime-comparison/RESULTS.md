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

## Live context for cancellation polling (2026-10-07)

Baseline: `51b9d37c5`. Generated JIT/AOT loops acquire a nullable borrowed
`RuntimeConfig*` once per invocation and pass the live view to the versioned
polling helper. The legacy and explicit paths share cancellation logic.
Exception construction, manager lookup and optional TLS fallback are outlined.
Deferral, thread-cancellation priority, Program scope, atomic ordering and
polling frequency are unchanged. This does not remove cancellation checks.

Five forks, 1024 elements, three warmups and five 300 ms samples per fork:

|!Kernel|!AOT before ns/element|!AOT after ns/element|!Java JIT ns/element
|Arithmetic|4.080|2.604|0.884
|Direct call|4.078|2.614|0.875
|Retained callback|21.463|20.012|1.664

Direct-call loop time falls **35.9%**, with nonoverlapping fork-mean ranges
4.064-4.151 versus 2.604-2.620 ns. Callbacks improve **6.8%**, with ranges
21.313-21.476 versus 19.891-20.334 ns. Java remains about **3.0x faster** for
the direct-call loop and **12.0x faster** for the retained callback. Direct
calls add almost nothing over Qore's arithmetic control: this gain primarily
reduces loop-poll overhead, not an isolated direct-call latency.

The initial explicit-context-only trial improved direct loops 17.4%; outlining
uncommon work produced the final result above. Both trial datasets are retained.
Interpreter ranges overlap; their small changes are not established gains.

A three-fork repeat at 4096 elements checks the wider execution workload:

|!Kernel|!AOT before ns/element|!AOT after ns/element
|Retained callback|20.95|19.47
|Arithmetic|3.42|1.95
|Direct call|3.46|1.96
|Final method|3.48|2.01
|Dispatch control|5.35|2.22
|List read|4.89|3.43
|Polymorphic method|268.81|263.11
|List build|82.57|78.30
|List mutation|157.26|155.18
|Temporary list|120.03|121.27
|Temporary hash|306.31|327.44

The allocation-heavy hash result initially regressed **6.9%**. An additional
five-fork repeat did **not** reproduce it: 302.59 before versus 301.30 after,
with overlapping ranges 294.21-312.97 and 289.35-307.11. Temporary-list ranges
also overlap (118.62 versus 119.43 median); callbacks again improve **7.6%**
(20.91 to 19.32). Both runs are retained, with no samples discarded. Allocation
results remain variable; no allocation-kernel improvement or universal
regression-free claim is made.

Native text grows from 51112 to 51648 bytes (**1.0%**); executable size grows
192 bytes. Single compilation observations were 235.53 ms before and 234.81 ms
after, not a compile-time distribution. Separate five-second sampling still
finds native frame setup/restoration, closure markers, validation and stack
checks prominent. The next bounded experiment is the empty closure-marker push.

Validation: **356 release cases / 12792 assertions**, five standalone scripts;
**124 debug cases / 11830 assertions**, four standalone scripts. The new fixture
covers AST/IR/JIT/tiered, AOT O0/O3, disabled context passing and native EH.
Native tests compare live, legacy and null-context paths through nested
Programs, deferral, cancellation priority, racing requests and foreign-thread
registration/teardown. Old AOT's 165 records match; 10 harness tests pass.

One existing wrapper rejects a default-target macOS linker warning. The saved
baseline compiler reproduces that warning; baseline and final release/debug
AOT fixtures execute successfully when invoked explicitly. The warning-rejecting
wrapper is not reported as passing. Some older debug wrappers do not set
DYLD_LIBRARY_PATH for children; explicit library selection is verified in the
new wrappers/manual checks. No full-suite, sanitizer or Linux claim is made.

[Sanitized evidence](results-loop-runtime-context-macos-arm64.json) retains all
four experiments, **246 jobs / 1230 samples**, hashes and recomputed medians.
[Full audit](FOLLOWUP-AUDIT.md#loop-runtime-context-audit-2026-10-07) records
every audit-changes checklist item and the validation limitations. Audit review
kept the borrowed context nullable at teardown and added direct semantic
coverage; no pending-state snapshot or weaker ordering was introduced.

## Inline native closure markers and combined result (2026-10-07)

The final small specialization uses the original empty closure-frame marker,
count and publication order, but inserts the marker inline when its stack block
has space. Only full-block growth/reuse calls the general insertion path.
Admission is still limited to compiler-proven TLS-independent scalar leaves;
ordinary calls and captured variables are unchanged. No calling-convention or
serialized metadata change is required.

Against `002ba39cb`, retained callbacks improve **2.6%** at 1024 elements
(19.968 to 19.451 ns/element, three forks) and **4.5%** in a five-fork repeat
at 4096 elements (19.482 to 18.597). Both runs have nonoverlapping before/after
fork-mean ranges. Arithmetic, direct-call, dispatch, polymorphic and allocation
controls have overlapping ranges; their small differences are not attributed
to this optimization. AOT native text is unchanged at 51648 bytes.

### Combined comparison with Java

A fresh, shuffled five-fork comparison measures the four changes together
against `1e974dc50`: exact scalar assignment guards, targeted scalar cleanup,
live-context cancellation polling/outlined cold paths, and native marker push.
These are measured combined results, not multiplied gains from separate runs.
Each fork uses three warmups and five 300 ms samples, with 1024 elements.

|!Kernel|!AOT before ns/element|!AOT after ns/element|!Java JIT ns/element|!Qore time reduction
|Arithmetic|4.077|2.604|0.875|36.1%
|Direct call|4.089|2.592|0.878|36.6%
|Retained callback|49.798|19.538|1.657|60.8%

The callback is **2.55x faster than the earlier Qore baseline**, but still
takes **11.8x Java's time**. Its fork ranges are 49.448-50.674 before and
19.310-19.631 after. Direct calls add effectively no cost over Qore's arithmetic
control; their loop remains about **3.0x Java's time**. These include the loop,
computation and checksum, not an isolated call-latency measurement.

### Remaining boundary

Final sampling no longer finds the general closure insertion routine on the
callback hot path. It still finds native frame setup/restoration, typed callable
validation, stack checks and cancellation. These results do not establish that
Java parity is impossible, but they do not demonstrate it either. Replacing
individual TLS lookups is no longer sufficient to close the retained-call gap.

The next substantial work is compiler-visible callback specialization/unboxed
continuations and a lower-cost representation of observable native frames.
Any such change must retain foreign-Program fallback, live debugger behavior,
stack inspection, source locations, ownership and cancellation. Immutable
binding/signature facts can be prepared earlier; current Program identity and
other changing state cannot simply be assumed from an opaque typed callback.
Polymorphic dispatch remains a separate, much slower path.

### Validation and evidence

The frame fixture now covers depths 0/1, 31/32, 63/64/65, 127/128/129 and 140,
including block reuse, captured-value reads, owned-object destruction, restored
stack depth and exception unwinding. It passes on the saved baseline in
AST/IR/JIT/tiered and stripped-source AOT O0/O3, and through the release/debug
wrapper's disabled-path, cross-Program, concurrent-stack and split-codegen cases.

Final selected verification passes **76 cases / 11631 assertions per build**
(release and debug), plus one standalone script each; all 165 old-AOT reference
records match and the 10 benchmark-harness tests pass. No full-suite, sanitizer
or Linux validation claim is made. Previously recorded correctness/toolchain
limitations are unchanged and remain documented in the preceding audits.

[Sanitized evidence](results-native-marker-macos-arm64.json) includes three
experiments, **125 jobs / 625 samples**, library/source hashes, combined-diff
identity and independently recomputed medians.
[Full audit](FOLLOWUP-AUDIT.md#native-marker-audit-2026-10-07) records all 62
checklist items. The audit strengthened block-boundary/reuse coverage and kept
the existing marker contents, ownership, frame eligibility and publication.

## Native callback continuations and scalar admission (2026-10-07)

The executed plan was to preserve the current build (`408affc4a`), remove
callback-result boxing where the consumer proves it unnecessary, measure the
result, then audit and commit only validated improvements. Inspection and the
first measurement identified two additional compiler-side gaps, addressed in
the same focused change:

1. A callback result with one normal StoreLocal consumer can carry its native
   payload directly into an exact native scalar local. The boxed cleanup slot
   owns only generic fallback results; the native edge stores NOTHING there.
2. Native-local argument loads reuse the existing flow-sensitive assignment
   proof. Previously, conservative producer facts caused wide native arguments
   to be boxed and then rejected by an int48-only guard.
3. Simple float add/subtract/multiply bodies reuse their complete FloatBinary
   leaf summary for native-entry admission. The expression-summary builder
   intentionally does not duplicate that summary. Division and comparison/
   branching targets remain outside this addition.

Current Program, debugger attachment, target-entry identity, observable frames,
source locations, stack protection, ownership and cancellation remain guarded.
No runtime helper ABI, serialization or IR-interpreter implementation changes.

### Rejected initial experiment

Result continuation alone was insufficient. Wide callbacks remained near
108 ns/element because arguments still fell back; generic float calls regressed
from 130.394 to 151.870 ns/element (**16.5% more time**) when continuation
lowering bypassed the existing immediate-scalar assignment shortcut. The final
implementation preserves that shortcut on generic returns.

The initial float targets used comparisons and therefore did not receive native
entries. Final native-float workloads use subtraction; the original branching
float target is retained as `indirect_branch_float`, a generic-dispatch control.
Before and after in each final run use identical revised sources. Do not compare
differently defined float kernels across experiments. The rejected run and its
source/compiler hashes remain in the evidence; no samples were discarded.

### Final before/after and Java comparison

macOS arm64, release Qore AOT, Temurin 25.0.2 / JMH 1.37. Five independent
forks, three warmups and five 300 ms measurements per fork, shuffled jobs,
1024 elements. Values are medians of fork means in ns/element, including the
loop, computation and checksum. Java callback targets retain DONT_INLINE.

|!Kernel|!AOT before|!AOT after|!Java JIT|!Qore time reduction
|Arithmetic control|2.859|2.853|0.892|No material change
|Direct call|2.863|2.880|0.893|No material change
|Small-integer callback|19.890|19.295|1.671|3.0%
|Wide-integer callback|107.641|19.306|1.673|82.1%
|Native float callback|123.593|19.580|1.643|84.2%
|Heap-encoded float callback|197.871|19.701|1.637|90.0%
|Generic branching float control|130.917|127.026|1.648|3.0%

Wide/native-float improvements repeat with nonoverlapping fork-mean ranges at
4096 elements: **106.480 to 18.352**, **122.996 to 18.688**, and
**196.159 to 18.535** respectively (82.8%, 84.8%, and 90.6% less time).
The small-integer repeat improves only **1.0%**, with overlapping ranges:
18.764 to 18.572. Treat its benefit as modest, not a large call-overhead win.
The generic float control improves 3.9% in the repeat, so the rejected
fallback regression is resolved. Arithmetic, direct-call, polymorphic and
temporary-hash controls have overlapping ranges; no general throughput
improvement is claimed for them.

Native text shrinks from **76416 to 75452 bytes** in both final runs. Recorded
compilation and startup observations are retained but are not evidence of
compile/startup speedups. Profiling was separate from timings, as were builds
and tests.

### Remaining bottleneck and next optimization plan

Wide integers and native floats now reach approximately the same cost as the
existing small-integer native callback path. They still take **11.5-12.0x
Java's time** in this comparison. Direct-call loop throughput is about 3.2x
Java, with effectively no extra cost versus the Qore arithmetic control.
These are retained-call workloads, not isolated call-latency measurements or
a claim about all Java/Qore programs.

A separate final wide-callback profile no longer places general boxing/
coercion on the per-iteration hot path. It still finds native-frame construction/
restoration, typed-reference validation, stack checks and cancellation.
Sampling is qualitative; counts are not additive cost attribution.

The next staged work should therefore be:

1. Prepare immutable callback target/signature facts in an invocation-scoped
   descriptor, with explicit ownership and invalidation. Keep current Program,
   debugger and compiled-entry identity checks live; preserve lazy materialization.
2. Design a compiler-visible compact native frame and end-to-end RuntimeConfig
   entry ABI. Preserve the concurrent-reader publication/pop handshake, source
   metadata, policy context and empty-frame boundaries. Consolidate stack checks
   only with a proven stack-budget contract, not by deleting guards.
3. Extend whole-body expression summaries to additional non-throwing float
   consumers/branches, then guarded method dispatch. Measure each independently
   against the retained-call and generic controls before widening admission.

The first two target the remaining native-call overhead; the third addresses
the much slower generic paths. This change establishes neither Java parity nor
that parity is impossible under Qore's runtime contracts.

### Validation and evidence

Final selected release and debug runs each pass **107 cases / 11931 assertions**
across 19 wrappers, plus **four standalone scripts**. The new continuation
wrapper contributes 170 assertions per build and checks native float adapter
selection as well as division exclusion. It covers AST/IR/JIT/tiered, AOT O0/O3,
disabled paths, v1/v2 helpers, EH, stripped source/debug info, split codegen,
unassigned/deleted inputs, wide values, NaNs, alternating native/generic results,
owned rejection cleanup, captured callbacks and foreign Programs. Its fixture
also passes on the saved baseline in all four modes and AOT O0/O3.

All 165 records from a pre-change AOT artifact remain identical on the new
runtime; every selected final benchmark implementation passes 225 independent
reference checks. The ten Python harness tests pass.

[Sanitized evidence](results-native-continuation-macos-arm64.json) retains
**285 jobs / 1425 samples**, including the rejected initial candidate, source/
library/compiler hashes, startup observations and independently checked medians.
[Full audit](FOLLOWUP-AUDIT.md#native-continuation-audit-2026-10-07) records all
62 checklist items. The audit added the explicit outer-scope exclusion,
unassigned/deleted-input coverage, and the generic-fallback performance control.
No full-suite, sanitizer, Linux or allocation-count claim is made; previously
recorded correctness and toolchain limitations remain separate.

## Prepared callback targets (2026-10-07)

The next iteration caches immutable target/signature facts in an owned,
invocation-local descriptor. Its owner word is also its validity token and
participates in persistent exit/exception cleanup. Exact non-capturing function
references only are retained; generic callback destruction timing is unchanged.
Program/debugger/compiled-entry and lazy context/PC-map readiness remain live.
This adds v3 helpers without changing v1/v2 or serialized AOT metadata.

### Controlled before/after results

Baseline is `75d26dde3`. Identical benchmark sources and five shuffled forks
use three warmup and five measurement windows of 300 ms. Values below are
median fork means, ns/element including loop/computation/checksum, not isolated
call latency. Java is Temurin 25.0.2/JMH 1.37 with retained DONT_INLINE targets.
No builds, tests or profiles ran alongside either timing experiment.

Count 1024 (`run-pvs_rt7k`):

|!Kernel|!Before|!After|!Reduction|!Java
|arithmetic|2.868|2.878|-0.3%|0.911
|direct_call|2.888|2.892|-0.1%|0.893
|indirect_call|19.381|15.849|18.2%|1.676
|indirect_wide|19.456|15.829|18.6%|1.678
|indirect_float|19.600|16.006|18.3%|1.649
|indirect_tagged_float|19.648|16.307|17.0%|1.642
|indirect_branch_float|127.201|129.103|-1.5%|1.659

Count 4096 repeat (`run-wrwqxgwx`):

|!Kernel|!Before|!After|!Reduction
|arithmetic|2.034|2.033|0.0%
|direct_call|2.040|2.043|-0.2%
|indirect_call|18.635|14.896|20.1%
|indirect_wide|18.625|14.950|19.7%
|indirect_float|18.471|15.206|17.7%
|indirect_tagged_float|18.529|15.409|16.8%
|indirect_branch_float|125.102|126.640|-1.2%
|polymorphic_call|272.337|274.841|-0.9%
|temporary_hash|304.948|318.005|-4.3%

All four native callback fork ranges are nonoverlapping in both runs:
**17-20% less time**. Arithmetic/direct-call controls overlap. Generic
branch-float medians are 1.2-1.5% slower, with overlapping ranges in both runs;
that small consistent cost is retained, not presented as an improvement.
Polymorphic and temporary-hash controls also overlap; temporary-hash's 4.3%
median increase is noisy (after 302.4-322.1 versus before 300.4-305.7 ns).
No generic speedup or proof of zero regression is claimed.

Native text grows **75452 to 77592 bytes** (2.8%); the stack-local descriptor
and its cleanup trade code/stack space for less repeated validation. Compilation
and startup observations are retained but are not speedup evidence.
Native retained callbacks still take **9.4-9.9x Java's time** at count 1024.

### Validation, audit and next iteration

Release and debug each pass **107 cases / 11969 assertions** in 19 wrappers
plus four standalone scripts. Added cases cover fresh target replacement,
caught-exception reuse, mixed local/foreign targets, and debugger attach/detach
after warm-up. The strengthened fixture also passes the saved baseline.
A pre-change AOT artifact's 225 records match the new runtime byte-for-byte;
each benchmark implementation passes all 225 independent reference checks.
Ten Python harness tests pass.

[Evidence](results-prepared-target-macos-arm64.json) retains **195 jobs /
975 samples**, all finite and positive with independently recomputed medians,
source/library/compiler hashes, inspection hashes and known limitations.
[Full audit](FOLLOWUP-AUDIT.md#prepared-target-audit-2026-10-07) reports all
62 checks. The audit specifically checked ownership, cache invalidation,
target pointer layout, live debugger guards and unchanged publication order.
No full-suite, sanitizer, Linux or allocation-count claim.

A separate five-second profile removes repeated boxed type/RTTI work from the
retained-callback hot path. Frame construction/restoration dominates; the
cache-hit function is still outlined. The first diagnostic profile overlapped
a qcc inspection, so it was discarded and repeated alone; neither overlapped
benchmark timings. Sampling is qualitative, not additive cost attribution.

Next, measure inlining the native frame helper and keeping the positive cache
hit inline while outlining cache misses. Preserve all stack guards, markers,
context restoration and stack-reader handshakes. Then revisit the end-to-end
RuntimeConfig entry ABI and compact observable-frame design. Java parity is
not reached, and the unchanged generic float-branch path still needs a separate
whole-body proof extension.

## Native frame and cache-hit inlining (2026-10-07)

This iteration changes C++ inlining boundaries, not the native helper ABI.
Prepared-target misses are outlined; positive-hit guards and native-frame
construction/restoration are visible to the typed helper's register allocator.
The enclosing diagnostic frame is included: a development disassembly showed
that hinting only its publishing member merely moved the out-of-line boundary.
That unmeasured prototype was refined and all tests rerun before timing.

All guards, stack checks, empty-frame markers, policy restoration and stack-reader
synchronization remain. Implementation-only hints retain ordinary out-of-line
symbols for split-compilation builds. Results below use the default
single-compilation-unit Clang build; split-build/GCC performance is unmeasured.

### Before/after measurements

Baseline is `1faf357d1`, including prepared targets. Identical sources, five
shuffled forks, three warmup/five measurement windows at 300 ms; ns/element
includes loop/computation/checksum. Java is Temurin 25.0.2/JMH 1.37 with
DONT_INLINE targets. Builds, tests and profiles did not overlap timing.

Count 1024 (`run-01xe0msk`):

|!Kernel|!Before|!After|!Reduction|!Java
|arithmetic|2.878|2.869|0.3%|0.890
|direct_call|2.890|2.882|0.3%|0.898
|indirect_call|15.797|13.342|15.5%|1.669
|indirect_wide|15.782|13.557|14.1%|1.684
|indirect_float|16.060|13.384|16.7%|1.648
|indirect_tagged_float|16.034|13.517|15.7%|1.651
|indirect_branch_float|127.886|128.923|-0.8%|1.657

Count 4096 repeat (`run-whu_tuy9`):

|!Kernel|!Before|!After|!Reduction
|arithmetic|2.036|2.033|0.2%
|direct_call|2.032|2.040|-0.4%
|indirect_call|14.878|12.556|15.6%
|indirect_wide|14.921|12.568|15.8%
|indirect_float|15.134|12.474|17.6%
|indirect_tagged_float|15.260|12.589|17.5%
|indirect_branch_float|127.722|129.700|-1.5%
|polymorphic_call|273.566|271.914|0.6%
|temporary_hash|310.694|307.122|1.1%

All four native callback ranges are nonoverlapping in both runs: **14-18%
less time** incrementally. Arithmetic/direct-call controls overlap. Generic
branch-float medians are 0.8-1.5% slower with overlapping ranges; other generic
controls are noisy and overlap. These observations do not establish zero
regression on generic paths. Native callbacks still take **8.0-8.2x Java's
time** at count 1024; this is not isolated call latency or Java parity.

AOT native text stays **77592 bytes**. Runtime-library text grows **13837892
to 13859680 bytes**, about 22 KB / 0.16%. Compilation/startup observations are
retained, not claimed as improvements.

### Validation and next measured target

Release/debug each pass **107 cases / 11969 assertions** in 19 wrappers plus
four standalone scripts, including stack exhaustion, debugger attach/detach,
concurrent walkers, cancellation, Program interruption, ownership and caught
exception reuse. A pre-change v3 AOT executable matches all 225 checksum records.
Every timed implementation passes the independent 225-record check; all ten
Python harness tests pass.

[Evidence](results-native-frame-inline-macos-arm64.json) retains **195 jobs /
975 samples**, independently recomputed medians and library/source/inspection
hashes. [Full audit](FOLLOWUP-AUDIT.md#native-frame-inlining-audit-2026-10-07)
covers all 62 checks. Audit/disassembly verified that the frame member ordering,
reader handshake and out-of-line compatibility symbols remain intact.

Separate five-second callback and arithmetic profiles did not overlap any
build/test/timing work or each other. Callback bookkeeping now samples inside
the typed helper, with no out-of-line frame/cache-hit layer in disassembly.
The arithmetic profile is dominated by the cancellation helper and its call
stub. Sample counts are qualitative, not additive time attribution.

The next priority is therefore an inline no-pending cancellation fast path at
every existing generated loop checkpoint. It must read live thread-cancel and
Program-interrupt flags with their existing seq_cst ordering, use the full
helper for pending requests or unavailable thread context, preserve deferral/
scope/priority and shutdown behavior, and never lower polling frequency.
After that, continue end-to-end RuntimeConfig callee entry work, then broader
non-throwing float-branch summaries and guarded method calls. No runtime
guarantee is to be traded away to claim Java-level speed.

No full-suite, sanitizer or Linux validation claim. Previously documented
legacy debug-wrapper selection, recursive-JIT-capture, nested AOT source-line
and default-target linker-warning limitations remain separate.

## Inline no-pending cancellation checks (2026-10-07)

Generated JIT/AOT code now borrows live flag addresses once per invocation and
checks deferral, thread cancellation and the global Program-interrupt count
at every existing checkpoint. Both request loads remain seq_cst. Pending
requests, unavailable thread state and unsupported primitive atomic layouts
call the full helper; no scope, manager, priority or diagnostic decision is
cached. Language loops still poll every iteration, and fused internal loops
retain their existing interval. Old helper ABIs and RuntimeConfig layout remain.

### Before/after measurements

Baseline is `77f11fc72`. Identical sources, five shuffled forks, three warmup
and five measurement windows of 300 ms; median fork means in ns/element include
loop/computation/checksum. Java is Temurin 25.0.2/JMH 1.37 with retained
DONT_INLINE targets. No builds/tests/profiles overlapped either timing run.

Count 1024 (`run-saxqlud0`):

|!Kernel|!Before|!After|!Reduction|!Java
|arithmetic|2.951|2.180|26.1%|0.924
|direct_call|2.963|2.182|26.4%|0.923
|indirect_call|13.846|12.802|7.5%|1.704
|indirect_wide|13.924|12.790|8.1%|1.709
|indirect_float|13.606|12.812|5.8%|1.673
|indirect_tagged_float|13.594|12.959|4.7%|1.671
|indirect_branch_float|132.682|129.303|2.5%|1.686

Count 4096 repeat (`run-t49g5i82`):

|!Kernel|!Before|!After|!Reduction
|arithmetic|2.091|1.310|37.3%
|direct_call|2.081|1.324|36.4%
|indirect_call|12.887|11.833|8.2%
|indirect_wide|12.855|11.846|7.8%
|indirect_float|12.704|12.100|4.8%
|indirect_tagged_float|12.653|11.979|5.3%
|indirect_branch_float|131.283|129.920|1.0%
|polymorphic_call|282.017|281.428|0.2%
|temporary_hash|318.602|322.804|-1.3%

Arithmetic/direct loops take **26-37% less time**, and all four native callback
loops take **5-8% less time**, with nonoverlapping fork ranges in both runs.
Generic branch/dispatch/allocation control ranges overlap; temporary-hash's
median is 1.3% slower. These controls do not establish either a generic gain
or zero regression. Native callbacks still take **7.5-7.8x Java's time** at
count 1024; arithmetic/direct loops take about 2.36x. None is isolated call
latency, and Java parity is not reached.

AOT native text grows **77592 to 81864 bytes** (5.5%); runtime text grows
**13859680 to 13861796 bytes**. Compilation/startup observations are retained
but are not speedup evidence.

### Validation, audit and next target

Release/debug each pass **108 cases / 12085 assertions** in 20 wrappers plus
seven standalone scripts. The loop wrapper now checks while/for/do/foreach,
both disable controls, JIT/tiered, AOT O0/O3, EH, stripped debug information
and split compilation, including emitted seq_cst loads. Native tests compare
full and inline checks under manager changes, deferral, request/clear races,
null descriptors and repeated foreign-thread registration/teardown. Fused-loop
and linked-executable shutdown checks also pass. An older AOT artifact matches
all 225 checksum records; every measured implementation passes all 225
independent checks, and ten Python harness tests pass.

[Evidence](results-inline-cancel-macos-arm64.json) contains all **195 jobs /
975 samples**, independently verified medians and source/library/inspection
hashes. [Full audit](FOLLOWUP-AUDIT.md#inline-cancellation-poll-audit-2026-10-07)
reports all 62 checks, with particular attention to borrowed address lifetime,
sequential consistency, pending-request fallback and teardown behavior.

Separate sequential five-second profiles show no cancellation helper in the
hot loop. AArch64 disassembly retains live atomic request loads and sink
checks. Native callback frame bookkeeping still dominates sampling; the
callee stack-check/TLS path remains visible. Sampling is qualitative, not
additive cost attribution. Next, pass RuntimeConfig through an additive typed
callee entry while retaining old entries and all stack/observer guarantees;
then revisit compact frames and generic float-branch admission.

No full-suite, sanitizer or Linux claim. Legacy debug-wrapper child-library
selection and the previously recorded recursive-JIT-capture, nested-AOT-line
and default-target-linker-warning limitations remain separate.

## RuntimeConfig-aware typed callee entry (2026-10-07)

An optional typed callee-entry v2 appends RuntimeConfig after the native
arguments, AOT context and exception sink. Helper v2/v3 prefers it; older
helpers/references retain the original entry. The compiler reuses the complete
non-throwing scalar summary and emits the same mandatory stack guard through
`qore_rt_check_stack_v2`. Calling the old fast entry would have kept its TLS
lookup, so the bounded body is re-emitted instead. Unsupported or disabled
summaries retain v1. No callback eligibility, RuntimeConfig layout, serialized
metadata or prepared-target descriptor is widened or changed.

### Controlled measurements

Baseline is `23434f6c8`, including inline cancellation polling. Same sources,
five shuffled forks, three warmup and five measurement windows of 300 ms.
Values are median fork means in ns/element including loop/computation/checksum,
not isolated call latency. Java is Temurin 25.0.2/JMH 1.37, retained DONT_INLINE
targets.

Count 1024 (`run-_4n7bg4q`):

|!Kernel|!Before|!After|!Reduction|!Java
|arithmetic|2.093|2.101|-0.4%|0.888
|direct_call|2.080|2.090|-0.5%|0.890
|indirect_call|12.522|11.982|4.3%|1.670
|indirect_wide|12.382|11.975|3.3%|1.682
|indirect_float|12.771|12.258|4.0%|1.642
|indirect_tagged_float|12.664|12.225|3.5%|1.640
|indirect_branch_float|127.217|126.374|0.7%|1.665

Count 4096 isolated repeat (`run-nquraq8x`):

|!Kernel|!Before|!After|!Reduction
|arithmetic|1.270|1.275|-0.5%
|direct_call|1.262|1.269|-0.6%
|indirect_call|11.640|11.081|4.8%
|indirect_wide|11.699|11.121|4.9%
|indirect_float|11.833|11.379|3.8%
|indirect_tagged_float|11.785|11.276|4.3%
|indirect_branch_float|126.660|126.712|-0.0%
|polymorphic_call|271.078|271.639|-0.2%
|temporary_hash|303.605|305.849|-0.7%

Native callback loops take **3.3-5.0% less time**, with nonoverlapping fork
ranges in both primary experiments. Arithmetic/direct medians are 0.4-0.6%
slower, with overlapping ranges. Generic controls also overlap; no generic
gain or proof of zero regression is claimed. Native callbacks still take
**7.1-7.5x Java's time** at count 1024.

The first 4096 repeat, `run-v0zgbvbz`, briefly overlapped two qpp documentation
checks near job 55/90. Its 90 jobs / 450 samples are retained as diagnostic
data, not used above. The entire repeat was rerun in isolation. Neither primary
experiment overlapped a build, test or profile.

AOT native text grows **81864 to 82344 bytes** (+480 / 0.6%); runtime text grows
**13861796 to 13865396 bytes** (+3600). Startup and compilation observations
are retained but not used as speedup evidence.

### Validation, audit and remaining work

Release/debug each pass **109 cases / 12280 assertions** in 21 wrappers plus
seven standalone checks. The expanded callback wrapper has 400 assertions
covering entry/summary disable controls, O0/O3, split/EH/stripped compilation,
zero/unary scalar values, ownership, foreign Program fallback, target
replacement and debugger attachment. A new native fixture exhausts the stack
through both guards, clears the sink, verifies recovery and repeats on a
foreign thread after re-registration. An old AOT executable matches all 225
records; every benchmark implementation passes 225 independent checks and ten
Python harness tests pass.

[Evidence](results-callee-runtime-config-macos-arm64.json) retains **285 jobs /
1425 samples**, including **195 primary jobs / 975 primary samples** and the
diagnostic run. Medians were independently recomputed.
[Full audit](FOLLOWUP-AUDIT.md#typed-callee-runtimeconfig-audit-2026-10-07)
covers all 62 checks. Review additionally cleared optional v2 bindings when v1
is rebound and removed the creating caller's debug scope from the generated
adapter before final tests/measurements.

A separate five-second profile and disassembly confirm the callee guard no
longer looks up TLS. Call-helper/frame bookkeeping remains dominant; all
caller/frame/callee checks, parse-option restoration and stack-reader handshakes
remain. Sampling is qualitative, not additive cost attribution.

Next, extend complete float summaries to non-throwing comparison/select
bodies, which still use generic dispatch at roughly 126 ns/element here.
Preserve ordered/unordered NaN comparisons, signed zero, branch laziness for
throwing/side-effecting operations and imported-summary validation. Then revisit
compact observable frames and guarded call-site invariants. Java parity remains
an active, incomplete goal, not a conclusion of these microbenchmarks.

No full-suite, sanitizer or Linux claim; previously documented debug-wrapper
selection, recursive-JIT-capture, nested-AOT-location and default-target-linker
limitations remain separate.

## Float comparison/select callback summaries (2026-10-07)

Baseline: `b6923e800`, saved before editing. Complete float summaries now
accept all six typed comparisons, branch-return trees and a single four-block
ternary diamond. Comparisons produce bool nodes; arithmetic and both selected
results remain float. Equality/order are ordered and inequality is unordered,
matching ordinary lowering for NaNs. Signed zero is preserved.

The collector requires complete, bounded, side-effect-free bodies and rejects
speculative division, generic calls, handlers and loops. Imported node types,
indices, predecessors and result types are validated. New optional node tags
do not change the serialized record layout: an old reader declines the summary
and uses ordinary calls. Native-entry admission still requires the existing
context-independence proof; live guards, ownership, RuntimeConfig, frame
publication/restoration, cancellation and both stack checks are unchanged.

### Isolated before/after measurements

Same shuffled five-fork methodology, three warmup and five 300 ms measurement
windows per fork. Units are ns/element including loop, calculation and checksum,
not isolated call latency. Java targets retain DONT_INLINE.

|!Branch-float callback|!Before|!After|!Time reduction|!Speedup
|1024 elements, run-uzzl65js|128.82348|12.36752|90.40%|10.42x
|4096 elements, run-icxfq0lk|125.18379|11.44669|90.86%|10.94x

Fork ranges do not overlap in either experiment: 126.12134-129.39152 versus
12.29808-12.55237 at 1024; 125.00519-126.09349 versus 11.36745-11.50890
at 4096. Java's 1024-element median is **1.65920**, leaving the optimized
Qore callback **7.45x slower**, not at parity.

Controls show no consistent general speedup. At 1024, arithmetic/direct move
-0.08%/+0.05% and previously native callbacks move -0.66% to +1.99% in time.
At 4096, arithmetic/direct move -0.59%/-0.20%, native callbacks -0.77% to
-1.82%, polymorphic dispatch -0.39%, and temporary hashes +0.93%.
Most fork ranges overlap; the small float improvement in the repeat does not
replicate with nonoverlapping ranges in the first run. All samples, including
an 18.06683 tagged-float fork and noisy generic-control forks, are retained.
No builds, tests, profiles or documentation validators overlapped either run.

[Evidence](results-float-comparison-macos-arm64.json) contains **195 jobs / 975
samples**, exact source/binary hashes, all fork/window values, and independently
recomputed medians. Startup/compilation observations are not used as speedup
evidence. The later source change only clarifies the benchmark control comment;
each experiment retains its exact source snapshot/hash.

AOT native text grows **82344 to 82972 bytes** (+628 / 0.76%); runtime text grows
**13865396 to 13868224 bytes** (+2828). Separate generated-code inspection shows
the branch callback's typed v1/v2 bindings and native compare/select. A separate
five-second profile attributes 2579 top-of-stack samples to the typed helper,
805 to its caller, 80 to the typed callee and 76 to its stack guard. These are
qualitative observations, not additive cost estimates; frame/helper work
remains the next target.

### Validation and audit

Release/debug each pass **155 cases / 13331 assertions** in 23 wrappers, plus
11 standalone checks. This includes the expanded 632-assertion comparison
wrapper and 44-case/419-assertion symbol-index suite. Comparisons are checked in
both operand positions for finite values, infinities and both NaNs; tests cover
signed zero, nested branches, unselected division and side effects, all 15
eligible native entries, O0/O3, split/EH/stripped compilation, disabled paths
and cross-object imports. The saved baseline passes the final explicit-truth-
table fixture; old/new provider/consumer object combinations pass in both
directions. An old AOT executable matches all 225 records; each measured
implementation passes 225 independent checks. Ten Python harness tests pass.

Test development exposed an unrelated nested-list-index oracle problem:
`row[expectedIndexes[i]]` produced 2.0 for the second entry of the first
truth-table row, where the independent expected value is 1.0. The original
fixture fails identically with saved b6923e800 and with float summaries disabled.
The ignored reproducer is
`build/bench-runtime-comparison/float-compare-diagnostic.qr`
(SHA-256 `32d0d6356cbe9b70e3ff7ee221b9f33d6e922c3fc174244fba06f1e1052a055d`).
The committed comparison oracle passes expected values directly and does not
depend on nested indexing. This finding remains unresolved and is not a
comparison-optimization fix.

The debug symbol-index suite initially used a stale April reflection binary;
rebuilding the debug reflection target restored all cases. Its wrapper also
now explicitly sets DYLD_LIBRARY_PATH for child processes, matching its
existing Linux-library selection.

The [full 62-check audit](FOLLOWUP-AUDIT.md#float-comparison-summary-audit-2026-10-07)
influenced boolean operand validation, safe PHI condition retention, conservative
branch laziness, explicit semantic oracles and child-library selection.
No full-suite, sanitizer or Linux claim; previously documented limitations
remain. Next: measure a smaller prepared-target-backed observable frame,
retaining live checks, state restoration and stack-reader synchronization.

## Compact prepared-call observable frame (2026-10-07)

Baseline: `0f3791786`, saved before editing. The v3 diagnostic frame now
borrows name, statement-block and Program metadata from the invocation-local
prepared descriptor instead of copying those three pointers on every call.
Caller location stays local; the state-saving helper is unchanged and remains
the last member. The exact scalar leaf cannot reenter or replace its cache.
The existing pop/walker handshake completes before any later descriptor
replacement or cleanup. V2 keeps the previous frame; no helper ABI, cache
layout, live guard, marker, policy or stack/cancellation check changes.

### Measurements and tradeoffs

Two isolated, shuffled five-fork experiments use the same contract as above.
Values are ns/element; negative time change is an improvement, not an isolated
call-latency measurement.

|!Kernel|!1024 before|!1024 after|!Time change|!4096 before|!4096 after|!Time change
|indirect_call|11.85653|11.69596|-1.35%|10.90207|10.92073|0.17%
|indirect_wide|12.02387|11.86649|-1.31%|10.89817|10.75328|-1.33%
|indirect_float|12.29293|11.95272|-2.77%|11.30442|11.05103|-2.24%
|indirect_tagged_float|12.17142|11.83242|-2.79%|11.20197|10.96094|-2.15%
|indirect_branch_float|12.41799|12.01120|-3.28%|11.29143|11.15981|-1.17%

The ordinary and tag-colliding float improvements have **nonoverlapping fork
ranges in both experiments**: a reproducible **2.15-2.79%** reduction. Other
callback improvements are less conclusive: integer ranges overlap, the small
integer median reverses direction in the repeat, and the branch-float ranges
overlap at 4096. This is not a universal call-speedup claim.

Arithmetic/direct medians move +0.77%/+1.13% at 1024 and +0.80%/+0.29% at
4096, all with overlapping ranges. Polymorphic/temporary-hash medians move
+0.42%/+0.43% in the repeat, also overlapping. The 18.63990 wide-callback fork
and all noisy controls are retained, not filtered. No build, test, profile or
documentation validator overlapped either timing run.

The 1024-element Java callback medians are 1.63941-1.67317; these Qore
callbacks still take **7.0-7.3x** as long. Java parity is not achieved.
[Evidence](results-compact-frame-macos-arm64.json) retains all **195 jobs /
975 samples**, hashes and independently recomputed medians from
`run-gasvhm51` and `run-98kuce3r`.

Disassembly confirms the integer helper's stack allocation shrinks **272 to
256 bytes**. A compile-time size assertion also requires the new frame to be
smaller than v2's. AOT text remains **82972 bytes**; runtime text grows
**13868224 to 13869316 bytes** (+1092) for the separate descriptor-backed class.
This small code-size cost accompanies the measured float improvement, not an
assumption that fewer source-level copies always improve performance.

A separate five-second wide-callback profile still finds helper/frame work
dominant (2631 top-of-stack samples versus 735 in the caller, 87 in the callee
and 84 in the callee stack helper). Sampling is qualitative; it does not
isolate additive costs.

### Validation and next experiment

Release/debug each pass **156 cases / 13403 assertions** in 24 wrappers plus
11 standalone checks. The new 72-assertion wrapper runs four callers and two
readers, with 400 all-thread snapshots per reader. Callers alternate cache-hit
batches with target-replacement batches and repeatedly destroy invocation
caches. The fixture checks leaf and enclosing frame names, types, locations
and callback checksums. It covers AST/IR/JIT/tiered, O0/O3, v2, generic fallback,
EH and split AOT; its pre-change IR and AOT O3 runs also pass.

Old AOT output matches 225 records; each benchmark implementation passes
225 independent checks, and ten Python harness tests pass. Existing ownership,
foreign-Program, debugger, exception, stack-exhaustion and cancellation tests
remain in the selected matrix. The [full audit](FOLLOWUP-AUDIT.md#compact-prepared-frame-audit-2026-10-07)
covers all 62 checks and specifically reviews borrowed descriptor lifetime
against the pop/walker handshake. No full-suite, sanitizer or Linux claim.

Next: evaluate an additive scalar callee entry with an inline live stack-limit
check. Keep prior entries and the full check on failure, unavailable state or
unsupported targets. This aims to remove the remaining out-of-line callee
guard cost without removing stack protection; it is not yet implemented or
measured in this section.

## Inline scalar callee stack guard (2026-10-07)

Baseline: `9438835b4`, preserved before editing. Optional callee-entry v3
appends a live stack-limit address to the explicit-context v2 ABI. The runtime
supplies it only for managed downward-growing 64-bit x86-64/AArch64 stacks.
Other configurations supply null and retain the full check. The limit and
native SP are read on every invocation; no limit value or SP is cached.

The compiler emits a null check and unsigned limit/SP comparison. Null or
failed checks return through the existing v2 entry, preserving its full guard,
overflow exception and result handling. The tail-call hint avoids merging a
helper return into the v3 body; a noinline call-site attribute preserves this
cold boundary. Prior entries remain available. Binding v3 requires validated
v2 metadata and normal-body identity; replacing v1/v2 clears v3. The disable
control is `QORE_DISABLE_AOT_FUNCREF_INLINE_STACK_CHECK`. Existing helper and
prepared-cache ABIs, admission proofs, caller/frame guards, policy state,
markers, cancellation and observer synchronization remain unchanged.

### Rejected first design and retained revision

The first design called the full guard and merged back into the scalar body.
Its two isolated runs (`run-9w6fkx60`, `run-7d620r7u`) showed no repeatable
benefit: ordinary float medians worsened 0.82% and 0.71%, and most callback
ranges overlapped. Disassembly showed a 32-byte callee frame with an argument
saved across the cold helper call even on success. Those 195 jobs / 975
samples are retained as negative evidence, not counted as improvement.

The revised cold return removes that saved argument and reduces the arm64
integer callee frame to 16 bytes. The enclosing native helper stays at 256
bytes. Two fresh shuffled five-fork comparisons produced the following
ns/element results (loop, computation and checksum included, not isolated
call latency):

|!Kernel|!1024 before|!1024 after|!Time change|!4096 before|!4096 after|!Time change
|indirect_call|11.79743|11.71259|-0.72%|10.91740|10.60618|-2.85%
|indirect_wide|11.81210|11.74741|-0.55%|10.83949|10.77973|-0.55%
|indirect_float|11.98546|11.90675|-0.66%|11.06115|10.92349|-1.24%
|indirect_tagged_float|11.90257|11.81354|-0.75%|11.05108|10.81365|-2.15%
|indirect_branch_float|12.05532|11.90934|-1.21%|11.13598|10.99187|-1.29%

Tagged-float ranges separate in both final runs: **0.75% and 2.15% less time**.
Ordinary floats separate only in the repeat; integer and branch ranges overlap.
All callback medians improve, but the evidence supports a modest, workload-
dependent benefit, not a universal speedup. Arithmetic/direct controls move
+0.21%/-0.03% at 1024 and -0.94%/+0.74% at 4096. Polymorphic and temporary-hash
repeat medians move +0.32% and +2.40%; their ranges overlap and all samples
remain included. No build, test, profile or docs validator ran during timings.

Java retained-call medians at 1024 are 1.64119-1.68025 ns/element. Qore still
takes **7.0-7.2x** as long: Java parity is not reached. Final runs
`run-ivz641wf` and `run-ikl3zooe` retain another 195 jobs / 975 samples.
[Evidence](results-inline-stack-macos-arm64.json) contains all four experiments
(**390 jobs / 1950 samples**), allowlisted hashes, ranges and independently
recomputed medians. AOT text grows 82972 to 83548 bytes (+576), and runtime
text 13869316 to 13871852 (+2536).

A separate five-second wide-callback profile records 2706 helper, 726 caller,
180 callee and 75 helper-stub top-of-stack samples. The callee's out-of-line
guard no longer appears among counts of five or more; this is qualitative,
not an additive attribution or proof that remaining frame work is removable.

### Validation, audit and next step

Each release/debug build passes **156 cases / 13929 assertions** in 24 wrappers
plus 11 standalone checks. Coverage includes AST/IR/JIT/tiered, O0/O3, disabled
v3, prior helpers, runtime generic fallback, EH/split AOT, IEEE float cases and
concurrent frame readers. The native harness exhausts and recovers the legacy,
explicit, live-view and forced-null guards, including three foreign-thread
registrations. It tests an equivalent C++ comparison; generated LLVM null/
failure edges and cold return are checked structurally, not invoked through
an exported generated-entry test. Ten Python tests and 225 old-artifact records
pass; every timed implementation passes 225 independent checks.

ELF64 x86-64 and ELF32 i386 objects are emitted and inspected. The qcc command
then exits 1 because its manifest checks the pre-suffix output name; the saved
baseline reproduces this unrelated CLI issue. This is object-code inspection,
not a clean cross-CLI or Linux runtime pass. No full-suite or sanitizer claim.

The [62-check audit](FOLLOWUP-AUDIT.md#inline-stack-guard-audit-2026-10-07)
drove explicit unsupported-layout fallback and live-address lifetime
documentation. No new filesystem/network operation or long runtime loop is
introduced. Next: identify which same-thread context saves/restores can safely
be deferred for proven non-reentrant scalar leaves, while preserving published
frames, empty local/closure markers and full failure diagnostics. Do not
assume that removing more checks is required or safe.

## Deferred pure scalar call context (2026-10-07)

Baseline: `9e74e1788`, saved before editing. Prepared calls to the re-emitted
pure scalar callee v3 now defer Program-call, closure, statement/location,
parse-option and saved-SP context traffic. These bodies can execute only
native scalar expressions or the full stack-guard edge: they cannot observe
or change those fields, allocate, or generically reenter Qore.

A same-thread invocation-local pointer identifies the deferred helper. Before
a full guard constructs an overflow exception, it materializes the original
context once, including live parse options and runtime overrides. Destruction
restores materialized fields in the original order. Successful calls leave
those fields untouched. The published diagnostic frame, caller location, empty
local/closure markers and pop/walker synchronization remain active on every
call. The publishing helper stays last. All live admission checks and stack/
cancellation checks remain; older entries and helper v2 stay eager. There is
no serialized, prepared-cache or native helper ABI change.

### Measured benefit and costs

The same isolated shuffled five-fork contract gives ns/element results below;
these include loop/computation/checksum work, not isolated call latency.

|!Kernel|!1024 before|!1024 after|!Time change|!4096 before|!4096 after|!Time change
|indirect_call|11.61965|10.44432|-10.11%|10.62244|9.58208|-9.79%
|indirect_wide|11.57739|10.48551|-9.43%|10.62372|9.49397|-10.63%
|indirect_float|11.77056|10.85802|-7.75%|10.79080|9.93837|-7.90%
|indirect_tagged_float|11.64819|10.78339|-7.42%|10.67099|9.89214|-7.30%
|indirect_branch_float|11.83330|10.91008|-7.80%|10.86487|9.98752|-8.08%

All five callback fork ranges separate in both runs: **7.30-10.63% less time**.
The controls do not show a general execution-speed improvement. Arithmetic
moves +1.64% at 1024 (separated ranges) and +0.16% at 4096 (overlap); direct call
moves +0.16% (overlap) and +0.64% (separated). Generic polymorphic/hash repeat
medians worsen 1.77%/1.73%, with overlapping ranges. All samples are retained.
The callback benefit is clear, but the small control regressions are a
tradeoff/continuing investigation, not silently classified as zero.

Java retained-call medians at 1024 are 1.63138-1.67142 ns/element; Qore still
takes **6.3-6.6x** as long. Java parity remains open.
[Evidence](results-deferred-context-macos-arm64.json) retains **195 jobs /
975 samples**, binary/source hashes and independently recomputed medians from
`run-fk5l8dsj` and `run-4p7lcn8n`. No build, test, profiler or documentation
validator overlapped either timing run.

The integer helper frame grows **256 to 304 bytes** despite fewer successful-
path state writes. AOT text is unchanged at 83548 bytes; runtime text falls
13871852 to 13865792 bytes (-6060). A separate five-second sample attributes
1342 top-of-stack samples to the constructor-selection lambda, 1335 to the
typed helper, 731 to the caller and 217 to the scalar callee. Disassembly
confirms an out-of-line lambda call. Sampling is qualitative, not an additive
cost model; removing that wrapper boundary is the next measured experiment.

### Validation and audit

Each final release/debug build passes **157 cases / 13957 assertions** in
25 wrappers plus 11 standalone checks, and ten Python harness tests pass.
Older callee-v2 and callee-v3 artifacts each reproduce 225 records. Each timed
implementation passes 225 independent reference checks.

The new native-boundary wrapper contributes 28 assertions. A native-register
script aggregate provides a validated typed callback in the same Program;
the host verifies the expected v3/eager binding and positive prepared cache.
It sweeps 4096 byte offsets of the current thread's test stack limit, restores
the real limit before inspecting errors, and requires both pre-frame and
published-leaf overflow failures. It checks exception diagnostics, caller
Program context, subsequent successful calls and a separate control function.
The sweep runs at O0/O3 with v3 enabled/disabled, on the main thread and three
successive foreign-thread registrations. The same final fixture passes the
saved baseline. Existing concurrent-reader, replacement/cleanup, captured/
generic/foreign-Program, debugger, policy, exception and cancellation coverage
remains in the selected matrix.

Fixture authoring initially used a qmod reference without static fast-call
metadata and a simple affine integer expression without typed admission.
The final fixture uses native script registration and an already admitted
masked expression. Those separate admission limitations were not changed or
mistaken for deferred-context failures.

The [full audit](FOLLOWUP-AUDIT.md#deferred-scalar-context-audit-2026-10-07)
covers all 62 checks. It drove forced boundary-failure testing and preservation
of restoration order, full exception materialization and observer lifetime.
No full-suite, sanitizer or Linux runtime claim. Next: measure explicit
inlining of the constructor-selection wrapper, retaining all state semantics;
then assess missing affine-integer admission with dedicated workloads.

## Inlined frame-helper selector (2026-10-07)

Baseline: `16efc2046`, saved before editing. The compile-time selector lambda
inside the already-inline diagnostic-frame constructor was still outlined by
the C++ compiler. An explicit inlining attribute removes that wrapper boundary
without changing context, publication, lifetime, admission, checks or ABI.

### Measurements

Isolated shuffled five-fork medians in ns/element, including loop/computation/
checksum work, not isolated call latency:

|!Kernel|!1024 before|!1024 after|!Time change|!4096 before|!4096 after|!Time change
|indirect_call|10.44586|9.74940|-6.67%|9.55752|8.85431|-7.36%
|indirect_wide|10.45421|9.67573|-7.45%|9.52998|8.82455|-7.40%
|indirect_float|10.82925|9.91949|-8.40%|9.92955|9.07715|-8.58%
|indirect_tagged_float|10.72821|9.95037|-7.25%|9.78668|9.09964|-7.02%
|indirect_branch_float|10.87642|10.02585|-7.82%|9.94822|9.12434|-8.28%

All five callback fork ranges separate in both runs: **6.67-8.58% less time**.
Arithmetic moves +1.16%/-0.41% and direct call -0.18%/+1.83% at 1024/4096.
Polymorphic/hash repeat medians move +0.64%/+1.56%. All control ranges overlap;
these are not claimed as general execution gains. Every sample is retained.

Java retained-call medians at 1024 are 1.63602-1.67007 ns/element. Qore still
takes **5.8-6.1x** as long; parity remains open.
[Evidence](results-frame-factory-macos-arm64.json) retains **195 jobs / 975
samples**, hashes and independently recomputed medians from `run-gnltzner`
and `run-jtqw56xx`. No build, test, profile or documentation validation
overlapped either timing run.

The integer helper frame shrinks **304 to 272 bytes**. AOT text stays 83548
bytes; runtime text grows 13865792 to 13876148 bytes (+10356). A separate
five-second sample no longer has the selector frame: top-of-stack counts are
2429 in the helper, 945 in the caller and 212 in the scalar callee. Disassembly
also confirms removal of the lambda call. Sampling is qualitative, not an
additive cost model.

### Validation and next step

Each release/debug build passes **157 cases / 13957 assertions** in 25 wrappers
plus 11 standalone checks; ten Python harness tests pass. Older callee-v2 and
callee-v3 artifacts each reproduce 225 records, and every timed implementation
passes 225 independent checks. Native overflow/materialization, concurrent
readers, fallback, replacement, policy, debugging and cancellation coverage is
unchanged. No full-suite, sanitizer or Linux runtime claim.

The [62-check audit](FOLLOWUP-AUDIT.md#frame-helper-selector-audit-2026-10-07)
requires preserving all frame/state semantics and recording code-size cost.
Next: extend native callback admission to existing nonthrowing scalar integer
summaries, which currently bypass integer-expression admission. Add equivalent
affine/branch workloads and prove correctness and actual fast-path selection
before measuring. This is separate from the unresolved qmod metadata case.

## Scalar-integer callback admission (2026-10-07)

Baseline: `761bedbc0`, saved before editing. The integer-expression builder
deliberately skips functions with a complete scalar-leaf summary, but native
callback admission previously ignored those summaries. Simple arithmetic,
affine compositions and branch-return bodies consequently fell back to
generic dispatch despite already having a complete proof.

NativeInt admission now accepts the six nonthrowing binary opcodes, affine
summaries and six integer comparisons selecting integer/affine results.
Callee v2/v3 reuse the existing operand/type-checked scalar-leaf emitter;
division/modulo, effects, unknown kinds and context-dependent bodies remain
excluded. All runtime guards, frame semantics, ownership and ABI are unchanged.
No serialized metadata change. The two-argument boxed adapter also benefits;
the unboxed ABI remains limited to zero/one argument.

### Measurements

Three new equivalent Qore/Java/Python workloads run against both compilers
from identical sources. Values do not overflow in these benchmarks; signed
wrapping is covered separately by regression tests. Five-fork medians below
are ns/element including loop/computation/checksum, not isolated call latency.

|!Kernel|!1024 before|!1024 after|!Time change|!4096 before|!4096 after|!Time change
|indirect_affine|114.56219|9.80514|-91.44%|114.32776|8.91374|-92.20%
|indirect_affine_wide|185.36084|9.87230|-94.67%|182.08436|8.86793|-95.13%
|indirect_branch_int|118.37036|9.86507|-91.67%|115.56476|8.93997|-92.26%

All three target fork ranges separate in both runs: **91.44-95.13% less time**
(11.7-20.5x throughput). This closes an admission gap; it does not speed up
callbacks that were already native. At 1024, arithmetic/direct/native-int/
native-float controls move +0.02%/+0.74%/-0.10%/+0.29%; at 4096 they move
-0.51%/+0.25%/+0.58%/-0.34%. Polymorphic/hash repeat controls move
+0.71%/-2.42%. All control fork ranges overlap and every sample is retained.

Java retained-call medians for the three targets are 1.64980-1.66860
ns/element; Qore remains **5.9-6.0x** slower. Parity is not achieved.
[Evidence](results-int-scalar-callbacks-macos-arm64.json) retains **195 jobs /
975 samples**, source/binary hashes and independently recomputed medians from
`run-964n2atc` and `run-t4x3z52b`. No builds, tests, profiling or documentation
validation overlapped either timing run.

With the same expanded benchmark sources, AOT text grows **90404 to 91320**
bytes (+916) and runtime text **13876148 to 13876284** (+136). The native
integer helper frame remains 272 bytes. A separate five-second sample places
2628 top-of-stack samples in the helper, 849 in the caller and 129 in the
affine callee; this is qualitative evidence, not an additive cost model.

### Validation, audit and next experiment

Each release/debug build passes **158 cases / 15329 assertions** in 26 wrappers
plus 11 standalone checks, and ten Python harness tests pass. The new wrapper
has 1372 assertions and verifies emitted native entries, not just outputs.
It covers AST/IR/JIT/tiered, O0/O3, eleven AOT configurations (summary/entry
controls, EH, stripped debug and split compilation), runtime fallback,
all arithmetic/bitwise and comparison cases, composed affine wrapping at
signed limits, wide boxed values, division/modulo exceptions and side effects.
Two-argument native boxed entry creation is checked separately.

Initial ternary cases already had general-expression admission, so the final
fixture explicitly uses branch-return bodies for compact CFG summaries.
Disabling that summary lets the existing general-expression proof take over;
the test checks that distinct behavior. The final fixture passes saved-baseline
AOT too. Each benchmark implementation now passes **270** independent checks.
New artifacts on the saved runtime and saved-compiler artifacts on the new
runtime each match 270 records; older callee-v2/v3 artifacts match 225 each.

The [62-check audit](FOLLOWUP-AUDIT.md#scalar-integer-callback-audit-2026-10-07)
drove explicit opcode exclusions, signed-edge coverage and actual admission
checks. No full-suite, sanitizer or Linux runtime claim. Next: separately
measure outlining uncommon readiness/eager-compatibility/variable-stack growth
paths from the retained-call helper, preserving all live checks and observer
semantics. The independent qmod metadata limitation remains out of scope.

## Rejected call-helper cold-path outlining (2026-10-07)

Baseline: `3519bad4f`. The experiment outlined lazy AOT context readiness,
legacy eager entry invocation and native local-stack marker block growth.
It preserved checks, state and frame publication but changed C++ boundaries.

**Rejected and production code reverted.** Smaller code did not produce a
repeatable retained-call benefit, and callee-v2 controls regressed. Five-fork
medians below are ns/element including loop/computation/checksum work.

|!Kernel|!1024 before|!1024 after|!Time change|!4096 before|!4096 after|!Time change
|indirect_call|9.92514|9.74530|-1.81%|8.90424|8.78893|-1.30%
|indirect_wide|9.92538|9.79170|-1.35%|8.86876|8.86165|-0.08%
|indirect_float|10.19365|10.19575|0.02%|9.15847|9.15780|-0.01%
|indirect_tagged_float|10.23261|10.18163|-0.50%|9.20536|9.20969|0.05%
|indirect_branch_float|10.31248|10.18980|-1.19%|9.26220|9.23823|-0.26%
|indirect_affine_wide|9.84094|9.77420|-0.68%|8.81903|8.83913|0.23%

Only `indirect_wide` separates in the primary run (-1.35%); every callback
range overlaps in the larger-loop repeat. Arithmetic/direct controls move
+0.06%/+0.19% at 1024 and +0.29%/+0.07% at 4096; generic polymorphic/hash
repeat medians move -1.13%/-2.02%. All these control ranges overlap.

A separate isolated run with
`QORE_DISABLE_AOT_FUNCREF_INLINE_STACK_CHECK=1` forces older callee-v2 entries.
Wide integer calls regress **11.71155 to 12.26196 (+4.70%)** and float calls
**11.84773 to 12.51261 (+5.61%)**, both with separated ranges. The added
compatibility-call boundary is therefore not acceptable for this experiment.

AOT text stays 91320 bytes; runtime text shrinks 13876284 to 13868632 (-7652),
and the helper frame shrinks 272 to 256 bytes. Those code-size benefits do not
establish a speedup. Java retained-call gap stays about 5.8-6.2x in the primary
run; parity remains open.

[Evidence](results-rejected-call-cold-macos-arm64.json) retains the rejected
production diff, hashes and **240 jobs / 1200 samples** from `run-q21s3e47`,
`run-lkb1vbju` and `run-qlh5thbu`. All medians are independently recomputed.
No build/test/profile/docs validator overlapped any timing run.

### Retained validation and next experiment

The candidate passes **159 cases / 15369 assertions** in 27 wrappers plus
11 standalones per release/debug build, ten Python tests, 270 independent
checks per timed implementation, and old callee-v2/v3 artifacts (225/270
records). Correctness passes did not override the performance rejection.

The new 40-assertion marker-boundary wrapper is retained. It exercises 0-70
live caller locals, thirteen native calls per caller and repeated entries,
through AST/IR/JIT/tiered and AOT O0/O3, v3/eager and runtime fallbacks.
It passes both final candidate builds and the saved baseline.
An initial direct-caller fixture kept locals in native slots and did not
reach block growth. Generic outer invocation fixes that; a separate debugger
run stops in the growth routine under native callback `padded53`, then
finishes successfully. This is explicit path evidence, not inferred coverage.

The [62-check audit](FOLLOWUP-AUDIT.md#rejected-cold-path-audit-2026-10-07)
requires reverting the unhelpful production diff while preserving the raw
evidence. No full-suite, sanitizer or Linux runtime claim.
Next: prove whether empty local/closure markers can be deferred for a
non-reentrant pure scalar v3 body, materializing them before exception work.
The diagnostic frame and its cross-thread reader handshake must remain.

## Rejected deferred variable markers (2026-10-07)

Baseline: `2bbe36d72`. The experiment deferred empty local/closure markers
until overflow materialized the context of a proven non-reentrant scalar v3
callee. Published diagnostic frames, reader synchronization and live checks
remained. Partial marker allocation and constructor exceptions had rollback.

**Rejected and production code reverted.** Integer callbacks improved, but
float callbacks regressed substantially. Five-fork medians are ns/element,
including loop, computation and checksum work, not isolated call latency.

|!Kernel|!1024 before|!1024 candidate|!Time change
|indirect_call|9.94625|8.39650|-15.58%
|indirect_wide|9.93811|8.29585|-16.52%
|indirect_affine_wide|9.82258|8.29644|-15.54%
|indirect_float|10.20828|14.40049|+41.07%
|indirect_tagged_float|10.27602|16.43320|+59.92%
|indirect_branch_float|10.18637|15.28241|+50.03%

All six callback ranges separate. Arithmetic/direct controls move +0.66%/
-0.96%, with overlapping ranges. The 4096 repeat confirms wide-int improvement
(8.88007 to 7.18744, -19.06%) and float regression (9.21077 to 13.90116,
+50.92%), both with separated ranges.

A second variant outlines only materialized exception cleanup. Its 4096 pilot
improves wide-int 8.91443 to 7.38669 (-17.14%), but float still regresses
9.16534 to 10.98306 (+19.83%); both ranges separate. This variant is rejected
too. No integer-only gain is presented as an overall callback improvement.

Initial runtime text changes 13876284 to 13876436 (+152 bytes); the cleanup
variant is 13870060 (-6224 versus baseline). The float helper frame stays
272 bytes in both variants; the initial integer helper is also 272 bytes.
A separate initial-candidate float profile has 2906
top-of-stack helper samples, 661 caller samples and 119 callee samples.
The machine-level cause of the float penalty is not established; profiling
and smaller code do not establish a speedup or a causal explanation.

[Evidence](results-rejected-lazy-markers-macos-arm64.json) retains both
production diffs, hashes and **160 jobs / 800 samples** from `run-grsv7ilr`,
`run-bycbw6if` and `run-er_ogh53`, with independently recomputed medians.
No build, test, profile or documentation validator overlapped timings.
Each measured implementation passes 270 independent checksum checks.
Java retained-call medians in the primary run remain 1.64-1.68 ns/element;
neither rejected variant establishes parity.

### Retained overflow coverage and next experiment

The initial candidate passes 159 cases / 15369 assertions in 27 wrappers plus
11 standalones per release/debug build, and ten Python harness tests. Older
callee-v2/v3 artifacts match 225/270 records. This matrix preceded the final
strengthening of the native host fixture; it is not a full-suite claim.

The retained `NativeLeafContext` host extension keeps 0-70 owned caller
strings and 1-71 captured integers live while sweeping the native stack limit.
Parsed callers explicitly use AST mode to put those values on TLS; the leaf
remains an actually bound AOT native entry. It validates overflow traces,
caller Program, locals, captures and successful recovery on the main thread
and three foreign-thread registrations, with O0/O3 and v3/eager controls.
The existing wrapper still reports 28 assertions; additional checks are
native C++ assertions. Final fixture checks pass saved-baseline and restored
release/debug runtimes.

Separate debugger runs prove local- and closure-block growth before exception
construction: position 64 becomes 1 in a new block, and both frame counts
increase from 0 to 1. Both runs continue to successful completion. The local
proof used the earlier single-capture fixture; the closure proof used the
expanded capture fixture. Early fixture errors (closure syntax, missing parse
lock, and callers using native rather than TLS locals) were corrected, not
counted as runtime failures. Allocation-failure injection was not performed.

The [62-check audit](FOLLOWUP-AUDIT.md#rejected-marker-deferral-audit-2026-10-07)
keeps the stronger test and negative evidence while reverting all runtime and
architecture-document edits. No full-suite, sanitizer or Linux runtime claim.

Next experiment: an additive prepared-call register-return convention for
value bits plus handled status, with old helpers retained. First verify the
C++/LLVM ABI on supported targets, then compare against the restored baseline.
Keep generic fallback/NOTHING behavior, sink and stack checks, live Program/
debugger/body/readiness guards, owned cache cleanup and diagnostic frame
publication. Inspect generated code for removal of the result-slot stores/
reload before measuring; do not assume it explains or fixes this regression.

## Rejected register-return calling convention (2026-10-07)

Baseline: `61345f823`. Two additive helper-v4 experiments returned native
payload and handled status in registers, retaining the prepared-v3 cache and
all older entry points. Target selection occurred after the AOT target layout
was known, restricted to 64-bit Darwin/Linux AArch64/x86-64; other targets kept
v3. No admission, ownership, readiness, stack or observer guard was removed.

**Both variants rejected and reverted.** Five shuffled forks, three warmup and
five 300 ms samples per fork, 4096 elements. Medians below include loop,
computation and checksum work; they are not isolated call latency.

|!Variant|!Kernel|!Before ns/element|!Candidate ns/element|!Time change
|Integer-register payload/status|indirect_wide|8.93278|8.94969|+0.19%
|Integer-register payload/status|indirect_float|9.21309|9.79966|+6.37%
|Same runtime, v3 control|indirect_wide|8.90537|8.93815|+0.37%
|Same runtime, v3 control|indirect_float|9.19544|9.17024|-0.27%
|FP-register float payload/status|indirect_wide|8.87308|8.96673|+1.06%
|FP-register float payload/status|indirect_float|9.17281|9.23937|+0.73%
|FP-register float payload/status|indirect_tagged_float|9.16733|9.07699|-0.99%
|FP-register float payload/status|indirect_branch_float|9.25740|9.25645|-0.01%

Only the first variant's float regression has separated fork-mean ranges.
All other ranges overlap. Returning through registers did not establish a
useful overall gain. There was no fresh Java timing in this pilot; the earlier
retained-call gap remains unresolved.

Native C++/LLVM ABI probes pass on macOS arm64, including signed zero, NaN
payloads, infinity and tag-colliding doubles. Cross-target IR/object inspection
covers Linux AArch64/x86-64 and Darwin x86-64, not foreign runtime execution.
The first float helper returns through an FP-to-integer move and its caller
moves the value back; the second uses a homogeneous double pair in d0/d1.
These are assembly observations, not a proven cause of the timing changes.

Runtime text grows from 13876284 to 13917584/13917724 bytes; AOT text from
91320 to 91352/91472. The first integer/float helper frames are 256/272 bytes;
the second float frame is 288 bytes. Smaller caller result traffic alone is
not evidence of improved throughput.

[Evidence](results-rejected-register-return-macos-arm64.json) retains both
production diffs and hashes and **80 jobs / 400 samples**, with independently
recomputed medians. No build, test, profile or documentation validator
overlapped timings. Every timed implementation passes 270 independent
reference checks; ten Python harness tests also pass.

The first candidate passes the 554-assertion release continuation wrapper,
472-assertion release scalar-ABI wrapper, and 40-assertion debug native-host
overflow wrapper (plus native assertions, exercising v3/v4 on main and
foreign threads). The FP variant passes 472 scalar-ABI assertions per build
and 592 release continuation assertions, including v4-disabled control.
These are focused tests, not a full selected matrix, full suite or sanitizer
claim. Initial test-only trim syntax and missing-newline errors were fixed.

The reusable `AOTFunctionRefScalarABI` fixture is retained without any v4
dependency. It covers all 12 zero/unary int/float/bool signatures, emitted
native integer/float zero/same-type callees and generic counterparts, full-width integers and tag-colliding
float results, AST/IR/JIT/tiered, O0/O3, helper v1/v2/v3, older callee controls,
stripped source, exception handling, no debug information and split compilation.
Unused-input cross-type and boolean targets are explicitly checked to stay
generic: this fixture has four native callee shapes, not twelve. The final
audit caught these existing admission limits and added this distinction.
The [62-check audit](FOLLOWUP-AUDIT.md#rejected-register-return-audit-2026-10-07)
records final validation and the production rollback.

Next: test removing the redundant middle stack guard only for pure,
non-reentrant callee-v3 entries. Keep the helper-entry and live callee-entry
guards, diagnostic publication, visible variable markers and reader handshake;
validate native overflow boundaries before isolated timing.
## Redundant middle stack guard (2026-10-07)

Retained a small runtime-only optimization: prepared calls to proven pure
callee-v3 bodies omit the duplicate guard in the deferred diagnostic-frame
constructor. The enclosing prepared helper and callee still check their live
stack positions. Deferred context is published before entering the callee;
overflow materializes it before constructing the exception. Older eager guards,
local/closure markers, concurrent-reader synchronization, mutable Program/
debugger/body/readiness checks and cancellation checkpoints are unchanged.
No native ABI, cache layout, serializer or compiler admission rule changed.

Baseline `089aaf452` (its preceding binary is runtime-equivalent; the commit
added only tests/evidence), Apple M4 Pro/macOS arm64. Five independent forks,
three warmups and five 300 ms measurements, randomized implementation/kernel
order. Numbers are medians of fork means in ns/element, including computation,
loop and checksum work: they are not isolated call latency. Every timed
implementation passed 270 independent reference checks before timing.

At 1024 elements, with fresh Java JIT measurements (`run-mgexvmrc`):

|!Kernel|!Before|!After|!Change|!Java
|arithmetic|2.205|2.187|-0.83%|0.881
|direct_call|2.232|2.193|-1.72%|0.897
|indirect_call|9.852|9.644|-2.11%|1.670
|indirect_wide|9.891|9.573|-3.22%|1.680
|indirect_float|10.111|10.116|0.04%|1.628
|indirect_tagged_float|10.161|10.023|-1.36%|1.641
|indirect_branch_float|10.243|10.183|-0.59%|1.651
|indirect_affine_wide|9.784|9.605|-1.84%|1.629

The bounded and wide integer callback ranges separate in this run; all other
Qore before/after ranges overlap. Qore's retained scalar callbacks remain
roughly 5.7–6.2 times Java's measured kernels, so this is not Java-speed parity.

At 4096 elements, the initial four-kernel pilot (`run-w722b3l5`) shows wide
integer 8.867 → 8.493 (-4.21%), float 9.118 → 8.927 (-2.09%), tagged float
9.045 → 8.924 (-1.34%) and branch float 9.234 → 9.114 (-1.30%); all four
ranges separate. The broader repeat (`run-z_14qjji`) shows bounded integer
8.920 → 8.671 (-2.79%), wide 8.937 → 8.655 (-3.15%), affine wide
8.904 → 8.683 (-2.49%) and tagged float 9.190 → 9.009 (-1.97%), with
separated ranges. Ordinary/branch float improve 1.22%/2.03%, but ranges overlap.
Arithmetic, direct-call and polymorphic controls also overlap.

The unfavorable control is retained explicitly: temporary hash slowed
303.753 → 313.806 (+3.31%) in that repeat, with before/after ranges
303.550–315.044 and 303.345–315.450. A seven-fork repeat with 500 ms windows
and a different shuffle (`run-f69t66yk`) again has a slower hash median,
315.119 → 319.498 (+1.39%), ranges 301.975–321.464 and 312.019–344.418.
Its wide callback improves 9.045 → 8.628 (-4.62%), but ranges overlap.
The hash kernel has no native scalar callback in its loop; nevertheless,
code layout effects or a small indirect regression are not ruled out by these
noisy data. Do not interpret overlapping ranges as proof of equivalence.

The eager-callee control (`run-4utlvldx`,
`QORE_DISABLE_AOT_FUNCREF_INLINE_STACK_CHECK=1`) has no median regression:
integer callbacks improve roughly 2.8–3.6%, while floating results overlap;
hash improves 312.953 → 310.052 (-0.93%), also overlapping. Because eager
calls do not execute the removed check, part of the gain can come from compiler
layout/register allocation: these results do not isolate the check's own cost.
The decision retains the repeated integer benefit while recording the unresolved
hash-control risk and nonuniform float benefit for subsequent iterations.

Runtime text shrinks 13876284 → 13875264 bytes (-1020); AOT text remains
91320 bytes and both inspected scalar helper frames remain 272 bytes.
Release/debug selected matrices each pass 39 files: 160 QUnit cases / 15899
assertions plus 11 standalone tests. The subsequent float boundary-test extension
passes saved baseline/release/debug (28 wrapper assertions each plus C++ asserts),
including eager/inline entries, tag-colliding recovery and foreign-thread
registration. Old v2/v3 artifacts match 225/270 records; ten Python harness
tests pass. No full-suite, sanitizer, allocation-failure or Linux runtime claim.

[Sanitized evidence](results-middle-guard-macos-arm64.json) retains the exact
runtime diff/hash, library and executable hashes, settings and all 358 jobs /
1790 samples; medians were recomputed independently. No build, test, profiler or
documentation validator overlapped timing. The
[62-check audit](FOLLOWUP-AUDIT.md#middle-stack-guard-audit-2026-10-07) records
the safety review and final validation. Next: acquire the invocation's stable
thread-data pointer once, keeping mutable fields live, and measure that isolated
change against this version before considering broader calling-convention work.

## Rejected thread-view reuse (2026-10-07)

Baseline `3af96b157`, macOS arm64. Two runtime-only candidates tried to remove
repeated `RuntimeConfig::impl->native_thread_data` loads without snapshotting
mutable Program/debugger/body/readiness/limit state. Neither changed native
helper ABI, cache layout, admission rules, publication, markers or cancellation.
**Both are rejected and their production changes are reverted.**

The broad candidate borrowed ThreadData once at the prepared helper entry and
passed it through internal live-Program, guard, frame and limit-address routines.
The narrow candidate borrowed only the pointer already stored in the active
deferred frame for its final limit-address handoff. All original exported
wrappers and unsupported-target full-guard fallbacks remained.

Five forks, three warmups and five windows per fork; 4096 elements and randomized
implementation/kernel order. Medians of fork means, ns/element, include loop,
scalar computation and checksum work. Pilot windows are 300 ms; the narrow
repeat uses 500 ms and a different shuffle seed.

|!Kernel|!Broad before|!Broad after|!Narrow before|!Narrow after|!Repeat before|!Repeat after
|indirect_wide|8.62273|8.37057|8.72526|8.59379|8.60937|8.53330
|indirect_float|9.02902|9.33989|9.07243|9.06974|9.00518|9.04403
|indirect_tagged_float|8.99755|8.75974|8.99424|9.17311|8.94943|8.99773
|indirect_branch_float|9.09756|9.29815|9.17532|9.13427|9.08245|9.05505

The broad pilot (`run-mfbdjmwq`) improves wide integer 2.92% with separated
ranges, but ordinary/branch-float medians worsen 3.44%/2.20%; float ranges
overlap, so those are not established population regressions. Tagged float
improves 2.64%, but includes an 11.55 ns fork outlier. Runtime text grows
13875264 → 13876424 bytes (+1160), the integer helper frame stays 272 bytes,
and the float frame grows to 288 bytes. Assembly removes the repeated pointer
loads, but that is not sufficient to establish a net optimization.

The narrow pilot (`run-3jlzyij3`) improves wide integer 1.51%, again with
separated ranges; ordinary float is essentially unchanged, tagged float worsens
1.99%, and branch float improves 0.45%. All float ranges overlap, with tagged/
branch outliers of 11.48/13.78 ns. The longer repeat (`run-soj4_t8_`) has changes
of -0.88%, +0.43%, +0.54% and -0.30% respectively; all ranges overlap, and a
branch-float fork reaches 13.35 ns. No samples or outliers were discarded.
Runtime text shrinks 84 bytes and both helper frames stay 272 bytes. The small,
mixed repeat does not justify retaining another internal accessor for this
performance goal; it does not prove that pointer reuse can never help.

Both candidates build in release/debug and pass five focused suites per build:
NativeLeafContext 28, NativeStackRuntimeConfig 3, AOTFunctionRefScalarABI 530,
IRNativeFunctionRefContinuation 554 and AOTPreparedFrameReaders 98 assertions
(1213 total). This includes actual scalar ABI selection, native integer/float
overflow/recovery, foreign-thread registration, callback replacement, mutable
debugger state and concurrent readers. Both changed implementation files pass
standalone release syntax checks; the first broad ad-hoc command lacked
`-DNDEBUG`, produced release-header assertion errors, and passed when corrected.
This is not a full split-build runtime or full-suite/sanitizer validation.

[Evidence](results-rejected-thread-view-macos-arm64.json) retains both exact
production diffs and hashes, candidate/baseline hashes, settings and all 120
jobs / 600 samples with recomputed medians. Every measured implementation passes
270 reference checks. There is no fresh Java measurement or Java-speed claim.
Restored release/debug builds and focused validation are recorded in the
[62-check audit](FOLLOWUP-AUDIT.md#rejected-thread-view-audit-2026-10-07).

Next: investigate reuse of immutable diagnostic-frame metadata or specialize
empty-marker handling for integer results, with the floating path as an explicit
regression gate. Preserve per-call publication/pop reader synchronization and
overflow-time variable/context visibility. Boolean identity/constant native
admission is a separate coverage opportunity, not evidence that the existing
retained-call overhead has reached Java's level.

## Prepared marker deferral and signature reuse (2026-10-07)

Baseline `cb1abb16e`, macOS arm64, M4 Pro. The retained runtime change combines
two independently examined mechanisms:

- Prepared pure integer/bool-result callee-v3 callbacks defer thread-private
  empty local/closure markers until stack-overflow materialization. Floating
  results keep eager markers. A compile-time cleanup policy and three-state
  enum avoid adding a successful floating-call policy branch.
- Prepared-only entry accessors reuse the immutable signature validated at
  cache population. Debug assertions check the invariant; non-prepared accessors
  still validate signatures. Program identity, debugger, normal-body identity
  and readiness remain live checks on every hit.

Diagnostic publication and its reader handshake remain per-call. Entry/callee
stack guards, exception context, cancellation and owned cache lifetime are
unchanged. A failed closure-marker allocation rolls back the local marker;
closure frame counts commit only after successful insertion. No exported native
helper, cache layout or serialized ABI changes. Native tests now reject
wrong-signature references and NOTHING on both cold and populated slots, then
verify valid reuse, for integer/float and eager/inline providers.

### Final combined measurements

Final runtime comparisons use the new `--reuse-aot-for-baseline` option:
one identical AOT executable runs against both compatible libraries. This
holds caller code fixed, not runtime-library layout or host state. Each mode
passes 270 independent reference records before timing. Default paired compiler
builds remain available and are required when measuring compiler changes.

Primary `run-cqfsxcbl`: 1024 elements, five forks, three warmups, five 300 ms
windows per fork, shuffled order, fresh Java retained-call run. Medians of fork
means, ns/element, include loop, scalar computation and checksum work; these
are not isolated call latencies.

|!Kernel|!Before|!After|!Change|!Java retained call
|arithmetic|2.23146|2.23841|+0.31%|0.89334
|direct_call|2.24910|2.25296|+0.17%|0.89300
|indirect_call|9.68725|8.27113|-14.62%|1.67575
|indirect_wide|9.62485|8.11854|-15.65%|1.67812
|indirect_float|10.06777|9.96438|-1.03%|1.65351
|indirect_tagged_float|10.07319|10.00968|-0.63%|1.64608
|indirect_branch_float|10.10630|10.11061|+0.04%|1.66090
|indirect_affine_wide|9.66591|8.16501|-15.53%|1.63987
|polymorphic_call|271.39105|272.16248|+0.28%|2.54271
|temporary_hash|316.39910|319.31831|+0.92%|15.49549

All three integer callback before/after fork ranges separate; all other primary
ranges overlap. The 4096-element repeat `run-8iahrd1r` (same five-fork/window
settings, new shuffle) confirms bounded/wide/affine-wide medians
8.54922 → 7.24827 (-15.22%), 8.57693 → 7.13542 (-16.81%) and
8.62027 → 7.17575 (-16.76%), again with separated ranges. Float changes are
+0.16%, -1.25% and -1.37%, all overlapping.

The repeat's polymorphic/hash medians are slower: 270.34603 → 277.14904
(+2.52%) and 309.66234 → 317.36946 (+2.49%). Their ranges overlap:
268.7497–272.9617 versus 265.7616–277.8918, and
301.7065–319.7815 versus 306.5108–322.9149. A longer seven-fork/500 ms
control repeat `run-k2il8efl` retains the wide benefit
8.57978 → 7.22991 (-15.73%, separated); polymorphic changes
267.31611 → 269.46801 (+0.80%), with ranges 264.0030–270.3298 versus
266.8103–278.7339, and hash 313.93776 → 314.25455 (+0.10%), ranges
299.9178–322.5188 versus 301.3040–320.6589. These controls do not use the
optimized scalar callback path, but an indirect/layout regression is not ruled
out. Overlapping ranges are not equivalence; all unfavorable samples remain.

The final eager-callee-v2 control `run-k7qob947` uses
`QORE_DISABLE_AOT_FUNCREF_INLINE_STACK_CHECK=1`, 4096 elements and seven
500 ms forks. Bounded/wide change 11.33899 → 11.39579 (+0.50%) and
11.34140 → 11.28534 (-0.49%), with overlapping ranges. Float/tagged/branch
improve 2.40%/2.32%/2.46%, with separated ranges. The earlier marker-only
eager integer slowdown below is not reproduced by the combined candidate;
this does not establish neutrality on every legacy workload.

### Rejected and intermediate variants

All stages, including unsuccessful results, are in the evidence. Earlier
paired compilations are not silently relabeled as identical-artifact runs.

|!Variant|!Run(s)|!Decision/evidence
|All-result marker deferral|run-yscj99s7|Wide improves 17.49%; ordinary/branch float regress 36.96%/34.38%, separated; rejected
|Integer markers, shared runtime cleanup|run-01dievdh|Wide improves 16.98%; float/tagged regress 4.78%/4.13%, separated; rejected
|Compile-time cleanup, two booleans|run-qlpj5if5; run-bkz6o6x4|Integer gains repeat; tagged primary median worsens 15.52%, with candidate forks 10.048–11.586; provisional only
|Same-artifact control for that policy|run-bcx00__a|Wide improves 16.57%, separated; floating ranges overlap
|Cross both earlier artifacts with both runtimes|marker-cross-px8j3ho_|Tagged runtime changes -0.16%/+1.95% depending on artifact; ranges overlap, candidate slow forks remain with both artifacts
|Single enum before unused-constructor removal|run-s2k9v876|Wide improves 17.07%, separated; floating changes approximately +0.01%/-0.12%/-0.18%, overlapping
|Marker-only final cleanup|run-nnowuyr7; run-trrpr1v3|Integer modern gains repeat; broader floating/control ranges overlap; held for eager control
|Marker-only eager control and repeat|run-b5nx9_0z; run-mnfsp34d|Bounded/wide about 3% slower; initial bounded ranges separate, longer repeat overlaps; not retained alone
|Signature-accessor increment vs saved marker candidate|run-rlb53iq_; run-2s5fd7un|Eager bounded/wide improve 1.43%/3.25%, overlapping; modern wide worsens 2.43%, overlapping; not a uniform gain

The crossed-artifact experiment does not establish that separate compilation
caused the earlier tagged-float anomaly: candidate slow forks of 11.529 and
10.900 ns occur with both artifacts. The final combined primary/repeat use
identical artifacts and retain all outliers. Likewise, the incremental signature
results compare against the saved marker candidate, not the original baseline;
only the final combined runs establish the reported production improvement.

### Validation, remaining gap and next work

Runtime text decreases 13875264 → 13872648 bytes (-2616); both inspected
integer/float native helper frames remain 272 bytes. Release/debug selected
matrices each pass 39 files: 160 QUnit cases / 15899 assertions plus 11 standalone
tests. Extended native admission tests also pass the saved original and
marker-only baselines. Old v2/v3 artifacts retain byte-identical 225/270 check
records. Eleven Python harness tests, standalone release syntax checks for both
implementation units, strict documentation tables and whitespace checks pass.
No full-suite, sanitizer, allocation-failure injection, Linux runtime or full
split-runtime-build claim.

[Sanitized evidence](results-prepared-markers-macos-arm64.json) retains exact
prototype/final runtime and test diffs/hashes, binary hashes, settings and every
1156 timing jobs / 5780 samples, including the 20-job crossed-artifact diagnostic.
Counts, uniqueness, finite samples and medians were checked independently. No
build, test, profile or documentation validator overlapped timing. The
[62-check audit](FOLLOWUP-AUDIT.md#prepared-markers-and-signatures-audit-2026-10-07)
records the complete checklist and limitations.

Java parity is **not** reached. At 1024 elements Qore integer callbacks take
about 8.1–8.3 ns/element versus Java 1.64–1.68 (roughly 4.8–5.0 times);
floating callbacks remain around 10 ns versus 1.65–1.66. Polymorphic calls
remain about 272 versus 2.54 ns, a much larger gap.

Separate five-second samples of the actual polymorphic workload find an
existing method-dispatch cache, followed by value/type/refcount traffic, TLS
lookups, statement-location changes and parameter setup. The AOT fast-method
branch still installs parameters in TLS before `execCachedFunction`; only the
eligible IR branch uses direct parameters. Counts are qualitative, not additive
time attribution. Next: examine explicit/native AOT method-parameter entry and
boxed/native local lowering on that real path, preserving receiver lifetime,
private defining-class context, overrides, cross-Program and debugger semantics.
For scalar calls, invocation-owned diagnostic metadata and exception-only
saved-state initialization remain hypotheses; do not remove the per-call reader
handshake, native guards or generic fallback merely to match Java's number.
