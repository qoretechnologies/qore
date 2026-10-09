# Qore IR and AOT performance findings

Copyright (c) 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT.

**Historical IR-mode caveat (2026-10-08):** before upstream `5243c8aef`,
`--exec-mode=ir` could compile and promote hot code to JIT. Those runs record
the requested mode, not a verified pure interpreter. A saved-runtime probe
confirms this for the polymorphic workload. AOT/Java comparisons are unaffected;
see the upstream integration/engine-provenance section below.

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

## AOT method implicit-context reuse (2026-10-08)

Baseline `870bdce9d`, macOS arm64, M4 Pro. The resolved dot-eval fast method
path, including polymorphic cache hits, previously instantiated synthetic
`self` and `argv` on every call despite existing compiler context-use metadata.
The retained change reads immutable flags from an acquire-published, already-
ready AOT context. With the same caller/execution Program and no attached
debugger, unused self setup can be skipped; unused argv setup is skipped only
without excess arguments.

The object and defining-class context, receiver type information, parameter
binding, variable/diagnostic frames, parse policy/location, return filtering
and cleanup remain. No context is eagerly materialized for this optimization.
Excess arguments keep their original list ownership and destruction order.
IR/JIT and cross-execution-Program calls retain their previous setup. No new
method-entry ABI or serialized format is needed for this step.

### Measurements

All comparisons use one identical AOT executable against the two compatible
libraries, randomized implementation/kernel/fork order, three warmups and five
measurement windows per fork. Every timed implementation first passes 270
independent reference records. Numbers are medians of fork means, ns/element,
including receiver selection/assignment, computation, loop and checksum work,
not isolated call latency.

Primary `run-m9xb9fns`: 1024 elements, five forks, 300 ms windows and fresh
Java measurements.

|!Kernel|!Before|!After|!Change|!Java
|polymorphic_call|273.08598|233.96860|-14.32%|2.51939
|method_call|2.45769|2.45605|-0.07%|0.87444
|dispatch_control|2.35622|2.32913|-1.15%|0.96814
|indirect_wide|8.05092|8.14915|1.22%|1.66695
|indirect_float|10.03246|10.03749|0.05%|1.63277
|indirect_tagged_float|10.04774|10.03474|-0.13%|1.63983
|indirect_branch_float|10.03145|10.04693|0.15%|1.64761
|temporary_hash|320.23280|304.20266|-5.01%|15.42502

Polymorphic ranges separate: 268.1989–281.5103 before versus
231.8401–240.4188 after. Hash ranges also separate in this run; all other
before/after ranges overlap. The final-method arithmetic workload can inline
and must not be presented as retained polymorphic-call performance.

The 4096-element pilot `run-slbko259` improves polymorphic calls
273.84242 → 237.64623 (-13.22%), ranges 263.8096–282.0575 versus
232.1913–239.5590. Its wide/float controls change +1.50%/+0.28% and hash
-4.30%, with overlapping ranges. A longer seven-fork/500 ms repeat
`run-_hg9d0h4` confirms 270.92341 → 235.01551 (-13.25%), ranges
268.0059–278.5568 versus 230.6194–238.1677.

The unfavorable scalar control is retained: wide callbacks have slower medians
in all three enabled runs. In the longer repeat they change
7.06756 → 7.09981 (+0.46%), ranges 7.0345–7.4502 versus 6.9940–7.2245.
This is smaller than the pilot/primary +1.50%/+1.22%, but overlapping ranges
are not proof of equivalence. Hash changes 306.89975 → 305.71600 (-0.39%),
ranges 301.6733–310.7114 versus 303.3012–312.5889. Its earlier larger benefits
are not isolated evidence about method setup; runtime layout and host variation
remain uncontrolled.

The diagnostic control `run-cg8u1ufx` sets
`QORE_DISABLE_IR_CONTEXT_ELISION=1` for compilation and both runtime modes
(4096 elements, seven forks, 500 ms). The polymorphic benefit disappears:
269.06657 → 271.22539 (+0.80%), ranges 266.5296–278.6610 versus
267.9540–279.4644. Wide changes -0.77%, also overlapping. The flag affects
other context-elision paths too, so this supports the mechanism without
measuring its exact isolated cost.

### Rejected first step and audit correction

An unused-self-only pilot `run-86jsu9wh` changed polymorphic calls
266.84121 → 263.06060 (-1.42%), ranges 265.0387–268.1120 versus
248.7820–268.3404. Wide/hash medians worsened 1.12%/2.30%; float improved
0.16%, all overlapping. It was not an established standalone performance win.

Thread-safety review also rejected its extra reads of
`UserVariantBase::usesSelf()/usesArgv()`: lazy IR/debug materialization can
rewrite these fields. The final implementation instead reads immutable flags
from an already-published AOT context and keeps the process disable control.
An intermediate combined mutable-flag build was never benchmarked or retained.
All measured variants and samples, including the self-only fast fork, remain
in [sanitized evidence](results-method-context-macos-arm64.json).

### Validation and next work

Release/debug qore/qcc builds pass. The expanded selected matrix passes
48 files per build: 31 QUnit wrappers with 166 cases / 15935 assertions, plus
17 standalone tests. Seven focused suites also pass per build. Extended
`AOTDotEvalOverload.qtest` compares source, qmod, disabled-cache and disabled-
context behavior: mixed receivers, explicit self, private members, closures,
recursion, nested receivers, argv restoration, excess-object destruction and
exception recovery. It passes the saved baseline as well. Initial missing
variadic declarations in the new fixture were rejected by the baseline;
corrected declarations pass before and after.

Old v2/v3 AOT artifacts retain byte-identical 225/270 reference records.
Eleven Python harness tests, standalone release syntax, strict Qore tables,
evidence integrity and whitespace checks pass. Runtime text grows
13872648 → 13873068 bytes (+420). No full-suite, sanitizer, injected
allocation-failure, Linux runtime or full split-runtime-build claim.

The [62-check audit](FOLLOWUP-AUDIT.md#aot-method-context-audit-2026-10-08)
records every item. Evidence includes the exact runtime/test diffs and hashes,
library/executable hashes, settings and all 310 jobs / 1550 samples; medians,
counts, uniqueness and finite values were independently checked. No build,
test, profile or documentation validator overlapped timings.

Java parity remains distant: primary polymorphic execution is about 234 versus
2.52 ns/element (roughly 93 times), while wide scalar callbacks are 8.15 versus
1.67. Compiler inspection confirms all three polymorphic target bodies already
declare unused self/argv, but the caller still uses boxed receiver assignment,
reference/reload tracking, argument boxing and
`qore_rt_dot_eval_object_method_direct_aot`. This inspection is of pre-native-
optimization IR, not a claim that every displayed store survives LLVM.

Next: inspect why the resolved base method misses the existing guarded direct
entry and evaluate explicit AOT method parameters/boxed-local lowering. Scalar
callbacks separately need a measured versioned reusable-frame/fast-entry
experiment; removing caller-frame construction requires preserving per-call
publication, reader completion, live guards, overflow and fallback semantics.

## Assigned receiver classes and guarded AOT calls (2026-10-08)

Baseline `132d53ad3`, macOS arm64, M4 Pro. Compiler diagnostics found that the
polymorphic caller failed just one guarded fast-entry condition: its receiver
class had been widened to generic `object`. Assignment/non-NOTHING, callee,
signature, argument-binding, context-independence and native-entry checks passed.
The exact boxed-local analysis already retains the enforced declared class in a
separate map; normal and exception-routed call lowering now consult that proof.
No receiver-validity or exact-class guard is removed.

### Correctness correction before retention

The first admission-only prototype improved the pilot from 235.82354 to
176.92699 ns/element (-24.97%), but the audit found a correctness failure.
Qore permits a subclass to change the base method's return contract. A probe
returned `36:null` in AST and baseline AOT but `36:0` with the prototype:
the existing speculative merge unboxed the subclass's NOTHING as an integer.
That prototype and its performance claim were rejected, despite its then-passing
selected regression matrix. Its wide-callback control also worsened 2.55% with
separated ranges. Both its pilot and disabled-control samples remain in the
evidence; they are not measurements of the retained code.

The corrected implementation keeps subclass fallback results boxed and boxes
native fast-hit results before merging. It does not apply the base method's
non-NOTHING or local-effect summary across an unknown subclass fallback. Native
integer/float ownership, boolean representation and exception routing remain.
Final/non-speculative targets retain their previous return convention. The
expanded regression covers NOTHING and owned-string overrides, captured-local
mutation, wide integers, tagged floats, booleans and throwing overrides, in
addition to absent/deleted receivers and missing required arguments.

### Corrected measurements

All runs use separate before/after AOT compilation against the compatible saved
and candidate runtimes, not a shared executable. Each implementation passes 270
independent reference records before timing. Jobs run sequentially in randomized
implementation/kernel/fork order, with three warmups and five measurement windows
per fork. Values are medians of independent fork means, ns/element including
receiver selection/assignment, computation, loop and checksum work—not isolated
call latency. No build, test, profile or validator overlaps timing.

Primary `run-46n9due0`: 1024 elements, five forks, 300 ms windows, fresh Java.

|!Kernel|!Before|!After|!Change|!Java
|polymorphic_call|237.44773|196.31836|-17.32%|2.53664
|method_call|2.46234|2.47240|0.41%|0.89591
|dispatch_control|2.35537|2.36144|0.26%|0.98407
|indirect_wide|8.13353|8.13235|-0.01%|1.67551
|indirect_float|10.06452|9.98807|-0.76%|1.64831
|indirect_tagged_float|10.03941|10.03628|-0.03%|1.64442
|indirect_branch_float|10.12531|10.18003|0.54%|1.65929
|temporary_hash|309.96777|311.93200|0.63%|15.55374

Polymorphic ranges separate: 234.71978–238.82010 before versus
194.50403–198.00686 after. Every other before/after range overlaps. Unfavorable
control medians and all outliers are retained; overlap is not proof of equivalence.
The earlier corrected 4096-element pilot `run-4aja5scf` changes
232.47944 → 193.87794 (-16.60%), also separated (231.76085–235.46894 versus
192.99123–195.20590). Its control ranges all overlap.

The corrected diagnostic control `run-4c264f9v` applies
`QORE_DISABLE_AOT_OBJECT_METHOD_FAST_ENTRY=1` to both compilation and runtime
(4096 elements, seven forks, 500 ms). The polymorphic benefit disappears:
235.01543 → 237.13949 (+0.90%), with overlapping ranges. Wide callbacks change
7.20890 → 7.19322 (-0.22%), also overlapping. This broad disable supports the
guarded-entry mechanism; it does not isolate individual costs or control layout.

Native disassembly confirms partial devirtualization: the exact base-class hit
imports its arithmetic, while the two derived cases retain runtime dispatch.
Java still uses `DONT_INLINE` on all three targets, so this is not a comparison
of three retained Qore calls against three retained Java calls. The primary
polymorphic workload remains roughly 77 times Java; wide callbacks remain about
4.9 times Java. Neither parity nor a universal performance improvement is claimed.

The longer repeat `run-l1c4klht` uses 4096 elements, seven forks and 500 ms
windows. It confirms the polymorphic benefit; all control ranges overlap.

|!Kernel|!Before|!After|!Change|!Before range|!After range
|polymorphic_call|234.92371|195.96261|-16.58%|233.38585–236.63413|192.14767–196.75914
|indirect_wide|7.18281|7.17496|-0.11%|6.95810–7.28870|7.09742–7.23714
|indirect_float|8.94952|9.03583|0.96%|8.92286–9.02658|8.98632–10.00923
|temporary_hash|309.07476|310.36051|0.42%|303.22095–313.19131|305.25508–331.78548

The high float/hash after-forks remain included. Float and hash medians are
unfavorable in this repeat; these are not hidden or presented as improvements.
The initial rejected wide-callback slowdown did not repeat in the corrected
primary or long repeat. Separate inspection of that rejected pilot found the
same wide-callback instruction sequence after address/literal-pool normalization,
but different placement. This suggests layout sensitivity, not isolated proof of
its cause or equivalence.

### Validation, audit and next work

Release/debug qore/qcc builds and the corrected selected matrix pass 51 files
per build: 33 QUnit wrappers with 168 cases / 16041 assertions plus 18 standalone
tests. The new regression has 98 assertions and proves guarded LLVM entry
selection for six normal/invoke/integer/float/boolean call sites. AST, IR, JIT,
tiered, -O0/-O3, source-stripped AOT, native EH and disabled-method-entry controls
agree. Expanded semantic checks also pass the saved baseline's AST and AOT.
The baseline fails the intended new guarded-code-generation assertion.

Old v2/v3 AOT artifacts retain byte-identical 225/270 reference records. Eleven
Python harness tests, standalone release syntax for `QoreIRToLLVM.cpp`, strict
Qore tables, source/binary hashes and whitespace checks pass. Runtime text grows
13873068 → 13873380 bytes (+312). No full-suite, sanitizer, allocation-failure
injection, Linux runtime or full split-runtime-build claim. Native artifacts
must be recompiled to acquire the changed code generation.

The [full 62-check audit](FOLLOWUP-AUDIT.md#assigned-method-fast-entry-audit-2026-10-08)
records the failure, correction and final review. [Sanitized evidence](results-assigned-method-macos-arm64.json)
retains exact compiler/test diffs, hashes, settings and all six runs: 352 jobs /
1760 samples. Mode/kernel/fork completeness, uniqueness, finite values, startup
sample counts and medians were independently checked. The admission-only
prototype's two runs are explicitly rejected; the corrected implementation has
264 jobs / 1320 samples across its pilot, disable control, primary and repeat.

The goal remains active. The actual native polymorphic loop still performs
receiver assignment filtering, runtime variable synchronization and ownership /
reload tracking; two of three receiver classes still enter generic method
runtime dispatch with TLS parameters. Next, investigate whether already-enforced
receiver types can safely remove repeated assignment filtering, retaining weak
reference evaluation, assignment errors and ownership. Separately, a versioned
native method-parameter entry could reuse resolved polymorphic targets; the
existing four-slot cache already avoids repeated name resolution. Any such
entry must preserve actual override return contracts, live Program/debugger/body
checks and diagnostic/stack/cancellation semantics.

## Plain-function access context (2026-10-08)

Baseline `c3ccb678e`, macOS arm64 M4 Pro. Investigation of repeated receiver
assignment checks found that a declared class type is not a sufficient proof
for removing the check: a parameter may be accepted with its caller's private
inheritance access, while assignment to the same type in a plain function must
fail. Blind check elision was rejected before measurement.

That regression also exposed a pre-existing AOT bug. The saved runtime's
prelinked plain-function path inherited its caller's object/class context.
AST, IR, JIT and tiered execution rejected the illegal assignment; saved AOT
accepted both assignments and the function return. Disabling prelinked calls
restored the expected error; disabling context elision did not.

The fix scopes the existing `ObjectSubstitutionHelper` around body execution
and return filtering in three general plain-function fast helpers. Parameter
binding and parameter cleanup keep their caller-context ordering. The helper
clears/restores both thread and TLS RuntimeConfig object/class state, including
exceptional exits. Methods, closures, native scalar adapters and ABI are unchanged.

### Measured correctness cost

These are runtime-only comparisons: each run uses one identical AOT executable
with both compatible libraries. All four modes first pass 270 independent
reference records. Jobs run sequentially in shuffled mode/kernel/fork order,
without overlapping builds, tests, profiles or evidence validation. Values are
medians of independent fork means, ns/element including loop/computation/checksum,
not isolated call latency. No Java comparison was repeated for this fix.

Primary `run-0wjj6czp`: 4096 elements, five forks, three warmups, five 300 ms
measurement windows per fork.

|!Mode|!Kernel|!Before|!After|!Change
|IR|direct_call|360.28096|358.44484|-0.51%
|IR|indirect_wide|331.28973|342.15799|3.28%
|IR|polymorphic_call|407.09655|408.10680|0.25%
|IR|temporary_hash|669.79424|676.92590|1.06%
|AOT|direct_call|1.31753|1.32339|0.45%
|AOT|indirect_wide|7.23129|7.20295|-0.39%
|AOT|polymorphic_call|196.53903|194.95396|-0.81%
|AOT|temporary_hash|311.12989|313.21635|0.67%

Only the IR callback ranges separate: 319.04029–335.89904 before versus
339.31964–343.69993 after. All other before/after ranges overlap; overlap does
not establish equivalence. All unfavorable medians and outliers are retained.

Longer `run-u7uytxq3`: 4096 elements, seven forks, three warmups and five
500 ms windows. `QORE_DISABLE_AOT_FUNCREF_NATIVE_ENTRY=1` disables AOT adapter
registration, exercising the corrected general callback helper. The IR mode
provides a longer repeat; this flag affects AOT compilation, not IR lowering.

|!Mode|!Before|!After|!Change|!Before range|!After range
|IR callback repeat|329.85275|338.82050|2.72%|328.14754–332.12842|331.91999–343.55842
|AOT general callback|183.96689|195.59143|6.32%|182.47911–186.21590|194.23454–199.24751

The IR repeat ranges narrowly overlap; the generic AOT ranges separate. This
is retained as a correctness fix with a measured cost, not as an optimization.
Future general-call improvements must preserve the corrected access boundary.

### Validation and next work

Release/debug qore/qcc builds and 55 selected files per build pass: 37 QUnit
wrappers, 202 cases / 16475 assertions, plus 18 standalone tests. The new wrapper
has 44 assertions, covering AST/IR/JIT/tiered, stripped-source -O0/-O3 AOT,
native EH, direct/invoke/callback calls, return filtering, caller restoration
and disabled-prelink/context/callback controls. The saved baseline fails at
AOT -O0 after 11 assertions, as expected. Access, class and cast suites pass.

Old v2/v3 AOT artifacts retain byte-identical 225/270 reference records. Eleven
Python harness tests, standalone runtime syntax, strict tables and whitespace
checks pass. Runtime text grows 13873380 → 13873616 bytes (+236). No full-suite,
sanitizer, allocation-failure injection, Linux runtime or full split-runtime
build claim. The workstation is shared with desktop/system applications.

The [62-check audit](FOLLOWUP-AUDIT.md#plain-function-context-audit-2026-10-08)
and [sanitized evidence](results-plain-function-context-macos-arm64.json) retain
exact diffs/hashes, all 108 jobs / 540 samples, settings, test counts and costs.
The separate receiver-initializer experiment is not part of this fix. Next,
measure its checked-assignment consolidation against this corrected baseline;
then investigate native parameters on polymorphic cache hits, reusing existing
fast-entry contracts where sound. The broader performance goal remains active.

## Checked class-local initialization (2026-10-08)

Baseline `114eb51e3`, including the plain-function access-context fix above,
macOS arm64 M4 Pro. Required-class AOT body-local initializers now use the
existing checked/eval-weak assignment directly, then reload the stored value.
This removes a separate coercion temporary and its extra reference cleanup;
it does not remove type/access checks or weak-reference normalization. The
native cache stays NOTHING until the assignment succeeds. Reassignments,
value-producing stores, optional/reference/complex types, IR-only locals and
non-normal assignment modes keep their previous lowering. No new runtime
helper, metadata cache or ABI is introduced.

### Measurements

All runs compile separate before/after AOT artifacts against their compatible
libraries, pass 270 independent reference records per implementation, and run
sequentially in shuffled mode/kernel/fork order. Each fork has three warmups
and five measurement windows. Values are medians of independent fork means,
ns/element including receiver selection/assignment, computation, loop and
checksum—not isolated call latency. No build, test, profile, disassembly or
evidence validator overlaps timing. All samples and unfavorable medians remain.

Pilot `run-zmx1uzee`: 4096 elements, five forks, 300 ms windows.

|!Kernel|!Before|!After|!Change|!Before range|!After range
|polymorphic_call|193.17560|180.33761|-6.65%|190.91942–196.54221|178.48663–182.83127
|indirect_wide|7.24994|7.28873|0.54%|7.13772–7.28196|6.90491–7.35551
|temporary_hash|316.13354|307.25925|-2.81%|304.63364–319.41634|299.61162–317.64752
|direct_call|1.28327|1.28175|-0.12%|1.27844–1.30640|1.26521–1.29904

Primary/repeat `run-k2ptife0`: 1024 elements, five forks, 500 ms windows and
fresh Java measurements.

|!Kernel|!Before|!After|!Change|!Java
|polymorphic_call|194.71968|183.27666|-5.88%|2.54110
|indirect_wide|8.19036|8.22909|0.47%|1.68414
|temporary_hash|314.25230|313.69379|-0.18%|15.43691

The primary polymorphic ranges separate: 192.41724–196.25488 before versus
182.58325–185.55041 after. All other before/after ranges overlap in both runs;
overlap does not establish equivalence. In particular, the small wide-callback
slowdowns remain reported, and the pilot's favorable hash/direct medians are
not separate optimization claims.

Diagnostic `run-ywyg4emx`: 4096 elements, seven forks, 500 ms windows with
`QORE_DISABLE_AOT_RUNTIME_CHECKED_CLASS_STORE=1` during compilation/execution.

|!Kernel|!Before|!After|!Change|!Before range|!After range
|polymorphic_call|193.25011|194.14440|0.46%|189.65654–196.50286|192.21379–197.69428
|indirect_wide|7.14267|7.12129|-0.30%|7.08488–7.21334|7.02772–7.25922

The benefit disappears with the lowering disabled, and both native binaries
retain separate coercion followed by unchecked assignment. Default pilot native
code instead has checked assignment plus reload, with 386 versus 430 instructions
in the inspected polymorphic function. This supports the mechanism without
isolating each instruction's cost or controlling all code-layout effects.

The primary polymorphic workload remains about 72.1 times Java; wide callbacks
remain about 4.89 times Java. Qore still inlines the exact base-class arithmetic
case while its two derived cases enter general method runtime dispatch. Java
uses DONT_INLINE on all three targets. This is neither Java parity nor a ratio
of three retained Qore calls to three retained Java calls.

### Validation, audit and next work

Release/debug qore/qcc builds and 56 selected files per build pass: 38 QUnit
wrappers with 203 cases / 16551 assertions, plus 18 standalone tests. The new
wrapper has 76 assertions and checks normal/invoke code generation, -O0/-O3,
stripped-source AOT, native EH, disabled-lowering and runtime-context controls.
Source AST/IR/JIT/tiered agree on private access, missing/wrong/removed values,
weak references, captured/reference mutation, owned alias cleanup, failed
reassignment and escaped-capture lifetime. The release wrapper was repeated
after the capture test was expanded; the debug matrix uses that expanded fixture.

The corrected saved baseline passes expanded AST/AOT semantics and fails the
intended new codegen assertion after 13 assertions. Old v2/v3 AOT artifacts
retain byte-identical 225/270 reference records. Eleven harness tests, standalone
LLVM compiler syntax, strict documentation tables, hashes and whitespace checks
pass. Runtime text grows 13873616 → 13873796 bytes (+180). No full-suite,
sanitizer, allocation-failure injection, Linux runtime or full split-runtime
build claim. Existing native artifacts require recompilation for this change.

The [full 62-check audit](FOLLOWUP-AUDIT.md#checked-class-initializer-audit-2026-10-08)
and [sanitized evidence](results-checked-class-store-macos-arm64.json) retain
exact diffs/hashes, all 113 jobs / 565 samples, settings, source/binary inspection
and validation counts. Mode/kernel/fork completeness, uniqueness, finite positive
values, startup counts and independently recomputed medians were checked.

Next: reach typed native method entries from polymorphic cache hits, avoiding
repeated TLS parameter binding where the actual resolved body permits it. Reuse
existing fast-entry contracts and keep actual override returns, Program/debugger/
body/readiness guards, receiver validity, ownership, stack publication/readers
and cancellation. This work is not implemented by the initializer change;
the broader performance goal remains active.

## Native scalar polymorphic entries (2026-10-08)

Retained result: the final AOT polymorphic workload improves **26.04%**,
183.07909 → 135.40062 ns/element, with separated fork ranges. The earlier
4096-element pilot improves 28.22%; it predates the final idempotent binding
load and debugger fallback correction and is reported separately. Java parity
is **not** achieved.

Baseline: `b9040b7e6d10bdde522ae3fceeb39f600e6d928a` on the same macOS arm64
workstation. Separate before/after artifacts were compiled against preserved
baseline and candidate runtimes; no AOT executable reuse was used. Raw samples,
exact source/test diffs, hashes and validation counts are in
[the sanitized evidence](results-native-method-entry-macos-arm64.json).

### Change and preserved semantics

A pure, non-throwing, context-independent `int(int)` instance method publishes
its existing typed companion when its normal AOT entry first executes. The
versioned binding is context-owned and atomically published, checks normal-body
identity, and keeps hidden native symbols alive without dynamic lookup. An
already-bound load avoids repeated CAS on generic entry calls. The context
field is appended; the existing function table and serialized format do not change.
Old artifacts remain on the fallback.

On a polymorphic-cache hit, the native path avoids TLS parameter binding but
retains the resolved override's exact signature and integer input checks,
receiver validity, access/epoch key, live normal-body/readiness/PC-map checks,
matching caller/object/execution/context Programs, and debugger exclusion.
Self/argv users, generic/imported/specialized/capturing bodies, changed or optional
returns and potentially throwing expressions are excluded. Object/class RAII,
explicit RuntimeConfig frame setup, local/closure markers, parse/source context,
reader publication/waiting, native stack checks and ordinary boxed return
ownership remain. The existing bounded scalar proof is shared with function refs.

The audit also found and fixed a pre-existing debugger fallback bug: warmed
resolved fast-method calls bypassed debugger-aware AST/lazy-debug-IR execution.
The baseline regression reports zero method entry/exit events after attachment;
the corrected runtime reports all 16 pairs and restores normal calls after detach.

### Final repeat

1024 elements, five forks, three warmups and five 500 ms measurement windows per
fork; current/baseline IR and AOT plus fresh Java: 75 jobs / 375 samples.
All five modes passed 270 independent reference checks before timing. No build,
test, profile, disassembly or evidence validation overlapped timing.

|!Mode and kernel|!Before ns/element|!After ns/element|!Change|!Before / after fork-mean ranges
|qore-ir polymorphic_call|407.81036|409.82739|0.49%|404.58180–413.96168 / 400.17125–417.90815
|qore-aot polymorphic_call|183.07909|135.40062|-26.04%|181.28356–184.86307 / 132.19223–136.26415
|qore-ir indirect_wide|340.64802|343.14110|0.73%|336.03192–344.91094 / 331.22746–354.64220
|qore-aot indirect_wide|8.41224|8.21040|-2.40%|8.28679–8.46256 / 7.98045–8.25596
|qore-ir temporary_hash|672.34192|657.65298|-2.18%|661.66472–682.79488 / 654.48595–671.53243
|qore-aot temporary_hash|329.89560|304.86293|-7.59%|308.89460–334.00731 / 300.04838–310.88154

IR ranges overlap; no IR speedup or clear regression is claimed. The AOT hash
ranges also overlap. Wide-callback ranges are separated, but that control also
improves with the native method entry disabled, so its change is not attributed
to this optimization. Fork ranges are descriptive, not confidence intervals.

Fresh Java medians are 2.51959 ns/element for polymorphic calls, 1.66986 for wide
callbacks and 15.38452 for temporary hashes. Current Qore AOT is therefore about
53.74× Java on the polymorphic workload and 4.92× on wide callbacks. Qore's
base-class arithmetic case can inline while the two derived targets retain
native calls; Java prevents inlining all three target bodies. These are complete
workload comparisons, not equivalent isolated call instructions.

### Pilot and disabled-entry attribution control

The earlier pilot used 4096 elements, five forks, three warmups and five 300 ms
windows: 30 jobs / 150 samples. Polymorphic time was 179.53007 → 128.85878
ns/element (−28.22%), with ranges 178.03810–181.89702 vs 127.71571–129.64106.
Wide callbacks were 7.70401 → 7.17309 (−6.89%); direct calls were
1.28492 → 1.29217 (+0.56%, overlapping ranges). Only the target improvement
is attributed, subject to the final repeat and control.

The final control set `QORE_DISABLE_AOT_NATIVE_METHOD_ENTRY=1` during compilation
and execution: 4096 elements, seven forks, three warmups and five 500 ms windows,
28 jobs / 140 samples.

|!Kernel|!Before ns/element|!After ns/element|!Change|!Before / after fork-mean ranges
|polymorphic_call|182.16129|182.64991|+0.27%|179.32106–184.20223 / 181.28181–184.15179
|indirect_wide|7.47651|7.19906|−3.71%|7.38058–7.49570 / 7.06470–7.26225

The target benefit disappears with overlapping ranges. The unrelated wide
control still improves, confirming why its change is not counted as a native
method-entry benefit. Both favorable and unfavorable control medians are retained.

### Validation and next work

Release and debug builds each pass 57 selected files: 39 QUnit wrappers,
206 cases / 16801 assertions, plus 18 standalone tests. The new wrapper contributes
three cases / 250 assertions across AST/IR/JIT/tiered with synchronous thresholds,
stripped AOT -O0/-O3, native EH, forced split codegen, no DWARF, and disable controls.
Its fixtures cover actual override returns, wrong/missing/excess arguments,
wide integers, self/private/argv fallback, foreign Programs, concurrent first
registration, stack readers and warmed debugger attachment/detachment.

Both standalone C++ syntax checks pass (79/76 existing warnings), 11 harness tests
pass, and older v2/v3 AOT executables retain byte-identical 225/270 reference
records. Final native disassembly shows a 14-instruction derived scalar entry
using an integer register, retaining `qore_rt_check_stack`, and returning a native
integer. The primary binary references the new binding helper; the disabled
binary does not. Runtime text grows 13873796 → 13878420 bytes (+4624).

All 133 jobs / 665 samples and startup samples were checked for complete unique
mode/kernel/fork coverage, finite positive values and independently recomputed
medians. The full 62-item audit records 21 Pass / 41 N/A. This is not a full
repository, sanitizer/race-detector, Linux, allocation-failure or complete
split-runtime-build result.

Next: move immutable resolved-method admission facts into the existing PIC entry,
keeping mutable guards live; then evaluate a typed caller continuation and an
explicit-RuntimeConfig native method entry. Parameter TLS is only one part of
the remaining type/access/PIC, boxed-value, context/frame and native stack-entry
cost. The broader performance goal remains active.

## Cached native-method admission (2026-10-08)

Retained result: AOT polymorphic calls improve **16.46%**, 133.44322 →
111.48285 ns/element, with separated fork ranges. A 4096-element pilot improves
16.62%; disabling only admission caching removes the benefit (−0.29%, overlapping
ranges). Java parity is **not** achieved: fresh Java measures 2.54098 ns/element,
about **43.87×** below current Qore on this whole workload.

Baseline: `929e08f9081a17efcc5bdcd20d09d6704a3f00f8`, macOS arm64,
`feature/performance`. This runtime-only change reuses an identical AOT executable
against both libraries, which already support the native-method v1 binding.
[Sanitized evidence](results-method-admission-macos-arm64.json) retains exact
source/test diffs, hashes, all samples and validation details.

### Implementation and guards

When the normal method body has already published its native scalar companion,
the existing PIC constructor records immutable actual-variant admission: static
fast-method eligibility, exact unary integer signature, execution Program and
defining class. Three borrowed pointer fields are initialized before release
publication. Prepared hits avoid repeated RTTI, signature scans and Program lookup.
No new allocation, compiler change or serialized ABI change is introduced.

Every hit still validates the full PIC key, actual integer argument, receiver
validity, body identity, context/PC-map readiness, matching Programs and debugger
state. Object/class substitution, frame publication/reader waiting, local/closure
markers, stack checks and boxed ownership are unchanged. Entries populated before
native publication keep ordinary admission, which can discover the companion
after debugger detach; no later prepared-entry upgrade is claimed.

### Longer repeat

1024 elements, five forks, three warmups and five 500 ms windows per fork.
All five modes pass 270 reference checks. The 75 jobs / 375 samples include
fresh Java and before/after IR and AOT. No builds, tests, profiles, disassembly
or evidence validation overlap timing.

|!Mode and kernel|!Before ns/element|!After ns/element|!Change|!Before / after fork-mean ranges
|qore-ir polymorphic_call|411.58656|409.88764|-0.41%|408.37964–413.74252 / 403.56890–418.62596
|qore-aot polymorphic_call|133.44322|111.48285|-16.46%|133.31260–135.15065 / 111.19630–112.50794
|qore-ir indirect_wide|340.20363|337.04226|-0.93%|335.61935–342.07432 / 332.96729–339.01690
|qore-aot indirect_wide|8.16456|8.09861|-0.81%|8.07493–10.92417 / 8.01006–11.14199
|qore-ir temporary_hash|652.55831|660.73619|1.25%|649.89338–662.24418 / 649.88672–668.51273
|qore-aot temporary_hash|319.11384|311.53810|-2.37%|313.84033–327.02860 / 300.84424–331.75521

IR and unrelated control ranges overlap; no gains are attributed to them.
The wide AOT control retains its 11.14199 ns candidate and 10.92417 ns baseline
fork outliers. No samples are dropped. Fresh Java wide/hash medians are 1.67468
and 15.52540 ns/element. Qore can inline the base-class arithmetic case but
retains the derived calls; Java disables inlining for all three bodies. Neither
these times nor their ratio measure an isolated call instruction.

### Pilot and attribution control

Pilot: 4096 elements, five forks, three warmups, five 300 ms windows;
30 jobs / 150 samples. Only the polymorphic target gain is attributed.

|!Mode and kernel|!Before ns/element|!After ns/element|!Change|!Before / after fork-mean ranges
|qore-aot polymorphic_call|130.44051|108.75662|-16.62%|129.43173–131.96343 / 107.43775–110.00604
|qore-aot indirect_wide|7.09401|7.26158|2.36%|6.93916–7.25208 / 7.11244–7.41953
|qore-aot direct_call|1.27587|1.25505|-1.63%|1.26963–1.28569 / 1.24118–1.26687

The final control sets `QORE_DISABLE_AOT_NATIVE_METHOD_ADMISSION_CACHE=1`,
retaining the native scalar companion. It uses 4096 elements, seven forks,
three warmups and five 500 ms windows: 28 jobs / 140 samples.

|!Mode and kernel|!Before ns/element|!After ns/element|!Change|!Before / after fork-mean ranges
|qore-aot polymorphic_call|132.51692|132.13502|-0.29%|129.88591–134.18644 / 131.53298–133.31561
|qore-aot indirect_wide|7.21275|7.18369|-0.40%|7.06262–7.28628 / 7.07585–7.28666

Both control ranges overlap, supporting the target attribution. Startup samples
are retained separately; filesystem caches are uncontrolled.

### Validation and next experiments

Release/debug builds each pass 57 selected files: 39 QUnit wrappers,
206 cases / 16835 assertions and 18 standalones. The native wrapper has three
cases / 284 assertions, including the admission-disable control. It was rerun
in both builds after adding receiver deletion at a warmed native call site.
The debugger fixture also tests first invocation while attached, then late native
publication after detach. Existing wide-integer ownership, foreign Program,
native-EH/split/no-DWARF and concurrent first-call/reader contracts remain covered.

Standalone runtime syntax passes with 79 existing warnings; 11 harness tests pass;
old v2/v3 AOT outputs remain byte-identical for 225/270 reference records. All
133 jobs / 665 samples have validated unique/complete combinations, positive
values, startup counts and recomputed medians. Runtime text grows 13878420 →
13879800 bytes (+1380). The full 62-check audit is recorded in
[FOLLOWUP-AUDIT.md](FOLLOWUP-AUDIT.md#cached-native-method-admission-audit-2026-10-08).
No full-suite, sanitizer/race-detector, Linux, allocation-failure or complete
split-runtime validation is claimed.

A separate 10-second profile after all timing runs samples 7570 main-thread
stacks. Distinct caller sites account for 2542 checked receiver-assignment samples,
1705 method-dispatch samples, 739 receiver-load samples, 485 local-clear samples,
464 temporary receiver-decref samples and 426 reload-if-stale samples. These are
coarse sampled site counts, not precise component latencies. Method object/class
substitution and native frame setup/teardown remain visible below dispatch.

Next: measure explicit-RuntimeConfig object/class save/restore to avoid repeated
TLS lookup without omitting context updates. Then address the larger local
assignment/load/ownership costs using guarded compiler proofs that preserve
class access, weak-reference normalization, receiver lifetime and fallback effects.
The active goal requires both call-path and surrounding execution improvements.

## Explicit native method object context (2026-10-08)

Retained result: polymorphic AOT improves **3.01%**, 112.26800 → 108.89255
ns/element, with separated fork ranges. The earlier pilot improves 3.93%.
Disabling native method entries removes the benefit: +0.57%, overlapping ranges.
Java parity is **not** achieved: fresh Java is 2.53536 ns/element, about **42.95×**
below current Qore on this whole workload.

Baseline: `85d37a49eb11aef5edcc20f7af9617796d135eb8` on
`feature/performance`, same macOS arm64 workstation. Runtime-only before/after
pairs reuse an identical AOT executable. [Sanitized evidence](results-native-method-context-macos-arm64.json)
retains the exact diff, library/source hashes, all samples and validation rows.

### Change and invariants

The pure native method path already has a live `RuntimeConfig`. Its new internal
`RuntimeConfigNativeObjectHelper` borrows that context and its thread view,
saving and restoring the live object/class fields and snapshot object/class
fields independently. It avoids rediscovering those stores through TLS; it does
not omit any context update or assume the two old stores were equal. The generic
`ObjectSubstitutionHelper` remains unchanged.

The admitted context is already initialized and belongs to the current thread
and invocation. Public getters/setters retain their behavior. No new allocation
occurs on that admitted path, and no object-reference ownership changes. Body,
readiness, Program, receiver, integer-kind and debugger guards, native stack
checks and frame-reader publication/waiting are unchanged. The inner diagnostic
frame finishes before context restoration. No public RuntimeConfig layout,
serialized AOT format or native entry ABI change is introduced.

### Longer repeat

1024 elements, five forks, three warmups and five 500 ms windows per fork:
75 jobs / 375 samples across baseline/current IR and AOT plus fresh Java. Every
mode passes 270 reference checks. No build, test, profile, disassembly or evidence
validation overlaps timing.

|!Mode and kernel|!Before ns/element|!After ns/element|!Change|!Before / after fork-mean ranges
|qore-ir polymorphic_call|408.76435|411.79117|0.74%|402.40673–419.77220 / 405.77431–414.40125
|qore-aot polymorphic_call|112.26800|108.89255|-3.01%|111.05948–113.45067 / 108.51424–109.08676
|qore-ir indirect_wide|336.98597|338.15913|0.35%|336.44165–339.87070 / 335.69437–342.39728
|qore-aot indirect_wide|8.11881|8.13349|0.18%|8.00380–8.14211 / 8.01098–8.21593
|qore-ir temporary_hash|657.89081|664.78867|1.05%|653.11573–659.65427 / 656.23537–669.27445
|qore-aot temporary_hash|314.82364|314.31578|-0.16%|310.15588–323.61362 / 308.26267–319.77186

All IR and unrelated wide/hash ranges overlap. Neither improvement nor clear
regression is attributed there. Fresh Java wide and hash medians are 1.67852
and 15.54032 ns/element. Qore can inline the base polymorphic body while retaining
the derived calls; Java prevents inlining all three bodies. This is not an
identical isolated retained-call comparison.

### Pilot and disabled-entry control

Pilot: 4096 elements, five forks, three warmups and five 300 ms windows;
30 jobs / 150 samples.

|!Mode and kernel|!Before ns/element|!After ns/element|!Change|!Before / after fork-mean ranges
|qore-aot polymorphic_call|111.23761|106.86153|-3.93%|108.03980–111.75979 / 105.67734–107.24463
|qore-aot indirect_wide|7.27759|7.07798|-2.74%|7.09867–7.34225 / 6.97417–7.29837
|qore-aot direct_call|1.31486|1.31107|-0.29%|1.31025–1.31628 / 1.30601–1.31716

The final control sets `QORE_DISABLE_AOT_NATIVE_METHOD_ENTRY=1` at compilation
and runtime. Disabling only the admission cache would not exclude this change:
both prepared and ordinary native admission use the new context helper. The
control uses 4096 elements, seven forks, three warmups and five 500 ms windows;
28 jobs / 140 samples.

|!Mode and kernel|!Before ns/element|!After ns/element|!Change|!Before / after fork-mean ranges
|qore-aot polymorphic_call|181.99065|183.02620|0.57%|179.76265–184.73262 / 179.75372–186.88206
|qore-aot indirect_wide|7.22115|7.23683|0.22%|7.13827–7.35664 / 7.07408–7.36265

Both control ranges overlap. All samples, including unfavorable ones, are
retained. Fork ranges are descriptive, not confidence intervals; filesystem
caches for startup samples are uncontrolled.

### Validation and next work

Release/debug builds each pass 57 selected files: 39 QUnit wrappers,
206 cases / 16835 assertions plus 18 standalones. The native method wrapper
contributes three cases / 284 assertions, including warmed receiver deletion,
late publication after debugger detach, wide ownership, foreign Programs and
concurrent readers/first calls. An initial eight-file subset also passes per
build: eight QUnit cases / 533 assertions plus two standalones.

Standalone runtime/thread syntax passes with 79/65 existing warnings; 11 harness
tests pass, and old v2/v3 AOT artifacts reproduce all 225/270 reference records
byte-for-byte. All 133 jobs / 665 samples, startup samples, complete/unique job
combinations and recomputed medians are validated.

Disassembly shows the native dispatch no longer calls the generic substitution
constructor/destructor: the explicit helper is inlined. Normal out-of-line
definitions remain for split builds. The dispatch stack allocation remains
416 bytes. Runtime text grows 13879800 → 13881348 bytes (+1548).
The [62-check audit](FOLLOWUP-AUDIT.md#explicit-native-method-object-context-audit-2026-10-08)
records scope and limits; no full suite, sanitizer/race detector, Linux,
allocation-failure injection or complete split-runtime build is claimed.

Next: the previous separate profile identifies receiver-local assignment/load/
ownership as larger costs than method dispatch. Investigate a fused checked
class-local assignment/result path to avoid a redundant local reload, while
retaining class access, type and weak-reference checks, cleanup ordering and
owned cache lifetime across aliases. The performance goal remains active.

## Owned checked class-local result (2026-10-08)

Retained result: polymorphic AOT improves **3.72%**, 107.09243 → 103.10729
ns/element, with separated fork ranges. The pilot improves 4.64%. Two disabled
controls have overlapping ranges, with the repeat near flat. Fresh Java is
2.52757 ns/element: Qore remains **40.79×** slower on this whole workload, not
at Java retained-call parity.

Baseline: `d698514a016a3c5b82a5831de77bdbce300e5f3c` on
`feature/performance`, same macOS arm64 workstation.
[Sanitized evidence](results-class-store-result-macos-arm64.json) retains the
exact runtime/compiler and test diffs, hashes, all 161 jobs / 805 samples, test
rows and all 62 audit checks. Before and after use separately compiled AOT
artifacts: the new helper symbols do not exist in the saved baseline.

### Change and invariants

Eligible noncaptured, non-self required-class initializers return the owned
actual stored value from their checked assignment directly into the existing
native cache/reload tracker. Only an empty plain runtime slot uses this
shortcut. It has no replaced value or recursive root whose cleanup could
reenter Qore and change the value. Nonfresh slots and defensive captured-slot
fallback keep the ordinary post-cleanup reload.

Class/access checks, weak normalization and all assignment errors remain.
The owned snapshot survives lvalue/weak-input cleanup; the compiler checks
errors before transferring its reference into the existing tracker and
deferred-release chain. Reference targets, special self lookup, nonreloadable
caches, reassignments and value-producing stores are excluded. The native-EH
wrapper releases any result before throwing. No runtime ownership, call guard,
cancellation checkpoint or existing serialized ABI is removed.

This removes a repeated local lookup/evaluation, not the assignment itself.
The separate earlier profile motivating it sampled receiver-local operations
more often than method dispatch; those sample counts were not isolated latency.

### Longer repeat

1024 elements, five forks, three warmups and five 500 ms measured windows.
75 jobs / 375 samples include before/after IR and AOT plus fresh Java. Every mode
passes 270 independent reference checks. No build, test, profile, disassembly
or evidence validation overlaps timing.

|!Mode and kernel|!Before ns/element|!After ns/element|!Change|!Before / after fork-mean ranges
|qore-ir polymorphic_call|407.41618|412.21871|1.18%|401.11834–408.36012 / 406.83740–420.36672
|qore-ir indirect_wide|342.95866|345.16775|0.64%|336.39869–343.85018 / 335.48819–347.50377
|qore-ir temporary_hash|657.03415|666.22882|1.40%|656.04265–665.12676 / 655.17357–675.95388
|qore-aot polymorphic_call|107.09243|103.10729|-3.72%|105.48737–110.46176 / 102.39946–104.64754
|qore-aot indirect_wide|8.15872|8.19979|0.50%|8.05349–8.23566 / 8.02945–8.34138
|qore-aot temporary_hash|319.84364|308.85647|-3.44%|304.90313–337.50640 / 300.81660–312.36892

All IR and unrelated wide/hash ranges overlap; no improvement is attributed
there. Fresh Java wide/hash medians are 1.67594 / 15.52544 ns/element. Qore can
inline the base polymorphic method while retaining derived calls; Java prevents
inlining all three bodies. These are equivalent computations, not identical
runtime contracts or isolated call instructions.

### Pilot and disabled controls

Pilot: 4096 elements, five forks, three warmups and five 300 ms windows;
30 jobs / 150 samples.

|!Mode and kernel|!Before ns/element|!After ns/element|!Change|!Before / after fork-mean ranges
|qore-aot polymorphic_call|107.16124|102.18518|-4.64%|106.24384–108.49531 / 101.07611–103.74105
|qore-aot indirect_wide|7.13816|7.15826|0.28%|7.02531–7.15510 / 6.98205–7.23139
|qore-aot direct_call|1.31556|1.31970|0.31%|1.31050–1.32796 / 1.31407–1.32002

Both controls set `QORE_DISABLE_AOT_CLASS_STORE_RESULT=1` at compilation and
runtime, restoring the separate reload. Each uses 4096 elements, seven forks,
three warmups and five 500 ms windows; 28 jobs / 140 samples. The baseline does
not recognize the new control and retains its original lowering.

First disabled control:

|!Mode and kernel|!Before ns/element|!After ns/element|!Change|!Before / after fork-mean ranges
|qore-aot polymorphic_call|105.35023|107.78449|2.31%|103.00641–106.43602 / 105.70640–112.78808
|qore-aot indirect_wide|7.05492|7.22292|2.38%|6.92417–7.50194 / 7.15401–7.31777

Both medians drifted upward by about 2.3% with overlapping ranges. Rather than
discarding that unfavorable run, a new shuffle seed repeated the same control:

|!Mode and kernel|!Before ns/element|!After ns/element|!Change|!Before / after fork-mean ranges
|qore-aot polymorphic_call|107.24579|106.47765|-0.72%|106.30350–108.21440 / 104.93213–107.26010
|qore-aot indirect_wide|7.20131|7.18356|-0.25%|6.99791–7.29401 / 7.08477–7.30078

Both repeated-control ranges overlap and their medians are near flat. The
enabled target benefit repeats with separated ranges; it does not repeat with
the optimization disabled. All samples remain in the evidence, including the
initial unfavorable control. Fork ranges are descriptive, not confidence
intervals; shared-host scheduling/frequency/thermal state and startup filesystem
caches are uncontrolled.

### Validation and next work

Release/debug builds each pass 57 selected files: 39 QUnit wrappers,
206 cases / 16919 assertions plus 18 standalone files. The expanded class-local
wrapper contributes 160 assertions per build, comparing AST with IR/JIT/tiered
and O0/O3 AOT, native EH, both compiler controls, runtime context control and
split codegen. It checks actual helper selection and captured fallback as well
as private inheritance, weak/deleted values, alias ownership and error cleanup.

Standalone runtime/compiler syntax passes with 79/76 existing warnings;
11 Python harness tests pass. Old AOT v2/v3 artifacts retain byte-identical
225/270-record outputs. Runtime/compiler diff SHA-256:
`c9e645acb299a04dd38eb5d4c64d0d392f9bef31e139aac7681bb15043041068`.
Release/debug library SHA-256:
`4c05a7e34825439dbebc6ca7d633c51dd26892a1c92297ad14cdb99f7feb6c2f` /
`6d7978146d5a944d1c0c23ae8e08a3f850b58764cbed6aebbcae4e777ff82877`.
Release text grows 13881348 → 13882644 bytes (+1296).

No full repository suite, sanitizer/race detector, Linux, allocation-failure
injection or complete split-runtime build is claimed. The next investigation
is native-only receiver-local storage with an explicit destructor/exception
ordering proof, or a fresh plain-slot assignment specialization if that proof
cannot be established. Prepared native method frames remain another target.
The performance goal is active; this is an incremental workload improvement.

## Fresh runtime-backed class-local stores (2026-10-08)

Baseline: `e494737b8f7b2bb53875b984627962834ed5b003`. The retained
specialization reduces the polymorphic AOT workload by **10.67%** in the longer
repeat, **102.88905 → 91.90602 ns/element**. Fresh Java is **2.53880**, leaving
a **36.20×** whole-workload gap. Java parity is not achieved.

### Change and rejected alternative

`LocalVar::tryAssignInitialClassValue()` bypasses generic lvalue setup only
for a compiler-admitted required-class initializer whose current-thread slot
is unassigned, ordinary and noncaptured. Self, substituted, finalized, static
and assigned slots retain the ordinary path. The same `acceptAssignment()`
type/access validator and `assignAssume()` real-reference accounting remain.
Weak normalization and owned-result cleanup are unchanged. Locals stay on the
runtime stack, preserving scope/exception destructor ordering. No compiler,
serialized-format or public ABI change is made.

A preceding native-only class-local prototype was **rejected before timing**:
it changed destructor order on three exception paths. Its exact rejected diff
and failures are retained in the evidence, not in the implementation. The new
13-scenario AST/AOT fixture covers normal/mixed/scope/loop/return lifetimes,
aliases, throwing destructors/reassignment and on-exit handlers. It also exposes
a **pre-existing JIT mismatch** in the saved baseline: mixed unwind destroys
`middle,last,first` rather than AST/AOT's `last,middle,first`. This independent
issue is not fixed here; the new wrapper intentionally claims AST/AOT coverage,
not all-mode lifetime equivalence.

### Measurements

Runtime-only comparisons reuse an identical AOT executable against both
libraries. All modes pass 270 independent reference checks before timing.
Jobs are sequential and shuffled, with no build, test, profile, disassembly or
evidence validation overlapping timing. Values are medians of process means,
ns/element including selection, assignment, computation, loop and checksum—not
isolated call instructions.

Primary `run-yz192xir`: 1024 elements, five forks, three warmups, five 500 ms
windows. 75 jobs / 375 samples include fresh Java and before/after IR/AOT.

|!Mode and kernel|!Before|!After|!Change|!Before / after fork-mean ranges
|AOT polymorphic_call|102.88905|91.90602|-10.67%|101.56864–104.60869 / 90.97598–92.68506
|AOT indirect_wide|8.21347|8.17261|-0.50%|8.00562–8.27631 / 8.14883–8.26050
|AOT direct_call|2.25633|2.27053|0.63%|2.23493–2.26227 / 2.25157–2.29303
|IR polymorphic_call|409.44292|409.94753|0.12%|403.29697–411.37786 / 405.20910–418.25825
|IR indirect_wide|340.36033|337.92075|-0.72%|337.70893–345.94340 / 332.06291–349.47250
|IR direct_call|357.27242|359.51034|0.63%|350.08008–362.39795 / 354.35716–374.92457

Only the target ranges separate. Java wide/direct medians are 1.67990/0.88916,
giving AOT ratios 4.86×/2.55×. Direct functions are inlineable, so that ratio is
not retained-call overhead. The polymorphic base body can be inlined by Qore,
whereas Java prevents inlining all three method bodies. Computations are
equivalent, runtime contracts and generated call patterns are not identical.

Pilot `run-t3awldof`: 4096 elements, five forks, three warmups, five 300 ms
windows; 30 jobs / 150 samples.

|!Kernel|!Before|!After|!Change|!Before / after fork-mean ranges
|polymorphic_call|99.68054|89.57882|-10.13%|99.32854–100.73662 / 88.74092–89.97119
|indirect_wide|7.16348|7.24238|1.10%|6.98678–7.23864 / 7.17427–7.28059
|direct_call|1.24867|1.27015|1.72%|1.23844–1.26495 / 1.26741–1.27976

The pilot direct-control ranges separate unfavorably. The longer primary and
disabled direct controls instead overlap (+0.63%/+0.07%); this does not prove
equivalence or exclude layout effects. All unfavorable samples remain.

Disabled control `run-ppjfq5v9`: `QORE_DISABLE_AOT_FRESH_CLASS_LOCAL_STORE=1`
at runtime, 4096 elements, seven forks, three warmups, five 500 ms windows;
42 jobs / 210 samples. The flag is sampled once, on first helper use.

|!Kernel|!Before|!After|!Change|!Before / after fork-mean ranges
|polymorphic_call|102.63029|100.53304|-2.04%|101.00801–103.68853 / 99.89636–102.46482
|indirect_wide|7.22083|7.25461|0.47%|7.12882–7.29012 / 7.02883–7.33159
|direct_call|1.31619|1.31709|0.07%|1.30748–1.32268 / 1.31329–1.31949

All disabled ranges overlap. The enabled target gain is larger and repeats
with separated ranges, supporting the specialization; do not attribute the
entire measured 10.67% to it or subtract medians to estimate isolated helper
cost. Fork ranges are descriptive, not confidence intervals. Shared-host
scheduling, frequency, thermal state and startup filesystem caches are uncontrolled.

### Validation and next work

Release/debug each pass 59 selected files: 41 QUnit wrappers, 215 cases /
16983 assertions, plus 18 standalones. Focused class-assignment and new lifetime
wrappers contribute 180 and 35 assertions per build, including O0/O3, native EH,
split codegen and the disabled runtime path. Existing coverage checks access,
weak/deleted values, alias/capture ownership, cancellation, stack limits,
debugger attachment, Program guards and concurrent frame readers.

Standalone runtime syntax passes with 79 existing warnings; 11 Python harness
tests pass. Old AOT v2/v3 outputs retain byte-identical 225/270 records.
Disassembly confirms the fresh branch bypasses generic lvalue setup while
calling the checked type validator and normal slot assignment.

Runtime diff SHA-256:
`d731abd0a919a7e71e3dc4fce71969789fabd797bf0f25a65a0438f82525dbb7`.
Release/debug library SHA-256:
`b1fa14387a9cf7e645cbd665d239210e8afb7c2ac1f985f8def875a280edb36b` /
`6ef42769f7ea188378582f143a0dd303cfd3791f4fe9aa5c2f4e1cc7770bf6a8`.
Release text grows 13882644 → 13883608 bytes (+964).

No full suite, sanitizer/race detector, Linux, allocation-failure injection or
complete split-runtime build is claimed. Next: profile the remaining native
method frame/context work and investigate deferring pure-method thread-private
setup while retaining frame publication, reader synchronization and stack-error
materialization. Native-only class locals remain excluded pending a unified
declaration-order cleanup design. The performance goal remains active.

### Refreshed call-category comparison

At the user's request, `run-dbk_klgd` refreshes the other existing call kernels
against Java: 1024 elements, five forks, three warmups and five 500 ms windows,
80 jobs / 400 samples, 270 reference checks per mode. This is a current AOT/Java
comparison, not an additional before/after optimization claim.

|!Kernel|!Qore AOT ns/element|!Java JIT ns/element|!AOT/Java
|method_call|2.50782|0.88512|2.83×
|indirect_call|8.19611|1.65925|4.94×
|indirect_float|10.07082|1.63656|6.15×
|indirect_tagged_float|10.09860|1.63383|6.18×
|indirect_branch_float|10.12481|1.64859|6.14×
|indirect_affine|8.15667|1.62656|5.01×
|indirect_affine_wide|8.05807|1.62793|4.95×
|indirect_branch_int|8.16437|1.66063|4.92×

Together with the primary run, the current snapshot puts integer callbacks at
4.86–5.01× Java, float callbacks at 6.14–6.18×, inlineable direct functions/methods
at 2.55×/2.83×, and the polymorphic workload at 36.20×. The direct/method ratios
do not measure retained calls. General callback fallback was about 195.59
ns/element in an earlier forced-fallback run, without a fresh Java comparison;
it is not represented by these scalar fast-path numbers. General captures,
constructors, builtins, complex arguments and cross-Program AOT calls have no
current matched Java comparison in this snapshot.

[Complete evidence](results-fresh-class-store-macos-arm64.json) retains all
227 jobs / 1135 samples across the pilot, primary, disabled control and fresh
call census, plus exact diffs/hashes, rejected-prototype evidence and all 62
audit checks (20 Pass/42 N/A). No timing overlapped validation or other experiments.

## Rejected deferred method frames and inline stack pops (2026-10-08)

Baseline `2ba74f4f6`, same macOS arm64 host. A prepared native method frame
deferred pure-body thread-private markers/context while retaining eager
object/class substitution, stack publication and reader synchronization. Stack
exceptions and wide-result allocation materialized the full context. A second
variant inlined stack-pop publication and moved only the rare reader wait to
a cold helper. Both runtime prototypes were removed; neither is a retained win.

All timings use 4096 elements, three warmups and five windows per fork, with
identical AOT executables against each pair of libraries. Values are medians
of fork means, ns/element, not isolated call latency. Pilot windows are 300 ms;
the seven-fork repeat uses 500 ms. No timing overlapped builds, tests, profiles,
disassembly or evidence validation.

|!Experiment|!Kernel|!Before|!After|!Change
|Deferred frame pilot, 5 forks|polymorphic_call|89.46827|89.12539|-0.38%
|Deferred frame repeat, 7 forks|polymorphic_call|91.02040|90.43374|-0.64%
|Deferred frame repeat, 7 forks|indirect_wide|7.18986|7.19222|0.03%
|Deferred frame repeat, 7 forks|indirect_float|9.04453|9.13060|0.95%
|Inline pop over deferred frame, 5 forks|polymorphic_call|90.44226|88.89771|-1.71%
|Inline pop over deferred frame, 5 forks|indirect_wide|7.28363|7.58726|4.17%
|Inline pop over deferred frame, 5 forks|indirect_float|9.10725|9.06435|-0.47%
|Inline pop over deferred frame, 5 forks|direct_call|1.33530|1.34049|0.39%

Every before/after fork range overlaps. The deferred repeat includes unfavorable
wide/float outliers (9.29/10.50 ns); none were discarded. The small target changes
and mixed controls do not establish a worthwhile optimization. Profiles and
assembly confirm both intended paths were selected, so this is not a missed
lowering. Shared-host load, frequency and thermal state remain uncontrolled;
fork ranges are descriptive, not confidence intervals.

Both variants build in release/debug and pass native-method and native-leaf
context regressions (316/40 assertions per build); the inline-pop variant also
passes concurrent stack readers (7 assertions per build). Retained coverage
adds wide-result caller-class restoration and warmed method stack-limit sweeps,
including foreign-thread calls. After removing the obsolete disable controls,
these tests pass against the original saved baseline (284/32 assertions).

[Evidence](results-rejected-method-frame-macos-arm64.json) retains exact rejected
diffs/hashes, all 122 jobs / 610 samples, recomputed medians and qualifications.
No full suite, sanitizer, Linux or Java-parity claim is made. The next target is
the larger checked class-local assignment hotspot: specialize successful class
acceptance without removing inheritance/access checks or changing diagnostics.

## Checked class acceptance and prepared-entry alignment (2026-10-08)

Baseline: `2ba74f4f65ee184391841eb77bcaefb053863d00`. The retained runtime
specializes successful fresh required-class acceptance and requests 16-byte
alignment on the twelve prepared scalar callback v3 entrypoints. Final
polymorphic AOT improves **3.77%**, **91.87791 → 88.40956 ns/element**.
Fresh Java is **2.53907**, leaving a **34.82×** whole-workload gap. Java parity
is not achieved.

### Contract and layout tradeoff

The fresh, ordinary, runtime-backed class local retains its owned value holder,
weak normalization, real-reference accounting and declaration-order cleanup.
Only a single unmapped `QTS_CLASS` acceptance spec with a strong object can use
the positive predicate. It shares the ordinary raw/parameterized-object gate
and inheritance/private-access validator. Failed predicates call the original
`acceptAssignment()` for conversion or diagnostics. No class/access decision
is cached; optional, union, mapped and parameterized target constraints retain
generic acceptance. Generic objects may still satisfy a plain base class or a
`legacy_raw_accepts` target.

`QORE_DISABLE_AOT_CLASS_ACCEPT_FAST_PATH=1` retains generic acceptance dispatch;
the presence flag is sampled on first helper use. The compiler, helper ABI,
stack checks, Program/debugger guards, frame publication and reader synchronization
are unchanged. All comparisons with AOT reuse one identical executable per pair.

The unaligned candidate repeatedly regressed wide callbacks. A 128-byte-aligned
alternative recovered that AOT control but increased text by 95,700 bytes over
the unaligned candidate and showed an unfavorable IR-wide signal. The retained
16-byte candidate costs 9,284 bytes over unaligned, or **9,640 bytes over HEAD**
(13,883,608 → 13,893,248). The larger growth than twelve entry paddings reflects
broader generated layout effects; their exact cause is not established.
No corresponding benefit is claimed on other platforms.

### Final comparison

`run-lzfbwmjt`: 1024 elements, five forks, three warmups and five 500 ms windows;
100 jobs / 500 samples across baseline/current IR and AOT plus fresh Java.
Every mode passes 270 independent reference checks. Jobs are sequential and
shuffled; no build, test, profile, disassembly or evidence validation overlaps timing.
Values are medians of fork means, including loop, receiver selection, assignment,
computation and checksum, not isolated call instructions.

|!Mode and kernel|!Before|!After|!Change|!Before / after fork-mean ranges
|AOT polymorphic_call|91.87791|88.40956|-3.77%|90.75425–92.68528 / 87.70481–92.39431
|AOT indirect_wide|8.19413|8.12048|-0.90%|8.12768–8.24447 / 8.09186–8.21653
|AOT indirect_float|10.03025|10.02601|-0.04%|9.91918–10.09567 / 9.87475–10.03706
|AOT direct_call|2.26906|2.25009|-0.84%|2.25229–2.28232 / 2.24727–2.26772
|IR polymorphic_call|412.82238|411.04999|-0.43%|403.00963–414.30176 / 408.99696–414.45292
|IR indirect_wide|344.69701|342.65629|-0.59%|334.18264–347.82098 / 333.26298–352.23769
|IR indirect_float|295.74118|295.23630|-0.17%|292.36642–298.46339 / 289.37822–296.61927
|IR direct_call|359.08206|359.05582|-0.01%|358.15679–360.83170 / 353.90043–363.17048

All final ranges overlap, including the target because one unfavorable after
fork reaches 92.39431 ns/element. No sample is dropped. Earlier target gains
across the unaligned and 128-byte variants support the specialization, but the
entire final 3.77% is not attributed to one helper. The final controls do not
establish gains or equivalence; shared-host scheduling, frequency and thermal
state remain uncontrolled, and fork ranges are not confidence intervals.

Fresh Java wide/float/direct medians are 1.67421 / 1.64661 / 0.88674, giving
AOT ratios **4.85× / 6.09× / 2.54×**. Direct calls permit inlining, and Qore
can inline the base polymorphic body while Java retains all three methods.
The computations match, but runtime contracts and retained-call patterns differ.

### Earlier variants and controls

All experiments remain in the evidence, including rejected alternatives and
outliers. The isolated 128-byte pilot compares against the saved unaligned
class candidate, not HEAD.

|!Experiment|!Key observations
|Unaligned pilot, 4096 elements, 5 forks, 300 ms|Polymorphic -4.78%; wide +3.80%
|Unaligned primary, 1024 elements, 5 forks, 500 ms|Polymorphic -3.03%; wide +3.36%, separated ranges
|Unaligned disabled control, 4096 elements, 7 forks, 500 ms|Polymorphic -0.07%; wide +2.93%, separated ranges
|128-byte isolated pilot, 4096 elements, 5 forks, 300 ms|Bounded integer callback -6.18%; branch integer -4.26%; wide -1.58%, overlap
|128-byte primary, 1024 elements, 5 forks, 500 ms|Polymorphic 91.90444 → 87.48944 (-4.80%); wide +0.20%, overlap
|128-byte disabled/IR control, 1024 elements, 5 forks, 500 ms|Disabled AOT polymorphic +2.01%; IR wide +2.71%, separated ranges
|128-byte IR repeat, 1024 elements, 7 forks, 500 ms|IR wide +1.28%, overlap; direct +0.07%, overlap
|16-byte final primary, 1024 elements, 5 forks, 500 ms|Polymorphic -3.77%; wide AOT -0.90% and IR -0.59%, both overlap

The wide AOT regression persists when the unaligned class predicate is disabled,
supporting an effect outside the positive shortcut, such as layout, without
proving its exact cause. The 128-byte IR signal weakened on repeat but was not
dismissed; the smaller alignment was selected and remeasured. No performance
claim is based on subtraction of unrelated-run medians.

### Validation and audit

The unaligned class candidate passes release/debug matrices of 60 selected
files per build: 42 QUnit wrappers, 235 cases / 17,319 assertions, plus 18
standalones. Both aligned variants pass nine focused wrappers per build:
12 cases / 4,111 assertions. This is focused validation of alignment, not a
claim that the whole 60-file matrix was rerun on each aligned build.

Expanded class fixtures cover positive/negative private access, raw generic
acceptance, a generic derived object assigned to its plain base, and exclusion
of parameterized targets. Existing cases retain weak/deleted objects, captures,
alias ownership, lifetime order and error recovery. Retained method tests add
wide-result private-context restoration and warmed method stack-limit sweeps
on main/foreign threads. Guard, debugger, cancellation, concurrency, scalar ABI
and reader tests remain green.

Final standalone runtime syntax passes with 79 existing warnings; unchanged
class implementation syntax passes with 80. Eleven Python harness tests pass.
Old AOT v2/v3 executables retain byte-identical 225/270 records. Disassembly
confirms the positive class helper and generic fallback, and all twelve final
prepared entry addresses are 16-byte aligned.

The `audit-changes` skill was applied to all retained changes, including the
tests/evidence from the rejected method-frame experiments above. All 62 checks
are reported individually with evidence: **21 Pass, 41 N/A, 0 Fail**. Review
added the missing immediate exception-sink assertion after the first new test
argument push; baseline and candidate tests pass with that assertion.

|!Audit group|!Status|!Evidence
|Module, QPP, DataProvider, factory and JAR registration|N/A|No such implementation or registration changes
|Test structure and dependencies|Pass|Modern executable fixtures; repository paths; bundled QUnit/FsUtil
|Sandboxing and cancellation|Pass|No production I/O; bounded helpers; sweep polls every 10 calls and stack walk every 100 entries
|Exception, thread, type and input safety|Pass|Owned holders, exact admission tags, unchanged live access checks and reader protocol
|Performance, documentation, security and correctness|Pass|All raw trials retained; explicit layout costs/limits; focused and broad coverage scoped above

[Complete evidence](results-class-acceptance-macos-arm64.json) retains all
**500 jobs / 2,500 samples**, exact variant diffs/library hashes, integrity
checks and the complete audit. Rejected method-frame evidence remains separate.
Final runtime diff SHA-256 is
`6cb27db13d24bb1c3bf8826e68360de8a219489c37d6c70ea5ad4a329f197f96`;
release/debug library hashes are
`f35473d120a7650d85c03433f1dc3674c8315f0c1c1e01416a4873e6350f818c` /
`5d9c36e363109cf3a83c70fc5ba38c7b6542a4b6388ad850ae464181e54c0f9e`.

No full suite, sanitizer/race detector, Linux, allocation-failure injection or
full split-runtime claim is made. The goal remains active. Next: extend the
existing bounded IR integer-leaf evaluator to composed scalar expressions,
then investigate immutable caller-owned diagnostic frames and unboxed method
continuations without weakening observability or cleanup.

## Composed IR integer leaves and closure call census (2026-10-08)

Baseline: `16367e39957d2f84d840d976d54d8c9f0bdd9bd8`. Bounded composed
integer leaf admission reduces the IR direct-call workload **60.27%**, from
359.33210 to 142.77692 ns/element. AOT noncapturing closures improve **57.64%**,
107.43783 to 45.51332. Both before/after fork ranges separate. These are not
isolated native-call latencies or evidence of Java parity.

### Admission and observability

The existing integer evaluator now admits single-block compositions, bitwise
AND/OR/XOR and three-way comparison. Compact single-operation summaries remain
preferred. Admission retains exact integer signatures, at most two parameters,
eight forward-only blocks, eight IR-only body locals, 64 instructions and value
IDs below 128. Captures, references, effects and backward branches are excluded.
Invalid operands and exceptional modulo abandon scratch evaluation before any
visible effect. Unsigned intermediates preserve full-width arithmetic wrapping.

This also changes AOT closure compilation: an admitted leaf need not emit a
generic native closure body. Its native caller still calls the runtime helper,
which can execute the bounded evaluator. The matched Java functional-object
targets explicitly prevent inlining. Captured-variable synchronization and
ownership differ from Java field access; the new capture benchmark reads an
integer without mutating it, with construction outside timing.

Validation exposed a pre-existing debugger problem: retained AOT closures did
not report entry/exit events after attachment. Direct/leaf closure shortcuts
now reject attached debuggers, and leaf execution requires the same Program.
QORD20 preserves entry file/source labels, offset, lines and lexical parse
options in a 40-byte payload. Both closure readers attach immutable,
variant-owned entry metadata before publication. Older supported formats remain
readable, but need recompilation to supply missing entry metadata; older runtimes
reject QORD20. The prepared scalar callback helper ABI is unchanged.

Review also corrected a unity-build-hidden `QoreParseOptions` header dependency
and routed new source labels through the prefix-map writer. A regression checks
all 48 debugger entries from two physical roots with spaces: the saved pre-map
candidate fails with an unmapped closure entry, while release/debug final builds
pass. The Python harness now explicitly selects macOS child-process libraries;
inherited `DYLD_*` variables are not reliable through system Python.

### Final paired measurements

`run-2adfuvk6`: 1024 elements, five forks, three warmups and five 500 ms windows;
175 jobs / 875 samples. Baseline/current IR and separately compiled AOT artifacts
plus fresh Java each pass 300 independent reference checks. Jobs run sequentially
in shuffled order, without overlapping builds, tests, profiles or evidence checks.
Values are medians of fork means in ns/element, including loop, dispatch,
computation and checksum. The expanded harness passes two additional closure
arguments per batch; do not subtract earlier-harness medians from these results.

|!Kernel|!IR before|!IR after|!AOT before|!AOT after|!Java JIT
|direct_call|359.33210|142.77692|2.35964|2.37417|0.88624
|indirect_wide|340.60394|340.16457|8.20412|8.22985|1.67833
|indirect_float|295.97036|298.93464|10.18199|10.16668|1.64416
|polymorphic_call|407.74319|404.83213|88.11007|87.36582|2.53789
|arithmetic|93.62041|95.59812|2.33129|2.34765|0.88658
|closure_call|368.44074|370.11959|107.43783|45.51332|1.66773
|closure_capture|403.01069|408.15201|126.80795|126.24921|1.75026

IR direct-call fork means range from 351.43068–363.81890 before and
138.85556–144.59000 after. AOT noncapturing closure ranges are
106.90270–108.00984 and 45.10210–46.00707. All other before/after ranges overlap;
their small movements are not claimed as improvements or proof of equivalence.
In particular, IR arithmetic is **2.11% slower** by median. Its earlier pilot
was 4.09% slower; this consistent direction is retained as an unresolved modest
cost, not dismissed because the ranges overlap. Shared-host scheduling,
frequency and thermal state remain uncontrolled. Fork ranges are not confidence
intervals, and startup samples do not establish cold-cache behavior.

The fresh AOT/Java workload ratios are **4.90×** for wide integer callbacks,
**6.18×** for floating callbacks, **34.42×** for polymorphic dispatch,
**27.29×** for noncapturing closures and **72.13×** for captured closures.
Direct calls allow inlining; Qore's polymorphic base body can inline while
Java retains all three targets. The new closure numbers must not be represented
as general capture-mutation or isolated native-call costs.

### Attribution control and pilot

`run-haxft31z` sets `QORE_DISABLE_IR_NATIVE_INT_PROGRAM_INLINE=1` for both
compilation and execution, with the same five-fork/500 ms settings and a new
shuffle: 60 jobs / 300 samples, 300 reference checks per mode.

|!Disabled-path workload|!Before|!After|!Change
|IR direct_call|356.26954|362.63930|+1.79%
|AOT direct_call|2.34098|2.36023|+0.82%
|IR closure_call|372.21566|370.69473|-0.41%
|AOT closure_call|106.95306|108.81225|+1.74%
|IR arithmetic|94.00202|95.74695|+1.86%
|AOT arithmetic|2.33872|2.36247|+1.02%

All disabled-control ranges overlap, and both large target gains disappear.
The arithmetic slowdown remains with the evaluator disabled; execution of the
new leaf path does not explain it. Other runtime/layout effects are not isolated.
No universal no-regression claim is made.

The initial IR-only pilot `run-ijj48usb` predates the debugger/metadata fixes:
five forks, three warmups and five 300 ms windows, 40 jobs / 200 samples. IR
direct calls improve 352.62501 → 140.58286 (-60.13%, separated ranges).
Indirect bounded/wide controls move +0.04%/+0.88%; arithmetic moves +4.09%.
All raw pilot samples and its exact separate runtime diff are retained.

### Validation and audit

Release/debug each pass the 72-file selected matrix before the final include
and source-map corrections: 49 QUnit wrappers, 247 cases / 17,594 assertions,
plus 23 standalones. Final builds rerun eight closure/metadata files per build:
five wrappers, nine cases / 164 assertions, plus three standalones. The composed
wrapper is rerun after the debugger path-check extension: 37 assertions per
build. Coverage includes wrapping, effects/errors, concurrent first calls,
Program churn/crossings, warmed/cold debugger attachment, metadata compression,
shared-module closure lifetime, cancellation, stack readers and typed callbacks.

The focused prefix-map regression passes release/debug. The wider 17-test
prefix-map suite is **not green** on this macOS host: final has 13 failures and
one skip; saved HEAD has those same failures plus the new debugger regression.
The unchanged failures include physical-path and byte-reproducibility problems;
their full cause diagnosis is not claimed. Early library-selection probes were
inconclusive and superseded by explicit child environments.

Standalone syntax checks pass for the interpreter, JIT runtime, binary
serializer, AOT runtime and owning expression/slot registry translation units;
the latter use their Qore header prerequisite. This is not a full split build.
Eleven benchmark-harness tests pass. Old AOT outputs retain byte-identical
225/270 reference records. The full-width fixture preserves a baseline JIT
borrowed-argument reproducer and therefore checks AST/IR/tiered/AOT, not JIT;
the separate debugger fixture does test JIT.

All 62 `audit-changes` checks are recorded individually: **20 Pass, 42 N/A,
0 Fail**, scoped to these changes and the stated validation limitations.

|!Audit group|!Status|!Evidence
|Module, QPP, DataProvider, factory and JAR registration|N/A|No such implementation or registration changes
|Test structure and dependencies|Pass|Modern executable fixtures; repository QUnit/FsUtil; isolated test artifacts
|Sandboxing and cancellation|Pass|No production I/O; bounded evaluator; existing cancellation and Program guards retained
|Exception, thread, type and input safety|Pass|RAII closure ownership; release/acquire descriptor publication; bounded payload/string validation
|Performance and correctness|Pass|Paired samples and disabled control; explicit arithmetic cost and baseline failures; no parity claim
|Documentation and source metadata|Pass|Version/lifetime contract; mapped debugger entry regression; no new public API

[Complete evidence](results-composed-ir-closures-macos-arm64.json) retains all
**275 jobs / 1,375 samples**, variant diffs/hashes, integrity checks, profile
summary and audit. Final production diff SHA-256:
`67298937350a496c481d9c73458b2489736b564765f203499b14b6fbf05365ec`.
Release/debug libraries:
`bdfcfdee3608a8122e56ebdb5aed99099ada531dc03b5995eeff507efe81516e` /
`6639e17beea88b381c0533a64a86b48791d6a8143853a93bf48865def29058ac`.
Runtime text grows 13,893,248 → 13,898,064 bytes (+4,816).
No full suite, sanitizer, Linux, allocation-failure or malformed-entry injection
claim is made. The overall performance goal remains active.

### Next measured bottleneck

A separate five-second profile of the actual captured-closure workload has
3,798 main-thread samples (the signal-wait thread is excluded). The native
closure body is reached through `execClosureDirect`, which still instantiates
scalar parameters in TLS, installs the capture environment/frame, and uses
boxed local/capture loads and cleanup. Leaf samples include 286 in
`execClosureDirect`, 152 in `LocalVarValue::set`, 146 in TLS access, 108 in
statement-context swapping and 102 in parameter instantiation. These are
sampling observations, not additive isolated latency estimates.

Next: expose guarded typed native closure entry to dynamically passed callbacks,
preserving capture synchronization, lexical Program/object/class context,
debugger fallback, stack/cancellation guards and reader-safe frame lifetime.
Existing compiler typed dispatch requires a recoverable closure-create target;
this callback-parameter loop does not provide one. After that, return to unboxed
polymorphic continuations and immutable caller-owned diagnostic frames. Neither
larger optimization is implemented by this iteration.

## Dynamic typed closure entry and scalar continuation (2026-10-08)

The retained same-Program closure candidate reduces AOT noncapturing execution
from **45.68454 to 25.83931 ns/element (-43.44%)** and captured-integer execution
from **127.77740 to 30.65052 (-76.01%)**. A larger-batch repeat confirms reductions
of **44.66% / 76.36%**. Fresh Java is **1.66986 / 1.75305**, leaving **15.47× /
17.48×** whole-workload gaps. Java parity is not reached.

Baseline is `b97968a5bc002add8850995bff58e78b044448e7`, library SHA-256
`bdfcfdee3608a8122e56ebdb5aed99099ada531dc03b5995eeff507efe81516e`.
Final release/debug hashes are
`05392bbff192656b92dec7f51470986e0f4943101c92d9866a99561e4e58b968` /
`cbad03bc88f3735f0f3c018b68e3ba03c9a788c6fbd3302bc1e05431e691c5e9`;
production diff SHA-256 is
`62828806aa4b8f38508a34dbf06ea98fc0915b104f28ca2058ab48c99a60b948`.

### Implementation and retained semantics

A compiler-proven exact `int(int)` closure can publish an optional native
companion even when the caller sees only an opaque typed callback. The entry
takes its borrowed live closure, explicit `RuntimeConfig`, unboxed argument,
fallback flag, runtime context and exception sink. The existing integer v3
function-reference continuation borrows this closure on a cache miss; it never
stores or retains a closure instance. Boxed closure entrypoints can use the
same companion while preserving numeric argument cleanup.

Admission requires a bounded straight-line pure scalar body (at most 64 IR
instructions), no body locals or generic type substitution, and only ordinary
nonreference integer captures. Each source capture read takes the existing
capture read lock separately. A failed capture read requests ordinary fallback
before argument consumption or visible effects; capture values are not
snapshotted, hoisted, retained in shared metadata or assumed immutable.
The whole-body proof excludes loops, calls, side effects and throwing arithmetic.

Runtime admission retains normal-body/context identity and readiness, live
same-Program ownership, exact signature, no attached debugger, stack guard and
self/argv exclusions. Lexical object/class/self context and reader-safe immutable
frame publication remain in place. The frame helper is the last data member;
reader grace periods are not shortened. A marker-allocation exception rolls
back the first marker before changing compatibility state. Existing serialized
formats/helper signatures do not change; the additive binding/read helpers
require paired compilation against each runtime. Old artifacts still run.

Disabled continuations reject before closure metadata decoding. Boxed helpers
check for a published companion before acquiring a TLS-backed runtime view.
This matters to the fallback-cost correction measured below.

### Final paired primary

`run-07c2ov0w`: 1024 elements, five forks, three warmups, five 500 ms windows;
150 jobs/750 samples. Baseline/current IR and independently compiled AOT, plus
fresh Temurin 25.0.2 / JMH 1.37 Java. Each mode passes 300 independent reference
checks. All jobs are shuffled and sequential; timing never overlaps builds,
tests, profiles, disassembly or evidence validation. Units include loop,
computation, dispatch and checksum, not isolated call latency.

|!Kernel|!IR before|!IR after|!AOT before|!AOT after|!Java JIT
|closure_call|368.43610|366.14021|45.68454|25.83931|1.66986
|closure_capture|404.64729|396.18628|127.77740|30.65052|1.75305
|indirect_wide|347.32007|341.43867|8.49899|8.32778|1.67983
|indirect_float|297.57268|293.92904|10.15903|10.18729|1.64747
|polymorphic_call|407.36165|410.77492|88.63748|89.93963|2.53918
|arithmetic|93.79568|94.96662|2.36051|2.37303|0.89069

Noncapturing fork ranges are 45.02995–46.21573 before and 25.72865–26.29285
after; capture ranges 126.98654–129.66690 and 30.40610–30.89761. Both separate.
All other AOT ranges overlap. Wide-ref improvement is not attributed to closure
execution. Polymorphic median is **1.47% slower**; its repeat below is also
slightly slower, so a modest cost is not excluded by overlap. IR ranges overlap
except wide-ref (a different path); no IR improvement is attributed here.
The float candidate retains a12.09579 ns slow fork; the baseline has11.38562.
Alignment does not establish universal layout neutrality or remove all outliers.

### Larger-batch repeat and disabled control

`run-r2m6hv2a`: 4096 elements, five forks, otherwise the same windows;60 jobs/
300 samples, separately compiled before/after AOT. Only the two closure target
ranges separate. No fresh Java run at this count.

|!Kernel|!AOT before|!AOT after|!Change|!Before / after fork ranges
|closure_call|44.26276|24.49506|-44.66%|43.84181–44.76462 / 24.40618–24.90831
|closure_capture|126.38745|29.87691|-76.36%|124.57022–127.59013 / 29.03234–30.55668
|indirect_wide|7.45827|7.29160|-2.23%|7.30726–7.59477 / 7.23227–7.36194
|indirect_float|9.03153|9.03792|0.07%|8.99489–9.91343 / 9.00266–9.09730
|polymorphic_call|87.53309|88.08406|0.63%|86.66393–87.84932 / 87.65816–89.62758
|arithmetic|1.33570|1.32761|-0.61%|1.32988–1.34151 / 1.32193–1.33863

`run-5q6czbue`: 1024 elements, five forks, 60 jobs/300 samples, with
`QORE_DISABLE_AOT_DYNAMIC_CLOSURE_ENTRY=1` during both compilation and execution.
Every before/after range overlaps. This does not prove zero overhead, but the
earlier roughly 7% fallback penalty is not reproduced.

|!Kernel|!AOT before|!AOT after|!Change
|closure_call|46.06231|45.94365|-0.26%
|closure_capture|130.05928|128.17620|-1.45%
|indirect_wide|8.49032|8.40939|-0.95%
|indirect_float|10.38193|10.26345|-1.14%
|polymorphic_call|89.14230|89.16924|0.03%
|arithmetic|2.35456|2.34261|-0.51%

### Earlier candidates and adverse evidence

All intermediate results remain in
[complete evidence](results-dynamic-closure-entry-macos-arm64.json):

|!Run|!Variant|!Outcome
|run-iua6ygd7|Boxed pilot;90 jobs/450 samples|Closure45.37406 → 31.60697;capture127.79727 → 36.73255. Float+4.25% overlaps;polymorphic+2.22% separates unfavorably.
|run-eawj0dg_|Unboxed continuation;150 jobs/750 samples|Closure45.73082 → 26.64901;capture130.38695 → 31.91748. Target ranges separate.
|run-mhhmykfc|Unboxed disabled;60 jobs/300 samples|Fallback closure+7.48%,capture+7.23%,both ranges separate. Rejected pending early disabled/absent-entry rejection.
|run-9nv6nbm0|Early rejection disabled;60 jobs/300 samples|All six ranges overlap;closure-0.81%,capture-0.14%.
|run-yuelz1l4|Early rejection16-byte primary;150 jobs/750 samples|Closure-43.52%,capture-75.54%;float+0.79% separates unfavorably.
|run-cdqdnrkv|Early rejection16-byte4096 repeat;80 jobs/400 samples|Closure-44.01%,capture-76.21%,but float+18.65% separates unfavorably. Commit held.

The saved-artifact cross `closure-float-cross-l5riyk7_` runs50 jobs/250 samples
on compatible old/new executables and runtimes at 1024 / 4096 elements. At4096,
the old executable measures9.07115 ns on HEAD and 9.07726 on early rejection;
the new executable measures10.74754 on early rejection but9.01609 on the
preceding unboxed runtime. A second new executable measures10.81618.
Normalized float-v3 bodies contain747 instructions in both libraries; only
two cold string offsets differ. This supports a layout interaction, not a
proven microarchitectural cause.

The 64-byte-alignment cross `closure-float-align64-_3os1713` runs60 jobs/
300 samples over six compatible pairs. On final alignment the two saved new
executables measure9.02340 / 9.00308 ns at 4096, versus10.56638 / 10.49899 on the
16-byte runtime. Their 1024 medians are 10.13241 / 10.16787 versus11.67622/
11.75910. The 16-byte forks mix fast/slow results; ranges overlap and all forks
are retained. Old-executable control ranges overlap. Final primary/repeat
above use newly compiled artifacts, not just these saved executables.

All twelve prepared v3 entrypoints now request64-byte alignment. Runtime
`__text` grows13,898,064 → 13,949,424 bytes (+51,360), of which38,816 is over
the 16-byte early-rejection candidate. This is measured target-specific tuning,
not proof of a benefit on other architectures.

### Validation and follow-up

Final release/debug each pass 16 focused files:14 QUnit wrappers,43 cases,
4087 assertions and 2 standalones. A subsequently expanded closure wrapper passes
3 cases / 114 assertions per build, adding generated-code checks that two capture
reads remain distinct at -O3. Coverage includes AST/IR/JIT/tiered,
AOT -O0/-O3, native EH, disabled compile/runtime, forced split codegen,
full-width wrapping, distinct environments, mutation, unassigned/reference
fallback, concurrent first calls, concurrent capture writers/stack readers,
and cold/warm debugger attachment and detachment.

The earlier unboxed compiler/runtime candidate passed73 selected files per
build (50 QUnit wrappers/249 cases/17680 assertions plus23 standalones), including
cancellation, Program/module lifetime, access and cleanup coverage. Its first
whole-module stack-helper assertion failed because dynamic companions also use
that helper; the assertion was narrowed to the intended function-reference
adapter and the matrix resumed successfully. Do not relabel that matrix as the
later alignment run. Four standalone translation units pass for the unboxed
candidate; final JITRuntime standalone syntax passes. Eleven harness tests pass.
Old AOT artifacts still produce byte-identical 225 / 270 reference records.

LLDB confirms the captured native route through integer v3 continuation,
`tryExecNativeIntClosureScalar`, dynamic companion and synchronized capture
reader; process completes successfully. No full suite, sanitizer/race detector,
Linux, allocation-fault injection or complete split-runtime build is claimed.
Forced split codegen is not a full split-runtime build.

The evidence retains all 970 jobs/4850 samples, source/library/artifact hashes,
production diffs, cross scripts and all 62 audit checks (20 Pass/42 N/A).
Shared-host thermal/frequency/scheduling state is uncontrolled; fork ranges
are not confidence intervals. Java retains callback targets with DONT_INLINE.
Its captured field access has different ownership/synchronization semantics,
and Qore's polymorphic base case can inline while Java retains all three.
The goal remains active. Next: commit the audited closure improvement, fetch/
merge origin/develop as requested, revalidate, then implement explicit-context
unboxed polymorphic caller continuations and broaden call-category coverage.

## Upstream integration and IR engine provenance (2026-10-08)

After audited closure commit `a3d430d0672eeb9e5626cd2542ed4d54f8f60fcd`,
fetched and integrated `origin/develop` at
`2e272bdd8beef659ecdb29e88be287d2bfff5e35`. Two manual conflicts preserve
both parents: dynamic closure fields coexist with the compiler's new native
symbol override; the single-list-index mutation shortcut remains, while the
general path now passes the sink to fallible hash-key conversion and propagates
its error before RHS retention or lvalue mutation.

Both release/debug qore/qcc builds pass. Each build passes 23 selected files:
21 QUnit wrappers / 62 cases / 82,292 assertions plus 2 standalones, including
the closure/callback/native-method tests and upstream IR-only, concurrent
promotion, hash-key encoding, concurrent lvalue paths, expression-result and
string-conversion encoding regressions. Old AOT artifacts retain byte-identical
225 / 270 / 300 reference records. This is focused integration validation, not
a full upstream suite, sanitizer/Linux run or validation of unrelated changed
qlib modules.

The merge includes upstream `5243c8aef`, which prevents native compilation in
explicit IR mode. A diagnostic using the unchanged polymorphic benchmark and
`QORE_JIT_TIMING=1` (no threshold override) confirms the distinction:

|!Runtime|!Requested mode|!Native promotion trace
|Saved premerge a3d430d06, library05392bbf|ir|polymorphicCall and two next methods compiled and promoted
|Merged runtime, library1c75a52b|ir|No compilation trace

Both invocations use `polymorphic_call 1024 3 5 100 7` and exit successfully.
The diagnostic's SAMPLE output is not a performance result. It demonstrates
that historical qore-ir labels are requested modes, not verified pure-IR
measurements; it cannot identify the engine of each historical timing sample.
AOT/Java comparisons are unaffected. A fresh pure-IR baseline must precede
further interpreter comparisons; the correction must not be called an
optimization regression.

[Integration evidence](results-dynamic-closure-merge-macos-arm64.json) records
parents, library hashes, the trace, test totals and all 62 audit items for the
manual resolutions/documentation (13 Pass / 49 N/A / 0 Fail). It is not a new
audit of all 178 upstream-only changed files. Next: capture a fresh merged
AOT/Java baseline, profile polymorphic dispatch, and implement the explicit-
context scalar method continuation described above.

## Scalar polymorphic caller continuation (2026-10-08)

Baseline: merged `5907d7c0e`, release library `1c75a52b`. Retained candidate:
library `6cad7615`, compiler/runtime diff `b59dd357`. Five independent forks,
three warmups, five 500 ms measurement windows; before/after AOT compiled
separately from identical sources and interleaved. Primary count1024 includes
Java; count4096 repeats the AOT comparison. Units are whole-loop ns/element,
including selection, receiver locals, computation and checksum, not isolated
call instruction latency.

|!Polymorphic workload|!Before|!After|!Change|!Before fork range|!After fork range
|Primary1024, run-7yz3lkk7|85.79750|78.69874|-8.27%|85.02125–87.58595|77.56925–79.85909
|Repeat4096, run-_bysdvv6|84.86514|76.65974|-9.67%|84.49667–85.13727|76.23613–77.60042
|Disabled4096, run-iujsmah6|84.75138|86.00696|+1.48%|83.96208–85.98207|84.88291–86.60289

Java primary is 2.51916ns/element: the retained candidate is **31.24x Java**
for this workload, so this is progress, not retained-call parity. Qore retains
the exact-base fast/inlined case while two subclasses use the native helper;
Java retains all three targets. Do not describe the comparison as three
equivalent native call instructions.

|!Control|!Primary before|!Primary after|!Primary change|!Repeat change
|closure_call|26.08352|25.38384|-2.68%|-0.97%
|closure_capture|31.11978|31.24201|+0.39%|-0.07%
|indirect_wide|8.27192|8.15192|-1.45%|-1.12%
|indirect_float|10.06262|10.14738|+0.84%|+0.41%
|arithmetic|2.34155|2.35101|+0.40%|+0.05%

Final control ranges overlap except the favorable primary closure movement;
all disabled-path control ranges overlap. The disabled polymorphic median is
still unfavorable: overlapping ranges do not prove absence of a layout cost.
No claim of universal improvement or zero fallback overhead is made.

The initial general-array lookup candidate is retained in the evidence, not
discarded: run-astxwdhf and run-63ciyka_ improve the target 8.79%/7.05%, but the
latter has a 1.04% floating-control slowdown with non-overlapping ranges.
Disabled run-im9pudyl has a 2.51% polymorphic slowdown with non-overlapping
ranges. Identical-executable diagnostic run-82mf4v4x still has a 2.29% unfavorable
median, with narrowly overlapping ranges; it holds generated code fixed but
does not isolate runtime layout from all other runtime changes. Inspection
found the new scalar path constructing general argument-key arrays. The retained
version compares the same complete key directly for its known unary integer ABI.
Its three final runs above were measured independently against the original
merged baseline; no unpaired initial/final speedup is claimed.

The versioned try-helper passes live RuntimeConfig and native integer payloads
without consuming borrowed receiver ownership. A sole exact native local store
can avoid result boxing; arbitrary generic returns, coercion/EH, access/parse
policy, Programs, epoch, deletion, debugger, body readiness and observable-frame
guards remain. Old v1 bindings and artifacts still work.

[Sanitized evidence](results-polymorphic-continuation-macos-arm64.json) includes
all seven runs: 470 process jobs and 2350 measured windows, library/source hashes,
every sample and recomputed summary, plus the full 62-item audit. Timing did not
overlap builds, tests, profiling, disassembly or evidence validation. Release and
debug each pass 24 selected files: 18 QUnit wrappers/46 cases/2510 assertions plus
6 standalone scripts. The expanded method wrapper has 514 assertions/build. Old
AOT retains all 300 checksum records. This is not a full-suite or sanitizer run.

A separate 5-second/1ms profile of the retained candidate at 4096 records 3803
main-thread samples, excluding 3803 signal-wait-thread samples. Direct caller
branches include receiver assignment 1399, native method attempt 676, receiver
cleanup 355 and stale-local reload 303. These are inclusive branch counts, not
isolated costs; do not add their nested children. Prominent leaves remain value
discard/type checks, object dereference, assignment and method frame setup.
The profile's SAMPLE output is not performance evidence.

Next: preserve caller-local caches after a proven-pure native hit while retaining
generic fallback invalidation, then investigate receiver assignment/ownership
and native frame setup. Preserve declaration-order destruction, reentrant effects,
debugger/stack readers and cross-Program fallback in each separately measured step.

After committing the continuation as `c911af9ae`, a fresh fetch brought two
additional upstream fixes through `e897fe252`: encoded hash-key cancellation
and archive source-revision handling. They merge without conflicts and leave
the continuation sources unchanged. Release/debug each pass 15 focused QUnit
files, 43 cases and 5409 assertions, plus the native 172-check cancellation
fixture; Python 3.12 passes both CMake suites (7 and 5 tests). Old AOT retains
all 300 records. [Integration evidence](results-polymorphic-continuation-merge-macos-arm64.json)
retains setup failures and the complete audit. The measurements above describe
the optimization commit before this merge, not a timed result for the new merge.
Rebuild and preserve the new merged baseline before the next optimization.


## Accepted-object node stores (2026-10-08)

Baseline: merged revision `d8b053c703d037a24fbc09ec3cfb3ec8f3da8d1c`.
After the existing unfiltered required-class check proves a strong object,
`LocalVar::tryAssignInitialClassValue()` uses the existing node-assignment
overload instead of generic scalar/enum/value storage dispatch. The current-thread
slot guards and class/access/raw-generic checks remain. The runtime slot and
returned cache each still own one reference; real-reference bookkeeping, weak
normalization and declaration-order cleanup are unchanged. No compiler, serialized
format, helper ABI or mutable shared cache is added.

### Measurements

Each run reuses one identical AOT executable against both compatible runtimes.
Five forks, three warmups, five 500 ms measured windows and three startup samples
are shuffled/interleaved. All selected modes pass 300 independent reference
checks. Timing does not overlap builds, tests, profiling, disassembly or evidence
validation. Values are median fork means in **whole-loop ns/element**, including
receiver selection/storage/cleanup, arithmetic and checksum, not isolated calls.

|!Run and count|!Before|!After|!Change|!Before / after fork-mean ranges
|run-iomrh9_x (enabled), 1024|80.00998|73.09252|-8.65%|77.97481–81.50682 / 72.57090–75.48772
|run-j5013uin (enabled), 4096|77.24882|72.46488|-6.19%|76.37255–77.33420 / 71.46365–72.65602
|run-y69f3xha (disabled), 4096|77.91831|79.14540|1.57%|76.89749–78.15872 / 78.29887–79.27886

Both enabled target ranges separate favorably. The primary Java value is
2.55113 ns/element, leaving a **28.65×** whole-workload gap. Qore can inline the
exact base case while Java retains all three target calls; parity is not achieved.

The disabled run sets `QORE_DISABLE_AOT_CLASS_NODE_STORE=1` at execution for
both runtimes (the baseline ignores this new flag). Its **1.57% slowdown has
non-overlapping ranges** and remains part of the result. It supports attribution
of the enabled improvement to the new path, but does not establish the cause of
the disabled slowdown or prove zero generic-path/layout overhead. Retaining the
optimization is based on the repeated enabled benefit with unchanged semantics,
not on claiming that every path improves.

|!Control|!Primary 1024|!Repeat 4096|!Disabled 4096
|indirect_wide|-2.22%|1.10%|0.99%
|indirect_float|-3.31%|-0.63%|-0.63%
|closure_call|-1.10%|0.58%|-0.13%
|arithmetic|-0.69%|0.02%|0.12%

All control fork ranges overlap in all three runs. The unfavorable wide-callback
and closure medians above are retained; overlap does not prove neutrality.
Startup measurements have uncontrolled filesystem caches and are not isolated
compiler or loading costs.

[Sanitized evidence](results-class-node-store-macos-arm64.json) retains every
sample and summary from all three runs: **175 jobs / 875 measured windows**,
library/source/artifact hashes, the exact runtime diff and the 62-item audit.
Candidate release/debug library SHA-256 values:
`494bbd622e791c4d0504c50c63482076acb7500249b485121e7a72babb3e44a8` /
`7e03241f4df568b8ddede5fc33a65109c660906506caf96520a2f2e8233eda68`.
The runtime diff SHA-256 is
`89a26cadfa9cdc500f4b74dd34e21907207a75d01d7c3c9c440aae7203027c4b`.
Release `__text` grows 13,958,844 → 13,959,236 bytes (+392).

### Validation and remaining costs

Release/debug each pass 24 selected files: 18 QUnit wrappers, 46 cases / 2542
assertions plus six standalone scripts. The extended assignment wrapper passes
280 assertions on the baseline and both candidates; it removes the original
owner and verifies the initialized local's sole-owner lifetime on normal/EH exits
with a weak observer. The AST/AOT destructor-order wrapper passes 47 assertions
per build, including disabled-path comparison. Its previously documented JIT
mixed-local unwind-order issue remains outside this change. Old AOT retains all
300 checksum records; 11 Python harness tests pass. No full-suite, sanitizer,
race-detector, Linux or allocation-failure-injection result is claimed.

A separate 5-second/1ms profile records 3791 main-thread samples (excluding 3791
signal-wait samples). Direct caller branches include assignment 1221, native
method attempt 592, runtime local clear 386, two stale reload sites 327/314 and
temporary decref 309. These are inclusive counts, not isolated costs; do not add
nested children or use the profile's SAMPLE output as timing evidence. Generated
code confirms the accepted node overload and retained generic fallback.

Next: reduce receiver ownership/reload work with an explicit cleanup-effect and
lifetime proof, or reduce repeated class compatibility/frame setup work. The
simple pure-call epoch-skipping proposal is insufficient: statement-boundary
cleanup also invalidates caches and can run destructors. An effect-aware boundary
must preserve earlier generic effects even when a later call is pure. The rejected
native-only local prototype must not be revived without declaration-order cleanup.

The optimization was committed as `57f6aa11e`. A subsequent fetch brought
`9951c7d3e`, fixing missing notifier dependencies in standalone debug compilation.
It merged without conflicts and leaves the optimization sources unchanged.
Standalone Debug syntax checks pass for the debug implementation and notifier
header. Release/debug each pass five focused QUnit files (9 cases / 1384
assertions), plus the debug C++ async-I/O wrapper (1 case / 2 assertions); old
AOT retains 300 records. [Integration evidence](results-class-node-store-merge-macos-arm64.json)
contains the full 62-item audit. The timings above are premerge; no new merged
performance or full non-SCU build result is claimed.

## Strong-object initializer entry (2026-10-08)

Baseline: final merged `d3aa2a9bc` on `feature/performance`, including
`origin/develop` at `9951c7d3e`. Its exact-revision release/debug builds and
ownership checks pass. The new runtime-only change identifies strong objects
before constructing a weak-input holder, then uses the existing checked fresh
initializer. A handled attempt avoids weak normalization and empty-holder
cleanup. Type/access checks, both owned references, runtime-backed storage,
real-reference accounting, errors and destructor order are unchanged. Weak,
non-object, captured and unadmitted cases retain their ordinary path; some
misses pay an additional admission probe. No compiler, AOT format or helper ABI
change is made. `QORE_DISABLE_AOT_CLASS_STRONG_STORE=1` selects the old entry path.

### Measurements and attribution

Apple M4 Pro, macOS arm64, Temurin Java 25.0.2+10. All comparisons use five
independent forks, three warmup windows and five 500 ms measured windows per
fork, three startup samples and shuffled sequential jobs. Each baseline pair
uses the identical AOT executable against saved/current libraries. The final
toggle comparison also holds the library fixed. All modes pass 300 independent
reference checks before timing. Timing does not overlap builds, tests, profiles,
disassembly or evidence validation. Values below are medians of fork means in
whole-loop ns/element, not isolated call latency or confidence intervals.

|!Comparison|!Before / disabled|!After / enabled|!Change|!Before / after fork-mean ranges
|run-925e5v99, enabled, count 1024|73.38268|69.53294|-5.25%|73.25845–73.56726 / 67.66657–69.60601
|run-g0ao81ak, enabled, count 4096|71.70679|67.94573|-5.25%|71.49596–72.99545 / 67.17278–68.26435
|run-0g3cbger, disabled, count 4096|71.82313|70.25146|-2.19%|71.04624–72.27300 / 69.11950–73.02172
|strong-toggle-tmw9dz33, same-library on/off, count 4096|70.16176|67.49224|-3.80%|69.51931–70.79992 / 66.52946–67.74187

The enabled target ranges separate in both baseline comparisons. Primary Java
measures 2.54843 ns/element, leaving a **27.28×** whole-workload gap. Qore may
inline the exact base case while Java retains all three target calls; this is
not Java parity or a pure dispatch-cost comparison.

In the disabled baseline comparison, both processes receive the new flag (the
baseline ignores it). Its target median improves 2.19% with overlapping ranges.
Therefore the full cross-library 5.25% gain cannot be assigned solely to the new
branch. The extra same-library comparison changes only the flag and demonstrates
a 3.80% target reduction with separated ranges. Its integer-callback control
changes -1.26%, with overlapping ranges. This supports the branch's benefit;
it does not partition the earlier gain exactly or prove zero fallback cost.

|!Unrelated control|!Primary 1024|!Repeat 4096|!Disabled 4096
|indirect_wide|0.96%|1.77%|-0.23%
|indirect_float|-0.70%|1.02%|-0.60%
|closure_call (noncapturing)|-1.09%|-2.17%|-2.66%
|arithmetic|0.48%|-0.14%|-0.63%

All enabled control ranges overlap. Adverse medians above remain part of the
record; overlap does not prove neutrality. The disabled run's closure ranges
separate favorably, but the new branch is off and no closure speedup is attributed
to it. Startup samples have uncontrolled filesystem caches. No samples or adverse
runs were discarded.

[Evidence](results-class-strong-store-macos-arm64.json) retains all four runs:
**195 jobs / 975 measured windows**, every raw timing value, recomputed summaries,
library/source/artifact hashes, the exact runtime diff, the same-library runner
source and all 62 audit checks. Baseline release library SHA-256:
`40caaa97d1eb3133113e14b32fe2cb2bebf53e8fe4a25b7cf244382e2fc83967`.
Measured candidate release/debug libraries:
`32e931f8d80f4d1cc6481ab54fea3b2c05c26579bb887e8d4c2aa9924224a207` /
`d269bd51427d3734f5e43473d26c3b9b6cbf8c6cbc7ab63239c61c8fc918900a`.
Runtime diff SHA-256:
`6b6b99a4821784e95793f408e7985694d65bc44799449e2e9f62f43356fbd362`.
Release executable `__text` grows 13,959,236 → 13,959,516 bytes (+280).

### Validation and next step

Release/debug each pass 24 selected files: 18 QUnit wrappers, 46 cases / 2574
assertions plus six standalone scripts. The expanded assignment wrapper passes
300 assertions on the saved baseline and both candidates. It adds an unrelated
strong-object rejection and flag-disabled comparisons, retaining private access,
weak expiry, generic compatibility and normal/EH final-owner checks. The AST/AOT
destructor-order wrapper passes 59 assertions per build; its previously documented
JIT mixed-local unwind issue remains outside this change. Old premerge AOT output
still matches all 300 reference records; 11 Python harness tests pass. No full
suite, sanitizer, race detector, Linux or allocation-failure injection is claimed.

A separate 5-second / 1 ms sample profile has 3785 main-thread samples (excluding
3785 signal-wait samples). Direct caller branches include assignment 1145, native
method attempt 632, runtime local clear 447, two stale reload sites 307/274 and
temporary decref 292. These are inclusive samples, not isolated costs or
before/after timing; nested children must not be added. Disassembly confirms the
early branch and retained fallback.

Next: derive a boundary-aware local-effect/lifetime proof, including pending
generic effects and destructor-capable cleanup, before attempting to retain
receiver caches across pure calls. Admission alone cannot justify eliminating
runtime slots or their ordered cleanup. Repeated class acceptance and native
frame setup remain fallback targets if that proof cannot admit useful cases.

## Exact receiver identity after reflective writes (2026-10-08)

This is a **correctness fix**, not a speedup. While investigating read-only
class-parameter caches, a baseline reproducer showed that an unwritten parameter
is not immutable: `set_local_var_value()` can replace it with a subclass instance.
The cached entry-class result then selected the old base method. AST returned
`110`, but baseline AOT returned `101`.

The generated guard now requires the call's actual receiver to equal the entry
receiver before reusing that class result. The existing entry cleanup must own
the original object, preventing address reuse; otherwise the compiler retains
the per-call class check. Changed receivers use normal dispatch. Validity,
Program/access checks, exceptions, runtime slots and ownership are unchanged.
Previously compiled native artifacts must be recompiled.

Added `monomorphic_call` to exercise this guard: one unchanged receiver parameter
of a non-final class, with the exact-base object created outside timing. The
existing three-type `polymorphic_call` uses an assigned body local and does not
exercise this hoist. Both baseline and candidate compile the same expanded
benchmark snapshot. Java's non-final target retains `DONT_INLINE`, whereas Qore
may inline the exact-base arithmetic; this is not an equal-inlining-policy or
isolated call-latency comparison.

Before: `6bb834ad1`; candidate: the compiler diff recorded in
[the complete evidence](results-exact-receiver-identity-macos-arm64.json).
Apple M4 Pro, Temurin 25.0.2+10, five forks, three warmups and five 500 ms
measurement windows, three startup samples, shuffled sequential jobs.
Separate baseline/candidate AOT compilation; no artifact-reuse shortcut.
Both runs retain all 100 jobs/500 windows with no build/test/profile overlap.
All 315 independent reference checks pass per selected mode.

|!Count|!Kernel|!Before ns/element|!After ns/element|!Change|!Fork ranges overlap
|1024|monomorphic_call|11.65187|11.66607|0.12%|Yes
|1024|polymorphic_call|68.83208|69.27922|0.65%|Yes
|1024|indirect_wide|8.34529|8.26068|-1.01%|Yes
|1024|arithmetic|2.35711|2.34356|-0.58%|Yes
|4096|monomorphic_call|10.38266|10.41820|0.34%|Yes
|4096|polymorphic_call|66.98306|67.15477|0.26%|Yes
|4096|indirect_wide|7.24567|7.32151|1.05%|Yes
|4096|arithmetic|1.34953|1.34209|-0.55%|Yes

The affected loop changes +0.12% and +0.34%; neither run separates its fork
ranges. All controls overlap too, including the adverse +1.05% wide-callback
repeat. Overlap is not a proof of neutrality and ranges are not confidence
intervals. No speedup or precise guard-cost attribution is claimed.

At count 1024, Java measures 1.66690 ns/element for `monomorphic_call` versus
Qore's 11.66607 (7.00x), and 2.53364 for `polymorphic_call` versus 69.27922
(27.34x). These include loop arithmetic, checks and ownership work; the
inlining-policy caveat above applies. Java parity is not reached.

Release/debug builds each pass 25 selected files: 47 QUnit cases/2696 assertions
plus six standalone scripts. The new regression contributes 122 assertions per
build, covering repeated replacement, return to the original receiver, base and
derived entry objects, exception handling, cross-Program writes, four concurrent
callers, O0/O3, source stripping, native EH, parallel compilation and disabled
hoist/runtime controls. All benchmark execution modes pass 315 reference records;
11 Python harness tests pass.

Separate existing limitation: baseline and candidate IR/JIT/tiered execution
raise `UNKNOWN-VARIABLE` for this uncaptured caller-parameter reflection case,
including with IR context elision disabled. The new regression therefore compares
AST with AOT; it does not claim to fix IR frame visibility. Full reproduction
and sample records, source/library hashes and all 62 audit checks are in the
evidence file. Audit: 15 Pass, 47 N/A, zero Fail.

Next: a guarded read-only strong-parameter cache with reflective-write
invalidation, not unconditional reload removal. Admission must validate the raw
slot against the already-owned cache snapshot; references, weak/escaped/captured
or written parameters and class body locals remain conservative. Keep runtime
slot ownership and cleanup ordering intact.

## Guarded class parameter caches (2026-10-08)

Copyright (c) 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT.

This is a correctness fix plus a guarded optimization, not a general call-speed
win. Eligible unwritten strong class parameters retain their real runtime slots
and existing entry owners. A compiler proof and matching raw-slot admission
allow reuse only while a same-thread/per-Program reflective-write epoch remains
unchanged. A write invalidates before reentrant assignment and permanently
returns that invocation to normal reference evaluation and reload/cleanup rules.
The epoch saturates rather than wraps. Optional, generic, captured, weak, written,
reference and AST-visible parameters and class body locals remain excluded.

The tests exposed an older correctness defect: the reload epoch was created
lazily while lowering the first mutating call. A receiver read lowered earlier
in a loop had no freshness check on later iterations. A subclass that replaces
its caller's receiver therefore produced 6 rather than AST's 51. Required-class
candidate loops now create the ordinary reload epoch eagerly, including with
the new cache disabled. Single returning blocks and reload-exempt uses avoid
unnecessary admission. This is why the old baseline is not a semantically
equivalent no-cache control.

Baseline: merged revision `d3bf25d52`; release library
`de24963eb55b60f25f4d69af052a8c45d560bbd00c4e4f915e6932609de3d219`.
Candidate: that revision plus the recorded compiler/runtime diff; release library
`efcff6b1cdc1bf83e9d1659fbbf33f52a11df23497178cf24e139edae23cc033`.
[Full evidence](results-class-param-cache-macos-arm64.json) retains source/library
hashes, all four runs and all 925 windows, reproduction sources and 62 audit rows.
Each run uses five forks, three warmups, five 500-ms samples and three startup
samples. AOT is compiled separately for each variant. No timing overlaps
builds, tests, profiles, disassembly or evidence validation.

Median of fork means, ns/element; negative changes are improvements:

|!Count|!Kernel|!Before|!After|!Change|!Fork ranges overlap
|1024|polymorphic_call|68.24817|66.56062|-2.47%|No
|1024|monomorphic_call|11.47280|11.99570|4.56%|No
|1024|indirect_wide|8.39884|8.14906|-2.97%|Yes
|1024|indirect_float|10.28466|10.27168|-0.13%|Yes
|1024|arithmetic|2.29576|2.37540|3.47%|No
|4096|polymorphic_call|67.43745|64.99998|-3.61%|No
|4096|monomorphic_call|10.39114|10.78800|3.82%|No
|4096|indirect_wide|7.22082|7.28043|0.83%|Yes
|4096|indirect_float|9.04115|9.08305|0.46%|Yes
|4096|arithmetic|1.31681|1.33781|1.59%|Yes
|1024|method_call|2.55764|2.65697|3.88%|No
|1024|direct_call|2.32010|2.38422|2.76%|No

Polymorphic gains repeat at 2.47% and 3.61%, while monomorphic regressions repeat
at 4.56% and 3.82%; neither target's fork ranges overlap. The added final-method
run regresses 3.88%, and its direct-call control regresses 2.76%. Arithmetic
regresses 3.47% at 1024 and 1.59% at 4096; wide/float control movement and all
adverse samples are retained. Do not attribute every cross-library or
cross-artifact change to the cache branch. Range overlap is not a significance
test and ranges are not confidence intervals.

A same-runtime, separately compiled comparison keeps the correctness fix in
both variants. Only `QORE_DISABLE_AOT_CLASS_PARAM_CACHE=1` differs at compilation;
`qore-aot-before` denotes disabled caching for this run, not the old library:

|!Count|!Kernel|!Disabled cache|!Enabled cache|!Change|!Fork ranges overlap
|1024|polymorphic_call|85.65964|65.98196|-22.97%|No
|1024|monomorphic_call|29.81555|12.06324|-59.54%|No
|1024|arithmetic|2.33726|2.35201|0.63%|No

This saves 22.97% on polymorphic and 59.54% on monomorphic calls compared with the
corrected ordinary-reload fallback, but the arithmetic control also changes
+0.63% with nonoverlapping ranges. The large on/off benefit mostly pays for the
freshness checks missing from the old baseline; it is not a 60% speedup over
the previous release.

At 1024, measured Qore/Java ratios are 26.35x for polymorphic calls
(66.56062/2.52573), 7.18x for monomorphic calls (11.99570/1.67006),
4.88x for wide callbacks and 6.25x for floating callbacks. The added run gives
3.00x for final methods (2.65697/0.88579) and 2.69x for direct calls
(2.38422/0.88514). These are whole-loop costs, not isolated call latency.
The existing inlining-policy caveat applies: Qore can inline the exact base
method while Java's non-final targets use DONT_INLINE. Java parity is not reached.

Release and debug each pass 26 selected files: 48 QUnit cases/3026 assertions
plus six standalone scripts. The new 330-assertion matrix covers AST reference,
O0/O3, stripped source, native EH, parallel compilation, full-context execution,
compile-disabled caching, mutation to invalid/reference values, later reference
changes, failed writes, destructor reentry, deletion, recursion and exclusions.
The existing identity test covers cross-Program mutation and concurrent callers.
All four benchmark modes and an older AOT artifact pass 315 independent
reference records; 11 Python harness tests pass. Generated LLVM contains all
three polymorphic parameter guards, including the formerly unchecked first read.

Known limitations, not passes: the optional-class reflective replacement probe
returns 101 in both old and candidate AOT versus AST's 110. Its complete source
and outputs are retained in the evidence. IR/JIT/tiered uncaptured caller-frame
reflection and zero-argument fast-call frame visibility have separately observed
baseline limitations; neither is claimed fixed. Saturation and entry-admission
mismatch are reviewed code invariants, not forcibly injected runtime tests.

Next: fix the optional-parameter visibility gap, then profile the remaining
polymorphic dispatch/context/ownership cost. Explore eliminating freshness work
only under a complete non-reentrant loop proof; source-level lack of assignment
alone is insufficient. Keep new baselines and report both gains and regressions.

## Optional class freshness (2026-10-08)

Copyright (c) 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT.

Optional class locals previously remained IR-only/reload-exempt because the
classifier recognized only required classes. AOT therefore retained a stale
receiver after reflection: the new regression produces 101 instead of AST's
110, loop results 6 instead of 51, and a pseudo-method error after a reflective
transition from `NOTHING` to an object. The candidate matches the AST reference.

The classifier now retains runtime storage/synchronization for optional classes.
Separately, every AOT function with nontrivial control flow and a runtime-
reloadable local creates the reload epoch before lowering the first read.
Freshness is no longer tied to the narrow parameter-cache eligibility test.
This covers written and AST-visible locals as well as read-only parameters;
native fast-entry arguments, nonreloadable locals and straight-line returning
functions keep their existing treatment.

Optional plain-class parameters can use the existing guarded cache only when
the raw slot is an identical strong object retained by the entry owner. Initial
`NOTHING`, references and weak slots retain fallback. The generic exclusion now
uses the declared type: removing `NOTHING` can erase a generic class's type
arguments. No new runtime helper, ABI or ownership transfer is introduced.
Artifacts must be recompiled to receive the compiler fix.

Baseline: `fedaa763d`, release library
`b09f71d35c1bf5f0f1275e4dbbd939b50264a93370ba825f4b8594adecf10330`.
Candidate: that revision plus the recorded compiler diff, release library
`8f29bc28d968a31e3de2c4a1593129a659e701cf3de041febba2f473cf91bc1a`.
[Full evidence](results-optional-class-freshness-macos-arm64.json) retains all
850 timing windows, hashes, reproduction, profile and all 62 audit checks.
Five forks, three warmups, five 500-ms samples, three startup samples per mode;
separate AOT compilation and randomized old/new job order. No measurement
overlapped builds, tests, profiling, disassembly or evidence validation.

The new `optional_method_call` workload uses the monomorphic receiver with a
nullable Qore contract; timed inputs are strong, non-null objects. Java references
already permit null and use the same non-final target. Mutation/null semantics
are tested separately, not mixed into the timed loop.

Median of fork means, ns/element; negative changes are improvements:

|!Mode|!Count|!Kernel|!Before|!After|!Change|!Fork ranges overlap
|qore-aot|1024|optional_method_call|39.46012|39.61111|0.38%|Yes
|qore-aot|1024|polymorphic_call|65.03656|64.94214|-0.15%|Yes
|qore-aot|1024|monomorphic_call|12.15556|12.31160|1.28%|Yes
|qore-aot|1024|method_call|2.62595|2.64252|0.63%|Yes
|qore-aot|1024|indirect_wide|8.34996|8.36038|0.12%|Yes
|qore-aot|1024|arithmetic|2.36215|2.36099|-0.05%|Yes
|qore-aot|4096|optional_method_call|38.37744|38.91940|1.41%|Yes
|qore-aot|4096|polymorphic_call|64.09131|63.54330|-0.86%|Yes
|qore-aot|4096|monomorphic_call|10.88830|10.82504|-0.58%|Yes
|qore-aot|4096|arithmetic|1.31194|1.31640|0.34%|Yes
|qore-ir|1024|optional_method_call|419.94324|418.27107|-0.40%|Yes
|qore-ir|1024|arithmetic|95.38259|97.49463|2.21%|Yes

Every old/new comparison has overlapping fork ranges, including the adverse
optional-call and IR arithmetic changes. This is not evidence of a new speedup
or proof of neutrality. Ranges are not confidence intervals and overlap is not
a significance test.

A same-library, separately compiled control retains the correctness fix in
both artifacts and changes only `QORE_DISABLE_AOT_CLASS_PARAM_CACHE=1`.
Here `qore-aot-before` means disabled caching, not the previous library:

|!Mode|!Count|!Kernel|!Disabled cache|!Enabled cache|!Change|!Fork ranges overlap
|qore-aot|1024|optional_method_call|58.22539|39.05660|-32.92%|No
|qore-aot|1024|arithmetic|2.30912|2.34767|1.67%|Yes

Guarded reuse saves 32.92% against corrected ordinary reloads; the arithmetic
control moves +1.67% with overlapping ranges. This is not a 33% gain over the
previous compiler: its optional reads could miss runtime writes entirely.

At count 1024, candidate AOT/Java ratios are 23.79x for optional calls
(39.61111/1.66483), 25.83x for polymorphic (64.94214/2.51382), 7.41x for
monomorphic (12.31160/1.66231), 3.01x for final methods and 5.01x for wide
callbacks. These include loop work, not isolated call latency. The existing
Java DONT_INLINE versus Qore exact-base inlining caveat remains; parity is not
reached. IR optional calls measure 418.27107 ns/element, separately from AOT.

Release/debug each pass 27 selected files: 49 QUnit cases/3298 assertions plus six
standalone scripts. The new 264-assertion matrix covers AST, O0/O3, stripped
source, EH, parallel codegen, full-context/cache-disabled execution, reflective
replacement, written/unwritten loop receivers, invalid/reference/NOTHING
transitions, deletion, weak/captured/generic exclusions and destructor order.
Optional lifetimes also match AST in IR/JIT/tiered. The expanded parameter-cache
test passes 338 assertions and now explicitly selects normal LLVM entry definitions
rather than accidentally checking a separately emitted fast variant.

All four benchmark modes and both saved baseline Qore modes pass 330 independent
reference records; an older AOT artifact passes its original 315 records on the
new runtime, and 11 harness tests pass. Full caller-frame reflection still raises
`UNKNOWN-VARIABLE` in baseline and candidate IR/JIT/tiered; those failures remain
recorded and are not counted as passes. The zero-argument fast-frame limitation
from the preceding checkpoint is outside this change.

A separate five-second sample of the optional workload (3788 main-thread samples)
still shows method-by-name dispatch, PIC lookup, native frame setup/teardown,
boxed-value cleanup and TLS/context acquisition. Its sampled costs are
attribution clues, not isolated latency measurements. The next optimization is
to investigate guarded native dispatch for nullable receivers, preserving
`NOTHING`/pseudo-method behavior and arbitrary subclass fallback, then tackle
the remaining receiver-local ownership and frame overhead.

## Name-dispatched native method continuations (2026-10-08)

Unresolved name-dispatched calls now reuse the existing native scalar method
continuation after ordinary dispatch has populated the full-key PIC. This covers
nullable-class and dynamic receivers without changing AST/IR method resolution
or narrowing an unknown return contract. Only an already-native integer argument
and a sole exact native-int local assignment qualify. Generic instantiations,
boxed/optional arguments and escaping results remain on their original paths.

The runtime change permits name-only slots in the existing try-helper. All
receiver validity/type, class/access context, caller/object/execution Program,
parse policy, dispatch epoch, signature, native-body identity/readiness, debugger,
stack and observer guards remain. The shared normal/invoke emitter boxes
arguments only on fallback. Fallback returns preserve assignment checks, cleanup,
side effects and exceptions; native returns use the existing handled/value
side channel with a NOTHING boxed cleanup slot. No new runtime ABI or cache
layout is needed, but callers must be recompiled to get the optimization.

### Identity and method

Before: `da4078d6089e6d24b7f138077e2b4af5bb81a611`, exact release library
`7ed5a4a78efddb539cf30c5b5dfab2f865323ad85259471645d36d37b7946b70`.
After: that revision plus C++ diff
`84698dca449c2db6cd1cbf0c5626a26476afb1c1b812188713ffb65e0eebec4c`,
release library
`60f6ec82a7e7d4de2520450fb74f474113fb520321d339b9e64ea55fbf930635`;
debug validation library
`0fa85f9ed71d47d5b9e89dc6450bfce539a916802c1866606d7e5f357ee5bfba`.

M4 Pro / macOS arm64; the Java run uses Temurin 25.0.2 and JMH 1.37.
Each run uses five randomized forks, three warmups and five 500 ms measured
windows per job, plus three startup samples per mode. All **800 measured windows**
are retained. No timing overlapped builds, tests, profiles, disassembly or
evidence validation. Numbers are medians of fork means in ns/element, not
isolated call latency; negative percentages mean less time. Before and after
AOT artifacts were separately compiled. Raw samples, identities, reproduction
commands, compiler control, tests, instruction comparison and all 62 audit
classifications are in
`results-name-method-continuation-macos-arm64.json`.

### Before/after and Java, count 1024

Run `run-yoebh2t_`, seed 810481, 90 jobs / 450 measured windows.

|!Kernel|!Before ns|!After ns|!Change|!Fork ranges overlap|!Java ns|!After / Java
|optional_method_call|39.190|25.722|-34.36%|no|1.659|15.50x
|polymorphic_call|65.794|65.218|-0.88%|yes|2.514|25.94x
|monomorphic_call|12.111|12.295|+1.51%|no|1.662|7.40x
|method_call|2.655|2.655|+0.02%|yes|0.880|3.02x
|indirect_wide|8.431|8.294|-1.63%|yes|1.670|4.97x
|arithmetic|2.354|2.371|+0.70%|yes|0.886|2.68x

Nullable/name-dispatched calls improve **34.36%**, but remain **15.50x** the Java
workload. This does not establish Java retained-call parity. The required-class
monomorphic control worsens **1.51% with non-overlapping fork ranges**; this
adverse result is retained. Exact-base/final Qore paths can inline arithmetic
while Java retains selected calls, so those rows have different inlining policies.
Polymorphic, wide callback and arithmetic movements have overlapping ranges;
do not attribute their small changes to this optimization.

Process startup medians: after 30.07 ms, before 30.37 ms, Java 23.57 ms. Filesystem
caches are uncontrolled; these are descriptive, not startup improvements.

### Longer loop, count 4096

Run `run-9mfak9qq`, seed 810482, 40 jobs / 200 measured windows.

|!Kernel|!Before ns|!After ns|!Change|!Fork ranges overlap
|optional_method_call|38.043|24.432|-35.78%|no
|polymorphic_call|63.906|64.175|+0.42%|yes
|monomorphic_call|10.762|10.925|+1.51%|yes
|arithmetic|1.320|1.313|-0.57%|yes

The optional-call gain repeats at **35.78%** with non-overlapping ranges.
The monomorphic median again worsens 1.51%, but its ranges overlap narrowly.
Startup medians: after 29.49 ms, before 30.04 ms.

### Same-runtime compiler control, count 1024

Run `name-method-toggle-3a6ot315`, seed 810484, 30 jobs / 150 measured windows.
Both artifacts use the same candidate runtime. Here “before” means compiled
with `QORE_DISABLE_AOT_NAME_METHOD_NATIVE_CONTINUATION=1`, not the older runtime.
The flag is absent at execution; freshness/admission guards are identical.

|!Kernel|!Before ns|!After ns|!Change|!Fork ranges overlap
|optional_method_call|38.747|25.572|-34.00%|no
|monomorphic_call|12.119|12.173|+0.45%|yes
|arithmetic|2.318|2.344|+1.13%|yes

The isolated compiler toggle reproduces **34.00%** lower optional-call time.
Monomorphic +0.45% and arithmetic +1.13% have overlapping ranges. This control
does not erase the adverse old/new monomorphic result. Startup medians:
enabled 28.31 ms, disabled 28.24 ms.

The old/new monomorphic AOT function contains the same **328 instructions**
after normalizing absolute stub addresses, local branch addresses and literal
pool/page placement while retaining helper names and branch-relative offsets.
This supports investigating layout effects, but does not prove their cause or
performance neutrality. It is not a byte-identical comparison.

### Validation, audit and remaining work

Release and debug each pass 28 selected test files: 51 QUnit cases / 3716
assertions in 22 files, plus six standalone scripts. The new test contributes
two cases / 418 assertions per build and checks AST/IR/JIT/tiered behavior,
stripped O0/O3, C++ EH, parallel compilation, compile/runtime disabled controls,
normal/invoke calls, wide integers, mismatched returns/arguments, NOTHING,
deletion, private access, overloads, generic/methodGate fallback, weak receivers,
cross-Program restoration, debugger attachment and concurrent native frame
readers. LLVM checks select the normal context entry, verify positive native
codegen and reject boxed/optional arguments and escaping-result continuations.
Existing ownership, stack, freshness and exact-receiver regressions also pass.

All six benchmark check modes (current IR/AOT, Java interpreted/JIT, saved
baseline IR/AOT) pass 330 independent reference records each. The saved baseline
AOT artifact also passes 330 records on the candidate runtime. Eleven harness
tests pass. There is no new IR timing result or IR-speedup claim. Previously
documented full caller-frame reflection limitations in IR/JIT and zero-argument
fast-frame visibility are outside this change and are not reported as fixed.

The audit classifies all 62 items: **15 Pass, 47 N/A, 0 Fail**. Details are in
`FOLLOWUP-AUDIT.md` and the JSON evidence. There is no new public API, module,
QPP flag, cancellation loop or filesystem/network operation in production code.

A separate five-second sample of the candidate optional workload contains 3796
main-thread samples. Top self samples include the guarded native try-helper
(855), native frame construction (728), QoreValue discard (592), frame destruction
(435), type checks (232), decref import stub (163) and decref helper (153).
The idle signal thread is excluded. These counts attribute work, not latency.
Steady-state method-by-name dispatch is gone; TLS acquisition is not the dominant
per-call cost in this path.

Next: eliminate cleanup calls only where their boxed values are proven empty
on native continuation edges, retaining all fallback ownership and EH behavior.
Then investigate narrower guarded admission for nullable exact-class targets
without overwriting unresolved AST/IR target facts, and reduce observable frame
setup without weakening stack readers, debugger behavior or Program boundaries.

## Scalar cleanup guards for native method callers (2026-10-08)

The existing late LLVM scalar-cleanup pass now also selects callers containing
`qore_rt_try_method_int_unary_v1`. It previously selected only native callbacks.
This is selection, not a new ownership rule: each cleanup still guards exact
NOTHING, int48 and bool values, while heap numbers, opaque references and other
owned values retain the original release helper, ordering and exception sink.
Normal optimization runs first; only local instruction/CFG simplification runs
afterward. Arbitrary functions are not selected.

`QORE_DISABLE_LLVM_METHOD_DECREF_GUARD=1` suppresses method-based selection at
compilation. Existing callback-selected functions, including mixed callers, stay
eligible. `QORE_DISABLE_LLVM_SCALAR_DECREF_GUARD=1` still disables the entire pass.
Compile separate artifacts for comparisons; neither control changes runtime
dispatch, native admission, coercion, frames or the ABI.

### Identity and measurement

Before: merged revision `b360e5a21bce122e9c66eba48ebca3fe867030fb`, which contains
the preceding name-method optimization and `origin/develop` at
`edf340a3ad28ce7bbc0d8d64c0e1de206bfdbf7d`. The upstream merge added unrelated
DataFrame/Parquet and MongoDB fixes. Both merged builds passed the preceding
28-file regression set and all four benchmark modes passed 330 reference checks
before this experiment. Saved release library:
`a6dda74687813883a643cb6c3c520752841d6784120c22adc902d486a8fc747a`.

After: that revision plus compiler diff
`51f520a9708143a1ba4a43d1f2293b724f4e640cdf4f1850628a8339c61a972c`,
release library
`1a7e7c5ef52a42533f55417d3ea040ef18aa8e5fae6357a94653704979f7f9e9`;
debug validation library
`f113f3940cd4d9237f4bc8600b1cb5e764417fb7a50ae263062d828030a97efa`.

M4 Pro / macOS arm64; Java Temurin 25.0.2, JMH 1.37. Five randomized forks,
three warmups, five 500 ms windows per job and three startup samples per mode.
All **1100 measured windows** are retained; no timing overlapped builds, tests,
profiles, disassembly or evidence validation. Tables show medians of fork means
in ns/element, not isolated call latency. Negative changes mean lower time.
The JSON evidence `results-method-cleanup-macos-arm64.json` contains samples,
exact identities, reproduction, compiler-toggle source, tests, optimized LLVM,
code-size/profile evidence and all 62 audit classifications.

### Before/after and Java, count 1024

Run `run-bnj70_ey`, seed 810485: 120 jobs / 600 windows.

|!Kernel|!Before ns|!After ns|!Change|!Fork ranges overlap|!Java ns|!After / Java
|optional_method_call|25.687|20.813|-18.97%|no|1.678|12.40x
|polymorphic_call|66.310|55.425|-16.41%|no|2.537|21.85x
|monomorphic_call|12.226|7.256|-40.65%|no|1.674|4.34x
|method_call|2.679|2.707|+1.05%|yes|0.889|3.05x
|indirect_wide|8.467|8.316|-1.78%|yes|1.680|4.95x
|arithmetic|2.398|2.433|+1.43%|yes|0.889|2.74x
|direct_call|2.423|2.412|-0.45%|yes|0.891|2.71x
|dispatch_control|2.555|2.579|+0.96%|no|0.967|2.67x

Nullable, polymorphic and monomorphic gains have non-overlapping ranges.
The **+0.96% dispatch-control regression also has non-overlapping ranges** and
is retained. Other small control changes overlap. Exact-base/final Qore paths
can inline arithmetic while Java retains selected calls: monomorphic 4.34x
and final-method 3.05x are workload ratios with different inlining policies,
not retained-call parity. Nullable remains **12.40x**, polymorphic **21.85x**
the Java workload. No new IR speedup is claimed.

### Longer loop, count 4096

Run `run-udn4hvpx`, seed 810486: 40 jobs / 200 windows.

|!Kernel|!Before ns|!After ns|!Change|!Fork ranges overlap
|optional_method_call|24.356|19.417|-20.28%|no
|polymorphic_call|64.616|53.438|-17.30%|no
|monomorphic_call|10.821|6.185|-42.84%|no
|arithmetic|1.321|1.327|+0.46%|yes

All three call-workload gains repeat with non-overlapping ranges. Arithmetic
moves +0.46% with overlap.

### Same-runtime compiler control, count 1024

Run `method-cleanup-toggle-rwl1rzdk`, seed 810487: 60 jobs / 300 windows.
Both sides use the candidate runtime. “Before” means the method cleanup guard
is disabled at compilation, not an old runtime; the flag is absent at execution.

|!Kernel|!Before ns|!After ns|!Change|!Fork ranges overlap
|optional_method_call|25.757|20.608|-19.99%|no
|polymorphic_call|65.699|55.751|-15.14%|no
|monomorphic_call|12.370|7.362|-40.49%|no
|arithmetic|2.387|2.376|-0.44%|yes
|direct_call|2.384|2.401|+0.72%|yes
|dispatch_control|2.553|2.568|+0.59%|yes

The compiler toggle reproduces all three gains. Dispatch control is +0.59%
with overlap; that does not erase the adverse old/new result or prove neutrality.
Direct-call and arithmetic controls overlap too. Accepting this change trades
the documented small control movement and code growth for repeatable gains
in the selected call workloads.

### Code size, compilation, startup and validation

The benchmark executable's `__text` section grows from **106116 to 110944 bytes**
(+4828 bytes, **4.55%**). Guard statistics grow from 174 to 288. Optional-method
optimized LLVM expands from 319 to 734 lines; both versions retain 27 static
decref call sites, but the new guards bypass them for exact immediate values.
Call-site counts alone do not describe dynamic work. Segment alignment masks
the text growth in whole-executable size; do not interpret file size as no growth.

Descriptive before/after compilation times in ms: 363.85/366.79 (main),
345.58/355.91 (4096), 342.43/357.69 (toggle). These are single compilations,
not a compile-time performance study. Startup medians in ms:
31.50/32.37 (main; Java 23.98), 30.17/28.99 (4096), 29.33/29.57 (toggle).
Each pair's startup ranges overlap; filesystem caches are uncontrolled.

Release and debug each pass 28 selected files: 51 QUnit cases / 3844 assertions
in 22 files, plus six standalone scripts. The method fixture now verifies that
108 owned-object returns rejected by an int assignment are all destroyed.
Its expanded driver passes two cases / 546 assertions per build and checks
method/global cleanup-disabled controls, positive/nonempty guard selection,
AST/IR/JIT/tiered, stripped O0/O3, C++ EH, parallel compilation, native-path
exclusions, arbitrary fallback returns, Program boundaries, debugger attachment
and concurrent frame readers. The expanded ownership fixture also passed saved
baseline AST/IR and stripped O3 before enabling this change. Existing mixed-scalar,
opaque-reference, local-lifetime and freshness regressions pass.

Six benchmark modes each pass 330 independent records; the saved AOT artifact
passes 330 records on the candidate, and all 11 harness tests pass. Audit:
**18 Pass, 44 N/A, 0 Fail**; see `FOLLOWUP-AUDIT.md`. Existing IR/JIT full
caller-frame reflection and zero-argument fast-frame limitations remain outside
scope, not newly passing.

A separate five-second candidate profile has 3828 main-thread samples (idle
signal thread excluded). Top self counts: guarded native helper 1056, native
frame construction 953, frame destruction 517, caller 505, value type checks
270; value discard is only 12. This supports cleanup leaving the hot path,
but sample counts are attribution, not latency or a measured percentage gain.

Next focus: full-key native admission and observable frame construction/destruction.
Investigate whether immutable checked-entry facts can remove redundant work,
and whether compiler-proven receiver/native-body facts permit narrower guarded
entrypoints. Keep mutable Program/body/debugger/receiver guards, stack checks
and concurrent frame-reader semantics; do not simply omit observable frames.


## Native method frame wrapper inlining (2026-10-08)

This runtime-only change adds explicit constructor/destructor inlining hints to
the native method diagnostic-frame wrapper, matching existing callback wrappers.
It retains the same frame publication, observer handshake, variable-stack markers,
RuntimeConfig save/restore, parse policy, stack guards and fallback checks.
Baseline is clean `c84d9fc502e8d8417317581b7874c0f3d99fb2ad`; candidate is that
revision plus the C++ diff in
[the full evidence](results-method-frame-inline-macos-arm64.json).
Measured libraries are identified by hashes, not relabelled as the later commit.

Apple M4 Pro, macOS arm64, release single-compilation-unit build, Temurin 25.0.2,
JMH 1.37. Each run uses five randomized forks, three warmups, five 500 ms sample
windows and three startup observations. Values are median fork means in
ns/element for the whole kernel, not isolated call latency. All 950 windows ran
without simultaneous builds, tests, disassembly or profiling.
`--reuse-aot-for-baseline` runs an identical AOT executable on both libraries
within each pair; this is appropriate because the compiler did not change.

Count 1024, seed 810488, `run-h6ey3mfh` (600 windows):

|!Kernel|!Before ns/element|!After ns/element|!Change|!Fork ranges overlap|!Java JIT|!Qore/Java
|optional_method_call|20.727|19.806|-4.44%|no|1.658|11.94x
|polymorphic_call|55.318|54.541|-1.41%|yes|2.517|21.67x
|monomorphic_call|7.485|7.255|-3.08%|yes|1.664|4.36x
|method_call|2.670|2.638|-1.20%|yes|0.878|3.00x
|indirect_wide|8.247|8.288|+0.49%|yes|1.670|4.96x
|arithmetic|2.378|2.366|-0.49%|yes|0.877|2.70x
|direct_call|2.379|2.384|+0.24%|yes|0.876|2.72x
|dispatch_control|2.561|2.553|-0.31%|yes|0.939|2.72x

Independent count-4096 replication, seed 810489, `run-_5hbiv36` (200 windows):

|!Kernel|!Before ns/element|!After ns/element|!Change|!Fork ranges overlap
|optional_method_call|19.256|18.406|-4.41%|no
|polymorphic_call|53.605|52.704|-1.68%|yes
|monomorphic_call|5.984|5.967|-0.27%|yes
|arithmetic|1.315|1.320|+0.42%|yes

Nullable method calls improve **4.44% and 4.41%**, with non-overlapping fork
ranges at both counts. Other call and arithmetic/control movements overlap:
no established improvement or regression is claimed for them. The nullable
kernel remains about **11.94x Java** in the count-1024 comparison; this is not
Java parity or a general IR/AOT speedup.

Negative control, count 1024, seed 810490, `run-nslvfd0x` (150 windows):
`QORE_DISABLE_AOT_NATIVE_METHOD_ENTRY=1` is set at both compilation and execution
for both libraries. This executable differs from the enabled runs but is identical
within its own before/after pair.

|!Kernel|!Before ns/element|!After ns/element|!Change|!Fork ranges overlap
|optional_method_call|140.109|139.522|-0.42%|yes
|polymorphic_call|152.329|150.539|-1.18%|yes
|arithmetic|2.388|2.366|-0.92%|yes

All disabled-entry movements overlap. Nullable movement is only -0.42%, supporting
attribution of the repeated enabled-path gain to native frame-wrapper inlining.
Startup medians before/after are 30.261/30.202 ms at 1024, 28.954/28.434 ms at 4096,
and 31.544/30.585 ms for the disabled control; all paired ranges overlap.
Java startup is 23.423 ms in the first run. The three candidate AOT compilations
took 355.193, 344.668 and 347.825 ms, respectively; no baseline compile timing
exists because the artifacts are reused. These are descriptive observations,
not startup or compilation speedup claims.

Assembly of `qore_rt_try_method_int_unary_v1` changes from three static wrapper
constructor/destructor call sites to zero. The helper keeps its 0x140-byte stack
reservation; instruction/directive rows including encoded `.long` instructions
increase from 441 to 613 as wrapper code moves into the helper. These are static
counts, not dynamic work. Runtime `__text` grows 2,564 bytes, from 13,966,516 to
13,969,080 (+0.0184%); executable text is identical within each benchmark pair.
Benefits and code generation on non-SCU builds or other targets are not established.

Release and debug each pass 28 files: 51 QUnit cases/3,844 assertions in 22 files
plus six standalones. The method continuation test includes 546 assertions,
ownership cleanup, normal/invoke, O0/O3, stripped sources, disabled/parallel
controls, concurrent frame readers and debugger coverage. Six benchmark modes
each pass 330 independent reference checks; a saved pre-change AOT executable
also passes all 330 on the candidate. All 11 harness tests pass.

A separate five-second, 1 ms sampling profile of the count-4096 nullable kernel
has 3,804 main-thread samples (idle signal thread excluded). Leading self counts:
native method helper 1,591; caller 496; local marker push 346; QoreValue type
access 258; closure marker push 181; frame unpublish 171; parse-policy override
157; stack check 93. The helper now includes formerly outlined frame code, so
its samples must not all be labelled admission cost. Sampling attributes remaining
work; it does not measure the percentage gain.

Next investigate deferred empty variable markers/private context for proven pure
native method bodies, while preserving published diagnostic frames and live-body
overflow checks. Existing callback descriptor v3 is a stable native ABI: do not
change its layout to share machinery. Method v2 already has a pure-body proof
and explicit stack guard, but compatibility and error materialization must be
audited before reusing that contract. Keep object/class context and mutable
dispatch/Program/debugger guards unless separately proven unnecessary.


## Deferred private context for native methods (2026-10-08)

This runtime-only change defers private context and empty variable markers for
the already-proven pure scalar method-v2 continuation. Dispatch-entry and live
callee stack guards, full PIC-key admission, mutable body/Program/debugger
checks, eager object/class context and published diagnostic frames remain.
On overflow, the existing materialization machinery installs both markers with
allocation rollback and applies current context/parse policy before exception
construction. Method-v1 stays eager. A private typed-union discriminator shares
the machinery without changing callback descriptor v3, method binding ABI or
serialized layouts; floating callback marker policy is unchanged.

Baseline is clean merged `1f599ed31c0898d5dabd7ac791ad7488389d3046`;
candidate is that revision plus the recorded C++ diff. Full samples, exact
library/artifact hashes, reproduction scripts and audit are in
[results-method-leaf-context-macos-arm64.json](results-method-leaf-context-macos-arm64.json).
Measured binaries must not be relabelled as later postcommit rebuilds.

Apple M4 Pro, macOS arm64, release SCU build, Temurin 25.0.2, JMH 1.37.
All runs use five randomized forks, three warmups, five 500 ms windows and three
startup observations. These are median fork means in ns/element for whole kernels,
not isolated call latency. All 1,575 windows ran without competing builds, tests,
profiling or disassembly. Before/after pairs use identical AOT executables on
compatible old/new libraries (`--reuse-aot-for-baseline`).

Count 1024, seed 810491, `run-m1y_foj8` (675 windows):

|!Kernel|!Before ns/element|!After ns/element|!Change|!Fork ranges overlap|!Java JIT|!Qore/Java
|optional_method_call|20.524|15.121|-26.33%|no|1.659|9.11x
|polymorphic_call|55.242|52.346|-5.24%|no|2.522|20.76x
|monomorphic_call|7.323|7.398|+1.03%|yes|1.658|4.46x
|method_call|2.684|2.677|-0.27%|yes|0.877|3.05x
|indirect_wide|8.364|8.287|-0.93%|yes|1.669|4.96x
|indirect_float|10.209|10.216|+0.07%|yes|1.633|6.26x
|arithmetic|2.413|2.404|-0.39%|yes|0.877|2.74x
|direct_call|2.407|2.398|-0.37%|yes|0.876|2.74x
|dispatch_control|2.583|2.574|-0.35%|yes|0.945|2.73x

Count 4096, seed 810492, `run-m1ef817f` (300 windows):

|!Kernel|!Before ns/element|!After ns/element|!Change|!Fork ranges overlap
|optional_method_call|19.111|13.897|-27.28%|no
|polymorphic_call|53.298|50.276|-5.67%|no
|monomorphic_call|5.957|5.976|+0.33%|yes
|indirect_wide|7.303|7.304|+0.02%|yes
|indirect_float|9.011|9.053|+0.46%|yes
|arithmetic|1.329|1.335|+0.43%|yes

Nullable calls improve **26.33% and 27.28%**, polymorphic calls **5.24% and 5.67%**,
with non-overlapping fork ranges in both runs. Other primary before/after movements
overlap; no improvement, regression or exact neutrality is established for them.
Nullable calls remain **9.11x Java**, polymorphic **20.76x** at count 1024.
These numbers are not general IR speedups or Java parity.

### Same-runtime eager-context attribution

`QORE_DISABLE_AOT_NATIVE_METHOD_DEFERRED_CONTEXT=1` selects eager private setup
at runtime only. The native continuation/body, diagnostic frame and guards stay
enabled. Both modes use the identical check-run executable and candidate library;
“before” here means the eager control, not the old library. The two attribution
runs use new randomized orders, each with 300 windows.

Seed 810493, `method-leaf-toggle-9xcxmi12`:

|!Kernel|!Before ns/element|!After ns/element|!Change|!Fork ranges overlap
|optional_method_call|20.195|15.172|-24.87%|no
|polymorphic_call|55.194|52.452|-4.97%|no
|monomorphic_call|7.408|7.373|-0.46%|yes
|indirect_wide|8.355|8.311|-0.52%|yes
|indirect_float|10.333|10.196|-1.33%|no
|arithmetic|2.409|2.398|-0.42%|yes

Seed 810494, `method-leaf-toggle-repeat-924m55ji`:

|!Kernel|!Before ns/element|!After ns/element|!Change|!Fork ranges overlap
|optional_method_call|20.164|15.249|-24.37%|no
|polymorphic_call|55.854|52.397|-6.19%|no
|monomorphic_call|7.407|7.269|-1.86%|yes
|indirect_wide|8.318|8.338|+0.24%|yes
|indirect_float|10.285|10.198|-0.85%|yes
|arithmetic|2.447|2.411|-1.46%|yes

Nullable gains repeat at **24.87%/24.37%**, polymorphic at **4.97%/6.19%**, all
non-overlapping. The first float control unexpectedly improves 1.33% with
non-overlapping ranges even though this method-only flag should not directly
affect that workload. Its repeat improves 0.85% with overlapping ranges, and
both primary old/new float comparisons also overlap. Retain this anomaly as
evidence of control variability; do not claim a callback speedup or attribute
every small movement to the optimization.

Startup before/after medians: 30.395/30.108 ms (1024), 31.800/29.479 ms (4096),
29.256/29.684 ms (first toggle) and 29.316/29.434 ms (repeat). Every paired range
overlaps. Java startup is 23.400 ms. Candidate compilation observations are
357.945/357.998 ms in the two primary runs, with no baseline compilation because
artifacts were reused. Toggle artifacts come from the check run; its 369.178 ms
compile observation is inherited metadata, not a new compile measurement.
No startup/compilation benefit is claimed; filesystem caches were uncontrolled.

### Correctness, size and remaining work

Release/debug each pass 29 files: 52 cases/3,904 assertions in 23 QUnit files plus
six standalones. The extended NativeLeafContext driver has 60 assertions and also
passes the unchanged baseline. It asserts actual required/nullable continuation
calls and method-v2 bindings in LLVM, sweeps early/published method overflow
boundaries, verifies recovery, and repeats with caller-owned locals and closure
stacks around marker-block boundaries. Compiled callers retain AOT execution
when newly parsed padded callers use AST mode (`evalIntern` selects cached AOT
before mode-based AST dispatch). The driver exercises O0/O3 and the runtime eager
control; the broader suite covers stripped-source normal/invoke/EH/parallel,
weak/generic/cross-Program/access/ownership cases, readers and debugger attachment.
Six benchmark modes each pass 330 reference checks; saved pre-change AOT also
passes 330 on the candidate. All 11 harness tests pass.

The private leaf helper remains **104 bytes**. The method dispatch helper's
native stack reservation grows **320 to 336 bytes**, with both eager and deferred
branches retained; static instruction/directive rows grow 613 to 715.
Runtime `__text` grows **1,388 bytes**, 13,968,504 to 13,969,892 (+0.0099%).
These static counts are not dynamic work. There is no executable text difference
within a pair. Other architectures/non-SCU configurations are not measured.

A separate five-second/1 ms nullable-kernel profile has 3,784 main-thread samples;
the idle signal thread is excluded. Leading self counts: method helper 1,784,
caller 675, value type access 425, frame unpublish 159, callee guard helper 143,
dispatch stack check 123, native method body 73 and callee-guard stub 61.
Empty-marker pushes and parse-policy writes are no longer leading hot entries.
The large method helper includes inlined admission, object/class and frame work;
it must not all be labelled TLS or dispatch lookup. Sampling is attribution,
not isolated latency or a measured percentage gain.

Next target localized type/admission overhead and the remaining stack-guard
helper boundary. Investigate an additive method companion using the existing
callback inline live-stack check contract; retain full fallback on unsupported
targets and overflow materialization. No stale stack limit, missing observer
frame, removed debugger guard or changed old-artifact ABI is justified by these
measurements.

## Native method live-stack companion (2026-10-08)

An additive method-v3 entry re-emits the existing complete pure int(int) body
with the callback-v3 live-stack view: load the current limit and current callee
SP, then execute the scalar body or tail-call method-v2's full guard. Null views
also use v2; unsupported layouts never omit the full check. The runtime retains
the dispatch-entry guard, full PIC key, receiver/access/Program/body/readiness/
debugger checks, object/class context, diagnostic publication and overflow
materialization. V1/v2 signatures, generated field offsets and serialized
metadata remain compatible; the new runtime-owned atomic is appended.
A method-only compile flag omits v3 for comparison.

Baseline is clean merged `53b60925647155d53808b417c07c4e54a72e69b9`;
candidate is that revision plus the recorded compiler/runtime/header diff.
[Full evidence](results-method-inline-stack-macos-arm64.json) includes every
sample, exact library/executable hashes, reproduction scripts, assembly and audit.
These measured precommit identities must not be relabelled as later rebuilds.

Apple M4 Pro, macOS arm64, release SCU, Temurin 25.0.2/JMH 1.37. Five randomized
forks, three warmups, five 500 ms windows and three startup observations per mode.
All 1,575 windows ran without competing builds, tests, profiling, disassembly or
evidence validation. Numbers are median fork means in ns/element for whole
kernels, not isolated call latency. Both primary comparisons compile separate
artifacts against their corresponding runtimes: v3 artifacts require a binder
absent from the baseline, so reusing one artifact across old/new is invalid.

Count 1024, seed 810501, `run-fxor7upn` (675 windows):

|!Kernel|!Before ns/element|!After ns/element|!Change|!Fork ranges overlap|!Java JIT|!Qore/Java
|optional_method_call|15.246|14.797|-2.95%|no|1.659|8.92x
|polymorphic_call|52.411|51.763|-1.24%|yes|2.523|20.52x
|monomorphic_call|7.143|7.018|-1.76%|yes|1.668|4.21x
|method_call|2.685|2.674|-0.41%|yes|0.885|3.02x
|indirect_wide|8.232|8.374|+1.72%|yes|1.666|5.03x
|indirect_float|10.226|10.255|+0.28%|yes|1.636|6.27x
|arithmetic|2.411|2.391|-0.82%|yes|0.878|2.72x
|direct_call|2.414|2.412|-0.09%|yes|0.879|2.74x
|dispatch_control|2.576|2.581|+0.20%|yes|0.954|2.70x

Count 4096, seed 810502, `run-6a5qidrc` (300 windows):

|!Kernel|!Before ns/element|!After ns/element|!Change|!Fork ranges overlap
|optional_method_call|13.888|13.490|-2.87%|yes
|polymorphic_call|50.641|50.648|+0.01%|yes
|monomorphic_call|5.750|5.685|-1.12%|yes
|indirect_wide|7.262|7.267|+0.08%|yes
|indirect_float|9.018|9.022|+0.05%|yes
|arithmetic|1.337|1.336|-0.10%|yes

Nullable-method time improves **2.95%** at 1024 with non-overlapping ranges and
**2.87%** at 4096 with overlapping ranges. The latter is consistent in median
but weaker evidence. Other primary movements overlap; neither improvement,
regression nor exact neutrality is established for them. In particular, retain
the wide-callback +1.72% movement rather than discarding an adverse control.

### Same-runtime compilation control

`QORE_DISABLE_AOT_NATIVE_METHOD_INLINE_STACK_CHECK=1` compiles method-v2 only.
Both modes use the same candidate library and freshly, separately compiled
executables; runtime flags are identical. The native continuation, frame and
guard semantics remain enabled. This tests v3 compilation without cross-library
layout changes, but does not isolate executable layout effects.

Seed 810503, `method-stack-toggle-7b2r2xxv` (300 windows):

|!Kernel|!Before ns/element|!After ns/element|!Change|!Fork ranges overlap
|optional_method_call|15.427|14.946|-3.12%|no
|polymorphic_call|52.642|51.218|-2.71%|yes
|monomorphic_call|7.415|6.985|-5.80%|no
|indirect_wide|8.192|8.329|+1.67%|yes
|indirect_float|10.250|10.209|-0.40%|yes
|arithmetic|2.405|2.401|-0.16%|yes

Seed 810504, `method-stack-toggle-repeat-ndbvt9_c` (300 windows):

|!Kernel|!Before ns/element|!After ns/element|!Change|!Fork ranges overlap
|optional_method_call|15.440|14.859|-3.76%|no
|polymorphic_call|52.014|51.949|-0.12%|yes
|monomorphic_call|7.314|7.076|-3.26%|yes
|indirect_wide|8.350|8.263|-1.04%|yes
|indirect_float|10.280|10.294|+0.14%|yes
|arithmetic|2.404|2.410|+0.25%|yes

Nullable gains repeat at **3.12% and 3.76%**, both non-overlapping. This supports
retaining v3 as a targeted nullable-method improvement. The first monomorphic
control improves 5.80% with non-overlapping ranges, but its 3.26% repeat and both
primary comparisons overlap: do not claim a general monomorphic improvement.
Polymorphic ranges overlap throughout. Wide-callback adverse movements overlap
and reverse in the repeat; no callback speedup or proven regression is claimed.

Startup before/after medians: 31.920/29.189 ms (primary 1024),
30.155/29.899 ms (primary 4096), 29.162/29.375 ms (control),
28.680/28.720 ms (repeat). Only the first pair has non-overlapping ranges; the
difference does not replicate, so no startup improvement is attributed. Java
startup is 23.059 ms. Before/after compile observations: 375.208/360.227 ms,
370.453/371.815 ms, 358.969/359.443 ms, 353.395/363.165 ms. These are single
observations, not evidence of compilation improvement; filesystem caches were
uncontrolled.

### Correctness, code size and remaining cost

Release/debug each pass 29 files: 52 cases/3,964 assertions in 23 QUnit files and
six standalones. NativeLeafContext now has 120 driver assertions and inspects
actual v3 binding/current-SP/live-limit/cold-v2 LLVM. It independently tests
method/callback compilation controls, O0/O3, and eager/deferred context.
The original external sweep found no published overflow in the smaller v3 frame:
the dispatch full guard reserves 48 bytes while this v3 guard uses 16.
The strengthened fixture retains early/success and full/null-view boundary
sweeps and delegates to the actual compiler-bound entry with a null view or a
live limit changed after publication. It verifies published method/file
diagnostics, recovery and caller-owned AST locals/captures. Test-only pointer/
limit overrides restore on exit; production has no guard bypass.

Broader regressions retain source-stripped normal/invoke/EH, parallel compilation,
ownership/return contracts, weak/generic/deleted/foreign-Program/access negatives,
concurrent readers and debugger events. Six benchmark modes pass 330 independent
references each, saved old AOT passes 330 on the candidate, and 11 harness tests
pass. Internal fixture dependency includes use CPATH where required.

Runtime `__text` grows 1,232 bytes (13,969,884 to 13,971,116, +0.0088%).
The check-pair AOT `__text` grows 288 bytes (110,944 to 111,232).
Dispatch helper stack reservation grows 336 to 352 bytes; instruction/directive
rows grow 715 to 743. The new callee has a live load/compare and a cold branch to
v2, without a full helper call on success. These are static code observations,
not dynamic work counts. Other architectures and non-SCU performance are unmeasured.

A separate five-second/1 ms nullable profile has 3,837 main-thread samples, excluding
the idle signal thread. Leading self counts: dispatch helper 1,963, caller 686,
QoreValue::getType 408, entry checkStack 145, frame unpublish 140 and native-v3
body 135. The old callee qore_rt_check_stack_v2/stub is no longer a leading hot
entry. The dispatch helper still includes inlined admission/context/frame work;
do not label all of it TLS or lookup.

Nullable remains **8.92x Java JIT** and polymorphic **20.52x** in the 1024 run.
Next investigate localized strong-object tag admission to avoid generic type
dispatch on the positive method path, while retaining enum/weak/generic fallback
and every access/Program/body/debugger guard. This is not general IR improvement
or Java retained-call parity.

## Local strong-object method admission (2026-10-08)

The native method continuation now checks the borrowed QoreValue pointer tag and
node type locally instead of calling general type dispatch and a generic object
accessor. Opaque strong references remain supported. Weak, scalar, enum and
non-object representations take the same ordinary miss path. Object validity,
access, the full PIC key, Program/body/readiness/debugger checks, object/class
context, diagnostic publication and live stack guards remain unchanged.
There is no compiler, public type API, ABI or serialized-format change.

Baseline is clean merged `29741785c34d49d8b2bdf396e4016f5160d3c441`,
including method-v3 and `origin/develop` at `4460f705b`. Candidate adds only the
recorded JITRuntime.cpp production diff. [Full evidence](results-method-receiver-tag-macos-arm64.json)
contains all samples, identities, assembly, profiling, reproduction and audit.
Measured precommit identities must not be relabelled as later rebuilds.

Apple M4 Pro, macOS arm64, release SCU, Temurin 25.0.2/JMH 1.37.
Five randomized forks, three warmups, five 500 ms windows and three startup
observations per mode. Each before/after pair uses the identical AOT executable
with the corresponding runtime library; compiler/runtime ABI identity is unchanged.
All 1,050 windows ran without competing builds, tests, profiling, disassembly or
evidence validation. Values are median fork means in ns/element for whole kernels,
not isolated call latency. Fork range overlap is descriptive, not a significance test.

Count 1024, seed 810601, `run-tw567pd_` (450 windows):

|!Kernel|!Before ns/element|!After ns/element|!Change|!Fork ranges overlap|!Java JIT|!Qore/Java
|optional_method_call|14.762|13.616|-7.77%|no|1.661|8.20x
|polymorphic_call|52.217|50.426|-3.43%|no|2.529|19.94x
|monomorphic_call|7.139|7.078|-0.86%|yes|1.667|4.25x
|indirect_wide|8.317|8.378|+0.73%|yes|1.684|4.97x
|indirect_float|10.226|10.216|-0.10%|yes|1.643|6.22x
|arithmetic|2.414|2.408|-0.25%|yes|0.895|2.69x

Count 4096, seed 810602, `run-i0q7z07s` (300 windows):

|!Kernel|!Before ns/element|!After ns/element|!Change|!Fork ranges overlap
|optional_method_call|13.617|12.216|-10.29%|no
|polymorphic_call|50.554|48.786|-3.50%|no
|monomorphic_call|5.828|5.779|-0.82%|yes
|indirect_wide|7.164|7.273|+1.52%|yes
|indirect_float|9.071|9.064|-0.08%|yes
|arithmetic|1.345|1.344|-0.07%|yes

Repeat count 1024, seed 810603, `run-pz1zr0nc` (300 windows):

|!Kernel|!Before ns/element|!After ns/element|!Change|!Fork ranges overlap
|optional_method_call|14.776|13.479|-8.78%|no
|polymorphic_call|52.004|50.541|-2.81%|no
|monomorphic_call|7.189|7.138|-0.71%|yes
|indirect_wide|8.273|8.379|+1.27%|yes
|indirect_float|10.236|10.258|+0.21%|yes
|arithmetic|2.394|2.388|-0.27%|yes

Nullable methods improve **7.77%, 10.29% and 8.78%**; polymorphic calls improve
**3.43%, 3.50% and 2.81%**. All six target comparisons have non-overlapping fork
ranges. Retain this small runtime-only change on that repeated evidence.
All other comparisons overlap: do not claim improvement, regression or exact
neutrality. In particular, wide-callback medians move adversely by 0.73%, 1.52%
and 1.27%; these overlapping controls are retained, not discarded.

Startup before/after medians are 30.700/29.951 ms, 30.026/29.815 ms and
30.843/32.605 ms; all ranges overlap. Java startup is 23.867 ms. No startup
improvement is claimed. AOT compilation observations are 361.365, 360.058 and
361.242 ms; each artifact is reused for the baseline rather than recompiled.
These single observations and uncontrolled filesystem caches do not establish
compilation performance.

### Correctness, code size and remaining work

Release/debug each pass 33 files: 67 cases/4,038 assertions in 25 QUnit files and
eight standalones. The native driver retains 120 assertions across O0/O3,
independent method/callback compilation controls and eager/deferred runtime
context. New native probes warm real required/nullable method caches, require
actual positive admission for strong/opaque objects, and reject weak wrappers,
NOTHING, NULL, bool, integer, float, short/long string, boxed integer/float,
list, hash and enum values. They verify unchanged result/sink/caller context
on misses and successful reuse afterward, on main and re-registered foreign threads.
The final fixture passes on both baseline builds before the runtime edit.

An initial host-level probe correctly missed because the compiled cache's source
parse policy differs from the host policy. The final test enters through a
compiled caller and builtin callback using the real policy; it does not overwrite
keys, policy or TLS to force admission. An exported Program lookup accesses the
documented internal reflection view. No production test bypass was introduced.

Six benchmark modes pass 330 independent references each; baseline/candidate AOT
share one checked executable. Eleven harness tests pass. Broader ownership,
weak/generic/foreign-Program/access, source-stripped EH, debugger/readers, module
context and stack regressions remain covered. Audit-changes: 18 Pass, 44 N/A,
zero failures. Other architectures and non-SCU performance remain unmeasured.

Runtime `__text` grows 60 bytes (13,974,324 to 13,974,384); total segment size
is unchanged. The helper grows 743 to 757 AArch64 instructions (2,972 to 3,028
bytes), while stack reservation stays 352 bytes. Its two generic type/accessor
calls disappear. These static counts are not dynamic work counts.

A separate five-second/1 ms nullable profile has 3,786 main-thread samples,
excluding the idle signal thread. Self counts: dispatch helper 2,388, generated
caller 700, frame unpublish 222, entry checkStack 155 and native-v3 body 106.
General getType has only ten samples elsewhere, none under the hot continuation.
Inlined admission/context/frame work remains in the helper; it must not all be
attributed to TLS or method lookup.

Nullable remains **8.20x Java JIT**, polymorphic **19.94x**, mandatory-class
monomorphic **4.25x**, wide callback **4.97x** and float callback **6.22x** in
the primary run. This is targeted progress, not Java parity or general IR speedup.
Next investigate compile-proven stable nullable receiver/callsite preparation
within an invocation. Any hoisting must retain weak/opaque/NOTHING semantics,
local mutation, dispatch epoch, access, Program/debugger, ownership, diagnostic
publication and live stack protection, with a fresh committed baseline and
before/after measurements.

## Native method argument-key proof rejected (2026-10-08)

A candidate proved the exact unary ordinary-int dispatch argument key when
publishing native method admission, then replaced repeated arity/type/state
checks with one immutable flag. A separate flag was necessary: int-backed enums
also use the existing generic native-method cache and must not lose that path.
All receiver/access/context/Program/parse-policy/epoch matching and live
body/readiness/debugger/stack guards remained.

**Rejected and reverted.** Two workload sizes did not establish a target
speedup, so smaller code did not justify additional cache state. Only the
regression tests and negative evidence are retained; current production code
still performs the original full-key checks.

Baseline is clean committed `afb78b7250a82116f46f96b434b7b44bed97d9e8`.
[Full evidence](results-method-argument-key-macos-arm64.json) preserves the
reverted production diff, exact precommit runtime identities, every sample,
assembly, tests and audit. The internal cache header changed the AOT fingerprint,
so each runtime compiled its own matching executable. Old standalone AOT
compatibility was separately checked; stale qmods correctly used source fallback.

Apple M4 Pro, macOS arm64, release SCU, Temurin 25.0.2/JMH 1.37.
Five randomized forks, three warmups, five 500 ms windows and three startup
observations per mode. All 750 timing windows ran without competing builds,
tests, profiling, disassembly or evidence validation. Values are median fork
means in ns/element for whole kernels, not isolated call latency. Range overlap
is descriptive rather than a statistical significance test.

Count 1024, seed 810701, `run-6kdvn1li` (450 windows):

|!Kernel|!Before ns/element|!Candidate ns/element|!Change|!Fork ranges overlap|!Java JIT
|optional_method_call|13.560|13.540|-0.15%|yes|1.660
|polymorphic_call|51.610|50.942|-1.29%|yes|2.523
|monomorphic_call|7.117|7.318|+2.83%|yes|1.669
|indirect_wide|8.496|8.333|-1.92%|yes|1.670
|indirect_float|10.193|10.187|-0.05%|yes|1.647
|arithmetic|2.417|2.404|-0.56%|yes|0.896

Count 4096, seed 810702, `run-yh0yx3ag` (300 windows):

|!Kernel|!Before ns/element|!Candidate ns/element|!Change|!Fork ranges overlap
|optional_method_call|12.188|12.186|-0.02%|yes
|polymorphic_call|49.082|49.487|+0.82%|yes
|monomorphic_call|5.844|5.846|+0.03%|yes
|indirect_wide|7.384|7.195|-2.56%|no
|indirect_float|9.093|9.134|+0.46%|yes
|arithmetic|1.350|1.349|-0.09%|yes

Nullable changes are -0.15%/-0.02%; polymorphic -1.29%/+0.82%. All target ranges
overlap, and polymorphic direction reverses. No target improvement or exact
neutrality is established. The wide-callback control improves 2.56% at 4096
with narrowly non-overlapping ranges, but does not exercise the changed method
admission path; retain this observation without attributing it to the candidate.
The 1024 monomorphic adverse movement of 2.83% also overlaps. No third batch or
new profile was run after these two target-negative results.

Startup before/candidate medians are 30.283/29.797 ms and 29.801/29.682 ms;
both ranges overlap. Java startup is 23.520 ms. Compile observations are
361.737/358.575 ms and 370.693/373.354 ms, single observations with uncontrolled
filesystem caches. No startup or compilation improvement is claimed.

The candidate reduced the dispatch helper from 757 to 725 AArch64 instructions
(3,028 to 2,900 bytes) and its stack reservation from 352 to 336 bytes. Runtime
`__text` shrank by 108 bytes (13,974,384 to 13,974,276); total segment size was
unchanged. The immutable cache entry grew from 208 to 216 bytes. These static
changes were real but did not yield a measured target benefit; all are reverted.

The retained native fixture warms real required/nullable callers with an
int-backed enum first. It verifies the actual enum argument key and requires the
integer helper to miss without changing result/sink/context. It then warms an
ordinary-int entry in the second PIC slot, requires actual native admission
past the first enum entry, and verifies enum dispatch plus integer reuse.
The shared cache-slot lookup retains cancellation checks. No test changes cache
keys, parse policy, TLS or production guards to force admission.

Expanded fixtures pass on both baseline builds and the candidate. Candidate
release/debug each pass 33 files: 67 cases/4,038 assertions in 25 QUnit files
plus eight standalones. The driver retains 120 assertions across O0/O3,
independent compilation controls, eager/deferred context and foreign-thread
re-registration; native assertions cover the new key probes. Six benchmark modes
pass 330 references each, the saved older standalone passes 330, and 11 harness
tests pass. After restoration, the expanded release/debug fixture passes again
with 120 driver assertions each, and four benchmark modes pass 330 references
each. The release library is byte-identical to the baseline; the restored debug
rebuild's distinct hash is recorded separately. No runtime/header diff remains.

This experiment makes no new speedup or Java-parity claim. Next investigate the
remaining object/class context traffic for whole-body-proven pure native methods,
preserving deferred error materialization and diagnostic readers, before a larger
invocation-local prepared-method ABI. Existing mutable guards and ownership
semantics remain requirements, not candidates for unchecked removal.

## Deferred object/class context for native methods (2026-10-08)

Retain the runtime-only deferral of object/class substitution for whole-body-proven
pure method-v2/v3 continuations. Three before/after batches reduce nullable-method
kernel time by **10.97%, 12.40% and 10.99%**, with non-overlapping fork ranges.
The eager-context negative control overlaps at -0.53%. This is targeted progress,
not Java parity or a general speedup for all methods.

Before this experiment, audited argument-key regression/negative evidence was
committed as `a99cf6341`, then origin was fetched and develop `1369d1bd5` was
merged cleanly as `f7945809656cda045c89424331eaabb39e296184`. Both builds and affected
Qore modules were rebuilt; the merged runtime is the fresh saved baseline.
[Full evidence](results-method-object-context-macos-arm64.json) records exact
precommit library/compiler identities, the runtime diff, all samples, assembly,
proof review, tests and the 62-item audit. A subsequent committed rebuild has a
new identity and is not substituted for either measured binary.

Apple M4 Pro, macOS arm64, release SCU, Temurin 25.0.2/JMH 1.37. Five randomized
forks, three warmups, five 500 ms windows and three startup observations per mode.
All **1,250 timing windows** ran without competing builds, tests, profiling,
disassembly or evidence validation. The first benchmark survived a tool-session
change; its original live process was verified and monitored, not restarted.
Each pair used one identical AOT executable on both libraries. Values are median
fork means in ns/element for entire kernels, not isolated call latency. Range
overlap is descriptive, not a statistical significance test.

Primary count 1024, seed 810801, `run-8ur6s_ap` (450 windows):

|!Kernel|!Before ns/element|!After ns/element|!Change|!Fork ranges overlap|!Java JIT
|optional_method_call|13.442|11.967|-10.97%|no|1.662
|polymorphic_call|50.383|49.933|-0.89%|yes|2.525
|monomorphic_call|7.052|7.141|+1.27%|yes|1.671
|indirect_wide|8.254|8.318|+0.76%|yes|1.674
|indirect_float|10.265|10.193|-0.70%|yes|1.636
|arithmetic|2.412|2.421|+0.35%|yes|0.878

Primary count 4096, seed 810802, `run-51dqhqsf` (300 windows):

|!Kernel|!Before ns/element|!After ns/element|!Change|!Fork ranges overlap
|optional_method_call|12.187|10.677|-12.40%|no
|polymorphic_call|49.224|48.256|-1.97%|no
|monomorphic_call|5.763|5.890|+2.21%|yes
|indirect_wide|7.210|7.184|-0.35%|yes
|indirect_float|8.981|9.063|+0.91%|yes
|arithmetic|1.349|1.349|-0.01%|yes

Repeat count 1024, seed 810803, `run-crmfidye` (300 windows):

|!Kernel|!Before ns/element|!After ns/element|!Change|!Fork ranges overlap
|optional_method_call|13.467|11.987|-10.99%|no
|polymorphic_call|50.570|50.185|-0.76%|yes
|monomorphic_call|7.080|7.172|+1.30%|yes
|indirect_wide|8.278|8.272|-0.07%|yes
|indirect_float|10.277|10.234|-0.42%|yes
|arithmetic|2.392|2.417|+1.03%|yes

The nullable result repeats at both sizes, saving roughly 1.5 ns/element.
Polymorphic movement is -0.89%/-1.97%/-0.76%; only the larger workload has
non-overlapping ranges, so a repeatable general polymorphic gain is not claimed.
Monomorphic adverse movement (+1.27%/+2.21%/+1.30%) is retained; all ranges overlap.
Callback and arithmetic controls also overlap. This does not prove exact
neutrality or rule out smaller code-layout effects.

Eager-context control, count 1024, seed 810804, `run-o3v_ti4l` (200 windows),
sets `QORE_DISABLE_AOT_NATIVE_METHOD_DEFERRED_CONTEXT=1` for compilation and
execution of both libraries. Native dispatch/body remain enabled, while both
use eager object/class and private-context setup. All control ranges overlap:

|!Kernel|!Before ns/element|!After ns/element|!Change|!Fork ranges overlap
|optional_method_call|18.991|18.890|-0.53%|yes
|polymorphic_call|54.030|54.345|+0.58%|yes
|monomorphic_call|7.136|7.145|+0.12%|yes
|arithmetic|2.409|2.385|-1.01%|yes

The repeated nullable improvement largely disappears in this control, supporting
the intended attribution. The control artifact differs from enabled batches but
is identical within its own before/after pair. No fresh profile was collected.

Startup before/after medians are 31.493/31.205, 30.590/30.501, 30.097/30.044 and
29.458/29.616 ms; all ranges overlap. Java startup is 23.649 ms. Each shared-artifact
pair has only one compile observation, not a before/after compilation comparison.
Filesystem caches are uncontrolled; no startup or compilation gain is claimed.

Implementation and tradeoff:

- The immutable cache, borrowed receiver and initialized live RuntimeConfig are
  held in a method-only invocation descriptor, which outlives frame unpublication
  and reader completion. No extra object reference delays destruction.
- Successful pure calls leave both live object/class and RuntimeConfig snapshot
  untouched. The callee's failing full stack guard materializes both independently
  before exception construction; cleanup restores their distinct previous values.
- Existing marker-allocation rollback, diagnostic publication/reader handshake,
  receiver/access/full-PIC-key/epoch/Program/body/readiness/debugger guards and
  live stack protection remain. Method-v1, ordinary methods and eager control
  keep eager substitution. Callback helper layout and successful cleanup are unchanged.
- The dispatch helper shrinks from 757 to 751 AArch64 instructions, but its stack
  reservation grows **352 to 400 bytes**. Runtime `__text` grows 360 bytes
  (13,974,800 to 13,975,160); total segment size stays 21,774,336. The repeated
  target saving justifies this bounded method-only tradeoff on the measured build;
  other architectures and non-SCU builds remain unmeasured.

The expanded native fixture passes on unchanged merged baseline and candidate,
release/debug: it checks snapshot receiver/defining-class context after an actual
published callee overflow, and independently reads a private member through the
live object/class path. A distinct caller snapshot survives success, enum-key
miss and materialized failure; recovery, O0/O3, independent full-guard controls,
eager context, padded caller local/closure stacks and foreign re-registration
remain covered. Artificial stack limits are restored before error inspection.

Baseline and candidate release/debug each pass 33 selected files: 67 cases and
4,038 assertions in 25 QUnit files plus eight standalones. The native driver has
120 assertions plus the native C++ probes. Six benchmark modes pass 330 references
each; the saved baseline executable also passes 330 on the candidate. Eleven
harness tests pass. After the merge, local-only Bedrock streaming, LLM cancellation
and LLM streaming pass 64 cases/387 assertions; live-service testing was disabled.
No database tests or external inference requests were run.

In the primary run, nullable remains **7.20x Java JIT**, polymorphic **19.78x**,
monomorphic **4.27x**, wide callback **4.97x**, and float callback **6.23x**.
Next investigate invocation-local preparation for stable nullable receivers,
reusing the existing strong-local mutation-epoch safeguards where applicable.
Hoisting must retain NOTHING/weak/opaque handling, reflective writes, receiver
lifetime, access and Program checks, dispatch/body changes, debugger observation
and stack diagnostics. Do not replace those semantics with an unchecked pointer
cache merely to reduce the benchmark number.

## Direct-node receiver validity (2026-10-08)

Retain the runtime-only specialization in `qore_rt_object_is_valid`. Baseline is
the clean committed rebuild of `97d69f3131222182ac18bfbbbb8305fd535512e0`, after
merging `origin/develop` at `1369d1bd538f1c25c04ee5c4db278c3c2d5b90f6`.
Baseline release SHA-256 is
`aaa4a31c00aa2dbe574ed1ca9d3f506810ca661884c90018e7de57d64f2c6b4e`;
the measured precommit candidate is
`05367116ebc76288a0820ce03e5eef24795138df647e858f3ef7bf6604dc3727`.
Neither is replaced by a subsequent committed rebuild in the evidence.
[Full evidence](results-receiver-validity-macos-arm64.json) records the exact
source diff, binary identities, raw windows, recomputed statistics and audit.

Apple M4 Pro, macOS arm64, release SCU, Temurin 25.0.2/JMH 1.37. Each run uses
five randomized forks, three warmups, five 500 ms samples and three startup
observations per mode. All 750 measurement windows ran without competing task
builds, tests, profiling, disassembly or evidence validation. Intermittent macOS
backup/background activity was observed before timing and was not stopped;
host activity and filesystem caches remain uncontrolled. Each before/after pair
runs the identical AOT executable, using unchanged tools and helper ABI.

Values are median fork means in ns/element for entire kernels, not isolated
call latency. Range overlap is descriptive, not a significance test.

Count 1024, seed 810805, `run-19cpr6k5`
(450 windows):

|!Kernel|!Before ns/element|!After ns/element|!Change|!Fork ranges overlap|!Java JIT
|monomorphic_call|7.139|5.282|-26.01%|no|1.663
|method_call|2.690|2.687|-0.09%|yes|0.880
|optional_method_call|11.968|11.989|+0.18%|yes|1.661
|polymorphic_call|50.336|49.273|-2.11%|no|2.525
|indirect_wide|8.403|8.353|-0.59%|yes|1.673
|arithmetic|2.421|2.412|-0.36%|yes|0.882

Count 4096, seed 810806, `run-1o6o9w06`
(300 windows):

|!Kernel|!Before ns/element|!After ns/element|!Change|!Fork ranges overlap
|monomorphic_call|5.847|3.996|-31.65%|no
|method_call|1.423|1.422|-0.08%|yes
|optional_method_call|10.534|10.706|+1.63%|yes
|polymorphic_call|48.026|48.004|-0.05%|yes
|indirect_wide|7.149|7.224|+1.04%|yes
|arithmetic|1.342|1.345|+0.21%|yes

Monomorphic improves 26.01% and 31.65%, saving 1.857 and 1.851 ns/element with
non-overlapping fork ranges at both sizes. Its primary ratio is now **3.18x Java
JIT** (5.282 versus 1.663 ns/element). Qore may inline the exact-base method
arithmetic, whereas Java's DynamicStep targets retain `DONT_INLINE`; this is
not an equal-inlining-policy comparison or proof of native-call parity.

Polymorphic improves 2.11% with non-overlapping ranges at 1024, but only 0.05%
with overlapping ranges at 4096: no repeated general polymorphic benefit is
claimed. All remaining controls overlap. Preserve the adverse optional-method
movement (+0.18%/+1.63%) and wide-callback movement (-0.59%/+1.04%); overlap does
not prove exact neutrality. Nullable remains 7.22x Java and polymorphic 19.51x
in the primary run. No Java timing was collected at 4096 in this experiment.

Startup before/after medians are 31.481/31.266 and 29.896/30.673 ms, with
overlapping ranges; Java startup is 24.473 ms. Each pair has only one compile
observation (359.34/365.19 ms for the separate runs), not a before/after compile
comparison. No startup or compilation improvement is claimed.

Implementation and validation:

- Strong and opaque pointer values inspect the node tag directly, then perform
  the same live deletion-status test. Null pointers and non-object nodes fail;
  non-pointer values retain the exact generic expression, including enums.
  No extra object owner, cached validity, new shared state, exported symbol,
  context/dispatch guard removal or compiler change is introduced.
- The strong-object path avoids two out-of-line generic accessor calls. The
  complete helper grows from 21 to 35 AArch64 instructions (84 to 140 bytes),
  with unchanged 32-byte stack reservation. Runtime text grows 56 bytes,
  13,975,160 to 13,975,216; total segment size remains 21,774,336 bytes. No new
  profile was collected; other architectures and non-SCU builds are unmeasured.
- The native fixture checks the helper against the original generic expression
  for strong/opaque objects, weak wrappers, NOTHING, NULL, inline/boxed scalars,
  strings, containers and an int-backed enum. It then deletes the still-owned
  object and requires ordinary/opaque/weak probes to reject it. Error/invalid
  inputs are correctness checks, not timed workloads.
- The expanded fixture passes on unchanged baseline and candidate, release and
  debug, under O0/O3, independent stack-guard controls and eager/deferred context.
  Candidate release/debug each pass 33 selected files: 25 QUnit files with
  67 cases/4,038 assertions, plus eight standalones. The native driver has
  120 assertions plus C++ probes, including foreign-thread re-registration.
- All six candidate/baseline/Java modes pass 330 independent references each.
  The saved baseline AOT executable also passes all 330 against the candidate;
  all 11 benchmark-harness tests pass. No external-service tests were needed.

Next: preserve this measured reduction while investigating invocation-local
preparation for stable nullable receivers. Parameter mutation-epoch caching
already covers optional classes; the remaining target is dispatch/admission,
not a duplicate local cache. Separately compare polymorphic dispatch with its
receiver-selection control before attributing its much larger gap to calls.

## Prepared method entry rejected (2026-10-09)

An invocation-local descriptor for stable required/nullable receiver parameters
did not improve execution time and was removed. The experiment reused the
caller's existing receiver owner and cached an immutable method-PIC entry;
live receiver, Program, parse-policy, dispatch-epoch, debugger, publication and
stack checks remained. It added a 16-byte descriptor and increased the unary
method helper's AArch64 stack reservation from 400 to 416 bytes.

All four runs used five randomized forks, three warmups and five 500 ms samples
per fork, with five startup observations. The 135 jobs / 675 timing windows ran
without concurrent task builds, tests, profiles or evidence computation.
Baseline was the saved committed `cbc9bfddae5b0039ef63c2efcc2246c299e06eba`
runtime. Every comparison compiled separate AOT executables. Values below are
median fork means for whole kernels, in ns/element, not isolated call latency.
[Evidence](results-prepared-method-rejected-macos-arm64.json) preserves exact
precommit binary identities, raw windows, statistics and all controls.

The initial implementation omitted recognition of the new helper in the
compiler's scalar-cleanup guard. At count 1024, seed 810807 (`run-wreqnh_5`),
nullable calls regressed from 12.031 to 17.258 ns/element (+43.45%) and
monomorphic calls from 5.328 to 10.280 (+92.95%), both non-overlapping fork
ranges. Polymorphic, wide callback and arithmetic controls moved +0.73%, +1.42%
and +0.06%, all overlapping. Java measured 1.673/1.678 ns/element for nullable/
monomorphic in this run; the rejected candidate does not redefine current Qore
performance. A same-library compiler-off control, seed 810808 (`run-9j8abnpq`),
reproduced the nullable +42.91% and monomorphic +94.23% regressions.

After correcting cleanup recognition, same-library compiler-off comparisons
still showed no benefit:

|!Count / seed / run|!Kernel|!Unprepared|!Prepared|!Change|!Fork ranges overlap
|1024 / 810809 / run-l_qy_c23|optional_method_call|12.044|12.131|+0.72%|yes
|1024 / 810809 / run-l_qy_c23|monomorphic_call|5.200|5.224|+0.45%|yes
|4096 / 810810 / run-vo3mhzks|optional_method_call|10.693|10.909|+2.02%|no
|4096 / 810810 / run-vo3mhzks|monomorphic_call|3.897|3.925|+0.73%|yes

Range overlap is descriptive, not a significance test. No Java timing was
collected for the corrected candidate, and no startup/compilation improvement
is claimed. Replacing PIC lookup with the descriptor did not pay for its
additional state and guards in these workloads. No prepared-method helper,
compiler flag, descriptor ABI or special cleanup case is retained.

The initial ownership-fixed candidate passed 35-file release/debug matrices;
the corrected candidate passed the expanded name-method suite (3 cases, 700
assertions) and native suite (120 driver assertions plus C++ checks) in both
builds. Its focused cleanup test failed on the frozen initial compiler and
passed after recognition was fixed. Both candidates passed six-mode reference
checks (330 per mode); all timing samples/statistics and artifact hashes were
independently checked. These are rejected-candidate results, not verification
of a later committed rebuild.

### Retained PC-location ownership fix

Expanded native stress tests exposed an independent lifetime bug: the
process-wide native-PC registry borrowed locations and strings from destructible
Program contexts and accumulated equal-base ranges on repeated registration.
An exception could read a destroyed prior Program's descriptor. The focused
`AOTPcLocationLifetime` test independently fails on the saved committed baseline
by overwriting still-allocated backing strings; it does not rely on allocator
reuse or dereferencing freed memory.

The registry now interns immutable location/string snapshots under its existing
lock and stores one range per native function base. Readers can retain borrowed
snapshot pointers after unlocking, even when another Program replaces a range.
Unknown symbol bounds are recomputed from the current next range. Like loaded
AOT images and the trailer cache, interned snapshots live until process exit;
equal values deduplicate across Programs. No normal-call guard or compiler ABI
changes are retained. Tests cover O0/O3, lazy/eager attachment, repeated Program
destruction, concurrent foreign-thread registration, ordinary and inline
descriptors, and independent backing-string ownership. Native continuation
tests also retain useful receiver-replacement, readiness and epoch-invalidation
coverage against the existing v1 helper.

With all prepared code removed, release/debug each pass 35 files (27 QUnit
files, 72 cases, 4,070 assertions plus eight standalones), six benchmark modes
pass 330 references each, and 11 harness tests pass. The retained-only precommit
release library is `7e364f910b1a7ea94c2875c43450a0100af8a2cf5c6f4382033684038ffbecb9`;
this is distinct from both rejected candidates. Identical-artifact comparisons
against the saved committed baseline add 200 quiet timing windows:

|!Count / seed / run|!Kernel|!Before|!After|!Change|!Fork ranges overlap
|1024 / 810811 / run-budwpdbx|optional_method_call|12.016|11.934|-0.69%|yes
|1024 / 810811 / run-budwpdbx|monomorphic_call|5.324|5.290|-0.64%|yes
|4096 / 810812 / run-6ddkmnav|optional_method_call|10.635|10.651|+0.16%|yes
|4096 / 810812 / run-6ddkmnav|monomorphic_call|4.033|3.922|-2.75%|yes

All ranges overlap; no execution or startup gain is claimed. Startup medians
before/after are 31.309/32.248 ms and 30.875/30.471 ms, also overlapping. Full
retained-only identities, raw samples, verification and the 62-check audit are
separate from the rejected candidates in the evidence.

Next experiment: examine per-call stack-check and diagnostic-frame costs.
Separate cold overflow materialization/exception construction from the hot
native stack-limit comparison, then inspect generated assembly and measure
before/after. Preserve native/RSE limits, pending-context materialization,
diagnostic publication and all existing call semantics. This is a hypothesis,
not a promised gain or Java-parity claim.

## Cold stack-check paths rejected (2026-10-09)

Neither tested cold-overflow variant is retained. Baseline is the committed
merged runtime at `688d20bb18a1ced15dee6ce262cb918d115d26ca`, including
`origin/develop` at `2afa62e5b2172c47f214ec6294b17933931f2a9f`. Release SHA-256:
`848219dc4e27e557ea209ef36476c8f08b160010802da06b088fab5a3b1c6e69`.
[Full evidence](results-stack-check-outline-macos-arm64.json) preserves both
uncommitted candidates, their source patches, raw samples and validation limits.

The first candidate moved deferred-context materialization and exception
construction into private cold, non-inlined helpers, retaining the native/RSE
limit comparisons and diagnostics. Clang then inlined the smaller native stack
check into dispatch helpers. Its standalone check shrank from 35 to 16 AArch64
instructions, with no stack reservation on success (previously 48 bytes).
However, the unary method helper grew from 751 to 764 instructions and from a
400- to 416-byte frame; runtime text increased by 6,416 bytes. Total segment
size stayed 21,790,720 bytes. No ABI or compiler-output change was needed.

Five randomized forks, three warmups, five 500 ms samples per fork, five startup
observations; identical AOT executable per before/after pair. All 1,050 windows
across the three runs were collected without concurrent task builds, tests,
profiles or evidence computation. Values are median fork means in ns/element
for complete kernels. Range overlap is descriptive, not a significance test.

Automatic-inlining candidate, count 1024, seed 810813, `run-97sdcjnc`:

|!Kernel|!Before|!After|!Change|!Fork ranges overlap|!Java JIT
|optional_method_call|12.089|11.863|-1.87%|yes|1.689
|monomorphic_call|5.341|5.284|-1.08%|yes|1.683
|polymorphic_call|49.478|50.211|+1.48%|yes|2.566
|indirect_wide|8.497|8.220|-3.26%|yes|1.689
|indirect_float|10.378|10.399|+0.20%|yes|1.665
|arithmetic|2.485|2.461|-0.97%|yes|0.941

Same candidate, count 4096, seed 810814, `run-u5inh_a_`:

|!Kernel|!Before|!After|!Change|!Fork ranges overlap
|optional_method_call|10.773|10.649|-1.15%|yes
|monomorphic_call|3.928|3.991|+1.61%|yes
|polymorphic_call|48.600|48.978|+0.78%|yes
|indirect_wide|7.215|7.105|-1.52%|yes
|indirect_float|9.213|9.187|-0.28%|yes
|arithmetic|1.383|1.389|+0.44%|yes

The second candidate additionally kept `RuntimeConfigNativeAccess::checkStack`
out of line. This restored the method helper's 400-byte frame (757 instructions)
while retaining the compact stack check. Runtime text was 5,668 bytes above
baseline, with unchanged total segment size. Its release-only screen at count
1024, seed 810815 (`run-_59oe0if`), did not resolve the mixed result:

|!Kernel|!Before|!After|!Change|!Fork ranges overlap
|optional_method_call|12.186|12.049|-1.13%|no
|monomorphic_call|5.459|5.474|+0.27%|yes
|polymorphic_call|50.208|50.828|+1.23%|yes
|indirect_wide|8.501|8.648|+1.72%|yes
|indirect_float|10.441|10.480|+0.37%|yes
|arithmetic|2.506|2.468|-1.49%|yes

Its small nullable improvement is recorded, not discarded, but the adverse
callback/polymorphic movement and incomplete debug validation do not justify
retention. No 4096 repeat or new Java timing was collected for this variant.
Neither variant establishes a broad speedup, startup gain or Java parity.

Validation and retained work:

- First candidate: release/debug each pass 35 selected files (27 QUnit files,
  72 cases, 4,073 assertions plus eight standalones), six modes with 330
  references each, 207 exception-location assertions and 11 harness tests.
- Second candidate: seven targeted release files and six-mode references pass.
  Debug native stack and compiled stack tests pass, but `NativeLeafContext`
  fails its static-limit sweep assertion requiring both early and leaf errors;
  a diagnostic retry observed zero leaf and 288 early errors. The remaining
  debug files were not reached. This candidate is not validated for retention.
  No assertion was weakened or disabled; all runtime edits and the diagnostic
  probe were removed.
- Retain the useful native stack regression improvement: compile its C++
  fixture at both O0 and O3 and escape the padding array's address so O3 cannot
  scalar-replace its intended 512-byte stack frame with two bytes. Both levels
  pass on the unchanged merged baseline in release/debug, covering legacy,
  explicit-context and live/null stack-limit views and foreign registration.
- The merge's release/debug tools and relevant qmods build. Three targeted
  upstream CMake cases pass 32 assertions with GNU touch; the new fixture uses
  `touch -d`, unsupported by macOS's default touch. Four external-database
  cases were deliberately skipped. No external database was contacted.

Next: profile polymorphic receiver selection, checked local initialization,
ownership and dispatch separately. The existing polymorphic kernel creates a
runtime-backed class local each iteration; its approximately 50 ns/element is
not an isolated virtual-call measurement. Investigate reducing repeated work
with explicit-context or compiler-backed proofs, preserving runtime locals,
private-inheritance access, weak normalization, reflective writes, ownership
and debugger/exception behavior. An identical class annotation alone is not
a valid reason to remove assignment checks.

After removal, the restored release SHA-256 exactly matches the saved merged
baseline. The separately rebuilt debug library is
`45610ef92e04b019c6152af4ec7e5a32000a6eda608227d65a559f6edec33afe`.
Both builds pass seven targeted files, 15 cases and 824 assertions, including
the unchanged native-leaf sweep that rejected the second candidate. Four
benchmark modes pass 330 references each. The retained changes pass all 62
audit-changes checks (14 Pass, 48 N/A); no runtime speedup is claimed.
