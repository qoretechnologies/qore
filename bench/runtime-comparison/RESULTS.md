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

## Prioritized follow-up work

These are profile-backed targets, not implemented optimizations or predicted
speedups. Profiles were collected separately from measurements with macOS
`sample` at 1 ms intervals for five seconds. Counts below exclude the idle
signal-wait thread and use leaf samples, avoiding recursive double counting.

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
