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

For compatible **runtime-only** changes, add `--reuse-aot-for-baseline` with
`--modes qore-aot --baseline-libdir /path/to/saved-library`. This compiles once
and runs the identical executable against both libraries, holding caller code
fixed without eliminating runtime-library layout or host variation. Both modes still pass the full
reference checks before timing. Metadata records `baseline_aot_reused: true`,
identical executable hashes and only one compile time. Do not use this option
to measure compiler optimizations or when the new executable requires symbols
or an ABI absent from the saved runtime; a failed baseline check aborts the run.

For AOT method-runtime context comparisons, the process presence flag
`QORE_DISABLE_IR_CONTEXT_ELISION=1` retains eager synthetic self/argv setup.
The optimized dot-eval path reads immutable flags from an already-ready AOT
context and is limited to the same execution Program without a debugger.
Unused argv setup is omitted only without excess arguments; their ownership
and cleanup remain unchanged. This flag also controls other context-elision
paths, so treat it as a diagnostic control, not isolated cost attribution.

For the general callback runtime path, compile and run with
`QORE_DISABLE_AOT_FUNCREF_NATIVE_ENTRY=1`; this omits native function-reference
adapters. The plain-function context fix is measured both normally and with this
control in [the results](RESULTS.md#plain-function-access-context-2026-10-08).
It corrects a caller-class access leak and has a measured generic-call cost;
do not describe it as a performance improvement.

`QORE_DISABLE_AOT_RUNTIME_CHECKED_CLASS_STORE=1` at compilation restores separate
coercion for required-class AOT local initializers. The optimized form retains
the checked assignment and reloads its stored value; it does not remove class
access or weak-reference checks. Use separate before/after compilation for this
compiler optimization, not `--reuse-aot-for-baseline`.

`QORE_DISABLE_AOT_CLASS_STORE_RESULT=1` at compilation retains a separate local
reload after that checked assignment. Otherwise eligible noncaptured, non-self
class initializers use a versioned helper returning the owned stored value to
the existing cache/reload tracker. Only an empty plain runtime slot returns the
snapshot; nonfresh/captured slots reload after cleanup. Type/access checks and
weak normalization remain. Compile separate before/after artifacts: the new
helper symbols are absent from older runtimes.

`QORE_DISABLE_AOT_PINNED_CLASS_LOCAL=1` at compilation disables guarded borrowing
of an existing entry owner for the cache of an initial required-class local.
The enabled path admits only bounded PHIs/loads of unmodified entry-owned
parameters and checks the actual value against those retained entry values.
Reflection replacing a source takes ordinary owned caching. The runtime still
checks the slot, strong-object representation and class/access acceptance, and
keeps its normal slot reference, real-reference accounting and cleanup order.
Only the additional cache reference is omitted. Body-local sources, filters,
fixed storage and other unadmitted values keep the owned path. Setting this
presence flag only at execution exercises that fallback. Compile separate
before/after artifacts: the versioned borrowing helpers are new runtime symbols.

`QORE_DISABLE_AOT_CLASS_LOCAL_SLOT=1` at compilation keeps ordinary TLS/stack
lookup for required-class body locals. Otherwise AOT resolves an opaque storage
handle after entry instantiation, using an explicit RuntimeConfig, and reuses it
for pinned initializers and loop-scope cleanup. The slot remains on the runtime
stack until function exit: reflection still sees and changes its live value.
This caches storage, not class acceptance or object identity; all assignment
type/access, empty-slot, representation and ownership checks remain, as do
real-reference accounting and source cleanup order. Captured, self, top-level,
complex and IR-only locals do not use the handle. At runtime the same presence
control, sampled at the first getter invocation, makes handle acquisition
return null, exercising ordinary lookup. Each
handle is confined to one invocation/thread, including recursive fast entries.
Use separately compiled before/after artifacts for the new versioned helpers.

`QORE_DISABLE_AOT_FRESH_CLASS_LOCAL_STORE=1` at runtime disables the fresh-slot
specialization inside that versioned helper. The enabled path skips generic
lvalue setup only for an unassigned, ordinary required-class local, keeping
normal assignment type/access checks and real-reference bookkeeping. Captured,
self, substituted, finalized, static and already-assigned slots fall back.
Storage and destructor cleanup remain runtime-backed. This runtime-only change
can use `--reuse-aot-for-baseline` when both runtimes provide the versioned helper.
The presence flag is sampled once, on first helper use.

`QORE_DISABLE_AOT_CLASS_ACCEPT_FAST_PATH=1` at runtime keeps generic conversion
dispatch for those fresh class stores. The enabled path positively checks a
single unmapped class constraint with the ordinary raw/parameterized-object and
inheritance/access validators; misses retain the original diagnostics. No
compatibility/access result is cached. The compiler and helper ABI are unchanged,
so runtime-only before/after comparisons can reuse the identical AOT executable.
The shared hierarchy lookup also recognizes exact class-definition identity
before entering general structural/injected-class comparison. Copied handles
still return the canonical class; private/internal base-edge checks and access
accumulation remain. No inheritance decision is cached. The
[identity follow-up](RESULTS.md#exact-class-definition-lookup-2026-10-10) records
three 3.43–4.08% typed-polymorphic gains and a possible small-loop monomorphic
tradeoff, using identical-executable runtime comparisons.
`QORE_DISABLE_AOT_CLASS_NODE_STORE=1` at runtime retains generic value storage
after that positive check. Otherwise an accepted strong object uses a narrow
node initializer for the proven-empty, nonfixed owning slot, avoiding generic
replacement/conversion work and empty temporary-owner cleanup. Slot guards,
the slot's owner (plus a second owner when an owned result is requested),
real-reference accounting, weak normalization, and cleanup order remain unchanged.
The [initialization follow-up](RESULTS.md#empty-class-local-node-initialization-2026-10-10)
records three 4.19–5.44% typed-polymorphic gains, all controls and one unexplained
floating-point callback outlier. No general-call gain is claimed. This presence flag is sampled
once; the helper ABI is unchanged and identical-executable comparisons are valid.
The measured disabled path has a small adverse cost; see `RESULTS.md` rather
than assuming the control path is performance-neutral.
`QORE_DISABLE_AOT_CLASS_STRONG_STORE=1` retains the weak-normalization entry path
for strong objects. Otherwise a positively identified strong input attempts the
same checked fresh initializer before constructing the weak-input holder. A
handled result skips only empty weak-holder setup/cleanup; ownership, class
access, errors and runtime-backed storage are unchanged. Weak/non-object and
unadmitted inputs retain the ordinary path (with an extra admission probe on
some misses). The flag is sampled once and the helper ABI is unchanged. See
`RESULTS.md` for both baseline comparisons and a same-library on/off comparison;
do not attribute all cross-library variation to this branch.

`QORE_DISABLE_AOT_CLASS_PARAM_CACHE=1` at compilation disables guarded reuse of
read-only class parameter caches. The default admits only unwritten,
unescaped, noncaptured, nonweak, nongeneric parameters with an existing entry
owner and an identical ordinary strong runtime slot. Required and optional
plain-class contracts are supported; an initial `NOTHING`, reference or weak
slot does not qualify. Generic exclusion checks the declared type before
optional-type folding can erase its arguments. A per-thread/per-Program
reflective-write epoch invalidates reuse before assignment can reenter Qore;
after the first write, the invocation keeps ordinary reload/reference semantics.
Runtime slots, owners, deferred cleanup, receiver validity and access checks remain.
The control retains the loop-backedge freshness fix, so it measures the cache
against a corrected fallback, not against the older unchecked first-receiver path.
Compile both artifacts separately; newly compiled callers need the additive
`qore_rt_get_class_param_epoch_aot_v1` runtime helper. Older artifacts still run
but must be recompiled to receive the fix. See the measured regressions as well
as gains in [RESULTS.md](RESULTS.md#guarded-class-parameter-caches-2026-10-08)
and the [optional-class follow-up](RESULTS.md#optional-class-freshness-2026-10-08).

An unrelated prepared **method** entry experiment was rejected after same-library
compiler controls showed no benefit; it adds no supported flag or helper ABI.
See [the negative result and retained diagnostic lifetime fix](RESULTS.md#prepared-method-entry-rejected-2026-10-09).

`QORE_DISABLE_AOT_INLINE_RECEIVER_VALIDITY=1` at compilation keeps the per-call
validity helper for stable exact-class receivers. Otherwise the existing entry
owner can pin a borrowed, versioned live status view. Generated code still
checks receiver identity and loads deletion status atomically with sequential
consistency on every call; it does not cache validity or add a reference.
Deleted receivers fall back, while calls during destruction retain `isValid()`
semantics. Setting the flag only at execution makes the getter return null and
exercises the helper fallback. Unsupported atomic representations also return
null. This compiler/runtime change requires separate baseline compilation:
new artifacts reference `qore_rt_get_object_status_view_v1`, absent in old runtimes.
See [the measured benefit and limits](RESULTS.md#live-receiver-status-loads-and-polymorphic-decomposition-2026-10-09).

Cold overflow-path separation was also [measured and rejected](RESULTS.md#cold-stack-check-paths-rejected-2026-10-09):
smaller checks did not establish a broad benefit. The next target is polymorphic
receiver-local setup and ownership versus dispatch. Existing scalar callback
preparation remains enabled.

The prepared scalar callback v3 entrypoints currently request 64-byte alignment.
Earlier class-check experiments used 16 bytes and also measured unaligned and
128-byte alternatives. The dynamic-closure follow-up compares saved executables
across runtimes before selecting 64 bytes; a new executable/runtime combination
otherwise regressed floating callbacks. Alignment changes no callback guard,
helper ABI or frame lifetime; it is not a portable performance guarantee.

`QORE_DISABLE_AOT_NATIVE_METHOD_ENTRY=1` disables the pure scalar method binding
at compilation and its polymorphic-cache native path at runtime. Eligible
`int(int)` and homogeneous `float(float)` overrides bypass parameter TLS binding but retain receiver, Program,
debugger, body-identity, stack and observer guards. Use separate before/after
compilation: the additive helper is absent from older runtimes. The original v1
caller uses boxed arguments/results; neither that measurement nor the scalar
continuation below isolates a native call instruction or proves Java retained-call parity.

`QORE_DISABLE_AOT_NATIVE_METHOD_CONTINUATION=1` disables the optional method v2
binding and scalar caller continuation at compilation, and its try-helper at
runtime. It retains the original v1 method path. Use separate before/after
compilation for this compiler/runtime change. Eligible unary integer/float calls pass
the live `RuntimeConfig` and scalar argument/result through a warmed full-key PIC;
a sole exact native local assignment can consume the result without boxing.
Unknown overrides still preserve arbitrary boxed returns and ordinary coercion,
cleanup, side effects and exception behavior. Receiver ownership, current access
context/parse policy, Program, epoch, debugger, body readiness and stack/frame
guards remain. The exact-base fast/inlined case is retained, so this workload
still does not represent three retained Qore methods.

`QORE_DISABLE_AOT_NAME_METHOD_NATIVE_CONTINUATION=1` disables the compiler's
native-result continuation for unresolved name-dispatched calls, including
nullable-class and dynamic receivers. Compile separate artifacts: this flag is
not a runtime switch. A warmed full-key PIC supplies the checked target; the
compiler does not change AST/IR resolution or the unknown return contract.
An already-native integer/float argument qualifies. A matching sole native-scalar
local assignment or proven sole scalar conversion consumes the unboxed result;
other consumers receive an owned boxed result. Wide integers and tag-colliding
doubles retain their ordinary heap encoding when boxed. Boxed and optional
arguments remain excluded. Every miss retains original
dispatch, arbitrary boxed returns, return checks, caller-local invalidation and cleanup.
The broader native-method entry/continuation controls also disable this path.

`QORE_DISABLE_AOT_NAME_METHOD_BOXED_RESULT=1` disables the additional non-store
consumers at compilation, including direct scalar conversions of name-method
results, while preserving exact native-local continuations.
Compile separate artifacts; this is not a runtime control. The new path reuses
existing scalar helpers and the caller's hoisted RuntimeConfig; no runtime ABI or
whole-body eligibility change is needed. Unsupported targets can pay an unsuccessful
native attempt before fallback, so include `dynamic_int_result_fallback` in comparisons.

`QORE_DISABLE_AOT_NATIVE_CALL_CONVERSION=1` disables native result consumption by
a sole `ToInt`/`ToFloat` instruction at compilation. Compile separate executables;
this is not a runtime switch. Typed function references and resolved/unresolved
scalar methods reuse the existing native-entry proofs and result side channel.
The compiler counts all uses, including returns and PHIs, and requires the producer
to be lowered before its consumer. Escaping and multiply consumed results stay boxed.
Native float-to-int conversion uses the existing runtime helper, preserving its
NaN/infinity/out-of-range behavior instead of introducing LLVM `fptosi` poison.
Arbitrary boxed fallbacks retain the original conversion, ownership, exceptions,
caller-local invalidation and diagnostic-frame behavior. No runtime ABI changes.
The measured gains are for floating-result workloads; small-integer results do not
show a consistent benefit, and some unchanged-code controls show small slowdowns.
See [results and limitations](RESULTS.md#native-call-result-conversions-2026-10-10).

Untyped LLVM local loads use `qore_rt_deref_if_reference` to resolve reference
nodes while returning all other representations unchanged. The helper checks
ordinary/opaque pointer tags and the node tag directly; it no longer invokes
generic scalar/enum classification. Reference evaluation, exception propagation
and ownership are unchanged. This is a runtime-only optimization without a new
switch or ABI change. The before/after library control uses the exact same AOT
executable after verifying matching runtime compatibility identities. Dynamic
call workloads improve, but the interpreter and generic fallback remain separate
optimization targets. See [measurements](RESULTS.md#direct-reference-tag-loads-2026-10-10).

Native unary dispatch also reuses the immutable cache's Program-equality proof:
preparation establishes one parameter and identical caller/receiver/execution
Programs before publication. A live receiver-Program check plus that witness
replaces redundant stored-key comparisons. Argument type/state, access context,
parse options, epoch, live body/readiness, debugger and stack guards remain.
This runtime-only change adds no switch or ABI field. Its measured gains are
small and not universal; see [results](RESULTS.md#native-program-proof-factoring-2026-10-10).

`QORE_DISABLE_AOT_NATIVE_FLOAT_METHOD_ENTRY=1` disables only the floating-method
bindings and caller continuations at compilation, and their native admission/
try-helper at runtime. Compile separate artifacts for this additive ABI change;
older runtimes do not provide the floating helpers. Eligible bodies have an exact
`float(float)` signature and a complete non-throwing, context-independent proof.
Native arguments/results remain doubles, including tag-colliding values; boxed
misses retain ordinary argument/return checking, ownership and exceptions.
Methods using self/argv, side effects, throwing operations or currently unsupported
mixed int/float expression summaries stay general. Existing integer bindings and
generated context offsets are preserved; three typed atomic pointers are appended.

Generic runtime method dispatch also invokes proven floating companions, including
callers whose result has no native local assignment. Actual floating arguments
are required; coercible integers retain ordinary argument checking. The runtime
prefers v3/v2 entries with explicit RuntimeConfig, while v1-only and uncached
admission retain eager receiver/class context. Prepared v2/v3 hits use the existing
compact frame and deferred context. An immutable PIC entry populated before native
admission can discover the live companion without mutating that entry.

`QORE_DISABLE_AOT_GENERIC_NATIVE_FLOAT_METHOD_ENTRY=1` disables only this generic
floating runtime path, leaving native-result caller continuations enabled. It is
a runtime-only control: `--reuse-aot-for-baseline` is valid for this follow-up when
both libraries support the existing float companion ABI. The general method-entry,
float-entry and continuation controls retain their documented behavior. No ABI
layout or compiler change is part of the generic-dispatch follow-up.
See the [native continuations](RESULTS.md#native-floating-method-continuations-2026-10-10)
and [generic floating dispatch measurements](RESULTS.md#generic-floating-method-dispatch-2026-10-10).

`QORE_DISABLE_AOT_NATIVE_METHOD_ADMISSION_CACHE=1` disables caching immutable
eligibility/signature/Program/class facts in polymorphic method entries without
disabling the native scalar companion itself. Mutable body/readiness, receiver,
argument and debugger guards remain active in either case. For this runtime-only
follow-up, `--reuse-aot-for-baseline` is valid when both saved and current runtimes
support `qore_rt_bind_method_int_unary_v1`; it isolates the runtime change with an
identical executable.

Native scalar admission facts can now be prepared before a callee's lazy AOT
context exists. They describe only the resolved signature and Program/class
identity; the live body, companion, self/argv, debugger and receiver checks still
decide whether a call can execute. `QORE_DISABLE_AOT_EARLY_NATIVE_METHOD_ADMISSION=1`
restores the requirement that a companion already exist when the immutable PIC
entry is built. Prepared admission misses proceed to the ordinary fallback
without repeating the same native attempt with uncached proofs.

Generic boxed-result integer method calls now use the existing v3/v2 companions
with explicit `RuntimeConfig` and stack-limit arguments, as floating calls do.
Prepared integer hits can use the same compact frame and deferred object/class
context; v1-only or uncached entries retain eager context. Set
`QORE_DISABLE_AOT_GENERIC_NATIVE_INT_CONTINUATION=1` to retain the integer v1
entry on this route. Both switches are runtime-only: there is no exported ABI,
header-layout or runtime-identity change, so compatible before/after libraries
can use an identical AOT executable. Context-using outer method bodies still
require ordinary parameter and self setup.

Native method object/class save/restore uses the already-acquired live
`RuntimeConfig`; proven pure method-v2/v3 continuations defer that work until
overflow materialization, while ordinary native methods retain eager setup.
The generic substitution helper is unchanged. Runtime-only
comparisons can reuse the same AOT executable. For a negative control, disable
the native entry with `QORE_DISABLE_AOT_NATIVE_METHOD_ENTRY=1`, not just its
admission cache: both prepared and ordinary native admission use the explicit
context helper.

The native method diagnostic-frame wrapper also uses explicit inlining hints,
matching the callback wrappers. No frame or guard is omitted. This runtime-only
change can use `--reuse-aot-for-baseline`; the same executable must run on both
libraries. The [frame-wrapper follow-up](RESULTS.md#native-method-frame-wrapper-inlining-2026-10-08)
records repeated nullable-call gains, overlapping control movements and code
growth in the single-compilation-unit macOS build. Its disabled-entry control
sets `QORE_DISABLE_AOT_NATIVE_METHOD_ENTRY=1` at both compilation and execution,
so it uses a different executable from the enabled runs, but one identical
executable within each before/after pair.

The deferred unary-integer method frame now borrows its immutable admitted PIC
entry for name, Program and statement metadata. It still publishes every call
and waits for stack readers before destruction. No dispatch guard or generated
ABI changes. The [compact-frame experiment](RESULTS.md#compact-native-method-diagnostic-frame-2026-10-10)
reuses identical executables on frozen before/after runtimes: three trials show
a 1.87–2.67% typed-polymorphic gain, but no repeatable expression-call gain.
Its raw-derived evidence includes every control, a baseline-only Java comparison,
and the runtime eager-frame reader/debugger controls.

`QORE_DISABLE_AOT_NATIVE_METHOD_DEFERRED_CONTEXT=1` selects eager private-context,
object/class and empty variable-marker setup for the method-v2 scalar continuation
at runtime. The native body, dispatch checks, published diagnostic frame
and stack guards remain enabled. This isolates deferral from native dispatch;
it can compare the same AOT artifact on the same runtime. The default defers
private state until an overflow needs it, using the compiler's whole pure-body
proof. Method-v1 and floating callback marker policy are unchanged. This is a
runtime-only change, so compatible baseline comparisons may reuse AOT artifacts.
The [object/class follow-up](RESULTS.md#deferred-objectclass-context-for-native-methods-2026-10-08)
records three nullable-call gains and an eager-control comparison. Its native
regression checks live private-member access and snapshot context after an actual
callee overflow, including restoration when the caller's two views differ.

`QORE_DISABLE_AOT_NATIVE_METHOD_INLINE_STACK_CHECK=1` at compilation omits the
additive method-v3 companion and keeps method-v2's full stack-guard helper.
The default companion loads the live limit from the invocation's RuntimeConfig
and compares it against its current native SP. Null views and failed comparisons
return through v2's full guard, including deferred-context materialization on
overflow. Unsupported stack layouts always use that full path. Diagnostic
frames, entry stack checks, receiver/access/Program/body/debugger guards and the
old v1/v2 ABIs remain. Compare separately compiled artifacts: new artifacts need
the v3 binder absent from old runtimes; do not use `--reuse-aot-for-baseline`.
For the internal-header `NativeLeafContext.qtest` fixture, provide dependency
include directories through the compiler's `CPATH` if needed (for example,
`CPATH=/opt/local/include` with a MacPorts dependency installation).
The fixture also probes actual warmed native method caches from a compiled
caller's parse-policy context: strong and opaque objects must be admitted, while
weak wrappers, scalars, boxed values, strings, containers and enums must reject
without changing the result, exception sink or caller context. Receiver admission
uses a local pointer/node-type check; the general QoreValue type API and all
subsequent method guards are unchanged.
Argument probes additionally require an integer continuation to miss an enum-only
cache, then hit a later integer entry while preserving ordinary enum dispatch.
The [argument-key experiment](RESULTS.md#native-method-argument-key-proof-rejected-2026-10-08)
records a publication-time proof flag that was rejected for lack of target benefit.
The [receiver-admission measurements](RESULTS.md#local-strong-object-method-admission-2026-10-08)
record repeated nullable/polymorphic gains against a merged method-v3 baseline,
using identical AOT artifacts for this runtime-only change.
The [measured follow-up](RESULTS.md#deferred-private-context-for-native-methods-2026-10-08)
records repeated nullable/polymorphic gains, same-runtime eager controls,
callback variability, stack/code growth and the remaining Java gap.

`QORE_DISABLE_AOT_DYNAMIC_CLOSURE_ENTRY=1` disables the dynamic native `int(int)`
closure companion at compilation and execution. Use it in both phases for an
attribution control: enabling compilation retains a normal native body to publish
the companion even when a noncapturing closure previously used the IR leaf path.
Paired compilation is required against runtimes predating the additive binding
and capture-read helpers. Older artifacts still execute on the new runtime.
The integer v3 callback continuation borrows each live closure without caching
or extending its lifetime. Each capture read remains separately synchronized;
references, unassigned captures, unsupported bodies, foreign Programs and attached
debuggers retain ordinary fallback. Disabled or absent native bindings are
rejected before unnecessary metadata/TLS acquisition. Presence flags are sampled
once at runtime, not toggled during measurement.

The early-native-admission and explicit integer-entry measurements are in
[RESULTS](RESULTS.md#early-native-admission-and-explicit-integer-method-entry-2026-10-11)
and [the full evidence](results-native-method-admission-macos-arm64.json).
The nested context-using workload improves roughly 4–6% across repeated runs;
its remaining cost is still about 66x the retained Java workload. This does not
establish a general call speedup or Java parity.

## Workloads and units

Engine provenance matters: upstream commit `5243c8aef` fixes unintended native
promotion under `--exec-mode=ir`. Earlier results record the requested mode,
not a verified pure-interpreter execution. A diagnostic on the saved pre-merge
`a3d430d06` runtime confirms JIT promotion of the polymorphic benchmark loop
and two target methods; the merged runtime produces no compilation trace for
the same invocation. AOT/Java comparisons are unaffected. Re-establish the IR
baseline after this merge; do not interpret the engine correction as an
optimization regression. See the merge/provenance note in `RESULTS.md`.

Each invocation processes `--count` elements, normally 1024. The reported unit
is nanoseconds per element, including all passes in that workload. One list
build therefore means appending one element plus its share of the final
checksum traversal; it is not a measurement of an isolated append instruction.

|!Kernel|!Work
|arithmetic|Dependent integer recurrence and checksum
|direct_call|Same recurrence through a small function
|method_call|Same recurrence through a method on a final class; receiver created outside timing
|indirect_call|Typed Qore function reference / Java LongUnaryOperator; callback created outside timing
|closure_call|Noncapturing Qore closure / Java functional object; created outside timing, passed into the callback loop
|closure_capture|Qore closure reading a captured integer / Java functional object reading a non-final integer field; no timed capture mutation
|indirect_wide|Same low-bit recurrence with bit 50 retained across the callback boundary; checksum uses low 16 bits
|indirect_float|Double callback with bounded addition/subtraction and an integer checksum
|indirect_int_result|Typed integer callback whose result is converted directly; native arguments advance independently of the result
|indirect_float_result|Typed double callback whose result is converted directly to an integer checksum; native arguments advance independently
|indirect_tagged_float|Double callback alternating -3e307 and -6e307, which require heap encoding when boxed in Qore
|indirect_affine|Integer callback adding 17, exercising the simple scalar-leaf summary
|indirect_affine_wide|The same callback with values above the inline 48-bit integer range; checksum uses low 16 bits
|indirect_branch_int|Integer comparison selecting one of two affine expressions
|indirect_branch_float|Float comparison selecting between two affine expressions through a retained callback
|polymorphic_call|Three receiver types selected cyclically into a typed receiver local each iteration, with different recurrence increments
|polymorphic_expression_call|The same three targets and selection used directly as the call receiver, without a per-iteration receiver local
|monomorphic_call|Non-final method with one unchanged exact-base receiver parameter; Qore may inline its arithmetic, while Java's target retains `DONT_INLINE`
|optional_method_call|Same stable receiver with Qore's nullable `*DynamicStep` contract; Java references already permit null and use the same non-final target
|polymorphic_float_call|Three non-final floating-point targets selected into a typed receiver local, with integer-exact subtraction recurrences
|polymorphic_float_expression_call|The same floating targets and selection directly in the receiver expression, without the per-iteration receiver local
|monomorphic_float_call|One unchanged exact-base floating receiver; Java retains its target call and Qore may inline the base body
|optional_float_method_call|Same floating receiver with Qore's nullable contract; only strong non-null inputs are timed
|boxed_float_method_call|Three auto receivers and an auto floating recurrence in Qore; Java retains native doubles and the same three non-final targets
|dynamic_int_result_call|Three auto receivers with native integer arguments and directly converted unknown results; the native recurrence is independent of each result
|dynamic_int_result_fallback|The same checksum through a context-using nested method excluded from native-leaf admission; measures failed-attempt overhead
|dynamic_float_result_call|Three auto receivers with a native float argument; each unknown method result is consumed directly by int conversion, with an independent native recurrence
|dispatch_control|Same receiver-selection arithmetic and increments without method calls
|list_read|Indexed traversal of a list prepared outside timing; XOR each element with the invocation seed
|list_build|Build a growing list, then read every element into the checksum
|list_mutate|Build a growing list, then increment and read every element
|temporary_list|Construct two elements per iteration and select with a runtime index
|temporary_hash|Construct a two-key hash per iteration and select with a runtime key

`polymorphic_expression_call` is a matched Java/Qore source-level decomposition,
not a replacement for `polymorphic_call` or a measured runtime optimization.
Their difference includes compiler lowering and ownership/cleanup effects; it
does not isolate an exact local-variable cost. Both use the same targets, seeds,
selection and recurrence. Java retains `DONT_INLINE` on all three target methods.

The four floating-method kernels mirror these receiver shapes using
`65535.0 - value`, `65537.0 - value` and `65541.0 - value`. Values remain exactly
representable doubles throughout the supported count/seed range; the independent
reference uses integer arithmetic and the checksum converts each result to an
integer. Receivers are created outside timed loops. Their initial addition was
measurement-only; the follow-up above implements the native floating path.
The Java methods retain `DONT_INLINE`;
Qore may inline its exact-base branch, so ratios are whole-kernel comparisons,
not isolated call latency or an equal-inlining-policy comparison. Exceptional
floating values and null receiver errors belong in separate correctness tests.

The generic-result kernels deliberately have no native result assignment contract.
The boxed floating recurrence stays on ordinary dispatch. Already-native integer
and floating arguments now permit a native try-helper followed by result boxing;
LLVM inspection verifies both that path and its retained ordinary fallback. The
boxed recurrence reuses the polymorphic float reference; the floating direct-result
kernel independently computes the selected target minus the current value and
advances `value = 65535.0 - value`. The integer direct-result kernels compute the
selected affine/masked result and independently advance `value = 65535 - value`.
The context-using variant delegates to the selected virtual target and has no native
companion. Java marks both the wrapper and targets `DONT_INLINE`.
All results are integer-exact; the full reference grid has 31 kernels and 465 rows
per mode. Java retains native scalar results, so these whole-workload comparisons
include Qore's broader dynamic representation and dispatch contract.

`monomorphic_call` exercises the cached exact-class check for an unwritten
receiver parameter. The generated AOT guard also checks receiver identity:
reflective writes can replace such parameters despite the absence of source
assignments. `QORE_DISABLE_AOT_STABLE_EXACT_RECEIVER_HOIST=1` at compilation keeps
per-call class checks for comparison. Compile separate baseline/current artifacts
for changes to this guard. Java retains the target call while Qore may inline
the exact-base arithmetic, so this kernel is not an equal-inlining-policy or
isolated call-latency comparison.

The runtime-only [receiver-validity follow-up](RESULTS.md#direct-node-receiver-validity-2026-10-08)
keeps that per-call live deletion check but skips generic scalar/enum type
dispatch for pointer-tagged receivers, including opaque strong references.
Non-object nodes still fail and non-pointer representations retain the original
generic path. The helper ABI and compiler are unchanged, allowing identical-AOT
before/after measurements. The measured monomorphic gain does not extend to all
method kinds; deleted/null/weak/error paths are tested but not timed.

`optional_method_call` times strong, non-null inputs only. It does not benchmark
null errors or reflective mutation; `AOTOptionalClassLocals.qtest` covers those
semantics separately. Older AOT artifacts can appear fast by missing reflective
updates, so compare the enabled cache with a separately compiled disabled cache
on the corrected runtime as well as with the old baseline.
The [name-method continuation follow-up](RESULTS.md#name-dispatched-native-method-continuations-2026-10-08)
compares the native continuation with separately compiled old and same-runtime
disabled artifacts. It retains the full nullable/name-dispatch contract and does
not claim that all optional or dynamic calls are native.

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

The closure workloads pass their callbacks as parameters to the same loop used
by `indirect_call`; construction and capture initialization are outside timing.
Java's two functional-object `applyAsLong()` targets have `DONT_INLINE`, just as
the function-reference targets do. A Qore captured variable has different
ownership/synchronization semantics from a Java non-final field; these are
matched computations, not identical contracts or a benchmark of capture mutation.

An AOT caller does not imply a native closure body: an eligible noncapturing
closure can execute through the bounded IR leaf evaluator. The caller retains
the runtime helper call. `QORE_DISABLE_IR_NATIVE_INT_PROGRAM_INLINE=1` disables
the composed integer evaluator; set it for compilation and execution when
comparing AOT closures, because admission also controls generic native-body
emission. QORD20 closure debugger metadata requires paired before/after AOT
compilation when the baseline predates that format.

The original direct/final-method kernels permit inlining. Indirect callbacks
retain calls in Qore AOT, as did the earlier polymorphic baselines. Java's indirect
and polymorphic target methods explicitly use JMH `CompilerControl(DONT_INLINE)`;
this is not a claim that default HotSpot would leave the calls uninlined. A Qore typed
call reference and a Java functional interface have different runtime contracts.
The polymorphic control measures the selection/recurrence work, but subtracting
it does not isolate an exact virtual-call cost.

With assigned receiver-class facts reused by guarded AOT method entry selection,
the exact base-class case in `polymorphic_call` may inline its arithmetic summary;
the two derived cases still dispatch normally. Such results measure partial
devirtualization, not three retained native calls. Java's three target methods
still have `DONT_INLINE`. Use `QORE_DISABLE_AOT_OBJECT_METHOD_FAST_ENTRY=1` at
compilation as a broad diagnostic control for this guarded path. Compiler changes
must use paired compilation, not `--reuse-aot-for-baseline`.

The float-branch callback now qualifies for a complete native comparison/select
summary; older compilers dispatch it generically. Its checksum and input are
unchanged. NaN comparisons and signed-zero selection retain Qore semantics;
throwing or side-effecting branches remain on the ordinary path. Polymorphic
dispatch and temporary hashes remain controls. `QORE_DISABLE_AOT_FLOAT_EXPRESSION_IMPORT`
disables this summary optimization at compilation.

Integer native callbacks also consume complete scalar-leaf summaries: six
nonthrowing binary operations, affine compositions and integer branch/select
bodies. Previously these summaries suppressed the general integer-expression
proof but were not used by native callback admission. Division, modulo and
side effects remain excluded. The affine/wide-affine/branch-int workloads
measure this admission gap using identical sources with both compilers.
`QORE_DISABLE_AOT_CROSS_OBJECT_LEAF_INLINE` retains the typed v1 adapter but
disables re-emission of these leaves into callee v2/v3. Disabling compact CFG
summaries instead allows the existing general-expression proof to handle
eligible branch bodies; it is not a blanket native-callback disable.

## Measurement contract

- Each selected mode first passes 300 checks: every kernel at sizes 0, 1, 17, 1024,
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
containing typed native callback or unary-integer native method calls. Other
functions are unchanged: broad guard insertion regressed direct-call and
dispatch-control loops. The enabled
pass runs after the normal optimizer and only simplifies locally afterward;
inserting these guards before the full pipeline caused excessive code growth.
Opaque references, heap numbers and other representations retain the original
release operation. `QORE_IR_OPT_STATS=1` reports the number of inserted guards.
This trades additional native code for fewer runtime cleanup calls; compare
code size and compilation time as well as steady-state performance.

`QORE_DISABLE_LLVM_METHOD_DECREF_GUARD=1` disables only method-caller selection
for this pass. Existing callback-selected functions remain eligible, including
functions with both call kinds. It is a compile-only control, requiring separately
compiled artifacts. No method dispatch/admission or result-coercion guard changes.
Owned fallback values are still released at the original point; only exact
NOTHING, int48 and bool representations skip the runtime cleanup call.

`QORE_DISABLE_LLVM_CANCEL_RUNTIME_CONFIG=1` at compilation selects legacy TLS
polling for generated JIT/AOT loops. The enabled path acquires a nullable,
borrowed live `RuntimeConfig*` once per invocation and passes it to
`qore_rt_check_cancel_v2`. Null selects the legacy path, including during
thread-data teardown. Polling frequency, deferral, cancellation priority,
Program scope and atomic ordering are unchanged. Uncommon exception/manager
work is outlined for both polling paths; this switch isolates context passing,
not that shared runtime improvement. Old AOT artifacts remain supported.

`QORE_DISABLE_LLVM_INLINE_CANCEL_POLL=1` at compilation retains the explicit
context helper call at every checkpoint. By default, generated code borrows
three flag addresses once per invocation and loads the live deferral depth,
thread request and global Program-interrupt count at each checkpoint. Both
request loads remain seq_cst; pending requests and unavailable/unsupported
native flag layouts use the full helper. Language-loop polling remains every
iteration and fused internal-loop polling retains its existing interval.
This switch isolates generated negative-path polling, not the interpreter.
Measure it separately from before/after compiler/library comparisons.

`QORE_DISABLE_AOT_FUNCREF_RUNTIME_CONFIG=1`
at compilation selects the original typed callback helpers instead of v2,
which acquires an explicit live execution context once per generated invocation.
Use separate shuffled runs with the same settings to measure this switch;
do not combine disabled-path measurements with before/after library labels.
V2 preserves diagnostic frames, policy overrides, stack-reader synchronization,
and lazy context materialization. It is not an end-to-end TLS-free native ABI.
An optional typed callee entry now receives RuntimeConfig after the AOT context
and exception sink, allowing its stack guard to avoid TLS lookup. It is emitted
only from the same complete non-throwing scalar summaries used for admission;
unavailable/disabled summaries keep the old entry. The original typed entry is
always retained. `QORE_DISABLE_AOT_FUNCREF_CALLEE_RUNTIME_CONFIG=1` suppresses
this additive entry at compilation, independently of the helper-version switch.
V2/v3 helpers prefer it when present; v1 helpers still use the original entry.
An optional callee-entry v3 additionally receives a live stack-limit address
and inlines its successful check. This callee version is separate from the
callback-helper versions. Only managed downward-growing 64-bit x86-64/AArch64
runtimes provide a non-null view; null/unsupported views and failed comparisons
tail-call the v2 entry with its full explicit-context guard. Keeping the slow
return separate avoids preserving arguments across a helper call on the hot
path. Limits and SP are read per call, not
snapshotted. `QORE_DISABLE_AOT_FUNCREF_INLINE_STACK_CHECK=1` suppresses this
entry while retaining callee v2. Existing helper/cache ABIs and old entries are
unchanged; newly bound v3 entries require the new runtime. Helper-entry and
callee-entry stack protection and overflow diagnostics remain.
Prepared calls to this re-emitted pure callee v3 defer same-thread context
save/restore until a stack guard actually fails. The full guard materializes
Program/closure/location/policy state before exception construction; successful
scalar bodies cannot access or mutate it. Published diagnostic frames and their
reader handshake remain active on every call. For integer/bool-result entries,
thread-private empty local/closure markers are also deferred until overflow
materialization; floating-result entries retain eager markers. The pure body
cannot observe these variable stacks or reenter Qore. Both markers are installed
before exception construction, with rollback if closure-marker allocation fails.
Older callee entries and helper v2 keep eager setup; disabling callee v3 tests
that path. This is not permission to defer context for generic or reentrant
callees. The prepared helper and callee each retain their entry stack guard;
only the redundant middle check during deferred-frame publication is omitted.
The frame is published before the callee checks the live limit, so overflow
still materializes context before exception construction. Eager legacy guards,
sink checks and cancellation frequency remain unchanged.
When local/closure markers are installed, compiler-proven scalar leaves
avoid general variable-cleanup bookkeeping for those empty frames. The native
closure-marker push handles an available slot inline and outlines full-block
growth/reuse; the entry fields and publication ordering are unchanged. V1 remains
available as a functional control; this does not broaden native eligibility.

`QORE_DISABLE_AOT_FUNCREF_PREPARED_TARGET=1` at compilation selects v2 instead
of v3's invocation-local prepared target. V3 caches immutable facts behind an
owned reference; Program/debugger/compiled-entry and lazy-readiness checks stay
live. A prepared slot belongs to one native signature, checked when its target
is populated. Prepared-only entry accessors assert that immutable fact in debug
builds instead of repeating the signature comparison on every release call.
Generic and non-prepared entry accessors retain their checks; a rejected target
or NOTHING leaves the prior valid prepared target available for later reuse.
It retains neither generic callbacks nor captured objects. Its first
word participates in persistent boxed cleanup and clearing it invalidates the
descriptor. Old v1/v2 artifacts remain supported; new v3 artifacts require the
updated runtime. Measure this switch separately from before/after library runs.

The v3 diagnostic frame borrows name, statement and Program metadata from that
descriptor, reducing per-call copies. The non-reentrant scalar-body proof and
existing reader handshake keep the borrowed descriptor valid until the frame
has been unpublished and readers finish. Caller location, state restoration,
live guards and the versioned descriptor layout are unchanged.

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

Generic AOT method-cache hits reuse the immutable diagnostic `Class::method`
name already stored in their PIC entry when no cached IR display name exists.
The IR name keeps precedence, misses still construct the ordinary name, and
`execJITWithDeopt` still copies it into an owned diagnostic frame. This removes
temporary string work without changing dispatch checks, frame-reader lifetime,
ABI or runtime identity. See the [measurements](RESULTS.md#generic-method-name-reuse-2026-10-10)
and [evidence](results-generic-method-name-macos-arm64.json), including identical-
executable controls, mixed native-call results and the remaining Java gap.

Generic AOT method-cache hits also reuse static user-method eligibility and an
all-arguments exact canonical `int`/`float`/`bool` binding proof. Boxed-value
ownership, ordinary TLS locals, context and live dispatch guards remain intact.
These facts are computed once when a resolved entry is published; this does not
yet pass generic-method arguments directly through a native calling convention.
`QORE_DISABLE_AOT_GENERIC_METHOD_ADMISSION_CACHE=1` disables both proofs;
`QORE_DISABLE_AOT_SCALAR_PARAM_BINDING_CACHE=1` disables only the binding proof.
Both switches act at runtime cache construction. The internal PIC layout changes
AOT runtime identity: rebuild artifacts instead of swapping old/new runtimes
under the same executable. Same-executable controls use one candidate runtime.
See [results](RESULTS.md#generic-method-binding-proofs-2026-10-10) and
[evidence](results-generic-method-binding-macos-arm64.json) for the repeated
10–12% fallback reduction, code-size/stack growth, and adverse controls.

Pre-resolved AOT object calls now reuse the same bounded dispatch cache without
re-resolving the parse-selected overload. Exact method/variant identity and the
full receiver, context, Program, parse-option, epoch and argument key must match.
Live body readiness, debugger, object validity, ownership and frame checks remain.
`QORE_DISABLE_AOT_RESOLVED_METHOD_CACHE=1` disables this route at process startup;
the existing polymorphic-cache and fast-entry switches also disable its use.
There is no exported ABI, header layout or AOT runtime-identity change.
Hot native-dispatch and key-building helpers remain inline to avoid slowing
existing boxed calls when the additional call site is introduced. See the
[results](RESULTS.md#resolved-aot-method-entries-2026-10-10) and
[evidence](results-resolved-method-entry-macos-arm64.json): the nested-call
workload improves about 15%, with small adverse control medians retained. This
does not eliminate generic outer-call setup or establish Java parity.

`QORE_DISABLE_AOT_STABLE_UNTYPED_LOAD=1` disables invocation-local caching of a
stable untyped parameter's non-reference classification. Set it during `qcc`
compilation, not just execution. The compiler excludes written, captured,
reference-addressed, weak-assigned and AST-visible locals, and rejects opaque
AST local access. Read-once parameters in loop-free functions stay on the ordinary
path; the loop-presence profitability test is conservative and function-wide.
Only the classification is cached: current local bits, freshness checks and
ownership remain live, while actual references are evaluated on every original
read. The new internal `qore_rt_deref_stable_local_v2` returns its classification
without exposing the flag address, allowing LLVM to keep the flag in registers.
The v2 suffix distinguishes uncommitted experimental artifacts. The internal
header changes AOT runtime identity, so baseline comparisons compile separate
executables; same-runtime controls compile with the switch off/on. Do not run a
new artifact against the old runtime by bypassing its identity check.
See the [measurements](RESULTS.md#stable-untyped-parameter-loads-2026-10-10) and
[full evidence](results-stable-untyped-load-macos-arm64.json), including rejected
experiments and adverse controls. This is an AOT optimization, not an IR speedup
or Java-parity claim.

The [object-admission follow-up](RESULTS.md#object-admission-specialization-2026-10-09)
specializes strong/opaque node checks without changing caller code. Its identical-
executable before/after measurements show a repeated polymorphic-kernel benefit,
not Java parity or an isolated virtual-call cost. Receiver-local ownership and
cleanup remain included. Raw normalized samples, exact binary identities and
separate profile observations are in
[the evidence](results-object-admission-macos-arm64.json).

The [empty-result-holder follow-up](RESULTS.md#empty-class-result-holder-2026-10-09)
removes replacement cleanup only for a proven-NOTHING initializer output. Three
identical-executable comparisons show modest 1.37–1.80% polymorphic median gains,
with overlapping fork ranges; RAII cleanup and both object references remain.
See [normalized evidence](results-class-holder-macos-arm64.json) for all controls,
binary identities and validation. This does not establish Java parity.

`QORE_DISABLE_AOT_SCALAR_RETURN_GUARD=1` disables the exact-immediate scalar
return guard during `qcc` compilation. For canonical `int`, `float` and `bool`
contracts, an already-correct immediate value avoids full runtime return-type
resolution/filtering. References, heap-boxed values, conversions and other
contracts retain the full filter; both edges retain exception checking and
cleanup. This is AOT-only. The compiler change updates runtime identity despite
an unchanged exported ABI, so compile baseline/candidate artifacts separately
against their matching runtimes. See the
[results](RESULTS.md#aot-exact-immediate-scalar-returns-2026-10-11) and
[evidence](results-scalar-return-guard-macos-arm64.json): the nested workload
improves repeatedly by about 9-10%, but remains far from Java. Adverse boxed-float
and monomorphic comparisons are retained; this is not a zero-regression claim.

`QORE_DISABLE_AOT_RECEIVER_METHOD_ENTRY=1` disables the receiver-aware forwarding
companion at compilation or its use at runtime (presence, sampled once at runtime).
Eligible same-Program `int(int)` instance methods that only forward their explicit
receiver and argument to another method can receive those inputs and a live
`RuntimeConfig` directly. The caller still installs the ordinary logical frame,
defining-class/object context and reflection-visible signature slots. Arbitrary
overrides, cleanup and full return filtering remain supported; this is not the
pure-leaf convention. Debug-enabled companions have separate native PC maps, and
are published only after their map is attached. No-debug bodies keep eager locations.

The internal context layout/runtime identity changes: rebuild AOT artifacts against
the matching runtime. See the [results](RESULTS.md#receiver-aware-forwarding-entry-2026-10-11)
and [evidence](results-receiver-entry-macos-arm64.json): the nested workload gains
10-12%, but remains about 53x Java. An integer-callback slowdown repeats at 1.09%
against the frozen baseline; all adverse controls are retained.
