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
read-only class parameter caches, including the `auto` extension below.
For declared class parameters, admission requires unwritten,
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

`QORE_DISABLE_AOT_OBJECT_PARAM_CACHE=1` at compilation disables the extension
to ordinary read-only `auto` parameters used directly as method receivers.
An existing entry owner, runtime-backed reload support and an identical strong
runtime slot are still required. Direct receiver use predicts profitability,
not object type; non-object, reference and weak slots keep normal reloads.
Reflective writes revoke reuse before reentry. Scalar/container-only parameters
and private direct-entry arguments are excluded. This AOT-only extension uses
the existing helper ABI; recompile artifacts to benefit. The class-cache switch
above also disables it. No IR/JIT or private-fast-call gain is claimed.

The [ordinary-entry companion](ordinary_params.qr) forces ordinary entries with
`call_function`. Its CLI is `check` or `NAME COUNT WARMUP SAMPLES MS SEED`, with
names `int`, `float`, `string`, `list`, `hash` or `object` followed by `_loop`
or `_short`. Each loop iteration has two reads and one retained callback;
object reads are field-reading method calls. Short cases enter a function on
every iteration. Reference totals are `2*n*seed + n*(n-1)/2` for loops and
`2*n*seed` for short cases. Results are whole-workload ns/iteration, not isolated
call latency. There is no Java counterpart for this companion.

For a manual smoke check, compile enabled and disabled artifacts separately
with the same compiler and matching runtime loader path. Set the switch only
on the disabled compilation, not at execution:

```sh
ordinary_probe_dir=$(mktemp -d /tmp/qore-ordinary-params.XXXXXX)
env DYLD_LIBRARY_PATH="$PWD/build" build/qcc -O3 --strip-source -o "$ordinary_probe_dir/ordinary-onx" bench/runtime-comparison/ordinary_params.qr
env DYLD_LIBRARY_PATH="$PWD/build" QORE_DISABLE_AOT_OBJECT_PARAM_CACHE=1 build/qcc -O3 --strip-source -o "$ordinary_probe_dir/ordinary-off" bench/runtime-comparison/ordinary_params.qr
env DYLD_LIBRARY_PATH="$PWD/build" "$ordinary_probe_dir/ordinary-onx" check
env DYLD_LIBRARY_PATH="$PWD/build" "$ordinary_probe_dir/ordinary-off" check
```

These commands use task-owned output paths and print reference outputs; the
recorded experiment independently checks all 144 totals. Its measurements use
frozen compiler/runtime identities, randomized independent forks and A/A
controls, not a single smoke run. See [the gain and control results](RESULTS.md#aot-object-valued-auto-parameter-cache-2026-10-11).

Ordinary runtime-backed boxed AOT parameters/body locals must reload after
potentially mutating calls even when every source access is IR-lowered.
Their stores must synchronize the same runtime slots; private direct-entry
arguments and native allocas retain their separate contracts. The
`AOTRuntimeLocalReload` regression covers replacement on return/throw, ownership
and assignment synchronization in actual stripped native entries. Recompile old
AOT artifacts to receive this correction; no helper ABI changes. The measured
reload cost is a correctness tradeoff, not a claimed performance improvement.
Pure-callee proofs do not by themselves authorize eliminating reloads when
caller-side destructors or deferred cleanup can reenter Qore.

`QORE_DISABLE_AOT_INLINE_RELOAD_GUARD=1` disables the compiler-side freshness
comparison before the stale-local helper. With the guard enabled, equal
invocation-local epochs bypass only the helper's existing no-op path; stale
values still use the complete runtime reload and deferred ownership cleanup.
Reflective class-parameter invalidation runs first. This AOT-only optimization
does not remove caller invalidation or change IR/JIT lowering. The switch is
compile-time only: recompile separate artifacts for on/off measurements, using
the same runtime environment. See the [measurements and noisy controls](RESULTS.md#aot-inline-local-cache-freshness-guard-2026-10-11).

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
arguments use the additional representation guards described below. Every miss retains original
dispatch, arbitrary boxed returns, return checks, caller-local invalidation and cleanup.
The broader native-method entry/continuation controls also disable this path.

`QORE_DISABLE_AOT_NAME_METHOD_BOXED_RESULT=1` disables the additional non-store
consumers at compilation, including direct scalar conversions of name-method
results, while preserving exact native-local continuations.
Compile separate artifacts; this is not a runtime control. For already-native
arguments, the path reuses existing scalar helpers and the caller's hoisted
RuntimeConfig; no additional runtime ABI or whole-body eligibility change is needed.
Unsupported targets can pay an unsuccessful
native attempt before fallback, so include `dynamic_int_result_fallback` in comparisons.

`QORE_DISABLE_AOT_NAME_METHOD_BOXED_ARGUMENT=1` disables the compiler's additional
immediate-scalar argument guards for name-dispatched unary methods. Compile separate
artifacts; this is not a runtime switch. The guard accepts only an actual immediate
integer or double, without coercion; heap-backed scalars, enum/reference wrappers,
weak values, NOTHING and other representations retain ordinary dispatch. A native
hit still requires the full live receiver, access, Program/options, epoch, body,
typed-companion and debugger checks. Because the argument family is unknown at
compile time, both successful scalar branches produce an owned boxed result, never
a guessed native-result side channel. Ordinary and invoke/EH callers retain local
invalidation, result ownership and exception handling.

The additive `qore_rt_try_method_{int,float}_unary_v2` helpers also reuse a fully
validated PIC match when a context-using body or missing typed companion needs
ordinary execution. Their result code distinguishes an untouched miss, native
result and owned boxed result. The ordinary path keeps parameter coercion, return
filtering, observable frames/locals and object/class context; it does not treat a
non-leaf body as a native scalar leaf. No negative body-eligibility fact is cached.
`QORE_DISABLE_AOT_MATCHED_SCALAR_FALLBACK=1` disables only this matched-fallback
reuse at runtime, leaving native leaf calls unchanged. This control can use an
identical candidate executable/library. The new helper declarations change the
private runtime identity, so comparisons against older builds require separately
compiled, matching artifacts. The historical `boxed_int_method_fallback` and
`boxed_float_method_fallback` names denote object-field reads; these can now use
the guarded member companion below. `boxed_int_method_effect` performs observable
member writes and measures an ordinary fallback, including all receiver counters
in its checked result. Counters are reset for each kernel invocation.
See the [boxed-argument measurements and remaining costs](RESULTS.md#aot-boxed-scalar-arguments-and-matched-fallback-2026-10-11).

`QORE_DISABLE_AOT_SCALAR_MEMBER_ENTRY=1` disables the independent whole-body proof
for AOT unary scalar methods containing exactly one live member read and otherwise
only homogeneous scalar arithmetic. It is both a compiler and runtime control.
The helper retains receiver/access/Program/body/debugger guards, synchronized field
reads, observable frames and explicit RuntimeConfig stack protection. It borrows
values only under the object lock; enum, reference, missing and mismatched fields
fall back without evaluating or taking ownership. Side effects, multiple reads,
unsupported operations and synchronized methods retain ordinary execution.

`QORE_DISABLE_AOT_SCALAR_MEMBER_PREHASH=1` separately disables compile-time member-name
hashes, or routes generated reads through the original runtime helper. Both target
widths are emitted; field values and access context are still read live. The general
compiler switch `QORE_DISABLE_AOT_PREHASHED_KEYS=1` also suppresses generation.

`QORE_DISABLE_AOT_SCALAR_MEMBER_CONTEXT=1` disables a separate positive declaration
proof: a locally declared `private:internal` member uses the live method variant's
defining class as its storage context, independent of receiver subclasses or
shadowing. The compiler can therefore omit the per-read member-context lookup.
No class pointer or mutable value is embedded or cached, and the ordinary object
lock and actual scalar checks remain. Public, ordinary private, inherited non-local
and unknown declarations keep normal lookup. The control works at compile time
and runtime; disabling member prehash also restores normal context lookup.

`QORE_DISABLE_SELF_MEMBER_INT_UPDATE=1` is a runtime-only control for direct
self-member increment/decrement helpers. A locked inline integer whose result
also fits inline storage can bypass generic lvalue bookkeeping. All other values
continue with the same locked lvalue, including reference resolution. The control
does not disable removal of duplicate object-context bookkeeping or the separate
scalar-method context-policy specialization, so on/off is not a control for the
entire change. Set diagnostic environment controls before process startup; this
one is sampled on the first representation-eligible update. AOT and JIT code
using the existing self-member helpers can benefit from the updated runtime;
the IR interpreter is unchanged. The final AOT measurements include typed/auto
integer, float, number and reference-backed update workloads, read-only methods,
plain methods, dynamic-result and indirect-call controls, including adverse results.
See [measurements and remaining regression](RESULTS.md#aot-self-member-updates-and-scalar-context-layout-2026-10-12).

`QORE_DISABLE_AOT_EFFECT_MEMBER_ENTRY=1` disables a separate AOT companion for
bounded standalone member-update prefixes followed by a scalar/member expression.
On a live representation miss, its ordinary continuation skips only completed
updates; it never replays their effects. Access, deletion and per-statement object
locks remain. Runtime parameters are visible during cold reentry, and the ordinary
callee frame covers return validation. The deferred-context disable also prevents
publication of this companion; read-only companion controls are independent.

`QORE_DISABLE_AOT_EFFECT_MEMBER_PREHASH=1` disables only compile-time update-name
hashing, or restores ordinary lookup in the new helper at runtime. It does not
disable the update companion. The global compiler prehash switch also suppresses
these hashes. Set all diagnostic controls before process startup. These changes
require recompilation with the matching runtime; IR/JIT timing gains are not claimed.
See [qualified measurements and remaining regressions](RESULTS.md#aot-member-update-prefix-candidate-2026-10-12).

Compile separate artifacts when comparing compiler changes or different private
runtime identities. Runtime-only on/off controls may use an identical executable.
For small timing differences, include identical-executable A/A controls and account
for environment/library-path differences; neither a median shift nor separated
observed ranges alone establishes its cause. Timings are whole-workload ns/element,
not isolated call latency or a claim of Java-level performance.

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

Native scalar PIC admission also records one immutable comparison key for the
Program and actual canonical argument kind (int or float), provided the argument
state is zero. The key never substitutes a coerced signature for the supplied
argument: enum keys retain ordinary checks, and an integer supplied to a floating
method does not authorize a floating continuation. All live receiver, access,
epoch, body/readiness, typed companion and debugger guards remain active.
`QORE_DISABLE_AOT_NATIVE_SCALAR_KEY=1` disables construction of this proof at its
first eligible admission; missing proofs use the original Program/type/state
comparisons. This private cache layout change requires separately compiled,
matching before/after AOT artifacts. A same-binary flag comparison can reuse the
candidate artifact to isolate the key's effect.

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
|boxed_int_method_call|The corresponding boxed integer recurrence; Java retains native integers and the same three non-final targets
|boxed_int_method_fallback|Boxed integer recurrence through three methods reading non-final per-object fields, requiring real receiver context
|boxed_float_method_fallback|The corresponding boxed floating recurrence with per-object field reads
|dynamic_int_result_call|Three auto receivers with native integer arguments and directly converted unknown results; the native recurrence is independent of each result
|dynamic_int_result_fallback|The same checksum through a nested forwarding method; originally a failed-native-attempt control, now eligible for independently proven forwarding entry
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
Boxed integer and floating recurrences now guard the actual immediate argument
representation before trying the matching native helper. Already-native integer
and floating arguments also permit a native try-helper followed by result boxing;
LLVM inspection verifies these paths and their retained ordinary fallback. The
boxed recurrence reuses the polymorphic float reference; the floating direct-result
kernel independently computes the selected target minus the current value and
advances `value = 65535.0 - value`. The integer direct-result kernels compute the
selected affine/masked result and independently advance `value = 65535 - value`.
The context-using variant delegates to the selected virtual target. Its whole-body
forwarding proof now admits the warmed exact inner scalar leaf without treating
arbitrary context-using bodies as scalar leaves. It is no longer a negative
native-admission control. Java marks both the wrapper and targets `DONT_INLINE`.
The boxed integer recurrence uses the same reference as the polymorphic integer
recurrence. Both boxed fallback kernels instead call `fieldNext()`, which reads
non-final per-object state and cannot use the native scalar leaf entry or the
forwarding shortcut. They measure a natural native-admission miss; disabling
native calls alone would not model the same live dispatch checks. Fields are
initialized outside timing and are not mutated during measurements. Java retains
native scalars and `DONT_INLINE` on all six field-reading targets. Adding these
fields changes benchmark object layouts, so comparisons must use fresh matched
artifacts; previous result grids remain associated with their recorded sources.
All results are integer-exact; the full reference grid has 36 kernels and 540 rows
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
LLVM cleanup guards for immediate int48, bool, NOTHING and inline floating-point
values in AOT functions containing typed native callback or unary integer/float
native method calls. Other
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
NOTHING, int48, bool and inline-float representations skip the runtime cleanup call.

`QORE_DISABLE_LLVM_FLOAT_DECREF_GUARD=1` at compilation selects the original
NOTHING/int48/bool predicate while retaining the same callback/method caller
selection. Like the other compiler controls here, presence selects the disabled
path, including a value of `0`. Recompile AOT artifacts to compare it; there is
no new runtime flag check. The enabled predicate keeps the integer tag exit
first, then recognizes the exact union of the original allowlist and
`QoreValue::isFloat()`. The short-string and opaque-reference tag families,
heap-encoded doubles/integers and plugin representations keep the existing
helper call. This is not a general non-pointer cleanup exemption. Cleanup order,
caller invalidation and cancellation checkpoints are unchanged.
`QORE_IR_OPT_STATS=1` also reports `scalar-cleanup-float-guards`; this counts
inserted guards using the expanded predicate, not floating-point executions.
The pass is AOT-only; no IR/JIT speedup is implied. The all-scalar control above
disables the entire pass, and the independent method control still leaves
callback-selected functions eligible. Retain short-invocation, code-size and
adverse-workload comparisons as well as the long-loop float gains; see
[the measured tradeoffs](RESULTS.md#aot-integer-first-inline-float-cleanup).

`QORE_DISABLE_LLVM_SCALAR_REFERENCE_GUARD=1` at compilation disables an independent
late AOT pass that bypasses `qore_rt_deref_if_reference` for non-pointer values in
native scalar callback/unary-method callers. Presence, including `0`, disables
the pass; recompile to compare it. There is no new runtime switch or ABI.
The predicate exactly matches `QoreValue::isPointer()`: the ordinary pointer
tag and the entire opaque tag family retain the original helper. Every other
representation is returned unchanged by that helper, so its call can be skipped.
This is not an ownership exemption: non-pointer enums, short strings and plugin
representations may still require cleanup. Reference evaluation, ownership,
caller invalidation, context and cancellation behavior remain unchanged.
The pass runs after the main optimizer and scalar-cleanup pass, then simplifies
only changed functions. Scalar/method/float cleanup controls do not disable it.
`QORE_IR_OPT_STATS=1` reports `scalar-reference-guards` and
`scalar-reference-removed` (compile-time sites, not execution counts).
No IR/JIT improvement is claimed. See the measured long-loop benefits,
short-invocation limits, pointer controls and code growth in
[the scalar reference guard results](RESULTS.md#aot-scalar-reference-resolution-guard).

`QORE_DISABLE_AOT_SCALAR_BOUNDARY_REUSE=1` at compilation retains v2 prepared
auto-local stores and omits scalar-publication state. Enabled AOT code can retain
one exact, still-current local cache at a temporary boundary after an admitted
scalar store. A pre-optimization effect pass revokes the witness before unknown
calls, owning cleanup and external writes; this is not a pure-callee assumption.
Runtime presence at first v3 helper use instead disables witness reporting only,
with ordinary assignment unchanged. Empty values and `0` also count as present.

`QORE_DISABLE_AOT_SCALAR_BOUNDARY_LIVENESS=1` is compile-only: it restores broad
early cleanup-guard selection for comparison. The default intersects publication
reachability with boundary liveness; it does not relax effect barriers.
`QORE_DISABLE_LLVM_SCALAR_DECREF_GUARD` disables these early scalar no-op guards,
and `QORE_DISABLE_LLVM_FLOAT_DECREF_GUARD` narrows their predicate. The independent
method-cleanup control still governs late caller selection, not early intervals
selected by the boundary pass. `QORE_IR_OPT_STATS=1` reports
`scalar-boundary-functions`, `cleanup-guards`, `barriers` and
`dead-cleanup-candidates`; these are compile-time counts, not execution counts.
Measure separately compiled artifacts for compiler controls and the same artifact
for runtime-off. Keep matching compiler/runtime pairs, A/A repetitions, pointer
and short-invocation controls. No IR/JIT gain is claimed. See
[the scalar-boundary measurements](RESULTS.md#aot-scalar-publication-boundary-reuse).

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


`QORE_DISABLE_METHOD_SCALAR_RETURN_GUARD=1` disables the runtime method-wrapper
return shortcut (presence sampled on first use). It accepts only immediate int/float/bool
values under their exact canonical declared type; every other value/type retains the
existing substitution/coercion/error path. It does not remove frame, argument cleanup
or Program/object context setup. Unlike the AOT compiler return guard, this applies to
the generic fast method wrapper used by both IR and AOT. No ABI change is introduced,
so one AOT executable can be reused with the saved compatible before/after libraries.
An additional same-binary control toggles only this flag for the before processes.
See the [measurements](RESULTS.md#exact-scalar-returns-in-the-method-wrapper-2026-10-11)
and [verified evidence](results-method-return-guard-macos-arm64.json).

`QORE_DISABLE_AOT_FORWARD_LEAF_PREFLIGHT=1` disables the runtime forwarding-leaf
shortcut (presence, sampled on first use). The compiler publishes the sole call
slot and source location only after proving the entire outer body is
`int forward(int value) { return self.method(value); }`. A warmed inner dispatch
cache must then identify an exact, ready, same-Program, context-independent
`int(int)` native-v2/v3 leaf under the outer method's defining-class context.
The preflight also checks live body identity, receiver/type, dispatch epoch,
parse options, argument state, debugger status, and parse-selected overloads.

Only immediate integer inputs enter this shortcut. An absent proof, cache miss,
different Program/options, reference, wide integer input, reflective override,
context-using body, or unavailable native companion retains ordinary dispatch
without changing arguments, ownership, TLS, or the exception sink on the miss.
Successful calls skip outer TLS signature slots and Program/object substitution,
but retain both logical stack frames and the explicit inner call site. The pure
leaf's failing stack guard materializes both contexts in caller order before
raising; native companions without PC maps are not counted as mapped frames in
exception location recovery. No arbitrary receiver-aware body is marked pure.

This changes compiler metadata and internal context layout/runtime identity;
compile baseline/candidate AOT artifacts separately against their matching
runtimes. The disable flag supports an additional same-binary/artifact control.

See the [forwarding measurements](RESULTS.md#preflighted-same-program-forwarding-leaves-2026-10-11)
and [verified evidence/audit](results-forward-leaf-preflight-macos-arm64.json).

`QORE_DISABLE_AOT_NATIVE_FORWARD_LEAF=1` disables only entry from the typed native
method helper into forwarding preflight (presence sampled on first use). The
boxed forwarding fallback remains available. Native entry passes the caller's
existing `RuntimeConfig` and full-width `int64_t` argument/result directly; boxed
entry still accepts only immediate integer inputs. Both use the same whole-body
proof, full inner dispatch key, logical frames and cold context materialization.
Neither path copies `RuntimeConfig`. A same-binary control for this flag isolates
native routing, not the additional removal of the old allocating configuration
copy; the full change requires a frozen before/after library comparison.

See the [native-forwarding measurements](RESULTS.md#native-forwarding-continuation-and-borrowed-configuration-2026-10-11)
and [verified evidence/audit](results-native-forward-entry-macos-arm64.json).

`QORE_DISABLE_AOT_FORWARD_CLASS_CONTEXT_CACHE=1` retains per-call defining-class
access normalization in forwarding preflight (presence sampled on first use).
The default caches that normalization when the full-key native method entry is
prepared. The cached value is used only for the inner dispatch key; deferred
exception context still uses the original defining class. Live receiver/type,
Program/options, dispatch epoch, body, debugger and readiness checks remain.
C++ base-class addition APIs advance the dispatch epoch, so an existing warmed
entry misses and is rebuilt after a hierarchy change. This adds internal PIC
metadata and changes the AOT runtime identity: before/after comparisons need
separate AOT artifacts compiled against their matching runtimes. The flag gives
an additional same-binary control for reusing the normalized context.

See [cached forwarding context measurements](RESULTS.md#cached-forwarding-class-context-2026-10-11)
and [verified evidence/audit](results-forward-access-context-macos-arm64.json).

`QORE_DISABLE_AOT_SCALAR_LOCAL_STORE=1` disables the runtime shortcut for normal
AOT assignments that replace an already assigned scalar in an ordinary `auto`
local (presence sampled on first use). Only immediate int/float/bool inputs and
existing scalar storage qualify. Captured, reference, typed, substituted,
read-only, top-level, unassigned, cleared, static and fixed-storage cases retain
normal assignment, as do weak/opaque/node inputs and replacement of owned nodes.
The shortcut updates the actual runtime slot; it does not introduce a separate
unobservable local cache. It does not change the interpreter's assignment path.

`QORE_DISABLE_AOT_DIRECT_SCALAR_STORE=1` independently restores generic scalar
storage after those same guards. By default, proven primitives are extracted
without conversion and copied directly into the runtime slot; assignment and
ownership flags are unchanged. The broader scalar-local control takes precedence.
Both controls use one immutable, thread-safe first-use configuration; presence
(even an empty value or `0`) disables the selected shortcut. Set them before
process startup. Declaration checks remain per-store unless the invocation
prepares declaration admission as described below. Ordinary lookup is retained
unless an invocation-local auto slot is prepared.
The private-header change alters the AOT runtime identity, so compare matching,
separately compiled before/after artifacts; candidate A/A and runtime on/off
controls can use an identical executable. This does not change IR/JIT local
assignment, widen fast-path admission or establish Java parity.

`QORE_DISABLE_AOT_AUTO_LOCAL_SLOT=1` disables preparation of ordinary `auto`
body-local storage. During compilation it omits the getter and prepared-slot
assignment helper. At runtime, presence sampled on the getter's first use makes
it return null, so the identical executable performs ordinary lookup on every
assignment. Empty values and `0` also disable preparation. This control does
not disable direct primitive copying, declaration/live-slot guards or the
fast-entry body-pop correctness correction; the broader scalar-local control
still takes precedence over the primitive shortcut.

By default, eligible AOT entries acquire the actual body-slot address after
instantiation and pass it to checked assignments. The address is invocation-local
and survives recursive stack growth, but not function exit. Values, flags and
owners are never cached by this mechanism; runtime publication, reflection,
reloads, generic fallback and destruction remain. Parameters, captured/typed
locals, private IR-only storage and no-coerce assignments are excluded.
Direct fast entries pop their own body slots on exit; wrapper-owned entries
retain wrapper cleanup. New versioned helpers and the runtime identity change
require separate matching before/after compilation. A runtime on/off comparison
isolates prepared lookup, not all code-generation or cleanup changes.

See [prepared auto-slot measurements and compile-time controls](RESULTS.md#prepared-ordinary-auto-aot-body-slots-2026-10-12),
[all verified samples](results-aot-auto-local-slot-macos-arm64.json), and the
[62-check audit](FOLLOWUP-AUDIT.md#prepared-ordinary-auto-aot-body-slots-audit-2026-10-12).
Boxed-call gains repeat, but list-construction and native-float adverse results
remain explicit; this is not a universal speedup or a Java-parity claim.

`QORE_DISABLE_AOT_AUTO_LOCAL_DECLARATION=1` independently disables preparation
of immutable auto-store declaration admission. At compilation it selects the
v1 storage-only getter and checked store. At first v2 store use, runtime presence
instead selects those same per-store declaration checks with the identical
prepared storage. Empty values and `0` also disable admission preparation.

The default v2 getter checks the complete declaration after local instantiation
and the existing same-Program storage check. Only ordinary writable, noncaptured,
non-substituted `auto` body locals qualify; parameters, self, top-level, read-only,
no-narrow and private IR-only locals remain excluded. These declaration facts
are complete before the body executes. The compiler passes that invocation's
borrowed handle to matching v2 stores; it never caches the value or its owner.

Every store still checks the incoming immediate representation and the live
assigned/finalized/cleared/static/fixed/storage-type state. Misses retain ordinary
assignment, ownership and cleanup. Actual runtime-slot publication, reflection,
reloads and exception handling are unchanged. A null handle uses v1 handling.
Slot, scalar-store, direct-store and reload controls remain independent; v1
helper contracts remain available. New helper symbols and runtime identity
require matching, separately compiled before/after artifacts. This changes AOT
lowering and its store helpers; no IR/JIT speedup or Java parity is claimed.

See [declaration-preparation measurements and matched Java comparison](RESULTS.md#prepared-aot-auto-store-declaration-admission-2026-10-12),
[all six matrices](results-aot-auto-declaration-macos-arm64.json), and the
[62-check audit](FOLLOWUP-AUDIT.md#prepared-aot-auto-store-declaration-admission-audit-2026-10-12).
Boxed-call gains repeat; short-invocation results and adverse native, indirect,
member-update and list controls remain explicit.

`QORE_DISABLE_AOT_PREPARED_LOCAL_RELOAD=1` independently disables prepared
body-local access for stale AOT cache reloads. During compilation it omits the
reload-slot getter and prepared-reload helper. At runtime, presence sampled on
the getter's first use makes it return null, so the identical executable uses
ordinary local lookup. Empty values and `0` also disable preparation. This flag
does not disable store-slot preparation, the inline freshness guard, reflective
invalidation or body-slot cleanup. Store-slot controls do not disable reload
preparation either.

Eligible ordinary body slots are resolved after this invocation's local
instantiation and retained only until its cleanup. Unlike auto-store admission,
reading does not require an auto declaration or a writable local. Parameters,
self, captures, top-level and IR-only storage are excluded; existing native,
weak-assigned and reload-exempt exclusions still apply. Null handles retain the
ordinary fallback. The handle caches storage, not its value or ownership.

The v1 prepared helper evaluates the live slot and its references. It
snapshots the old owners before evaluation, publishes the new cache and owner
chain before releasing the old deferred value, and publishes the completed
epoch last, including when cleanup raises an exception. It does not remove
reloads or change reference-counting and destruction semantics. New versioned
helpers and the runtime identity require matching, separately compiled
before/after artifacts. This is AOT-specific code generation; an IR/JIT speedup
has not been measured for this change.

See [prepared-reload measurements and matched Java comparison](RESULTS.md#prepared-runtime-backed-aot-local-reloads-2026-10-12)
and [verified evidence/audit](results-aot-prepared-local-reload-macos-arm64.json).

`QORE_DISABLE_AOT_SCALAR_LOCAL_RELOAD=1` independently disables exact live scalar
reuse in prepared AOT reloads. At compilation it selects the existing v1 reload
helper; runtime presence at first stale v2 use instead delegates to v1 in the
identical executable. Empty values and `0` also disable reuse. Runtime-off
includes that delegation cost; compile separate matching artifacts to compare
the complete compiler/runtime change.

By default, a stale prepared reload can retain the exact cached int/float/bool
bits only when they match the actual assigned, nonfinalized, noncleared slot and
both cleanup trackers contain only NOTHING or immediate scalars. Float matching
is bitwise, and float admission excludes opaque-reference and inline-string
prefixes. Tracker rotation and completed-epoch publication remain ordered.
Any mismatch, owning cleanup, unsupported representation or populated exception
sink retains v1 evaluation and cleanup. No epoch invalidation, reflective check,
destructor, logical frame or cancellation point is removed. This is an AOT
code-generation path; no IR/JIT benefit is implied.

See [exact live scalar reload measurements](RESULTS.md#exact-live-scalar-reuse-in-aot-reloads-2026-10-13)
and [verified evidence/audit](results-aot-scalar-local-reload-macos-arm64.json).
The measured reload-heavy gains have an explicit native-integer regression;
this is not a universal speedup.

See [AOT scalar-store measurements](RESULTS.md#aot-scalar-local-replacement-2026-10-11) and [verified evidence/audit](results-aot-scalar-local-store-macos-arm64.json).

See [direct primitive-store measurements and longer confirmation](RESULTS.md#direct-aot-primitive-local-replacement-2026-10-12),
[all verified samples](results-aot-direct-scalar-store-macos-arm64.json), and the
[62-check audit](FOLLOWUP-AUDIT.md#direct-aot-primitive-local-replacement-audit-2026-10-12).

See [native scalar-key measurements](RESULTS.md#aot-native-scalar-dispatch-key-2026-10-11) and [verified evidence/audit](results-native-scalar-key-macos-arm64.json).

## Optimized reporting policy and mixed workloads

All headline Java and develop/feature comparisons use an optimized Qore runtime
(`CMAKE_BUILD_TYPE=Release`, verified actual `-O3 -DNDEBUG` flags), `qcc -O3`
executables, and warmed Java JIT execution. Debug/sanitizer builds are for
correctness testing, not comparative timing. Historical `java-int` results are
interpreter diagnostics, not optimized Java comparisons. Select
`--modes qore-aot java-jit` for optimized AOT/Java runs with the general runner.

`mixed_workloads.qr` adds four dependency-free synthetic processing workloads:
CSV filtering/grouping/reporting, object-order processing, a materialized typed
callback/captured-closure pipeline, and text normalization/frequency indexing.
Input generation is excluded from timing; processing and result formatting are
included. These are broader than call microbenchmarks but are not production
application, I/O, startup or Java mixed-workload comparisons.

```sh
python3 bench/runtime-comparison/test_mixed_reference.py
build/qore --exec-mode=ir bench/runtime-comparison/mixed_workloads.qr check
build/qcc -O3 -o build/mixed-workloads bench/runtime-comparison/mixed_workloads.qr
build/mixed-workloads check
build/mixed-workloads callback_pipeline 4096 3 3 300 7
```

`mixed_reference.validate_checks(output)` validates all 60 check rows against
an independent Python reference. The archived checkpoint driver independently
compiles both revisions, verifies loaded libraries and optimized build flags,
then randomizes revision/layout order across five processes and three layouts.
See the optimized checkpoint in `RESULTS.md` for complete results, regressions,
raw evidence, exact revisions and limitations; do not treat a suite average as
an overall application speedup.


`QORE_DISABLE_AOT_DYNAMIC_CLOSURE_CONTEXT_V2=1` disables the additive fully
deferred object/class/self contract at compilation and execution; v1's eager
object context and independently published binding remain. New v2 publication
requires both the existing bounded IR eligibility proof and a complete lowered
call proof. It still uses the existing `_dynamic_int_v1` companion body, not a
separate LLVM body. Capture reads stay separately synchronized. Full diagnostic
frame publication, reader waits and all live admission/stack guards remain.
Exception-time materialization installs the closure context and restores live
and RuntimeConfig snapshot state independently, including after an Impl move.
The older `QORE_DISABLE_AOT_DYNAMIC_CLOSURE_DEFERRED_CONTEXT=1` control also
disables v2 selection and restores eager markers. Controls are presence flags,
sampled once at runtime. Compare separately compiled optimized artifacts against
older runtimes; new artifacts need the v2 binder. See RESULTS for the measured
closure gains and the effectful-method regression, not a universal speedup.

`QORE_DISABLE_AOT_CROSS_PROGRAM_CLOSURE_ENTRY=1` disables the native scalar closure retry after the generic path enters the owning Program. It is a runtime presence flag sampled once; empty and `0` values also disable it. Same-Program admission and the complete generic Program transition remain. No recompile is needed for this diagnostic control. Compare frozen optimized runtimes for before/after performance; see RESULTS for cross-Program call measurements and all control costs.

`QORE_DISABLE_AOT_CROSS_PROGRAM_METHOD_ENTRY=1` disables the scalar method
retry after an AOT dispatch site completes its ordinary Program transition. This
runtime presence flag is sampled once; empty and `0` values also disable it, and
no recompilation is needed. The matched dispatch entry must prove exact canonical
arguments and a ready homogeneous `int(int)` or `float(float)` companion. After
the transition, invocation rechecks the live receiver, body, entry, signature,
Program and debugger state using stable method metadata. The frame retains the
original caller's Program. Compiler-proven unary member reads and integer effect
prefixes can also retry their native companions. Unsupported and parameterized
methods keep ordinary binding, as does a read-only native miss. A partial effect
prefix resumes after completed updates without replaying them. The same-Program compact
dispatch proof and the full transition's counts, locks and lifetime are unchanged.
See RESULTS for optimized before/after timings, the disabling control and costs
in other workloads. This changes AOT method dispatch; it does not add companions
to bodies executed only by IR/JIT.

`QORE_DISABLE_AOT_CROSS_PROGRAM_MEMBER_ENTRY=1` disables only the member/effect
retry, retaining the pure scalar retry. It is a runtime presence control sampled
once; empty and `0` values also disable it. The full Program transition remains,
and the member invocation establishes eager object/class context with the original
caller Program in its diagnostic frame. No recompilation is needed for this
control. Compilation conservatively omits read-only unary/getter companions for
strict reference member declarations, including inherited declarations; dynamic
and optional declarations retain live guards, and effect prefixes remain eligible.
Recompile to obtain that compiler refinement. See RESULTS for optimized before/after
measurements, reference-backed controls and the remaining costs.

`QORE_DISABLE_AOT_IMPORTED_CLOSURE_ENTRY=1` disables native integer closure
admission when the closure executes in a different Program from its compiled
body's owner. This runtime presence flag is sampled once; no recompile is needed.
Admission requires the v2 explicit-context entry and the existing complete scalar
body proof. The supplied RuntimeConfig provides live thread context; the
execution-Program option override is installed only on cold materialization.
Both Programs' debugger guards, normal-body
identity, capture checks, stack protection and diagnostic frames remain. A call
from another execution Program still performs the ordinary Program transition.
Disabling deferred context or v2 also disables this imported path. These runtime
helpers benefit calls to eligible AOT bodies, including callers in IR/JIT mode;
they do not add native companions to bodies executed only by IR/JIT.

`QORE_DISABLE_AOT_IMPORTED_CLOSURE_DEFERRED_OPTIONS=1` restores eager option
setup while retaining native imported-closure admission. This runtime presence
flag is sampled once, requires no recompilation, and treats empty and `0` values
as enabled controls. The existing complete v2 lowered-body proof permits only
the dominating stack guard, location-slot access and non-evaluating scalar
capture reads. Successful execution cannot observe runtime options; a failed
stack guard installs them before constructing its exception. Cold restoration
restores the caller's override before applying its saved runtime options. See
RESULTS for the incremental gains and the measured costs in control workloads.

`OpaqueCallbackBench.java` supplies ordinary optimized Java counterparts for all four
`opaque_callbacks.qr` kernels, with no `DONT_INLINE` controls. The choice of two
literal functions is made once per invocation. JMH performs normal inlining and
constant reduction; in particular the constant integer accumulation becomes
count-scaled arithmetic, so its very small ns/item value is not a call latency.
The committed result evidence includes the exact JDK/JMH setup and raw samples.

`QORE_DISABLE_AOT_FINITE_CALLREF_TARGETS=1` disables finite target selection at
compilation. Recompile to compare the ordinary runtime callback path. Admission
requires two complete nonthrowing scalar targets, exact argument binding and a
scalar caller proven not to reenter arbitrary code. This is an AOT optimization;
IR and JIT behavior, runtime bindings, ownership and cancellation are unchanged.

`QORE_DISABLE_AOT_FINITE_CALLREF_RELOAD_ELISION=1` restores callback-local cache
refresh checks at compilation while retaining finite target selection. The
default uses the same no-reentry, assignment, lifetime and dominance proof to
omit a refresh whose value cannot change. Original binding, cached loads,
ownership and cancellation remain. Recompile for this diagnostic control;
see RESULTS for optimized before/after, disabled-control and Java measurements.


`short_callbacks.qr`, `short_callbacks.py` and `ShortCallbackBench.java` measure
batches of short invocations with the same callback bodies as `opaque_callbacks`.
Compile with the intended optimized `qcc -O3 --strip-source` and run, for example,
`./short_callbacks int_unary_1 4096 2 3 150 7`. The final suffix is the inner
callback count (1 or 17); `_inline` kernels are algebraic controls. Each timed
item is one short invocation, with 4096 items per timer/string dispatch. Use
`./short_callbacks check` and validate its output with `short_callbacks.py`.
Java uses the same JMH annotation-processing setup as the other benchmark classes.
Normal Java inlining remains enabled. Report whole-batch ns/invocation; do not
subtract controls as isolated call latency or pool these numbers with the older
per-invocation timer harness. See RESULTS for frozen-runtime provenance and limits.

`QORE_DISABLE_AOT_FINITE_CALLREF_BIND_ELISION=1` retains optional callback native/typed
adapter binding at compilation. By default, only creations whose sole use feeds
a private admitted finite-target PHI qualify; deletion occurs after every call
site in that group has actually inlined. Reference creation, local ownership,
cleanup and cancellation remain. Recompile for the diagnostic control. The
RESULTS section records independently confirmed short-call gains and the repeated
polymorphic/long-loop costs; these are not a general Java parity claim.

`QORE_DISABLE_AOT_FINITE_CALLREF_COERCE_ELISION=1` restores assignment coercion
for exact finite callback locals at compilation. The default requires both
known target signatures to match the local's nonoptional, nonvariadic typed-code
declaration exactly. It omits the redundant coercion helper and that helper's
temporary owner, retaining creation, normal runtime publication, cache ownership,
cleanup and cancellation. Plain, optional, differently typed, aliased and escaping
cases retain their existing paths. Recompile for the diagnostic control. See
RESULTS for independently confirmed short-invocation gains, broader costs and
fresh Java measurements; these are incremental branch results, not pure develop.

The [latest optimized cumulative checkpoint](RESULTS.md#optimized-module-checkpoint-after-finite-callback-coercion-omission-2026-10-28) compares branch 8276756a7a with pure develop 62a415e603 across 78 Qore workloads and 45 fresh Java counterparts. It includes mixed workloads and documents whole-kernel units, inlining controls and Program transitions. The dedicated short finite-callback experiment has a separate preceding-branch baseline.

The native constant-callback rewrite removes private two-target zero-argument constant callback objects before LLVM lowering and restores the original IR for metadata/debugging. `QORE_DISABLE_AOT_CONSTANT_CALLBACK_SCALAR_REPLACEMENT=1` restores the previous compiler path. The [optimized comparison](results-finite-constant-callback-macos-arm64.json) records 73–78% less time for short targets, about 9.6% less for long constant-callback loops, fresh Java results, broader tradeoffs and 1578 verified timing jobs. See [RESULTS.md](RESULTS.md) and the [62-check audit](audit-finite-constant-callback.md).


Prepared reload, auto and class body-local handles also work inside imported
AOT functions whose executing Program differs from their code owner. The
compiler-proven instantiation point and exact local identity select active
invocation storage through RuntimeConfig; handles cache storage, not values or
type/access acceptance. Program transitions and live mutation/ownership checks
remain. Existing runtime presence controls
`QORE_DISABLE_AOT_PREPARED_LOCAL_RELOAD`, `QORE_DISABLE_AOT_AUTO_LOCAL_SLOT` and
`QORE_DISABLE_AOT_CLASS_LOCAL_SLOT` independently disable acquisition. No
recompilation is required for this helper-only change. See the
[optimized incremental results](RESULTS.md#prepared-body-local-storage-in-imported-aot-functions-2026-10-29)
for imported calls, list mutation, mixed workloads, retained costs and attribution.


`QORE_DISABLE_AOT_CROSS_PROGRAM_DEFERRED_CONTEXT=1` restores eager object/class and empty-marker setup for pure native method-v2/v3 retries after a normal Program transition. It is a runtime presence flag, sampled once; empty and `0` values also disable the optimization. The common `QORE_DISABLE_AOT_NATIVE_METHOD_DEFERRED_CONTEXT` control restores this setup too. No recompilation is needed. The new path borrows invocation/Program-owned metadata, retains the caller Program in the logical frame, and materializes context before a failing stack guard constructs its exception. Full Program transitions, live admission, diagnostics and ownership remain. See [optimized results](results-cross-program-deferred-context-macos-arm64.json) for gains and retained costs.


Cross-Program native scalar/member retries share one live `RuntimeConfig&` with their call-option guard after the complete ordinary Program transition. The reference remains invocation/thread-owned; live admission and fallback checks remain. Same-Program typed entries and the generated AOT contract are unchanged. This runtime-only experiment uses identical AOT artifacts across frozen optimized libraries; see the [complete comparison](results-cross-program-shared-context-macos-arm64.json) for gains, frame growth and retained control costs.


`QORE_DISABLE_AOT_RECEIVER_FLOAT_FORWARD=1` disables the AOT proof and runtime shortcut
for an exact `float(float)` method whose entire body returns one `self.method(value)`
call. The warmed inner target must independently qualify as a pure same-Program native
floating leaf. Unsupported bodies, references, Programs/options, debugger attachment and
unready targets retain ordinary execution. Both logical frames and the inner source call
site remain; integer and floating ABI entries are separately typed. Float member admission
runs before the forwarding probe, and the existing context/descriptor layout is retained.

The optimized matched-toolchain comparison uses fresh before/after artifacts across three
layouts. The floating forwarding kernel improves 80.99% in confirmation, with two-call
floating +2.52%, field +1.03% and closure +1.83% controls retained as part of the tradeoff.
See the [complete results](RESULTS.md#floating-point-aot-forwarding-2026-10-30),
[evidence](results-scalar-forwarding-macos-arm64.json), and
[audit](audit-scalar-forwarding.md). This is a narrow whole-kernel improvement; the new
probe has no qualified Java equivalent and does not establish an overall application gain.


Immutable dot-call type-metadata caching was [measured and rejected](RESULTS.md#rejected-immutable-dot-call-metadata-cache-2026-10-30). Small forwarding gains did not justify repeated field and side-effecting call costs. Runtime source is restored; module identity guards remain enabled. Next: investigate reader-safe publication/removal of paired forwarding frames.


Reader-safe paired forwarding frames are [measured and retained](RESULTS.md#paired-forwarding-frames-2026-10-30): roughly 3% integer and 3.6% floating forwarding reductions, with all slower controls reported. Both logical frames and native frame sizes are preserved. See the [full 62-item audit](audit-paired-frames.md). A fresh cumulative optimized develop/Java comparison is the next checkpoint.


The [post-paired-frame optimized checkpoint](RESULTS.md#optimized-checkpoint-after-paired-forwarding-frames-2026-10-30) refreshes all 32 workloads against current develop and 25 fresh Java rows. All 651 processes /1953 measured windows are preserved; see the full table and optimizer/Program-context qualifications.


The [matched forwarding comparison](RESULTS.md#matched-forwarding-comparison-with-java-2026-10-30) adds four same-program kernels against develop and both normal Java C2 and leaf-only boundary controls. It identifies the two-call outer body as a substantial remaining gap.


[Resolved native method calls](RESULTS.md#resolved-native-method-calls-2026-10-30) extend typed scalar dispatch to eligible parse-resolved sites while retaining selected overloads and ordinary fallback. The full41-workload table reports all gains and costs; see the [audit](audit-resolved-native-method.md).


[Native binary call arithmetic](RESULTS.md#native-binary-call-arithmetic-2026-10-30) removes reboxing between eligible single-use scalar method calls and typed arithmetic. Common-case, allocation-stress and all broader control results are reported separately, including integer stack-frame growth. See the [audit](audit-native-binary-call.md).


[Bounded polymorphic scalar calls](RESULTS.md#bounded-polymorphic-scalar-calls-2026-10-30) imports known pure override bodies behind load-bound receiver/Program checks. Both optimized campaigns and all control costs are recorded with the [audit](audit-polymorphic-scalar.md).


The [post-polymorphic optimized checkpoint](RESULTS.md#optimized-checkpoint-after-polymorphic-scalar-inlining-2026-10-30) compares39workloads with develop and32Java rows, including29fresh ordinary-C2 counterparts alongside original boundary controls. All885processes/2655windows are retained.


[Scalar override guard hoisting](RESULTS.md#scalar-override-guard-hoisting-2026-10-30) moves immutable target admission out of AOT loops and shares live receiver classification. Both optimized campaigns and slower controls are retained in the [evidence](results-scalar-override-guard-macos-arm64.json) and [audit](audit-scalar-override-guard.md).


[Develop debugger integration](RESULTS.md#develop-debugger-merge-and-native-admission-2026-10-31) preserves native call admission while recording every AOT debug statement. Optimized before/after results and qualification are in the [evidence](results-guard-develop-merge-macos-arm64.json) and [audit](audit-guard-develop-merge.md).


[Develop annotation followup](RESULTS.md#develop-annotation-followup-2026-10-31): optimized merge checkpoint, shared JIT/AOT summary validation and [62-item audit](audit-guard-develop-followup.md).


[Retained receiver classification](RESULTS.md#retained-receiver-classification-2026-10-31): bounded entry admission with live identity/deletion checks; optimized [evidence](results-pinned-override-macos-arm64.json) and [audit](audit-pinned-override.md).


[Retained receiver base-class checks](RESULTS.md#retained-receiver-base-class-checks-2026-10-31): shared status views and cached base admission; optimized [evidence](results-pinned-base-macos-arm64.json) and [audit](audit-pinned-base.md).


The [post-pinned-base optimized checkpoint](RESULTS.md#optimized-checkpoint-after-retained-receiver-classification-2026-10-31) compares 39 workloads with develop and 32 Java rows, including 29 fresh ordinary-C2 counterparts alongside original boundary controls. All 885 processes / 2,655 windows are retained.


[Retained receiver metadata PHIs](RESULTS.md#retained-receiver-metadata-phis-2026-10-31): selected-snapshot identity with live validity; optimized [evidence](results-pinned-phi-macos-arm64.json) and [audit](audit-pinned-phi.md).


[Rejected class-local receiver snapshot PHIs](RESULTS.md#rejected-class-local-receiver-snapshot-phis-2026-10-31): reduced comparison count did not justify repeated broader costs; optimized [evidence](results-pinned-borrow-phi-macos-arm64.json) and [audit](audit-pinned-borrow-phi.md).

[Rejected uniform AOT function alignment](RESULTS.md#rejected-uniform-aot-function-alignment-2026-10-31): all four policies and fixed-policy confirmation, with [evidence](results-aot-function-alignment-macos-arm64.json) and [audit](audit-aot-function-alignment.md).


[Receiver-local provenance](RESULTS.md#receiver-local-provenance-2026-10-31): same-block assignment hints reuse retained receiver metadata with actual-value and live deletion guards. Optimized [evidence](results-pinned-local-receiver-macos-arm64.json) and [audit](audit-pinned-local-receiver.md).


The [post-local-receiver optimized checkpoint](RESULTS.md#optimized-checkpoint-after-receiver-local-optimization-2026-10-31) compares 39 workloads with develop and 32 Java rows, including 29 fresh ordinary-C2 counterparts alongside original boundary controls. All 885 processes / 2,655 windows are retained.

[Rejected inline scalar assignment coercion omission](RESULTS.md#rejected-inline-scalar-assignment-coercion-omission-2026-10-31): repeated list-mutation gains and adverse indirect-call controls, with [evidence](results-inline-scalar-coerce-rejected-macos-arm64.json) and [62-item audit](audit-inline-scalar-coerce-rejected.md). The cancellation regression now exercises both early analysis and late cleanup checkpoints.

[Unary floating callback result initialization](RESULTS.md#unary-floating-callback-result-initialization-2026-10-31): a runtime-only unary floating improvement after rejecting integer and zero-argument extensions, with [all six measured epochs](results-deferred-float-unary-macos-arm64.json) and [62-item audit](audit-deferred-float-unary.md). Existing v9 AOT artifacts can benefit; measured controls and shared-host limits remain explicit.
