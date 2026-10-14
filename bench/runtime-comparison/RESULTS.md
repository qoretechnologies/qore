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

## Object-admission specialization (2026-10-09)

Retain direct node-tag admission at the profiled class-initializer and exact-class
dispatch sites. The merged baseline runtime is `688d20bb18a1ced15dee6ce262cb918d115d26ca`;
the experiment starts at `9be31eb9d`, whose extra changes are tests/docs only.
The runtime checks ordinary and opaque pointer nodes directly, retaining generic
dispatch for non-pointer representations. Weak normalization, raw/parameterized
class compatibility, private-inheritance access, slot ownership, reflective
mutation and live deletion behavior are unchanged. No compiler or ABI change.

[Full evidence](results-object-admission-macos-arm64.json) records the exact
precommit release/debug libraries, production patch, raw normalized samples,
fork statistics and checks. Five randomized forks, three warmups, five 500 ms
samples per fork and five startup observations; identical AOT executable within
each before/after pair. All 1,050 windows ran without concurrent task builds,
tests, profiles, disassembly or evidence calculations. Host background activity
and filesystem caches remain uncontrolled. Values are median fork means in
ns/element for whole kernels, not isolated calls.

Count 1024, seed 810816, `run-6j96ghza`:

|!Kernel|!Before|!After|!Change|!Fork ranges overlap|!Java JIT|!Qore/Java
|polymorphic_call|50.044|45.946|-8.19%|no|2.594|17.71x
|optional_method_call|12.167|12.244|+0.64%|yes|1.702|7.19x
|monomorphic_call|5.443|5.417|-0.49%|yes|1.685|3.21x
|indirect_wide|8.348|8.554|+2.47%|yes|1.721|4.97x
|indirect_float|10.469|10.346|-1.17%|yes|1.668|6.20x
|arithmetic|2.426|2.437|+0.44%|yes|0.956|2.55x
|dispatch_control|2.604|2.616|+0.44%|yes|1.006|2.60x

Count 4096, seed 810817, `run-jv2imezr`:

|!Kernel|!Before|!After|!Change|!Fork ranges overlap|!Java JIT|!Qore/Java
|polymorphic_call|48.597|45.038|-7.32%|no|2.570|17.52x
|optional_method_call|10.834|10.896|+0.57%|yes|1.697|6.42x
|monomorphic_call|4.049|4.066|+0.42%|yes|1.694|2.40x
|indirect_wide|7.252|7.250|-0.03%|yes|1.708|4.24x
|indirect_float|9.197|9.143|-0.59%|yes|1.674|5.46x
|arithmetic|1.396|1.376|-1.38%|yes|1.007|1.37x
|dispatch_control|1.452|1.440|-0.82%|yes|1.015|1.42x

Polymorphic execution improves **8.19%** at 1024 and **7.32%** at 4096, with
nonoverlapping before/after fork ranges in both runs. All six control kernels'
ranges overlap at both sizes. The first wide-callback median increases 2.47%,
but the repeat is flat (-0.03%); do not hide that unfavorable first result.
Nullable medians increase about 0.6% in both runs within overlapping ranges.
Range overlap is descriptive, not a significance test. No broad speedup,
startup improvement or Java parity is claimed.

At 4096 elements the remaining Qore/Java ratios are about 2.4x monomorphic,
6.4x nullable, 4.2x wide callback, 5.5x float callback and 17.5x polymorphic.
Java's DynamicStep methods are DONT_INLINE; Qore can inline the exact-base
branch, so this is not an equal-inlining-policy call-latency comparison.
The polymorphic kernel additionally selects and initializes a runtime-backed
receiver local on every iteration.

Validation: release/debug each pass 35 selected files (27 QUnit files, 72 cases,
4,073 assertions plus eight standalones); six reference modes pass 330 checks
each; all 11 harness tests pass. Expanded class-assignment and native receiver
fixtures also pass the unchanged release baseline. The native fixture temporarily
installs a fixed class only in an unshared test Program for the non-calling
predicates, restoring it before real dispatch. This covers positive and negative
exact-class checks even when the original name-method slot has no fixed class.
Scalar/enum/weak/opaque values, deleted receivers, class access, scope/unwind
ownership, reflective mutation and ordinary fallback paths are covered.

Separate 10-second steady-state profiles (not performance timing) show
`QoreValue::getType()` top samples falling from 689/8,332 to 13/8,300.
The candidate's receiver-local initialization branch has 3,441 inclusive samples,
local cleanup 1,380, cache release 766 and native method dispatch 966. These are
sibling kernel branches; do not add nested inclusive counts. Runtime text grows
by 516 bytes. The exact-class helper's frame grows from 48 to 64 bytes, but its
strong-object path avoids generic calls; the repeated kernel benefit determines
retention, not static instruction count.

Next: reduce repeated class compatibility/local setup and owned-value cleanup,
using live explicit RuntimeConfig or valid compiler proofs. Preserve runtime
local visibility and declaration-order destruction. Do not revive the rejected
native-only local lowering without its required unified cleanup proof.


## Empty class-result holder (2026-10-09)

Retained runtime-only follow-up to object admission: an admitted class-local
initializer fills its result holder directly instead of using replacement
assignment, which unnecessarily calls `discard()` on NOTHING. The internal
precondition now asserts `isNothing()`, not the broader false-value test. Both
callers create the holder empty; a rejected first attempt does not modify it.
The holder remains an RAII owner. Slot/cache references, type/access validation,
weak normalization, real-reference bookkeeping and error cleanup are unchanged.
This is not a general reference-count or runtime-local elimination.

Baseline is the committed merged `18d04ebe5` runtime (release SHA256
`c76ba61124c09e517c22633d75ed488a31bd1d150e51f98187a91e6408c9cdf4`),
not the preceding section's precommit candidate. The measured candidate release
SHA256 is `de83c7febad5addb2ea94387dacbe8a20657616aa533d8193559a3163e83559c`.
The header edit changes the module runtime identity, so test qmods were rebuilt
for both release and debug. Standalone AOT ABI remains compatible: each run uses
the exact same executable for before/after. Complete source patch, binary
identities and normalized samples are in
[the evidence](results-class-holder-macos-arm64.json).

Two randomized seven-kernel runs use five forks, three warmups and five 500ms
samples per fork. A focused confirmation uses twelve forks with the same windows.
Together: 282 jobs and 1,410 timing windows, independently checked against raw
Qore/JMH output. No task builds, tests, profiles, disassembly or evidence
computation ran during timing. Host background activity is uncontrolled.
All numbers below are whole-kernel ns/element, median of fork means.

Count 1024, 5 forks, seed 810818, `run-vpsp78_j`:

|!Kernel|!Before|!After|!Change|!Fork ranges overlap|!Java JIT|!Qore/Java
|polymorphic_call|45.838|45.033|-1.76%|yes|2.536|17.76x
|optional_method_call|12.053|12.123|+0.58%|yes|1.675|7.24x
|monomorphic_call|5.471|5.317|-2.81%|yes|1.677|3.17x
|indirect_wide|8.340|8.507|+1.99%|yes|1.684|5.05x
|indirect_float|10.261|10.300|+0.37%|yes|1.654|6.23x
|arithmetic|2.475|2.448|-1.05%|yes|0.890|2.75x
|dispatch_control|2.598|2.608|+0.38%|yes|0.988|2.64x

Count 4096, 5 forks, seed 810819, `run-vroocuab`:

|!Kernel|!Before|!After|!Change|!Fork ranges overlap|!Java JIT|!Qore/Java
|polymorphic_call|44.535|43.926|-1.37%|yes|2.543|17.27x
|optional_method_call|10.689|10.759|+0.65%|yes|1.674|6.43x
|monomorphic_call|4.014|4.012|-0.05%|yes|1.676|2.39x
|indirect_wide|7.269|7.216|-0.73%|yes|1.686|4.28x
|indirect_float|9.165|9.106|-0.65%|yes|1.655|5.50x
|arithmetic|1.365|1.370|+0.35%|yes|0.929|1.47x
|dispatch_control|1.424|1.423|-0.08%|yes|0.998|1.43x

Count 4096, 12 forks, seed 810820, `run-v313j42s`:

|!Kernel|!Before|!After|!Change|!Fork ranges overlap|!Java JIT|!Qore/Java
|polymorphic_call|44.334|43.535|-1.80%|yes|not rerun|n/a
|optional_method_call|10.777|10.669|-1.00%|yes|not rerun|n/a
|dispatch_control|1.424|1.426|+0.14%|yes|not rerun|n/a

The polymorphic median improves 1.76%, 1.37% and 1.80%; the repeated direction
and removal of a known-empty cleanup support keeping this small change.
All fork ranges overlap; overlap is descriptive, not a significance test.
The nullable medians initially rise about 0.6%, then fall 1.00% in the twelve-fork
repeat. Wide callbacks initially rise 1.99%, then fall 0.73%. Do not claim a
broad speedup, an isolated call-latency gain or a startup improvement.

At 4096 elements Java remains about 2.4x faster for monomorphic calls, 6.4x
nullable, 4.3x wide callbacks, 5.5x float callbacks and 17.3x for this mixed
polymorphic kernel. Java's DynamicStep methods are DONT_INLINE; Qore can inline
the exact-base branch, so these do not enforce the same inlining policy.

Validation: release and debug each pass 35 selected files (27 QUnit files,
72 cases, 4,073 assertions plus eight standalones); six execution/reference
modes pass 330 checks each and all eleven harness tests pass. Existing tests
cover both result-producing branches, disabled fast paths, weak/opaque inputs,
class errors/access, scope/unwind ownership, deletion and reflective mutation.
No full-repository, sanitizer or cross-architecture qualification is claimed.
The initializer has zero direct discard callsites instead of two, but grows
from 257 to 260 static instructions; runtime text grows 12 bytes. This does
not remove the holder destructor or either owned object reference.

Next: add a separately named, matched Java/Qore polymorphic-expression kernel
to distinguish receiver-local initialization/cleanup from dispatch work. Keep
the existing kernel and its historical results intact. Any ownership elimination
must preserve reflective local visibility and declaration-order destruction;
the previously rejected native-only local lowering remains rejected.

## Live receiver-status loads and polymorphic decomposition (2026-10-09)

The separately named `polymorphic_expression_call` uses the same three receivers,
selection, recurrence and checksum as `polymorphic_call`, but calls directly on
the selection expression instead of assigning a typed local each iteration.
The original benchmark and its historical results remain unchanged. This is a
source-level diagnostic, not a runtime optimization or an isolated local-variable
cost. Both Java forms keep DONT_INLINE on all three target methods.

Fresh committed merged baseline: `fd6632de9`, incorporating upstream
`f9be2f451`; release SHA256
`438e4d9932b8abba3b60829ab6922c08965a89631e0a2535ae1639b76deabbda`.
At count 4096, five forks, three warmups and five 500 ms windows, seed 810821,
`run-zjvv3g09` measures:

|!Kernel|!Qore IR|!Qore AOT|!Java JIT
|polymorphic_call|1007.087|43.834|2.563
|polymorphic_expression_call|763.809|10.327|2.564
|monomorphic_call|430.589|4.079|1.692
|dispatch_control|276.449|1.443|0.999

Units are whole-kernel ns/element, median of fork means. The AOT expression form
is about 76.4% lower than the typed-local form; IR is about 24.2% lower. Java is
essentially unchanged. The gap includes lowering, local setup and ownership/
cleanup effects, not just method dispatch. Even the expression form remains
about 4.0x Java. These numbers motivate further receiver-local work without
reviving the previously rejected cleanup-order-changing native-only lowering.

The retained optimization borrows a live status address from an already-owned,
stable exact-class receiver at function entry. Generated code replaces the
per-call validity-helper call with a sequentially consistent atomic load.
It still checks identity; changed receivers use normal dispatch. Validity is
never snapshotted. Explicit deletion remains observable, while calls during
destruction retain the existing isValid() semantics. No owner is added and no
runtime slot, cleanup order, Program/access/observer/stack guard is removed.
Null views and unsupported atomic representations retain the old helper.

The new internal `qore_rt_get_object_status_view_v1` ABI requires separately
compiled baseline/candidate executables. Do not reuse a candidate executable on
the old runtime. The compile-time presence flag
`QORE_DISABLE_AOT_INLINE_RECEIVER_VALIDITY=1` omits the new view; setting it only
at execution returns null and exercises the old helper. Candidate release SHA256:
`39fdff511350b546ba6e82bad73d44d06bbca6c425220095c94827d908d1f00c`.
Complete source patch, library/tool identities, normalized raw windows and
recomputed statistics are in [the evidence](results-receiver-status-macos-arm64.json).

Each paired run uses five forks, three warmups, five 500 ms samples, five startup
samples and a randomized baseline/candidate/Java schedule. Together with the
diagnostic: 300 jobs, 1,500 timing windows. No task builds, tests, profiling,
disassembly or evidence computation ran during timing. Host background activity
is uncontrolled. Startup has no claimed improvement.

Count 4096, seed 810822, `run-80amfebq`:

|!Kernel|!Before|!After|!Change|!Fork ranges overlap|!Java JIT|!Qore/Java
|monomorphic_call|3.954|3.286|-16.89%|no|1.681|1.95x
|optional_method_call|10.767|10.773|+0.05%|yes|1.688|6.38x
|polymorphic_call|43.857|43.642|-0.49%|yes|2.518|17.33x
|polymorphic_expression_call|10.312|10.333|+0.20%|yes|2.549|4.05x
|indirect_wide|7.333|7.208|-1.70%|yes|1.698|4.24x
|indirect_float|9.078|9.026|-0.58%|yes|1.659|5.44x
|arithmetic|1.370|1.375|+0.33%|yes|0.997|1.38x
|dispatch_control|1.424|1.421|-0.18%|yes|1.008|1.41x

Count 1024, seed 810823, `run-k3dgaifw`:

|!Kernel|!Before|!After|!Change|!Fork ranges overlap|!Java JIT|!Qore/Java
|monomorphic_call|5.389|4.688|-13.00%|no|1.678|2.79x
|optional_method_call|12.284|12.218|-0.54%|yes|1.687|7.24x
|polymorphic_call|46.186|45.573|-1.33%|yes|2.556|17.83x
|polymorphic_expression_call|11.764|11.830|+0.56%|yes|2.563|4.62x
|indirect_wide|8.422|8.395|-0.31%|yes|1.690|4.97x
|indirect_float|10.250|10.298|+0.47%|yes|1.656|6.22x
|arithmetic|2.471|2.462|-0.36%|yes|0.930|2.65x
|dispatch_control|2.605|2.590|-0.58%|yes|0.995|2.60x

Monomorphic cost falls 16.89% and 13.00%; its fork ranges do not overlap in either
run. All control ranges overlap, with small mixed movements. Range overlap is
descriptive, not a significance test. Retain the targeted change, not a broad
call-speed claim. At 4096, Qore is still about 1.95x Java for this monomorphic
kernel, 6.38x nullable, 4.05x expression-polymorphic, 4.24x wide callbacks, 5.44x
float callbacks and 17.33x for the typed-local polymorphic kernel. Qore may inline
the exact-base branch; Java keeps its target uninlined, so this is not equal-
inlining-policy retained-call latency or Java parity.

Validation: release/debug each pass 35 selected files (27 QUnit files, 72 cases,
4,295 assertions plus 8 standalones) and two context files (4 cases, 150 assertions).
Six reference modes pass 345 checks each; 11 harness tests pass. Expanded fixtures
cover reflective replacement, deletion, destructor-in-progress, null-view
fallback and strong/opaque/weak/nonobject representations, with O0/O3, exception-
handling, parallel compilation and context controls. The original runtime also
passed AST/AOT reference semantics for the new deletion/destructor cases.
QUnit/FsUtil/Util qmods were rebuilt sequentially for both trees after the runtime
identity changed. No full-repository, sanitizer or cross-architecture claim.

Arm64 inspection confirms one entry getter and an in-loop `ldar w` on the live
path; the helper remains on the null-view branch. The monomorphic function grows
from 614 to 616 static instructions. These are static counts, not dynamic costs.

Next: optimize receiver-local setup/ownership while preserving reflective
visibility and declaration-order destruction, and profile retained polymorphic
native dispatch separately from that local-handling cost.

## Entry-pinned class-local cache borrowing (2026-10-09)

This optimization removes one duplicate cache reference, not the runtime local.
A bounded compiler analysis follows loads/PHIs back to existing unmodified
entry owners. At each eligible initial class-local assignment, actual identity
must match one of those retained values. A versioned runtime helper checks the
ordinary slot, strong representation and class/access acceptance and acquires
the normal runtime-slot reference. The cache then borrows the independently
pinned value. Reflection replacing a source, weak/opaque values, filtering,
fixed storage, captured locals and unknown sources keep ordinary owned caching.
Real-reference accounting, declaration-order cleanup, runtime visibility,
exception handling and call/context/observer guards remain intact.

The existing entry owner, not the mutable runtime slot, protects the borrowed
cache and SSA uses. No extra entry owner is added. This is not the previously
rejected native-only body-local lowering. The compile/runtime presence control
is `QORE_DISABLE_AOT_PINNED_CLASS_LOCAL=1`; the new helper ABI requires separately
compiled baseline/candidate artifacts for the enabled optimization.

Baseline is committed `5bcdad02e`, rebuilt before edits; release SHA256
`c7acc35efad5e1807c264f2ce4ab7ac1ee887b4fa7f0e6ffbf1376f37c46db2c`.
Candidate release SHA256 is
`bcc506c9bc8db0913f3c6029a6c976b8a1d7862b68cdbcadad1626da6ec5a23f`.
The complete production patch, identities, reference validation, raw-window
normalization, summaries and profile metadata are in
[the evidence](results-pinned-class-local-macos-arm64.json).

Each main run uses five forks, three warmups, five 500 ms windows, five startup
samples and a randomized baseline/candidate/Java schedule. Whole-kernel
ns/element, median of fork means, excludes startup. No task builds, tests,
profiling, disassembly or evidence computation overlapped timing; only progress
reads. Host background activity and filesystem caches remain uncontrolled.

Count 4096, seed 810831, `run-v2vmnig2`:

|!Kernel|!Before|!After|!Change|!Fork ranges overlap|!Java JIT|!Qore/Java
|polymorphic_call|44.136|38.525|-12.71%|no|2.594|14.85x
|polymorphic_expression_call|10.405|10.783|+3.62%|no|2.587|4.17x
|monomorphic_call|3.347|3.413|+1.96%|yes|1.698|2.01x
|optional_method_call|11.036|10.938|-0.88%|yes|1.696|6.45x
|indirect_wide|7.337|7.325|-0.16%|yes|1.696|4.32x
|indirect_float|9.189|9.187|-0.02%|yes|1.687|5.44x
|arithmetic|1.393|1.393|-0.02%|yes|0.983|1.42x
|dispatch_control|1.451|1.435|-1.09%|yes|1.009|1.42x

Count 1024, seed 810832, `run-hn6bvcid`:

|!Kernel|!Before|!After|!Change|!Fork ranges overlap|!Java JIT|!Qore/Java
|polymorphic_call|45.947|40.461|-11.94%|no|2.559|15.81x
|polymorphic_expression_call|11.848|12.180|+2.81%|yes|2.558|4.76x
|monomorphic_call|4.710|4.721|+0.23%|yes|1.693|2.79x
|optional_method_call|12.282|12.380|+0.79%|yes|1.689|7.33x
|indirect_wide|8.576|8.443|-1.55%|yes|1.692|4.99x
|indirect_float|10.324|10.399|+0.73%|yes|1.686|6.17x
|arithmetic|2.485|2.481|-0.15%|yes|0.978|2.54x
|dispatch_control|2.624|2.645|+0.80%|yes|1.010|2.62x

The target saves about 5.5 ns/element in both runs, with disjoint fork ranges.
There is also a consistent expression-control regression; do not describe this
as a universally faster call implementation. Other control ranges overlap.
Range overlap is descriptive, not a significance test. Retain the targeted
12% local-reference saving with the roughly 3% expression regression explicitly
recorded for follow-up. No startup, isolated call-latency or Java-parity gain is
claimed. Java retains DONT_INLINE targets, while Qore may inline its exact-base
branch; the ratios are whole-kernel comparisons, not equal-inlining-policy
retained-call latency.

The expression and monomorphic functions have identical normalized instruction
sequences, respectively 982 and 616 static instructions, after resolving
relocations; their placement changes. The typed-local function grows from 1217
to 1241 instructions. To test whether executable layout alone explained the
regression, `run-3x8qoz_f` uses the same executable on both runtimes, with borrowing
disabled at compilation and execution. It imports no new borrowing helper, so
this diagnostic can safely reuse the artifact. At count 4096, five forks and the
same window settings, seed 810833:

|!Kernel|!Old runtime|!New runtime|!Change|!Fork ranges overlap
|polymorphic_call|43.883|44.568|+1.56%|yes
|polymorphic_expression_call|10.438|10.726|+2.76%|yes
|monomorphic_call|3.299|3.277|-0.66%|yes

The expression difference persists with identical generated code and layout;
the runtime/library effect needs further investigation. A library-layout cause
is plausible but unproven. This disabled-path diagnostic is not the enabled
optimization comparison. All three runs total 270 jobs/1350 windows, independently
checked against raw Qore/JMH output, reference checks and binary/source hashes.

Validation: release/debug each pass 38 files: 30 QUnit files, 77 cases, 4745
assertions and eight standalones. The new fixture passes 194 assertions per build
and the expanded assignment fixture 406. Cases include bounded PHIs, reflective
source/receiver replacement, repeated borrowed/owned transitions, self-cycles,
returned values, on_exit use, mixed-local destruction and exception cleanup.
O0/O3, native EH, parallel compilation, full contexts and compile/runtime
fallback controls pass. The frozen baseline also matches AST output for the
new fixture. Six reference modes pass 345 checks each; 11 harness tests pass.
QUnit/FsUtil/Util qmods were rebuilt sequentially before tests. No full-repository,
sanitizer or cross-architecture qualification is claimed.

Post-timing profiles (10 seconds at 1 ms after warmup, sampled separately):

|!Kernel|!Main samples|!Checked initializer|!Runtime-local clear|!Native continuation|!Exact-class guard
|polymorphic_call|8287|3359|1606|1177|403
|polymorphic_expression_call|8494|N/A|N/A|4119|1283

These are disjoint callsite-inclusive sample counts, not timings; nested
descendants must not be added again. The expression trace also has 380 inlined
native-continuation sibling samples outside the 4119 callsite samples. Runtime
local setup/cleanup still dominates the typed-local case; native dispatch and
class checks remain central in the expression case.

Next: investigate the runtime/library expression regression, then test moving
repeated class-admission work to guarded entry-time proofs and reducing native
dispatch checks where Program, receiver identity, access and observability
invariants genuinely permit it. Keep the runtime slot and destruction semantics;
do not replace dynamic checks with unguarded parse-time assumptions.

## Rejected native-method configuration aggregation (2026-10-10)

After merging `origin/develop` at `f0b67f173`, test whether sharing one static
initialization for native-method admission and deferred-context controls reduces
retained-call cost. The candidate changes only `qore_rt_try_method_int_unary_v1`:
one immutable flag word replaces two separately initialized booleans. No caller
ABI, compiler, Program/access/type/epoch, frame, stack or exception check changes.
It samples deferred-context configuration at the first helper invocation rather
than the first admitted call. The experiment is **not retained**: fewer static
instructions did not produce a repeatable measured benefit. The original runtime
and configuration semantics are restored; two extra runtime-control test legs
are retained.

[Complete evidence](results-native-method-flags-macos-arm64.json) includes the
rejected patch, binary/source hashes, all raw-derived samples, references and
summaries. Frozen baseline release SHA256 is
`ed38958bd81afcc8664141211839bbc106e9561e980b8b416e870ee61417586f`;
candidate is `f942d877c4383310c353f7b3724345eb90e47657837033876a2fb62d966b891c`.
Each paired run reuses one identical executable with the two compatible runtimes.
All runs use five forks, three warmups, five 500 ms windows and five startup
samples, with randomized schedules. Whole-kernel ns/element is the median of
fork means, excluding startup. No task builds, tests, profiling, disassembly or
evidence computation overlapped timing; only progress reads. Host background
activity and filesystem caches remain uncontrolled. No sample is discarded.

The first paired run (`run-vwcr3bme`, count 4096, seed 810842) has very large
slowdowns in both versions: typed-polymorphic baseline fork means span
34.60–237.67 ns and expression baseline fork means span 10.33–195.21 ns. A repeat
at count 1024 (`run-fgntc20h`, seed 810843) and a cleaner confirmation at 4096
(`run-7j4z_6vo`, seed 810844, Qore modes only) do not establish a consistent gain:

|!Kernel|!First 4096 change|!1024 change|!Confirmation before|!Confirmation after|!Confirmation change
|polymorphic_call|+11.00%|-0.98%|37.943|37.116|-2.18%
|polymorphic_expression_call|-2.30%|-1.10%|10.518|10.572|+0.51%
|monomorphic_call|-7.31%|-0.52%|3.391|3.319|-2.14%
|optional_method_call|+10.05%|-1.71%|10.889|11.010|+1.11%
|indirect_wide|+0.33%|+4.72%|7.278|7.338|+0.82%
|indirect_float|-3.24%|-0.71%|9.084|9.273|+2.07%
|arithmetic|+0.43%|+0.04%|1.367|1.371|+0.29%
|dispatch_control|-6.67%|-0.18%|1.436|1.422|-0.98%

All before/after fork ranges overlap, including the confirmation. Range overlap
is descriptive, not a significance test; neither a proven regression nor a
speedup is claimed. The baseline-only run before production edits (`run-x_wjqfy8`,
count 4096, seed 810841) provides this post-merge Java comparison:

|!Kernel|!Qore AOT|!Java JIT|!Qore/Java
|polymorphic_call|38.829|2.613|14.86x
|polymorphic_expression_call|10.578|2.597|4.07x
|monomorphic_call|3.413|1.703|2.00x
|optional_method_call|11.097|1.706|6.51x

Java retains DONT_INLINE targets; Qore may inline its exact-base branch. These
are whole-kernel comparisons, not equal-inlining-policy isolated call latency or
Java parity. All four runs total 360 jobs/1800 measured windows, independently
recomputed from raw Qore/JMH output with reference and identity checks.

Arm64 inspection finds 752 to 743 static instructions in the native helper and
one fewer static-initialization guard. The live `pc_loc_map_attached` atomic
admission check remains. The baseline-only and first paired generated kernel
bodies are relocation-normalized identical for typed polymorphic, expression
polymorphic and monomorphic calls (1241, 982 and 616 instructions). This does not
prove a cause for runtime/layout or host-dependent timing differences.

Candidate validation passes 17 files in each build: 13 QUnit files, 37 cases,
2695 assertions plus four standalones. Six candidate reference modes and six
baseline reference modes each pass 345 checks; the 11 harness tests pass. The
expanded name-method driver covers deferred-context disabled alone and together
with native continuation disabled, across its existing optimization/EH/parallel
matrix. After restoring and rebuilding both runtimes and QUnit/FsUtil/Util qmods,
the name-method and native-context drivers pass again in each build (three cases,
742 assertions). The restored release library matches the frozen baseline hash
exactly. No full-repository, sanitizer or cross-architecture claim.

Next: target repeated class admission and runtime-local cleanup. Preserve the
runtime slot, access-sensitive acceptance and declaration-order destruction;
measure any guarded proof/cache against this unchanged post-merge baseline.
Inspection found that the existing method-dispatch epoch is not advanced by all
base-class addition paths (`QoreClass::addBuiltinBaseClass` and
`qore_class_private::addBaseClass`). Do not treat it as a complete inheritance
invalidation mechanism. Any cached admission must establish a sufficient
invariant or add the appropriate invalidation before it can replace a check.


## Invocation-local AOT class body-slot handles (2026-10-10)

Retain an entry-time storage lookup for ordinary required-class body locals.
The compiler proves the local is noncaptured, nonself, nontoplevel and backed
by a runtime slot. After entry instantiation, an explicit RuntimeConfig resolves
that slot once; pinned initialization and source block cleanup receive the
borrowed handle. This removes repeated TLS and stack lookup, not assignment
validation. Type/access, empty-slot, representation and real-reference checks
remain. Reflection still sees and replaces the live slot value. The storage
survives until function exit, including nested calls across stack blocks; no
value or extra owner is cached. Other locals and null handles use the old path.

`QORE_DISABLE_AOT_CLASS_LOCAL_SLOT=1` is a compile-time control and a runtime
presence control sampled at the first getter call. Old helper exports remain;
new versioned helpers require separately compiled baseline/candidate artifacts.
The new direct clear helper's admitted arm64 branch tail-calls the existing
`VarValueBase::del` without a TLS lookup. No general class-acceptance cache is
introduced: the method-dispatch epoch still cannot prove all inheritance
mutations absent.

Baseline is `0e8f443dd` (runtime source unchanged from `f0b67f173`), release
SHA256 `ed38958bd81afcc8664141211839bbc106e9561e980b8b416e870ee61417586f`.
Candidate release SHA256 is
`91f586f481ce30922c24d83f3feae1ce8dec798b3cf3d30b2a3ed34497854629`.
[Complete evidence](results-class-local-slot-macos-arm64.json) preserves the
production patch, identities, raw-derived windows, summaries, test/reference
hashes and assembly. All full runs use five forks, three warmups, five 500 ms
windows, five startup samples and a randomized before/after/Java schedule.
Whole-kernel ns/element is the median of fork means; startup is excluded.

Count 4096, seed 810851, `run-9bhu8r0p`:

|!Kernel|!Before|!After|!Change|!Fork ranges overlap|!Java JIT|!Qore/Java
|polymorphic_call|38.357|34.882|-9.06%|no|2.591|13.46x
|polymorphic_expression_call|10.608|10.705|+0.91%|yes|2.618|4.09x
|monomorphic_call|3.431|3.371|-1.76%|yes|1.713|1.97x
|optional_method_call|10.747|10.919|+1.60%|yes|1.702|6.42x
|indirect_wide|7.463|7.372|-1.21%|yes|1.707|4.32x
|indirect_float|9.239|9.262|+0.25%|yes|1.694|5.47x
|arithmetic|1.393|1.396|+0.27%|yes|0.967|1.44x
|dispatch_control|1.447|1.450|+0.26%|yes|1.024|1.42x

Count 1024, seed 810852, `run-cy77g_z3`:

|!Kernel|!Before|!After|!Change|!Fork ranges overlap|!Java JIT|!Qore/Java
|polymorphic_call|39.877|36.575|-8.28%|no|2.572|14.22x
|polymorphic_expression_call|12.009|12.326|+2.64%|yes|2.563|4.81x
|monomorphic_call|4.771|4.739|-0.68%|yes|1.698|2.79x
|optional_method_call|12.291|12.278|-0.10%|yes|1.708|7.19x
|indirect_wide|8.711|8.368|-3.93%|no|1.708|4.90x
|indirect_float|10.414|10.371|-0.41%|yes|1.676|6.19x
|arithmetic|2.462|2.460|-0.07%|yes|0.989|2.49x
|dispatch_control|2.634|2.580|-2.03%|yes|1.003|2.57x

Count 1024, seed 810853, `run-w0qqaglq`:

|!Kernel|!Before|!After|!Change|!Fork ranges overlap|!Java JIT|!Qore/Java
|polymorphic_call|39.974|36.687|-8.22%|no|2.585|14.19x
|polymorphic_expression_call|12.123|12.216|+0.77%|yes|2.579|4.74x
|monomorphic_call|4.768|4.686|-1.72%|yes|1.686|2.78x
|optional_method_call|12.332|12.540|+1.69%|yes|1.693|7.41x
|indirect_wide|8.624|8.573|-0.60%|yes|1.702|5.04x
|indirect_float|10.477|10.515|+0.36%|yes|1.685|6.24x
|arithmetic|2.467|2.486|+0.75%|yes|0.944|2.63x
|dispatch_control|2.627|2.657|+1.11%|yes|0.984|2.70x

The target saves 3.3–3.5 ns/element (8.22–9.06%) in all three runs, with disjoint
fork ranges. Expression controls increase 0.77–2.64%; nullable calls change
+1.60%, -0.10%, +1.69%. Their ranges overlap in every trial; this does not prove
there is no small regression. The one disjoint 3.93% indirect-wide gain is not
repeated at that magnitude. Range overlap is descriptive, not a significance
test. Retain the targeted storage-lookup saving, explicitly preserve the
control results, and investigate the remaining expression/runtime effects.
No general call, isolated latency, IR, startup or Java-parity gain is claimed.
Java retains DONT_INLINE targets; Qore may inline its exact-base branch. These
whole-kernel ratios do not use equal inlining policies.

The first 1024 run lost its process session, but subsequent artifact inspection
found all 120 jobs and the complete summary. Every raw job was revalidated;
it is included above. A sandbox retry, `run-86_72vy8`, failed at job three when
JMH's localhost coordination socket was denied. Its two completed Qore jobs
(10 windows), failed stderr and hashes are preserved separately, without a
paired conclusion. The approved confirmation uses a fresh directory and the
same retry seed. Across full trials: 360 jobs/1800 windows, all independently
recomputed. No task builds, tests, profiling, disassembly or evidence computation
overlapped timing; only progress reads. Host background activity and filesystem
caches remain uncontrolled.

Release/debug each pass 38 files: 30 QUnit files, 77 cases, 5097 assertions plus
eight standalones. Pinned-local coverage expands to 388 assertions, assignment
coverage to 500 per build. The new recursion case crosses 64-entry stack blocks;
it exercises ordinary AOT entry, not explicit recursive fast-entry coverage.
Existing fast-entry, reflective replacement, weak/reference, generic/private
class, lifetime, exception, source-location, parallel, cancellation and stack
guard cases pass. O0/O3, native EH, compile/runtime-disabled and full-context
legs pass. The baseline also matches AST output for the expanded fixture.
Candidate release has four reference modes (IR/AOT/Java interpreter/JIT), debug
has two (IR/AOT), and baseline release has four; each passes 345 checks. There
is no fresh baseline debug reference grid in this experiment. Eleven harness
tests pass. Both core and QUnit/FsUtil/Util qmod rebuilds finished before tests.
No full-repository, sanitizer or cross-architecture qualification is claimed.

Next: commit the audited result, synchronize with develop, then profile the
remaining checked initializer, real-reference cleanup and native continuation.
Use those profiles to select the next same-Program fast-call optimization;
do not replace access-sensitive acceptance with an incomplete epoch proof.

## Compact native-method diagnostic frame (2026-10-10)

The same-Program unary-integer continuation now borrows the already-admitted
immutable PIC entry for diagnostic metadata, instead of copying three metadata
pointers into every frame. The caller location remains per-call. Complete
dispatch/admission checks, stack guards, publication, reader synchronization,
cold context materialization and eager fallback are unchanged. Pure calls
still publish frames because concurrent stack readers can observe them.
The caller's AOT context retains the entry, and the last-member publishing
helper waits for readers before frame members are destroyed.

Baseline source is `abb1e399b`, after the develop merge, with release SHA256
`01b61050e6de5f84684ff23444ba61b3ae6efcd83de2a4c0ac78beefcb1f1e47`.
Candidate release SHA256 is
`dd9faaedf83418c3d1f34ffc7d35bb3fa6afe49d1eeb8d5c145f5cc39e406600`.
[Complete evidence](results-compact-native-method-frame-macos-arm64.json)
records the production patch, binary identities, all raw-derived timing windows,
references, test/log hashes and assembly. This is runtime-only: every paired
run reuses one identical executable on frozen before/after libraries.

All runs use five forks, three warmups, five 500 ms samples and five startup
samples, with randomized job schedules. Values below are whole-kernel
ns/element, median of fork means; startup is excluded. Java target methods
retain DONT_INLINE, while Qore may inline its exact-base branch. These are not
equal-inlining-policy isolated call latency comparisons.

The pre-edit baseline-only run, `run-yf2kz96q`, count 4096, seed 810861:

|!Kernel|!Baseline Qore|!Java JIT|!Qore/Java
|polymorphic_call|35.588|2.593|13.72x
|polymorphic_expression_call|10.720|2.589|4.14x
|monomorphic_call|3.385|1.697|1.99x
|optional_method_call|11.080|1.707|6.49x

Paired trial, count 4096, seed 810862, `run-38do266a`:

|!Kernel|!Before|!After|!Change|!Fork ranges overlap|!Java JIT|!Qore/Java
|polymorphic_call|36.314|35.501|-2.24%|yes|2.616|13.57x
|polymorphic_expression_call|10.905|10.795|-1.01%|yes|2.629|4.11x
|monomorphic_call|3.391|3.409|+0.54%|yes|1.713|1.99x
|optional_method_call|11.242|11.170|-0.64%|yes|1.717|6.50x
|indirect_wide|7.477|7.391|-1.16%|yes|1.721|4.29x
|indirect_float|9.325|9.245|-0.87%|yes|1.707|5.41x
|arithmetic|1.404|1.411|+0.51%|yes|1.016|1.39x
|dispatch_control|1.463|1.464|+0.07%|yes|1.025|1.43x

Paired trial, count 1024, seed 810863, `run-kiqj40bn`:

|!Kernel|!Before|!After|!Change|!Fork ranges overlap|!Java JIT|!Qore/Java
|polymorphic_call|37.499|36.798|-1.87%|no|2.592|14.20x
|polymorphic_expression_call|12.328|12.326|-0.02%|yes|2.590|4.76x
|monomorphic_call|4.800|4.766|-0.71%|yes|1.708|2.79x
|optional_method_call|12.430|12.357|-0.59%|yes|1.706|7.24x
|indirect_wide|8.675|8.556|-1.37%|yes|1.707|5.01x
|indirect_float|10.422|10.486|+0.61%|yes|1.696|6.18x
|arithmetic|2.505|2.511|+0.26%|yes|0.995|2.52x
|dispatch_control|2.653|2.662|+0.36%|yes|1.013|2.63x

Confirmation, count 4096, seed 810864, `run-t9_iruez`:

|!Kernel|!Before|!After|!Change|!Fork ranges overlap
|polymorphic_call|36.092|35.127|-2.67%|no
|polymorphic_expression_call|10.843|10.837|-0.05%|yes
|monomorphic_call|3.415|3.386|-0.85%|yes
|optional_method_call|11.283|11.159|-1.11%|yes
|indirect_wide|7.369|7.455|+1.17%|yes
|indirect_float|9.241|9.254|+0.14%|yes
|arithmetic|1.405|1.406|+0.08%|yes
|dispatch_control|1.452|1.467|+1.00%|yes

Retain the targeted typed-polymorphic gain: 1.87–2.67%, or 0.70–0.97 ns/element,
in all three trials. Fork ranges are disjoint in the 1024 repeat and 4096
confirmation, but overlap in the first paired trial. Expression calls improve
1.01% initially, then only 0.02% and 0.05%; no repeatable benefit is established
there. Nullable medians improve 0.59–1.11%, with overlapping ranges throughout.
Every control range overlaps, including the small arithmetic/dispatch increases.
Overlap is descriptive, not a significance test or proof of no regression.
No general-call, isolated latency, IR, startup or Java-parity gain is claimed.
Do not add this percentage to the earlier pre-merge slot optimization as an
unmeasured cumulative result.

All four completed runs contain 360 jobs and 1800 windows, independently
recomputed from raw Qore/JMH output. No task builds, tests, profiling,
disassembly or evidence computation overlapped timing; only progress reads.
Host background activity and filesystem caches remain uncontrolled.
The arm64 continuation shrinks from 752 to 749 static instructions and from
416 to 400 stack bytes; the private frame also has a compile-time size check.
Those assembly observations alone are not performance evidence.

Release/debug each pass 38 files: 30 QUnit files, 77 cases, 5121 assertions plus
eight standalones. The added eager-frame controls exercise native-method and
nullable/name-dispatched concurrent readers and debugger behavior. Both expanded
drivers also pass against the frozen baseline (five cases, 1148 assertions).
Existing coverage includes foreign Programs, deleted receivers, overrides,
generic/private contexts, stack overflow, exceptions, source stripping, O0/O3,
native EH, parallel codegen and disabled paths. Both core and QUnit/FsUtil/Util
qmod builds completed before tests. Each frozen runtime has six reference modes
(release IR/AOT/Java interpreter/JIT, debug IR/AOT), 345 checks per mode.
Eleven harness tests pass. No full-suite, new sanitizer or cross-architecture
qualification is claimed.

Next: audit and commit this bounded change, synchronize with develop, and
investigate checked class-local initialization and repeated full native PIC
admission. Preserve live validity, context, debugger and entry-readiness checks.
Inheritance acceptance must not be cached against the incomplete method epoch.

## Exact class-definition lookup (2026-10-10)

Runtime hierarchy lookup now tests exact class-definition identity before
general structural/injected-class equivalence. Both `QoreClass::getClass()`
and the matching `BCNode` edge return the canonical definition handle, including
when either input is a copied `QoreClass` wrapper. Null-base and private/internal
edge checks still run first; access accumulation still runs after a base match.
Non-identical definitions retain the original lookup. Raw/parameterized-object
gates, private-context validation, value ownership and dispatch guards are
unchanged. This caches no inheritance fact and uses no epoch assumption.

Baseline source `6cf499bbf` includes the compact frame and develop through
`d3e549391`. Its release SHA256 is
`db8499c10b2d47516ef50e0b3318d965b410de0a80bc6712b0e26b1c27ddc143`.
Candidate release SHA256 is
`a927fddaaa850b510d64b8ffbd88d00a26cf53b69b80391bf51fe0fb05140265`.
[Complete evidence](results-class-definition-identity-macos-arm64.json)
preserves the production patch, binary/source identities, every raw-derived
window and summary, validation hashes and assembly. No generated ABI or
compiler change: each before/after pair uses the identical AOT executable.

All runs use five forks, three warmups, five 500 ms samples and five startup
samples, with independently randomized schedules. Values are whole-kernel
ns/element, median of fork means, excluding startup.

Paired trial, count 4096, seed 810871, `run-o7t5jzk7`:

|!Kernel|!Before|!After|!Change|!Fork ranges overlap|!Java JIT|!Qore/Java
|polymorphic_call|35.123|33.918|-3.43%|no|2.613|12.98x
|polymorphic_expression_call|10.720|10.614|-0.99%|yes|2.618|4.05x
|monomorphic_call|3.401|3.377|-0.71%|yes|1.718|1.97x
|optional_method_call|11.159|11.004|-1.38%|yes|1.709|6.44x
|indirect_wide|7.444|7.356|-1.18%|yes|1.706|4.31x
|indirect_float|9.277|9.220|-0.62%|yes|1.706|5.41x
|arithmetic|1.401|1.409|+0.59%|yes|1.016|1.39x
|dispatch_control|1.466|1.454|-0.80%|yes|1.028|1.41x

Paired trial, count 1024, seed 810872, `run-uyl4m3ur`:

|!Kernel|!Before|!After|!Change|!Fork ranges overlap
|polymorphic_call|36.799|35.500|-3.53%|no
|polymorphic_expression_call|12.339|12.303|-0.29%|yes
|monomorphic_call|4.740|4.858|+2.51%|yes
|optional_method_call|12.623|12.511|-0.89%|yes
|indirect_wide|8.683|8.588|-1.10%|yes
|indirect_float|10.529|10.590|+0.58%|yes
|arithmetic|2.538|2.522|-0.63%|yes
|dispatch_control|2.675|2.650|-0.92%|yes

Focused confirmation, count 1024, seed 810873, `run-_ntv95gh`:

|!Kernel|!Before|!After|!Change|!Fork ranges overlap
|polymorphic_call|37.388|35.862|-4.08%|no
|polymorphic_expression_call|12.460|12.207|-2.03%|yes
|monomorphic_call|4.843|4.892|+1.02%|yes
|optional_method_call|12.554|12.612|+0.46%|yes

Retain the targeted 3.43–4.08% typed-polymorphic gain (1.20–1.53 ns/element).
Its fork ranges are disjoint in all three trials. Every control range overlaps,
but this does not prove no regression: monomorphic medians change -0.71% at
4096 and +2.51%/+1.02% in the two 1024 trials. The third run specifically checks
that small-loop concern, and leaves a possible modest tradeoff, not a clean
general-call win. Expression medians improve 0.29–2.03% with overlap; nullable
changes are -1.38%, -0.89%, +0.46%. Wide/float/arithmetic/dispatch controls are
preserved in both broader trials. Range overlap is descriptive, not a
significance test.

The 4096 trial's candidate remains 12.98x Java for typed polymorphism, 4.05x for
expression polymorphism, 1.97x for monomorphic and 6.44x for nullable calls.
Java retains DONT_INLINE targets and Qore may inline the exact-base branch:
these are not equal-policy isolated call latency comparisons. No fresh Java
timing was collected in the 1024 trials. No IR timing, startup improvement,
universal call improvement or Java-parity claim is made. Do not add gains from
different pre-merge experiments as an unmeasured cumulative result.

All 240 jobs/1200 windows and their statistics were independently recomputed
from raw Qore/JMH output. No task builds, tests, profiling, disassembly or
evidence computation overlapped timing; only progress reads. Host background
activity and filesystem caches remain uncontrolled. The arm64 public wrapper
grows from 19 to 29 static instructions and the base-edge wrapper from 25 to 39;
each retains its general fallback call but skips it on identity hits. This is
a hot-path call reduction, not a static code-size reduction.

Release/debug each pass 39 performance files (31 QUnit files, 78 cases, 5127
assertions, eight standalones) and eleven class integration files (94 cases,
1492 assertions). The new native driver has six assertions at host O0/O3,
covering canonical results for copied handles, copied base edges and deeper
inheritance. The expanded assignment fixture checks private/internal edge
rejection and authorized access in AST/IR/JIT/tiered and AOT configurations.
Class/generic/injection/import/deleted-Program/override regressions pass.
The expanded assignment driver passes the frozen release baseline with 500
assertions; the native driver passes frozen release and debug with six each.
Each runtime has six reference modes with 345 checks each; eleven harness
tests pass. No full-suite, sanitizer or cross-architecture qualification.

Premeasurement review corrected the first draft to compare private definition
identity and return its canonical handle. A debug compilation had already read
the earlier source and finished after that edit, so an ordinary incremental
rebuild incorrectly treated its object as current. The new native test caught
the stale wrapper-identity result. Disassembly confirmed it; a forced fresh SCU
build, new debug reference grid and complete debug rerun pass. The failed
artifact, log, assembly and partial matrix are preserved separately. It supplied
no timing data. Final debug SHA256 is
`4662bc7ae9017db0dd230779abaac8cc15e2fe9d4a2dd1674c2e55592645fd3f`.

Next: profile remaining checked class-local initialization, real-reference/
ownership cleanup, native admission/frame costs and small-loop monomorphic
entry overhead. Preserve runtime slots, destruction order and concurrent
diagnostics; any further bookkeeping elimination needs a complete lifetime
and observability proof.

## Empty class-local node initialization (2026-10-10)

Retain a narrower storage operation for an already-admitted required-class
initializer. Three randomized trials reduce the typed-polymorphic kernel by
4.19–5.44% (1.48–1.92 ns/element), with disjoint baseline/candidate fork ranges
in each trial. This is an initialization improvement, not a general call-speed
or Java-parity result.

Fresh pre-change sampling found 3339 initialization and 1473 cleanup samples
under an 8193-sample typed-kernel parent, about 59% combined. Native-method
helper callsites accounted for another 1429 samples. These are disjoint
inclusive callsites plus helper siblings; nested child samples are not added
again. The sample guides selection, not exact cost attribution.

### Change and invariants

`LocalVar::tryAssignInitialClassValue()` already excludes captured/self,
substituted, finalized, assigned and static slots, positively accepts the
strong object's class/access constraint, and rejects fixed storage for its
node path. The new internal `QoreLValue::assignInitialNode()` consumes one
reference directly into that proven-empty slot. It preserves assignment order,
`tracksRealRefs()` and `check_lvalue_object_in_out()`, with no replaced value,
conversion, allocation or Qore callback in the storage operation. There is no
temporary owner left to clean up. An optional owned result still receives
its second reference; the pinned-result path still omits only that extra cache
owner. All ordinary fallback, weak normalization, lexical cleanup, GC rooting,
type/private/internal/generic acceptance and reflective mutation behavior remain.

Release disassembly shows 212 → 204 static instructions in the complete
initializer wrapper, with its stack frame unchanged at 96 bytes. Its storage
callee changes from the general 153-instruction node assignment to a
21-instruction initializer; the latter has no call instruction. Those are
static function sizes, not executed instruction counts. The empty
`ReferenceHolder<QoreObject>` destructor call is absent.

The baseline is clean `8622be389` (embedded revision `6cf499bbf`); candidate
is that source plus production patch
`5169b4623a4eef356aa58f515c22c24320221b879cad6651fb4fc0ea0a965d4e`.
Release hashes are
`a927fddaaa850b510d64b8ffbd88d00a26cf53b69b80391bf51fe0fb05140265`
and
`a768e5b62ff1e7b37154071f57661ec8365f5efacb3e61719b4a396ebd540748`.
No public/helper ABI or class layout changed. Because `LocalVar.h` is a
conservative qmod-fingerprint input, the module runtime identity changes;
QUnit/FsUtil/Util and dependencies were rebuilt for both builds. The frozen
debug baseline test driver consequently loads those unchanged module sources
through the loader's explicit identity-mismatch fallback. The module-free
benchmark uses one identical AOT executable against both compatible libraries,
and each mode passes all 345 reference checks before timing.

### Measurements and controls

Apple M4 Pro/macOS arm64, release Qore/LLVM 22, unchanged benchmark source and
Java/JMH settings. Five independent processes per kernel/mode; three warmups,
five measured 500 ms windows and five separately reported startup samples.
Run `run-6mtmtlgj` uses count 4096 and seed 810881, with Java; the two
count-1024 runs `run-e_hmd4or` and `run-tin3qowc` use seeds 810882/810883,
without Java. All eight kernels remain in each run. The 280 jobs and 1400
measurement windows were independently re-parsed and their summaries,
combinations, source/tool/library/executable identities and references checked.
No task build, test, profiling or analysis computation ran during timing;
host background activity, core placement, clocks and filesystem caches remain
uncontrolled. Values are ns/element, median of five process means; lower is better.

|!Kernel|!4096 before → after (change)|!1024 repeat|!1024 confirmation
|polymorphic_call|33.815 → 32.269 (-4.57%)|35.360 → 33.436 (-5.44%)|35.413 → 33.929 (-4.19%)
|polymorphic_expression_call|10.668 → 10.666 (-0.01%)|12.240 → 12.137 (-0.84%)|12.250 → 12.251 (+0.00%)
|monomorphic_call|3.404 → 3.429 (+0.74%)|4.821 → 4.838 (+0.36%)|4.839 → 4.787 (-1.07%)
|optional_method_call|10.980 → 11.064 (+0.77%)|12.461 → 12.447 (-0.11%)|12.792 → 12.360 (-3.37%)
|indirect_wide|7.453 → 7.306 (-1.97%)|8.549 → 8.606 (+0.67%)|8.685 → 8.558 (-1.47%)
|indirect_float|9.169 → 9.296 (+1.39%)|10.413 → 10.479 (+0.63%)|10.599 → 10.520 (-0.75%)
|arithmetic|1.400 → 1.415 (+1.09%)|2.525 → 2.526 (+0.05%)|2.511 → 2.533 (+0.90%)
|dispatch_control|1.461 → 1.445 (-1.06%)|2.637 → 2.690 (+1.99%)|2.678 → 2.663 (-0.54%)

Target fork ranges:

|!Run|!Baseline fork range|!Candidate fork range
|run-6mtmtlgj|33.567–34.181|31.386–32.356
|run-e_hmd4or|35.136–35.779|33.142–34.130
|run-tin3qowc|35.104–35.882|33.798–34.341

All nontarget fork ranges overlap. Overlap is descriptive, not a significance
test or proof of no regression. The +1.99% dispatch-control median in the first
1024 run repeats as -0.54%; monomorphic changes are +0.74%, +0.36%, -1.07%.
Arithmetic medians are slightly higher in all three trials (+0.05–1.09%).
One candidate `indirect_float` process in the confirmation has a 20.527
ns/element mean (all five windows 20.224–20.795), while the other candidate
processes are 10.438–10.609. Its cause is not established, and it is retained
unchanged in all raw data and summaries. No floating-point improvement or
absence-of-regression guarantee is claimed.

At count 4096, candidate/Java whole-kernel ratios are 12.41x typed polymorphic,
4.12x expression polymorphic, 2.01x monomorphic and 6.47x nullable method.
Java callees remain DONT_INLINE; Qore may inline an exact-base branch.
These are not equal-policy isolated call latencies. There is no IR timing,
startup speedup, cross-architecture or cumulative-across-commits gain claim.

### Correctness and follow-up

Release and debug each pass 56 files: 194 cases, 6785 assertions, eight
standalones. The expanded assignment fixture checks a repeated class-local
cycle root after its source owner is removed, both ordinary and exception
cleanup, weak observation, and slot reuse. The full driver passes 500 assertions
on each frozen baseline as well as each candidate. Existing matrices cover
reflection, recursive locals, class access/generics, debugger/readers, weak and
opaque values, Program/module boundaries and dense/concurrent cycle collection.
Debug native units pass another case/two assertions. Fresh six-mode grids for
both candidate and baseline pass 345 checks per mode, in addition to the paired
AOT checks before each timing run. Baseline checks use the saved compatible
release host executables with each saved release/debug library; candidate
debug checks use separately archived debug host executables.
Eleven harness tests pass. No full-suite or sanitizer qualification is claimed.

The full 62-item audit is recorded in
[FOLLOWUP-AUDIT.md](FOLLOWUP-AUDIT.md#empty-class-local-node-initialization-audit-2026-10-10).
[Complete evidence](results-initial-class-node-macos-arm64.json) retains every
run, control, outlier, raw window, identity, validation log hash and the production
patch. Next: profile the remaining initialization/cleanup split, and investigate
whether immutable local metadata and duplicate method admission work can be
moved out of hot calls. Do not cache class acceptance without a complete
invalidation proof or remove slot/root ownership without proving reflection,
cycle collection and destruction order.

## Fused method admission rejected (2026-10-10)

Decision: do not retain the fused exact-class/native-admission implementation.
Two complete comparisons show target median changes below 1%, with overlapping
process ranges throughout. This is an investigated opportunity, not a new
performance gain. Production, tests and positive implementation documentation
are restored to `c41ca13e4`; the discarded patch and fixture are preserved in
[the evidence](results-fused-method-admission-rejected-macos-arm64.json).

### Experiment and code selection

The preceding profile of expression-polymorphic execution has 7970 samples
under the kernel: 4337 in disjoint native-method callsites/helper siblings and
1297 in the exact guard callsite. Inclusive children are not added twice.
This guided an attempt to combine receiver extraction and live validity checks
into one exact-body/native-override/ordinary-fallback decision.

The experimental versioned helper accepted only context-independent exact
bodies and already-assigned native integer arguments. It retained full PIC-key,
access context, Program, epoch, debugger, body/readiness, sink, stack and frame
checks, with no receiver ownership or cache-layout change. Stable hoisted
receivers and name dispatch retained their original compiler paths. A
compilation-only control restored the separate probes. No runtime validity
snapshot was cached.

An initial unmeasured wrapper spilled 112 bytes because generic extraction
kept native arguments live across cold calls. Isolating that cold branch made
the measured strong-object wrapper frameless and its native transfer a tail
call. The wrapper has 40 static instructions, the prepared native continuation
816, and the old v1 helper changes from 749 to 784; native frames remain
400 bytes. These are whole-function static counts, not executed hot-path
instructions. Different inlining/register allocation accompanies the change;
no causal explanation of the small timing movement is established.

The measured `run-onbac2of/kernels-aot` actually calls the fused helper in
both polymorphic kernels (verified with `otool`, not merely a linked symbol).
A separate O3 LLVM probe confirms that each selected caller has the fused call
and no separate exact or v1 probe at that callsite.

### Measurements

Apple M4 Pro/macOS arm64, release Qore/LLVM 22. The baseline is the final
`candidate-initial-node-O9SDuM` runtime from the previous experiment, whose
source became `c41ca13e4`. Candidate `candidate-fused-method-kvf3ya` adds
production patch SHA-256
`49e94ea19d51bfe00f15914d26ad3bddfdfe5583a899a8126328976d476974d9`.
Both libraries, both compiler outputs and the host tools are frozen and hashed.
Each AOT executable was compiled against and run with its respective runtime;
identical-executable reuse would be invalid for the new helper symbol.

Five processes per kernel/mode, three warmups, five measured 500 ms windows,
five separately reported startup samples, shuffled single-job execution.
`run-onbac2of` uses count 4096, seed 810901 and warmed Java controls;
`run-elo9trao` uses count 1024, seed 810902 and the two Qore modes.
All 200 jobs/1000 windows, raw records, summaries, source/tool/library/executable
identities and 345 reference checks per mode were independently revalidated.
No task build, test, profiling or analysis computation ran during timing.
Host background work, core placement, clocks and filesystem caches remain
uncontrolled. Values are ns/element, median of process means; lower is better.

|!Kernel|!4096 before → candidate (change)|!1024 before → candidate (change)
|polymorphic_call|32.054 → 31.831 (-0.70%)|33.440 → 33.374 (-0.20%)
|polymorphic_expression_call|10.615 → 10.550 (-0.62%)|12.099 → 12.010 (-0.74%)
|monomorphic_call|3.350 → 3.351 (+0.03%)|4.744 → 4.797 (+1.10%)
|optional_method_call|11.253 → 11.250 (-0.03%)|12.773 → 12.754 (-0.15%)
|indirect_wide|7.454 → 7.507 (+0.71%)|8.634 → 8.572 (-0.72%)
|indirect_float|9.155 → 9.227 (+0.79%)|10.416 → 10.399 (-0.17%)
|arithmetic|1.400 → 1.403 (+0.17%)|2.493 → 2.495 (+0.06%)
|dispatch_control|1.458 → 1.452 (-0.40%)|2.669 → 2.648 (-0.79%)

Every before/candidate process range overlaps. This is descriptive, not a
significance test, an equivalence proof or a guarantee of no regression.
Expression medians move -0.62%/-0.74%, typed-polymorphic -0.70%/-0.20%.
Arithmetic medians are slightly higher in both trials; smaller-loop monomorphic
execution is +1.10%. All control results and individual windows are retained.
The evidence does not justify adding this helper/codegen complexity.

The retained baseline's whole-kernel ratios to Java in the 4096 trial are
about 12.3x typed polymorphic, 4.1x expression polymorphic, 2.0x monomorphic
and 6.6x nullable method. Java callees remain DONT_INLINE while Qore can inline
the exact-base case. These are not equal-policy isolated call latencies, and
there is no Java parity, general-call, IR timing or startup-benefit claim.

### Validation, restoration and next work

Candidate release/debug each pass 57 selected files: 195 cases, 6951 assertions,
eight standalones. The new experiment-only driver contributes 166 assertions;
the native-method entry matrix has 522, assigned-method 98 and native-leaf
context 132. Existing coverage includes wide integers, arbitrary override
returns, deletion, side effects, exception merges, weak/opaque values,
reflection, cycle roots, Program/module boundaries, debugger attachment and
concurrent readers. Debug native units add one case/two assertions. Fresh
candidate reference grids pass 345 checks in each of six modes; 11 harness
tests pass. No full-suite, sanitizer or cross-architecture qualification.

The first NativeLeafContext diagnostic was an old-helper-name IR assertion,
not a semantic mismatch. It was updated to recognize either helper before the
complete candidate matrix ran. The earlier unmeasured wrapper and that
diagnostic are identified in the evidence rather than silently discarded.

Candidate qmods were rebuilt for its compiler fingerprint. After removal,
release/debug cores and QUnit/FsUtil/Util dependencies were rebuilt for the
restored fingerprint. Each build then passed the three affected original drivers
(five cases, 752 assertions) and both 345-check IR/AOT reference grids. Symbol
checks confirm the experimental helper is absent. The final commit contains only this rejection report, evidence
and the full [62-item audit](FOLLOWUP-AUDIT.md#fused-method-admission-rejection-audit-2026-10-10).

Next: retain the proven baseline and prioritize the larger remaining class-local
initialization/cleanup cost, then native frame/context setup. Any new
same-Program fast path must retain real-reference bookkeeping, reflection,
cycle roots, destruction order, live invalidation and stack observers.
Do not reintroduce this fused helper unchanged or infer performance from
fewer source-level checks alone.

## Direct local-value discard rejected (2026-10-10)

Decision: reject the broad local-deletion shortcut and restore the runtime
headers exactly to the merged `157323d11` baseline. Retain 29 native cleanup
regression checks and this measurement record. There is **no retained speedup**
from this experiment.
[Raw samples, source patch, identities and validation](results-local-discard-rejected-macos-arm64.json).

### Experiment and correctness contract

A fresh profile of the merged baseline found 1368 cleanup-callsite samples plus
49 sibling samples under 7559 inclusive typed-polymorphic kernel samples
(about 18.7%). Descendants are not added to their parents. The source executable
was `run-03nzhlji/kernels-aot`, SHA-256 `bd0b30faa6749dd9e81979cef8336104b8a9f223d4dd4c617db5bc2f37858b40`.

The candidate replaced `VarValueBase::del()`'s
`removeValue(true).discard(xsink)` with an internal
`QoreLValue::discardForDelete()`. It unassigned the slot before release,
retained real-reference bookkeeping, normalized null/NOTHING nodes, left
borrowed static values on `unassignIgnore()`, and accessed no lvalue members
after final dereference. Scalars remained deletion no-ops after unassignment.
It skipped the temporary QoreValue box/unbox/discard round trip; it did not
remove an owner, a cycle root, runtime type checks or stack observability.
No public ABI, layout or compiler-lowering change was proposed.

The new native checks cover all six storage tags, repeated deletion, NOTHING,
owned and borrowed references, and a node whose final release reenters deletion
and deletes its own variable container. They run through both existing native
unit-test entry points and are retained against the original cleanup path.

Static ARM64 inspection found the candidate's `VarValueBase::del()` reduced
from 68 to 24 instructions, but it tail-called a new 99-instruction helper
containing inlined node-release work. This is not a reduction of the complete
cleanup path to 24 instructions and was not used as evidence of a speedup.

### Paired measurements

Each trial uses five independent forks per mode/kernel, three warmup and five
500-ms measurement windows, five startup samples and a shuffled schedule.
The same AOT executable runs against both compatible libraries within each
trial. Before: release SHA-256 `8d47bf0dc5c0e534771f168d239101e7aa8d78b1d88d35a6f6dd8b397abdfe54`;
after: `5818427c163c33d472cd04c5855a794f10f592caf33c396452bd2df9d8c9c599`.
Frozen libraries, tools, source patches and reference checks identify the
artifacts. Both runtimes pass 345 reference checks before each timing run.

- `run-v5dvs72j`: 4096 iterations, seed 810911, AOT before/after plus Java JIT;
  120 jobs and 600 windows.
- `run-92zsvby7`: 1024 iterations, seed 810912, AOT before/after;
  80 jobs and 400 windows.
- `run-yqajwdd6`: 4096 iterations, seed 810913, independent repeat;
  80 jobs and 400 windows.

All 280 jobs and 1400 windows were re-parsed; fork statistics, raw output,
checksums and source/tool/library/executable identities were verified. No task
build, test, profile or evidence computation ran during timing. Host background
work, core placement, clocks and caches were not controlled.

Median of fork means in ns/element; negative percentages mean faster:

|!Kernel|!4096 first: before -> after|!1024: before -> after|!4096 repeat: before -> after
|polymorphic_call|31.231 -> 30.508 (-2.31%)|32.399 -> 31.727 (-2.07%)|30.835 -> 30.466 (-1.19%)
|polymorphic_expression_call|10.299 -> 10.464 (+1.59%)|11.774 -> 11.803 (+0.24%)|10.279 -> 10.349 (+0.68%)
|monomorphic_call|3.358 -> 3.338 (-0.60%)|4.610 -> 4.582 (-0.60%)|3.302 -> 3.235 (-2.00%)
|optional_method_call|10.883 -> 10.793 (-0.83%)|12.064 -> 12.024 (-0.33%)|10.856 -> 10.873 (+0.16%)
|indirect_wide|7.355 -> 7.212 (-1.94%)|8.244 -> 8.471 (+2.75%)|7.236 -> 7.182 (-0.75%)
|indirect_float|9.071 -> 9.655 (+6.44%)|10.156 -> 10.169 (+0.13%)|8.953 -> 9.141 (+2.10%)
|arithmetic|1.354 -> 1.364 (+0.78%)|2.418 -> 2.384 (-1.42%)|1.347 -> 1.324 (-1.73%)
|dispatch_control|1.426 -> 1.412 (-1.02%)|2.560 -> 2.531 (-1.16%)|1.413 -> 1.401 (-0.87%)

Typed-polymorphic medians improved by 1.19–2.31%, but observed fork ranges
overlap in all three trials. Floating callbacks were slower in both 4096
trials (+6.44% and +2.10%), with overlapping ranges, and essentially unchanged
at 1024 (+0.13%). Wide-integer callbacks were mixed (-1.94%, +2.75%, -0.75%).
Expression receivers were slightly slower in every trial (+0.24–1.59%). These
are reasons not to accept the broad change, not proof of a particular cause or
statistically established regression. An adverse code-layout effect is only
a hypothesis. Overlapping ranges do not prove equivalence or no regression.

Three floating candidate forks in the first trial have all five windows around
9.56–10.01 ns, versus two around 8.89–9.03 ns. The repeat also retains a slow
baseline typed-polymorphic fork (mean 39.465 ns; all five windows 39.089–40.543 ns).
Neither cluster was removed. Scalar controls also move: dispatch is faster
in all three trials, with disjoint ranges in the last two; arithmetic is mixed,
with disjoint faster ranges in the last trial. Thus source-level simplification
cannot be treated as isolated causal attribution.

The first trial's **retained baseline**, not the rejected candidate, is about
12.4x Java for typed polymorphism, 4.4x for expression receivers, 2.0x for
monomorphic calls and 6.5x for nullable methods. These are whole-kernel ratios:
Java methods are DONT_INLINE while Qore can inline an exact-base branch.
There is no equal-policy isolated-call, Java-parity, IR-timing or startup-gain claim.

### Qualification and next work

Candidate release: 73 files, 260 reported cases, 11254 assertions, nine
standalones and one expected debug-only short-string skip. Candidate debug:
73 files, 267 cases, 11353 assertions and nine standalones, no skips. Debug
native-unit driver adds one case/two assertions. The focused native checks
increase from 80/80 in the saved baseline to 109/109, adding 29 cleanup checks.
Fresh reference grids pass 345 checks in each of four release and two debug
modes; all 11 benchmark-harness tests pass. Qmod dependencies were rebuilt
before their respective test suites.

RecursiveClosureLocals initially rejected a linker warning: LLVM emitted a
macOS 28 object while the default link target was macOS 27. The saved baseline
reproduced it. Setting MACOSX_DEPLOYMENT_TARGET=28.0 only for that warning-strict
fixture aligns the link and all 28 assertions then pass; benchmark configuration
is unchanged. Two focused-unit command/loader setup errors and an exploratory
cross-target object-only invocation are also retained in the evidence, separately
from the successful qualification. No production workaround was added.

Next: investigate a narrowly guarded strong-object cleanup path and the larger
class-acceptance/native-frame costs. Do not reintroduce this broad helper
unchanged, eliminate real-reference marks, or borrow owning locals without a
proof covering reflection, cycle roots and destruction order. Java parity is
still unachieved.

After restoration, both cores and QUnit/FsUtil dependencies were rebuilt.
Each build passes five targeted lifetime drivers (21 cases,
849 assertions), and fresh IR/AOT grids pass 345 checks per mode.
Restored debug passes all 109 focused native checks plus the native-unit driver
(one case/two assertions). These checks qualify the final tree separately from
the rejected candidate; no timing benefit is claimed for the restored libraries.
The [full audit](FOLLOWUP-AUDIT.md#direct-local-value-discard-rejection-audit-2026-10-10)
records 14 Pass, 48 N/A and zero Fail across all 62 checklist items.

## Strong-object AOT class-local cleanup (2026-10-10)

Retained a narrowly guarded specialization of the existing
qore_rt_clear_class_local_slot_aot_v1 helper. Typed-polymorphic kernel medians
improve **3.83–4.74%** across three five-fork trials, with disjoint before/after
fork-mean ranges each time. This is a measured incremental gain, not Java parity.
Floating-callback medians are **0.39–1.19% slower** in all three trials, with
overlapping ranges; that small unresolved median tradeoff is accepted and
documented, not dismissed as noise or proof of no regression.

[Complete evidence](results-strong-class-clear-macos-arm64.json) contains
frozen identities and source patch, all 280 jobs / 1400 timing windows,
independently recomputed statistics and raw-window hashes, qualification logs,
LLVM selection, assembly counts and separate sampling-profile hashes.

### Implementation and invariants

The prepared slot caches invocation-local storage, not a type or ownership
decision. Only a currently assigned, nonstatic QV_Node containing a strong
NT_OBJECT takes the specialization. It marks block cleanup and clears assigned
state before the existing real-reference removal and ordinary virtual node
release. Nothing accesses the slot after final release, which can reenter
cleanup and propagate a destructor exception.

Weak, opaque, empty, scalar and borrowed/static slots retain VarValueBase::del;
missing prepared slots retain the original local lookup. No owner, real-reference
mark, runtime-visible local, custom virtual release, exception check or
destruction-order requirement is removed. The generic deletion implementation
and compiler lowering are unchanged. QORE_DISABLE_AOT_STRONG_CLASS_LOCAL_CLEAR,
sampled once on first use, provides a runtime control without recompilation.

The helper grows from 8 to 134 static arm64 instructions because the new branch
inlines ordinary node release; the unchanged generic deletion routine has
68 instructions in both libraries. These are static sizes, not dynamic-path
counts or evidence of a whole-path instruction reduction.

### Repeated measurements

Apple M4 Pro, macOS arm64. Five forks, three warmups and five 500-ms measurement
windows per fork; shuffled order seeds 811001–811003. One identical executable
loads the frozen baseline and candidate runtime within each trial. Runtime
identity is unchanged (371edf33cd7c9979a5ab6de514e5ab771505b74a0737ae5281a5038f33450105).
Baseline source is 14cf060cb (binary revision 54c5d1bbd before its audit-only
amendment); candidate binaries contain 14cf060cb plus the frozen runtime patch.
No task builds, regression tests, profiles or evidence generation overlap timing.

Typed-polymorphic values are median fork means in ns/element; lower is better:

|!Trial|!Count|!Before|!After|!Change|!Before / after fork-mean ranges
|1: run-5btsj_pi|4096|31.489780|30.084395|-4.46%|30.735–31.680 / 29.803–30.449
|2: run-9q5_y339|1024|32.487804|31.241940|-3.83%|32.457–32.602 / 30.938–31.521
|3: run-rhzqjvnf|4096|31.521123|30.027402|-4.74%|31.230–31.735 / 29.820–30.192

All eight kernels are retained below. Parenthesized disjoint means the observed
fork-mean ranges do not overlap; other ranges overlap. These are not confidence
intervals.

|!Kernel|!4096 first|!1024 repeat|!4096 repeat
|polymorphic_call|-4.46% (disjoint)|-3.83% (disjoint)|-4.74% (disjoint)
|polymorphic_expression_call|-1.52%|-0.84% (disjoint)|-1.18%
|monomorphic_call|-0.06%|-1.77%|+0.83%
|optional_method_call|-1.89% (disjoint)|-1.35%|-1.31%
|indirect_wide|-0.01%|-0.12%|-0.41%
|indirect_float|+1.19%|+0.39%|+0.66%
|arithmetic|-0.25%|-0.34% (disjoint)|-1.72%
|dispatch_control|+0.13%|-0.64%|-0.09%

Expression and nullable methods have smaller favorable median changes, but
ranges generally overlap. Monomorphic methods are mixed. The floating control
retains two first-trial candidate fork means of 10.196 and 10.355 ns and a
repeat baseline fork mean of 10.449 ns, including all their windows. Floating
means across all forks therefore move differently from medians: the first
trial is 9.262 → 9.595 ns and the last is 9.316 → 9.043 ns.
Neither outliers nor entire forks were removed. Code-layout effects are a
hypothesis, not an established explanation. No blanket no-regression claim.

The first trial puts the candidate at approximately **11.8x Java** for typed
polymorphism, **4.1x** for expression receivers, **2.0x** for monomorphic calls,
and **6.5x** for nullable methods. These are whole-kernel ratios; Java retained
methods use DONT_INLINE while Qore can inline an exact-base branch. There is
no equal-policy isolated-call, IR-timing, startup-gain or Java-parity claim.

### Qualification and next work

Both builds pass 73 selected files: release 260 reported cases / 11282
assertions / nine standalones, with one expected debug-only short-string skip;
debug 267 cases / 11381 assertions / nine standalones, no skips. The new
destructor-exception and weak/opaque retagging fixtures pass 528 assertions on
both candidates and on the unchanged release baseline. They cover AST, IR,
JIT/tiered, AOT O0/O3, EH, split builds and runtime disable controls. Inspected
LLVM confirms both new functions use prepared-slot getters and clear helpers.

The native-unit driver passes one case / two assertions; all 109 focused native
checks pass. Fresh release IR/AOT/Java-interpreted/Java-JIT and debug IR/AOT
grids pass 345 reference checks per mode. All 11 harness tests pass. QUnit,
FsUtil, HttpServer and HttpServerUtil qmods were rebuilt before each build's
tests. Only the warning-strict RecursiveClosureLocals fixture uses the previously
established MACOSX_DEPLOYMENT_TARGET=28.0 link/object alignment; benchmarks do not.

Separate profiles use the same saved executable before and after. Within the
typed kernel's inclusive parent (7537 before / 7501 after samples), the cleanup
callsite falls from 1370 to 880 samples, with separate cleanup-helper siblings
of 52 / 68. Tail-call attribution is incomplete; unrelated customDeref siblings
are not assigned to that callsite, and descendants are not added to parents.
These profile fractions are not isolated timing measurements. Class-local
initialization remains the largest callsite block (2774 / 2941 samples);
native-method dispatch callsites account for 1327 / 1346.

Next, investigate preparing immutable class-local type/admission metadata once
per invocation, while retaining live representation/ownership checks and
runtime private-access behavior. A caller-accepted parameter annotation alone
does not prove assignment acceptance in the callee. Follow with native
frame/context work that preserves concurrent stack readers and debugger events.
Do not combine real-reference removal with ordinary virtual release without a
separate correctness proof. Java parity remains unachieved.

The [full audit](FOLLOWUP-AUDIT.md#strong-object-aot-class-local-cleanup-audit-2026-10-10)
records all 62 checks. No full-suite, sanitizer or cross-architecture claim.

## Prepared AOT class-local declaration metadata (2026-10-10)

Retained invocation-local preparation of immutable class-local declaration
metadata. Typed-polymorphic kernel medians improve **18.25–18.55%** in two
five-fork trials at counts 1024 and 4096, with disjoint before/after fork-mean
ranges. A single-iteration workload is **0.31% slower**, with overlapping ranges:
there is no short-call speedup claim. This is an incremental AOT improvement,
not Java parity or an improvement to every call kind.

[Complete evidence](results-class-local-prepare-macos-arm64.json) records the
frozen source patch and binaries, qualification, all 280 jobs / 1400 measured
windows, recomputed statistics, raw-window hashes and separate profiles.

### Implementation and invariants

The compiler prepares a borrowed required-class constraint immediately after
this invocation's prepared body-slot getter, outside the loop. LocalVar name,
capture/self/top-level/substitution flags and the immutable type's single
unfiltered QTS_CLASS shape are examined once per invocation. This is preparation
of declaration metadata, not caching an object's acceptance or private access.

The new v3 initializer still checks the exception sink, finalized/assigned/static/
fixed slot state and the live strong-object representation at every store.
It invokes the same class-acceptance implementation for raw/parameterized
compatibility, current inheritance and private access, then acquires the
ordinary slot owner and real-reference mark. It does not remove runtime-visible
locals, ownership, destruction, reflection or stack-reader behavior.
Unsupported metadata or missing prepared storage uses the existing v2/v1 and
ordinary assignment paths. The old ABI entrypoints remain available.

QORE_DISABLE_AOT_CLASS_LOCAL_PREPARE disables selection at compile time and
preparation at runtime; runtime admission also respects all existing pinned,
fresh, strong-store, class-acceptance and node-store controls. The prepared
constraint belongs to this ctx/index/slot and owning Program invocation; it
must not be shared with another Program or recursive invocation's storage.
Substituted, captured, reference and special locals are excluded.

Inspected LLVM has one constraint getter in the typed kernel's entry block and
one v3 initializer in its loop probe. The new helper has 107 static arm64
instructions; the old probe and separate LocalVar initializer have 98 and 204.
Those are complete static function sizes, not executed instruction counts;
branches and callees must not be summed into a dynamic-cost claim.

### Measurements and tradeoffs

Apple M4 Pro, macOS arm64; five forks, three warmups and five 500-ms measurement
windows per fork, shuffled order seeds 812001–812003. Baseline source is
9d5a49fe6 (its previously qualified binary embeds 14cf060cb plus the committed
cleanup patch); the candidate embeds 9d5a49fe6 plus this frozen production patch.
Generated code and runtime identity change, so each trial compiles separate
before/after AOT executables against their matching frozen runtimes.
No task builds, tests, profiles or evidence generation overlap timing.

Typed-polymorphic values are median fork means in ns/element, lower is better:

|!Trial|!Count|!Before|!After|!Change|!Before / after fork-mean ranges
|1: run-gpik3mx5|4096|29.654411|24.154478|-18.55%|29.560–30.909 / 24.045–25.018
|2: run-klc4onwy|1024|31.177462|25.488943|-18.25%|30.964–31.279 / 25.314–25.906
|3: run-ym0lbofq|1|1953.293683|1959.271737|+0.31%|1935.968–1966.881 / 1938.894–1980.965

At count one, the loop selects only its first receiver and surrounding dispatch/
invocation costs dominate. This checks setup amortization in that workload,
not isolated fast-call latency or all possible short functions.

All eight kernels are retained below. Disjoint identifies non-overlapping
observed fork-mean ranges, not confidence intervals; other ranges overlap.

|!Kernel|!4096|!1024|!1
|polymorphic_call|-18.55% (disjoint)|-18.25% (disjoint)|+0.31%
|polymorphic_expression_call|-1.68%|-0.77%|-0.47%
|monomorphic_call|+0.36%|-0.98%|+0.04%
|optional_method_call|-0.57%|-1.19% (disjoint)|+0.64%
|indirect_wide|-2.41%|-0.15%|+0.31%
|indirect_float|+0.06%|-1.55% (disjoint)|+1.11%
|arithmetic|+0.31%|+0.49%|-0.44%
|dispatch_control|+0.31%|+0.44%|+0.13%

The smaller adverse median changes are accepted and disclosed, not dismissed
as noise or asserted absent. In particular, the short floating-callback ranges
overlap only narrowly (baseline 1517.860–1539.494 ns; candidate
1539.182–1550.117 ns). The first long trial retains a 19.707-ns baseline floating
fork mean; all windows and outliers remain in the evidence. Different code layout
is a possible explanation for changes outside the targeted path, not proof.

OpenJDK 25.0.2 Temurin provides the first trial's Java comparison. Candidate
whole-kernel ratios are **9.58x** for typed polymorphism (24.154 vs 2.521 ns),
**4.44x** for expression receivers, **1.97x** for monomorphic calls and **6.57x**
for nullable methods. Java retained methods use DONT_INLINE; Qore can inline
an exact-base branch. These are not equal-inlining-policy isolated-call costs.
There is no new IR-timing, startup-gain, universal no-regression or Java-parity claim.

### Qualification, profiles and next work

Release/debug each pass 73 selected files: release 260 reported cases / 11614
assertions / nine standalones and the expected debug-only short-string skip;
debug 267 cases / 11713 assertions / nine standalones, no skips. Expanded pinned
and class-assignment drivers pass 614 and 634 assertions in each build, including
compile/runtime disable controls, AST references, O0/O3, EH and split compilation.
Their existing fixtures cover private callee context, reflection, recursion,
parameterized/raw types, weak/opaque values, lifetime and destructor exceptions.

The native-unit driver passes one case / two assertions; all 109 focused native
checks and 11 harness tests pass. Fresh release IR/AOT/Java-interpreted/Java-JIT
and debug IR/AOT grids pass 345 reference checks per mode. QUnit, FsUtil,
HttpServer and HttpServerUtil qmods were rebuilt before each build's tests.
Only the warning-strict RecursiveClosureLocals test uses the previously
established MACOSX_DEPLOYMENT_TARGET=28.0 alignment; timing does not.

Sequential ten-second profiles of the separately compiled typed executables
show initialization callsites of 2943 before / 1850 after within inclusive
parents of 7501 / 7497 samples. Native-method callsites are 1289 / 1533 and
cleanup callsites 909 / 1256. Their larger fractions after initialization is
faster do not establish regressions. Tail attribution is incomplete; descendants
are not added to parents and unrelated release siblings are not reassigned.

Next, investigate remaining native-method admission/frame-publication costs,
preserving concurrent readers, debugger events and current RuntimeConfig
semantics. Consider preparing fixed declaration metadata at Program linking
rather than every invocation only after proving metadata lifetime and retargeting
rules; repeat short-invocation measurements for that change. Live class
compatibility still costs time, but must not be cached using a dispatch epoch
that fails to cover every inheritance mutation.

The [full audit](FOLLOWUP-AUDIT.md#prepared-aot-class-local-declaration-metadata-audit-2026-10-10)
records all 62 checks. No full-suite, sanitizer or cross-architecture claim.

## Caller-reserved native-method frame storage rejected (2026-10-10)

Rejected moving native-method diagnostic-frame storage into the generated
caller. Two three-fork screens show **3.09–4.23% slower expression-receiver
calls**, with disjoint observed fork-mean ranges. Typed calls improve in neither
screen. The runtime/compiler change and all associated test adaptations were
removed; the accepted class-local preparation optimization remains.

[Complete evidence](results-native-method-frame-storage-rejected-macos-arm64.json)
archives the rejected nine-file patch, frozen release/debug identities,
qualification, all 72 jobs / 360 measured windows and independently recomputed
statistics. This is a negative experiment, not a shipped ABI change.

### Experiment and qualification

One aligned 256-byte scratch buffer was reserved in each LLVM caller's entry
block and reused only after the preceding synchronous helper returned. The new
v2 helper placement-constructed and destroyed the ordinary context/frame on
every admitted call. It did not cache a published frame, receiver acceptance or
live context. The existing v1 entry remained available.

Receiver validity, the full dispatch-cache key, Program and parse-options
checks, dispatch epoch, debugger/body readiness, both stack guards and explicit
RuntimeConfig remained. Frame publication, exception materialization,
restoration and the concurrent-reader unpublication handshake were unchanged.
The eager-context diagnostic control delegated to v1 after repeating admission;
it did not promise the same physical stack depth as an original v1 call.

The helper's arm64 stack reservation shrank from 400 to 128 bytes, but the caller
added 256 bytes per invocation. Its static instruction count fell from 749 to
438, partly by delegating the cold eager path. Neither figure demonstrates a
dynamic-cost reduction or a 272-byte total-stack saving. LLVM inspection
confirmed a single entry-block scratch allocation and v2 selection for the
typed, expression and nullable callers; scalar cleanup guards were preserved.

Both builds passed 73 selected test files: release 260 cases / 12028 assertions
and debug 267 cases / 12127 assertions, with nine standalones each. Release had
the expected debug-only short-string skip; debug had no skips. The expanded
native-leaf driver passed 276 assertions per build, covering legacy/caller
storage, O0/O3, inline/eager guards and compile/runtime controls. Existing native
method tests covered concurrent stack readers. The native-unit driver, 109
focused native checks and 11 harness tests passed. Fresh release IR/AOT/Java
interpreted/Java JIT and debug IR/AOT grids passed 345 checks per mode.

The runtime-class-context driver initially required the old helper spelling;
its assertion was adapted and the failing final release file rerun successfully.
The initial release compile's undeclared LLVM byte-type name was fixed before
the qualified build. Only those final binaries were measured. QUnit, FsUtil,
HttpServer and HttpServerUtil qmods were rebuilt for each build's qualification.
Only RecursiveClosureLocals used the previously established macOS deployment
target alignment; benchmark environments were unchanged.

### Measurements and decision

Apple M4 Pro, macOS arm64; three forks, three warmups, five 500-ms measurement
windows per fork, and separately compiled baseline/candidate AOT executables.
Baseline source was 2a0631a2a, using its qualified frozen binaries; candidate
source was that revision plus the archived experiment patch. Counts were 4096
and 1024, with shuffled order seeds 813001 and 813002. No task builds, tests,
profiles or evidence generation overlapped timing.

Median fork means are ns/element, lower is better. Positive changes are slower.
Disjoint refers to observed fork-mean ranges, not confidence intervals.

|!Kernel|!4096 before → candidate|!Change|!1024 before → candidate|!Change
|polymorphic_call|24.300830 → 24.461884|+0.66%|25.555659 → 26.038245|+1.89% (disjoint)
|polymorphic_expression_call|10.132406 → 10.560793|+4.23% (disjoint)|11.763918 → 12.127439|+3.09% (disjoint)
|monomorphic_call|3.278187 → 3.352819|+2.28%|4.616671 → 4.778673|+3.51%
|optional_method_call|11.115213 → 11.401107|+2.57% (disjoint)|12.616731 → 12.763833|+1.17%
|indirect_float|9.756125 → 9.931689|+1.80%|10.197007 → 11.326914|+11.08% (disjoint)
|arithmetic|1.326286 → 1.331768|+0.41%|2.408262 → 2.439493|+1.30%

All samples and outliers remain, including the first trial's wide typed and
floating-control ranges. The second trial's adverse floating-control change
is not discarded or causally attributed to method frames without evidence.
There is no startup, new IR-timing, fresh Java comparison or speedup claim.

Moving storage alone leaves per-call frame initialization/publication, live
admission and register spills. The smaller static helper did not translate into
a useful result. Do not repeat this implementation unchanged. Next, examine
which native admission facts are immutable after publication and which still
need live guards; establish their lifetime and invalidation rules before
attempting to move more checks into linking or compilation. Keep receiver
validity, debugger behavior and concurrent stack observability intact.
Initial follow-up inspection found that AOT registration can replace and delete
the old context, and lazy registration can clear it (Function.cpp,
registerPrecompiledAOTFunction and registerLazyPrecompiledAOTFunction). An
unconditionally cached context pointer therefore needs a separate lifetime and
invalidation proof; the dispatch-cache entry's own lifetime is not sufficient.
Compiler-proven body facts and live context identity must be treated separately.
Java parity remains unachieved.

After removal, both core builds and the four qualification qmods were rebuilt.
The original native-method, name-method and native-leaf drivers passed again
in each build (1280 assertions each), as did fresh IR/AOT 345-check grids.
Runtime identity returned to the accepted baseline's value. Rebuilt binary
hashes differ because they embed the later source revision; neither restored
binary is represented as having been timed in these two screens.

The [full audit](FOLLOWUP-AUDIT.md#caller-reserved-native-method-frame-storage-rejection-audit-2026-10-10)
covers the retained documentation/evidence. No full-suite, sanitizer or
cross-architecture claim.


## Floating-method baseline and next optimization (2026-10-10)

Added four matched Qore/Java floating-method kernels after the audited develop
merge `e6ebee141`. **No runtime/compiler optimization is included here.**
This broadens coverage beyond the existing integer-method and float-callback
cases and establishes a measured baseline for extending native method calls.
The [complete evidence](results-float-method-baseline-macos-arm64.json) retains
all settings, samples, fork statistics, binary/source identities and raw hashes.

The kernels use three non-final receiver types and exactly representable double
subtractions, with an independent integer reference. They distinguish a typed
per-iteration receiver local, a direct receiver expression, a stable exact-base
receiver and a nullable receiver. Objects are constructed outside timed loops.
All Java targets retain `DONT_INLINE`; Qore can inline the exact-base case.
Thus these are whole-kernel comparisons including selection, conversion and
ownership work, **not isolated call latency or matched inlining policies**.
The nullable cases time non-null strong objects only.

### Fresh measurements on the unmodified merged runtime

Apple M4 Pro / macOS arm64, Temurin 25.0.2, run `run-akzwimx2`, 4096 elements,
three forks, three warmup and five measured 500 ms windows, randomized seed
814001. Numbers are medians of fork means in ns/element, startup excluded.
All 72 jobs / 360 measured windows were independently rechecked; nothing was
discarded and no task build, test, profile or evidence work overlapped timing.

|!Kernel|!Qore IR|!Qore AOT|!Java JIT|!AOT / Java
|polymorphic_float_call|1022.49|140.16|2.60|54.01x
|polymorphic_float_expression_call|782.64|124.10|2.58|48.03x
|monomorphic_float_call|442.62|8.97|1.67|5.37x
|optional_float_method_call|442.59|145.25|1.67|87.14x
|polymorphic_call|977.82|24.28|2.56|9.48x
|polymorphic_expression_call|740.79|10.31|2.55|4.04x
|indirect_float|448.73|9.12|1.66|5.51x
|arithmetic|94.99|1.45|1.00|1.45x

Observed AOT fork-mean ranges for the four new kernels:

|!Kernel|!Range (ns/element)
|polymorphic_float_call|139.496–140.269
|polymorphic_float_expression_call|122.417–125.072
|monomorphic_float_call|8.816–8.967
|optional_float_method_call|142.510–147.865

The existing int-expression and float-callback controls are around 10.31 and
9.12 ns in AOT. The new floating polymorphic/nullable kernels instead spend
124–145 ns. This identifies a much larger extension opportunity than moving
native-int frame storage, but does not establish the size of a future gain.
The three new receiver arguments add outer-dispatch setup; do not compare these
controls directly against historical harness revisions as a before/after result.
This is one three-fork baseline, not a cross-count stability or startup claim.

### Why this is the next target

Precise LLVM function extraction finds no native-method attempt in any of the
four floating caller bodies and no native-method binding in any of the three
floating target bodies. Each caller retains a general dot-eval fallback plus
float boxing/unboxing. The exact-base case can still use its existing typed
body; the missing extension concerns dynamically resolved overrides and
nullable/name-dispatched calls, not every floating call.

A separate ten-second, 1 ms profile, completed before timing, records 7458
inclusive samples in the top-level executor. At a dominant floating-expression
callsite, 6706 samples include 6569 in the general object-method helper, with
variant dispatch, generic invocation/context and local setup underneath.
These are nested inclusive counts, not independent percentages or additive
costs. Tail attribution and individual callsite visibility are incomplete.
The profile supports bypassing repeated general invocation work with the
existing compiler-proven scalar mechanism; it does not justify removing live
receiver, context, debugger or stack-reader guarantees.

### Qualification and execution plan

All 12 Python harness tests pass. Release IR/AOT/Java-interpreted/Java-JIT,
debug IR/AOT and the three timed modes each pass all 405 reference checks.
The core binaries are unchanged from the fully qualified merged baseline:
release `573bcf7d503983a1084f458676f2ac9820f1d7b0191009fdac4baa22ef5c651b`,
debug `190caee0f81513e813a6ac36081d916abb6f7be1fe18ac90e1cc8cd1f8e3624a`.
Both are frozen with tools and identities in
`baseline-post-merge-5c31ca5c-glYWh9`. They embed d976c5f07 plus the merged
source/audit adjustments now committed as e6ebee141; no future revision is
represented as embedded in them.

The next implementation steps are:

1. Extend whole-body-proven native method companions from `int(int)` to
   `float(float)`, retaining explicit RuntimeConfig and existing integer ABIs.
   Reuse the complete dispatch key, deferred callee context, frame publication
   and concurrent-reader handshake. Keep AOT context/body identity live:
   registration can replace/delete it, so PIC lifetime alone is not sufficient.
2. Preserve boxed fallback return contracts and ownership. Test O0/O3,
   stripped/split/EH compilation, disable controls, exclusions for self/argv/
   effectful bodies, same/cross-Program calls, debugger and stack readers,
   null/override/error paths, signed zero, infinities, NaNs and tag-colliding
   doubles. Requalify existing integer calls in release/debug.
3. Compile before/after AOT artifacts separately against the frozen and new
   libraries. Run isolated randomized multi-fork comparisons at 4096 and 1024
   elements, with short-invocation checks if setup changes warrant them. Keep
   integer-method and float-callback controls and all adverse results.
4. Retain only measured, validated improvements, then audit and commit.
   Investigate IR invocation costs separately: an AOT companion does not by
   itself speed up the much slower interpreter measurements in this table.

The [full audit](FOLLOWUP-AUDIT.md#floating-method-baseline-audit-2026-10-10)
covers this benchmark-only change. No runtime improvement, full-suite rerun,
sanitizer, cross-architecture or Java-parity claim.

## Native floating-method continuations (2026-10-10)

Decision: retain the additive `float(float)` native method path. It substantially
reduces the previously measured floating-method dispatch gap while preserving the
existing integer ABI, same-program/runtime guards and observable frame behavior.
This is an AOT improvement, not an IR speedup or Java-parity claim.

[Complete evidence](results-native-float-method-macos-arm64.json) records frozen
source/tool/library identities, all raw normalized samples and fork summaries,
qualification logs, LLVM/profile hashes and the 62-item audit. The baseline is
the merged runtime from e6ebee141 with the unchanged benchmark expansion from
0fcbe8f00. Candidate binaries were built from 0fcbe8f00 plus the recorded patch,
not from a prospective commit.

### Implementation and scope

The compiler reuses its complete non-throwing, context-independent scalar-body
proof for exact homogeneous `float(float)` methods. It publishes typed v1/v2/v3
companions and passes the live RuntimeConfig through the v2/v3 entries. Three
atomic pointers are appended to QoreAOTContext; existing field offsets and integer
function-pointer ABIs are unchanged. Shared PIC metadata is named for scalar
admission, but the dispatcher must load a matching typed companion from the live
context. It never reinterprets integer entrypoints as floating entrypoints.

Typed and name-dispatched caller continuations carry arguments/results as doubles,
including values whose bit patterns collide with NaN-box tags. A boxed input must
actually be a boxed float to enter this path; coercible integers do not authorize
a float-key cache hit. Successful native local stores avoid repeated boxing and
unboxing. Unknown overrides retain their ordinary boxed return, assignment check,
cleanup, invalidation and exception behavior.

The fixed four-entry PIC retains the full receiver/type/access/Program/parse-policy/
epoch/argument key. Body identity, metadata readiness, debugger, receiver deletion,
stack guards, compact frame publication and concurrent-reader handshake remain
live. AOT context lifetime is not inferred from PIC lifetime: registration can
replace and delete the context. Deferred receiver/class state is materialized on
the exceptional stack path and restored independently of RuntimeConfig's
compatibility snapshot.

The float v1 binding currently supplies admission proof; generic runtime-only
v1 dispatch is still integer-only. Floating calls without a compiler continuation,
or a PIC entry populated before companion publication, can remain general.
Debugger-detach correctness is tested, not fast-path re-admission for that case.
Methods using self/argv, throwing/effectful operations, incompatible signatures,
cross-Program contexts or unsupported mixed int/float expression summaries retain
their established fallback. `QORE_DISABLE_AOT_NATIVE_FLOAT_METHOD_ENTRY=1`
provides a float-only compilation/runtime control.

### Measurements

Apple M4 Pro / macOS arm64; release binaries; three independent forks per
kernel/mode, three warmup windows then five 500 ms measurement windows. Jobs are
randomized by the recorded seed. Before and after AOT executables are separately
compiled against their frozen libraries; no AOT-reuse shortcut is used.
No task build, test, profile or evidence work overlaps timing. All 264 jobs /
1320 measurement windows are retained and independently revalidated.

Count 4096, `run-vvtn2_cq`: median of fork means, ns/iteration; positive
reduction means less elapsed time.

|!Kernel|!Before|!After|!Reduction %|!Java JIT|!After / Java
|polymorphic_float_call|138.860|24.749|82.18|2.560|9.67
|polymorphic_float_expression_call|120.778|10.513|91.30|2.552|4.12
|monomorphic_float_call|8.802|3.339|62.07|1.638|2.04
|optional_float_method_call|142.958|11.139|92.21|1.667|6.68
|polymorphic_call|24.052|23.644|1.70|2.330|10.15
|polymorphic_expression_call|10.158|10.311|-1.51|2.523|4.09
|indirect_float|9.219|9.023|2.13|1.672|5.40
|arithmetic|1.434|1.439|-0.37|0.892|1.61

All four floating-method before/after fork ranges are disjoint at counts 4096
and 1024. The following repeats include every control, not just improved kernels.
The same-runtime control compiles only its before executable with the float-only
feature disabled; both measured processes use the same candidate library without
a runtime disable flag.

|!Kernel|!1024 before|!1024 after|!Reduction %|!Same-runtime 4096 before|!Same-runtime 4096 after|!Reduction %
|polymorphic_float_call|143.597|26.329|81.66|143.216|24.760|82.71
|polymorphic_float_expression_call|122.829|12.347|89.95|122.494|10.455|91.46
|monomorphic_float_call|10.502|4.959|52.78|8.300|3.380|59.27
|optional_float_method_call|144.122|12.682|91.20|142.622|11.311|92.07
|polymorphic_call|25.636|25.626|0.04|24.152|23.795|1.48
|polymorphic_expression_call|12.154|11.918|1.94|10.443|10.378|0.62
|indirect_float|10.415|10.481|-0.63|9.054|9.104|-0.55
|arithmetic|2.768|2.751|0.60|1.456|1.452|0.28

Count 1024 is `run-wzn5iy4f`; same-runtime count 4096 is
`run-pp38q5ar`. Typed float polymorphism reduces time by 81.66–82.71%,
expression polymorphism by 89.95–91.46%, and nullable float methods by
91.20–92.21%. Monomorphic float improves by 52.78–62.07%; its exact-base
path also benefits from keeping the caller continuation native.

The integer-expression control is 1.51% slower in the first 4096 trial with
disjoint observed ranges, but 1.94% faster at 1024 and 0.62% faster in the
same-runtime control. Indirect float changes from 2.13% faster to 0.63%/0.55%
slower; the latter ranges overlap. These results remain in the evidence and
do not establish either a new control optimization or a repeated steady-state
regression. Observed fork ranges are not statistical confidence intervals.

Count one, original before/after `run-j88ksnqk` and additional same-runtime
control `run-3jfkf51h`:

|!Kernel|!Before|!After|!Reduction %|!Same-runtime before|!Same-runtime after|!Reduction %
|polymorphic_float_call|2435.220|2378.034|2.35|2414.888|2361.537|2.21
|polymorphic_float_expression_call|2363.507|2376.432|-0.55|2379.798|2354.880|1.05
|monomorphic_float_call|2193.121|2226.252|-1.51|2220.920|2214.487|0.29
|optional_float_method_call|2387.735|2243.545|6.04|2387.576|2211.649|7.37
|polymorphic_call|2324.349|2368.156|-1.88|2311.444|2311.432|0.00
|polymorphic_expression_call|2301.464|2313.725|-0.53|2266.824|2292.451|-1.13
|indirect_float|1892.230|1887.912|0.23|1893.633|1882.072|0.61
|arithmetic|1801.014|1804.772|-0.21|1796.080|1797.346|-0.07

These small-count kernels include roughly two microseconds of surrounding
dispatch/harness/invocation work. Initial monomorphic-float +1.51% and typed-integer
+1.88% slowdowns have disjoint observed fork ranges; neither repeats in the
same-runtime control (0.29% faster and effectively unchanged, respectively).
The integer-expression short control remains +0.53%/+1.13%, with overlapping
ranges in both runs. Nullable floats improve by 6.04%/7.37%, but the mixed results
do not support a general single-call or startup improvement. Process-launch
samples are recorded with uncontrolled filesystem caches and are not used to
claim an optimization.

Fresh Temurin 25.0.2 Java JIT measurements retain DONT_INLINE target methods;
Qore can inline the exact-base branch. These are whole kernels including
selection, local ownership and conversion, not isolated calls with equal
inlining policy. The floating gap is now about 2.04x for monomorphic, 4.12x for
expression-polymorphic, 6.68x for nullable and 9.67x for typed-polymorphic kernels.
The prior IR baseline remains much slower and is not improved by these AOT
entrypoints.

### Qualification and observed code

Release and debug builds each pass 76 selected files: 314 cases / 12502
assertions in release and 321 / 12601 in debug, plus nine standalone files
within each selection. The release short-string suite has its expected
debug-only skip. The native-unit driver skips in release and passes one case /
two assertions in debug; all 146 native debug checks pass.

The float driver passes 706 assertions per build, covering O0/O3, stripped
sources, split compilation, EH/no-debug-info modes, compiler/runtime disable
controls, optional/string overrides, arguments, deletion, cross-Program calls,
debugger event counts, concurrent first publication and stack readers.
NativeLeafContext passes 174 QUnit assertions per build plus C++ host assertions
for typed and opaque strong receivers, weak/scalar rejection, wrong argument keys,
missing bindings, live body/readiness/Program checks and pending exceptions.
The real generated v3 and v2 companions exercise null-limit fallback, forced
stack overflow, private-context materialization and independent snapshot
restoration. Signed zero, infinities, NaNs and tag-colliding finite values pass.

An initial debugger fixture mixed a float parameter with integer literals, which
does not satisfy the existing whole-body float proof. Its binding assertion
failed. The fixture now uses explicit floating literals, and a separate mixed
literal body is asserted to remain excluded. No production proof was weakened.

Fresh release/debug IR/AOT grids and every measured mode each pass all 405
independent references. All 12 Python harness tests pass. QUnit, FsUtil,
HttpServer and HttpServerUtil qmods are rebuilt sequentially for each build.
Only RecursiveClosureLocals uses the previously reproduced macOS deployment-target
adjustment. No full-suite, sanitizer or cross-architecture claim is made.

Generated LLVM contains one native float attempt and a double continuation in
each of the four normal benchmark callers. A separate ten-second, 1 ms profile
finished before timing: the new helper is visible, and general method dispatch no
longer dominates the expression workload. Inclusive branches are nested and tail
attribution is incomplete; they are not additive costs or precise guard timings.

### Next optimizations

1. Measure float method shapes still on general dispatch: runtime-only calls
   without native assignments and first publication under a debugger. Extend
   typed generic dispatch or re-admission only with complete ownership/body/
   debugger proofs and a dedicated before/after benchmark.
2. Extend mixed int/float whole-body proof where language conversion, overflow,
   exceptions and signed-zero behavior can be preserved; keep division and
   effectful bodies excluded unless separately proven.
3. Profile the remaining approximately 25 ns typed-local versus 10.5 ns expression
   cost. Investigate local ownership/assignment lowering and prepared dispatch,
   without deleting real references, using stale context pointers or bypassing
   virtual destruction and frame-reader guarantees.
4. Profile IR invocation separately and target prepared same-program scalar
   dispatch. An AOT-only result must not be presented as an interpreter gain.

The [audit](FOLLOWUP-AUDIT.md#native-floating-method-audit-2026-10-10) reports
19 Pass, 43 N/A and no failures. Java-level retained-call speed remains an open
goal; this iteration closes a substantial floating-method gap.

## Generic floating-method dispatch (2026-10-10)

Decision: retain native floating dispatch for generic AOT callers, with the two
internal invocation/cleanup closures forced inline. This closes the runtime-only
gap left by the preceding native-result continuation work. It does not change
compiler lowering or the ABI, and does not establish Java parity or an IR speedup.

[Complete evidence](results-generic-float-method-macos-arm64.json) preserves the
initial baseline, the first candidate and its repeated integer regression, and
the qualified revised candidate. All 406 jobs / 2030 measurement windows across
these stages are independently revalidated; 186 jobs / 930 windows belong to the
accepted revision. No task build, test, profile or evidence work overlaps actual
timing. The first candidate's qualitative profile briefly overlapped the harness
unit test; the revised profile was captured separately after qualification.

### Scope and proof

The new `boxed_float_method_call` kernel carries an `auto` floating recurrence
through three dynamic receivers. `dynamic_float_result_call` passes a native float
but consumes each unknown method result directly as an integer, independently
advancing its native recurrence. LLVM inspection finds ordinary boxed method
dispatch and no native method try-helper in either caller. The reference grid now
covers 29 kernels, 435 checks per mode, with independent integer-exact checksums.

The original 541602b19 baseline (`run-u_hayg9s`) measured roughly 201/174 ns per
iteration for these AOT kernels and 757/752 ns in IR. Its qualitative profile
placed most sampled work beneath generic method dispatch. The new runtime uses
the already-published, whole-body-proven homogeneous `float(float)` companions
for actual floating arguments. It prefers v3/v2 with explicit RuntimeConfig;
prepared hits use the existing compact frame/deferred receiver-class context.
V1-only and late/uncached admission retain eager context. Late admission never
mutates a previously published immutable PIC descriptor.

Full-key dispatch, live context/body identity and metadata readiness, same-Program
checks, debugger attachment, receiver deletion, stack guards and frame-reader
publication remain. Integer generic dispatch retains its eager v1 convention.
Arguments remain borrowed on a miss; handled cleanup is consumed once. Special
float encodings and arbitrary boxed fallback results preserve ordinary ownership.
No proof is inferred from mere coercibility or an old context pointer.

`QORE_DISABLE_AOT_GENERIC_NATIVE_FLOAT_METHOD_ENTRY=1` is a runtime-only control
for this path. Native-result caller continuations remain enabled. Methods with
unsupported bodies/signatures, integer arguments needing float coercion, or
cross-Program contexts retain the existing fallback.

### Revised candidate measurements

Apple M4 Pro, macOS arm64, release build; median of independent fork means in
ns/iteration, excluding startup. Each fork has three warmup windows and five
500 ms measurement windows. All before/after trials reuse one identical AOT
executable against the recorded libraries. The source baseline is 541602b19;
the candidate is that parent plus the recorded patch, not a prospective commit.

Default path, count 4096, three forks, `run-s0by7y7b`:

|!Kernel|!Before|!After|!Reduction %|!Java JIT|!After / Java
|boxed_float_method_call|202.649|64.353|68.24|2.551|25.23
|dynamic_float_result_call|174.522|35.908|79.43|3.466|10.36
|polymorphic_float_expression_call|10.516|10.372|1.37|2.560|4.05
|polymorphic_expression_call|10.271|10.349|-0.76|2.521|4.11
|indirect_float|9.002|8.971|0.34|1.631|5.50
|arithmetic|1.417|1.419|-0.15|0.891|1.59

Count 1024 is `run-18gwp9_v`. The same-runtime control, `run-04p4fyko`, uses
count 4096 and the same candidate library/executable on both sides; only before
runtime processes receive the generic-float disable flag. Both use three forks.

|!Kernel|!1024 before|!1024 after|!Reduction %|!Same-runtime before|!Same-runtime after|!Reduction %
|boxed_float_method_call|203.778|66.321|67.45|204.776|63.683|68.90
|dynamic_float_result_call|174.823|37.383|78.62|175.020|35.892|79.49
|polymorphic_float_expression_call|12.214|12.330|-0.95|10.551|10.501|0.47
|polymorphic_expression_call|12.056|11.982|0.61|10.435|10.363|0.69
|indirect_float|10.459|10.457|0.02|9.043|8.982|0.68
|arithmetic|2.779|2.759|0.74|1.420|1.411|0.62

The new floating kernels have disjoint observed before/after fork ranges in all
three trials. Small adverse native-expression/arithmetic control changes are
retained; their ranges overlap and their direction does not persist across
repeats. Observed ranges are not confidence intervals. No new optimization of
these unchanged controls is claimed.

### Integer regression caught and revised

The first candidate substantially improved the new floating kernels, but the
v1-only control exposed integer slowdowns. With
`QORE_DISABLE_AOT_NATIVE_METHOD_CONTINUATION=1` at compilation and execution,
`run-d2h_okg3` measured typed integer +1.86% and integer expression +4.56% slower,
with disjoint observed ranges. A five-fork repeat, `run-b4_q8occ`, measured
+1.77%/+3.28%; the expression repeat had an overlapping range and a slow baseline
fork, all retained in the evidence. That candidate was not accepted unchanged.

Symbol inspection found new outlined invocation/cleanup lambda operators,
including integer specializations. Forcing just these two closures inline removes
those symbols. The revised five-fork v1-only trial, `run-mp6pe_lk`, gives:

|!Kernel|!Before|!After|!Reduction %
|boxed_float_method_call|200.513|72.263|63.96
|dynamic_float_result_call|172.796|44.099|74.48
|polymorphic_call|46.484|46.522|-0.08
|polymorphic_expression_call|29.722|29.621|0.34
|indirect_float|9.058|9.084|-0.28
|arithmetic|1.418|1.418|0.01

Both integer controls now overlap their baseline ranges: typed integer
46.236–46.607 before / 46.209–46.679 after; expression 29.601–30.000 before /
29.542–29.682 after. The earlier repeated regression is no longer observed.
The v1-only floating benefit is smaller than the default v2/v3 path, as expected
for its eager context/frame convention, but remains substantial.

### Qualification and remaining gap

Both revised builds pass 76 selected test files: release 314 cases / 12764 QUnit
assertions, debug 321 / 12863, plus nine standalone fixtures in each. The expanded
float driver contributes 932 assertions per build; NativeLeafContext contributes
210 plus native host assertions. Tests cover v1/v2/v3 selection, late admission,
integer coercion fallback, consumed ownership, signed zero/NaN/infinities and
tag-colliding doubles, forced stack overflow and recovery, debugger attach/detach,
concurrent readers/calls, deletion and cross-Program fallback. Fresh IR/AOT grids
pass 435 checks per mode/build; 146 debug-native checks and 13 harness tests pass.
The existing release-only short-string skip is retained. Only the established
macOS deployment-target workaround for RecursiveClosureLocals is used.
No full-suite, sanitizer or cross-architecture qualification is claimed.

The revised isolated profile still shows generic dispatch, full-key PIC lookup,
RuntimeConfig/TLS retrieval, float boxing/unboxing, result conversion and cleanup.
These inclusive samples are nested, not additive cost estimates. The profile
supports a remaining wrapper/representation bottleneck, not a claim that any one
check accounts for the whole gap. Java keeps native doubles and retained
`DONT_INLINE` targets while Qore's `auto` contract is broader; the measured ratios
are whole-kernel comparisons, not equal-representation isolated call latency.
The already-native expression paths remain about 4x Java here; these generic
float callers remain about 10x–25x. No new IR or single-iteration improvement is
measured in this runtime-only change.

Next experiments, in order:

1. Extend compiler native try-helper emission to already-native scalar arguments
   whose result is consumed without a native assignment. Pass the hoisted
   RuntimeConfig explicitly; preserve boxed results, arbitrary override fallback,
   cleanup, mutation invalidation and exceptions. Measure separately compiled
   before/after executables and retain integer/default/disabled-path controls.
2. Consider an actual-type guard for boxed float arguments only after that result
   path is validated. Do not treat coercibility as native admission.
3. Profile prepared same-Program IR invocation separately; AOT runtime gains do
   not imply interpreter gains. Preserve frame observability and real lifetimes.

The [62-item audit](FOLLOWUP-AUDIT.md#generic-floating-method-audit-2026-10-10)
reports 19 Pass, 43 N/A, zero failures. Java-level call efficiency remains open.

## Native name-method calls with boxed results (2026-10-10)

Decision: retain the compiler extension for already-native integer/float arguments
whose unknown method results do not feed an exact native-local assignment. The
measured direct-result workloads improve repeatedly by roughly 50–65%. This is
an AOT improvement, not an IR speedup or Java parity. Unsupported targets can pay
a small extra failed-attempt cost before ordinary dispatch.

[Complete evidence](results-boxed-result-method-macos-arm64.json) records 270
randomized jobs / 1350 measurement windows, including 72 baseline jobs and 198
candidate/control jobs. Source, binary and raw-output identities and all adverse
controls are retained. No task build, test, profile or evidence work overlaps
timing. A separate qualitative profile was captured between completed trials.

### Scope and baseline

The source parent is f7c9da741, the clean merge of origin/develop 1800de49a after
the preceding cf3f68118 optimization. That upstream change affected connection
option modules, not the compiler/runtime. Both builds and 109 affected/support
qmod targets were rebuilt. The upstream connection-option test passes four cases
and 28 assertions per build with the restricted local module search. The first
unrestricted attempt failed on an installed external ABI-1 qmod; a temporary
external-module rebuild attempt also failed. These attempts and remaining
external-provider fallback warnings are recorded, not represented as clean
full-environment qualification. No installed module was overwritten.

The frozen baseline contains the expanded benchmark and initial new regression
fixtures but unchanged compiler/runtime sources. The candidate is that source
parent plus the recorded implementation patch. Its binaries were built before
commit; no prospective commit hash is assigned to them. The runtime compatibility
identity changes with the source build, while the runtime ABI layout and helper
signatures do not change. All comparisons compile separate AOT executables.

Two new integer kernels distinguish a useful hit from a deliberate miss:
`dynamic_int_result_call` passes a native integer and converts the unknown result
directly; `dynamic_int_result_fallback` calls a context-using wrapper that delegates
to the selected virtual target and has no native leaf companion. Java retains
both the wrapper and target calls with `DONT_INLINE`. The existing floating
direct-result kernel has the corresponding native-double argument shape.
All 31 kernels have independent integer-exact references: 465 checks per mode.

### Lowering and correctness boundary

The compiler selects the integer or floating helper from the actual native
argument representation. A matching sole native-local store still receives its
unboxed continuation. Other consumers receive an owned boxed result, including
wide integers and tag-colliding doubles. The existing warmed full-key PIC,
hoisted RuntimeConfig, native companions and live guards are reused. Boxed and
optional arguments remain excluded; no whole-body proof is broadened.

Ordinary fallback still permits arbitrary return types, side effects, caller-local
mutation and exceptions. Argument consumption, result ownership, pending-sink and
invoke handling, stack guards, receiver deletion, Program/access context, debugger
checks and concurrent diagnostic-frame publication remain. The compiler-only
`QORE_DISABLE_AOT_NAME_METHOD_BOXED_RESULT=1` control preserves the older native-local
continuation while disabling the new boxed-result consumers.

The new suite covers numeric encoding boundaries, conversions, string/NOTHING/
object/throwing/self/generic/private/gate overrides, weak/null/deleted receivers,
owned result destruction, cross-Program recovery, debugger and concurrent readers.
Normal and exceptional reflective receiver mutation are checked through ordinary
AOT function entries with nullable class locals, plus the AST reference. Existing
IR/JIT caller-frame visibility and reflected non-class/native-local freshness
limitations are not fixed or claimed covered. An initial fixture assertion
incorrectly assumed a catch-visible parameter had native storage; explicit
integer conversion supplies the intended native operand, and the final fixture
also passes on the frozen baseline. No compiler proof was relaxed.

### Measurements

Apple M4 Pro, macOS arm64 release build; median of independent fork means in
ns/iteration, excluding startup. Each fork uses three warmup and five 500 ms
measurement windows. Java is the recorded local Temurin 25.0.2 JIT. These are
whole workloads with unequal dynamic representation/inlining contracts, not
isolated call latency.

Main trial, count 4096, three forks, `run-_w73c0g2`:

|!Kernel|!Before|!After|!Reduction %|!Java JIT|!After / Java
|dynamic_int_result_call|38.404|13.412|65.08|3.070|4.37
|dynamic_float_result_call|35.881|16.832|53.09|3.471|4.85
|dynamic_int_result_fallback|298.429|300.077|-0.55|3.000|100.02
|boxed_float_method_call|63.885|64.433|-0.86|2.554|25.23
|polymorphic_expression_call|10.380|10.294|0.83|2.334|4.41
|polymorphic_float_expression_call|10.234|10.438|-2.00|2.554|4.09
|indirect_float|9.069|9.093|-0.26|1.640|5.54
|arithmetic|1.428|1.429|-0.12|0.892|1.60

Count 1024 is `run-279_d2wb`. Same-runtime `run-kobhnoqv` uses count 4096
and identical candidate libraries, but separately compiled executables; only
before compilation receives the new disable flag. Both use three forks.

|!Kernel|!1024 before|!1024 after|!Reduction %|!Same-runtime before|!Same-runtime after|!Reduction %
|dynamic_int_result_call|39.375|14.936|62.07|38.313|13.673|64.31
|dynamic_float_result_call|37.373|18.614|50.19|36.501|17.165|52.97
|dynamic_int_result_fallback|303.208|303.129|0.03|301.892|306.029|-1.37
|boxed_float_method_call|66.299|65.465|1.26|64.668|64.988|-0.49
|polymorphic_expression_call|12.247|12.150|0.79|10.501|10.407|0.89
|polymorphic_float_expression_call|12.114|12.289|-1.45|10.609|10.476|1.25
|indirect_float|10.463|10.659|-1.88|9.145|9.242|-1.07
|arithmetic|2.829|2.801|1.01|1.471|1.471|0.04

The targeted kernels have disjoint before/after fork ranges in all three trials.
The unsupported-body workload is not improved: a failed native attempt precedes
ordinary dispatch. Its observed change is -0.03% to +1.37% elapsed time across
these trials. The 1.37% same-runtime slowdown has disjoint observed fork ranges;
this is a measured tradeoff, not dismissed as noise.

A five-fork same-runtime repeat, `run-k6zd83oj`, examines the adverse controls:

|!Kernel|!Before|!After|!Reduction %|!Before fork range|!After fork range
|dynamic_int_result_fallback|300.633|303.002|-0.79|297.018–302.095|299.536–312.272
|polymorphic_float_expression_call|10.529|10.371|1.50|10.395–10.655|10.284–10.516
|indirect_float|9.025|9.065|-0.44|8.971–9.101|9.028–9.125

Ranges overlap in this repeat; they are not confidence intervals. The floating
expression's initial 1.45–2% slowdown reverses in both same-runtime trials.
All five unchanged control functions have identical normal-entry LLVM after
debug-ID removal and branch-profile-ID resolution. This supports unchanged
lowering, not a proof of identical final machine-code placement or zero cost.
Small adverse changes are retained. The large direct-result gains justify this
scoped change, with unsuccessful admission an explicit next target.

### Qualification and next optimizations

Both builds pass 77 selected files: release 316 cases / 14075 assertions, debug
323 / 14174, with nine standalone files each. Release skips the debug-only native
unit assertion group; debug runs it. The new driver has 1311 assertions per build.
Fresh IR/AOT grids pass all 465 references per mode; every measured mode also
passes its complete grid. Four support qmods are rebuilt sequentially per build.
The harness passes 14 tests and focused native debug tests pass 146/146.
No full-suite, sanitizer, cross-architecture or single-iteration claim is made.

A separate ten-second, 1 ms profile of the floating direct-result workload shows
the native helper, receiver `qore_rt_deref_if_reference`, `qore_rt_to_int`, and
result `qore_rt_decref` among the remaining work. Sampled branches are nested and
tail attribution is incomplete; they are not additive or precise guard timings.

Next experiments, each requiring fresh before/after measurement and audit:

1. Avoid boxing/conversion/cleanup on native hits when the sole consumer is an
   explicit scalar conversion, preserving arbitrary fallback values and Qore's
   NaN/infinity/out-of-range conversion behavior.
2. Specialize receiver dereferencing using proven representation or guarded
   strong-object checks, retaining weak/reference ownership and deletion behavior.
3. Reduce repeated failed admission for unsupported targets without stale
   body/context pointers or assumptions that all hierarchy changes bump an epoch.
4. Profile prepared same-Program IR dispatch separately; this AOT-only change
   does not narrow the interpreter gap. Context-using and cross-Program calls
   still need separate calling-convention work and remain far from Java here.

The [audit](FOLLOWUP-AUDIT.md#boxed-result-method-audit-2026-10-10) records all
62 checks: 15 Pass, 47 N/A, no outstanding failures.

## Native call-result conversions (2026-10-10)

Decision: retain the single-use scalar-conversion continuation for its repeated
floating-result gains. Dynamic floating calls improve by 14–17% and typed floating
callbacks by 20–23%. Small-integer results show no consistent benefit. Some controls
are slower, including floating-polymorphic calls in every trial; those costs are
retained and remain unexplained, not dismissed as noise. This is not Java parity.

[Complete evidence](results-native-call-conversion-macos-arm64.json) records
360 randomized jobs / 1800 measurement windows: 90 baseline jobs, 90 main
before/after/Java jobs, two 60-job repeats/controls and a 60-job five-fork control
repeat. No task build, test, profile or evidence work overlapped timing.

### Source and correctness boundary

Source parent 738a26661 already contains the preceding boxed-result optimization;
origin/develop 1800de49a was already merged. The frozen baseline copies previously
qualified binaries whose compiler/runtime source hashes match that commit. Its
identity retains the original pre-commit build provenance, rather than claiming a
new post-commit build. Only the benchmark gains two callback-result kernels.
The candidate is the stated parent plus its saved implementation patch. All AOT
comparisons compile separate executables. No runtime ABI/helper signature changes.

The compiler counts all IR uses, including return/PHI uses, and requires producer
lowering before a sole ToInt/ToFloat consumer. Typed function references and
resolved/unresolved native scalar methods expose an unboxed side channel.
Native bool conversions use unsigned widening. Float-to-int retains
qore_rt_float_to_int for NaN, infinity and out-of-range values; no new LLVM
fptosi poison is introduced. A miss retains arbitrary owned boxed results,
conversion, exceptions, invoke routing, local invalidation and cleanup.
Escaping/multiple consumers remain boxed. Existing Program/access, receiver,
body/readiness, stack, debugger and concurrent frame-publication checks remain.

The compile-only QORE_DISABLE_AOT_NATIVE_CALL_CONVERSION=1 control restores the
boxed conversion path while leaving the preceding native-store and boxed-result
optimizations enabled. Existing broader controls retain their scope.

The regression fixture exposed a pre-existing discrepancy: AST rejects int(list)
with RUNTIME-OVERLOAD-ERROR, while baseline IR/AOT return zero. Object conversions
also differ. Tests compare these fallbacks with each backend's independent ordinary
boxed conversion and verify owned-result destruction; they do not claim AST/IR
semantic parity. The minimal list reproducer and baseline logs are retained.
The final fixture passes baseline AST/IR/JIT/tiered and stripped AOT O0/O3.

### Measurements

Apple M4 Pro, macOS arm64 release build; median of fork means in ns/iteration,
startup excluded. Three warmup and five 500 ms measurement windows per fork.
Java is local Temurin 25.0.2 JIT with retained target calls. These whole workloads
have unequal representation/inlining contracts and are not isolated call latencies.

Main count 4096, three forks, run-x1nr0572:

|!Kernel|!Before ns|!After ns|!Reduction %|!Java JIT ns|!After / Java
|dynamic_int_result_call|13.639|13.618|0.15|3.107|4.38
|dynamic_float_result_call|17.199|14.298|16.87|3.517|4.07
|indirect_int_result|6.940|7.199|-3.72|1.909|3.77
|indirect_float_result|11.445|8.826|22.88|2.778|3.18
|dynamic_int_result_fallback|306.050|304.814|0.40|3.028|100.66
|boxed_float_method_call|64.791|65.081|-0.45|2.584|25.18
|polymorphic_expression_call|10.482|10.638|-1.49|2.552|4.17
|polymorphic_float_expression_call|10.802|10.886|-0.78|2.590|4.20
|indirect_float|9.201|9.210|-0.10|1.658|5.56
|arithmetic|1.471|1.481|-0.64|0.999|1.48

Count 1024 is run-fqmsqhya; same-runtime count 4096 is run-9kz3f84c.
Both use three forks. The same-runtime trial changes only before compilation,
setting the new disable flag; both executable runs use the same candidate library.

|!Kernel|!1024 before|!1024 after|!Reduction %|!Same-runtime before|!Same-runtime after|!Reduction %
|dynamic_int_result_call|14.890|14.862|0.19|13.720|13.327|2.87
|dynamic_float_result_call|18.565|16.005|13.79|16.945|14.070|16.97
|indirect_int_result|8.460|8.321|1.65|6.826|6.982|-2.28
|indirect_float_result|12.978|10.325|20.44|11.428|8.871|22.37
|dynamic_int_result_fallback|306.444|313.093|-2.17|304.065|304.039|0.01
|boxed_float_method_call|66.740|66.906|-0.25|64.840|65.021|-0.28
|polymorphic_expression_call|12.170|12.245|-0.62|10.324|10.542|-2.12
|polymorphic_float_expression_call|12.526|12.710|-1.47|10.594|11.030|-4.12
|indirect_float|10.699|10.647|0.49|9.072|9.170|-1.09
|arithmetic|2.850|2.860|-0.36|1.462|1.476|-0.94

The two floating-result workloads have disjoint before/after fork ranges in all
three trials. Integer callback results vary from a 3.73% slowdown to a 1.65% gain;
there is no consistent benefit. The unsupported fallback's 2.17% slowdown at
count 1024 has disjoint observed ranges, but does not repeat at count 4096.
The main boxed-floating and integer-polymorphic controls also have small
disjoint-range slowdowns. All raw samples are retained.

Five-fork same-runtime adverse-control repeat, run-5dwy9189:

|!Kernel|!Before ns|!After ns|!Reduction %|!Before fork range|!After fork range
|indirect_int_result|6.969|6.900|1.00|6.919–9.270|6.787–7.081
|polymorphic_expression_call|10.474|10.431|0.41|10.290–10.588|10.180–10.517
|polymorphic_float_expression_call|10.660|10.881|-2.08|10.577–10.835|10.740–11.007
|dynamic_int_result_fallback|308.666|309.181|-0.17|302.295–313.134|302.047–322.045
|boxed_float_method_call|64.844|64.726|0.18|63.779–65.721|64.244–65.214
|arithmetic|1.456|1.476|-1.41|1.453–1.480|1.454–1.477

All repeat ranges overlap; these ranges are not confidence intervals. The integer
callback slowdown reverses, with an unusually slow before fork retained. The
floating-polymorphic median remains 2.08% slower (0.78–4.12% across all trials);
arithmetic remains 1.41% slower in this repeat. The five unchanged control
functions have identical normalized LLVM bodies for every emitted entry, including
fast entries where present. That does not prove equal final instruction placement
or explain the observed costs. Retention is a scoped tradeoff for the larger,
repeatable floating-result gains, not a universal speedup claim.

### Qualification and next work

Release passes 78 selected files, 317 cases / 15680 assertions, with one known
release-only skip; debug passes 78 files, 324 cases / 15779 assertions, no skips.
Both have nine standalone files. The new driver passes 1142 assertions and the
updated boxed-result/debugger/reader driver 1774 per build. Four supporting qmod
targets and their dependencies were rebuilt before each build's tests.
The 33-kernel grid has 495 references per mode: release IR/AOT/Java-int/Java-JIT
and debug IR/AOT pass. All 15 harness tests and 146 native debug checks pass.
No full-suite, sanitizer, cross-platform or new IR-speedup claim is made.

A separate ten-second 1 ms profile after the three primary timing trials records
7655 top-level inclusive samples. Native-entry dispatch/frame/stack work and
receiver dereferencing remain prominent; the existing float-to-int helper is
still visible. Inclusive/nested counts are not additive and tail attribution is
incomplete. The profile is qualitative, not another timing result.

Next priorities are guarded strong-object receiver dereferencing, repeated failed
native admission, and explicit-context/frame costs for same-Program calls. Preserve
weak/reference resolution, receiver lifetime, live metadata and diagnostic readers.
Measure wide-result and mixed scalar-conversion workloads before claiming any
integer advantage. Investigate the persistent small control costs, and separately
profile IR and context-using/cross-Program calls; the generic fallback here remains
roughly 101 times Java's whole-workload time. The improved floating-result kernels
still take 3.18–4.07 times Java's time.

The [audit](FOLLOWUP-AUDIT.md#native-call-conversion-audit-2026-10-10) records every
check: 18 Pass, 44 N/A, 0 Fail.

## Direct reference-tag loads (2026-10-10)

Decision: retain the runtime-only untyped-load optimization. Across three trials,
dynamic integer calls improve by 12.6–14.7%, dynamic floating calls by 11.3–12.7%,
and boxed floating calls by 2.8–4.3%. These are whole-workload gains, not isolated
call latencies or Java parity.

[Complete evidence](results-reference-tag-load-macos-arm64.json) records
300 randomized jobs / 1500 measurement windows: 90 fresh baseline jobs, 90 main
before/after/Java jobs, 60 count-1024 jobs and 60 same-AOT runtime-library-control
jobs. No task build, test, profile or evidence work overlapped timing.

### Source and correctness boundary

Source parent d3a29966b contains the preceding native-result conversion change;
origin/develop 1800de49a was already merged. The frozen baseline copies qualified
binaries whose source hashes match that commit, preserving original pre-commit
build provenance. The candidate is that parent plus the saved implementation patch.
No benchmark, compiler, runtime ABI, helper signature or compatibility identity
changes in this iteration.

The old hasNode() predicate already admitted only non-null ordinary/opaque pointer
representations. For those values, generic QoreValue::getType() returns the same
immutable node tag. Reading it directly avoids scalar/enum classification without
changing reference resolution, ownership or exception propagation. Inline values,
including enums, remain untouched; weak wrappers keep their existing handling.
Generated arm64 assembly returns on non-reference paths before creating a C++
stack frame; only the reference path retains its frame and evaluation work.

All 129 non-bootstrap emitted LLVM entry bodies are identical after debug/profile
metadata normalization. Only main's bootstrap metadata length differs (27936 to
27934 bytes); the serialization cause is not established. A separate control uses
the exact same compiled executable with each runtime library, verified by SHA-256
and matching RUID. It repeats the principal gains without that bootstrap difference.

The fixture exposed existing baseline limitations, not new failures. An expired
weak reference retains a distinct deleted-object representation, not plain NOTHING;
calling fullType() on it works in AST but raises OBJECT-ALREADY-DELETED in baseline
IR. Tests compare the expired weak value directly. Storing an iterator's raw
reference into an IR-only auto local raises LVALUE-ERROR in baseline IR/tiered.
The default parameter form passes all four modes; raw-local mode remains tested
in AST/JIT/AOT and is retained as a baseline IR reproducer. Neither discrepancy is
claimed fixed. Initial diagnostics and final baseline passes are preserved.

### Measurements

Apple M4 Pro, macOS arm64 release build; median of fork means in ns/iteration,
startup excluded. Each fork uses three warmup and five 500 ms measurement windows.
Java is local Temurin 25.0.2 JIT with retained target calls. Representation and
inlining contracts remain unequal.

Main count 4096, three forks, run-6xkx_o7r:

|!Kernel|!Before ns|!After ns|!Reduction %|!Java JIT ns|!After / Java
|dynamic_int_result_call|13.630|11.654|14.49|3.091|3.77
|dynamic_float_result_call|14.147|12.381|12.48|3.473|3.56
|indirect_int_result|6.886|6.828|0.84|1.909|3.58
|indirect_float_result|9.141|8.732|4.47|2.937|2.97
|dynamic_int_result_fallback|304.024|298.017|1.98|3.026|98.50
|boxed_float_method_call|64.135|61.412|4.25|2.549|24.09
|polymorphic_expression_call|10.545|10.402|1.36|2.513|4.14
|polymorphic_float_expression_call|10.754|10.508|2.29|2.555|4.11
|indirect_float|9.174|9.234|-0.65|1.629|5.67
|arithmetic|1.419|1.427|-0.61|0.888|1.61

Count 1024 is run-p_jc14_r; same-AOT count 4096 is run-iev9jcc1. Both have
three forks. Main and count-1024 trials compile separate AOT executables;
the same-AOT trial changes only the loaded runtime library.

|!Kernel|!1024 before|!1024 after|!Reduction %|!Same-AOT before|!Same-AOT after|!Reduction %
|dynamic_int_result_call|14.876|13.000|12.61|13.274|11.328|14.66
|dynamic_float_result_call|15.756|13.972|11.32|14.035|12.252|12.70
|indirect_int_result|8.387|8.295|1.10|6.834|6.828|0.10
|indirect_float_result|10.591|10.104|4.60|8.879|8.616|2.96
|dynamic_int_result_fallback|305.783|301.058|1.55|300.444|303.089|-0.88
|boxed_float_method_call|65.623|63.795|2.79|64.772|61.968|4.33
|polymorphic_expression_call|12.127|12.214|-0.72|10.296|10.357|-0.59
|polymorphic_float_expression_call|12.515|12.302|1.70|10.671|10.685|-0.13
|indirect_float|10.572|10.585|-0.12|9.077|8.979|1.08
|arithmetic|2.803|2.818|-0.52|1.435|1.435|-0.02

The three targeted call workloads have disjoint before/after fork ranges in
every trial. Observed fork ranges are not confidence intervals. The typed floating
callback-result workload also improves 3.0–4.6%, but neither its normal nor fast
LLVM entry calls the changed helper: that control effect remains unexplained
and is not attributed to direct reference-tag checking.

Unsupported-body fallback improves 1.5–2.0% in the separate-executable trials but
slows 0.88% in the same-AOT control (before 299.168–304.287 ns, after
296.818–303.338 ns). There is no reliable fallback speedup claim. Every adverse
control here has overlapping fork ranges, including small indirect-float,
polymorphic and arithmetic slowdowns; all samples and costs are retained.
Identical LLVM or shared executable bytes do not prove identical library layout
effects or eliminate environmental variation.

### Qualification and next work

Release passes 90 selected files, 372 cases / 16254 assertions, with one known
release-only skip; debug passes 90 files, 379 cases / 16353 assertions, no skips.
Each includes 12 standalone files. The new driver has 196 assertions per build
and checks actual helper emission, O0/O3 stripped AOT, EH/no-debug/parallel codegen,
runtime disable controls, scalar/enum representations, raw references, weak/opaque
values and destruction counts. Four supporting qmod targets and dependencies were
rebuilt sequentially for each build.

All 495 reference checks per mode pass: release IR/AOT/Java-int/Java-JIT and
debug IR/AOT. The harness passes 15 tests; focused native debug checks pass 146/146.
No full-suite, sanitizer, cross-platform or IR-speedup claim is made.

A separate ten-second 1 ms profile after all timing records 7613 top-level inclusive
samples. The top-of-stack summary includes 3774 native floating helper samples,
464 reference-dereference helper samples plus 391 at its dynamic-linker stub,
364 popped-frame publication samples and 301 stack-check samples. Nested inclusive
branches are not additive, and tail attribution is incomplete; these are qualitative
priorities, not exact guard timings.

Next experiments:

1. Remove redundant native-admission work using parse/compile-time facts and
   existing live metadata checks; preserve Program/access, debugger and body readiness.
2. Reduce explicit-context/frame and stack-guard costs without losing concurrent
   frame-reader visibility or receiver lifetime.
3. Investigate repeated unsuccessful native admission separately: the fallback
   still takes about 98.5 times Java's whole-workload time in the main trial.
4. Profile prepared same-Program IR and context-using/cross-Program calls separately.
   Dynamic native call workloads here still take 3.56–3.77 times Java's time,
   and boxed floating calls 24.1 times. Faster loads alone cannot close those gaps.

The [audit](FOLLOWUP-AUDIT.md#reference-tag-load-audit-2026-10-10) records every
check: 15 Pass, 47 N/A, 0 Fail.

## Native Program-proof factoring (2026-10-10)

Decision: retain the narrowly scoped immutable-proof reuse. Floating dynamic-call
medians improve 1.9–2.3% across four trials, but their fork ranges overlap in each.
Dynamic integer calls improve 5.58% and 3.93% in the two same-executable trials,
with disjoint ranges, while the separately compiled main trial is 0.46% slower.
This is a modest conditional gain, not a universal speedup or Java parity.

[Complete evidence](results-native-program-proof-macos-arm64.json) records
360 randomized jobs / 1800 windows: 90 fresh baseline, 90 main, 60 shorter-loop,
60 same-AOT and 60 five-fork confirmation jobs. No task build, test, profile,
inspection or evidence work overlapped timing.

### Proof and qualification

Source parent ed2ef50a5; origin/develop 1800de49a was already merged.
Frozen baseline binaries match that committed source and retain their original
pre-commit provenance. The candidate is the parent plus its saved patch.

Cache preparation already proves one parameter and equal caller, receiver and
execution Programs before publishing a native descriptor. The hot unary path
checks the live receiver Program once and matches the cached non-null Program
witness. That also proves the stored nargs/variant checks. A debug assertion
checks the implications. The sole witness assignment remains after all proofs
and before release-publication; readers acquire the immutable entry.

Receiver validity, hierarchy-sensitive private access, receiver class/type,
argument type/state, parse options, epoch, live AOT body/readiness, debugger and
both stack guards remain. No context pointer is newly cached. Receiver/argument
ownership, fallback, deferred context and concurrent diagnostic-frame publication
are unchanged. Rejected non-native entries can require scanning further in the
bounded four-entry PIC; fallback is measured rather than assumed free.

No compiler, ABI, layout, runtime-identity or benchmark change. All 130 normalized
emitted LLVM bodies, including bootstrap main, match. Both arm64 unary helpers
shrink from 749 to 723 static instructions, retaining their 400-byte stack frames.
Smaller code is supporting evidence, not a performance result.

The expanded integer and floating fixtures alternate foreign-Program and
same-Program receivers at dynamic/nullable call sites. Both pass the frozen
baseline in AST/IR/JIT/tiered and stripped AOT O0/O3. Candidate release/debug pass
90 selected files each: 372/379 cases, 16254/16353 assertions, 12 standalone files,
one known release-only skip and no debug skips. All 495 references per mode pass
(release IR/AOT/Java-int/Java-JIT, debug IR/AOT), as do 15 harness tests and
146 native debug checks. Support qmods were rebuilt sequentially per build.
No full-suite, sanitizer, cross-platform or IR-speedup claim.

### Measurements

Apple M4 Pro, macOS arm64 release; median fork means in ns/iteration, startup
excluded. Three warmups and five 500 ms measurement windows per fork; local
Temurin 25.0.2 JIT retains target calls. Whole workloads have unequal Qore/Java
representation and inlining contracts.

Main count 4096, three forks, run-lhvijh83:

|!Kernel|!Before ns|!After ns|!Reduction %|!Java JIT ns|!After / Java
|dynamic_int_result_call|11.812|11.867|-0.46|3.079|3.85
|dynamic_float_result_call|12.728|12.437|2.29|3.560|3.49
|indirect_int_result|6.948|7.101|-2.19|1.947|3.65
|indirect_float_result|8.991|9.173|-2.02|2.855|3.21
|dynamic_int_result_fallback|311.995|314.073|-0.67|3.005|104.51
|boxed_float_method_call|64.527|64.944|-0.65|2.557|25.40
|polymorphic_expression_call|10.775|10.172|5.60|2.615|3.89
|polymorphic_float_expression_call|11.236|10.727|4.52|2.654|4.04
|indirect_float|9.379|8.959|4.48|1.698|5.28
|arithmetic|1.499|1.504|-0.36|1.058|1.42

Count 1024, run-8or1ocwa, and same-AOT count 4096, run-wntmqm7m, have three forks.
Main/1024 compile separate executables. Same-AOT uses identical executable bytes
with compatible baseline/candidate libraries, verified by SHA-256 and RUID.

|!Kernel|!1024 before|!1024 after|!Reduction %|!Same-AOT before|!Same-AOT after|!Reduction %
|dynamic_int_result_call|13.584|13.458|0.93|12.068|11.395|5.58
|dynamic_float_result_call|14.603|14.308|2.02|12.880|12.635|1.90
|indirect_int_result|8.735|8.546|2.15|7.070|7.034|0.51
|indirect_float_result|10.613|10.617|-0.04|9.106|8.934|1.89
|dynamic_int_result_fallback|317.626|316.693|0.29|316.489|312.566|1.24
|boxed_float_method_call|66.356|66.740|-0.58|64.099|63.939|0.25
|polymorphic_expression_call|12.693|12.408|2.25|10.640|10.587|0.50
|polymorphic_float_expression_call|13.023|12.893|0.99|11.182|11.046|1.21
|indirect_float|10.906|10.880|0.25|9.384|9.362|0.23
|arithmetic|2.961|2.950|0.38|1.493|1.500|-0.46

Five-fork same-AOT confirmation, count 4096, run-s94t3c50:

|!Kernel|!Before ns|!After ns|!Reduction %|!Before fork range|!After fork range
|dynamic_int_result_call|12.003|11.531|3.93|11.924–12.548|11.424–11.827
|dynamic_float_result_call|12.867|12.618|1.93|12.604–13.485|12.558–12.714
|polymorphic_expression_call|10.769|10.695|0.69|10.684–10.809|10.435–10.905
|polymorphic_float_expression_call|11.211|10.902|2.76|11.126–11.420|10.845–11.303
|indirect_float_result|8.931|8.869|0.70|8.803–9.315|8.817–9.122
|arithmetic|1.509|1.504|0.33|1.501–1.523|1.494–1.536

Observed ranges are not confidence intervals. Only the integer dynamic workload
has disjoint ranges in this confirmation. Floating-polymorphic medians improve
in all four trials (0.99–4.52%); main and the first same-AOT trial have disjoint
ranges. Integer-polymorphic gains shrink from 5.60% in main to 0.50–0.69% in the
same-AOT trials, so the largest observed gain is not treated as a stable effect.

The unchanged floating callback-result control slows 2.02% in main with disjoint
ranges, is nearly unchanged at 1024, and improves 1.89%/0.70% in the same-AOT
trials. It does not call the changed method helper; the initial adverse effect
remains unexplained, not attributed to admission. Other adverse controls include
main integer callbacks, unsupported fallback and boxed floating calls, plus small
arithmetic changes. They do not consistently repeat; every raw sample is retained.
The data supports limited retention, not a claim that all call categories improve.

### Next work

A separate ten-second 1 ms profile after all timing records 8322 top-level inclusive
samples. Top-of-stack counts include 4131 native floating helper, 483 receiver
reference-check helper, 379 frame-unpublication and 327 stack-check samples.
Nested inclusive counts are not additive; attribution is qualitative.

Investigate fusing a sole-use untyped receiver load with the native method attempt,
so an admitted strong object avoids a separate reference-check helper call.
Any such lowering needs explicit evaluation-order and ownership proofs: delaying
reference evaluation past an argument effect or exception is not valid. Keep
the existing resolver on every rejected/reference/weak path. Also investigate
invocation-local reuse of immutable dispatch facts without retaining stale AOT
contexts or bypassing live body/debugger/access checks.

Dynamic native workloads in main still take 3.49–3.85 times Java's time;
generic fallback takes about 104.5 times. IR and context-using/cross-Program calls
remain separate large gaps. The [audit](FOLLOWUP-AUDIT.md#native-program-proof-audit-2026-10-10)
records all 62 checks: 13 Pass, 49 N/A, 0 Fail. No Java-parity claim.

## Stable untyped parameter loads (2026-10-10)

Retain invocation-local non-reference classification for compiler-proven stable
untyped parameters. Main dynamic integer/float time falls 8.81%/5.55%; the
five-fork same-runtime compiler control confirms 10.81%/8.62%, with disjoint fork
ranges for both workloads in both trials. These are whole-workload improvements,
not isolated call costs; Java remains 3.65/3.33 times faster in main.

The classifier runs lazily at the first original load. A successful non-reference
classification can be reused, but neither the value nor an object pointer is
cached. Actual references evaluate at every read in original order. Cache
freshness runs first; runtime reloads normalize reference values before replacing
local bits. Compiler writes, references to the local, captures, weak assignment,
AST-visible locals and opaque AST local access are excluded. Single-read,
loop-free functions retain the ordinary helper. The function-wide loop indicator
is conservative: it does not prove that the particular load is on a loop path.

Returning an i8 classification rather than passing the flag address to the
runtime lets LLVM promote the invocation-local flag. Existing ownership, frame
publication, debugger, cancellation, stack and Program admission remain. The
old reference helper has identical normalized arm64 instructions. The new
internal header changes RUID; no incompatible same-executable library swap is
used. The compile-only switch is QORE_DISABLE_AOT_STABLE_UNTYPED_LOAD.

### Qualification and experiments

Baseline is 889f0d148 plus an empty patch, using its previously qualified frozen
binaries with original provenance retained. Final candidate is that parent plus
the frozen implementation patch; a future commit hash is not invented. Release
and debug each pass 91 integration files plus the new exclusion driver (92 files):
374/381 cases, 16470/16569 assertions, 12 standalone files each, one known release
skip and no debug skips. The value-tag driver has 272 assertions, exclusion
driver 134 and native raw-reference driver six (plus its C++ assertions) per
build. Coverage includes O0/O3, EH, stripped/no-debug, split compilation,
fallback switches, captured/written/weak/reference locals and owned reference
results. Support qmods were rebuilt sequentially. All 495 reference checks per
mode pass (release IR/AOT/Java-int/Java-JIT, debug IR/AOT), plus 15 harness tests
and 146 native debug checks. No full-suite, sanitizer or cross-platform claim.

All eight trials are retained: 540 jobs / 2700 measurement windows, including
baseline and superseded screens. Final candidate accounts for 180 jobs / 900
windows. Source and binaries were frozen before timing; no task builds, tests,
profiles or evidence processing overlapped timing. Each raw checksum/window,
summary and binary/source identity was independently revalidated.

The initial v1 experiment passed the flag address to runtime. It improved the
targeted calls but added stack-byte flag reloads; generic fallback slowed 1.68%
with disjoint ranges in its whole-build trial. V2 returns classification and
removes that flag escape; its three-fork screen retained a slow integer fork.
The final profitability gate additionally excludes loop-free read-once functions.
The count-one v1 trial is dominated by roughly 2 microseconds of invocation and
harness work and cannot rule out small isolated first-read regressions. None of
these earlier binaries is relabelled as the final candidate.

A reflection probe's set_local_var_value call reports UNKNOWN-VARIABLE in
IR/JIT/tiered and AOT on both baseline and candidate, including generic-entry and
full-context controls; AST succeeds. An earlier iterator-reference argument probe
also showed an existing AST versus IR/AOT argument-binding difference. Neither
limitation is claimed fixed or used as successful coverage. The native regression
test directly supplies and mutates a raw ReferenceNode to validate reevaluation
and ownership without that binding confound.

### Measurements

Apple M4 Pro, macOS arm64 release; count 4096, three warmup and five 500 ms
measurement windows per fork. Median fork means in ns/iteration; startup excluded.
Local Temurin 25.0.2 JIT retains the target calls. Qore and Java representation and
inlining contracts differ. Observed fork ranges are not confidence intervals.

Main, three forks, run-clbqtfc4:

|!Kernel|!Before ns|!After ns|!Reduction %|!Java JIT ns|!After / Java
|dynamic_int_result_call|11.553|10.535|8.81|2.890|3.65
|dynamic_float_result_call|12.568|11.871|5.55|3.561|3.33
|indirect_int_result|6.990|7.047|-0.81|1.934|3.64
|indirect_float_result|9.239|9.019|2.38|2.853|3.16
|dynamic_int_result_fallback|311.805|320.429|-2.77|3.019|106.16
|boxed_float_method_call|64.546|64.232|0.49|2.609|24.62
|polymorphic_expression_call|10.627|10.525|0.96|2.596|4.05
|polymorphic_float_expression_call|10.703|10.797|-0.88|2.626|4.11
|indirect_float|9.383|9.453|-0.75|1.698|5.57
|arithmetic|1.499|1.506|-0.46|1.004|1.50

Same-runtime enabled/disabled compiler control, five forks, run-c68ft9wh:

|!Kernel|!Disabled ns|!Enabled ns|!Reduction %|!Disabled fork range|!Enabled fork range
|dynamic_int_result_call|11.719|10.453|10.81|11.634–11.860|10.381–10.471
|dynamic_float_result_call|12.846|11.740|8.62|12.651–13.143|11.596–11.987
|dynamic_int_result_fallback|314.369|309.884|1.43|309.920–317.331|306.907–315.545
|boxed_float_method_call|66.340|63.485|4.30|64.781–66.781|63.143–63.816
|indirect_int_result|6.911|7.014|-1.49|6.901–7.066|6.877–7.079
|indirect_float_result|8.950|8.919|0.36|8.872–8.978|8.898–9.179
|arithmetic|1.501|1.498|0.19|1.498–1.540|1.489–1.508

Fallback slows 2.77% in main with disjoint ranges, but improves 1.43% in the
same-runtime control with overlapping ranges. A focused five-fork whole-build
repeat, run-c0n4x0bc, measures 308.864 to 308.986 ns (-0.04%), with overlapping
307.804–311.395 / 305.709–310.244 ranges. The main regression does not reproduce;
its cause remains unresolved, not erased or attributed confidently to noise.

Indirect integer-result calls slow 0.81% in main (disjoint ranges), 1.49% in the
compiler control and 1.96% in the focused repeat (both overlapping). The latter
is 6.912 to 7.047 ns, ranges 6.847–6.999 / 6.891–7.132. This adverse median trend
remains a caveat, not a claimed speedup. Boxed-float improves 4.30% in the compiler
control with disjoint ranges but only 0.49% in main with overlap. Other controls
show mixed small changes; all raw samples remain in the evidence.

### Next optimization plan

Two separate ten-second 1 ms profiles after timing locate remaining costs. The
floating workload records 4432 top-of-stack samples in the native method helper,
421 in frame unpublication and 378 in stack checks; the former repeated reference
check helper is no longer a >=5-sample top-of-stack entry. Fallback spreads work
across generic dispatch, type checks, parameter instantiation, local/frame setup,
TLS, string construction and cleanup. Nested inclusive counts are not additive;
profiling is qualitative and its windows are not benchmark samples.

Next inspect repeated diagnostic method-name construction in generic dispatch
for immutable metadata reuse, with the fallback workload as primary and native
and callback workloads as controls. Validate stack names, exceptions, Program
boundaries and deoptimization before retaining any measured reduction. Then
investigate reusable native dispatch proofs while preserving live body,
debugger/access and diagnostic-frame lifetime checks. Avoid replacing required
runtime observability with an unsafe raw function-pointer call.

Generic fallback remains about 106 times Java in main, boxed floating results
about 25 times, and retained native dynamic calls about 3.3–3.6 times. IR and
cross-Program paths remain separate gaps. The [audit](FOLLOWUP-AUDIT.md#stable-untyped-load-audit-2026-10-10)
records all 62 checks: 18 Pass, 44 N/A, 0 Fail. Full identities, raw samples,
earlier screens and reproduction scripts are in
[the evidence](results-stable-untyped-load-macos-arm64.json). No Java parity claim.

## Generic method-name reuse (2026-10-10)

Retain reuse of an immutable PIC entry's existing diagnostic name in generic
method dispatch. The context-using fallback workload improves 8.65% in the main
comparison and 10.78% with identical executable bytes, with disjoint observed
fork ranges in both. Native-call controls do not establish a speedup; their
adverse separate-executable results are preserved below. This is an incremental
generic-dispatch improvement, not Java parity.

The old path rebuilt declaring-class::method on every no-IR call even though
the matching entry already owned exactly that string. The new path uses that
immutable name while preserving IR display-name precedence and ordinary miss
construction. The sole supplying caller passes the entry's own method and
variant; a debug assertion checks that invariant. execJITWithDeopt continues to
copy into its owned frame, so no borrowed diagnostic-frame lifetime, publication,
reader handshake or deoptimization contract changes. Program/object/access,
parse-option, type, body/readiness, debugger, stack and cancellation checks remain.

No compiler, header, ABI, cache-layout or runtime-identity change. All 129
non-bootstrap normalized LLVM bodies match. Bootstrap differs only in compressed
metadata length, 27934 versus 27935 bytes; no cause for that serialization
difference is inferred. The generic helper has 804/803 static arm64 instructions,
but its stack frame grows from 448 to 464 bytes. Those code-shape observations
are not performance measurements; the measured net effect determines retention.

### Qualification

Baseline is ec0089000 with its previously qualified frozen binaries and original
pre-commit provenance retained. Candidate is that parent plus the frozen patch,
not a prospective commit hash. Sources and binaries were frozen before timing.
Release/debug pass 93 selected files: 375/382 cases, 16558/16657 assertions,
12 standalone files each, one known release skip and no debug skips. Support
qmods were rebuilt sequentially for each build. Both builds pass IR/AOT 495-case
reference grids; release also passes Java-interpreter/JIT grids, and 15 harness
tests and 146 native debug checks pass. No full-suite, sanitizer or cross-platform
claim is made.

The new diagnostic driver passes 88 assertions on the frozen baseline and both
candidate builds. It warms polymorphic caches before exceptions and validates
long inherited/overridden names, namespaces, foreign-Program calls, normal and
exception stacks, and cache/fast-entry/context-elision-disabled paths. AST's bare
method spelling is accepted; any incorrect qualified method name is rejected.
AST/IR/JIT/tiered, stripped AOT O0/O3, EH, no-debug and split compilation are covered.

### Measurements

Apple M4 Pro, macOS arm64 release; count 4096, three warmups and five 500 ms
measurement windows per fork. Values are median fork means in ns/iteration,
startup excluded. Local Temurin 25.0.2 JIT retains target calls. Qore/Java
representation and inlining contracts differ: these are complete workloads, not
isolated call latencies. All 171 jobs / 855 windows are independently revalidated
and retained. No task builds, tests, profiling or evidence work overlapped timing.

Main, three forks, run-vcad4ch2:

|!Kernel|!Before ns|!After ns|!Reduction %|!Java JIT ns|!After / Java
|dynamic_int_result_fallback|313.853|286.703|8.65|3.013|95.15
|dynamic_int_result_call|10.406|10.757|-3.37|3.164|3.40
|dynamic_float_result_call|11.439|12.003|-4.92|3.616|3.32
|boxed_float_method_call|64.225|64.434|-0.33|2.620|24.59
|indirect_int_result|7.097|7.112|-0.21|1.946|3.65
|indirect_float_result|8.962|9.090|-1.43|2.927|3.11
|polymorphic_expression_call|10.856|10.479|3.47|2.636|3.98
|polymorphic_float_expression_call|10.723|10.874|-1.41|2.648|4.11
|arithmetic|1.530|1.529|0.11|1.019|1.50

Identical-executable comparison, five forks, run-t528f7qs:

|!Kernel|!Before ns|!After ns|!Reduction %|!Before fork range|!After fork range
|dynamic_int_result_fallback|318.804|284.452|10.78|314.352–322.361|282.061–290.474
|dynamic_int_result_call|10.575|10.522|0.49|10.454–10.782|10.295–10.597
|dynamic_float_result_call|11.806|11.789|0.14|11.656–11.894|11.643–12.050
|boxed_float_method_call|63.706|63.845|-0.22|62.711–64.761|63.486–65.394
|indirect_int_result|7.094|7.071|0.32|6.986–7.168|6.937–7.217
|indirect_float_result|9.093|9.160|-0.73|8.928–9.155|8.918–10.700
|arithmetic|1.518|1.507|0.73|1.511–1.537|1.500–1.516

Main fallback ranges are 312.247–315.486 before and 282.268–289.300 after. Its
gain repeats in the identical-executable control. Observed ranges are not
confidence intervals. Boxed and callback controls have mixed small changes;
the slow 10.700 ns floating-callback fork is retained, not filtered.

Main native-floating calls slow 4.92% with disjoint ranges, but the five-fork
identical-executable median improves 0.14% with overlap. An additional five-fork
identical-executable trial, run-jydpdnqd, improves 0.75% (11.927 to 11.837 ns,
overlapping 11.670–12.018 / 11.770–12.053 ranges). The main adverse effect is
unresolved, not claimed to be a runtime-helper slowdown or confidently dismissed
as noise. Main native-integer calls also slow 3.37% with overlapping ranges,
versus a 0.49% improvement with overlap in the first same-AOT control.

Floating-polymorphic calls slow 1.41% in main with disjoint ranges. The focused
same-AOT repeat slows 0.79% (10.779 to 10.864 ns), with overlapping
10.667–10.929 / 10.713–10.917 ranges. That adverse median trend remains a caveat.
No general native-call, callback or polymorphic speedup is claimed.

### Remaining costs and next plan

A separate ten-second 1 ms fallback profile after timing still shows type
classification (414 top-of-stack samples), generic dispatch (397/380), TLS
resolution (277), local setup/evaluation and parameter instantiation (131).
Owned diagnostic-frame copies remain, as intended. Profile counts are qualitative,
include a sleeping signal thread, and are not additive inclusive CPU percentages
or benchmark measurements.

Next investigate immutable generic-call eligibility and signature facts for
reuse in the existing dispatch cache, retaining live body/debugger/Program/access
guards. Also map context-using same-Program methods to the
existing compiled fast-entry conventions: generic fallback still pushes arguments
through TLS and instantiates locals despite having compiled bodies. Any broader
adapter must preserve coercion, self/class context, reflection, exceptions and
deoptimization; measure it separately from metadata reuse. The remaining gap
requires reducing this call setup, not merely more diagnostic-string tuning.

Main native dynamic workloads remain about 3.3–3.4 times Java, boxed floating
results about 24.6 times, and context-using fallback about 95.1 times. IR and
cross-Program execution remain separate gaps. See the [audit](FOLLOWUP-AUDIT.md#generic-method-name-audit-2026-10-10)
(all 62 checks: 15 Pass, 47 N/A, 0 Fail) and
[full evidence](results-generic-method-name-macos-arm64.json).

## Generic method-binding proofs (2026-10-10)

Retain immutable generic-method eligibility and exact-scalar binding proofs.
The contextual fallback workload improves 10.27% against the saved baseline
and 11.97% with identical candidate executable/runtime bytes and only the new
proofs disabled for before. Scalar binding alone improves 2.68%. All three
observed fork ranges are disjoint; the earlier screen repeats these gains.
This is an incremental generic-call improvement, not Java parity or a universal
call-speed improvement. Adverse callback and polymorphic controls are retained.

### Implementation and limits

After ordinary dispatch resolves a callable AOT method, its PIC entry records
static fast-call eligibility: user method, no defaults or method type parameters,
not synchronized, and no no-narrow container parameters. Static methods and
copy() remain excluded. Null admission retains the original eligibility path.
The cache does not retain a new body/context pointer or assume current readiness.
Method signatures are resolved before publication; AOT deferred defaults finish
during deserialization. Signature retargeting applies to fallback closures,
not these published method variants. Fields initialize before release-publication
and remain immutable under existing entry ownership and acquire-loads.

The binding proof additionally requires equal supplied/declared arity (at most
eight), canonical hard int/float/bool parameter pointers, and exact equality with
each argument type/state in the full live PIC key. It is all-or-nothing: ordinary
binding gets no new per-parameter branch. Enums, references, coercions, optional,
soft, container and type-parameter values retain ordinary filtering. Canonical
mixed scalar signatures may qualify; scalar/non-scalar mixtures do not.
Boxed full-width integers and escaped floats still acquire refSelf ownership.
Ordinary TLS local instantiation and reverse cleanup remain unchanged.

Live body/readiness, debugger, object validity, private access, Program context,
self/class state, stack/cancellation, return filtering and diagnostic-frame
ownership remain. This amortizes static checks at cache construction; it does
not yet remove generic-call TLS parameter setup or create a new native entry ABI.

Appending internal PIC fields changes RUID from
`1b99ebeb1fe729ae7ba258037db9dd5c2b8aa5c6f003e1a49fc116d7bf0e6118` to
`10e1d35fdd035f0754fa4ded65cfdf83bdc1412063951efe41dc06123e0f691e`.
Baseline and candidate therefore compile separate AOT artifacts. Release launcher
hashes remain unchanged. Same-executable feature controls use only the candidate
runtime, with QORE_DISABLE_AOT_GENERIC_METHOD_ADMISSION_CACHE or
QORE_DISABLE_AOT_SCALAR_PARAM_BINDING_CACHE set only for before processes.

All 129 normalized non-bootstrap LLVM bodies match baseline. Bootstrap differs
only in compressed metadata length, 27935 to 27933 bytes; no cause is inferred.
The generic runtime helper grows from 803 to 1857 arm64 instructions and from
464 to 592 stack bytes. Assembly shows the existing native-method fallback is
newly inlined; scalar-local setup still calls LocalVar::instantiateIntern out
of line. This is a real code-size/stack tradeoff, not a performance measurement.

### Qualification

Baseline is 7ab51e4ad, using its previous qualified snapshot with original
pre-commit provenance retained. Candidate is that parent plus the frozen patch.
Release/debug pass 94 selected files: 376/383 cases and 16678/16777 assertions,
12 standalone files each, one known release skip and no debug skips. Support
qmods and qualifications ran sequentially for each build. Both builds pass
IR/AOT 495-check grids; release also passes Java-interpreter/JIT grids. All 15
harness tests and 146 native debug checks pass. No full-suite, sanitizer or
cross-platform claim is made.

The new 120-assertion driver passes the unchanged baseline and both candidate
builds, covering AST/IR/JIT/tiered and stripped AOT O0/O3, EH, no-debug and split
compilation with proof/cache/fast-entry/context-elision controls. It exercises
scalar boxes, float special values, enum coercion, reference<softint> mutation,
defaults, synchronization, optional values, excess args, no-narrow containers,
generic receivers, mixed arguments, eight/nine-argument boundaries and exceptions.
Existing tests cover cross-Program calls, debugger attachment and frame readers.

### Measurements

Apple M4 Pro, macOS arm64 release, local Temurin 25.0.2 JIT. Count 4096,
three warmups, five 500 ms measurement windows per fork; median fork means in
ns/iteration, startup excluded. Java retains the target calls, but representation
and observability contracts differ: these are whole workloads, not isolated
call latencies. All 223 jobs / 1115 windows (screen plus final trials) are retained
and independently verified. No task builds, tests, profiles or evidence processing
overlapped timing. Observed ranges are not confidence intervals.

Main, three forks, run-mz0wcz6q:

|!Kernel|!Before ns|!After ns|!Reduction %|!Java JIT ns|!After / Java
|dynamic_int_result_fallback|281.886|252.926|10.27|3.018|83.81
|dynamic_int_result_call|10.588|10.620|-0.30|3.143|3.38
|dynamic_float_result_call|11.875|11.775|0.84|3.569|3.30
|boxed_float_method_call|64.157|64.455|-0.47|2.648|24.34
|indirect_int_result|6.951|7.194|-3.50|1.931|3.73
|indirect_float_result|9.097|8.997|1.10|2.902|3.10
|polymorphic_expression_call|10.580|10.678|-0.93|2.602|4.10
|polymorphic_float_expression_call|10.829|10.706|1.14|2.640|4.06
|arithmetic|1.512|1.510|0.12|0.983|1.54

All-proofs-disabled control, identical executable/runtime, five forks, run-o3k725kc:

|!Kernel|!Before ns|!After ns|!Reduction %|!Before fork range|!After fork range
|dynamic_int_result_fallback|285.101|250.989|11.97|282.219–298.524|248.730–255.322
|dynamic_int_result_call|10.535|10.507|0.26|10.436–10.667|10.419–11.053
|dynamic_float_result_call|11.853|12.059|-1.74|11.826–12.226|11.711–12.262
|boxed_float_method_call|64.492|64.509|-0.03|63.805–64.755|64.271–64.946
|indirect_int_result|7.151|7.043|1.51|7.115–7.237|7.005–7.182
|indirect_float_result|9.141|9.034|1.17|8.992–9.405|8.880–9.525
|arithmetic|1.520|1.515|0.34|1.512–1.530|1.499–1.534

Main fallback ranges are 281.581–283.477 before and 250.774–255.469 after.
The five-fork scalar-only-disabled control, run-a7az09x5, is 263.120 to 256.067 ns
(2.68%), with disjoint 262.006–266.839 / 255.459–258.852 ranges. The screen's
baseline, all-proofs and scalar-only comparisons improve 9.66%, 10.06% and 2.67%,
respectively, also with disjoint ranges. No additive decomposition is inferred
across independent trials.

The main integer callback slows 3.50% with disjoint ranges (6.929–7.024 versus
7.145–7.207). Its same-runtime control improves 1.51% with overlap, and the
five-fork baseline repeat, run-drii6uls, slows only 0.16% (7.146 to 7.158 ns;
7.037–7.175 / 7.068–7.222). The main adverse result remains unresolved, not
confidently attributed to noise or to this particular helper.

Integer polymorphic calls slow 0.93% in main with disjoint ranges
(10.242–10.607 / 10.667–10.710). The five-fork repeat slows 1.22%
(10.581 to 10.711 ns), with overlapping 10.482–10.745 / 10.491–10.829 ranges.
The adverse median trend persists and remains a caveat. Native integer and boxed
floating main medians also worsen slightly with overlapping ranges. Native float
improves 0.84% in main with disjoint ranges, but its fixed-executable control
slows 1.74% with overlap. No broad native/callback/polymorphic gain is claimed.

### Remaining costs and next plan

A separate ten-second 1 ms profile after the first final timing batch and before
the adverse-result repeat shows type classification (507 top-of-stack samples),
generic dispatch (486/481), TLS resolution (290), local setup (189), parameter
loads and owned frame work. It also shows nested viaContext -> self.next calls
entering the generic method dispatcher. Counts include a sleeping signal thread,
are qualitative and are not additive inclusive CPU percentages or normalized
before/after measurements.

Next investigate native admission for pre-resolved same-Program method targets
and the remaining nested self-call fallback, then an explicit-self/RuntimeConfig
entry for context-using bodies. Preserve overload/coercion, variant identity,
private class context, return conversion, reflection, exceptions and live-body
guards. Separately measure a code-shape control that keeps the native fallback
out of the generic dispatcher; do not assume outlining improves throughput.

Native dynamic workloads remain about 3.3–3.4 times Java; context-using fallback
is about 83.8 times and boxed floating results about 24.3 times. The remaining
gap requires reducing complete call setup, not merely additional signature
caching. See the [62-item audit](FOLLOWUP-AUDIT.md#generic-method-binding-audit-2026-10-10)
(16 Pass, 46 N/A, 0 Fail) and [full evidence](results-generic-method-binding-macos-arm64.json).

## Resolved AOT method entries (2026-10-10)

Retain a full-key cache route for pre-resolved object method calls, including
nested `self` calls. It preserves the parse-selected method and variant instead
of doing name-based overload resolution. Missing, saturated, incompatible and
ineligible entries retain the previous path. No exported ABI, header layout,
runtime identity or IR-interpreter change is involved.

On this Apple M4 Pro/macOS arm64 host, the final nested-call workload decreases
from **244.50 to 207.50 ns/element (15.13%)**. The identical executable/runtime
control decreases from **245.87 to 207.58 (15.57%)** when the new route is enabled.
Both have disjoint observed fork ranges, and the five-fork screen improves
14.85%. These are complete workload costs, not isolated call latency.

### Final comparison with Java

Three randomized forks per runtime, three warmups and five 500 ms measurement
windows per fork, count 4096; Java is Temurin 25.0.2/JMH. Startup is excluded.
The separate control uses five forks with identical AOT/runtime bytes and only
`QORE_DISABLE_AOT_RESOLVED_METHOD_CACHE=1` on the disabled side.

|!Workload|!Baseline Qore ns/element|!Qore ns/element|!Java ns/element|!Qore/Java
|Nested context-using call|244.50|207.50|3.02|68.79x
|Dynamic integer result|10.17|10.21|3.10|3.29x
|Dynamic float result|11.44|11.43|3.46|3.30x
|Boxed float method result|61.88|62.33|2.56|24.38x
|Integer callback result|6.87|6.90|1.88|3.67x
|Float callback result|8.81|8.79|2.63|3.34x
|Polymorphic integer expression|10.24|10.12|2.54|3.99x
|Polymorphic float expression|10.41|10.31|2.55|4.04x
|Final-class method|1.51|1.51|0.89|1.70x
|Monomorphic virtual method|3.39|3.42|1.66|2.06x
|Arithmetic control|1.43|1.43|0.89|1.60x

Only the nested target has separated ranges in this final main/control batch.
Boxed-float medians remain 0.74% slower in main and 0.32% slower in the control,
both with overlapping ranges. Monomorphic, integer callback and native integer
medians also worsen slightly with overlap. No general control speedup, proven
zero regression, statistical confidence interval or Java parity is claimed.

Supplemental exact-receiver probes improve 17.76% for a context-using scalar
method and 18.80% for a two-argument method, with separated ranges. An eight-type
site exceeding the four-entry cache improves only 0.47%, with overlap. These
probes have fixed-seed checksum validation and are not Java comparisons.

### Revisions, qualification and costs

The original prototype improved the target but regressed boxed-float calls
4.78% in main and 5.03% in a five-fork repeat, both with separated ranges.
Assembly showed that adding another caller caused previously inlined native
dispatch and cache/key helpers to be outlined. Inlining just the wrapper moved
the extra call into scalar dispatch; this intermediate was correctness-tested
but not timed. Inlining scalar dispatch alone still regressed boxed calls 3.66%.
A compact-slot shortcut was also tried, showed inconclusive incremental benefit,
and was removed. Preserving cache/key inlining improved boxed calls 3.03% against
the scalar-inline revision with separated ranges, while retaining the target gain.

The final name-dispatch helper retains the baseline helper-call set and its
608-byte frame (1689 to 1690 instructions). The new resolved helper has 1563
instructions and a 608-byte frame, which remains live through dispatch. The
general wrapper grows from 96 to 112 stack bytes; the existing generic helper
remains at 592 bytes. These stack/code costs are not hidden by the timing gain.
Saturated misses can still allocate/discard an entry, and replaced cache entries
stay owned until context destruction. No cold-call or cache-saturation gain is
established.

Release/debug qualification passes 95 selected files each: 377/384 cases and
16830/16929 assertions, with 12 standalone files each, one known release skip
and no debug skips. The new 152-assertion fixture passes baseline and candidate;
146 native debug checks and 15 harness tests pass. IR/AOT/interpreted-Java/JIT-Java
reference grids pass 495 checks per mode. All 129 normalized non-bootstrap LLVM
bodies are unchanged. Builds, tests, profiling and evidence work did not overlap
benchmark timing. No full-suite, sanitizer or cross-platform claim is made.

The [evidence](results-resolved-method-entry-macos-arm64.json) retains all
**551 jobs / 2755 measurement windows**, including discarded revisions,
supplemental probes, normalized samples, hashes, qualification, reproduction
scripts and the implementation patch. Fork ranges are descriptive, not confidence
intervals. The baseline was built during upstream integration from unchanged
core source; generated revision/build metadata may differ, hence the additional
identical-runtime controls.

### Remaining costs and next plan

A separate ten-second, 1 ms profile of the qualified candidate shows generic
dispatch (539 top-of-stack samples), name dispatch (507), type classification
(480), TLS lookup (279), cache matching (244) and local setup (209), plus ownership,
frame and return-filter work. The sample includes a sleeping signal thread;
counts are qualitative, not additive CPU percentages or a normalized comparison.
Nested exact calls reach the resolved helper and still incur live native
admission through the generic dispatcher; outer context-using setup remains.

Next distinguish that admission work from generic outer-call setup, then
prototype explicit receiver/`RuntimeConfig` arguments for a conservative subset
of context-using same-Program bodies. Keep replaceable body readiness, debugger,
Program, exception, frame, return-filter and ownership semantics live. Measure
cold/miss/saturated sites and the existing native/callback/polymorphic controls,
qualify and audit before another commit. The current 3–4x native-dynamic and
roughly 69x generic nested gaps show why complete call setup remains the priority.
See the [62-item audit](FOLLOWUP-AUDIT.md#resolved-aot-method-entry-audit-2026-10-10)
(15 Pass, 47 N/A, 0 Fail).

## Early native admission and explicit integer method entry (2026-10-11)

After integrating origin/develop through `f9aab72d3` (merge `5b65a7af1`),
debugger inspection found that a cold resolved method can populate its dispatch
cache before its callee AOT context exists. The immutable entry then lacks native
admission even after the companion becomes available. Cache only the immutable
signature/class/same-Program proofs early; continue checking receiver validity,
body identity/readiness, companion availability, context use, debugger and stack
state on every call. Avoid retrying the same prepared admission after a miss.

Generic boxed-result integer calls now also use the existing v3/v2 companions
with explicit `RuntimeConfig` and stack-limit arguments, as float calls already
did. Prepared entries can use the existing deferred method context and compact
frame; v1-only and uncached entries retain eager context. This is a runtime-only
change: no compiler, exported ABI, header layout or runtime identity change.
`QORE_DISABLE_AOT_EARLY_NATIVE_METHOD_ADMISSION=1` and
`QORE_DISABLE_AOT_GENERIC_NATIVE_INT_CONTINUATION=1` control the two changes.

### Final comparison with Java

Three randomized forks per runtime, three warmups and five 500 ms measurement
windows per fork, count 4096; Java is Temurin 25.0.2/JMH. Values below are medians
of fork means for complete workloads, not isolated call latency. Startup is
excluded; observed fork ranges are descriptive, not confidence intervals.

|!Workload|!Baseline Qore ns/element|!Qore ns/element|!Java ns/element|!Qore/Java
|Nested context-using call|212.51|200.16|3.02|66.21x
|Dynamic integer result|10.15|10.20|3.07|3.32x
|Dynamic float result|11.53|11.75|3.48|3.38x
|Boxed float method result|60.96|61.65|2.56|24.08x
|Integer callback result|6.88|6.94|1.89|3.68x
|Float callback result|8.90|8.81|2.78|3.17x
|Polymorphic integer expression|10.19|10.20|2.55|4.00x
|Polymorphic float expression|10.27|10.29|2.55|4.04x
|Final-class method|1.53|1.54|0.89|1.73x
|Monomorphic virtual method|3.44|3.40|1.67|2.03x
|Arithmetic control|1.48|1.47|0.89|1.66x

The nested target improves **5.81%** in this run, **5.98%** in a five-fork
identical-executable/library control with both new flags disabled on the old
side, and **4.56%** in a five-fork same-executable/different-library repeat.
All three have separated observed ranges. The earlier explicit-entry screen and
repeat improve 4.45% and 4.09%. This is a repeatable incremental gain, not Java
parity or a general speedup.

The main float-result control is 1.89% slower with separated ranges. Its focused
repeat is 0.71% faster with overlap, and the identical-binary control is 0.47%
slower with overlap: the slowdown did not repeat. Boxed-float main is 1.14%
slower with overlap; control/repeat medians improve 0.21/0.85%, also overlapping.
Arithmetic repeat is 1.78% slower with overlap. Small adverse integer/callback/
polymorphic medians remain in the evidence; no proven zero-regression claim.

### Development, qualification and costs

The admission-only prototype produced a smaller nested gain, with a 1.85%
boxed slowdown in its control (separated ranges) and 2.01% in its repeat
(overlap). This led to the explicit integer entry refinement. Its first screen
showed 2.32% callback and 1.39% arithmetic slowdowns with separated ranges;
the callback repeat reversed and arithmetic ranges overlapped. All attempts,
including the pre-change run, remain in the evidence.

Initial qualification stopped at an obsolete native-leaf fixture assertion
that required late native admission to remain absent. The fixture now checks
immutable admission independently of live body eligibility, retains its
pointer-immutability check, and exercises the old behavior with the early-admission
flag disabled. Added generic-integer tests assert context restoration, published
overflow diagnostics, null/full stack guards and caller-local ownership.
Release/debug qualification then passed 95 selected files each: 377/384 cases,
16940/17039 assertions, 12 standalone files each, one known release skip and
no debug skips. The resolved-call fixture passes 232 assertions in baseline and
candidate; the strengthened leaf fixture passes 240 in both builds. All 146
native debug checks, 15 harness tests and 495 reference checks in each of Qore
IR/AOT and interpreted/JIT Java pass.

All 129 normalized non-bootstrap LLVM bodies are unchanged. Arm64 generic/name/
resolved helper instruction counts grow from 1846/1690/1563 to 1901/1866/1767.
Generic frame size stays at 592 bytes; name/resolved frames shrink from 608 to
592 bytes. These are prologue sizes, not peak-stack measurements. No cold-call,
saturation, full-suite, sanitizer or cross-platform improvement is claimed.

The [evidence](results-native-method-admission-macos-arm64.json) retains all
**397 jobs / 1985 measurement windows**, raw sample checksums, frozen identities,
qualification hashes, rejected phases, debugger observations, reproduction
scripts and implementation patch. No task builds, tests, profiles or evidence
work overlapped timing. The
[62-item audit](FOLLOWUP-AUDIT.md#early-native-method-admission-audit-2026-10-11)
reports 18 Pass, 44 N/A and 0 Fail.

### Remaining costs and next plan

A separate ten-second, 1 ms profile still shows name dispatch (553 top-of-stack
samples), type classification (487), generic dispatch (337), TLS lookup (323),
discard (304), cache matching (228) and local setup (220). Counts include a
sleeping signal thread and are qualitative, not additive CPU percentages.
The release debugger trace confirms the nested callee now reaches its v3 entry
with nonnull explicit context/stack-limit arguments; its outer context-using
method still needs ordinary self/parameter setup, frame handling and filters.

Next inspect direct scalar-return consumers: determine whether nested exact
int/float calls can use native-result continuations instead of generic boxed
dispatch without changing return/exception semantics. Then design an explicit
receiver/RuntimeConfig entry for a conservative subset of context-using
same-Program methods. Measure against this qualified runtime, retain live
body/debugger/Program/stack checks and ownership/observability contracts, and
qualify/audit each improvement separately.


## AOT exact-immediate scalar returns (2026-10-11)

The normal entry of the context-using nested method still filters its unknown
boxed result against its declared scalar return type. Emit a short value-tag
guard for pointer-exact canonical `int`, `float` and `bool` return contracts.
An already-correct immediate scalar owns no node and needs neither type-path
resolution nor assignment filtering. Keep cleanup initialized to NOTHING and
retain the exception-sink check on both edges. Heap-boxed scalars, references,
mismatches, soft/optional/enum types and other contracts use the existing full
filter; timeout handling is unchanged.

`QORE_DISABLE_AOT_SCALAR_RETURN_GUARD=1` controls emission at compile time.
No exported ABI or header layout changes, but the compiler implementation is
part of AOT runtime identity, which changes from `10e1d35f...` to `02b3335a...`.
Actual before/after runs therefore compile separate executables against matching
frozen runtimes. The compiler-flag control compiles two executables with the
same candidate compiler/library. No identity check is bypassed. IR/JIT lowering
is unchanged by this AOT-only guard.

### Final comparison with Java

Three randomized forks per runtime, three warmups and five 500 ms measurement
windows per fork, count 4096; Java is Temurin 25.0.2/JMH. Values are medians of
fork means for whole workloads, not isolated call latency. Startup is excluded;
observed fork ranges are descriptive, not confidence intervals.

|!Workload|!Baseline Qore ns/element|!Qore ns/element|!Java ns/element|!Qore/Java
|Nested context-using call|199.29|179.11|3.01|59.42x
|Dynamic integer result|10.18|10.04|3.07|3.27x
|Dynamic float result|11.33|11.05|3.47|3.18x
|Boxed float method result|61.45|61.61|2.55|24.16x
|Integer callback result|6.91|6.79|1.89|3.60x
|Float callback result|8.73|8.75|2.69|3.25x
|Polymorphic integer expression|10.13|10.12|2.34|4.32x
|Polymorphic float expression|10.32|10.18|2.55|3.99x
|Final-class method|1.51|1.50|0.89|1.68x
|Monomorphic virtual method|3.41|3.48|1.66|2.10x
|Arithmetic control|1.42|1.42|0.89|1.60x

The nested target improves **10.12%** in the final main run, **8.83%** in the
five-fork same-compiler control, **9.95%** in a five-fork actual-baseline repeat
and **9.82%** in an additional five-fork compiler control. All four have
separated observed ranges. Earlier screen/control/repeat medians improve
9.78/8.48/8.96%; the last has overlapping ranges due to a candidate outlier.
This is a targeted repeated benefit, not Java parity or a general speedup.

Adverse controls remain important. Monomorphic main/repeat medians are
**2.05/2.11% slower**, both with overlapping ranges. Boxed-float main is 0.25%
slower with separated ranges, and its actual-baseline repeat is **2.60% slower**
with separated ranges. The same-runtime compiler control/repeat boxed medians
are 0.25/0.40% slower with overlap; monomorphic compiler-control repeat is 0.40%
faster with overlap. These controls do not reproduce the larger penalties,
but do not erase or fix the adverse actual-baseline observations. Runtime/code
layout and host variation are possible explanations, not established causes.

The initial boxed-float screen was 2.27% adverse with separated ranges; its
first repeat narrowed to 0.93% with overlap. The initial integer-callback
2.02% adverse median (overlap) did not persist. Native-float final main/control/
repeat medians improve 2.52/1.60/3.23%, all with separated ranges; this is a
non-target observation, not evidence that its unchanged body acquired a new
native-call optimization. All small signed changes remain in the evidence.

### Qualification, evidence and limitations

Release/debug each pass 101 selected files: 381/388 reported cases,
27184/27283 assertions, 15 standalone files each, one known release skip and
no debug skips. The new fixture passes 232 assertions per build across O0/O3,
guard-disabled, EH, no-debug-info and split compilation. It covers 48-/64-bit
integers, tag-colliding doubles, signed zero, infinities/NaNs, reference returns,
soft/optional/timeout/enum contracts, conversions, invalid returns, owned locals
and overridden nested calls. All 146 debug native checks, 15 harness tests and
495 reference checks in each of Qore IR/AOT and interpreted/JIT Java pass.

Early fixture development exposed differences on the unchanged baseline:
IR handled return-filter errors outside a same-function catch, and IR/tiered
reported six owned-local destructions where AST expected ten. The final fixture
catches a nested typed-call error and uses AST as the AOT cleanup/filter
reference. Baseline AST/JIT and eight AOT configurations pass. These baseline
IR/tiered observations and failure logs are retained; this AOT change neither
fixes them nor claims full fixture parity across modes.

Only two of 129 non-bootstrap LLVM bodies change: the dispatch wrapper and the
nested context-using method. Bootstrap metadata also changes. Inspected runtime
helper instruction counts and prologue frame sizes remain unchanged; those are
not peak-stack measurements. No full-suite, sanitizer, cold-call/saturation or
cross-platform improvement is claimed. Known build warnings remain.

The [evidence](results-scalar-return-guard-macos-arm64.json) retains all
**319 jobs / 1595 measurement windows**, identities, raw-sample verification,
qualification hashes, profile, implementation/test sources and reproduction
scripts. No task build/test/profile/inspection/evidence work overlapped timing;
unrelated editor processes were left untouched. The
[62-item audit](FOLLOWUP-AUDIT.md#aot-scalar-return-guard-audit-2026-10-11)
reports 15 Pass, 47 N/A and 0 Fail.

### Remaining costs and next plan

A separate ten-second, 1 ms profile shows name dispatch (584 top-of-stack
samples), type classification (449), generic dispatch (391), discard (337),
TLS lookup (308), cache matching (278) and local setup (232). It includes a
sleeping signal thread; counts are qualitative, not additive CPU percentages.
The generated nested entry still loads self/parameters through runtime/TLS
helpers and uses ordinary method/frame/ownership setup.

Next investigate direct native-return continuations and an explicit receiver/
RuntimeConfig entry for eligible context-using same-Program methods. Existing
direct-argument lowering can map a hidden receiver argument, but receiver
ownership, defining-class access, logical frames, selected overload identity,
exceptions, debugger fallback and live body/Program/stack checks must remain.
Do not remove the pure-leaf `uses_self` restriction without a new entry contract.
Keep boxed-float and monomorphic controls prominent in subsequent measurements;
qualify, audit and commit each demonstrated improvement separately.

## Receiver-aware forwarding entry (2026-10-11)

Add a separate AOT entry for the exact `int(int)` instance-method shape
`return self.method(value)`. The compiler proves that the only local reads are
the receiver and parameter, both before the sole nested call; no stale native
copy is reused after an override can observe or change caller locals. Pass a
native integer, borrowed boxed receiver, live `RuntimeConfig`, AOT context and
exception sink. Return an owned boxed value through the normal declared-return
filter. This does not relax the existing pure-leaf `uses_self` restrictions.

Same-Program admission retains live receiver/body/readiness/debugger checks and
the full method-cache key. The normal runtime frame, signature slots, object/
defining-class context, coercion and cleanup remain. Cross-Program calls and
unsupported signatures/bodies take the normal entry. The presence flag
`QORE_DISABLE_AOT_RECEIVER_METHOD_ENTRY` controls compilation and runtime use.
This internal ABI/context-layout change updates runtime identity, so all
comparisons use separately compiled artifacts and their matching frozen runtimes.

### Final comparison with Java

Apple M4 Pro, macOS arm64, Temurin 25.0.2/JMH. Three randomized forks per runtime,
three warmups and five 500 ms windows per fork, count 4096. Values are medians of
fork means for whole workloads, not isolated call latency; startup is excluded.
Observed fork ranges are descriptive, not confidence intervals.

|!Workload|!Baseline Qore ns/element|!Qore ns/element|!Java ns/element|!Qore/Java
|Nested context-using call|179.14|158.32|3.00|52.81x
|Dynamic integer result|10.38|10.14|3.09|3.28x
|Dynamic float result|10.91|11.14|3.46|3.22x
|Boxed float method result|59.77|60.22|2.56|23.56x
|Integer callback result|6.76|6.92|1.88|3.68x
|Float callback result|8.76|8.79|2.86|3.07x
|Polymorphic integer expression|10.22|10.06|2.32|4.33x
|Polymorphic float expression|10.13|10.13|2.56|3.96x
|Final-class method|1.49|1.50|0.89|1.69x
|Monomorphic virtual method|3.37|3.44|1.66|2.08x
|Arithmetic control|1.42|1.42|0.90|1.58x

The nested target improves **11.62%** in the main run, **10.27%** in the
five-fork same-runtime compiler control and **12.10%** in a five-fork
actual-baseline repeat. All three observed fork ranges are separated. Earlier
prototype screen/control gains were 10.12/12.50%, but those binaries had not yet
received the diagnostic/source-map correction and are not the accepted candidate.

This is a targeted improvement with an observed tradeoff, not a general or
zero-regression speedup. Integer callbacks are **2.37% slower** in the main run
and **1.09% slower** in a five-fork actual-baseline repeat, both with separated
ranges. The five-fork compiler control is 1.50% slower with a small range overlap.
The callback is not eligible for the new entry. Layout/host effects are possible
explanations, not established causes; the repeated adverse result is retained.

Monomorphic calls are 2.21% slower in the main run (separated ranges), 1.64%
slower in the compiler control (overlap), and 1.22% faster in the actual-baseline
repeat (overlap). Boxed-float medians are 0.75/0.06% slower in main/control and
0.08% faster in the repeat, all overlapping. Dynamic-float main is 2.08% slower;
its additional baseline/control repeats narrow to 0.37/0.49% slower, all with
overlap. Final-class methods are 0.44% slower in main (barely separated ranges),
then 0.15% faster/0.11% slower in baseline/control repeats (overlap). All signed
changes, including the non-target dynamic-integer 2.40% gain, remain in the data.

### Correctness gate and qualification

Initial broad qualification missed a source-location bug: the companion had its
own native body but no registered PC range. A warmed exception probe reported
line 19 for a nested call whose baseline location was line 8. Before acceptance,
give the hidden native companion its own symbol-keyed PC map, sharing the owner's
location table but never its function-relative offsets. Serialize companion maps
in the existing PC payload, not as additional callable variants/slot records.
Attach under the existing context attachment lock before release publication;
if a debug-enabled map is unavailable, do not publish the fast entry. Bodies
without debug info keep eager locations. Existing outer-frame imprecision in
the baseline probe is not claimed fixed.

The regression now asserts the actual nested call line after warmup, map
serialization/registration, and absence of TLS local loads in the companion.
It also checks reference/wide integer inputs, reflection reading the outer
receiver/parameter slots and changing the parameter, overriding/private context, recursion,
exceptions/recovery, invalid arguments, deleted objects, foreign Programs and
concurrent first publication. All 190 assertions pass in release/debug across
O0/O3, disabled compilation, EH, no-debug and split compilation, with runtime
disable/admission/PIC/context-elision controls.

Each build passes 102 selected files: release/debug report 382/389 cases and
27374/27473 assertions, with 15 standalones each and one known release skip.
All 153 debug native checks, 15 runner checks and 495 references in each of Qore
IR/AOT and Java interpreted/JIT modes pass. The final compiler-only PC-map
serialization also propagates cooperative cancellation rather than leaving a
partial payload. No full-suite, sanitizer or cross-platform validation is claimed.

The [evidence](results-receiver-entry-macos-arm64.json) retains **321 jobs / 1605
measurement windows**: 102 earlier baseline/screen/control jobs and 219 final
qualified jobs, including adverse-control repeats. Frozen identities, source
hashes, fixture contents, all normalized/raw-sample checks and reproduction
scripts distinguish the screened, rejected-diagnostic and accepted versions.
No task build/test/profile/inspection/evidence work overlapped timing; unrelated
editor processes were left untouched. See the [62-item audit](FOLLOWUP-AUDIT.md#receiver-aware-entry-audit-2026-10-11).

### Remaining costs and next plan

The final LLVM dump has 131 definitions (baseline 130). The normal forwarding
entry still has two TLS local loads; its 142-line companion has zero and uses
the explicit RuntimeConfig stack guard. These are IR-body observations, not
claims that unrelated machine-code bodies or peak stack use are unchanged.

A separate ten-second, 1 ms profile confirms the companion executes. Top-of-stack
counts include name dispatch 642, generic dispatch 493, type classification 402,
TLS lookup 357, cache matching 307, local setup 249 and explicit RuntimeConfig
acquisition 164. Return acceptance and its type machinery also remain visible.
The profile includes a sleeping signal thread; counts are qualitative and are
not additive CPU percentages.

Next isolate redundant return validation in the generic dispatch epilogue, with
an exact immediate-value/canonical-type guard and the existing full fallback.
Then investigate a preflighted native forwarding path that can prove the nested
target is an existing scalar leaf before eliding frame/slot work. Arbitrary
overrides must retain observable locals; misses must be effect-free, and selected
overload identity, defining-class context, logical stack traces, debugger,
receiver lifetime and Program/body guards must remain. Keep callback and
monomorphic controls in subsequent measurements. Java parity remains unachieved.


## Upstream integration 8417bde52 (2026-10-11)

Integrated SQL default/type fixes and provider capability metadata after receiver-entry
commit dca00b43c; no core runtime change. Added a 1432-assertion offline SQL regression,
a linear SQL Server normalization scan and correct database constraint-name quoting.
Release/debug each pass 13 selected files and the IR/AOT 495-check reference grids;
153 debug native checks pass. Live database cases were skipped.

This is integration qualification, not a new performance measurement. The prior
receiver-entry gain and callback regression remain as reported above.
[Evidence and all 62 audit items](results-merge-8417bde-macos-arm64.json).

Next: measure a narrow exact-scalar return guard in the generic method wrapper,
retaining full optional/soft/generic/reference/heap/missing-return handling and
measuring callback, boxed-float and monomorphic controls before any optimization commit.


## Exact scalar returns in the method wrapper (2026-10-11)

Baseline is merge c5c7b80d2. The runtime now accepts immediate int/float/bool values
directly when the method has that exact canonical declared return type. Optional, soft,
generic, enum, reference, heap-backed and missing-return cases retain the full filter.
Existing argument cleanup, frames, Program/object context and diagnostics are unchanged.

Frozen before/after libraries run one identical AOT artifact. Main: three forks;
identical-binary guard-off/on control: five forks. Each uses three warmups and five
500 ms measurement windows at count 4096, with shuffled scheduling. No task build,
test, profile or inspection runs during timing. Values are median fork means in
ns/element; positive reduction means faster.

|!Workload|!Before|!After|!Time reduction|!Java JIT|!After / Java
|dynamic_int_result_fallback|158.330124|148.910590|5.949%|3.006616|49.528x
|dynamic_int_result_call|10.094968|10.089744|0.052%|3.081880|3.274x
|dynamic_float_result_call|10.915636|11.073500|-1.446%|3.463900|3.197x
|boxed_float_method_call|60.437286|60.086341|0.581%|2.554555|23.521x
|indirect_int_result|6.904788|6.925970|-0.307%|1.879951|3.684x
|indirect_float_result|8.795923|8.825958|-0.341%|2.684673|3.288x
|polymorphic_expression_call|10.034748|10.228484|-1.931%|2.519411|4.060x
|polymorphic_float_expression_call|10.182675|10.285589|-1.011%|2.546730|4.039x
|method_call|1.494582|1.482087|0.836%|0.889005|1.667x
|monomorphic_call|3.327583|3.426869|-2.984%|1.661858|2.062x
|arithmetic|1.406730|1.406383|0.025%|0.891363|1.578x

Identical-binary runtime control (disabled / enabled):

|!Workload|!Disabled|!Enabled|!Time reduction
|dynamic_int_result_fallback|159.087079|151.806574|4.576%
|indirect_int_result|6.987542|7.007672|-0.288%
|monomorphic_call|3.433814|3.397680|1.052%

Five-fork confirmation after the main adverse observations (all original results retained):

|!Comparison|!Workload|!Before / disabled|!After / enabled|!Time reduction
|repeat|dynamic_int_result_fallback|160.508108|152.236597|5.153%
|repeat|monomorphic_call|3.507570|3.444914|1.786%
|repeat|polymorphic_expression_call|10.235975|10.224154|0.115%
|control|dynamic_int_result_fallback|160.263304|151.114759|5.708%
|control|monomorphic_call|3.444054|3.444964|-0.026%
|control|polymorphic_expression_call|10.306734|10.315303|-0.083%

IR interpreter comparison:

|!Workload|!Before|!After|!Time reduction
|dynamic_int_result_fallback|1321.827818|1319.564898|0.171%
|boxed_float_method_call|750.618655|746.143786|0.596%

Nested AOT improves 5.949% in main, 5.153% in baseline confirmation and 5.708% in
same-binary confirmation, with separated observed ranges. The first five-fork
same-binary control improves 4.576% but has overlapping ranges due to one enabled
fork at 161.714 ns. The initial screen found 5.011% and 6.005% gains.

Main monomorphic/polymorphic-int calls are 2.984%/1.931% slower with separated
ranges; this did not repeat: baseline confirmation is 1.786%/0.115% faster,
and same-binary confirmation is 0.026%/0.083% slower, all with overlapping ranges.
Other adverse medians remain in the table/evidence. IR changes of 0.171% and
0.596% overlap variation and are not claimed as demonstrated IR gains. No general
or zero-regression claim is made. All 285 jobs / 1425 measurement windows are retained.

Qualification: 103 selected files per release/debug build; 86 assertions in the new
fixture; 153 debug native checks; 15 harness tests; 495 references per execution mode.
The baseline fixture passes as well. Initial temporary-list lifetime differences in
baseline IR are documented; the final fixture explicitly owns/deletes its invalid object
and separately checks callee-local cleanup. No temporary-lifetime fix is claimed.

A separate 10-second/1 ms profile still shows method dispatch, TLS lookup,
local-slot setup and execution/frame context costs. It is qualitative evidence,
not a timing run or additive CPU-percentage breakdown; the sleeping signal thread
is included in the raw profile.

This does not reach Java parity. Next is a preflighted forwarding path that can prove
its inner target is a same-Program scalar leaf before omitting outer TLS/slot setup,
while preserving logical frames, access context and effect-free fallback.
[Full evidence and all 62 audit items](results-method-return-guard-macos-arm64.json).


## Preflighted same-Program forwarding leaves (2026-10-11)

Baseline c59c38d3f. The compiler identifies a complete int(int) self-forwarding
body and publishes its call slot/location. A warmed, exact same-Program inner scalar
leaf permits bypassing outer TLS signature slots and Program/object substitution.
Both logical frames and exact inner call site remain; stack overflow materializes
contexts in caller order. Missing proofs, reflective overrides and cross-Program
calls retain ordinary execution. Native scalar companions without PC maps are
classified separately during exception location recovery.

Apple M4 Pro/macOS arm64; three-fork main and five-fork same-binary control/repeat;
three warmups, five 500 ms windows, count 4096, shuffled jobs. Baseline/candidate
AOT artifacts are compiled separately because runtime identities differ. Flag
controls reuse one identical candidate artifact/library. No task builds, tests,
profiles or inspection overlap timing. Medians of fork means, ns/element; positive
time reduction means faster. Not isolated call latency.

|!Workload|!Before|!After|!Time reduction|!Java JIT|!After / Java
|dynamic_int_result_fallback|158.059891|60.836028|61.511%|2.959057|20.559x
|dynamic_int_result_call|10.560404|10.649496|-0.844%|3.208738|3.319x
|dynamic_float_result_call|11.647297|11.534144|0.971%|3.590289|3.213x
|boxed_float_method_call|62.715783|62.661235|0.087%|2.652631|23.622x
|indirect_int_result|7.141286|7.246446|-1.473%|1.966966|3.684x
|indirect_float_result|9.080841|9.152449|-0.789%|2.941780|3.111x
|polymorphic_expression_call|10.611150|10.461332|1.412%|2.637804|3.966x
|polymorphic_float_expression_call|10.729427|10.816367|-0.810%|2.691301|4.019x
|method_call|1.581939|1.581328|0.039%|1.023843|1.545x
|monomorphic_call|3.625226|3.546099|2.183%|1.727738|2.052x
|arithmetic|1.521954|1.523436|-0.097%|1.017964|1.497x

Five-fork controls and repeat:

|!Comparison|!Workload|!Before / disabled|!After / enabled|!Time reduction
|same binary|dynamic_int_result_fallback|159.021313|60.446351|61.989%
|same binary|indirect_int_result|7.194435|7.159889|0.480%
|same binary|monomorphic_call|3.543003|3.548546|-0.156%
|separate binary repeat|dynamic_int_result_fallback|156.952946|60.396508|61.519%
|separate binary repeat|indirect_int_result|7.114794|7.311431|-2.764%

IR interpreter comparison:

|!Workload|!Before|!After|!Time reduction
|dynamic_int_result_fallback|1377.209191|1384.520091|-0.531%
|boxed_float_method_call|787.433830|781.616778|0.739%

Nested AOT improves 61.511% in main, 61.989% in the final same-binary control
and 61.519% in the separate-binary repeat, all with separated observed ranges.
Screening gains are 61.285%/62.140%; the AOT comparison accompanying IR gains
61.571%. All 218 jobs / 1090 measurement windows are retained, including outliers.

Integer callback medians are 1.172% slower in the separate-binary screen (separated
ranges), 1.473% slower in main and 2.764% slower in the repeat (overlapping ranges).
Same-binary controls instead show 0.611%/0.480% improvements. This does not establish
a flag-path regression, but neither does it rule out a build-layout effect; the cause
remains unresolved. No zero-regression claim. Other adverse medians remain above.
IR changes overlap variation; no demonstrated IR benefit. Java parity is not reached.

Qualification: 103 selected files per release/debug build, plus seven extra stack/
exception fixtures per build; 153 debug native checks; 15 harness tests; 495 references
per execution mode. Tests cover private/derived/reflective targets, overloads, deleted
objects, references/wide values, cross-Program fallback, concurrent publication/readers,
forced stack overflow and restoration with both eager/deferred controls. Reader
fixture refinements after screening did not change the timed release binaries.

A separate 10-second/1 ms sample shows remaining failed native-try, generic dispatch,
value handling and RuntimeConfig/TLS lookup costs before preflight. Counts are
qualitative, not additive CPU percentages; the sleeping signal thread remains in raw
data. Next: enter this proven forwarding path directly from the typed native helper,
reusing RuntimeConfig and native arguments/results while preserving all logical frames
and effect-free fallback. Fetch/merge upstream after this audited commit first.
[Full evidence, reproduction and all 62 audit items](results-forward-leaf-preflight-macos-arm64.json).


## Native forwarding continuation and borrowed configuration (2026-10-11)

Baseline is merged commit 958c12ae9. The typed integer method helper now enters
the independently proven forwarding path directly, passing the existing RuntimeConfig
and native int64_t argument/result. This avoids a failed native attempt followed by
generic dispatch, argument/result boxing and another TLS capture. The boxed adapter
shares execution but keeps its immediate-input and cleanup contract. Both retain the
full inner dispatch proof, two logical frames and cold exception materialization.

The earlier preflight copied RuntimeConfig, whose copy constructor allocates Impl.
That copy is now a reference. The earlier audit incorrectly described the path as
allocation-free; its correction is retained. Wider boxed results may still allocate.

Three-fork main, five-fork same-binary routing control; three warmups and five
500 ms windows, count 4096, shuffled scheduling. The ABI/runtime identity is unchanged,
so paired before/after uses one identical AOT artifact with separately frozen libraries.
The flag control uses the same artifact AND library; it isolates native routing only,
not removal of the old allocating copy. Builds/tests/profiles/inspection do not overlap
timing. Values are medians of fork means, ns/element; positive reduction means faster.

|!Workload|!Before|!After|!Time reduction|!Java JIT|!After / Java
|dynamic_int_result_fallback|59.027843|23.269027|60.580%|3.013477|7.722x
|dynamic_int_result_call|10.047448|10.487848|-4.383%|3.100398|3.383x
|dynamic_float_result_call|11.032037|11.221815|-1.720%|3.476903|3.228x
|boxed_float_method_call|60.749087|60.398807|0.577%|2.566619|23.532x
|indirect_int_result|6.945575|6.954222|-0.124%|1.892215|3.675x
|indirect_float_result|8.794219|8.805536|-0.129%|2.749152|3.203x
|polymorphic_expression_call|10.169334|10.201679|-0.318%|2.369888|4.305x
|polymorphic_float_expression_call|10.513554|10.180045|3.172%|2.555440|3.984x
|method_call|1.518751|1.521853|-0.204%|0.903918|1.684x
|monomorphic_call|3.446278|3.446731|-0.013%|1.668025|2.066x
|arithmetic|1.450979|1.451730|-0.052%|0.890629|1.630x

Native-routing-only control:

|!Workload|!Disabled|!Enabled|!Time reduction
|dynamic_int_result_fallback|47.647467|23.230635|51.245%
|indirect_int_result|6.958255|6.961626|-0.048%
|monomorphic_call|3.419567|3.406445|0.384%

IR interpreter comparison:

|!Workload|!Before|!After|!Time reduction
|dynamic_int_result_fallback|1326.711943|1345.847776|-1.442%
|boxed_float_method_call|752.180156|755.959575|-0.502%

Five-fork adverse-result repeat (new shuffle seed 826037):

|!Workload|!Before|!After|!Time reduction
|dynamic_int_result_fallback|59.284275|23.133280|60.979%
|dynamic_int_result_call|10.196505|10.328876|-1.298%
|dynamic_float_result_call|11.000989|11.121670|-1.097%
|indirect_int_result|7.006467|7.001884|0.065%

The non-forwarding dynamic integer median regressed 4.383% in the main batch
and 1.298% in the independent five-fork repeat; floating results regressed
1.720% and 1.097%. Observed fork ranges overlap in both batches, but the
repeated adverse direction is retained as a possible small regression, not
dismissed as noise. The forwarding gain repeats at 60.979%, while the integer
callback control is effectively unchanged. This is accepted as a targeted
forwarding improvement with those explicit limitations, not an all-call win.

The main forwarding improvement is 60.580% and the native-routing-only
control is 51.245%, both with separated observed fork ranges. Screening
found 60.419%/51.305% reductions. All adverse medians and outliers remain in the
evidence: 247 jobs / 1235 measurement windows. These are whole workloads,
not isolated call latency or proof of Java parity. Any IR deltas require separate
interpretation; the new route admits only AOT-proven forwarding leaves.

Qualification covers 103 files per release/debug, seven additional stack/exception
fixtures per build, 153 debug native checks, 15 harness tests and 495 references per
mode. The direct native probes test full-width integers, strong/opaque/deleted/weak
receiver behavior, body/map readiness, dispatch epoch invalidation and independent
RuntimeConfig snapshot restoration through forced exceptions. Existing private,
reflective, cross-Program and concurrent reader/publication tests remain.

A separate 10-second/1 ms profile (concurrent debug qualification, no timing jobs)
shows remaining forwarding validation, class-access normalization and frame traffic.
Next evaluate moving immutable checks to binding/initial admission, then examine boxed
floating calls and IR invocation separately.
[Full evidence, reproduction and all 62 audit items](results-native-forward-entry-macos-arm64.json).


## Cached forwarding class context (2026-10-11)

Baseline: merged 70a4c09eb. Normalize the defining-class context once at native PIC admission,
then reuse it only under the exact receiver/type/Program/options/epoch key. Keep the original
class pointer for cold exception frames. C++ base-addition APIs advance the dispatch epoch,
and forwarding rejects mismatched outer/inner epochs. Live body/readiness/debugger checks remain.

Private PIC layout/runtime identity changed: paired runs compile separate matching AOT artifacts.
The same-binary flag control disables normalized-context reuse only. Three-fork main and
five-fork control use count 4096, three warmups and five 500 ms windows, shuffled scheduling.
No task build/test/profile/source inspection overlapped timing. Values are medians of fork
means in ns/element, including loop/checksum work, not isolated call latencies.

|!Workload|!Before|!After|!Time reduction|!Java JIT|!After / Java
|dynamic_int_result_fallback|23.335801|21.654001|7.207%|3.035308|7.134x
|dynamic_int_result_call|10.201750|10.337302|-1.329%|3.100043|3.335x
|dynamic_float_result_call|11.179789|11.322403|-1.276%|3.471207|3.262x
|boxed_float_method_call|61.227140|61.751876|-0.857%|2.575524|23.976x
|indirect_int_result|6.845741|6.911226|-0.957%|1.889761|3.657x
|indirect_float_result|8.861056|10.321888|-16.486%|2.618716|3.942x
|polymorphic_expression_call|10.302162|10.180704|1.179%|2.521393|4.038x
|polymorphic_float_expression_call|10.360939|10.408053|-0.455%|2.552761|4.077x
|method_call|1.537542|1.522297|0.992%|0.896541|1.698x
|monomorphic_call|3.446631|3.381552|1.888%|1.676881|2.017x
|arithmetic|1.445832|1.449944|-0.284%|0.891051|1.627x

Same-binary normalization control:

|!Workload|!Before|!After|!Time reduction
|dynamic_int_result_fallback|24.145524|21.670233|10.252%
|indirect_int_result|6.933610|6.883084|0.729%
|monomorphic_call|3.480706|3.481312|-0.017%

IR interpreter comparison:

|!Workload|!Before|!After|!Time reduction
|dynamic_int_result_fallback|1348.559302|1331.198741|1.287%
|boxed_float_method_call|761.579459|746.845085|1.935%

Additional confirmation run-jzwesok2:

|!Workload|!Before|!After|!Time reduction
|dynamic_int_result_fallback|23.123212|21.587696|6.641%
|indirect_float_result|8.776473|8.689313|0.993%
|dynamic_int_result_call|10.311277|10.191582|1.161%
|dynamic_float_result_call|10.795070|11.048298|-2.346%
|boxed_float_method_call|60.950594|60.985928|-0.058%
|indirect_int_result|6.796148|6.888228|-1.355%

The forwarding gain repeats across screening (7.249%), main (7.207%), the AOT comparison accompanying IR (7.061%), and the independent five-fork repeat (6.641%). All four target comparisons have separated observed fork ranges. Same-binary controls improve 10.152% in screening and 10.252% in the final five-fork control. Main forwarding remains 7.134 times Java's whole-workload time; Java parity is not reached.

The main floating-callback median is 16.486% slower (8.861 to 10.322 ns), with all five windows elevated in two of three candidate processes. The five-fork repeat instead improves 0.993% (8.776 to 8.689 ns) with separated ranges; the large slowdown does not reproduce. Its cause is not established, and the original processes and every raw sample remain in the evidence. No samples are excluded.

Non-forwarding dynamic floating calls are 1.276% slower in main and 2.346% slower in the five-fork repeat (screening was 0.720% slower). Integer callbacks are 0.957% and 1.355% slower in main/repeat, while screening and the same-binary control move in the opposite direction. The adverse main/repeat fork ranges overlap, but these are retained as possible small regressions, not dismissed as zero effect. Dynamic integer calls change from 1.329% slower in main to 1.161% faster in the repeat; boxed floating results change from 0.857% slower to 0.058% slower. Accept as a targeted forwarding improvement with these limitations, not a universal call win.

IR medians improve 1.287%/1.935% for forwarding/boxed-float workloads, with separated ranges in this one batch. The optimized route requires an AOT-native forwarding proof, so these observations do not establish a direct IR optimization or repeatable IR gain. Startup is excluded from steady-state values; the stored launch-to-exit samples are not cold-cache latency measurements.

Evidence retains 294 jobs / 1470 windows. Release/debug each
pass 103 selected fixtures plus the expanded native private-forwarding fixture (300 assertions)
and seven extra exception fixtures. 153 native debug checks, 15 harness tests and 495
references per execution mode pass. The stronger fixture changed no timed runtime binary.

Separate forwarding and boxed-float profiles guide the next AOT-first steps:

1. Keep AOT primary. Inspect the boxed floating recurrence's local assignment and generic dispatch: the sample contains substantial LValueHelper, type/coercion, reference handling and TLS work. Prototype an exact scalar local-store or native dispatch specialization only with explicit alias/ownership/context guards, then measure before/after against the frozen qualified baseline.
2. Move the immutable explicit-type-instantiation absence check in forwarding from per-call AST lookup to binding or context preparation, preserving explicit generic instantiations, stripped/non-stripped artifacts, body identity and publication ordering.
3. Investigate compact storage of the normalization proof (original defining class or null) and monitor non-forwarding dynamic-float/callback controls; retain all adverse results. Preserve logical frames, exceptions, debugger behavior, concurrent stack readers and cross-Program fallback.

[Measurements, identities, profiles, reproduction and all 62 audit items](results-forward-access-context-macos-arm64.json).


## AOT scalar-local replacement (2026-10-11)

Baseline: merged 64c3f8e53. Skip generic lvalue setup only when an ordinary `auto` local already holds a scalar and receives an immediate int/float/bool. Preserve the real runtime slot and ordinary fallback for ownership, references, closures, filters and special storage. This changes an AOT assignment helper, not IR interpreter assignment or call dispatch.

Separate matching AOT artifacts are built for the changed private runtime identity. Main comparison: three forks; same-binary on/off control: five forks. Count 4096, three warmups, five 500 ms windows per fork, shuffled scheduling. No task build/test/profile/source inspection overlapped timing. Values are medians of fork means in ns/element, not isolated call latency.

|!Workload|!Before|!After|!Time reduction|!Java JIT|!After / Java
|boxed_float_method_call|59.691851|43.550601|27.041%|2.547558|17.095x
|dynamic_int_result_call|10.197566|10.457506|-2.549%|3.066573|3.410x
|dynamic_float_result_call|11.195807|10.895372|2.683%|3.463178|3.146x
|indirect_int_result|6.919300|6.930636|-0.164%|1.873139|3.700x
|indirect_float_result|8.623076|8.795501|-2.000%|2.875706|3.059x
|dynamic_int_result_fallback|22.020436|21.682982|1.532%|3.003145|7.220x
|list_build|74.546038|74.919184|-0.501%|2.458388|30.475x
|list_mutate|165.114863|166.953009|-1.113%|4.061532|41.106x
|temporary_hash|300.584509|315.173433|-4.854%|15.107940|20.861x
|method_call|1.489866|1.493782|-0.263%|0.888964|1.680x
|monomorphic_call|3.360259|3.316287|1.309%|1.659984|1.998x
|arithmetic|1.382970|1.393444|-0.757%|0.890374|1.565x

Same-binary scalar-store control:

|!Workload|!Disabled|!Enabled|!Time reduction
|boxed_float_method_call|61.668261|44.071551|28.534%
|dynamic_int_result_call|10.355899|10.435058|-0.764%
|list_mutate|167.052501|165.361658|1.012%
|temporary_hash|308.982478|306.611080|0.767%

Additional confirmation run-gpm9pco6:

|!Workload|!Before|!After|!Time reduction
|boxed_float_method_call|59.486281|43.776798|26.409%
|dynamic_int_result_call|10.186627|10.393889|-2.035%
|indirect_float_result|8.701417|8.761663|-0.692%
|list_mutate|164.314545|164.708516|-0.240%
|temporary_hash|312.030449|307.116925|1.575%

The target gain repeats across screening (26.298%), final comparison (27.041%) and the independent five-fork repeat (26.409%), with separated observed fork ranges in all three batches. Same-binary scalar-store controls improve 28.563% and 28.534%. Final AOT time is 43.550601 ns/element versus Java JIT 2.547558 ns/element (17.095 times); this is a whole boxed-recurrence workload, not isolated call latency or Java parity.

Dynamic integer calls regress 1.252% in screening, 2.549% in the final comparison and 2.035% in the five-fork repeat. The final ranges are separated; repeat ranges overlap. The same-binary control is 0.764% slower with overlapping ranges. Retain this as a possible small regression of the changed build, not zero effect or established measurement noise; its cause is not established. Floating callbacks regress 0.164%, 2.000% and 0.692% across screening/main/repeat, with separated ranges in main and overlapping ranges in repeat. List mutation regresses 0.991%, 1.113% and 0.240%, with overlapping ranges, while its same-binary control improves 1.012%. These repeated adverse directions remain limitations of this targeted change.

Temporary hashes regress 4.854% in main with separated observed ranges, but improve 2.579% in screening, 1.575% in the five-fork repeat and 0.767% in the same-binary control. The main slowdown does not repeat and its cause is unknown. No process or measurement window is discarded. Dynamic floating-result calls improve 2.683% in main; screening and same-binary results differ, so no separate causal speedup claim is made. Small method/monomorphic/arithmetic differences are controls, not new calling-convention gains.

The implementation keeps the ordinary interpreter/JIT assignment path unchanged. IR correctness is checked, but this stage makes no new IR/JIT timing claim. The first-use flag isolates the runtime shortcut on the same binary; separate matching AOT artifacts are used for old/new private runtime identities. Startup samples are excluded from steady-state figures and are not cold-cache measurements.

Evidence retains 315 jobs / 1575 windows. Release/debug each pass 104 selected files and seven exception/stack fixtures; 205 native debug checks, 15 harness tests and 495 references per mode pass. The new 77-assertion driver also passes the frozen baseline. No full-suite or sanitizer claim.

Baseline diagnostics found two existing visibility defects: compiled block locals disappear from reflection after their first iteration, and direct constant-specialized AOT calls can expose an empty local map. Dynamic/retained ordinary call visibility passes. Reproducers and failure logs are retained; this store optimization does not repair those defects or claim complete reflection coverage.

Next AOT-first steps:

1. Keep AOT primary. Investigate the repeated approximately 2% dynamic-integer and smaller callback/list regressions with a minimal binary/control comparison; retain them as unresolved until explained or removed.
2. Use the residual boxed-recurrence profile to prototype compile/bind-time admission for immutable ordinary-auto declaration checks and a more direct immediate-scalar store. Keep live slot/ownership/alias checks and write the actual runtime slot; measure matched baselines and fallback-heavy containers.
3. Evaluate a native unary floating-argument path for dynamically named methods whose result must remain boxed, so generic key/type/reference work is avoided under an exact same-Program receiver/options/epoch proof. Preserve checked return semantics, wide/boxed values, logical frames, exceptions and cross-Program fallback.
4. Move immutable explicit-type-instantiation absence checks out of forwarding's per-call AST lookup only after binding/body identity and publication are proved. Resolve the documented baseline local-reflection defects before expanding frame or local-state elision.

[Measurements, identities, baseline diagnostics, reproduction and all 62 audit items](results-aot-scalar-local-store-macos-arm64.json).


## AOT native scalar dispatch key (2026-10-11)

Baseline: merged 60c442fa5. Prepare one immutable integer comparison key from the same-Program witness and actual canonical int/float argument kind when scalar argument state is zero. Test the expected hit first; absent proofs keep the original comparisons. The encoded key is never dereferenced. All live receiver, access, Program/options, epoch, body/readiness, companion and debugger guards remain, as do logical frames and context restoration.

The added private cache word changes the AOT runtime identity, so old/new comparisons use separately compiled matching artifacts. The runtime-only flag control reuses an identical candidate executable and library. Count 4096, three warmups, five 500 ms windows per fork, randomized scheduling; three forks in the Java comparison and five in the independent repeat and flag control. No task build/test/profile/source inspection overlapped timing. Values are medians of fork means in ns/element, not isolated call latency.

|!Workload|!Before|!After|!Time reduction|!Java JIT|!After / Java
|dynamic_int_result_call|10.419742|10.084379|3.219%|3.077980|3.276x
|dynamic_float_result_call|10.986279|10.722428|2.402%|3.453311|3.105x
|dynamic_int_result_fallback|21.655499|20.867683|3.638%|3.009173|6.935x
|boxed_float_method_call|43.893758|43.162071|1.667%|2.548645|16.935x
|indirect_int_result|6.826783|6.969095|-2.085%|1.880515|3.706x
|indirect_float_result|8.712189|8.807298|-1.092%|2.778745|3.170x
|method_call|1.477347|1.472625|0.320%|0.890822|1.653x
|monomorphic_call|3.412274|3.393742|0.543%|1.650808|2.056x

Same-binary compact-key control:

|!Workload|!Disabled|!Enabled|!Time reduction
|dynamic_int_result_call|10.616087|10.162262|4.275%
|dynamic_float_result_call|11.417362|10.964749|3.964%
|dynamic_int_result_fallback|22.242736|21.257711|4.429%
|boxed_float_method_call|44.341582|44.355960|-0.032%

Independent five-fork old/new repeat:

|!Workload|!Before|!After|!Time reduction
|dynamic_int_result_call|10.233206|9.975438|2.519%
|dynamic_float_result_call|10.879677|10.802478|0.710%
|dynamic_int_result_fallback|21.808641|20.957433|3.903%
|boxed_float_method_call|44.570877|43.670599|2.020%
|indirect_int_result|6.903705|6.881290|0.325%
|indirect_float_result|8.762885|8.728638|0.391%

The retained hit-first implementation reduces dynamic integer-method workload time 3.219% in the three-fork comparison and 2.519% in the independent five-fork repeat. Forwarding improves 3.638% and 3.903%, with separated observed fork ranges in both batches. Integer ranges overlap in the retained screen and narrowly in the repeat, so these percentages describe observed medians, not confidence bounds. Floating methods improve 2.402% in the screen but only 0.710% in the repeat with overlapping ranges; no reliable independent floating speedup is claimed. Same-binary compact-key controls reduce int, float and forwarding time 4.275%, 3.964% and 4.429%, with separated ranges. Those controls include the new disabled-proof fallback branch/layout and must not be substituted for net gains over the original runtime.

Integer and floating callback controls are 2.085% and 1.092% slower in the retained screen, but 0.325% and 0.391% faster in the independent repeat, with overlapping ranges. The adverse direction does not repeat; it is retained rather than discarded or called impossible. Boxed floating recurrence improves 1.667% and 2.020% versus the old build, but its same-binary key control is effectively unchanged (0.032% slower). Its gain is not attributed to compact-key lookup; code-layout and OS effects remain possible. Small direct/monomorphic differences are controls, not new call-convention gains.

The superseded presence-first prototype improved integer and forwarding screen medians 1.563% and 1.739%, but floating calls were 0.532% slower. Its same-binary controls improved the three targets 3.870%, 4.634% and 2.986%. Disassembly showed a separate key-presence branch on an ordinary hit. The retained source tests the matching key first, preserves null-Program and fallback behavior, and is independently rebuilt and tested. All earlier raw results and implementation patches remain available.

The Java comparison still has AOT native integer/floating workloads at about 3.28x/3.10x Java, forwarding at 6.93x and boxed recurrence at 16.94x. These are whole workloads in ns/element, not isolated call latency or general parity. The change does not eliminate Program/receiver checks, logical frames, stack observation or ownership semantics, and it does not repair the prior stage's baseline local-reflection defects.

All six unique measurement batches are retained: 302 jobs / 1510 windows, including the superseded presence-first prototype. Release/debug each pass 104 selected files and seven exception/stack fixtures; the native driver has 312 assertions, the debug native suite has 205 checks, the harness has 15 tests, and each reference mode has 495 checks. No full-suite, sanitizer or cross-platform claim.

This is AOT runtime-dispatch work; it makes no new IR/JIT timing claim. It adds one machine word per retained PIC entry, not a per-call allocation. Known baseline local-reflection defects from the preceding stage remain unresolved; this change does not broaden frame/local elision.

Next AOT-first steps:

1. Commit this validated and audited stage, then fetch origin and merge origin/develop; inspect and validate any merged runtime or compiler changes before freezing the next baseline. Do not push.
2. Prototype guarded immediate boxed-int/boxed-float arguments for AOT name-dispatched unary calls, reusing the existing typed native helpers and same-Program/PIC/body/debugger proofs. Guard actual representation without coercion; preserve owned/enum/weak/reference/missing and wrong-signature fallback, checked return behavior, argument cleanup, exceptions and logical frames.
3. Verify actual native hits and compiler selection for the boxed-argument route, including ordinary and invoke/EH edges, source stripping and split compilation. Measure the boxed recurrence plus existing native methods, callbacks and fallback-heavy calls against a fresh matched AOT baseline and a compiler-disabled control before retaining the change.
4. Separately evaluate a compile/bind-time absence proof for forwarding-call explicit type instantiation, covering all context loaders and immutable publication before removing any per-call AST query. Keep live Program/options/epoch/body/receiver checks.
5. If the remaining boxed profile still warrants it, move immutable ordinary-auto declaration eligibility to bind time and use explicit RuntimeConfig for the real local-slot lookup. Preserve live slot, ownership, alias and reflection checks; do not add a private shadow local. Resolve the known baseline visibility defects before broader frame/local elision.

[Measurements, identities, implementation variants, reproduction and all 62 audit items](results-native-scalar-key-macos-arm64.json).


## AOT boxed scalar arguments and matched fallback (2026-10-11)

Baseline: 888fe2144 benchmark controls over merged runtime 9b745b8d8. Name-dispatched unary AOT callers now guard actual immediate integer/float arguments and retain boxed results. Native eligibility still uses the full live PIC/body/context proofs. On a context-using body or missing companion, additive v2 helpers can reuse the validated match to enter ordinary dispatch, preserving signature slots, frames, coercion, ownership and return checks. No negative body cache or broader local/frame elision.

Before/after artifacts are separately compiled against matching private runtime identities. Count 4096, three warmups, five 500 ms windows per fork, randomized scheduling. Three-fork screen; independent five-fork old/new/Java repeat and two five-fork controls. No task build/test/profile/source inspection overlapped timing. Values are medians of fork means in ns/element, not isolated call latency.

|!Workload|!Before|!After|!Time reduction|!Java JIT|!After / Java
|boxed_int_method_call|39.573322|22.697259|42.645%|2.325728|9.759x
|boxed_float_method_call|43.268660|28.065525|35.137%|2.549186|11.010x
|boxed_int_method_fallback|159.539794|147.745067|7.393%|2.526843|58.470x
|boxed_float_method_fallback|170.959711|162.495646|4.951%|2.525462|64.343x
|dynamic_int_result_call|9.912359|9.960105|-0.482%|3.080277|3.234x
|dynamic_float_result_call|10.973665|10.845802|1.165%|3.468125|3.127x
|dynamic_int_result_fallback|21.038140|20.980750|0.273%|3.009341|6.972x
|indirect_int_result|6.940805|6.853529|1.257%|1.880518|3.644x
|indirect_float_result|8.796885|8.796242|0.007%|2.767143|3.179x

Same-compiler boxed-argument-disabled control:

|!Workload|!Disabled|!Enabled|!Time reduction
|boxed_int_method_call|39.342073|22.488617|42.838%
|boxed_float_method_call|42.928447|27.975677|34.832%
|boxed_int_method_fallback|159.027950|146.166003|8.088%
|boxed_float_method_fallback|169.724181|160.967790|5.159%

Same-executable matched-fallback-disabled control:

|!Workload|!Disabled|!Enabled|!Time reduction
|boxed_int_method_call|22.828842|22.823602|0.023%
|boxed_float_method_call|28.231251|28.259994|-0.102%
|boxed_int_method_fallback|163.603192|148.589720|9.177%
|boxed_float_method_fallback|178.652529|163.330380|8.577%

The guarded AOT caller's gains repeat: boxed integer/float leaf workload time falls 43.995%/35.309% in screening and 42.645%/35.137% in the independent five-fork comparison. Natural integer/float field-method workloads improve 7.501%/4.394% in screening and 7.393%/4.951% in the repeat. All four targets have separated observed fork ranges in both batches. Same-compiler disabled-guard controls confirm reductions of 42.838%/34.832% and 8.088%/5.159%. These ranges are descriptive, not confidence intervals.

The identical-executable matched-fallback switch reduces integer and float field-method time 9.177% and 8.577%, with separated ranges. Leaf workloads are effectively unchanged (+0.023% and -0.102%, overlapping ranges), as expected because this switch does not disable native leaves. This isolates saved repeated dispatch work, not the net gain against the original build: the disabled runtime still includes the new compiler guards and helper. The superseded v1 prototype had larger leaf speedups but repeatedly slowed the float field-method workload by 3.3-3.4%. Reusing a fully validated match removes that repeated slowdown and improves both natural fallback workloads. V1 and v2 were not directly paired with each other; their individual before/after data are all retained.

Existing native integer, native float and forwarding controls change -0.482%, +1.165% and +0.273% in the repeat, with overlapping ranges. Integer callbacks improve 1.257% in that batch with narrowly separated ranges but only 0.076% with overlap in the screen; no callback gain is attributed to this method-only optimization. Float callback medians are essentially unchanged (+0.007%); both sides' slow outlying forks remain in the evidence. These controls do not establish zero regression or universal improvement.

Java remains substantially ahead. The repeat puts native integer/float call workloads at 3.234x/3.127x Java, forwarding at 6.972x, boxed leaf recurrences at 9.759x/11.010x, and field-method workloads at 58.470x/64.343x. Those are whole-workload ns/element ratios, including dynamic representation, local storage, loops and checksums, not isolated call-instruction costs or general language-performance ratios. This is AOT-specific caller/runtime work; no new IR/JIT timing gain is claimed.

The exact-candidate profiles show remaining dispatch/stack bookkeeping and scalar local assignment/classification/ref-deref work in the boxed integer recurrence. The field-method profile remains spread across ordinary call setup, TLS/local/frame setup, value ownership and member lookup; the new matched helper is present and no longer repeats the full name dispatch first. Ignore the idle signal-wait thread. Sampling counts are investigative evidence, not causal percentages or isolated latency.

All seven unique batches are retained: 417 jobs / 2085 windows, including the superseded v1 prototype and its repeated float-fallback slowdown. Release/debug each pass 104 selected files plus seven exception/stack fixtures, including 3153 boxed-call assertions, 378 native driver assertions and 1110 float-method assertions. The debug native suite has 205 checks; the harness has 16 tests; all four reference modes pass 540 checks. No full-suite, sanitizer or cross-platform claim.

Initial qualification encountered two obsolete generic-only codegen assertions; corrected tests now require guarded v2 calls and retained fallback. A separate auto-receiver reflection defect reproduces on the frozen baseline and with the new controls disabled: callee replacement can remain stale after an exception. Its failing source and logs are retained, not claimed fixed. The supported nullable typed receiver case is tested with genuinely boxed arguments.

Next AOT-first steps:

1. Commit this measured, qualified and audited stage on feature/performance, then fetch origin and merge origin/develop. Inspect and validate any merged compiler/runtime changes before freezing the next matched baseline; do not push.
2. Prioritize the much slower field-reading AOT methods. Inspect their IR and current receiver-companion eligibility, then prototype a narrowly proven scalar member-read entry passing the receiver and RuntimeConfig explicitly. Do not classify arbitrary self-using methods as pure leaves; preserve member access/locking, deletion, exceptions, coercion, logical frames, debugger/concurrent readers and cross-Program fallback.
3. If eligibility expands, add a genuine non-eligible context/side-effect benchmark control before measuring. Compare integer/float field methods, boxed leaf recurrences, existing native calls and callbacks against fresh matching baseline artifacts, with disabled controls and repeated Java measurements; audit and commit only a verified net benefit.
4. Use the boxed-leaf profile to evaluate compile/bind-time ordinary-auto declaration/member facts and direct RuntimeConfig access to real runtime slots. First repair the saved auto-receiver exception/reload defect before broadening caller-local or frame elision; retain earlier documented reflection limitations until individually reproduced and fixed. Never introduce an unobservable shadow local.

[Measurements, identities, superseded results, profiles, reproductions and all 62 audit items](results-boxed-argument-macos-arm64.json).


## AOT scalar member companions and compile-time member hashes (2026-10-11)

Baseline: committed 155ea99ac, with the same 37-kernel benchmark sources on both sides.
The final candidate adds a whole-body-proven, single-member scalar companion for
homogeneous unary integer/float methods. It passes receiver, defining class and
RuntimeConfig explicitly, reads the current scalar under the ordinary object lock,
and retains ordinary fallback for unsupported representations or bodies. Access,
same-Program, epoch/body, debugger, stack and logical-frame guards remain. Compiled
XXH64/XXH32 name hashes remove runtime hashing, not member-context resolution or
locking. See the design document for the complete eligibility and ownership contract.

Final independent five-fork comparison (`run-9m_epgk2`): count 4096, three warmups,
five 500 ms windows per fork, randomized scheduling (seed 826121). Separately compiled
artifacts use their matching private runtime identities. Values are medians of fork
means in whole-workload ns/element, not isolated call latency.

|!Workload|!Committed baseline|!Final candidate|!Time reduction|!Observed fork ranges
|Integer member method|149.455608|42.831400|71.342%|Separated
|Float member method|162.483946|44.712176|72.482%|Separated
|Boxed integer pure method|22.425995|22.320568|0.470%|Overlap
|Boxed float pure method|27.882218|28.195380|-1.123%|Overlap

The historical `boxed_*_method_fallback` names now exercise native member entries
when eligible. The added `boxed_int_method_effect` deliberately mutates receiver
state, remains ordinary fallback, and consumes every receiver's counter in its
checksum; counters reset on every invocation in both Qore and Java.

The refined member prototype before prehashing reduced member workload time by
69.38%/71.28%, but slowed pure integer/float method workloads by 1.99%/3.05% in that
five-fork comparison. Two layout experiments were measured and rejected: outlining
context work slowed the float member workload 2.73%, and 64-byte method alignment
slowed it 2.42% and the float callback 1.25%, with separated observed fork ranges.
Neither layout change remains in production.

Prehashing versus that refined prototype (`run-imyvmxtw`, five forks) reduces integer
member time 45.480113 to 42.921454 ns/element (5.626%) and float member time 46.773257
to 44.791063 (4.238%), both with separated ranges. The effectful control is 0.106%
slower with overlap; forwarding and callback controls also overlap. Pure-method
improvements in this separate-build comparison are not attributed to hashing:
those methods never call the new member-read helper.

|!Prehash control|!Integer member reduction|!Float member reduction|!Pure float change
|Same executable, runtime disabled versus enabled, three forks|6.996%, separated|8.407%, separated|0.792% slower, separated
|Compiler disabled versus enabled, three forks|6.297%, separated|6.842%, overlap|0.803% slower, separated

The same-library comparison setup prepended a duplicate library directory and added
an environment flag. That difference was identified, not established as the cause
of the small pure-call shifts. A subsequent balanced experiment used one executable,
one library path, two exact ordered-environment A/A modes, and a disabled mode that
changed only the final character of an equal-length environment key. Five forks,
three kernels, randomized blocks/mode order: 45 jobs and 225 windows, all retained.

|!Balanced workload|!Enabled A|!Enabled B|!Prehash disabled|!Observed fork ranges
|Pure float method|28.191080|28.003440|28.003580|All overlap
|Pure integer method|22.138810|22.251780|22.166870|All overlap
|Float member method|44.579100|44.492290|48.147720|Both enabled ranges separated from disabled

The balanced run reproduces the member benefit (about 7.4-7.6%), not a consistent
pure-call penalty. It does not prove the cause of earlier adverse shifts or zero
regression. Ranges are descriptive, not confidence intervals. No task inspection,
build, test or profiling overlapped timing; background desktop activity was observed.
All 168 prehash/comparison jobs (840 windows), balanced jobs, earlier prototypes,
adverse results and raw-output identities are retained in the evidence.

Serial sampling of the refined candidate identified member-context lookup, name
hashing/comparison, ordinary read locking, native dispatch and caller scalar
local/boxing work. These are investigative stack samples, not normalized CPU
percentages or causal latency measurements. Prehashing addresses only one of them.
No fresh Java timing was taken for the final candidate; initial-member Java results
are historical and must not be combined with final-candidate numbers into a matched
ratio. This is AOT-specific work; no IR/JIT speedup or Java-parity claim is made.

Qualification verified unchanged frozen production/test/benchmark sources and the
measured release library. Release: 105 selected files, 388 cases, 29,967 assertions,
15 standalone fixtures and one skip. Debug: 105 files, 395 cases, 30,066 assertions,
15 standalones and no skips. Both also pass seven exception/stack fixtures;
205 debug native checks, 17 benchmark-harness tests and 555 reference checks in
each of IR, AOT, interpreted Java and Java JIT pass. New member suite: 607 assertions;
native context driver: 426. No full-suite, sanitizer or cross-platform claim.

An initial test used the wrong integer type-name spelling; it was corrected, with
the failure retained. A screening command named a nonexistent fixture; the corrected
boxed-call suite passed 3153 assertions. The previously reproduced auto-receiver
reflection/reload defect remains unfixed and is not caused by this optimization.

Next AOT-first steps:

1. Commit this qualified stage after all 62 audit checks; fetch origin and merge
   origin/develop, inspecting and validating any incoming runtime/compiler changes.
   Do not push. Freeze the merged baseline before another experiment.
2. Refresh matched AOT/Java measurements and profiles for member methods, pure boxed
   methods, native calls, callbacks and genuinely effectful fallback. Keep timing
   isolated from builds/tests/profiles and preserve raw samples and identities.
3. Investigate compile/bind-time member-storage context facts under exact class and
   live body guards. Establish class augmentation/lifetime rules before caching
   metadata; do not cache mutable member values or remove the ordinary read lock.
   Retain inheritance, private/internal shadowing, reinjection and deletion tests.
4. Repair the saved baseline reflective auto-receiver exception reload defect before
   broader caller-local/frame/SSA elision. Then target measured local classification,
   boxing/ownership and real runtime-slot access via RuntimeConfig. Preserve debugger,
   reflection, concurrent readers, aliasing and cross-Program behavior.

[Verified measurements and qualification](results-scalar-member-macos-arm64.json).
[All 62 audit checks](FOLLOWUP-AUDIT.md#aot-scalar-member-companion-audit-2026-10-11).


## AOT compile-time internal-member storage context (2026-10-11)

After committing scalar-member companions as e292ebd63, origin/develop was fetched
and merged at ba1be8003 (upstream 669783da1). Incoming changes concerned dependency
source-manifest tooling, not runtime/compiler fast paths. Release/debug builds,
qmods, manifest checks and relevant tests passed before freezing the merged baseline.

The next profile-guided change proves a positive local `private:internal` declaration
at AOT compilation. That declaration always uses the live variant's defining class
as its storage context, including inherited methods on shadowing subclasses. The
new read helper omits repeated context resolution while retaining the ordinary object
read lock, current scalar checks, exact-body/Program guards and fallback. No member
value or metadata pointer is cached, and no class pointer is embedded in the artifact.
Public/private/non-local or unknown declarations retain ordinary context lookup.
The design document records class immutability, copies and loader constraints.

Five-fork before/after (`run-tsz0h7ud`): count 4096, three warmups, five 500 ms
windows, randomized scheduling seed 826137; nine kernels, 90 jobs and 450 windows.
Separately compiled artifacts use their matching private runtime identities. Values
are medians of fork means in whole-workload ns/element, not isolated call latency.

|!Workload|!Merged baseline|!Context proof|!Time reduction|!Observed fork ranges
|Integer member method|43.018133|36.996074|13.999%|Separated
|Float member method|45.098039|40.496524|10.203%|Separated
|Effectful integer method|250.277898|247.621381|1.061%|Overlap
|Boxed pure integer method|22.640571|22.280938|1.588%|Overlap
|Boxed pure float method|28.281977|28.154367|0.451%|Overlap
|Native integer call|10.162447|9.913253|2.452%|Overlap
|Native float call|10.911720|11.094248|-1.673%|Overlap
|Integer callback|6.800607|6.874009|-1.079%|Overlap
|Float callback|8.759948|8.729424|0.348%|Overlap

Only the two member workloads exercise the changed helper. All seven non-target
control ranges overlap, so their median shifts are not attributed to this proof.
No sample or slower fork was discarded; observed ranges are descriptive, not
confidence intervals or evidence of zero regression.

A separate balanced experiment uses one executable, one frozen library, one normalized
library path and exact ordered-environment A/A modes. The disabled mode changes only
the final character of an equal-length environment key to activate
`QORE_DISABLE_AOT_SCALAR_MEMBER_CONTEXT=1`. Kernel/fork blocks and mode order are
randomized (seed 826139), with five forks, four kernels, 60 jobs and 300 windows.

|!Balanced workload|!Enabled A|!Enabled B|!Context proof disabled|!Observed fork ranges
|Integer member method|36.674118|36.476276|42.305069|Both enabled ranges separate from disabled
|Float member method|40.351742|40.380136|45.645151|Both enabled ranges separate from disabled
|Native float call|10.842452|10.861630|10.998530|Overlap
|Integer callback|6.908422|6.853114|6.877203|Overlap

The identical-binary control reproduces about 13-14% integer and 11.5-11.6% float
member-workload reductions. A/A member ranges overlap. The effect is therefore not
dependent on the separate-build layout comparison, although it is not a universal
speedup or a guarantee of no regression. All 150 candidate-comparison jobs and
750 windows are retained with raw-output, source, artifact and environment hashes.
Background desktop activity was observed; no owned inspection, build, test or
profiling work overlapped timing.

Before this prototype, a fresh matched merged-baseline AOT/Java run (`run-ksiselid`,
five forks, seed 826131, nine kernels, 90 jobs and 450 windows) established the
remaining gaps below. This is not a matched Java comparison for the context-proof
candidate; do not combine these Java values with later candidate timings.

|!Merged-baseline workload|!AOT ns/element|!Java JIT ns/element|!AOT/Java
|Integer member method|42.516370|2.526597|16.828x
|Float member method|45.144125|2.517227|17.934x
|Effectful integer method|249.823768|1.862001|134.169x
|Boxed pure integer method|22.452567|2.508591|8.950x
|Boxed pure float method|28.076048|2.544647|11.033x
|Native integer call|10.107698|3.080169|3.282x
|Native float call|11.027260|3.461558|3.186x
|Integer callback|6.847464|1.876405|3.649x
|Float callback|8.724958|2.684331|3.250x

These are retained-call workloads, not isolated call instructions or general language
speed ratios. Java no-inline directives include the effectful target; both languages
reset and consume receiver counters. Exact-baseline serial profiles, with matched
binary UUIDs and validated workload checksums, show remaining dispatch/stack setup,
scalar local assignment and value ownership, read locking and member lookup. The
effectful path additionally spends time in ordinary local/frame/TLS setup and member
updates. Sample counts are qualitative, not causal timing percentages; the idle
signal-wait thread is excluded from interpretation. No Java-parity claim is made.

Release/debug qualification independently verifies the unchanged measured release
library and all frozen production/test/benchmark sources. Release: 105 selected files,
388 cases, 30,363 assertions, 15 standalones and one skip. Debug: 105 files, 395 cases,
30,462 assertions, 15 standalones and no skips. Both additionally pass seven exception/
stack fixtures. The member suite now has 825 assertions; native context has 604.
Also passed: 205 debug native checks, 17 harness tests and 555 reference checks per
IR, AOT, interpreted Java and Java JIT mode. New coverage includes internal float
shadowing, concurrent internal writers, native integer/float hit and miss probes,
special scalar encodings and compile/runtime disable controls. No full-suite,
sanitizer, cross-platform or IR/JIT timing benefit is claimed.

Next AOT-first steps:

1. Commit this measured, qualified and audited stage on feature/performance; fetch
   origin and merge origin/develop, inspect incoming changes and validate as needed.
   Do not push; freeze the next baseline after any relink.
2. Repair the saved pre-existing reflective auto-receiver exception reload defect
   before broader caller-local or frame elision. Check ordinary AOT parameter and
   body-local paths, successful and throwing callees, and ownership on replacement.
   Measure the correction; do not mask it by weakening tests or hiding runtime slots.
3. Target effectful ordinary AOT call setup and boxed caller local/value work using
   explicit RuntimeConfig and compile/bind-time effects proofs where justified.
   Preserve side effects exactly once, stack/debugger/reflection semantics, deletion,
   locking and cross-Program fallback. Re-measure with matched baseline and Java.

[Verified measurements, baseline profiles and qualification](results-scalar-member-context-macos-arm64.json).


## Runtime-backed boxed AOT local freshness (2026-10-11)

AOT remains the primary target; IR/JIT gains are secondary. This stage fixes a
correctness defect required before further call/local-state elision; it is not
a performance improvement. All source accesses being IR-lowered did not make
ordinary AOT boxed parameter/body slots invisible to reflective callees.
Successful and throwing callees could replace runtime slots while the caller
kept stale cached receivers or payloads.

The corrected compiler uses the adjusted private body-local set for reload
exemptions and synchronizes ordinary boxed parameter stores to the same runtime
slots. An initial prototype fixed replacement but lost assigned parameters after
reload, reproduced in both focused controls and FsUtil::make_tmp_file. It was
rejected; the final candidate corrects both storage and reload classification.
The saved baseline fails six reflective groups and passes both assignment
controls; the rejected prototype has the opposite failures. AST passes all eight.

Native scalar allocas, private direct-entry arguments and private IR-only body
storage retain their earlier contracts. This does not fix arbitrary IR/JIT or
private-entry reflection. There is no helper ABI or serialized-format change;
existing AOT artifacts require recompilation.

Three-fork randomized before/after screen, run-grz85ic8: 54 jobs, 270 measured
windows, three warmups, five 500-ms samples/fork, count 4096, seed 826151. Separate
artifacts were compiled against frozen baseline/candidate libraries. Every raw
timing was re-parsed, all summaries recomputed, and source/binary identities
verified. Both builds pass 555 independent reference checks. All samples and
regressions are retained; no owned inspection/build/test/profile overlapped timing.

|!Workload|!Before ns/element|!After ns/element|!Increase|!Fork ranges separate
|boxed_int_method_fallback|36.899892|46.993822|+27.355%|yes
|boxed_float_method_fallback|41.315909|50.923088|+23.253%|yes
|boxed_int_method_effect|249.342138|260.846541|+4.614%|yes
|boxed_int_method_call|21.872929|32.508058|+48.622%|yes
|boxed_float_method_call|28.277143|38.618948|+36.573%|yes
|dynamic_int_result_call|9.921065|9.911814|-0.093%|no
|dynamic_float_result_call|10.869217|11.169973|+2.767%|yes
|indirect_int_result|6.722949|6.890138|+2.487%|yes
|indirect_float_result|8.751906|8.776564|+0.282%|no

The boxed-method workloads add roughly 10 ns/element. Even the pure native
method branch currently joins a conservative caller-local invalidation path.
Two smaller control increases have separated three-fork ranges; they are not
discarded or labeled equivalent. These are whole-workload measurements, not
isolated call latency or statistical equivalence. There is no fresh Java timing
or claim that this stage narrows the Java gap.

Release: 106 selected files, 389 cases, 31513 assertions, 15 standalones, 1 skipped.
Debug: 106 selected files, 396 cases, 31612 assertions, 15 standalones, 0 skipped.
Both additionally pass seven exception/stack fixtures. The new regression has
1,150 assertions including native LLVM-entry/epoch checks, stripped -O0/-O3,
native EH, split codegen, disable controls, special scalar/container values and
replacement destruction counts. Also passed: 205 debug native checks, 17 harness
tests and 555 references per IR/AOT/Java mode. No full-suite, sanitizer or
cross-platform qualification is claimed.

Next AOT-first work:

1. Keep this measured correctness baseline and audit before committing. Do not
   use the previously unchecked local-cache path as the correctness standard.
2. Inspect generated code for calls to the stale-local helper when the cache
   epoch is already fresh. Prototype a compiler-side epoch guard that bypasses
   only the existing no-op helper path; retain all stale reloads and cleanup.
   Compare against this corrected baseline and a compiler-disable control.
3. Before native-edge invalidation elision, prove receiver/argument cleanup,
   overwritten result cleanup and deferred local owners cannot invoke user
   destructors, or preserve invalidation on those paths. Native status 1 and
   same-Program admission alone are insufficient. Preserve arbitrary matched
   fallback and normal/EH mutation visibility.
4. Continue with effectful ordinary-call frame/local/TLS setup using explicit
   RuntimeConfig and compile/bind-time effect proofs. Measure independently;
   only then refresh matched Java comparisons. Do not retry an effectful body
   after a partial update.

[Verified measurements and qualification](results-aot-runtime-local-reload-macos-arm64.json).


## AOT inline local-cache freshness guard (2026-10-11)

AOT is the primary target. The compiler now compares the invocation-local
reload/valid epochs before entering the existing stale-local helper. A fresh
cache bypasses only the helper's no-op path; stale caches retain runtime-slot
evaluation, reference ownership, deferred cleanup and exception behavior.
Reflective class-parameter invalidation runs before this guard. No broader
callee/caller-cleanup effect elision, helper ABI or IR/JIT lowering change.
Existing AOT artifacts require recompilation.

The baseline is the corrected runtime-backed local implementation committed
as adf56d2b7, not the earlier unchecked cache. Its previously frozen binary
was bound to that commit by verifying all relevant source blobs and artifact
hashes; it was not relabeled as a post-commit rebuild. This optimization
recovers a small part of the correctness cost, not the entire cost.

Randomized before/after run-0pprtu4k: 3 forks, 3 warmups, 5 measured 500-ms
windows/fork, count 4096, seed 826157; 54 jobs / 270 measured windows. Baseline
and candidate use separately compiled AOT artifacts and frozen libraries.

|!Workload|!Before ns/element|!After ns/element|!Time reduction|!Fork ranges separate
|boxed_int_method_fallback|46.898586|45.644210|+2.675%|yes
|boxed_float_method_fallback|51.536461|50.257742|+2.481%|yes
|boxed_int_method_effect|259.065366|260.127695|-0.410%|no
|boxed_int_method_call|32.576418|31.586730|+3.038%|yes
|boxed_float_method_call|38.784436|37.390418|+3.594%|yes
|dynamic_int_result_call|10.004055|10.026332|-0.223%|no
|dynamic_float_result_call|11.274021|11.334172|-0.534%|no
|indirect_int_result|6.886676|6.892805|-0.089%|no
|indirect_float_result|8.841571|8.827635|+0.158%|no

Balanced compile-time control aot-inline-reload-balanced-YE9rFD: 5 forks,
3 warmups, 5 measured 500-ms windows/fork, count 4096, seed 826159; 105 jobs /
525 measured windows. Both binaries use the identical frozen compiler and
runtime library. The disabled binary is compiled with
`QORE_DISABLE_AOT_INLINE_RELOAD_GUARD=1`; the enabled compilation uses a
same-length inert key. All timed environments are identical and contain
neither flag. Enabled A/A modes use the exact same executable and arguments.

|!Workload|!Disabled ns/element|!Enabled A ns/element|!Enabled B ns/element|!Time reduction A / B
|boxed_int_method_fallback|46.859678|45.652514|45.593849|+2.576% / +2.701%
|boxed_float_method_fallback|51.496720|50.018627|49.740630|+2.870% / +3.410%
|boxed_int_method_call|32.202249|31.134838|31.187605|+3.315% / +3.151%
|boxed_float_method_call|38.559999|37.600350|37.325951|+2.489% / +3.200%
|boxed_int_method_effect|262.854054|261.705347|259.800671|+0.437% / +1.162%
|dynamic_float_result_call|11.249329|11.244426|11.144807|+0.044% / +0.929%
|indirect_int_result|6.933228|6.868211|6.982783|+0.938% / -0.715%

Both enabled ranges separate from disabled for integer/float member reads
and integer pure calls. Float pure-call medians also improve, but enabled A
contains a retained 53.982490 ns/element outlier fork and overlaps disabled;
enabled B is separated. All A/A ranges overlap. Effectful methods, native
float calls and integer callbacks have overlapping control ranges, so no
speedup or equivalence is claimed for them. Every sample is retained.

All 795 timing windows were independently replayed; summaries, source/binary
identities, randomized job order and 555 reference results per measured mode
were verified. No owned inspection, build, test or profiling overlapped timing.
These are whole-workload medians of fork means, not isolated call latency,
statistical equivalence or a general language comparison. No fresh Java
timing or IR/JIT performance gain is claimed.

The control executable native text grows from 233252 to 234068 bytes (+816 bytes,
+0.350%). This is one benchmark artifact, not a general code-size or compile-time claim.

Release: 106 selected files, 389 cases, 31803 assertions, 15 standalones, 1 skipped.
Debug: 106 selected files, 396 cases, 31902 assertions, 15 standalones, 0 skipped.
Both additionally pass seven exception/stack fixtures. The extended reload
regression has 1,440 assertions: normal LLVM entry and guard presence/absence,
stripped -O0/-O3, return/throw, scalar/container values, ownership counts,
written parameters, native EH, split compilation and fallback controls.
Also passed: 205 debug native checks, 17 harness tests and 555 references per
IR/AOT/Java mode. No full-suite, sanitizer or cross-platform claim.

The up-to-date release module targets did not restore source links changed by
the debug build. All 13 task-built module links were explicitly restored to
their release artifacts and verified by target/content hash; the 1,440-assertion
reload regression passed again with those links in place.

Next AOT-first plan:

1. Preserve this validated corrected baseline. Assess extending the existing
   strong-slot/mutation-epoch cache to read-only auto parameters holding objects.
   Require an existing entry owner, no address escape and exact live-slot
   admission; prove reflective replacement/references and non-object fallback
   behavior before implementation. Measure non-object controls too.
2. Propagate the already-live
   RuntimeConfig through matched ordinary scalar-method fallback; first prove
   same-thread lifetime, Program-data presence and object/class restoration
   across arbitrary reentry. Preserve cross-Program and debugger paths.
3. Measure context lookup/setup independently on the effectful method workload.
   Keep ordinary frames, observable parameter/body slots, return filtering and
   destruction order. Do not borrow a PIC-owned call-name across a callee that
   can replace the cache.
4. Consider broader native field operations only with complete admission before
   effects or a true continuation; never retry after a partial write. Any caller
   reload elision must cover argument/receiver, overwritten result and deferred
   owner destruction, not merely a pure callee.
5. Re-run matched Java comparisons after a larger measured AOT improvement.

[Verified measurements, controls and qualification](results-aot-inline-reload-guard-macos-arm64.json).


## AOT object-valued auto-parameter cache (2026-10-11)

AOT is the primary target. Ordinary read-only `auto` parameters used directly
as method receivers can now reuse the existing strong-object parameter cache.
The compiler requires an existing entry owner and runtime-backed reload
support; the runtime still proves the identical ordinary strong-object slot.
References, weak values and non-objects keep normal reloads. Reflective writes
permanently revoke reuse for the invocation before assignment can reenter.
No new helper ABI, serialized representation or IR/JIT optimization is added.
Recompile AOT artifacts to receive the optimization.

A direct receiver use is a profitability heuristic, not a proof of object
type. The first prototype admitted all otherwise-eligible auto parameters:
its ordinary object loop improved, but rejected float loads regressed
1.91–1.96% in both on/off control legs. That broader heuristic is not retained.
The final compiler excludes scalar/container-only parameters from the new
guard. Dynamic pseudo receivers can still reject at runtime; a string
`toInt()` loop/short-call probe has overlapping on/off and A/A ranges.

Eight of the nine established control kernels have compiler-emitted private
direct-entry paths, whose arguments have a separate ownership/visibility contract
and do not use this cache. The effectful kernel has an ordinary entry with no
cache admission; this control group is not exclusively a private-entry test.
The initial nine-kernel screen showed no gain
and a small separated float-callback slowdown; the final recheck below has
overlapping ranges for every kernel. No private-fast-call gain is claimed.

The new [ordinary-entry companion](ordinary_params.qr) uses `call_function`
to force normal native entries. Loop iterations contain two reads and one
retained callback; short cases enter a function for every iteration. The
object loop contains two field-reading method calls. Timings are whole-
workload ns/iteration, not isolated call latency or a comparison to Java.
Independent checks use `2*n*seed + n*(n-1)/2` for loops and `2*n*seed`
for short cases, across four counts and three seeds (144 checks).

Five-fork confirmation `aot-object-param-narrowed-confirm-OlXzzt`: 3 warmups,
5 measured 500-ms windows/fork, count 4096, order seed 827211; 160 jobs /
800 measured windows. Before is frozen commit 21de4a0c0 source state, bound
to the existing pre-commit-built artifact without claiming a post-commit
rebuild. Before/after AOT artifacts were compiled separately. On/off use
the same frozen compiler/library, a same-length inert/disable compilation
key and identical runtime environments without either key. A/A modes use
the identical enabled executable and command arguments.

|!Workload|!Before ns/iteration|!Disabled ns/iteration|!Enabled A|!Enabled B|!On/off time reduction A / B
|int_loop|41.285705|42.203583|41.762183|42.321396|+1.046% / -0.279%
|float_loop|45.972207|46.538480|46.287681|46.069862|+0.539% / +1.007%
|string_loop|62.260685|61.860063|62.529980|61.473361|-1.083% / +0.625%
|list_loop|57.041646|57.047235|57.011047|57.359508|+0.063% / -0.547%
|hash_loop|73.157941|73.914856|73.813343|73.821903|+0.137% / +0.126%
|object_loop|272.836770|268.233233|230.627336|232.288853|+14.020% / +13.400%
|object_short|1007.361326|999.187528|991.219309|1003.854853|+0.797% / -0.467%
|string_short|721.325342|726.378280|730.531231|727.103213|-0.572% / -0.100%

Object-loop on/off time reduction is 13.4–14.0%; both enabled fork ranges
separate from disabled. Before/after time reduction is 15.47%. Every other
on/off range and every A/A range overlaps. The earlier three-fork narrowed
screen retained noisy list/string controls, motivating this longer run.
Overlapping ranges are not proof of equivalence. No short-call gain is claimed.

Final nine-kernel recheck `run-8b07knyd`: 3 forks, 3 warmups, 5 measured
500-ms windows, count 4096, seed 827223; 54 jobs / 270 measured windows.

|!Existing workload|!Before ns/element|!After ns/element|!Time reduction|!Fork ranges separate
|boxed_int_method_fallback|47.801277|47.813069|-0.025%|no
|boxed_float_method_fallback|53.043110|52.418595|+1.177%|no
|boxed_int_method_effect|274.524106|276.157859|-0.595%|no
|boxed_int_method_call|33.019359|32.845157|+0.528%|no
|boxed_float_method_call|39.689802|39.777147|-0.220%|no
|dynamic_int_result_call|10.440689|10.460683|-0.191%|no
|dynamic_float_result_call|11.845319|11.681226|+1.385%|no
|indirect_int_result|7.127313|7.215299|-1.234%|no
|indirect_float_result|9.141430|9.130610|+0.118%|no

Companion native text: 55588 → 55752 bytes (+164, +0.295%).
This is one artifact, not a general code-size or compilation-time claim.

All 2276 measurement windows across six runs were retained and replayed,
including the rejected prototype and all adverse/noisy controls. Frozen
source/binary identities bind timing to qualification. No owned inspection,
build, test or profiling overlapped timing. Environment values are explicitly
allowlisted; the full environment is recorded only as a hash, not exported values.

Release: 107 selected files, 390 cases, 32909 assertions, 15 standalones, 1 skipped.
Debug: 107 selected files, 397 cases, 33008 assertions, 15 standalones, 0 skipped.
Both additionally pass seven exception/stack fixtures and reference/native
checks. The selected files include the new 1106-assertion cache regression. It verifies ordinary
entry admission and private-entry exclusion, stripped O0/O3, native EH,
split compilation, compiler/runtime controls, references, failed writes,
deletion, destructor replacement, throw, recursion and non-object values.
All 13 task-owned source qmod links were restored and content-verified
against release artifacts, with unrelated links unchanged. Cache and
runtime-local regressions pass again after restoration. Also passed:
205 debug native checks, 17 harness checks and 555 references per IR/AOT/Java
mode. No fresh Java timing, full-suite, sanitizer or cross-platform claim.

Initial fixture attempts exercised pre-existing private-entry caller-frame
visibility limitations. The final ordinary-entry fixture passes AST and
stripped O0/O3 on the frozen baseline too; those limitations are not newly
fixed. A helper assertion initially counted an unrelated typed parameter
cache; the fixture was made unambiguous and rerun. Failed attempts remain
archived with the other evidence.

Next AOT-first work: carry the already-live `RuntimeConfig` through matched
ordinary scalar-method fallback. Prove same-thread lifetime and live Program
data before avoiding redundant TLS lookup/setup; preserve independent object/
class restoration, cross-Program fallback, debugger paths, observable frames,
argument slots, return filtering and cleanup. Measure effectful methods and
native controls before acceptance. Java parity remains an open goal.

[Verified measurements, controls, qualification and archived prototypes](results-aot-object-param-cache-macos-arm64.json).


## AOT self-member updates and scalar context layout (2026-10-12)

AOT is the primary target. This runtime change speeds direct self-member
integer updates and removes duplicate object-context bookkeeping from their
shared lvalue path. Scalar-member dispatch acquires its typed companion once
and specializes deferred/eager context helpers; the non-deferred pure method
invocation is outlined so its storage does not enlarge the usual deferred call.
Access, same-Program, live-body, debugger, stack, frame and ownership guards
remain. No exported helper signature or serialized layout changes.

The update admission is actual inline integer representation, not declared
type. Both old and new values must fit inline storage. It resolves access and
storage once, retains the normal existence pin and write lock, and cannot
call user code or change graph/ownership counts on success. Every miss continues
the same locked ordinary lvalue, including references/conversions, without
retrying lookup or replaying effects. Generic bookkeeping and destructor
ordering remain. Assignment/compound paths also avoid recording context twice.

Compatible existing AOT binaries already calling these helpers can benefit
from the updated runtime. Shared LLVM self-member lowering also uses them in
JIT mode; the IR interpreter is unchanged. Scalar companions are AOT-specific.
No IR/JIT performance gain is inferred from these AOT timings.

Untimed compatibility checks reused both frozen executables without recompilation:
old executable/new runtime and new executable/old runtime each passed all 555
reference checks. Loader tracing confirmed the intended runtime in both directions;
the private runtime-identity headers are byte-identical. This is compatibility
evidence, not a cross-runtime timing claim.

Final frozen candidate: `aot-local-options-candidate-Ecp99t`, identity SHA-256
`933879cccd53c6d949c80de210d23f51c29030d6908beca095acdd2afbc235e6`.
Before is the qualified 6a1eb5c8fe object-parameter-cache baseline, bound to
its existing pre-commit artifact; no post-commit rebuild is implied. The
intermediate comparison is frozen eager-outline candidate uMrtQT.

Final matrix: nine canonical kernels plus four independently checked member
declaration variants; 195 jobs / 975 measured windows. Each run uses three
forks, three warmups and five 500-ms measurements, count 4096, order seed 829601.
Five interleaved modes are before, preceding candidate, enabled-A/B and
integer-update disabled. A/B use identical executable, library, argv and
ordered environment. On/off differs by the last character of equal-length
environment keys and disables only inline integer updating, not context
deduplication or dispatch specialization. All five modes pass 555 reference
checks. Diagnostic switches must be set before startup; the update switch
is sampled on the first representation-eligible operation.

Values are medians of per-fork means in whole-workload ns/element, not isolated
call latency. Positive percentages mean less time than the matched baseline.

|!Workload|!Before|!Enabled A|!Enabled B|!Time reduction A / B
|boxed_int_method_effect|258.899080|227.883955|228.146854|+11.980% / +11.878%
|boxed_int_method_fallback|45.974129|44.361233|44.602004|+3.508% / +2.985%
|boxed_float_method_fallback|50.102350|48.779746|49.370985|+2.640% / +1.460%
|boxed_int_method_call|31.304956|30.428202|29.889474|+2.801% / +4.522%
|boxed_float_method_call|37.634267|37.036168|37.152807|+1.589% / +1.279%
|dynamic_int_result_call|10.011062|10.171123|10.142688|-1.599% / -1.315%
|dynamic_float_result_call|11.210544|11.161244|11.399148|+0.440% / -1.682%
|indirect_int_result|6.942505|6.879014|6.940407|+0.915% / +0.030%
|indirect_float_result|8.834082|8.812167|8.810703|+0.248% / +0.265%
|auto member update variant|262.148678|230.421941|230.423798|+12.103% / +12.102%
|float member update variant|263.699649|250.369135|252.750278|+5.055% / +4.152%
|number member update variant|295.710001|277.060753|279.300211|+6.307% / +5.549%
|reference member update variant|305.954760|296.464856|299.375947|+3.102% / +2.150%

Typed and auto integer-update workloads improve about 12%; their same-binary
update on/off comparisons also separate in both enabled arms. Float and number
update workloads improve 4–6% against baseline while retaining generic arithmetic.
Reference medians improve 2–3%, but enabled-B overlaps baseline and both enabled
arms are 1.8–2.8% slower than the update-disabled control with separated ranges.
That adverse control is retained; its cause is not established.

The tiny dynamic-integer control remains 1.3–1.6% slower than baseline, with
both observed ranges separate. This is an accepted, explicitly tracked tradeoff
for the broader AOT gains, not a universal improvement. Plain integer/float
A/A ranges also separate; small median movements are not independently causal
evidence. Dynamic-float A/B medians straddle baseline. Overlapping observed
ranges do not prove equivalence; these ranges are not confidence intervals.

Earlier experiments and regressions remain archived. The first update attempt
repeated generic lookup and regressed auto/number fallback about 4%; it was
replaced by a same-lvalue continuation. Canonical-int-only admission was then
expanded by representation proof. Duplicate context bookkeeping was removed
after reviewing its lookup callers, with node-conversion/destructor reentry
coverage added. No reproduced crash or unrelated bug fix is claimed.

A packed diagnostic options snapshot and a likely-branch hint were not
retained. A frozen old/new executable/library crossover isolated an earlier
large dynamic-integer slowdown to the runtime artifact: library-only swaps
were 7.7–9.7% slower, while an executable-only swap overlapped. This identifies
the artifact, not a microarchitectural cause. Outlining eager contexts reduced
v1 caller frames to 320 bytes and v2 to 368 bytes. Restoring path-local switches
then recovered most of the dynamic-integer slowdown: the final run improves
6.4–6.6% versus the preceding outline candidate. Its float member-read control
gives back 0.9–2.2% versus that intermediate candidate, while remaining faster
than the qualified baseline. Instruction counts include cold paths; no padding
or lucky-address explanation is asserted.

The four-kernel focused candidate screen adds 300 retained windows. All 1275
current-candidate windows were independently replayed from raw output; all
adverse samples were retained. Earlier checkpoint summaries, identities and
artifact references remain in the evidence record and local raw archives.
No owned inspection/build/test/profile overlapped timing. Published environment
values are explicitly allowlisted; inherited environment values are not exported.

Release: 110 selected files, 395 cases, 33527 assertions, 15 standalones, 1 skipped.
Debug: 110 selected files, 402 cases, 33626 assertions, 15 standalones, 0 skipped.
Both additionally pass seven exception/stack fixtures and native/reference
checks. Also passed: 205 debug native checks, 17 harness tests and 555 references
per IR/AOT/Java mode. All 13 owned source qmod aliases are restored and content-
verified against release artifacts, with unrelated aliases unchanged. Four
targeted regressions pass again after restoration. No full-suite, sanitizer,
cross-platform, fresh Java timing or Java-parity claim.

The new 192-assertion wrapper exercises all four updates in AST/IR/JIT/tiered
and stripped AOT O0/O3, native EH, no-debug-info and split compilation, with
public/private/internal storage and the update switch on/off. Its inner checks
cover 48-bit boundaries, wide values, optional/generic/soft declarations,
enums, floats/numbers, references, deletion, closures, removed members,
exception-after-effect, reentrant node destruction, cold initialization on
separate objects and atomic concurrent updates on shared storage. Existing
receiver/member/boxed-result fixtures add combined diagnostic-control coverage.

Next AOT-first work: isolate the remaining tiny dynamic-call runtime cost with
same-executable cross-runtime controls; evaluate compile-time member metadata
and explicit RuntimeConfig propagation only with complete live access/storage/
lifetime proofs. Preserve statement-level locking, observability, reflection,
effects and cross-Program fallback. Refresh matched Java comparisons after a
further meaningful AOT gain; Java efficiency remains an open goal.

[Verified measurements, qualification and archived experiments](results-aot-call-member-runtime-macos-arm64.json).


## AOT member-update prefix candidate (2026-10-12)

Status: the corrected revision passed selected release/debug qualification and
fresh matched AOT/Java measurement verification. Historical intermediate results
are retained below; the final corrected-artifact comparison follows them. Every
comparison binds its own frozen artifacts, not a mixture of revisions.

AOT now emits a bounded native member-update prefix with an ordinary cold
continuation. A miss resumes after exactly the completed updates instead of
replaying them. The suffix reads its member at the original source position;
each operation retains its own object lock. Explicit receiver, defining class
and RuntimeConfig arguments avoid eager context setup on the non-reentrant
native path. Generic cold work receives ordinary visible argument slots and
a callee frame that remains present during return validation. Compile-time
name hashes remove repeated hashing, not live access/storage checks.

The final measured candidate is `aot-effect-prehash-candidate-6pytrs13`, identity
SHA-256 `97e47e8f6f8a1d0aab7f5090187a66d27b228807cb3eb1b64168390e1766e2d6`.
Before is qualified commit 21b2b072b7, using its frozen pre-commit artifact.
The intermediate candidate includes the effect companion and cold return-frame
fix but not prehashed updates. Each uses its matching frozen compiler/runtime.

The prehash matrix contains 135 jobs / 675 measured windows: nine unchanged
kernels, three forks, three warmups and five 500-ms samples, count 4096, order
seed 831211. Before, intermediate, enabled-A/B and prehash-disabled runs are
interleaved. A/B use the identical executable and ordered environment; on/off
differs by one character of an equal-length diagnostic key. Library aliases
have equal-length paths. Loader tracing and helper-symbol checks confirm the
selected artifacts. All five configurations pass 555 reference checks.

These are medians of per-fork means in whole-workload ns/element, not isolated
call latency. Positive percentages mean less time than the matched baseline.

|!Workload|!Before|!Enabled A|!Enabled B|!Time reduction A / B
|boxed_int_method_effect|245.9551|109.1240|108.6490|+55.633% / +55.826%
|boxed_int_method_fallback|47.3868|47.5930|47.3057|-0.435% / +0.171%
|boxed_float_method_fallback|52.3227|53.1140|53.2946|-1.512% / -1.858%
|boxed_int_method_call|32.4804|32.6200|32.5014|-0.430% / -0.065%
|boxed_float_method_call|39.5552|39.2979|39.5002|+0.650% / +0.139%
|dynamic_int_result_call|10.8211|11.0609|11.1563|-2.216% / -3.097%
|dynamic_float_result_call|12.3739|11.8230|11.9588|+4.452% / +3.355%
|indirect_int_result|7.2735|7.3204|7.3648|-0.644% / -1.255%
|indirect_float_result|9.3851|9.4210|9.3298|-0.383% / +0.589%

The effectful workload is 2.25–2.26 times faster than the qualified baseline.
Prehashing alone improves the intermediate 111.5091 ns result by 2.139–2.565%,
and the same-binary runtime-off result (112.0419 ns) by 2.604–3.028%. Both
comparisons have separated observed fork ranges in both enabled arms; A/A
ranges overlap. This is a targeted gain, not a universal improvement.

The native-integer-result control is 2.2–3.1% slower than baseline, with
separated ranges in both enabled arms. Its prehash-disabled result is also
slower (11.2607 ns), while comparisons against the intermediate candidate
overlap; prehash lookup is not established as the cause. The read-only float
control is 1.5–1.9% slower, with only enabled-B separating baseline. Most other
controls overlap. The preceding run's 1.2–1.3% separated-range pure-integer
regression does not clearly reproduce here, but remains in the evidence.
Observed ranges are not confidence intervals, and overlap is not equivalence.

Read-only disassembly comparisons find unchanged normalized caller/callee
bodies for the inspected boxed/native-result kernels. The integer runtime-v2
dispatcher remains 487 instructions with different branch lowering/layout;
other helper sizes also vary. Normalization masks relocation/address details.
This does not establish complete binary equivalence or a cycle-level cause
for the small control regressions.

### Matched Java comparison for the preceding candidate

The earlier observation-fixed candidate has its own fresh matched Java run:
`aot-effect-member-observation-bench-qbf98y1s`, 162 jobs / 810 measured windows,
nine kernels and six interleaved configurations. It uses Temurin 25.0.2 and
JMH 1.37 with generated no-inline hints for the actual target methods. Each
configuration passes 555 references. The same fork/warmup/sample/count settings
apply, with seed 831107. One noisy native-integer baseline fork is retained.

Do not combine these Java numbers with the newer prehash candidate above.
This table compares only the AOT and Java artifacts timed together in that run.

|!Workload|!AOT enabled A / B ns/element|!Java JIT ns/element|!AOT / Java
|member updates|111.8450 / 111.9033|1.9341|57.8–57.9x
|read-only integer member|48.3162 / 47.6062|2.6837|17.7–18.0x
|read-only float member|51.9708 / 51.7979|2.6495|19.6x
|boxed integer method|32.4803 / 32.4468|2.6584|12.2x
|boxed float method|39.4246 / 39.1483|2.6830|14.6–14.7x
|native integer result|10.9522 / 10.9306|2.9413|3.7x
|native float result|11.7318 / 11.8329|3.6309|3.2–3.3x
|indirect integer result|7.3342 / 7.3072|1.9805|3.7x
|indirect float result|9.4213 / 9.2659|2.8794|3.2–3.3x

Java parity is not reached. Qore's dynamic member storage, per-operation locks,
boxed caller bookkeeping and observable runtime state remain materially
different costs; this workload comparison does not assign a cost to each one.

All 1485 windows from these two complete runs were independently replayed;
all adverse samples are retained. No owned build, test, profile or inspection
overlapped timing. Published environment values use an explicit allowlist;
the full environment is represented only by a hash. An initial Java attempt
stopped on an unrecognized JMH deprecation warning after three jobs and is
excluded as incomplete, not presented as a completed result. The exact warning
was allowlisted and the complete matched run uses the documented Temurin JVM.

The earlier release screen passed 13 cases / 7212 assertions, 17 harness checks
and 555 references each for IR and AOT. New probes cover actual fast/resume
hits, partial-prefix ownership, return-error frames and caller-PIC retirement;
they do not prove arbitrary active-callee replacement or all private-entry
reflection paths. Broader release qualification has since passed 111 selected
files, 398 cases / 34011 assertions, 15 standalone fixtures and one skip. Debug
passed 111 files, 405 cases / 34110 assertions, 15 standalones and no skips.
Both also passed seven exception/stack fixtures and native/reference checks;
205 debug native checks, 17 harness tests and 555 references in each IR/AOT/
Java-interpreted/Java-JIT mode passed. All 13 owned qmod aliases were restored
and verified, with unrelated aliases unchanged and five release smoke fixtures
rerun. The audit then added compiler parameter-scan cancellation polling and a
129-parameter regression, and clarified helper/context contracts. That revised
source subsequently passed the separate qualification and fresh measurements
below. No full-suite, sanitizer or cross-platform claim.

### Final corrected-source qualification and matched Java comparison

The corrected frozen candidate is `aot-effect-final2-candidate-ilvmt6iz`, identity
SHA-256 `46fbbaff96bf9345b6353a6a449a28579e56c258f9a1a1c1a3de6eaf9fb27399`.
It contains the compiler parameter-scan cancellation fix, corrected 129-parameter
fixture and clarified helper/context contracts. The first new-fixture attempt
failed because Qore does not support the introduced list.push syntax; its logs
and source snapshot are preserved. Qore push statements corrected the fixture,
which then passed four cases and 452 assertions in release and debug.

Release passed 111 selected files, 399 cases / 34027 assertions, 15 standalones
and one skip. Debug passed 111 files, 406 cases / 34126 assertions, 15 standalones
and no skips. Both passed seven exception/stack fixtures and IR/AOT reference
grids. All 205 debug native checks, 17 harness tests and 555 references per
IR/AOT/Java-interpreted/Java-JIT mode passed. Thirteen owned qmod aliases were
restored to release and content-verified, unrelated aliases were unchanged,
and five restored-release smoke fixtures passed. Independent qualification
record SHA-256: `a59609557e5efe21189ee8680be84edcbef87de874ac47ffbec7cb1893fad900`.

Fresh archive `aot-effect-final2-bench-0wmb720q` contains 162 jobs / 810 measured
windows: the same nine kernels, three forks, three warmups and five 500-ms
samples per fork, count 4096, randomized order seed 831307. Six interleaved modes
are qualified baseline 21b2b072b7, the prehash candidate, corrected enabled-A/B,
corrected effect-entry-disabled, and Temurin 25.0.2/JMH 1.37. Each Qore revision
uses its separately compiled matching frozen compiler/runtime. A/A and runtime
on/off reuse the identical corrected executable. The final disabled control
disables the whole effect companion, not only prehashing. All six modes pass
555 references; loader paths and generated Java target no-inline hints were
verified. No owned build, test, profile or inspection overlapped timing.

The table is one matched comparison. Values are medians of fork means in
whole-workload ns/element, including loops, representation and bookkeeping;
they are not isolated call latency. Positive reductions mean less elapsed time.

|!Workload|!Before|!Enabled A / B|!Reduction A / B|!Java JIT|!Enabled / Java
|member updates|242.3041|107.3614 / 108.4253|55.691% / 55.252%|1.9116|56.16 / 56.72x
|read-only integer member|47.3979|48.5314 / 48.5907|-2.391% / -2.517%|2.6748|18.14 / 18.17x
|read-only float member|53.7084|54.0298 / 53.8973|-0.598% / -0.352%|2.5946|20.82 / 20.77x
|boxed integer method|32.2797|32.6222 / 32.6204|-1.061% / -1.055%|2.6524|12.30 / 12.30x
|boxed float method|39.3433|39.2211 / 38.8058|0.311% / 1.366%|2.6577|14.76 / 14.60x
|native integer result|10.2719|10.4900 / 10.6078|-2.123% / -3.269%|3.0762|3.41 / 3.45x
|native float result|11.9715|11.9898 / 12.2075|-0.153% / -1.972%|3.5681|3.36 / 3.42x
|indirect integer result|7.3719|7.3188 / 7.3217|0.720% / 0.680%|1.9598|3.73 / 3.74x
|indirect float result|9.2538|9.2650 / 9.2967|-0.121% / -0.464%|2.9336|3.16 / 3.17x

The target is 2.23–2.26 times faster than the committed baseline, with separated
observed fork ranges in both enabled arms. Its same-binary disabled result is
270.1539 ns; enabling the companion reduces that by 59.865–60.259%, also with
separated ranges. Compared with the preceding prehash candidate's 109.5354 ns,
the corrected medians are 1.013–1.985% lower, but ranges overlap: no additional
runtime speedup is established for the compiler-only audit correction.

The pure boxed-integer regression reappears: 1.055–1.061% slower than baseline
with separated ranges in both arms, and 0.592–0.598% slower than the preceding
prehash candidate, also separated. Its disabled result is 32.8601 ns, so the
effect entry being enabled is not established as the cause. Native-integer
medians remain 2.123–3.269% slower than baseline; ranges overlap in this run,
without erasing the earlier separated-range regression. Read-only integer
medians are 2.391–2.517% slower with overlapping baseline ranges; only enabled-A
separates from the preceding candidate. Other baseline control comparisons
overlap. These are accepted, explicitly tracked tradeoffs for the scoped
member-update gain, not a universal improvement or proof of equivalence.

No samples were dropped. In particular, the enabled-A indirect-integer fork
means span 7.2063–14.0349 ns and the preceding effect candidate spans
107.9440–129.0236 ns; these noisy forks remain in the raw record and ranges.
Observed ranges are not confidence intervals. All 810 final windows were
independently replayed; final verification SHA-256 is
`e861f91b34a91a2ed0bcca7f07d59d789c1d905050c02659c4fd5f1385d3c1e9`.
The public record retains all 2295 windows across the three complete runs.
Java parity remains unachieved, including same-Program calls. This is primarily
an AOT compiler/runtime gain; no new IR/JIT timing benefit is claimed.
The [complete audit](FOLLOWUP-AUDIT.md#aot-member-update-prefix-and-cold-continuation-audit-2026-10-12)
records all 62 checks: 19 Pass, 43 N/A and no remaining failures. Strict table
validation passed for all four changed documentation files.

Next: profile the qualified boxed caller, then separately measure immediate
scalar-store specialization and preparation of immutable auto-local facts and
stable invocation-local slots. Keep actual runtime-slot publication and live
ownership/reentry checks. A separate write-lock lead is to attempt uncontended
acquisition under the existing mutex before registering a waiter, retrying after
registration before any wait; it requires dedicated contention/notification
validation. Neither lead is implemented or measured here, and neither is a
proven cure for the control regressions. Continue publishing adverse results.

[Verified frozen measurements and candidate status](results-aot-effect-member-prefix-macos-arm64.json).


## Direct AOT primitive local replacement (2026-10-12)

This runtime-only AOT follow-up replaces generic scalar storage with direct
primitive extraction after the existing declaration/input/live-slot guards.
It changes neither admission nor runtime-slot visibility. Old and new values
cannot own nodes, so no conversion, destructor, reference scan or allocation
is needed. The ordinary fallback remains, including typed/reference/captured
locals, nodes, wide boxed integers, escaped floats and enum identity.

The new first-use presence control QORE_DISABLE_AOT_DIRECT_SCALAR_STORE
restores the previous generic scalar operation after identical admission.
QORE_DISABLE_AOT_SCALAR_LOCAL_STORE still disables the entire shortcut and
takes precedence. One immutable enum combines both controls into one static
initialization guard. Declaration checks and TLS/runtime-slot lookup are
still performed per assignment; they are not claimed as eliminated.

The first candidate used two separate static booleans. Its independently
verified 660-window screen improved boxed calls but slowed list build by
3.767–4.403%, with separated observed fork ranges. The 750-window compact
control repeat preserved call gains and left list build 0.638–0.929% slower
than its matched baseline, with overlapping ranges. List mutation and native
integer comparisons also overlapped. Neither result establishes equivalence
or the cause of the earlier regression: list build uses another, unchanged
no-coerce assignment helper. Instrumented profiles are diagnostic only,
excluded from timing evidence; they still show local reload and lookup work.

Initial qualification passed release, then debug compilation found four
native test callers using the old one-argument helper signature. Corrected
tests explicitly exercise both direct and generic modes. The debug build
passes 257 native checks, including exact bits and rejection without slot
or ownership mutation. Its rebuilt release qore/qcc/libqore/runtime-identity
artifacts are byte-identical to the compact candidate. Nevertheless, the
corrected source was frozen and selected qualification repeated. Both this
failure and the earlier missing-generated-qmod-alias setup failure are kept
in the evidence, not counted as successful runs.

### Corrected-source qualification

Release: 112 selected files, 415 cases / 34062 assertions, 15 standalones and 9 skipped.
Debug: 112 selected files, 422 cases / 34161 assertions, 15 standalones and 8 skipped.

Both builds pass seven exception/stack fixtures and IR/AOT reference grids.
All 257 debug native checks, 17 harness tests and 555 references per
IR/AOT/Java-interpreted/Java-JIT mode pass. Thirteen owned qmod aliases are
restored and content-verified against release artifacts; unrelated aliases
are unchanged. Six restored-release regression fixtures pass. This is not
the full repository suite, sanitizer testing or cross-platform coverage.
Eight NaN-boxing resident-memory checks per build require Linux
/proc/self/smaps_rollup and are skipped on this Mac; value/representation
checks still run. Release also skips one debug-only short-string check.
The separate native-unit wrapper skips in release and runs in debug.

Candidate identity SHA-256: 8d1bbcfd88b8775b6026bfe9d842552afa6b951c836df02fc46fa549c56ed478.
Qualification SHA-256: 158a3bfcaddfad85c5757d25f934a0fb1c37d35cd04df54faaa4539d407d6c0a.

### Final matched AOT/Java comparison

Archive aot-direct-store-final-bench-t0lc4ek6 contains 165 jobs / 825 measured windows:
eleven kernels, three forks, three warmups and five 500-ms samples per fork,
count 4096, randomized kernel/fork blocks with seed 847913. Five interleaved
modes are the post-merge baseline, corrected candidate A/A, same-candidate
direct-store-disabled, and Temurin 25.0.2 with JMH 1.37. Separately compiled
Qore artifacts use matching frozen compiler/runtime pairs; candidate A/A and
on/off share one executable. Library selection, source/artifact hashes, all
555 references per mode and generated Java target no-inline hints are checked.
No other owned build, test, profile or inspection overlaps timing.

Values are medians of fork means, in whole-workload ns/element; lower is
better. Positive reductions mean less elapsed time. These include loops,
representation, ownership and bookkeeping, not isolated call latency.
Qore and Java compute matching results but do not have identical runtime
contracts. List workloads include their complete passes, not a single store.

|!Workload|!Before|!Enabled A / B|!Reduction A / B|!Direct disabled|!Java JIT|!Enabled / Java
|boxed integer method|30.8726|27.5662 / 27.8877|10.710% / 9.669%|31.1826|2.3244|11.86 / 12.00x
|boxed float method|37.2223|34.8622 / 34.8967|6.341% / 6.248%|38.1776|2.5522|13.66 / 13.67x
|member updates|102.8618|98.2055 / 97.7927|4.527% / 4.928%|102.7119|1.8915|51.92 / 51.70x
|read-only integer member|45.6111|42.2649 / 41.9829|7.336% / 7.955%|46.8084|2.5384|16.65 / 16.54x
|read-only float member|51.0062|47.0904 / 46.9408|7.677% / 7.970%|51.2254|2.5484|18.48 / 18.42x
|native integer result|10.5815|10.3450 / 10.5055|2.235% / 0.718%|10.3947|3.0720|3.37 / 3.42x
|native float result|11.3825|11.2944 / 11.1732|0.774% / 1.839%|11.2514|3.4664|3.26 / 3.22x
|indirect integer result|6.9317|6.9200 / 6.8718|0.169% / 0.865%|7.0003|1.8845|3.67 / 3.65x
|indirect float result|8.8353|8.8696 / 8.8828|-0.388% / -0.538%|8.8753|2.6528|3.34 / 3.35x
|list build|84.2808|84.0400 / 83.2658|0.286% / 1.204%|84.0528|2.5837|32.53 / 32.23x
|list mutation|232.4295|235.1289 / 231.7627|-1.161% / 0.287%|231.8858|4.1309|56.92 / 56.10x

All final before/after comparisons are recorded below with their observed
fork-range relationship. Separation is descriptive, not a confidence interval
or proof of causation; an overlap does not establish equivalence.

|!Workload|!Enabled arm|!Elapsed-time reduction|!Observed fork ranges
|boxed integer method|enabled-a|10.710%|separated
|boxed integer method|enabled-b|9.669%|overlap
|boxed float method|enabled-a|6.341%|separated
|boxed float method|enabled-b|6.248%|overlap
|member updates|enabled-a|4.527%|separated
|member updates|enabled-b|4.928%|separated
|read-only integer member|enabled-a|7.336%|separated
|read-only integer member|enabled-b|7.955%|separated
|read-only float member|enabled-a|7.677%|separated
|read-only float member|enabled-b|7.970%|separated
|native integer result|enabled-a|2.235%|overlap
|native integer result|enabled-b|0.718%|overlap
|native float result|enabled-a|0.774%|overlap
|native float result|enabled-b|1.839%|separated
|indirect integer result|enabled-a|0.169%|overlap
|indirect integer result|enabled-b|0.865%|overlap
|indirect float result|enabled-a|-0.388%|overlap
|indirect float result|enabled-b|-0.538%|overlap
|list build|enabled-a|0.286%|overlap
|list build|enabled-b|1.204%|overlap
|list mutation|enabled-a|-1.161%|overlap
|list mutation|enabled-b|0.287%|overlap

Both target medians improve in this Java matrix, but enabled-B has noisy
forks: the boxed-integer range overlaps baseline, and the boxed-float range
overlaps both baseline and disabled. In particular, the float enabled-B
fork means span 34.5997–44.7781 ns. The list-mutation A/A arms also separate.
These observations prompted the fixed longer confirmation below; no sample
was removed, and no source change was made between these runs.

### Fixed longer confirmation

Archive aot-direct-store-confirm-bench-kaxnh5da contains 120 jobs / 600 windows:
six kernels, four Qore configurations, five forks, three warmups and five
750-ms windows, count 4096, randomized order seed 951427. Matching frozen
compiler/runtime pairs, A/A identity, on/off controls, source/artifact
hashes, library selection and all 555 references are verified again.
Java is compiled and reference-checked but not timed here; the Java ratios
above come only from the matched Java matrix. No other owned work overlaps
the confirmation timings. This is a separate run, not a replacement sample.

|!Workload|!Before|!Enabled A / B|!Reduction A / B|!Direct disabled
|boxed integer method|30.7152|27.2478 / 27.3659|11.289% / 10.904%|30.9099
|boxed float method|36.7555|34.5615 / 34.4255|5.969% / 6.339%|37.8685
|member updates|102.4810|97.4491 / 97.0714|4.910% / 5.279%|102.0608
|native integer result|10.4505|10.3126 / 10.1950|1.319% / 2.445%|10.3810
|list build|82.1631|82.8251 / 82.6599|-0.806% / -0.605%|82.6518
|list mutation|227.2860|229.0337 / 227.3948|-0.769% / -0.048%|225.4036

Both boxed targets and member updates improve with separated observed
ranges against baseline and same-binary disabled controls in both A/A arms.
Boxed integer improves 10.904–11.289% versus baseline, boxed float
5.969–6.339%, and member updates 4.910–5.279%. Native-integer baseline
comparisons also separate (1.319–2.445% less time), but on/off overlaps;
that change cannot be attributed to direct primitive copying.

Adverse controls remain: list construction takes 0.605–0.806% more time
and list mutation 0.048–0.769% more versus baseline, with overlapping
ranges. Their on/off comparisons also overlap. This does not establish
equivalence or erase the initial list-build regression. All A/A ranges
overlap in this confirmation. The target gains are retained with these
explicit limits, not as a universal execution speedup.

Confirmation verification SHA-256: 882697e2568827c292f5144e0b5e7cc9540cf6931bce925036fd27d9c2112e2e.

Every raw window is retained, including slower controls and noisy forks.
All 2835 windows across the initial, compact-control, Java and confirmation runs have
independent replay records. No ratios combine Java from a different run.
Final verification SHA-256: 331d155e2c640c9d1d323043491d5c2c6a05f40a2dd9c69c258be89b6e5ba55c.

The improvement is specific to the AOT local-store helper. No new IR/JIT
timing gain or Java parity is claimed. The existing fixture limitation on
repeated block-local reflection remains documented: all modes check values,
while the complete reflected block map is required only in AST.

Next: prepare immutable ordinary-auto declaration facts and stable
invocation-local body-slot handles after callee instantiation, preserving
live slot flags, recursion, fallback and actual runtime publication. Caller
reload work remains another measured target, but skipping it requires proof
covering argument/receiver/result cleanup and reentry, not just a pure
same-Program callee. Neither follow-up is implemented by this change.

[Verified measurements, adverse controls and qualification](results-aot-direct-scalar-store-macos-arm64.json).

## Prepared ordinary-auto AOT body slots (2026-10-12)

This change resolves eligible runtime-backed auto body-local storage once after
callee instantiation and passes the borrowed address to checked assignments.
It removes repeated slot lookup, not declaration/input/live-state checks,
runtime publication, reflection, caller reloads or ownership handling. The
handle is invocation-local, never shared or retained beyond cleanup. Parameters,
captured/self/top/read-only/typed locals, private IR-only storage and no-coerce
assignments are excluded. No existing helper ABI is removed.

The first recursive fixture exposed a prerequisite correctness problem: direct
fast entries pushed ordinary body slots but only cleared them on return. The
strengthened baseline test fails before the parent overwrites its value; reverse
body-slot pops fix it. Wrapper-owned entries and block exits remain clear-only,
and captured locals retain their separate lifecycle. This fix is not disabled
for performance comparisons. Cleanup emission also checks compiler cancellation
every 100 scanned locals, rejects the partial function and permits reuse of the
same lowerer. There is no new runtime hot-path cancellation polling.

QORE_DISABLE_AOT_AUTO_LOCAL_SLOT is a presence control: at compilation it omits
the getter/prepared assignment; at first runtime getter use it returns null,
retaining lookup in the identical executable. Neither mode disables the cleanup
fix or existing scalar-store shortcuts. Before/after use separately compiled,
matching frozen compiler/runtime pairs because the runtime identity changed.

### Selected qualification

Release: 114 files, 417 cases / 34417 assertions, 15 standalones and 9 skipped.
Debug: 114 files, 424 cases / 34516 assertions, 15 standalones and 8 skipped.
Both builds pass seven exception/stack fixtures and IR/AOT reference grids.
All 385 focused native checks, 17 harness tests, 555 references in each restored
IR/AOT/Java-interpreted/Java-JIT mode and eight restored-release smoke fixtures
pass. All 13 owned release qmod aliases/artifacts are restored and verified;
unrelated aliases are unchanged. Eight Linux-only RSS checks per build are
skipped on this Mac, plus the release debug-only short-string check. The native
unit wrapper separately skips in release and executes in debug.

The new auto-slot fixture has 331 assertions across AST/IR/JIT/tiered and AOT
O0/O3, native-EH, split, no-debug, stripped-source and disabled configurations.
It covers recursion beyond one stack block, repeated block lifetimes, reflective
mutation, captured/typed/parameter fallbacks, representation changes, destruction,
reentry and unwind. The existing incomplete source-specialized recursive/block
reflection maps remain documented; full block-map inspection is AST-only.
The compiler-cancellation fixture adds eight assertions with normal/fast entries,
both rejection checkpoints, LLVM verification and same-lowerer recovery.
This is not full-repository, sanitizer, leak or cross-platform qualification.

Candidate identity: f78cd8166016eca2f463407c74f1f54254dcd15958dcc2ecbcb9d27f9b7ab000.
Qualification: 50c298d8b1915c8ac68ac55aa8b5e5b28fc0df56750b8550b832863f3082f13c.

### Initial screen and longer confirmation

The first screen has 360 windows: six kernels, four Qore modes, three forks,
three warmups and five 500-ms windows, count 4096, order seed 108126. Boxed int/float
gains have separated observed baseline/off ranges, but list build is 1.205-1.959%
slower with separated baseline ranges. No sample is removed.

|!Workload|!Before|!Enabled A / B|!Reduction A / B|!Runtime disabled
|boxed integer method|27.4263|25.7060 / 25.9442|6.273% / 5.404%|27.4722
|boxed float method|34.2432|32.2182 / 32.3284|5.914% / 5.592%|34.0462
|member updates|96.7754|94.7669 / 94.9249|2.075% / 1.912%|96.8115
|native integer result|10.2619|10.1678 / 10.1655|0.918% / 0.940%|10.1385
|list build|82.4490|84.0641 / 83.4423|-1.959% / -1.205%|83.1157
|list mutation|228.4857|225.4465 / 225.3863|1.330% / 1.357%|226.4048

The fixed longer confirmation has 600 windows: the same six kernels/four modes,
five forks, three warmups and five 750-ms windows, count 4096, seed 109127. Boxed
int/float gains repeat against baseline/off with separated ranges. Member-update,
native-int and list-mutation comparisons overlap. List build is 1.459-2.852%
slower with overlapping ranges; this does not erase the earlier result. All
A/A ranges overlap. Neither run times Java.

|!Workload|!Before|!Enabled A / B|!Reduction A / B|!Runtime disabled
|boxed integer method|27.5608|25.6865 / 26.0591|6.801% / 5.449%|27.7258
|boxed float method|34.4672|32.3994 / 32.3951|5.999% / 6.012%|33.9778
|member updates|96.4847|95.1503 / 95.3594|1.383% / 1.166%|97.7370
|native integer result|10.3882|10.0966 / 10.1206|2.807% / 2.576%|10.1089
|list build|82.2832|83.4834 / 84.6300|-1.459% / -2.852%|84.2805
|list mutation|228.7899|226.5672 / 226.2598|0.971% / 1.106%|226.4852

### Matched Java comparison

Archive aot-auto-slot-final-bench-xwg61goo has 165 jobs / 825 measured windows:
eleven kernels, baseline, enabled A/A, same-binary runtime-off and Java JIT,
three forks, three warmups, five 500-ms windows, count 4096, seed 117129. Java uses
Temurin 25.0.2/JMH 1.37 with checked generated target dontinline hints. All modes
pass 555 references; source/artifact hashes and library selection are verified.
No other owned work overlaps timing. Every raw window is independently replayed.

Values are medians of fork means in whole-workload ns/element, not isolated
call latency. Positive reductions mean less elapsed time. Runtime contracts,
representation, ownership and synchronization differ between Qore and Java.

|!Workload|!Before|!Enabled A / B|!Reduction A / B|!Runtime disabled|!Java JIT|!Enabled / Java
|boxed integer method|27.6506|26.1308 / 25.6985|5.496% / 7.060%|28.1138|2.3446|11.15 / 10.96x
|boxed float method|35.0381|32.9671 / 32.9212|5.911% / 6.042%|34.6775|2.5595|12.88 / 12.86x
|member updates|97.8814|96.0037 / 96.9536|1.918% / 0.948%|98.5485|1.8693|51.36 / 51.87x
|read-only integer member|42.3394|40.5357 / 40.8622|4.260% / 3.489%|42.3764|2.5726|15.76 / 15.88x
|read-only float member|47.1359|45.7988 / 45.2370|2.837% / 4.029%|47.3563|2.5234|18.15 / 17.93x
|native integer result|10.4031|10.1207 / 10.3155|2.714% / 0.842%|10.1012|3.0596|3.31 / 3.37x
|native float result|11.2862|11.5123 / 11.4955|-2.003% / -1.854%|11.3595|3.4678|3.32 / 3.31x
|indirect integer result|6.9083|6.8807 / 6.9232|0.400% / -0.215%|6.9313|1.8778|3.66 / 3.69x
|indirect float result|8.8476|8.8337 / 8.8337|0.157% / 0.158%|8.8615|2.7879|3.17 / 3.17x
|list build|83.4258|85.3845 / 84.6187|-2.348% / -1.430%|84.7275|2.5397|33.62 / 33.32x
|list mutation|229.2804|228.7152 / 230.0634|0.246% / -0.342%|230.9583|4.0667|56.24 / 56.57x

Both boxed-call targets, read-only members and member updates improve against
baseline/off with separated observed ranges. Native-int baseline ranges separate
but its off comparisons overlap, so its gain is not attributed to slot lookup.
Indirect controls overlap. Native float is 1.854-2.003% slower: enabled-A baseline
ranges separate, enabled-B overlaps. List build is 1.430-2.348% slower with
separated baseline ranges in both arms; off comparisons overlap. List mutation
has mixed medians and separated A/A ranges. Read-only float A/A also separates.
Observed ranges are descriptive, not confidence intervals or causal proof.

Verification: bb74556ce1a422df22bc83c8408a60c1cc4e557825f1049b0644e1b7b4b23713.

### Compile-time control and retention

The adverse controls prompted a separate fixed 300-window run, not a replacement
sample: four kernels, five modes, three forks, three warmups, five 750-ms windows,
count 4096, seed 118130. The extra executable is compiled with preparation disabled
and uses the same candidate runtime/environment as enabled A/A. Symbol checks
confirm no auto-slot getter/store while retaining correct body-slot pops. All
five modes pass 555 references and matching-library checks. No Java timing here.

|!Workload|!Before|!Enabled A / B|!Reduction A / B|!Runtime disabled|!Compile disabled
|boxed integer method|27.5009|25.7439 / 25.8379|6.389% / 6.047%|27.8311|27.6095
|boxed float method|34.0078|32.2915 / 32.5480|5.047% / 4.293%|34.0260|33.4435
|native float result|11.1933|11.2793 / 11.4125|-0.769% / -1.959%|11.4533|11.1818
|list build|83.0201|83.0851 / 84.0030|-0.078% / -1.184%|83.0320|83.3694

Boxed integer gains 6.047-6.389% against baseline and 6.417-6.757% against compile-off;
boxed float gains 4.293-5.047% and 2.678-3.445%, respectively. All these ranges
separate, as do both runtime-off target comparisons. List build remains 0.078-1.184%
slower in median, with overlapping baseline/runtime-off/compile-off ranges.
Native float remains 0.769-1.959% slower, also with overlapping comparisons.
All A/A ranges overlap. This does not establish equivalence, diagnose the
earlier slowdowns or invalidate the separated adverse results in previous runs.

Verification: 68ee94799be319b01fb9f2f360f0ad25f0c20435379517cec94048f0d5d58e82.

Retention decision: retain the repeated boxed-call improvement with the modest
adverse list/native-float results explicitly recorded, not as a universal
execution speedup. All 2085 windows across four runs remain in the evidence.
Disabling preparation has not isolated a separated list-build cost; the cleanup
correctness fix is retained in every candidate control. No IR/JIT timing gain,
Java parity, regression-free result or causal code-layout explanation is claimed.

Four serial diagnostic profiles are excluded from timing evidence. Before/after
list profiles show generic local assignment/lvalue cleanup, list/type checks,
value ref/discard and list operations. Resolved Mach-O call inventories show no
auto-slot helper in listBuild; the fast-entry call difference is clear-to-pop
cleanup. Equal call counts/profile similarity do not prove equal runtime cost.
Boxed profiles still show runtime-local reload/evaluation, per-store checks,
typed method dispatch, ownership and stack/frame bookkeeping. Waiting signal
thread samples are not application CPU work; sample weights are not latency
attribution.

Next: investigate prepared access for caller reloads and hoist only proved
immutable declaration checks. Keep live slot/receiver guards, actual publication,
cleanup/reentry semantics and observability. Typed list assignment/coercion is a
separate execution bottleneck. The fastest retained-call workloads remain about
3-4x Java; boxed calls about 11-13x and member updates about 52x in this matched run.

[All verified samples, qualification and audit](results-aot-auto-local-slot-macos-arm64.json).

## Prepared runtime-backed AOT local reloads (2026-10-12)

Eligible stale-cache reloads now reuse ordinary body-local storage resolved once
after invocation-local instantiation. The handle removes LocalVar/TLS/stack
lookup, not live slot evaluation, reference resolution, ownership or reflection.
The shared reload protocol snapshots both old owners before evaluation, publishes
the cache/current/deferred owner chain before deferred destruction, and publishes
the completed epoch last, including on sink failure. Reentry and block clearing
retain the same storage and evaluation semantics.

Parameters, captures, self, top-level and private IR-only storage are excluded.
Existing native/weak-assigned/reload-exempt restrictions remain. Reload admission
can accept typed/read-only body locals; store restrictions are independent.
QORE_DISABLE_AOT_PREPARED_LOCAL_RELOAD is a presence control: compilation omits
preparation, while first runtime getter use returns null and retains lookup in
the identical executable. Store preparation and existing cleanup remain enabled.
New versioned helpers require matching compiler/runtime artifacts. This is an
AOT code-generation optimization; no new IR/JIT timing benefit is claimed.

Baseline: merged revision 2b1a4145a28a23ecfbf841512cbe796dd562aec7.
Candidate identity: 44a7363b0eaf1c3494dcb8778429c5a7c9a56f0070cadcd12d79f9698e9d04da.
Measurements use frozen matching old/new compiler and runtime pairs, identical
enabled A/A binaries/environments, equal-length aliases/control variables,
shuffled serial jobs, and all raw windows without exclusions. No other owned
work overlaps timing. Values below are medians of fork means in whole-workload
ns/element, not isolated ABI call latency. Positive reduction means less time.

### Initial and longer confirmation

Initial: 600 windows, eight kernels, five modes, three forks, three warmups,
five 500-ms windows, count 4096, seed 119131. Both boxed-int and read-only targets
separate from baseline/runtime-off/compile-off. Boxed-float enabled-A includes
a retained noisy fork (54.7209 ns/element); enabled-B separates from controls.
Member updates improve versus baseline, but enabled-A runtime-off ranges overlap
and A/A ranges separate. Native-float off ranges overlap. List build is
1.095-2.900% slower; enabled-A baseline ranges barely separate, the other controls
overlap. List mutation is mixed with overlapping ranges.

|!Workload|!Before|!Enabled A / B|!Reduction A / B|!Runtime disabled|!Compile disabled
|boxed integer method|26.4822|22.7432 / 22.7110|14.119% / 14.240%|25.8293|25.3618
|boxed float method|32.5050|29.7654 / 29.3336|8.428% / 9.756%|32.3316|31.6869
|member updates|98.1382|91.7546 / 90.6003|6.505% / 7.681%|95.2605|95.2469
|read-only integer member|41.5164|36.8102 / 36.7408|11.336% / 11.503%|40.7575|39.8291
|read-only float member|45.1285|42.2776 / 42.2573|6.317% / 6.362%|44.5104|44.8054
|native float result|11.6732|11.5388 / 11.5063|1.151% / 1.429%|11.5059|11.4616
|list build|84.1651|85.0865 / 86.6058|-1.095% / -2.900%|85.6085|85.1832
|list mutation|229.6627|231.6475 / 228.5647|-0.864% / 0.478%|229.8522|231.0993

Longer confirmation: 750 windows, six kernels, five modes, five forks, three
warmups, five 750-ms windows, count 4096, seed 120132. Both enabled arms of all
four boxed/member targets separate from baseline, runtime-off and compile-off.
All A/A ranges overlap. List build is 1.435-1.825% slower with overlapping
controls. Native float is mixed versus baseline; enabled-B is 1.847% slower than
compile-off with separated ranges. The earlier adverse observations remain.

|!Workload|!Before|!Enabled A / B|!Reduction A / B|!Runtime disabled|!Compile disabled
|boxed integer method|26.6650|22.7190 / 22.8470|14.798% / 14.318%|25.3747|25.0615
|boxed float method|32.5559|29.4674 / 29.3663|9.487% / 9.797%|32.2080|32.1118
|member updates|98.2118|90.9409 / 90.8295|7.403% / 7.517%|93.8682|94.9816
|read-only integer member|41.7744|37.0809 / 37.2146|11.235% / 10.915%|40.1622|40.2100
|native float result|11.4933|11.3743 / 11.5217|1.035% / -0.248%|11.4243|11.3128
|list build|83.4142|84.9365 / 84.6110|-1.825% / -1.435%|84.2429|84.2654

### Short invocations

450 windows: the same six kernels/five modes, count 1, three forks, three
warmups, five 500-ms windows, seed 121133. All six workloads improve by
5.230-8.905% versus baseline; both arms separate from baseline/runtime-off/
compile-off and A/A ranges overlap. These are whole short invocations, not
isolated entry costs or proof that every cold branch is free. No Java timing.

|!Workload|!Before|!Enabled A / B|!Reduction A / B|!Runtime disabled|!Compile disabled
|boxed integer method|2479.4686|2296.5792 / 2298.2568|7.376% / 7.308%|2497.4819|2505.7922
|boxed float method|2456.8292|2285.0540 / 2258.3119|6.992% / 8.080%|2438.8434|2442.9743
|member updates|3366.8710|3190.7767 / 3178.2575|5.230% / 5.602%|3374.8345|3386.3805
|read-only integer member|2500.8713|2291.2679 / 2278.1748|8.381% / 8.905%|2502.4729|2472.2827
|native float result|2406.1655|2216.5373 / 2218.3525|7.881% / 7.805%|2416.9314|2381.5882
|list build|2747.0233|2552.8728 / 2574.6022|7.068% / 6.277%|2795.6057|2743.2030

### Fresh matched Java comparison

825 windows: eleven kernels, baseline, enabled A/A, same-binary runtime-off and
Java JIT, three forks, three warmups, five 500-ms windows, count 4096, seed
122134. Temurin 25.0.2 and JMH 1.37 use checked generated target dontinline hints.
Every mode passes 555 reference checks; actual Qore library selection is checked.
Runtime contracts, representation, ownership and synchronization differ; this is
not a general language ranking or comparison with freely inlined Java targets.

|!Workload|!Before|!Enabled A / B|!Reduction A / B|!Runtime disabled|!Java JIT|!Enabled / Java
|boxed integer method|26.7062|22.7887 / 22.3518|14.669% / 16.305%|25.3683|2.5224|9.03 / 8.86x
|boxed float method|32.7103|29.2555 / 29.3382|10.562% / 10.309%|32.3071|2.5480|11.48 / 11.51x
|member updates|97.4167|90.0560 / 90.2198|7.556% / 7.388%|93.9815|1.8696|48.17 / 48.26x
|read-only integer member|41.0191|37.0169 / 36.8722|9.757% / 10.110%|39.4909|2.5306|14.63 / 14.57x
|read-only float member|44.1220|41.1133 / 41.0251|6.819% / 7.019%|43.7654|2.5072|16.40 / 16.36x
|native integer result|10.0754|10.2721 / 10.1948|-1.952% / -1.185%|10.2568|3.0382|3.38 / 3.36x
|native float result|11.3332|11.3685 / 11.3287|-0.312% / 0.039%|11.3911|3.4603|3.29 / 3.27x
|indirect integer result|7.0195|6.9809 / 6.8096|0.550% / 2.990%|6.9354|1.8738|3.73 / 3.63x
|indirect float result|8.7975|8.8216 / 8.8530|-0.274% / -0.631%|8.8241|2.6036|3.39 / 3.40x
|list build|82.4584|84.7595 / 83.7661|-2.791% / -1.586%|83.4784|2.5274|33.54 / 33.14x
|list mutation|227.4403|228.7078 / 230.6630|-0.557% / -1.417%|230.7113|4.0777|56.09 / 56.57x

Both arms of all five boxed/member workloads improve with separated baseline
and runtime-off ranges. Boxed-integer A/A ranges also separate; retain both arms
rather than selecting the faster one. Native integer is 1.185-1.952% slower than
baseline (enabled-B ranges separate); both off comparisons overlap. Native float
and indirect controls overlap. List build is 1.586-2.791% slower and mutation
0.557-1.417% slower; all their before/off/A/A ranges overlap. Overlap does not
establish equivalence, and range separation is not a confidence interval or a
causal explanation. These results do not erase the earlier adverse controls.

Only this matched matrix supplies the current Java ratios: native/indirect
workloads remain about 3.3-3.7x Java, boxed int about 8.9-9.0x, boxed float 11.5x,
read-only members 14.6-16.4x, updates 48x and lists 33-57x. Java parity is not reached.
Verification: 05acb34edaaab6a8318ca6427660f9baaeaea5a63b7b1bef364f933927458b4f.

### Qualification, profiles and retention

Release: 114 files, 417 cases / 35301 assertions, 15 standalones and 9 skipped.
Debug: 114 files, 424 cases / 35400 assertions, 15 standalones and 8 skipped.
Both pass seven exception/stack fixtures and IR/AOT reference grids. Also pass:
483 focused native checks, 17 harness tests, 555 references per restored IR/AOT/
Java-interpreted/Java-JIT mode, and eight restored-release smoke fixtures. All 39
owned release qmods/aliases in the declared dependency closure are verified;
unrelated aliases are unchanged. Eight Linux-only RSS checks per build and the
release debug-only short-string check are skipped; the native wrapper separately
skips release and executes in debug. Not full-repository, sanitizer, leak or
cross-platform qualification.

AOTRuntimeLocalReload has 2236 assertions and AOTAutoLocalSlot has 419, including
O0/O3, EH, split, no-debug and independent compiler/runtime/store controls.
Native tests directly force prepared reloads, including when the getter control
is disabled: exact owner counts, fresh epochs, publication before deferred
destruction, reentrant replacement, cleanup exceptions, clearing and reassignment.
Existing recursive/multiblock, reference, reflection and fallback coverage remains.
The pre-existing incomplete source-specialized block reflection maps remain
documented; this change does not claim to repair them.

Preserved setup failures: an initial debug test descriptor typo was corrected
before freezing the measured candidate. The first broad qualification attempt
stopped before tests because its harness rejected 21 CMake-declared transitive
qmod aliases; the corrected harness uses the complete 39-module dependency graph
and restores matching release artifacts. Neither failure is silently excluded.

Eight serial diagnostic profiles confirm use of prepared reloads. Remaining
boxed-call work includes dispatch, live slot evaluation, store admission and
ownership; lists retain generic assignment, coercion and ref/discard work.
Only complete main-thread trees are counted; family sample shares overlap.
Instrumented timings are excluded. Neither profiles nor static call inventories
diagnose the small adverse timing differences.

Retention: retain repeated boxed/member gains after selected qualification and
the complete 62-item audit, with all 2625 windows and adverse controls recorded.
This is not a universal speedup, regression-free result or Java-parity claim.
Next, investigate hoisting immutable auto-store declaration admission while
keeping live slot/input/ownership guards, then remaining dispatch/frame costs.
Typed container assignment/coercion remains a separate AOT execution priority.

[Verified measurements, qualification and audit](results-aot-prepared-local-reload-macos-arm64.json).

## Prepared AOT auto-store declaration admission (2026-10-12)

Eligible ordinary auto body locals now prepare immutable declaration admission
once after invocation-local instantiation. The v2 getter checks the complete
declaration and existing same-Program storage contract. Each store still checks
the incoming immediate representation, exception sink and live slot flags/type.
Actual publication, ownership, generic fallback, reflection, reloads and cleanup
are unchanged. Parameters, captures, self, top-level, read-only, no-narrow,
substituted and private IR-only declarations remain excluded.

QORE_DISABLE_AOT_AUTO_LOCAL_DECLARATION selects v1 getter/store code at compile
time. Runtime presence at first v2 store use restores per-store declaration
checks with the same prepared storage. The runtime-off path also includes v2
delegation, so its time difference is not an isolated predicate cost. Existing
slot/scalar/direct/reload controls remain independent. Old ABIs remain; new
symbols and private-header runtime identity require matching compiler/runtime
artifacts. No new IR/JIT speedup is claimed.

Baseline: merged revision 2c24732a2fbf054b59b66d4ab8d6b0c30844ef2b.
Final candidate identity: 98b249fa1a57ecac3fbd04e843dcc1d7641708d36f76b4a9573d32a34747d745.
All six matrices use matching frozen compiler/runtime pairs, identical enabled
A/A artifacts, ordered environments and argv, equal-length aliases/control
variables, shuffled serial jobs and every raw window without exclusions.
No other owned work overlaps timing. Values are medians of process means in
whole-workload ns/element, not isolated call latency. Positive reduction is less
time. Range overlap is not equivalence; separation is not a confidence interval
or causal explanation. All earlier candidate and adverse observations remain.

### Initial outlined candidate

675 windows: nine kernels, five modes, three forks, three warmups and five
500-ms windows, count 4096, seed 123135. Boxed float improves 3.469-4.030% and
read-only float 1.932-2.210% with separated baseline ranges. Boxed-int baseline
ranges overlap. Member updates are 1.958-3.035% slower with separated ranges;
compile-off is also 1.945% slower. A noisy 50.8-ns boxed-float baseline fork is
retained. This candidate is superseded, not the qualified source below.

|!Workload|!Before|!Enabled A / B|!Reduction A / B|!Runtime disabled|!Compile disabled
|boxed integer method|22.6386|22.3757 / 22.4921|1.161% / 0.647%|23.6810|23.2203
|boxed float method|29.4036|28.2187 / 28.3837|4.030% / 3.469%|30.2249|29.7579
|member updates|89.5627|91.3167 / 92.2813|-1.958% / -3.035%|92.7384|91.3046
|read-only integer member|36.8341|36.8842 / 36.8634|-0.136% / -0.079%|38.0687|37.6161
|read-only float member|41.4019|40.6022 / 40.4870|1.932% / 2.210%|42.2878|41.6893
|native integer result|10.2155|10.0667 / 9.9944|1.456% / 2.164%|10.0513|10.0191
|native float result|11.6178|11.3826 / 11.6156|2.024% / 0.018%|11.5916|11.5009
|list build|83.0970|81.8821 / 82.3082|1.462% / 0.949%|81.9341|81.7200
|list mutation|227.8507|228.4579 / 225.8217|-0.267% / 0.890%|227.4906|224.8103

### Inlined live-slot operation

675 windows with the same settings and seed 124136. The live-slot operation is
forced inline, avoiding a new outlined call and duplicated legacy input checks.
Boxed float improves 3.782-4.017% and read-only float 2.194-2.538%, separating
baseline/runtime-off/compile-off. Boxed int improves 2.095-2.960%, but one
baseline comparison overlaps and A/A separates. Member updates remain
0.606-2.221% slower, with baseline overlap and A/A separation; compile-off is
1.841% slower with separated baseline ranges. Native/list controls are mixed.
This candidate is also superseded; no adverse result is removed.

|!Workload|!Before|!Enabled A / B|!Reduction A / B|!Runtime disabled|!Compile disabled
|boxed integer method|22.8659|22.1892 / 22.3867|2.960% / 2.095%|24.1047|23.6824
|boxed float method|29.4786|28.3638 / 28.2945|3.782% / 4.017%|31.3355|30.8671
|member updates|91.9969|94.0397 / 92.5542|-2.221% / -0.606%|94.4929|93.6901
|read-only integer member|37.5693|36.9085 / 36.9008|1.759% / 1.779%|39.4953|39.1385
|read-only float member|41.9508|41.0304 / 40.8861|2.194% / 2.538%|43.5133|43.5519
|native integer result|10.4424|10.2520 / 10.2159|1.823% / 2.169%|10.1855|10.2909
|native float result|11.9645|11.2284 / 11.4927|6.152% / 3.943%|11.5880|11.4324
|list build|86.1122|85.2397 / 84.8356|1.013% / 1.482%|84.7331|84.0743
|list mutation|235.5941|236.4484 / 231.5896|-0.363% / 1.700%|231.7947|235.5114

### Final candidate with one configuration guard

675 windows with the same settings and seed 125137. One immutable v2 mode now
combines the declaration choice with shared scalar-store configuration. ARM64
disassembly confirms one hot initialization acquire guard rather than two and
no outlined live-slot operation. This is code-generation evidence, not a causal
explanation for every workload difference.

Boxed int improves 3.915-3.989% and boxed float 6.027-6.094%; both enabled arms
separate baseline and both disabled controls, while A/A overlaps. Read-only
float improves 1.582-1.954% with separated controls. Read-only integer B overlaps
baseline/compile-off. Member updates are mixed: A is 0.632% slower with separated
baseline ranges, B is 0.433% faster with overlap, and A/A separates. Native gains
over baseline overlap off controls and are not assigned to declaration
preparation. List results are mixed.

|!Workload|!Before|!Enabled A / B|!Reduction A / B|!Runtime disabled|!Compile disabled
|boxed integer method|22.7270|21.8204 / 21.8372|3.989% / 3.915%|23.6556|22.8948
|boxed float method|29.5472|27.7663 / 27.7466|6.027% / 6.094%|30.2504|29.7721
|member updates|91.0214|91.5965 / 90.6276|-0.632% / 0.433%|92.7038|91.6069
|read-only integer member|37.0853|36.4091 / 36.7228|1.824% / 0.977%|38.5952|37.9157
|read-only float member|41.5097|40.8532 / 40.6988|1.582% / 1.954%|43.2496|42.6899
|native integer result|10.2934|10.0405 / 10.0870|2.457% / 2.006%|10.0298|10.0350
|native float result|11.7371|11.5269 / 11.4011|1.791% / 2.863%|11.5230|11.5280
|list build|83.8735|83.9895 / 82.9904|-0.138% / 1.053%|82.8725|83.4320
|list mutation|228.7792|227.2429 / 229.1727|0.672% / -0.172%|228.6871|229.8663

### Longer confirmation

1000 windows: eight kernels, five modes, five forks, three warmups and five
750-ms windows, count 4096, seed 126138. Boxed int improves 3.672-4.347%, with
baseline overlap and separated disabled controls. Boxed float improves
4.831-5.229%, separating baseline and both controls. Read-only integer and float
medians improve, but baseline ranges overlap. Member updates are mixed with
overlapping controls. Native float is 2.141-3.014% slower and list mutation
0.073-0.843% slower, with overlapping ranges. All A/A ranges overlap. These
adverse observations remain, even where a later run differs.

|!Workload|!Before|!Enabled A / B|!Reduction A / B|!Runtime disabled|!Compile disabled
|boxed integer method|23.4180|22.3999 / 22.5580|4.347% / 3.672%|24.2421|23.7602
|boxed float method|30.2336|28.6527 / 28.7731|5.229% / 4.831%|31.1717|30.4660
|member updates|92.4922|92.1826 / 92.6633|0.335% / -0.185%|93.3050|92.8489
|read-only integer member|38.1499|37.8037 / 37.7590|0.907% / 1.024%|39.3637|38.8932
|read-only float member|42.8225|42.3831 / 41.9058|1.026% / 2.141%|44.4879|43.9210
|native float result|11.7039|11.9545 / 12.0566|-2.141% / -3.014%|11.7237|11.9826
|list build|83.9879|83.9136 / 83.1427|0.088% / 1.006%|83.4535|83.1918
|list mutation|230.7250|232.6710 / 230.8933|-0.843% / -0.073%|232.6980|230.6190

### Short invocations

600 windows: the same eight kernels/five modes, count 1, three forks, three
warmups and five 500-ms windows, seed 127139. Every enabled-versus-baseline
range overlaps; median reductions range from -1.109% to +0.504%. There is no
demonstrated short-invocation benefit or equivalence. List-build A/A ranges
separate. List-mutation B is 0.539% slower than compile-off with separated
ranges. These are whole short invocations, not isolated entry costs. No Java
timing is taken from this matrix.

|!Workload|!Before|!Enabled A / B|!Reduction A / B|!Runtime disabled|!Compile disabled
|boxed integer method|2349.2260|2362.8649 / 2375.2766|-0.581% / -1.109%|2358.5666|2385.4337
|boxed float method|2338.8864|2355.9658 / 2355.7395|-0.730% / -0.721%|2383.2501|2358.2393
|member updates|3315.4127|3298.6999 / 3314.5129|0.504% / 0.027%|3322.2809|3311.0778
|read-only integer member|2381.7854|2384.9242 / 2395.7303|-0.132% / -0.585%|2403.9834|2390.1707
|read-only float member|2408.8145|2429.5820 / 2428.2835|-0.862% / -0.808%|2409.7005|2443.2485
|native float result|2324.7542|2333.6278 / 2324.1414|-0.382% / 0.026%|2330.2989|2336.7264
|list build|2683.7475|2693.9884 / 2675.1959|-0.382% / 0.319%|2692.0809|2675.2776
|list mutation|3063.3125|3077.1410 / 3073.1913|-0.451% / -0.322%|3078.3519|3056.7299

### Fresh matched Java comparison

825 windows: eleven kernels, baseline, enabled A/A, same-binary runtime-off and
Java JIT, three forks, three warmups and five 500-ms windows, count 4096, seed
128140. Temurin 25.0.2/JMH 1.37 generated target dontinline hints are checked;
all modes pass 555 references and Qore library selection is verified. This is
not a comparison with freely inlined Java or equivalent runtime contracts.

|!Workload|!Before|!Enabled A / B|!Reduction A / B|!Runtime disabled|!Java JIT|!Enabled / Java
|boxed integer method|24.1726|22.7810 / 23.0812|5.757% / 4.515%|24.7268|2.6769|8.51 / 8.62x
|boxed float method|30.8080|29.1086 / 29.2264|5.516% / 5.134%|31.9583|2.6590|10.95 / 10.99x
|member updates|95.5266|95.4253 / 95.9939|0.106% / -0.489%|96.8382|1.9236|49.61 / 49.90x
|read-only integer member|38.9441|39.1586 / 38.9957|-0.551% / -0.133%|40.5873|2.6702|14.67 / 14.60x
|read-only float member|44.1623|43.2180 / 42.9583|2.138% / 2.726%|45.1103|2.6338|16.41 / 16.31x
|native integer result|10.6901|10.6436 / 10.5789|0.435% / 1.040%|10.4913|3.2018|3.32 / 3.30x
|native float result|12.1090|12.0704 / 12.1599|0.319% / -0.420%|12.1279|3.6302|3.32 / 3.35x
|indirect integer result|7.1756|7.2115 / 7.1913|-0.499% / -0.219%|7.1660|1.9536|3.69 / 3.68x
|indirect float result|9.1406|9.2378 / 9.1353|-1.063% / 0.059%|9.2005|2.8987|3.19 / 3.15x
|list build|89.3348|87.7347 / 88.6416|1.791% / 0.776%|89.6179|2.6995|32.50 / 32.84x
|list mutation|242.8602|244.4069 / 244.6832|-0.637% / -0.751%|241.9948|4.2476|57.54 / 57.61x

Both boxed-integer arms improve 4.515-5.757% and boxed-float arms 5.134-5.516%,
with separated baseline and runtime-off ranges. Boxed-int A/A also separates
by 1.317%; retain both arms. Read-only integer is 0.133-0.551% slower and float
2.138-2.726% faster, with baseline overlap; both off comparisons separate.
Member updates are mixed with baseline overlap and A/A separation. Native
integer improves 0.435-1.040%, native float is mixed, and their ranges overlap.

Indirect integer is 0.219-0.499% slower with overlap. Indirect-float A is 1.063%
slower with separated baseline ranges; B is 0.059% faster with overlap, and A/A
separates. List build improves 0.776-1.791% with overlap. List mutation is
0.637-0.751% slower: A separates baseline and runtime-off, B overlaps both.
These results do not erase the earlier native-float/member-update adverse
observations or establish a regression-free result.

Only this matched matrix supplies the new Java ratios: native/indirect workloads
remain about 3.2-3.7x Java, boxed integer 8.5-8.6x, boxed float 11x, read-only
members 14.6-16.4x, updates 49.6-49.9x, list build 32.5-32.8x and mutation 57.5-57.6x.
Java parity remains unachieved.
Verification: 021ee09ee8106a21489401731b29050cf444a5ab835fe753ffc7f5aeb9c6e270.

### Qualification, profiles and retention

Release: 114 files, 417 cases / 36057 assertions, 15 standalones, nine skips.
Debug: 114 files, 424 cases / 36156 assertions, 15 standalones, eight skips.
Both pass seven exception/stack fixtures and IR/AOT grids; the restored grid
also passes 555 references per Java-interpreted/Java-JIT mode. Focused native
checks pass 642/642, the harness passes 17 tests, and eight restored-release
smokes pass. All 57 owned release qmods are restored and unrelated aliases are
unchanged. Linux-only RSS and release debug-only checks are explicit skips;
the native wrapper skips release and runs in debug. This is selected coverage,
not the full repository, sanitizer, leak or cross-platform qualification.

The changed wrappers contain 557 auto-slot, 153 scalar-store and 2810 reload
assertions. Native matrices directly force looked-up, storage-prepared and
declaration-prepared scalar operations under both store modes, including live
guard misses, exact bits, ownership, recursion, block growth and declaration
rejection. Normal/EH, O0/O3, split, no-debug and independent compiler/runtime
controls remain covered. Five focused debug configurations and four additional
combined-control configurations each pass 642 native checks. Existing incomplete
source-specialized block reflection maps remain documented, not repaired here.

One preserved setup failure preceded the inline candidate's restored-module
check: its harness incorrectly required all old aliases to exist, although
runtime-identity regeneration had removed 54 stale owned aliases. It stopped
before building or mutating links. The corrected preflight permits missing
owned generated aliases but rejects foreign files/targets, then restores and
verifies the 57-module dependency closure. The failure is retained in evidence.

Eleven serial diagnostic profiles confirm v2 stores execute. Enabled boxed
integer/float samples show prepared reload families at 19.98/15.51%, declaration-
prepared stores at 6.85/7.81%, and typed dispatch at 36.00/29.46%. Native float's
typed-dispatch family is 67.52%; member updates retain generic reload work.
List mutation retains generic assignment, coercion, lvalue cleanup and ownership
in before/enabled/compile-off profiles. Only complete main-thread trees are
counted; these inclusive families overlap and must not be added. Instrumented
times are excluded. Profile similarity does not explain away measured adverse
differences or prove their cause.

Retention: keep the final candidate's repeated boxed-call gains after selected
qualification and the complete 62-check audit, retaining all 4450 windows,
superseded candidates, short-invocation results and adverse controls. No universal
speedup or Java parity is claimed. Next investigate explicit pure-call completion
proofs only together with non-reentrant caller cleanup, then remaining live
dispatch/frame costs. Container assignment/coercion is a separate execution
priority; successful native dispatch alone does not authorize removing reloads.

[Verified samples, qualification and audit](results-aot-auto-declaration-macos-arm64.json).

## Exact live scalar reuse in AOT reloads (2026-10-13)

A stale prepared AOT local reload now reuses an exact immediate int/float/bool
cache only after comparing it with the assigned, nonfinalized, noncleared live
slot and positively admitting both cleanup owners as NOTHING or immediate
scalars. Float equality is bitwise. No allocation, reference counting or reentry
occurs on a hit. Tracker rotation and epoch-last publication remain unchanged.
Mismatch, nodes/references, wide numbers, opaque owners and populated sinks
retain the complete v1 evaluation and cleanup. No invalidation, reflection,
logical frame or cancellation operation is removed.

QORE_DISABLE_AOT_SCALAR_LOCAL_RELOAD selects v1 at compilation. Its presence
at first stale v2 use delegates to v1 in the same executable; runtime-off includes
that wrapper cost. Existing acquisition/store/epoch controls remain independent.
The additive v2 symbol and private-header runtime identity require matching
compiler/runtime artifacts. No IR/JIT speedup is claimed.

Baseline: merged revision 1adbb1b7ccf1e134aa16d31f33d979ea99e6e1e8.
Candidate identity: 1b3d703ed5aa043acc3e8e8ef9b5f93dd1a9d22834490c5c1664ea31cf5b667e.
All four matrices use matching frozen compiler/runtime pairs, identical enabled
A/A artifacts, ordered environments and argv, equal-length aliases/control
variables and shuffled serial jobs. All 3300 windows are retained. No owned
build/test/profile work overlaps timing. Values are medians of process means
in whole-workload ns/element, not isolated call latency. Positive reduction
means less time. Range overlap is not equivalence; separation is not a confidence
interval or causal explanation. This is a targeted gain with adverse controls.

### Initial measurement

675 windows: nine kernels, five modes, three forks, three warmups and five
500-ms windows, count 4096, seed 129149. All five targeted workloads have both
enabled arms separated from baseline and both disabled controls. Boxed integer
improves 13.057-13.366%, float 11.203-11.752%, updates 4.635-4.879%, read-only
integer 8.777-9.043%, and read-only float 8.056-8.925%.

Native integer is 5.399-5.650% slower with separated baseline ranges; compile-off
is also 4.257% slower. List build is 1.386-1.737% slower, separates baseline and
has separated A/A ranges. Native float improves 2.451-2.452% with separated
baseline ranges but overlaps disabled controls. List mutation improves
1.042-1.280%; A separates baseline and B overlaps. All windows, including the
runtime-off read-integer 58.68-ns fork and compile-off list-mutation 239.95-ns
fork, remain. Neither adverse result is explained away.

|!Workload|!Before|!Enabled A / B|!Reduction A / B|!Runtime disabled|!Compile disabled
|boxed integer method|21.7325|18.8949 / 18.8277|13.057% / 13.366%|21.9358|21.5636
|boxed float method|27.6250|24.3784 / 24.5302|11.752% / 11.203%|28.0640|27.8093
|member updates|91.0815|86.8603 / 86.6372|4.635% / 4.879%|90.0322|89.5914
|read-only integer member|36.6015|33.2917 / 33.3891|9.043% / 8.777%|36.6022|36.2238
|read-only float member|40.6083|37.3371 / 36.9841|8.056% / 8.925%|40.8614|40.3744
|native integer result|10.0320|10.5988 / 10.5736|-5.650% / -5.399%|10.4902|10.4591
|native float result|11.6583|11.3725 / 11.3724|2.451% / 2.452%|11.5340|11.4421
|list build|82.9774|84.4185 / 84.1273|-1.737% / -1.386%|83.6594|83.9611
|list mutation|229.8200|226.8790 / 227.4258|1.280% / 1.042%|227.3636|229.3263

### Five-fork confirmation

1125 windows: the same nine kernels/five modes and window settings, five forks,
count 4096, seed 130151. All five targets again separate baseline and both
disabled controls: boxed integer 13.343-13.815%, float 10.650-10.923%, updates
3.436-3.792%, read-only integer 8.740-8.757%, read-only float 8.593-9.140%.

Native integer remains 3.198-3.236% slower with separated baseline ranges;
compile-off is 4.768% slower too. List build is 0.691-2.088% slower with baseline
overlap; compile-off is 1.901% slower with separation. Native float improves
1.896-2.247%, A overlaps baseline and B separates; disabled comparisons overlap.
List mutation improves 0.026-0.928% with overlap. All A/A ranges overlap.

|!Workload|!Before|!Enabled A / B|!Reduction A / B|!Runtime disabled|!Compile disabled
|boxed integer method|21.6389|18.6495 / 18.7517|13.815% / 13.343%|21.7261|21.6500
|boxed float method|27.7214|24.6933 / 24.7691|10.923% / 10.650%|28.0025|27.8484
|member updates|90.0139|86.6002 / 86.9207|3.792% / 3.436%|90.2759|89.8296
|read-only integer member|36.5180|33.3263 / 33.3201|8.740% / 8.757%|36.3388|35.9163
|read-only float member|40.7493|37.2479 / 37.0249|8.593% / 9.140%|40.6151|39.9138
|native integer result|10.0822|10.4047 / 10.4084|-3.198% / -3.236%|10.5574|10.5629
|native float result|11.6544|11.4335 / 11.3926|1.896% / 2.247%|11.2962|11.4654
|list build|82.7079|83.2795 / 84.4350|-0.691% / -2.088%|83.3435|84.2799
|list mutation|228.7806|226.6576 / 228.7207|0.928% / 0.026%|227.1628|229.2586

### Short invocations

675 windows: nine kernels, five modes, three forks, three warmups and five
500-ms windows, count 1, seed 131153. No consistent short-invocation benefit:
enabled/baseline reductions range from -1.467% to +1.033%, mostly overlapping.
Read-only integer A is 0.720% slower and separates baseline; B is 0.217% slower
with overlap. Boxed-float B is 0.858% slower than compile-off with separation.
All A/A ranges overlap. These invocations include the benchmark's 20-argument
dispatch, selection, seed/checksum/call-count updates and clock query; their
microsecond totals cannot be assigned to call entry alone. No Java denominator
is taken from another run.

|!Workload|!Before|!Enabled A / B|!Reduction A / B|!Runtime disabled|!Compile disabled
|boxed integer method|2231.1352|2210.7839 / 2263.8623|0.912% / -1.467%|2265.3205|2197.1411
|boxed float method|2256.6475|2233.3285 / 2241.1351|1.033% / 0.687%|2236.4297|2222.0714
|member updates|3138.9713|3124.7537 / 3160.4614|0.453% / -0.685%|3097.3403|3150.5747
|read-only integer member|2263.7016|2279.9905 / 2268.6031|-0.720% / -0.217%|2268.1654|2271.0539
|read-only float member|2312.3096|2308.4178 / 2322.2293|0.168% / -0.429%|2292.9692|2298.0142
|native integer result|2184.3277|2203.1259 / 2194.8026|-0.861% / -0.480%|2219.6063|2174.1246
|native float result|2229.2697|2206.5193 / 2206.5302|1.021% / 1.020%|2219.3807|2204.8783
|list build|2549.7242|2580.1075 / 2570.5574|-1.192% / -0.817%|2559.3222|2562.3715
|list mutation|2909.1691|2939.2459 / 2916.4537|-1.034% / -0.250%|2904.7233|2932.1055

### Fresh matched Java comparison

825 windows: eleven kernels, baseline, enabled A/A, same-binary runtime-off and
Java JIT, three forks, three warmups and five 500-ms windows, count 4096, seed
132154. Temurin 25.0.2/JMH 1.37 generated target dontinline hints are checked.
Every mode passes 555 references and Qore library selection is verified. This
is not a comparison with freely inlined Java or equivalent runtime contracts.

|!Workload|!Before|!Enabled A / B|!Reduction A / B|!Runtime disabled|!Java JIT|!Enabled / Java
|boxed integer method|21.9320|18.6958 / 18.9141|14.755% / 13.760%|21.7051|2.3472|7.96 / 8.06x
|boxed float method|27.5295|24.4290 / 24.6450|11.262% / 10.478%|27.8936|2.5444|9.60 / 9.69x
|member updates|91.0391|86.5626 / 86.5465|4.917% / 4.935%|91.0742|1.8657|46.40 / 46.39x
|read-only integer member|36.6539|33.3193 / 33.1826|9.098% / 9.471%|36.3806|2.5279|13.18 / 13.13x
|read-only float member|41.0718|37.3142 / 37.1870|9.149% / 9.459%|40.7053|2.5204|14.80 / 14.75x
|native integer result|10.0650|10.5159 / 10.4572|-4.479% / -3.897%|10.4528|3.0497|3.45 / 3.43x
|native float result|11.5955|11.3322 / 11.2148|2.270% / 3.283%|11.5981|3.4674|3.27 / 3.23x
|indirect integer result|6.8815|6.9154 / 6.8752|-0.493% / 0.092%|6.9171|1.8739|3.69 / 3.67x
|indirect float result|8.7904|8.8082 / 8.7643|-0.202% / 0.296%|8.7600|2.7968|3.15 / 3.13x
|list build|83.2862|83.6030 / 83.6347|-0.380% / -0.418%|83.5809|2.5219|33.15 / 33.16x
|list mutation|227.1223|228.0075 / 228.1862|-0.390% / -0.468%|227.9764|4.0661|56.07 / 56.12x

Boxed integer improves 13.760-14.755%, boxed float 10.478-11.262%, and updates
4.917-4.935%, with both enabled arms separating baseline and runtime-off.
Read-only integer improves 9.098-9.471% with separated controls; its A/A ranges
also separate by 0.410%. Read-only float improves 9.149-9.459%: A overlaps
baseline/runtime-off and B separates both. All A/A results remain.

Native integer is 3.897-4.479% slower with separated baseline ranges; both arms
overlap runtime-off. Native float improves 2.270-3.283% and separates baseline
but overlaps runtime-off. Indirect integer and float are mixed within 0.5% and
overlap baseline. List build is 0.380-0.418% slower, and mutation 0.390-0.468%
slower, with overlapping baseline/off ranges. This does not erase earlier
list-build or short-invocation adverse comparisons.

Only this matched matrix supplies the new Java ratios: native integer
3.43-3.45x, native float 3.23-3.27x, indirect integer 3.67-3.69x, indirect float
3.13-3.15x, boxed integer 7.96-8.06x, boxed float 9.60-9.69x, read-only integer
13.13-13.18x, read-only float 14.75-14.80x, updates 46.39-46.40x, list build
33.15-33.16x and mutation 56.07-56.12x. Java parity remains unachieved.
Verification: 113aacfec17b8a4f5230b0a6217bfc8ff30526c4b449c5f8fa5be91a9e5975ca.

The tool response containing the timing driver's handle was truncated. The
specific process was observed live repeatedly, then absent with a complete
archive. Its outer exit status is unobserved, not assumed zero. Independent
verification replays every recorded successful child exit, all 825 windows,
references, identities, environments, Java settings and summary calculations.
The terminal-observation limitation is retained in the evidence.

### Qualification, profiles and retention

Release: 114 files, 417 cases / 36695 assertions, 15 standalones, nine skips.
Debug: 114 files, 424 cases / 36794 assertions, 15 standalones, eight skips.
Both pass seven exception/stack fixtures and IR/AOT grids; the restored grid
also passes 555 references per Java-interpreted/Java-JIT mode. Focused native
checks pass 795/795, the harness passes 17 tests, and eight restored-release
smokes pass. All 61 owned release qmods are restored; unrelated aliases remain
unchanged. Linux-only RSS and release debug-only checks are explicit skips.
This is selected coverage, not full repository/sanitizer/leak/cross-platform
qualification.

The changed reload wrapper now has 3448 assertions. Native tests force exact
scalar reuse independently of the diagnostic switch and test bit preservation,
tracker rotation, replacement, signed zero, cleared/finalized/unassigned state,
owning/opaque cleanup, wide integers and escaped doubles. Generic/v1/v2 paths
test reentrant and throwing deferred destruction. Seven focused debug control
configurations each pass 795 checks. O0/O3, normal/EH, split, no-debug and
independent compiler/runtime controls remain covered. Existing incomplete
source-specialized block reflection maps remain documented, not repaired here.

Eleven serial diagnostic profiles show native integer typed dispatch at
69-71% of main-thread samples and stack/context around 13%. Enabled boxed
integer/float scalar reloads account for 13.83/10.62%, declaration stores
8.21/8.47%, and typed dispatch 38.65/31.82%. List build retains generic
assignment, coercion, lvalue cleanup and ownership. Complete main-thread trees
are used; inclusive family percentages overlap and must not be added.
Instrumented times are excluded. Profile similarity cannot dismiss regressions.

The native integer/float private caller entries have identical disassembled
instruction/address text across old/new/compile-off executables. Their shared
runtime helpers differ despite unchanged helper sources: integer 477 to 468
instructions, float 444 to 455, with changed configuration-test lowering and
code/data placement. Neither inspection establishes a cause or justifies
discarding the persistent integer slowdown. No padding/alignment workaround
is introduced.

Retention: keep this qualified targeted AOT improvement for repeated 3-15%
gains in five reload-heavy workloads, with the persistent 3-6% native-integer
regression explicit. All 3300 windows, disabled controls, short-invocation
results and adverse observations remain. This is not a universal speedup,
regression-free result, or Java-parity claim. Next measure immutable dispatch
policy preparation separately, preserving live checks and generic-v1 admission.
Full pure-call cleanup proofs and container assignment/coercion remain further
priorities; a successful native dispatch does not prove cleanup is non-reentrant.

[Verified samples, qualification and audit](results-aot-scalar-local-reload-macos-arm64.json).


## AOT integer-first inline-float cleanup

AOT-first follow-up to `5593a0db7`, measured against merged baseline
`ea36807378d0ab75c695a4689474ef9374239e9d` on macOS arm64. The late LLVM
cleanup pass now recognizes inline floating-point values in its existing
native-callback/method callers, keeping the int48 tag exit first. The new test
is exactly the union of NOTHING/int48/bool and `QoreValue::isFloat()`.
Short-string and opaque-reference families, heap numbers and plugin tags retain
the helper. Cleanup stays at its original point; no ownership, caller epoch,
context, cancellation, dispatch or frame contract is weakened. The pass is
called by AOT lowering only; no IR/JIT improvement is claimed.

`QORE_DISABLE_LLVM_FLOAT_DECREF_GUARD=1` during compilation selects the old
predicate on the candidate runtime. It is independent of the all-scalar and
method-selection controls and adds no runtime flag test. Presence, including
`0`, disables the float extension. Old AOT artifacts and the runtime ABI are
unchanged. The `scalar-cleanup-float-guards` diagnostic counts expanded guards,
not observed float executions.

### Screening, rejected predecessor and confirmation

The predecessor combined predicate improved boxed floats by about 11% but
regressed integer member workloads by 3-5% in 660 windows. Its frozen sources,
artifacts and raw data remain separate; it was not committed. Restoring an
early integer exit produced the current candidate. Actual integer-member
caller disassembly has 24 integer-first sequences, versus zero in baseline and
compile-disabled artifacts. This establishes the lowering, not timing causality.

Current-candidate initial screening retained 660 windows (three forks), followed
by 1100 confirmation windows (five forks). Both use eleven kernels, before,
identical enabled A/A and separately compiled old-predicate controls, count
4096, three warmups and five 500-ms windows. The initial matrix improved boxed
floats 10.4-11.0%, float member reads 8.6-9.3%, and indirect integers 3.0-3.6%,
with separated before/disabled fork ranges. Confirmation follows in ns/element:

|!Workload|!Before|!Enabled A / B|!Reduction A / B|!Compile disabled
|boxed integer method|18.7301|18.8562 / 18.9769|-0.674% / -1.318%|18.8678
|boxed float method|24.5062|21.8642 / 21.8855|10.781% / 10.694%|24.3845
|member updates|86.5733|87.1130 / 86.4017|-0.623% / 0.198%|86.2718
|read-only integer member|32.9340|32.7813 / 32.8285|0.464% / 0.320%|32.7256
|read-only float member|37.0865|33.7784 / 33.8270|8.920% / 8.789%|37.2659
|native integer result|10.3021|10.1370 / 10.3060|1.602% / -0.038%|10.1594
|native float result|11.3607|11.1883 / 11.3570|1.518% / 0.033%|11.3963
|indirect integer result|6.8085|6.6464 / 6.6136|2.381% / 2.863%|6.9651
|indirect float result|8.7491|8.6690 / 8.7701|0.916% / -0.240%|8.7887
|list build|83.4612|83.0129 / 82.6324|0.537% / 0.993%|83.6191
|list mutation|229.2144|231.4225 / 229.1306|-0.963% / 0.037%|228.8866

Confirmation repeats the three main improvements, with separated before/off
ranges in both enabled arms. It does not prove universal improvement: boxed
integer medians are 0.7-1.3% slower with overlapping ranges. Integer member
updates are mixed, with overlapping ranges. The initial native-integer
1.3-1.5% slowdown and list-mutation 1.5-2.2% slowdown are not consistent in the
confirmation; all original samples remain. This does not resolve the older
committed native-integer regression. Confirmation includes a 10.09-ns
indirect-float fork mean without exclusion. Fork-range separation is not a
confidence interval, and overlap is not equivalence.

### Short invocations and artifact cost

660 further windows use count 1, eleven kernels, the same four modes and three
forks. Values below are ns per whole invocation, including outer-call/harness
work; they are not isolated call-entry latency. No Java timing in this matrix.

|!Workload|!Before|!Enabled A / B|!Reduction A / B|!Compile disabled
|boxed integer method|2294.1634|2312.4205 / 2309.3729|-0.796% / -0.663%|2321.8989
|boxed float method|2282.0750|2307.3819 / 2281.1877|-1.109% / 0.039%|2294.8521
|member updates|3246.1704|3254.2223 / 3252.2729|-0.248% / -0.188%|3252.7681
|read-only integer member|2316.1854|2327.9138 / 2314.6898|-0.506% / 0.065%|2327.9326
|read-only float member|2270.6608|2252.1967 / 2239.2880|0.813% / 1.382%|2264.5474
|native integer result|2260.9285|2272.6866 / 2254.5807|-0.520% / 0.281%|2255.6704
|native float result|2264.8974|2265.6774 / 2275.7101|-0.034% / -0.477%|2261.8699
|indirect integer result|2144.7613|2128.0459 / 2104.9981|0.779% / 1.854%|2109.3459
|indirect float result|2113.8621|2105.2491 / 2130.0719|0.407% / -0.767%|2111.2029
|list build|2613.7612|2648.9709 / 2658.0600|-1.347% / -1.695%|2610.2241
|list mutation|3013.1369|3016.4978 / 3017.1725|-0.112% / -0.134%|2985.4836

Every before/enabled fork-range pair overlaps; no broad short-invocation
benefit or equivalence is established. List build medians are 1.35-1.69% slower
than baseline and 1.48-1.83% slower than compile-off; enabled-B separates the
off range. Indirect-float enabled-B is 0.89% slower than off with separated
ranges. List mutation is about 0.1% slower than baseline. These adverse results
remain part of retention, not discarded as setup noise.

The short benchmark executable grows from 394938 to 405626 bytes (2.71%);
the fresh-Java matrix grows from 394938 to 405610 bytes (2.70%). Compile-off is
394938 bytes in both. These are whole-file sizes, including non-code sections,
not isolated text size. Single observed compilation durations are approximately
0.74/0.53/0.74 seconds (before/enabled/off) and 0.75/0.54/0.76 seconds in the
Java matrix. These are recorded costs, not a controlled compiler-throughput
claim or proof that the added pass work is free.

### Fresh matched Java comparison

825 windows: eleven kernels, five interleaved modes, three forks, three warmups
and five 500-ms windows, count 4096, order seed 135179. Temurin 25.0.2 and pinned
JMH 1.37 use verified generated target DONT_INLINE hints. All five modes pass
555 references; Qore library selection, Java tools/classes/JARs, actual fork
settings and every raw window are independently checked. There is no mixing
with historical Java times. Values are whole-workload ns/element, not isolated
call latency or equivalent ownership/synchronization contracts.

|!Workload|!Before|!Enabled A / B|!Reduction A / B|!Compile disabled|!Java JIT|!Enabled / Java
|boxed integer method|18.7294|18.9290 / 18.8619|-1.066% / -0.707%|18.9148|2.5209|7.51 / 7.48x
|boxed float method|24.6736|21.7822 / 22.1013|11.719% / 10.425%|24.5684|2.5394|8.58 / 8.70x
|member updates|85.9132|87.4657 / 87.1920|-1.807% / -1.488%|87.3850|1.8513|47.25 / 47.10x
|read-only integer member|32.8933|32.6975 / 32.5953|0.595% / 0.906%|32.8288|2.5341|12.90 / 12.86x
|read-only float member|37.3161|33.7610 / 33.8704|9.527% / 9.234%|37.1843|2.4930|13.54 / 13.59x
|native integer result|10.1773|10.1761 / 10.2007|0.011% / -0.230%|10.2607|3.0532|3.33 / 3.34x
|native float result|11.2989|11.3811 / 11.3497|-0.727% / -0.450%|11.4278|3.4658|3.28 / 3.27x
|indirect integer result|6.8178|6.5797 / 6.6564|3.491% / 2.367%|6.8228|1.8742|3.51 / 3.55x
|indirect float result|8.7312|8.6581 / 8.6644|0.837% / 0.766%|8.7921|2.6374|3.28 / 3.29x
|list build|83.4682|84.5082 / 82.6898|-1.246% / 0.933%|83.9871|2.5089|33.68 / 32.96x
|list mutation|230.2578|232.0820 / 229.9972|-0.792% / 0.113%|231.1607|4.0748|56.95 / 56.44x

Boxed floats improve 10.4-11.7%, read-only float members 9.2-9.5%, and indirect
integers 2.4-3.5%; both enabled arms separate before and compile-off ranges.
Boxed-float A/A medians differ 1.46% with separated ranges; both arms remain in
the report. Indirect floats improve 0.77-0.84% with separated ranges in this
matrix, but earlier matrices were mixed, so no repeatable broad float-callback
gain is asserted.

Integer member updates regress 1.49-1.81% against baseline with separated
ranges; enabled/off ranges overlap. The compile-disabled observation does not
establish a cause or dismiss the regression. Boxed integers are 0.7-1.1% slower
with overlapping baseline ranges. Native integer/float results and list
controls are mixed or slightly adverse; list-build A/A differs 2.15% with
overlapping ranges. All data and comparisons remain.

Only this matched matrix supplies the new Java ratios: native integer/float
about 3.3x, indirect integer 3.51-3.55x, indirect float 3.28-3.29x, boxed integer
7.48-7.51x, boxed float 8.58-8.70x, read-only integer 12.86-12.90x, read-only
float 13.54-13.59x, updates 47.10-47.25x, list build 32.96-33.68x and mutation
56.44-56.95x. Java parity remains unachieved.

### Qualification, review and retention

Release passes 114 files, 417 cases / 36725 assertions, 15 standalones and nine
explicit skips. Debug passes 114 files, 424 cases / 36824 assertions, 15
standalones and eight skips. Both pass seven exception/stack fixtures and
IR/AOT reference grids. All 795 focused debug native checks and 17 harness
tests pass; restored IR/AOT/Java-interpreted/Java-JIT grids each pass 555
references. Nine restored-release smokes pass. All 61 owned qmod aliases point
to the matching release build; unrelated aliases are unchanged. Independent
verification reconstructs all 262 qualification child records. This is selected
coverage, not full repository, sanitizer, leak or cross-platform qualification;
Linux-only RSS and release debug-hook skips remain explicit.

The scalar-assignment fixture covers AST/IR/JIT/tiered, AOT O0/O3, normal/EH and
independent controls, including signed zero, infinities, NaNs, tag-colliding
heap doubles and sole opaque ownership/destruction. A separate actual-QoreValue
predicate check covers all 65536 upper-16-bit tags with nine payloads each
(589824 representations), not an exhaustive 64-bit sweep. Generated owning
fallbacks and ordinary cleanup/exception ordering remain intact.

An initial qualification-driver preflight compared a legacy list to a target
map and failed before any build/test/link change; it was corrected and retained.
The first independent verifier assumed generated Makefile2 byte stability.
Recorded builds reconfigured CMake; verification instead reconstructs the exact
unchanged 61-target dependency closure and target map, retaining pre/post graph
hashes and snapshots without claiming byte identity. A malformed unrun Java
driver was caught in review and corrected before compilation/timing. No runtime
failure or timing sample was replaced to hide these bookkeeping issues.

All 3245 current-candidate windows are independently replayed; the predecessor
660 windows remain separately archived. Performance data binds the unchanged
three implementation/test sources and frozen binaries. README/RESULTS and this
publication are documentation-only additions after qualification; the original
broad source manifest is not relabeled as their validation. The 62-item
audit-changes review records 17 Pass, 45 N/A and no remaining failures; strict
table checks are recorded separately. No new module, DataProvider, QPP, JNI or
public runtime ABI is introduced, and no optimization release note is added.

Retention: keep the repeated roughly 11% boxed-float and 9% float-member gains,
plus the repeated indirect-integer improvement, with integer regressions,
short-invocation costs and code growth explicit. This is not a universal or
regression-free improvement, nor evidence of Java parity. Next investigate
complete pure-call completion together with non-reentrant caller cleanup, and
invocation-prepared live dispatch/body proofs. Native success alone is not
purity; earlier generic effects, receiver/result destruction and deferred
cleanup must keep caller invalidation unless separately proven safe. Member
locking/storage and container coercion/ownership remain larger gaps.

Audit summary (the linked evidence reports each of the 62 items individually):

|!Check|!Status|!Finding
|Module docs and release notes|N/A|No new module; no optimization release note
|CMake and QMOD registration|N/A|No module registration change
|Module structure and QPP namespace|N/A|No module or QPP class change
|New-file copyright|Pass|2026 MIT evidence; existing fixtures retain their headers
|Test structure and dependencies|Pass|Modern, executable, local module path first; shipped dependencies only
|Sandboxed I/O|N/A|No new filesystem or network operation
|Cooperative cancellation|Pass|Existing 100/10 compiler checks and runtime checkpoints preserved
|Blocking operations|N/A|No new blocking runtime work
|DataProvider action registration|N/A|No action changes
|DataProvider types and allowed values|N/A|No data type changes
|App registration and descriptions|N/A|No app or description schema changes
|Factory and record-provider registration|N/A|No provider changes
|JNI dependencies and install rules|N/A|No JNI change; JMH is cached benchmark tooling
|Workarounds and stubs|Pass|Complete predicate; old-predicate control is exact
|Exception safety|Pass|LLVM ownership and owned-value cleanup remain intact
|Thread safety|Pass|Pass-local compiler state; runtime publication unchanged
|Type safety|Pass|Typed callbacks and fixed-width unsigned tag predicates
|Performance|Pass|Measured targeted gains with regressions and code growth explicit
|Error handling|Pass|Existing cancellation, exception and module-verification failures propagate
|Documentation|Pass|Compiler control, ownership contract, costs and limits documented
|QPP flags|N/A|No QPP method change
|Formats, bounds and credentials|Pass|Fixed formats; no unsafe buffers or published credentials
|Correctness|Pass|Predicate proof, selected release/debug tests and all raw measurements replayed

[Verified samples, qualification and all audit items](results-aot-inline-float-cleanup-macos-arm64.json).


## AOT scalar reference-resolution guard

Measured against `8bf4c74756f399b0bf107d7be68f1252ae085ec5` on macOS arm64.
An independent late AOT pass skips `qore_rt_deref_if_reference` when its value
is not a pointer. Its predicate exactly matches `QoreValue::isPointer()`,
including the entire opaque family. The helper is an identity operation for
all other representations, with no exception-sink or reference-count effects.
This is not a non-pointer cleanup exemption: enums and plugin representations
can still own payloads. Pointer/reference evaluation, release order, caller
invalidation, RuntimeConfig/TLS, frames, cancellation and runtime ABIs do not
change. No IR/JIT speedup is claimed.

The pass selects native scalar callback/unary-method callers, independently of
cleanup controls. It runs after the main optimizer and scalar-cleanup pass,
then uses local InstCombine/SimplifyCFG passes and verifies the module.
Constant non-pointer calls disappear; dynamic loads branch to the existing
helper only for ordinary/opaque pointers. `QORE_DISABLE_LLVM_SCALAR_REFERENCE_GUARD`
at compilation disables it, including a value of `0`. There is no runtime flag
check. `scalar-reference-guards` and `scalar-reference-removed` count compile-time
sites, not execution frequency.

A fresh pre-change diagnostic profile identified reference resolution at
4.84-8.20% of boxed-call inclusive samples. Those overlapping profile families
motivate the experiment; they are not causal speedup estimates. Actual timed
integer and float fast callers each retain two helper sites, now with ordinary
and opaque tag guards and result PHIs; before and compile-off have no such
guards. No ownership or caller-completion proof is inferred from native success.

### Initial screen and five-fork confirmation

The initial 660-window matrix improved boxed floats 3.36-3.60%, integer member
reads 2.76-2.93%, and float member reads 2.73-3.53% against baseline. Boxed integer
medians improved 1.52-3.77%, but enabled A includes a 41.56-ns fork mean and
its baseline range overlaps. Member updates improved 1.82-1.86%; compile-off
includes a 108.8-ns fork mean. Indirect integer B is 1.56% slower than compile-off
with separated ranges. Every original window remains.

Confirmation retains 1100 windows: eleven kernels, five forks per mode, before,
identical enabled A/A and separately compiled disabled controls, count 4096,
three warmups and five 500-ms windows. Values are whole-workload ns/element:

|!Workload|!Before|!Enabled A / B|!Reduction A / B|!Compile disabled
|boxed integer method|20.2061|19.1118 / 19.1871|5.416% / 5.043%|20.3993
|boxed float method|23.2145|22.2663 / 22.1821|4.084% / 4.447%|23.1016
|member updates|91.5857|90.8797 / 90.2846|0.771% / 1.421%|91.8080
|read-only integer member|34.3875|33.5594 / 33.6230|2.408% / 2.223%|34.3885
|read-only float member|35.3535|34.6096 / 34.2316|2.104% / 3.173%|35.6704
|native integer result|10.9319|10.8067 / 10.8640|1.145% / 0.621%|10.7447
|native float result|11.8291|11.7033 / 11.8309|1.064% / -0.015%|11.9311
|indirect integer result|7.0170|6.9824 / 6.9793|0.494% / 0.538%|6.9367
|indirect float result|9.1640|9.2276 / 9.1238|-0.694% / 0.439%|9.0758
|list build|88.6944|88.7879 / 87.9713|-0.105% / 0.815%|88.5983
|list mutation|243.5331|243.3809 / 243.2022|0.062% / 0.136%|246.0505

Boxed integer medians improve 5.04-5.42%, boxed floats 4.08-4.45%, integer
member reads 2.22-2.41%, and float reads 2.10-3.17%. Range separation is mixed:
both boxed-integer enabled arms separate compile-off, but only A separates
baseline. For boxed floats only B separates baseline; both overlap compile-off.
Native/indirect/list controls remain mixed. Indirect float A is 1.67% slower
than compile-off with separated ranges, including a retained 10.119-ns fork.
Fork ranges are not confidence intervals, and overlap is not equivalence.

### Pointer-heavy controls

A separate Qore-only supplement retains 360 windows across six workloads,
before/enabled A/A/compile-off, three forks, count 4096 and the same window
settings. All four AOT modes and AST pass 90 independent arithmetic/length
references. Values below are whole-workload ns/element, not helper latency:

|!Workload|!Before|!Enabled A / B|!Reduction A / B|!Compile disabled
|heap_string|152.5915|152.2061 / 153.9547|0.253% / -0.893%|155.1001
|short_string|152.4612|153.4605 / 153.4905|-0.655% / -0.675%|153.2699
|list|154.8396|153.6864 / 154.8443|0.745% / -0.003%|154.3784
|hash|162.4828|160.9020 / 161.6255|0.973% / 0.528%|162.3286
|opaque_list|229.9581|228.8129 / 229.6447|0.498% / 0.136%|231.0133
|raw_reference|174.8234|174.7230 / 175.0213|0.057% / -0.113%|175.9652

All baseline/enabled ranges overlap. Short strings are 0.66-0.68% slower than
baseline; heap-string B is 0.89% slower while A is 0.25% faster. These adverse
results remain; no equivalence or universal pointer benefit is asserted.
The opaque-local workload uses the dedicated weak loader and is untransformed,
so it does not measure the cost of passing opaque bits through this guard.
The raw-reference workload begins with an iterator reference, but runtime
reloads may evaluate it before the guarded helper; no claim is made that
NT_REFERENCE reaches every measured helper call. Ordinary-pointer fallback
and actual generated guards are separately inspected.

### Short invocations and code cost

660 windows use count 1, three forks and the same four Qore modes. These are
ns per whole invocation, including outer dispatch and harness work, not
isolated entry latency:

|!Workload|!Before|!Enabled A / B|!Reduction A / B|!Compile disabled
|boxed integer method|2413.7625|2411.9844 / 2413.8538|0.074% / -0.004%|2417.3999
|boxed float method|2419.5489|2410.8196 / 2400.8067|0.361% / 0.775%|2414.5238
|member updates|3378.1164|3356.2051 / 3380.7221|0.649% / -0.077%|3356.9807
|read-only integer member|2440.0587|2446.9714 / 2440.0422|-0.283% / 0.001%|2436.5643
|read-only float member|2471.3018|2465.1542 / 2467.2247|0.249% / 0.165%|2459.1694
|native integer result|2373.9928|2362.0568 / 2384.1737|0.503% / -0.429%|2342.0518
|native float result|2389.3228|2373.2495 / 2370.3979|0.673% / 0.792%|2339.9472
|indirect integer result|2207.7226|2217.9907 / 2205.1864|-0.465% / 0.115%|2217.8443
|indirect float result|2237.7239|2230.3940 / 2249.6631|0.328% / -0.534%|2204.1816
|list build|2800.3952|2764.6854 / 2756.1105|1.275% / 1.581%|2750.7257
|list mutation|3175.1140|3150.0758 / 3164.5376|0.789% / 0.333%|3160.3522

No clear short-call gain is established. Boxed integer changes are about
0.00-0.07%, boxed float reductions 0.36-0.77%, with overlapping ranges.
Member-update B is 0.71% slower than compile-off with separated ranges.
Native/indirect enabled arms reach 2.06% slower than compile-off with overlap.
List build improves 1.27-1.58% against baseline with separated ranges but is
0.20-0.51% slower than compile-off with overlap; that is not attributed to this
pass. No sample or adverse result is excluded.

Whole executable sizes match within each matrix (405690 bytes in confirmation,
405658 in the short matrix, 405642 in the fresh Java matrix). Padding hides
text growth: confirmation native text increases from 257856 to 258512 bytes,
656 bytes or 0.25%. Each inspected boxed fast caller adds 15 ARM64 instructions
(60 bytes). Single confirmation compilation observations are 1.134/1.137/1.093
seconds for before/enabled/off; short observations are 1.154/1.170/1.126 seconds.
They include full diagnostic LLVM dumps, not a controlled compiler-throughput
measurement. Initial compilation durations were not recorded or fabricated.

### Fresh matched Java comparison

825 interleaved windows, eleven workloads, five modes, three forks, count 4096,
three warmups, five 500-ms windows, seed 135229. Temurin 25.0.2 and pinned
JMH 1.37 use 18 verified generated target DONT_INLINE hints. All five modes
pass 555 references; actual Qore library selection, Java tools/classes/JARs,
fork settings and raw windows are independently checked. These are matched
whole-workload ns/element, not isolated call latency or equivalent runtime
contracts. Historical Java timings are not mixed into these ratios.

|!Workload|!Before|!Enabled A / B|!Reduction A / B|!Compile disabled|!Java JIT|!Enabled / Java
|boxed integer method|19.5298|18.3615 / 18.4691|5.982% / 5.431%|19.6397|2.5992|7.06 / 7.11x
|boxed float method|22.6343|22.0298 / 21.8065|2.671% / 3.657%|22.6490|2.6164|8.42 / 8.33x
|member updates|89.8817|88.3455 / 88.1299|1.709% / 1.949%|89.5680|1.8888|46.77 / 46.66x
|read-only integer member|33.7344|32.3787 / 32.6015|4.019% / 3.358%|33.4843|2.5806|12.55 / 12.63x
|read-only float member|34.3567|33.7563 / 34.0965|1.747% / 0.757%|34.9778|2.5499|13.24 / 13.37x
|native integer result|10.3518|10.4116 / 10.4433|-0.578% / -0.884%|10.4896|3.1297|3.33 / 3.34x
|native float result|11.5221|11.6089 / 11.6483|-0.753% / -1.095%|11.5021|3.5476|3.27 / 3.28x
|indirect integer result|6.8908|6.9001 / 6.8171|-0.135% / 1.070%|6.9591|1.9344|3.57 / 3.52x
|indirect float result|8.9689|8.9650 / 8.8696|0.044% / 1.108%|8.8072|2.8174|3.18 / 3.15x
|list build|85.7899|84.9843 / 86.4795|0.939% / -0.804%|85.3776|2.6051|32.62 / 33.20x
|list mutation|243.0698|239.9631 / 238.8705|1.278% / 1.728%|236.8958|4.1817|57.38 / 57.12x

Boxed integer calls improve 5.43-5.98%, boxed floats 2.67-3.66%, and integer
member reads 3.36-4.02%; both enabled arms separate baseline and compile-off.
Float reads improve 0.76-1.75% against baseline (only B separates) and
2.52-3.49% against compile-off (both separate). Member updates improve
1.71-1.95% against baseline, with overlapping ranges.

Native integer results are 0.58-0.88% slower and native floats 0.75-1.10%
slower than baseline, with overlapping ranges. Indirect float A is 1.79% slower
than compile-off with separated ranges; B is 0.71% slower with overlap.
List-build B is 0.80% slower than baseline and 1.29% slower than off; its A/A
medians differ 1.76% with overlap. List mutation improves against baseline
but is 0.83-1.29% slower than off, with overlap. A native-integer B fork at
11.68 ns remains. These controls do not establish broad native/indirect/list
gains, dismiss regressions, or resolve the older committed native-integer loss.

Native/indirect workloads remain about 3.15-3.57x Java; boxed methods 7.06-8.42x;
member reads 12.55-13.37x; updates 46.66-46.77x; lists 32.62-57.38x. Java parity
is unachieved. This small boxed-load optimization does not complete the larger
callee/caller effect proof or eliminate dispatch, frame and container costs.

### Qualification, audit and retention

Release passes 114 selected files, 417 cases / 36897 assertions, 15 standalone
fixtures and nine explicit skips. Debug passes 114 files, 424 cases / 36996
assertions, 15 standalones and eight skips. Both pass seven exception/stack
fixtures, native-unit checks and IR/AOT reference grids. All 795 focused debug
native checks, 17 benchmark-harness tests, four-mode 555-reference grids and
ten restored-release smokes pass. All 61 owned qmod aliases are restored to
matching release artifacts; unrelated aliases are unchanged. Independent
verification reconstructs 263 qualification child records. This is selected
macOS coverage, not full-repository, sanitizer, leak or cross-platform coverage;
Linux RSS and release-only debug-hook skips remain explicit.

The expanded value-tag fixture covers AST/IR/JIT/tiered and AOT O0/O3,
normal/EH/no-debug/split builds, independent compile/runtime controls, refs,
enums, inline/heap values, opaque/weak values and destruction. Existing raw-local
IR/tiered limitations remain explicit; supported modes exercise that form.
A separate actual-QoreValue/helper check covers 589824 tag/payload combinations
(all 65536 upper tags times nine payloads); fabricated pointers are never
read or released. This is not an exhaustive 64-bit sweep.

Preflight failures remain archived: the first initial verifier had an escaped
regex mismatch; pointer preparation initially assumed all entries were private
fast entries instead of admitting normal entries. Inspection also superseded
an initial opaque-parameter hypothesis: the dedicated weak loader explains the
untransformed opaque control. These were not runtime or timing failures.
The short-preparation tool response was truncated; saved artifacts and completed
jobs were independently checked after confirming no process remained, without
restarting preparation or inventing an exit code. A reporting query assumed
optional initial compile-cost fields and was corrected without rerunning data.

All 3605 candidate windows are retained and independently verified. Frozen
sources/binaries and qualification remain unchanged; this README/RESULTS and
publication are documentation-only additions checked separately. The full
62-item audit-changes review is recorded with each item in the linked evidence.

|!Audit group|!Status|!Evidence
|Module docs, registration, layout and QPP namespace|N/A|No new module or QPP class; no optimization release note
|New-file copyright and tests|Pass|2026 MIT evidence; modern executable fixtures; shipped module dependencies
|Filesystem and network sandbox hooks|N/A|No added filesystem/network operation in production code
|Cooperative cancellation|Pass|New function/instruction scans every 100 iterations; guard/simplification work every 10
|DataProvider actions, types, descriptions and factories|N/A|No DataProvider implementation changes
|JNI JAR/install rules and QPP flags|N/A|No JNI/module installation or QPP changes
|Workarounds, types, errors and threading|Pass|Complete pass; typed callbacks; verifier/cancellation failures propagate; compilation-local state
|Exception safety and ownership|Pass|LLVM nodes remain module-owned; exact non-pointer identity bypass; original pointer helper/order retained
|Performance|Pass|All 3605 windows, adverse controls, short-call limits and text growth retained
|Documentation|Pass|Internal method contract, independent compiler controls, results and strict Qore table checks
|Fixed formats and sensitive data handling|Pass|Fixed diagnostics; no new raw buffer or credential; allowlisted environment evidence
|Correctness|Pass|Tag/helper predicate checks; selected release/debug, EH, ownership and observable-context tests

Retention accepts the repeated boxed/member loop gains with the small adverse
controls, short-call limits and code growth explicit; it is not universal or
regression-free improvement. Continue toward a complete successful-pure-callee
witness plus CFG-based caller/cleanup effect proof before eliding invalidation:
even non-final decrements can enter DGC. Invocation-prepared live dispatch/body
proofs and member/container storage, locking, coercion and ownership remain
separate unfinished targets. No goal-completion or Java-parity claim is made.

[Verified samples, qualification and all audit items](results-aot-scalar-reference-guard-macos-arm64.json).

## AOT scalar publication boundary reuse

Primary target: AOT. Baseline: `a74cc851ee5769267e356feac91d19ef5c3ee90d`
on `feature/performance`, with matching frozen compiler/runtime pairs on macOS
arm64. This change preserves one exact scalar local cache across a temporary
cleanup boundary after a successful prepared store. It does not assume callee
purity, remove ownership work, or establish an IR/JIT speedup.

### Final matched AOT and Java comparison

The final candidate includes the cancellation fix found during audit. Its fresh
Java comparison uses Temurin 25.0.2+10, JMH 1.37, verified generated DONT_INLINE
hints for retained call targets, one thread, three fresh process repetitions,
three warmups and five 500-ms measurement windows per repetition. Qore uses
`qcc -O3`; both languages use count 4096 and input seed 7. Randomized order seed
135401 interleaves baseline, enabled A/A, compile-off and Java. All 825 windows
are retained. Java allocation data is retained, including about 36.22 B/element
for list construction and 60.17 for list mutation.

Values below are medians of per-process means, in whole-workload ns/element,
not isolated entry latency. Enabled ranges span the two A/A medians, not a
confidence interval. Ratios use Java from this same matched matrix only.

|!Workload|!Baseline AOT|!Enabled AOT A/A|!Java JIT|!Enabled / Java
|Boxed integer method|18.085|15.734-15.770|2.552|6.17-6.18x
|Boxed float method|21.341|19.218-19.437|2.554|7.52-7.61x
|Boxed integer effect/member update|87.982|84.410-84.741|1.882|44.85-45.02x
|Integer fallback/member read|31.788|29.939-30.178|2.533|11.82-11.91x
|Float fallback/member read|34.023|31.848-31.868|2.520|12.64-12.65x
|Native integer method result|10.172|9.861-9.957|3.052|3.23-3.26x
|Native float method result|11.299|11.430-11.447|3.468|3.30x
|Indirect integer result|6.714|6.588-6.696|1.878|3.51-3.56x
|Indirect float result|8.761|8.603-8.700|2.946|2.92-2.95x
|List construction|82.508|83.581-83.992|2.521|33.15-33.32x
|List mutation|229.801|226.654-226.688|4.074|55.63-55.64x

Boxed integer time falls 12.80-13.00%, float 8.93-9.95%, and member-read/fallback
time 5.06-6.39%; both enabled process ranges separate from baseline in each
case. Effect/member-update time falls 3.68-4.06%, also separated. The reductions
remain against compile-off: integer 12.47-12.67%, float 11.03-12.03%.
These are sampled range comparisons, not statistical significance tests.

Native integer improves 2.12-3.06% versus baseline, but its enabled and compile-off
ranges overlap. Native float is 1.16-1.31% slower than baseline, with overlapping
ranges and almost identical compile-off timing. List construction is 1.30-1.80%
slower, with overlapping ranges; list mutation is 1.35-1.37% faster, also overlapping.
Indirect changes are small and mostly overlapping; the Java indirect-float
process means themselves span 2.575-3.008 ns/element. Do not attribute every
baseline difference to the new boundary path or claim a universal improvement.

A separate final-candidate matrix (order seed 135389) retains 990 windows across
before/A/A/compile-off/runtime-off/liveness-off. Boxed integer is
17.935 -> 15.809-15.816 ns/element, 11.81-11.85% lower; float is
21.702 -> 19.274-19.407, 10.57-11.19% lower. Both enabled ranges separate from
baseline, compile-off and runtime-off. Native float is 1.35-2.07% slower than
baseline (one enabled range separates); compile-off is also 1.53% slower.
List construction is mixed with overlapping baseline ranges. These numbers
are not combined with Java from another matrix.

### Iterations, controls and code cost

The linked evidence retains 6985 windows: initial v1 825, v1 confirmation 1375,
v1 pointer controls 450, v2 long-loop 990, v2 pointer controls 540, v2 short
invocations 990, final v3 990, and fresh matched Java 825. Candidate versions
are distinct from helper ABI version v3. Historical candidates were not committed.

The initial broad cleanup selection improved boxed loops but added unnecessary
guards to pointer paths. In v1, the list-pointer workload was 2.20-2.36% slower
than compile-off with both process ranges separated. V2 intersects forward
publication reachability with backward boundary liveness. Its pointer-list
enabled medians differ from compile-off by only +0.02-0.47%, with overlapping
ranges. Other pointer controls remain mixed: raw-reference time is 1.17-1.35%
slower than unpruned selection (one range separates), but only 0.48-0.66% slower
than compile-off with overlapping ranges. No general pointer speedup is claimed.

Count-one v2 measurements show no short-call benefit. Boxed integer/float medians
are about 0.47-0.93% slower with overlapping ranges; native integer is 0.68-0.96%
slower with both ranges separated. These approximately 2.2-us invocations include
outer dispatch and harness work, not just the fast call. V3 changes compiler
cancellation checks only: 163 non-main LLVM definitions compare exactly with v2
after removing debug annotations. Two launchers differ only in validated metadata
array lengths. This supports relevance of the v2 controls, but does not turn them
into final-v3 short/pointer measurements or prove binary-layout equivalence.

The final Java-matrix enabled module has nine publication functions, 54 early
cleanup guards, 1163 effect barriers and 235 rejected dead-cleanup candidates.
Unpruned v2 selection inserted 289 early guards. These are compiler site counts,
not dynamic execution counts. The final boxed integer private caller grows
from 829 to 913 ARM64 instructions, float from 833 to 917; compile-off returns
to baseline counts. The executable grows from 405674 to 422186 bytes (4.07%).
Single preparation compiles took 1.039 s baseline, 1.054 s enabled and 1.074 s
compile-off; these are observations, not a compile-time performance claim.
The pruned pointer private caller retained its 895 baseline ARM64 instructions
in v2, versus 1072 under unpruned selection.

### Runtime contract and qualification

The additive v3 helper shares v2 assignment semantics and reports only exact,
sink-clean, non-reentrant scalar publication in an admitted live slot. A caller
captures the witness immediately, before subsequent retention and cleanup.
Only the matching cache with matching bits and an already-current epoch can
remain fresh; other locals stay stale. Before any LLVM optimization, unknown
calls/invokes, external writes and fences revoke the token. Scalar no-op edges
can bypass reference helpers; generic incref and decref are never assumed pure.
Original owner rotation, exceptional cleanup, reflection and cancellation remain.

Compile-time `QORE_DISABLE_AOT_SCALAR_BOUNDARY_REUSE=0` removes the new state and
uses v2 stores. Runtime presence suppresses only witness reporting, sampled once
at first use. Compile-time `QORE_DISABLE_AOT_SCALAR_BOUNDARY_LIVENESS=0` restores
broad guard selection without weakening barriers. Global scalar/float cleanup
controls govern early guards; the method-only cleanup flag still governs the
independent late selection. Compiler/runtime identities must match.

Release passes 115 selected files, 418 cases / 37109 assertions; debug passes
115 files, 425 cases / 37208 assertions. Each includes 15 standalone fixtures;
nine release and eight debug skips remain explicit (Linux RSS checks and the
release build's unavailable debug hooks). Both pass seven exception/stack fixtures,
native-unit checks and IR/AOT reference grids. Six focused debug control modes
each pass 829 native checks. The 17 harness tests, four-mode 555-reference grids
and 11 restored-release smokes pass. All 62 owned qmod aliases are restored to
matching release artifacts; unrelated links are unchanged. Independent replay
checks 266 qualification child records. This is selected macOS coverage, not
full-repository, cross-platform, sanitizer or leak qualification.

The new regression exercises scalar loops, earlier reflective mutation of a
different local, reference/destructor reentry, throwing cleanup, branches,
backedges, zero/single inputs and boxed value families. It compares AST with
AOT O0/O3, EH, no-debug, split and disabled controls. The baseline IR interpreter
does not expose this fixture's caller body local to the reflection helper;
that limitation is explicit rather than described as a passing IR reflection test.

Preserved preflight failures include an inherited runtime-control test expectation,
an initial LLVM comparator that omitted launcher metadata-length differences,
and a Java preparation assertion that incorrectly required a v2 store symbol
in the fully v3-enabled fixture. None supplied retained timing data. Truncated
tool responses for short preparation and final broad qualification are recorded
with unknown outer exit codes; independent replay validates child results and
artifacts without inventing successful terminal observations. The final AOT and
Java timing drivers and their verifiers have observed successful terminal exits.

### Audit and next work

The audit-changes review covers all 62 checklist items in the linked evidence.
It found and fixed missing cancellation checks on duplicate worklist pops before
final qualification and measurement. No production code changed after the freeze;
design, controls and this results publication are checked as documentation-only
additions.

|!Audit group|!Status|!Evidence
|Module registration, layout and QPP namespace|N/A|No new module or QPP class; no optimization release note
|New files and tests|Pass|2026 MIT headers; modern executable fixtures; delivered QUnit/FsUtil dependencies
|Filesystem and network operations|N/A|No new production filesystem/network operations
|Cooperative cancellation|Pass|Instruction, CFG-edge and worklist-pop checks every 100; guard transforms every 10
|DataProvider and JNI registration|N/A|No action, app, factory, JAR or module-install changes
|Workarounds, types, errors and threading|Pass|Complete pass; compiler-owned state; immutable helper configuration; error propagation
|Ownership and exception safety|Pass|Point-in-time witness; full effect barriers; owning and throwing paths retained
|Performance|Pass|All 6985 windows retained, including adverse controls, short-call limits and code growth
|Documentation|Pass|Internal contracts, design, independent controls, results and strict table validation
|QPP flags|N/A|No QPP method changed
|Bounds, formats and sensitive data|Pass|Existing slot admission; fixed diagnostics; allowlisted environment evidence
|Correctness|Pass|Selected release/debug, EH, reentry, observable context and reference checks

Retain the repeatable boxed-loop gains with the costs above explicit. Java parity
is unachieved: native/indirect calls still take roughly 3x Java time; boxed calls
6-8x; member/container paths much more. Profile the retained AOT build next,
then target typed dispatch/frame/context overhead and member/container storage
costs. The prior profile showed negligible TLS sampling, so another broad TLS
rewrite is not justified by this evidence. Dead publication returns are a
separate possible code-cost cleanup, not a demonstrated next speedup. Do not
repeat the previously rejected prepared-body experiment without a new proof or
measured bottleneck.

[Verified measurements, qualification and every audit item](results-aot-scalar-boundary-reuse-macos-arm64.json).

## Rejected AOT register-result conventions (2026-10-14)

Neither of these experiments is retained. Both used the validated post-merge
`6b4590ba714a07a4ba361c63640a19a658529858` baseline on macOS arm64. The proposal
was to replace typed method helper output pointers with a two-word return:
full-width scalar or owned boxed payload, plus miss/native/ordinary-result kind.
Admission, Program/access/epoch checks, observable frames and exception handling
were preserved. Original output-pointer helpers remained available.

The first version combined native-only and matched-boxed dispatch with a policy
argument. Its native helper frame grew from 320 to 384 bytes and the result still
spilled during context cleanup. The second separated native-only and matched
helpers and isolated the eager-context output address. Actual assembly then kept
pure results in registers; native frames returned to 320 bytes, and matched
frames used 368 bytes. This removed the intended spill but did not produce a
worthwhile performance improvement.

Each matrix used 11 workloads, a frozen baseline, enabled A/A, and a candidate
compiled with the new convention disabled. Three forks, three warmups and three
300 ms samples at count 4096 yielded 396 measured windows per matrix: all 792
windows are retained, including noisy forks. Runtime aliases had equal length;
loaded libraries and 555 references per configuration were checked before
timing. Values below are whole-workload ns/element, not isolated call latency.
The two enabled medians are shown as a range, not as a confidence interval.

|!Experiment and workload|!Before|!Enabled A/A|!Compile-off|!Change versus before
|Unified: native integer|9.868|10.967-11.017|9.814|11.13-11.64% slower
|Unified: native floating point|11.433|11.901-11.989|11.179|4.09-4.87% slower
|Unified: boxed integer|15.709|15.950-16.103|15.628|1.53-2.51% slower
|Split: native integer|10.023|10.928-11.194|9.923|9.03-11.68% slower
|Split: native floating point|11.401|11.493-11.736|11.763|0.80-2.94% slower
|Split: boxed integer|15.592|15.727-15.808|16.094|0.86-1.38% slower
|Split: boxed floating point|19.512|19.441-19.447|19.239|0.33-0.36% lower; overlapping fork ranges

Native integer fork ranges were separated from baseline and compile-off in both
experiments. Other movements were mixed or overlapped: for example, the split
floating native compile-off control was also slower. The unified experiment's
unrelated floating callback regression did not persist in the split experiment.
Do not credit such movements to a faster method ABI, or infer that register
returns are inherently slower in other implementations.

The split prototype passed 15 selected release test files, 27 cases and 12278
assertions, plus both 555-check IR/AOT reference grids. Coverage included native
and legacy helpers, full-width values, pending exceptions, stack exhaustion,
member effects, debugger attachment and concurrent frame readers. Minimal C++
and LLVM code-generation probes agreed on aggregate returns for arm64/x86-64
Darwin/Linux; x86-64 Windows differed, and Windows targets were excluded. This is cross-target codegen
evidence, not foreign-platform runtime testing. Debug qualification and a new
Java comparison were not pursued after the negative performance result.

The experimental runtime, lowering and test changes were removed. Their patches,
supplemental test source, raw samples, reference outputs, controls and verification
records remain in the [rejected-experiment dataset](results-aot-register-result-rejected-macos-arm64.json).
The existing output-pointer convention remains the baseline. Next work should
target the measured dispatch/admission and member-access costs; another
result-layout variant is not justified by these results alone.

## Rejected AOT receiver-type admission proofs (2026-10-14)

Neither receiver-proof implementation is retained. The frozen baseline was
`fd6aabe6ce1ac5f26efb417853eb9bdc8b655259`; its production sources match the
validated restored runtime from the preceding experiment (the intervening commit
changed documentation/data only). These experiments targeted typed AOT method
dispatch, not a new IR/JIT optimization or Java comparison.

An exact receiver-class match and an admission-time canonical-type proof can
avoid loading the ordinary receiver's type through its class/private-data chain.
The first version stored a boolean in existing PIC-entry padding. The second
folded that proof into an available alignment bit of the existing scalar
comparison word. Neither reconstructed or dereferenced a pointer from that word.
Non-null instantiated types retained their full comparison; unsupported alignment
and missing/disabled proofs retained the original checks. Program, access, epoch,
body, debugger, stack and frame contracts were unchanged.

Each version used a frozen baseline, two identical enabled configurations and a
runtime-disabled proof control. The control was presence-based (including `0`)
and evaluated at admission, not on each call. Eleven workloads, three forks,
three warmups and three 300 ms samples at count 4096 produced 396 measured
windows per version, with independently randomized orders. All 792 windows are
retained. Runtime paths and control environment entries had matching lengths;
loaded libraries and 555 reference outputs per configuration were verified.

Values below are whole-workload ns/element, not isolated call latency. Enabled
ranges show the two A/A medians, not confidence intervals.

|!Version and workload|!Before|!Enabled A/A|!Runtime-off|!Change versus before
|Boolean: native integer|10.080|10.298-10.511|10.258|2.16-4.28% slower
|Boolean: boxed floating point|19.212|19.508-19.521|19.467|1.54-1.61% slower
|Combined key: native integer|10.061|10.072-10.109|10.659|0.11-0.48% slower; effectively flat
|Combined key: native floating point|11.538|11.291-11.431|11.887|0.93-2.14% faster
|Combined key: boxed floating point|19.132|19.681-19.801|19.739|2.87-3.50% slower
|Combined key: effectful integer member|83.899|85.236-85.662|86.036|1.59-2.10% slower

The combined version's boxed-float and effectful-member fork ranges were
separated from baseline. Its native-integer result overlapped baseline; the
small native-float improvement did not justify the other regressions. Other
workloads were mixed or noisy, including one slow effectful-member control fork;
none was dropped. Comparing only enabled versus runtime-off would misleadingly
suggest a useful native-integer improvement: the relevant unchanged baseline
was already as fast as the enabled candidate.

Both versions retained the 240-byte dispatch-entry layout and the native-int
helper's 320-byte stack frame on macOS arm64. The intended dependent type loads
were removed from the proved hit, but the complete native-int helper grew from
477 instructions to 607 (boolean) or 615 (combined key). These code-generation
observations do not establish the exact cause of every timing movement.

The boolean release screen passed 14 test runs, 26 cases and 12704 QUnit
assertions; the combined-key screen passed 13 runs, 25 cases and 12036 assertions.
Both included the C++ receiver/context probes, disabled controls, native/boxed
calls, member effects, frame readers, cleanup/cancellation and both IR/AOT
555-check grids. Initial test-development failures incorrectly expected native
admission for parameterized receivers; the existing exclusion was retained and
the probes corrected before timing. Debug and foreign-platform qualification
were not pursued after the negative measurements.

All experimental runtime and test changes were removed. The restored release
core and 62 matching AOT modules were rebuilt; original method tests passed
3 files, 9 cases and 4905 assertions, plus both IR/AOT reference grids. Source
and runtime-identity checks match the pre-experiment baseline. The
[dataset](results-aot-receiver-proof-rejected-macos-arm64.json) preserves both
patches, raw timings, reference outputs, assembly, restoration and all audit
items. Next work should investigate per-call configuration/initialization guards
and more granular dispatch evidence before another receiver-proof encoding.

## Rejected AOT single-load policy cache (2026-10-14)

The policy-cache prototype is not retained. It replaced four function-static
boolean policy sites in typed method dispatch with a single atomic byte holding
both initialization state and value. A per-object `std::call_once` performed cold
initialization at the original first-use point; acquire/release preserved
publication, independent policies, latching and retry after an exception. No
policy moved to parse time or admission, and existing environment controls kept
their presence-based behavior. Call ABI, lowering, dispatch layout, live guards,
frames and receiver ownership were unchanged.

The frozen baseline was `c6794233ccfc013d74935aeff859ee189092c7ba` on
`feature/performance`. Both matrices compared two identical baseline
configurations with two identical candidate configurations, using randomized
serial execution, equal-length runtime aliases and verified loaded libraries.
All four configurations passed 555 reference outputs before each matrix. Eleven
workloads at count 4096 used three warmups and three measurement samples per
process: the initial matrix had three forks and 300 ms samples (396 windows),
and confirmation had five forks and 400 ms samples (660 windows).

Values below are whole-workload ns/element, not isolated call latency. Each
range contains the two A/A medians, not a confidence interval. Compare before
and after within the same matrix; absolute timings rose between matrices.

|!Matrix and workload|!Before A/A|!Candidate A/A|!Conclusion
|Initial: native integer|9.964-10.014|10.013-10.015|Effectively unchanged
|Initial: native floating point|11.283-11.450|10.908-10.985|2.64-4.73% faster
|Initial: effectful integer member|83.570-84.644|85.856-86.353|1.43-3.33% slower
|Confirmation: native integer|10.343-10.366|10.274-10.475|Mixed; within fork variation
|Confirmation: native floating point|11.760-11.852|11.550-11.645|0.97-2.54% faster
|Confirmation: effectful integer member|87.639-88.207|89.332-89.895|1.28-2.57% slower

Percentage ranges include all combinations of before/after A/A medians. Native
integer calls showed no repeatable benefit. The floating-point gain did not
justify effectful calls getting slower in both matrices. Other workloads were
mixed: an initial integer-callback slowdown did not clearly repeat; confirmation
list mutation medians were higher, with overlapping fork ranges. No noisy sample
was dropped, and these movements do not establish the cause of each regression.

Actual arm64 assembly confirmed the intended removal of one policy load on the
initialized path. Native-only helper frames stayed at 320 bytes, and matched
helpers stayed at 368 bytes. Complete native-int helper sizes fell from 477 to
465 instructions (v1) and from 487 to 475 (v2); floating helpers changed from
444 to 445 and from 460 to 458. These code-generation improvements alone were
not sufficient evidence to retain the candidate.

The candidate passed 14 selected release test runs, 26 cases and 12026 QUnit
assertions, plus both IR/AOT reference grids. Its standalone policy helper also
passed ThreadSanitizer and AddressSanitizer/UndefinedBehaviorSanitizer probes
for concurrent first use, publication, false/true latching, independent objects
and exception retry. This was not a sanitized full-runtime build. New debug,
foreign-platform and Java qualification were not pursued after rejection;
there is no new IR/JIT timing claim.

The first timing driver's terminal output was lost during context compaction.
Its complete 132 child exit records and raw outputs were independently replayed;
the parent exit status remains explicitly unknown. The larger confirmation's
timing and verification exits were directly observed as successful. All 1056
measurement windows, both A/A controls, raw outputs, source changes, assembly,
validation and audit records are preserved in the
[dataset](results-aot-native-policy-rejected-macos-arm64.json).

The original runtime sources were restored, the release core and 62 matching
AOT modules rebuilt, and five original test files passed 14 cases and 6009
assertions plus both reference grids. No policy helper or experimental test
source remains in the production tree.

|!Audit scope|!Pass|!N/A|!Fail
|Retained documentation and research data; all 62 checklist items recorded in the dataset|7|55|0

The audit does not approve the archived prototype for production use.

Next work targets effectful member access. Source tracing shows that the
prehashed update path still resolves declaration/access context before locking
and looking up storage. Exact-class object summaries already cache immutable
member metadata; extending that to polymorphic effectful calls requires a live
receiver/context key and a lifetime/invalidation proof. Keep object locking,
deletion checks, reference/enum/overflow fallbacks and exact-once continuation;
do not cache a raw member pointer across mutations. Another policy-byte encoding
is not justified by these results alone.
