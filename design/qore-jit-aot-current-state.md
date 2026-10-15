# Qore JIT and AOT Current Design

## Status

Qore has one LLVM-backed compilation pipeline shared by JIT and AOT:

```text
Qore AST -> Qore IR -> LLVM IR -> native code
```

Runtime modes:

- `--exec-mode=ast`: AST interpreter and default execution mode.
- `--exec-mode=ir`: eager IR lowering and IR interpreter execution.
- `--exec-mode=jit`: eager IR lowering; the program's own functions are compiled
  to native code on a background thread behind a parse-commit barrier, while
  functions imported from modules promote to native adaptively on first use.
- `--exec-mode=tiered`: explicit adaptive execution, promoting hot functions
  AST -> IR -> JIT.
- `qcc`: ahead-of-time compiler and artifact inspection tool for executables,
  `.qmod`, `.qo`, and `.qoa` artifacts.

This document is the durable index for the current IR/JIT/AOT architecture. It
should describe stable contracts and link to narrower design notes; transient
milestones and unfinished task lists belong under `docs/plans/`.

## Bounded IR integer leaves and closure observability

The interpreter's integer leaf evaluator accepts one- or multi-block compositions
with one or two exact integer parameters and an exact integer return. Admission
remains bounded to eight forward-only blocks, eight IR-only body locals, 64
instructions and value IDs below 128. Captures, references, side effects and
backward branches retain ordinary execution. Compact single-operation descriptors
remain preferred because native AOT summaries also consume them.

Composed leaves support integer bitwise AND/OR/XOR and three-way comparison in
addition to the existing integer arithmetic and comparison operations. Addition,
subtraction, multiplication and local increment use unsigned intermediates to
preserve full-width wrapping without signed C++ overflow. Invalid/unassigned
operands and exceptional modulo cases abandon the scratch evaluator and use the
ordinary call path. No externally visible state is changed before fallback.
`QORE_DISABLE_IR_NATIVE_INT_PROGRAM_INLINE` and
`QORE_DISABLE_IR_NATIVE_LEAF_INLINE` retain the corresponding ordinary paths.

Closure runtime shortcuts reject attached debuggers, and leaf execution is
limited to the same Program. Normal closure dispatch selects AST or debug IR
when attached. QORD version 20 preserves each closure's entry file/source label,
offset, line range and lexical parse options in a fixed 40-byte payload after
its varargs flags and before captures. Both expression and expression-slot
readers attach an immutable, variant-owned entry statement before publishing the
closure. Entry statements receive IDs but do not replace executable statements
in the file/line index. This supplies the identity required for function entry
and exit events from source-stripped closure IR. File/source labels use the
normal compiler prefix-map writer; debugger entry locations must not restore
physical build paths after other source metadata has been mapped.

Older supported QORD versions remain readable without that payload; recompiling
is required to supply missing closure entry metadata. Older runtimes reject
version-20 artifacts. No prepared scalar callback helper ABI changes. Compiler
comparisons must use separate before/after AOT compilation, not a newly compiled
executable against the older runtime. `IRComposedIntLeaf.qtest` covers full-width
compositions, effects and exception fallback, concurrent first calls, Program
churn/crossings, and warmed/cold debugger attachment through source-stripped AOT.

### Dynamic native integer closures

AOT emits an optional dynamic `int(int)` closure companion independently of
whether the caller can recover the closure-create expression. Its ABI passes
the unboxed integer, borrowed closure instance, live same-thread `RuntimeConfig`,
an acceptance flag, the callee's AOT context and the exception sink. The normal
body publishes this companion atomically through
`qore_rt_bind_closure_int_unary_v1`; the context owns no per-instance environment
pointer. A normal first call establishes the binding and metadata readiness.

Admission currently requires one block of at most 64 whitelisted scalar
instructions, exact integer parameter/return types, no body locals or effects,
and only exact integer capture loads. Each capture load calls
`qore_rt_try_load_closure_int_v1` at its original instruction position. This
resolves the lexical local in the explicit closure's immutable environment and
reads the full-width integer under the ordinary capture read lock. Repeated
reads are not merged into an entry snapshot. An unassigned or reference-backed
capture clears the acceptance flag and unwinds native cleanup before ordinary
execution; no reference evaluation or visible effect can precede that retry.

Dispatch rechecks same-Program identity, normal-body identity, metadata
readiness, exact signature, debugger attachment and the current thread's
variable-stack availability. It preserves lexical object/class context, native
stack guards, empty variable-frame markers and reader-safe diagnostic frame
publication. Marker allocation finishes before compatibility context is changed;
a failed closure-marker allocation rolls back the local marker. Cross-Program,
debugger and unsupported bodies keep their existing paths.

The integer v3 callback helper's miss path can invoke this companion through its
existing unboxed continuation. Its positive function-reference cache still
retains only exact function references: dynamic closures are borrowed, never
cached or retained past the call. Ordinary boxed closure entrypoints can use the
same companion while keeping scalar argument cleanup. No serialized ABI or
existing helper signature changes. `QORE_DISABLE_AOT_DYNAMIC_CLOSURE_ENTRY`
disables companion compilation and runtime dispatch; compiler comparisons need
paired AOT compilation against each runtime.
Disabled continuations reject before decoding closure metadata. Boxed entry
checks for a published companion before acquiring the live TLS-backed context,
so old native artifacts and unsupported bodies do not acquire an unused view.

`AOTDynamicClosureEntry.qtest` checks full-width values, distinct environments,
mutation, references/unassigned captures, concurrent first calls, ordinary
fallback and concurrent stack readers while a writer changes a repeated capture.
Its debugger fixture checks warmed and cold capturing entries across attachment
and detachment. Generated-code assertions require two distinct synchronized
capture reads for the repeated-read fixture even at `-O3`;
`IRComposedIntLeaf.qtest` also covers noncapturing entries.

## JIT Runtime

JIT-compiled functions call C ABI helpers in `JITRuntime.cpp` for operations
that need Qore runtime semantics: calls, object/member access, exceptions,
closures, local writes, lvalue mutation, context handling, and cleanup.

The JIT path must preserve:

- `ExceptionSink` semantics through native exception unwinding.
- Qore reference ownership for every returned value.
- Local and closure variable coherence when runtime helpers can observe or
  mutate variables outside native SSA state.
- Debugger-driven AST override in tiered mode.

Implicit `copy()` calls are constructor operations, not ordinary methods. Both
parser paths that create `SelfFunctionCallNode` mark that identity before IR
lowering, including the bare-function rewrite that calls `parseInitCall()`
directly. The flag survives argument-list cloning and directs execution through
`execCopy()`, preserving the dynamic class, copy hooks, and exception cleanup.
Normal method dispatch cannot invoke a copy variant: its calling convention is
different. `ImplicitCopyDispatch.qtest` exercises polymorphic copies and failing
copy hooks in all four execution modes.

### Plain-function access context

Fast plain-function calls bind parameters in the caller's object/class context,
then clear both contexts while executing the function body. Return filtering
also uses the function's empty access context. `ObjectSubstitutionHelper` keeps
the thread context and `RuntimeConfig` synchronized and restores both on exit;
parameter cleanup retains the existing caller-context ordering. This applies
to ordinary fast calls, target-prelinked calls and prelinked AOT function calls.
Methods and closures retain their separate object/lexical-class conventions.

An enforced class parameter type is not by itself proof that a later assignment
to the same declared type cannot fail: the caller may have private-inheritance
access that the plain function does not. `AOTPlainFunctionClassContext.qtest`
compares this boundary with AST execution, including normal/invoke/callback
calls, return filtering, caller-context restoration and native-EH AOT.

### Runtime-backed boxed local freshness in AOT

An ordinary AOT entry instantiates boxed parameter and body-local slots on the
runtime stack. Lowering every source access to IR does not make those slots
invisible: a callee can replace them with `set_local_var_value()` and either
return or throw. Reload exemptions therefore use the adjusted private body-local
set, not the original IR-only access classification. The bulk reload skip and
per-local predicate must agree about this distinction.

Ordinary boxed parameter assignments must also synchronize their runtime slots.
Enabling reloads while leaving those stores alloca-only loses the assignment at
the next call boundary. Required native scalar allocas and private direct-entry
arguments retain their existing storage conventions; reloading a private
argument from the runtime stack could alias an outer frame. Private IR-only
string/body storage also retains its prior visibility contract. This correction
does not establish general reflective visibility for private entries or IR/JIT.

The existing lazy reload epochs, exception-path reloads, owned tracker/deferred
cleanup and reflective class-parameter cache guards remain in use. No runtime
helper ABI, serialized IR format or public language API changes. Existing AOT
artifacts must be recompiled to receive the corrected storage/reload decisions.

`AOTRuntimeLocalReload.qtest` compares ordinary-entry behavior with AST execution
for boxed receiver/payload replacement, scalar/container representation changes,
successful and throwing callees, destruction counts and written parameter
controls. It asserts native LLVM entries and runtime-slot invalidation, using
stripped `-O0`/`-O3` artifacts, native EH, split codegen and fallback controls.

This is a correctness prerequisite for further AOT call optimization, not a
speedup. The measured additional reload cost must be reported. A future native
call effect proof must cover receiver/argument cleanup, overwritten result
cleanup and deferred local owners as well as the callee body: a destructor can
reenter Qore and mutate caller locals even after a pure scalar method. Neither
same-Program admission nor a successful native helper result alone proves those
cleanup effects absent.

The AOT compiler can bypass the stale-local helper when the invocation-local
valid epoch already equals the current reload epoch. This exposes the helper's
existing no-op branch to LLVM without changing invalidation or slot visibility.
Reflective class-parameter mutation forcing runs before the comparison; stale
values still use the complete helper, including reference evaluation, owned
trackers, deferred cleanup and exception handling. This does not infer that a
native call or its caller cleanup is effect-free.

`QORE_DISABLE_AOT_INLINE_RELOAD_GUARD=1` restores unconditional helper calls at
compilation for differential validation. It is not a runtime switch: compile
separate artifacts to compare it. IR/JIT lowering and helper ABI are unchanged.
The reload regression asserts both ordinary native entries and guard
presence/absence, including stripped `-O0`/`-O3` output and fallback controls.

### Direct primitive replacement in ordinary AOT locals

The checked/eval-weak AOT assignment helper can replace an already assigned
primitive in an ordinary `auto` local without generic value conversion. It
retains the existing declaration and live-slot admission: captured, self,
top-level, read-only, no-narrowing, reference and substituted declarations are
excluded, as are absent, finalized, block-cleared, unassigned, static and
fixed-type slots. Both the old storage and incoming value must be primitive;
the incoming value must be an immediate integer, float or boolean.

After admission, the helper copies the matching primitive and updates the
storage tag. It leaves all ownership and assignment flags unchanged and writes
the actual runtime slot, so reflection and subsequent reloads see the result.
Neither side owns a node, so there is no conversion, reference scan, allocation,
destructor or reentry to perform. Wide boxed integers, escaped floats, enums,
nodes, weak/opaque values and reference-backed storage retain ordinary
assignment. Declaration checks remain per assignment unless declaration
admission is prepared as described below. Without a prepared body-slot handle,
the helper also performs the ordinary thread-local lookup on every assignment.

`QORE_DISABLE_AOT_DIRECT_SCALAR_STORE=1` restores the prior generic scalar
storage operation after the same admission guards. The broader
`QORE_DISABLE_AOT_SCALAR_LOCAL_STORE=1` disables admission altogether and takes
precedence. Presence, including an empty value or `0`, disables the corresponding
shortcut; one thread-safe immutable configuration samples the flags on first
helper use. Set controls before process startup. No exported helper signature
or serialized IR format changes, but the private-header change alters the AOT
runtime identity: before/after builds require separately compiled artifacts
with matching runtimes. Same-runtime on/off can reuse one executable.

`AOTScalarLocalStore.qtest` compares AST/IR/JIT/tiered results and normal,
source-stripped, native-EH, split and no-debug AOT at `-O0`/`-O3`. It exercises
primitive-family transitions, signed zero, infinities, NaNs, immediate-integer
boundaries and fallback representations, with type/format checks, references,
destruction and runtime-slot observation. Its existing block-local reflection
limitation remains explicit: all modes check repeated values, while full
block-map observation is required only in AST. Debug-native checks invoke both
storage modes directly, asserting admission, exact bits, unchanged slots on
rejection and node ownership. The optimization changes the AOT helper, not the
interpreter or JIT local-store paths.

### Prepared ordinary-auto body slots in AOT

For normal checked assignments to an ordinary runtime-backed `auto` body local,
AOT can resolve the actual runtime storage once per invocation. The compiler
excludes parameters, captured/self/top-level/read-only locals, non-auto lvalue
types, private IR-only storage and no-coerce assignments. The getter is inserted
after the entry's local-instantiation prologue and native stack guard, not at
the first instruction of a recursive callee before its own bindings exist.

`qore_rt_get_auto_local_slot_aot_v1` takes the current AOT context, local index
and borrowed same-thread `RuntimeConfig`. A missing context, mismatched Program
or unsupported/unavailable slot returns null. The handle lives in generated
invocation-local SSA, not a shared `QoreAOTContext` cache. It borrows storage,
never a value or reference owner. Ordinary block exits clear values without
popping that storage; recursive calls and stack-block growth retain the outer
slot's address. No handle may survive its invocation's cleanup.

`qore_rt_assign_auto_local_slot_aot_v1` uses the handle only to avoid lookup in
the existing primitive-store shortcut. Every declaration, input and live-slot
guard remains. A null handle uses ordinary lookup; an unsupported assignment
keeps the original generic path, including weak/reference handling, ownership,
destruction and errors. The native-EH counterpart preserves exception-sink
propagation. Assignments still publish into the real slot, and caller reloads
and reflective mutation are not elided.

Direct AOT fast entries bypass the ordinary wrapper and instantiate their own
runtime-backed ordinary body slots. Their function-exit cleanup must therefore
pop those slots in reverse push order, using `qore_rt_pop_body_local_aot_v1`.
Merely clearing them leaves a recursive child's binding above its parent's.
The later entry-local cleanup skips ordinary body slots already handled by
this loop. Wrapper-owned entries and block exits retain their existing
clear-only behavior; captured locals retain their separate closure-stack
lifecycle. Stack-guard failures before instantiation do not run body pops.

`QORE_DISABLE_AOT_AUTO_LOCAL_SLOT` disables preparation when present during
compilation. At runtime, presence sampled on the getter's first use makes it
return null, retaining per-assignment lookup in the same executable. Neither
control disables the body-pop correctness fix or the older scalar-store
shortcut. No existing exported helper signature or serialized IR format changes;
new versioned helpers and the changed runtime identity require matching,
separately compiled before/after AOT artifacts.

`AOTAutoLocalSlot.qtest` covers recursive parent values before overwrite,
stack-block growth, repeated block lifetimes, representation changes, captured
and typed fallbacks, destruction/reentry, exceptional cleanup and generated
normal/fast-entry ownership at `-O0`/`-O3`, including stripped-source, native-EH,
split, no-debug and disabled modes. Existing incomplete reflected maps for
source-specialized recursion and repeated block locals remain explicit; this
optimization does not claim to repair those maps. The native unit checks
exercise prepared and looked-up slots with both primitive-store modes.
`AOTLocalCleanupCancellation.qtest` verifies cancellation during cleanup
emission rejects the partial LLVM function and permits reuse of the same
lowerer after clearing cancellation, for both normal and fast entries.

### Prepared auto-store declaration admission in AOT

For an eligible ordinary auto body slot, `getAOTAutoLocalSlot` selects
`qore_rt_get_auto_local_slot_aot_v2` and matching normal/native-EH v2 assignment
helpers. The getter first performs the existing storage and same-Program checks,
then checks `LocalVar::isAOTScalarStoreDeclaration()`: noncaptured, non-self,
non-top-level, writable, without a no-narrow marker, exact `autoTypeInfo`, no reference
target and no type-substitution requirement. A miss returns null for v1 handling.
Only a handle from this v2 getter may be used as a non-null v2 store handle.

The declaration is complete before the admitted body executes. Signature type
replacement affects excluded parameters, while fallback type retargeting leaves
untyped auto unchanged. Reference-addressed locals are parser-marked as captured
before execution and are excluded; AOT reference creation only repeats that mark.
No value, reference classification or mutable assignment flag is part of this
cached proof. Storage and declaration admission remain invocation-local, with
the same post-instantiation ordering, recursive lifetime and cleanup as v1.

The v2 store omits repeated declaration checks but keeps all input and live-slot
guards in `LocalVarValue::tryAssignAOTScalarValue`. The incoming value must be an
immediate integer, float or boolean, and storage must be assigned, nonfinalized,
noncleared, nonstatic, nonfixed and scalar. A populated exception sink cannot
take the direct path. Successful stores publish the real slot; misses retain
the same generic eval-weak assignment and ownership/error behavior. No reload,
reflection, node-release or runtime-context obligation is removed.

`QORE_DISABLE_AOT_AUTO_LOCAL_DECLARATION` selects v1 getter/store code at
compilation. Runtime presence, sampled at first v2 store use, instead restores
per-store declaration checks with the same prepared storage. The v2 helper
resolves this choice and the shared scalar/direct-store configuration once into
one immutable mode; it does not revisit two initialization guards on each store.
Empty values and `0` count as present. Existing scalar/direct/slot/reload controls
remain independent. Legacy v1 helpers and serialized IR are unchanged; the new
symbols/private headers require matching compiler/runtime identities.

Native regression modes directly force the declaration-prepared live-slot
operation, including when runtime controls disable the helper, so fallback-only
success cannot hide a guard error. They check exact bits, rejected live states,
null fallback, node owner counts, declaration rejection and recursive stack
growth. The auto-slot, scalar-store and reload wrappers exercise compiler/runtime
controls, matching getter/store versions, normal/EH entries and independent
reload behavior. This is AOT-specific; no IR/JIT timing benefit is established.

### Scalar publication across AOT temporary boundaries

An admitted v3 auto-local store reports whether it performed a sink-clean,
immediate scalar replacement in the exact live slot. It shares v2 assignment
semantics and returns zero for ordinary lookup, owning inputs or old owners,
errors, and disabled reporting. Native-EH emission preserves unwinding. This
point-in-time witness is not a pure-callee guarantee: earlier calls may already
have changed other locals, and subsequent reference operations may reenter.

Lowering captures the witness and exact boxed bits immediately after assignment,
before retention, cleanup or exception checking. Each invocation has one token
identifying the last published cache. At the existing temporary boundary, only
that cache can keep the next freshness epoch, and only if its old epoch was
current and its bits still match. Other locals remain stale; the token is reset.
The original owner rotation and cleanup order are unchanged.

Before LLVM optimization or inlining, `prepareScalarBoundaryReuse` inserts a
revocation before every unknown call/invoke, external write or fence. Only
selected compiler intrinsics, ordinary private-alloca stores and non-writing
instructions preserve the witness. Exact scalar no-op guards can bypass selected
incref/decref calls; general reference operations are not pure. Forward publication
reachability intersected with backward boundary liveness limits early guards to
useful intervals. All remaining effect barriers are retained independently of
that pruning. Analysis visits instructions/CFG edges with cancellation every 100
steps, including duplicate worklist pops; guard splitting checks every 10.
Cancellation or LLVM verification failure prevents object emission.

Compile-time presence of `QORE_DISABLE_AOT_SCALAR_BOUNDARY_REUSE`, including `0`,
retains v2 stores and omits publication state. Runtime presence, sampled once at
first v3 use, suppresses reporting without changing assignment. The compile-only
`QORE_DISABLE_AOT_SCALAR_BOUNDARY_LIVENESS` restores unpruned early guard selection,
not weaker barriers. Global scalar/float cleanup controls also govern early
guards; the method-cleanup control governs the existing late caller selection,
not these independently selected early intervals. Legacy helpers remain available;
new artifacts require matching compiler/runtime identities. This is AOT-only,
not an IR/JIT performance claim. See the scalar-boundary benchmark results for
long-loop gains, short-call limits, pointer controls and code growth.

### Prepared runtime-backed AOT local reloads

The stale-cache reload path can reuse an invocation-local body-slot address,
independently of prepared assignment storage. `getAOTReloadLocalSlot` requires
an ordinary runtime-backed AOT body local and excludes parameters, captures,
self (including the reserved name), top-level and IR-only locals. Existing
reload eligibility also excludes native, weak-assigned and reload-exempt
locals. Typed and read-only body locals can be read; auto-store declaration
restrictions are not appropriate for reload admission.

`qore_rt_get_reload_local_slot_aot_v1` accepts the AOT context, local index and
borrowed live `RuntimeConfig`. It checks the Program and storage availability,
returning null for ordinary lookup when preparation is unavailable. Emission
uses the same post-instantiation anchor as body-store preparation, with a
separate per-function map. Handles are neither shared in the AOT context nor
retained beyond invocation cleanup. Recursive calls and block clearing do not
invalidate the containing invocation's ordinary slot address.

After the existing freshness and class-parameter mutation guards, a non-null
handle selects the prepared reload protocol; a null handle selects the existing
reload helper. The original `qore_rt_reload_prepared_local_if_stale_aot_v1`
helper takes seven arguments:
slot, cache, current owner, deferred owner, valid-epoch address, current epoch
and exception sink. It still invokes `LocalVarValue::eval` and the ordinary
loaded-reference handling. Only the `LocalVar`/TLS/stack lookup is omitted.

Both helpers share the existing ownership protocol. Equal epochs are a no-op.
Otherwise, both old owners are snapshotted before potentially reentrant
evaluation; the new cache/current owner and old current owner as deferred are
published before old deferred cleanup. The valid epoch is published last,
including on sink failure. No evaluated value, type, ownership flag or
reference-evaluation result is cached by slot preparation. Eager runtime reload
paths outside this stale-cache helper are unchanged.

`QORE_DISABLE_AOT_PREPARED_LOCAL_RELOAD` omits this preparation at compilation;
presence at first runtime getter use instead returns null in the same binary.
The flag and compiler map are independent of class/auto store controls. No
existing helper interface or serialized IR format is changed; new versioned
symbols and runtime identity still require matching compiler and runtime
artifacts. No IR/JIT timing benefit is established by this AOT path.

The reload and auto-slot fixtures exercise compile/runtime controls, independent
store controls, reflection, recursion and cleanup. Debug native checks force
both looked-up and non-null prepared reloads, checking fresh epochs, exact node
owner counts, deferred destruction/reentry, cleanup exceptions, block clearing
and reassignment. They also check getter rejection and typed read-only storage.

### Exact live scalar reuse in prepared AOT reloads

The default prepared reload selects the additive
`qore_rt_reload_prepared_local_if_stale_aot_v2` helper with the same seven-argument
signature and invocation-local storage contract. Equal epochs remain a no-op.
For a stale cache and an empty exception sink, it can reuse the cached bits only
after checking the actual live slot in `LocalVarValue::tryReloadAOTScalarValue`.

The slot must be assigned, nonfinalized and noncleared, with immediate integer,
float or boolean storage matching the cache. Integers and booleans must match
their primitive values; floats must match their decoded bits, preserving signed
zero and NaN payloads. Wide integers, tag-colliding doubles, nodes, references,
weak wrappers, enums and other storage retain ordinary evaluation. The check
does not allocate or embed the C++ storage layout into LLVM code.

Both cleanup slots must also be positively identified as NOTHING or immediate
integer/float/boolean values. The float predicate excludes inline-string and
opaque-reference prefixes; a pointer-tag-only test is insufficient. On success,
the exact cache stays unchanged, its bits become the current tracker, the old
tracker becomes deferred, and the completed epoch is published last. No owner
can be destroyed or user code reentered on this admitted path. A miss changes
no state and uses v1, including its evaluation, ownership and cleanup ordering.
A populated sink also uses v1.

This optimization does not infer purity from a successful native call. Every
existing invalidation, reflective write check and deferred cleanup remains.
Prepared-slot acquisition, recursive lifetime, block clearing, cancellation,
logical frames and cross-Program fallback are unchanged.

`QORE_DISABLE_AOT_SCALAR_LOCAL_RELOAD` selects v1 during compilation. Runtime
presence at first stale v2 use instead delegates to v1 in the same executable;
empty values and `0` also disable reuse. Runtime-off therefore includes the
v2-to-v1 wrapper cost. This control is independent of prepared-slot acquisition,
the inline epoch guard and assignment controls. The new symbol/private header
requires matching compiler/runtime identities; legacy v1 is unchanged. The
compiler selects v2 only in AOT mode, not the IR interpreter or JIT reload path.

Native tests force exact scalar reuse separately from the diagnostic switch
and check bit preservation, tracker rotation, changed live values, signed-zero
mismatch, cleared/finalized/unassigned state, owning and opaque cleanup values,
wide numbers and escaped doubles. Reentrant and throwing deferred destruction
is checked through generic, v1 and v2 reloads. The AOT wrapper retains O0/O3,
normal/EH, no-debug, split-compilation and independent compile/runtime controls.

### Checked AOT class-local initialization

A normal initial assignment to a runtime-backed AOT body local with one required,
non-complex class type can use the existing checked/eval-weak assignment directly.
It does not need a separate pre-coercion temporary and unchecked store. The
native cache remains NOTHING until assignment succeeds, then reloads the actual
stored value with the existing ownership/reload tracking. Private-inheritance
access, weak-reference normalization, missing/wrong-type errors and cleanup are
still handled by the runtime; a matching class annotation does not remove them.

This lowering excludes reassignments, value-producing IR stores, optional or
reference types, complex/parameterized types, IR-only locals and non-normal
assignment modes. `AOTClassLocalAssignment.qtest` checks normal/invoke paths,
weak/deleted values, captured/reference mutation, escaped-capture lifetime,
failed reassignment, exception cleanup and caller/callee access differences.
`QORE_DISABLE_AOT_RUNTIME_CHECKED_CLASS_STORE=1` at compilation restores the
separate coercion path. Existing native artifacts require recompilation; no
serialized format or existing helper ABI is changed.

For a reloadable, noncaptured, non-self local, the versioned
`qore_rt_assign_class_local_result_aot_v1` helper combines this checked store
with producing the owned value for the existing cache/reload tracker. A plain
empty runtime slot has no replaced value or recursive root whose cleanup can
reenter Qore and change the result. Only that case retains the actual stored
value through lvalue and weak-input cleanup instead of looking up/evaluating
the local again. Nonfresh slots and defensive captured-slot fallback still
reload after cleanup. Reference targets, special `self` lookup and nonreloadable
caches are excluded at compilation; normal and native-EH error paths retain
ownership cleanup. The returned reference is transferred, not retained again,
into the usual tracker/deferred-release chain.

`QORE_DISABLE_AOT_CLASS_STORE_RESULT=1` at compilation keeps the separate reload.
The regression also checks this control, captured fallback and split codegen.
The additive versioned symbols require a compatible runtime for newly compiled
artifacts; older artifacts keep their existing helpers.

Inside the versioned helper, a fresh plain slot can omit generic `LValueHelper`
construction/destruction. `LocalVar::tryAssignInitialClassValue()` checks the
current-thread slot and excludes captured, self, substituted, finalized, static
and already-assigned storage. It calls the same `acceptAssignment()` type/access
validator and `assignAssume()` real-reference bookkeeping as the ordinary path.
The stored value remains runtime-backed, so scope/exception cleanup and local
ordering do not change. `QORE_DISABLE_AOT_FRESH_CLASS_LOCAL_STORE=1` restores the
ordinary lvalue path at runtime (presence sampled once).

For that same fresh slot, a single unmapped required-class acceptance spec can
use `acceptsRequiredClassValueWithoutFilter()` before generic conversion dispatch.
It shares the ordinary raw/parameterized-object gate and inheritance/access
validator; a failed positive check always falls back to `acceptAssignment()`
for the original conversion or diagnostic. Optional, union, mapped and
parameterized target specs retain generic acceptance. A parameterized object
can still satisfy a plain base class or a `legacy_raw_accepts` target. No result,
class-compatibility or access decision is cached across calls.

This changes neither slot ownership nor weak normalization or cleanup ordering.
`QORE_DISABLE_AOT_CLASS_ACCEPT_FAST_PATH=1` disables the positive check at runtime
(presence sampled once). The compiler and versioned helper ABI are unchanged;
existing AOT artifacts using the fresh-store helper can use the specialization.

Once this same positive check proves a strong object, a non-fixed fresh slot
uses the existing `assign(AbstractQoreNode*)` overload instead of redispatching
through all scalar/enum/value cases in `assignAssume(QoreValue&)`. A
`ReferenceHolder<QoreObject>` transfers one reference to the runtime slot; the
returned cache value receives a second reference. The node overload preserves
normal real-reference bookkeeping. No acceptance result is cached, and private
access, raw generic compatibility, weak normalization, captured/special slot
fallback and declaration-order cleanup remain unchanged. The input stays
borrowed and protected by its caller while the non-mutating acceptance check
runs. `QORE_DISABLE_AOT_CLASS_NODE_STORE=1` retains generic value storage
(presence sampled once); no compiler or helper ABI change is required.

The versioned class-store helper also attempts this checked initializer before
constructing a weak-input holder when the borrowed input is positively identified
as a strong object. A handled attempt needs no weak normalization or empty
weak-holder cleanup. The result holder, type/access checks, runtime slot and both
owned references remain unchanged; errors still return NOTHING with the sink
set. Captured locals take their existing path before this probe. Weak/non-object
inputs and unadmitted slots keep the normal path. No lifetime or cache elision is
implied. `QORE_DISABLE_AOT_CLASS_STRONG_STORE=1` selects the old entry path
(presence sampled once). A same-library flag comparison isolates this branch
from cross-library layout variation; see the benchmark results.

The first native-purity local-cache proposal needs additional work: statement
boundary cleanup independently advances the reload epoch and can run destructors.
Skipping only the increment after a pure call cannot preserve caches across that
boundary. A future effect-aware boundary must preserve all earlier pending
generic effects and cleanup effects, not just inspect the last call's purity.

Object-only admission in the fresh-store helper, its required-class positive
validator and both exact-AOT-target predicates uses direct node tags for ordinary
and opaque pointers. Non-pointer values retain generic dispatch; weak wrappers
remain outside strong-object admission. No class compatibility/access result is
cached, and the live exact-class predicate still checks deletion on every call.
The class-only predicate retains its existing caller-provided validity contract.
This runtime-only specialization changes no ABI or ownership. Five-fork paired
measurements at 1024/4096 elements improve the mixed polymorphic kernel by
8.19%/7.32%; Java remains about 17.5x faster at 4096. Receiver-local setup and
cleanup remain the dominant follow-up targets; see
[object-admission results](../bench/runtime-comparison/RESULTS.md#object-admission-specialization-2026-10-09).

The admitted class initializer also fills its known-NOTHING result holder
directly, avoiding replacement cleanup while retaining the holder's RAII
ownership and both slot/cache references. Both internal callers satisfy the
explicit empty-output precondition, including a rejected first attempt followed
by weak normalization. Three identical-executable comparisons show modest
1.37–1.80% polymorphic median gains with overlapping fork ranges; no general
owner elimination or Java parity follows. See
[empty-holder results](../bench/runtime-comparison/RESULTS.md#empty-class-result-holder-2026-10-09).

The class-check runtime comparison requested 16-byte alignment on the twelve
prepared scalar callback v3 entrypoints. The unaligned class-check candidate
repeatedly regressed wide callbacks; a 128-byte alternative recovered that AOT
control but increased runtime text by 95,700 bytes and showed an unfavorable IR
wide-callback signal. That iteration's 16-byte build adds 9,284 bytes over the
unaligned candidate (9,640 over its baseline) and the final before/after run
does not reproduce the IR-wide slowdown. Alignment changes no entry ABI, guard,
frame lifetime or publication protocol; no equivalent benefit is established
on other targets.

The dynamic-closure follow-up requests 64-byte alignment. Its earlier 16-byte
runtime showed a floating-callback slowdown only with the newly compiled AOT
executables: the same executable ran faster on the preceding runtime, while the
old executable remained fast on both. Normalized float-v3 instructions were
unchanged apart from cold string offsets. Crossed saved-artifact measurements
support a layout interaction, not a proven microarchitectural explanation.
The 64-byte candidate restores the lower timings for both saved new executables
at 1024 and 4096 elements, with broader validation recorded in the benchmark
results. It increases runtime text by 38,816 bytes over the 16-byte candidate;
neither unchanged semantics nor fixed alignment proves universal neutrality.
The final newly compiled primary still contains slow floating-callback forks
in both baseline and candidate. Its median and the larger-batch repeat are
near flat; polymorphic controls retain a small unfavorable median movement.

An experimental native-only class-local lowering was rejected before timing:
separate native/runtime cleanup phases changed destructor order on exception
exits, including mixed typed/`auto` locals. `AOTClassBodyLocals.qtest` retains the
AST/AOT lifetime regression coverage. The same fixture also exposes an existing
JIT mixed-local unwind-order mismatch in the saved baseline; this AOT store
specialization does not fix that independent issue. Extending native-only local
admission requires a unified declaration-order cleanup proof first.

### AOT method context elision

The resolved dot-eval fast-method path (also used by polymorphic dispatch-cache
hits) reuses the final IR's conservative implicit-context requirements. For a
ready AOT context, a caller in the method's execution Program and no attached
debugger, it can omit the synthetic `self` local when unused and omit the `argv`
local/context when unused and no excess arguments were supplied. Parameters,
object/defining-class substitution, receiver type information, variable-frame
boundaries, diagnostic frames, parse policy/location and return filtering remain.

Flags are read from the acquire-published, immutable `QoreAOTContext`, not the
variant's mutable `usesSelf()`/`usesArgv()` fields that IR materialization may
rewrite. A missing context keeps the existing setup and does not trigger eager
materialization. IR/JIT, cross-execution-Program and debugger calls keep their
original context setup. The process presence flag
`QORE_DISABLE_IR_CONTEXT_ELISION`, sampled on first fast-method use, disables
this optimization along with the existing context-elision controls.

Explicit `self`, self members/calls, method closures and opaque AST access are
handled conservatively by the existing context-use analysis. Excess arguments
retain their original list allocation, ownership and cleanup order even when
the body does not read them. `AOTDotEvalOverload.qtest` covers mixed receivers,
private members, explicit self, closures, recursion, nested contexts, argv,
object-argument destruction and exception recovery against source, qmod and
disabled-cache/context controls. No method entry or serialized ABI changes.

### Native scalar entries on polymorphic cache hits

A whole-body-proven, context-independent, non-throwing `int(int)` instance method
can publish its existing typed native companion through
`qore_rt_bind_method_int_unary_v1` when its normal AOT entry first executes.
The binding checks the normal body's identity against the context's `pc_loc_fn`.
An append-only context field holds the acquire/release-published entry; repeated
generic calls use a relaxed already-bound check before attempting publication.
No `QoreAOTFunc` table stride or serialized symbol-index format changes, and old
native artifacts have no binding and retain their existing path.

A polymorphic cache hit still validates the resolved override, receiver validity,
dispatch epoch/access key, exact integer argument and actual `int(int)` signature.
The native path additionally requires a ready context/PC map, unchanged normal
body, matching caller/object/execution/context Programs, no debugger and no
generic receiver. Imported/specialized/capturing bodies, self/argv users, optional
or changed return contracts and potentially throwing expressions are excluded.
The bounded whole-body proof is shared with the function-reference adapter;
an integer annotation alone is not sufficient.

The runtime passes an unboxed integer to the native entry without parameter TLS
binding, then returns a normally owned boxed integer, including full-width values.
It retains object/defining-class substitution, explicit `RuntimeConfig` frame
setup, empty local/closure markers, source/parse-policy state and the stack-reader
publication handshake. Both runtime and native-entry stack checks remain. The
bounded scalar body introduces no loop or cancellation omission. This v1 path
retains boxed caller arguments/results and the original native-entry stack guard.

An optional `qore_rt_bind_method_int_unary_v2` companion re-emits the same whole
scalar proof with an explicit live `RuntimeConfig` and its own v2 stack guard.
The v1 entry remains separately callable; the extra context field is appended,
without changing serialized table strides or old generated field offsets.
Eligible speculative object calls first retain their exact-class fast/inlined
branch, then try `qore_rt_try_method_int_unary_v1` on a subclass miss. This helper
borrows the receiver and looks only in an existing immutable PIC. Its fixed
unary-integer key avoids constructing a general argument array, but compares
every receiver class/type, normalized access class, caller/object Program,
parse-option, epoch and argument type/state component. Access class and parse
options come from the live thread view, not refreshed compatibility snapshots.

An unwritten receiver parameter can still be replaced through reflective local
assignment. A hoisted exact-class result is therefore reusable only when the
call's actual receiver is identical to the entry receiver. The existing owned
entry-load cleanup must retain that original object, preventing address reuse;
without that owner the compiler keeps the per-call class check. Receiver validity
is still checked on every call. An identity mismatch selects normal dispatch,
including subclass overrides, without changing local storage or destruction
order. No source-level write is not a sufficient cache-validity proof. Old AOT
artifacts must be recompiled to receive this generated guard.

The runtime `qore_rt_object_is_valid` helper specializes pointer-tagged values
with direct node-type inspection, including opaque strong references. It still
calls the ordinary live `QoreObject::isValid()` on every invocation; object
validity is never hoisted or cached. Null pointers and non-object nodes fail;
non-pointer values retain generic dispatch, including enum handling. This does
not change object ownership, status synchronization, access/Program guards,
compiler output or the helper ABI. Identical-AOT comparisons at 1024/4096 loop
iterations show 26.0%/31.7% lower monomorphic kernel time (about 1.85 ns/element),
with 56 bytes of additional runtime text. Other call kinds do not show a repeated
gain. See `bench/runtime-comparison/results-receiver-validity-macos-arm64.json`.

An invocation-local prepared method-PIC entry for stable required/nullable
receivers was measured and rejected. After correcting scalar-cleanup integration,
it showed no improvement at 1024 iterations and a 2.02% nullable regression at
4096; the descriptor/helper/emitter are not retained. Existing scalar callback
preparation is unaffected. See
`bench/runtime-comparison/results-prepared-method-rejected-macos-arm64.json`.
The experiment's stress tests exposed an independent diagnostic lifetime bug:
the native-PC registry now owns interned immutable location/string snapshots,
rather than borrowing from destructible Program contexts. Its locked ordered
map replaces repeated registrations at the same native function base; old
snapshot pointers remain valid after readers release the lock. The cache owns
no Program and deduplicates equal values for the process lifetime.

Separating cold stack-overflow construction from native limit checks was also
measured and rejected. Automatic inlining removed the standalone check's hot
frame but enlarged method dispatch; an out-of-line variant restored its frame
size but produced mixed timings and incomplete debug validation. Neither
runtime variant is retained. See
`bench/runtime-comparison/results-stack-check-outline-macos-arm64.json`.
The native stack regression now tests O0/O3 C++ frames with non-scalar-replaced
padding. Polymorphic receiver-local initialization/ownership remains a separate
target from dispatch and cannot be removed merely from a matching class type.

Plain-class AOT parameters can avoid repeated reload-helper calls only with
both a compiler proof and live invalidation. Admission excludes written,
captured, reference/weak, AST-visible, self/top-level and parameterized
class parameters, and requires the existing preinstantiated entry cleanup owner.
The runtime additionally rejects absent, finalized, cleared, static, unassigned
or non-strong slots and requires raw slot identity with the retained entry value.
That identity check also protects against reentrant evaluation of another
parameter before the snapshot. Class body locals remain runtime-backed.
Optional plain-class contracts can also qualify, but only on that positive
strong-slot check; `NOTHING` and raw reference/weak slots keep ordinary reloads.
Complexity is checked on the original declared type before nullable type folding,
which can otherwise erase a generic class's arguments. Optional object locals
are excluded from IR-only storage just like required objects, preserving runtime
slot synchronization, reflective visibility and scope cleanup.

Ordinary read-only `auto` parameters can also use this cache when a direct
`LoadLocal` result is used as a method receiver. This is a profitability
heuristic, not a static proof of object type: the existing entry owner and
runtime-backed reload support are required, and the runtime must still prove
an identical ordinary strong-object slot. Non-object, reference and weak slots
keep ordinary reloads. Scalar/container-only parameters are not charged for
this new guard; dynamic pseudo receivers can still reach and reject admission.
Reflective writes revoke reuse before reentry, without caching method dispatch,
access checks or receiver deletion status. Private direct-entry arguments are
excluded. `QORE_DISABLE_AOT_OBJECT_PARAM_CACHE=1` disables this extension at
compilation; `QORE_DISABLE_AOT_CLASS_PARAM_CACHE=1` disables both caches.
This is AOT-only, reuses the existing helper ABI, and requires recompilation.
See [the measurements and rejected broader heuristic](../bench/runtime-comparison/RESULTS.md#aot-object-valued-auto-parameter-cache-2026-10-11).

`ThreadLocalVariableData` owns a same-thread, per-Program 64-bit mutation epoch.
Both reflective assignment APIs reach the target stack's `setVarValue()`, which
increments the epoch before lvalue resolution or assignment can reenter Qore.
It saturates at `UINT64_MAX`; this value permanently rejects cache admission.
The versioned AOT helper obtains a borrowed counter address through the live
`RuntimeConfig`, not a copied context or per-iteration TLS lookup. The address
must not escape its invocation/thread; no reference ownership is transferred.
An unchanged epoch skips the reload helper. Any write permanently revokes this
invocation's entry-cache reuse. Each newly observed epoch forces freshness once,
then preserves normal tracker/deferred cleanup and reference evaluation; a
reference installed reflectively can subsequently change without a new write.

AOT multi-block functions (and non-returning single blocks) with any runtime-
reloadable local create their ordinary reload epoch before lowering the first
read: a read lowered before a call can execute after it through a loop backedge.
This is independent of cache eligibility, including written, optional or
AST-visible locals. This correctness fix also applies
with `QORE_DISABLE_AOT_CLASS_PARAM_CACHE=1`; straight-line returning functions
retain lazy setup, and admission is materialized only when a reload is needed.
The new helper is additive; new artifacts require the matching runtime and old
artifacts need recompilation to get the generated fix. Existing old artifacts
were separately checked on the new runtime.

The required- and optional-class AOT reflection/backedge regressions are fixed,
including transitions from `NOTHING` and changes to reference-valued slots.
IR/JIT caller-frame reflection and zero-argument fast-frame visibility also have
pre-existing limitations. These are not treated as passing or fixed by this
optimization. See `results-class-param-cache-macos-arm64.json` and
`results-optional-class-freshness-macos-arm64.json` for reproduction, scope, full
samples and audit; correctness repairs and on/off cache savings are reported
separately from comparisons with older partly unchecked paths.

The helper rechecks deletion, actual body identity/readiness, Program and debugger
state before calling the companion. Missing admission/companion data requests
effect-free generic fallback; no receiver or argument ownership is consumed.
The frame remains observable and retains its reader handshake. A sole exact
native `StoreLocal` consumer receives a native-value/handled side channel; its
boxed cleanup slot contains `NOTHING` only on native edges. Generic fallback
results retain their real boxed value, assignment checks and cleanup. In
particular, an integer annotation never licenses converting a subclass's
`NOTHING` or incompatible return to zero. Normal and invoke paths retain a sink
check after merging with the non-throwing helper. Runtime-backed receiver locals
and generic-call invalidation remain unchanged.

The native method diagnostic-frame constructor/destructor use the same explicit
inlining hints as callback frame wrappers. This exposes context save/restore to
the dispatch caller's register allocator without changing fields, publication
order, variable markers, guards or reader synchronization. It is a runtime-only
optimization; identical AOT artifacts can be compared across compatible saved
and new libraries. Measurements use the normal single-compilation-unit build;
code generation and benefits on other builds are not guaranteed. See
`results-method-frame-inline-macos-arm64.json` for repeated nullable-call gains,
overlapping controls, runtime code growth and native-entry-disabled comparisons.

The unary-int method-v2 continuation can also defer thread-private Program-call,
closure, statement/location, parse-policy and empty local/closure marker setup.
Since its introduction, this entry has re-emitted a complete non-throwing native
scalar body with an explicit live-stack check before any body operation. That
compile-time proof excludes allocation, TLS context access and Qore reentry.
The runtime retains full-key admission, live body/map/Program/debugger checks,
object/class context substitution, the dispatch-entry stack check and the
published diagnostic frame with its reader handshake. The callee's full guard
materializes both variable markers and the current context before constructing
an overflow exception, including allocation rollback and live parse-policy
override; successful calls have no deferred state to restore. Method-v1 keeps
its eager path. `QORE_DISABLE_AOT_NATIVE_METHOD_DEFERRED_CONTEXT=1` is a
runtime-only eager control; it does not disable the native continuation or body.
Only the private runtime leaf helper uses a typed metadata discriminator for
callback caches versus invocation-owned method context. No generated callback
descriptor, method binding ABI or serialized metadata layout changes; floating callback marker policy
remains eager. The boundary test checks required and nullable native method
continuations with caller-owned local and closure stacks still live.

The same whole-body proof now defers method object/class substitution as well.
An invocation-owned descriptor borrows the already-owned receiver, live
RuntimeConfig and immutable method cache entry. Only overflow materialization
saves and installs the live object/class and the independent RuntimeConfig
snapshot; exception cleanup restores both views after frame removal and reader
completion. Successful pure calls leave both views untouched. The descriptor
outlives the published frame and adds no receiver reference or callback-frame
storage. Ordinary methods, method-v1 and the eager control retain eager
substitution. No admission guard, diagnostic frame, stack check, emitted ABI or
serialized layout changes. The native fixture verifies private member access
and both context views after an actual callee overflow, distinct snapshot
restoration on success/miss/failure, and reuse afterward. Repeated same-artifact
measurements show about 11-12.4% lower nullable-call kernel time; polymorphic
evidence is mixed. See `results-method-object-context-macos-arm64.json`.

An additive method-v3 companion applies the same live-stack view as callback v3.
It exists only after the complete non-throwing scalar proof and successful v2
emission. Every invocation loads the live limit and its current native SP; a null
view or failed unsigned comparison tail-calls v2 with a noinline callsite and
returns directly, so the hot body need not preserve arguments across the full
guard. Only supported downward-growing 64-bit aarch64/x86_64 runtimes provide a
nonnull view; other targets retain the full helper, including IA64 register-stack
checks. No limit value or previous SP is cached. The dispatch entry guard,
receiver/access/full-PIC-key/Program/body/readiness/debugger checks and published
frame remain. The full overflow path still materializes private context before
raising. A typed atomic is appended to the runtime-owned AOT context, preserving
prior generated field offsets; v1/v2 entry signatures and serialized layouts are
unchanged. `QORE_DISABLE_AOT_NATIVE_METHOD_INLINE_STACK_CHECK` is a method-only
compilation control. Old artifacts retain v2; new artifacts require the additive
binder, so compiler/runtime comparisons must compile both artifacts separately.

The native boundary fixture checks actual v3 binding, current-SP/live-load LLVM,
both method and callback compilation controls, O0/O3, and eager/deferred context.
Its test-only adapter delegates to the real compiled entry with a null view or
with the same-thread limit changed after frame publication. This verifies the
callee's own overflow and recovery even when its smaller native frame stays
above the dispatch guard's SP throughout an external boundary sweep. The test
restores the live limit on every exit and the original bound entry before its
unshared Program is destroyed; production has no test-only guard bypass.

The native method continuation's receiver gate uses `QoreValue::isPointer()` and
the borrowed node's `NT_OBJECT` type, without general scalar/enum type dispatch.
Opaque strong references remain admissible; weak wrappers, non-pointer values,
and non-object nodes return to ordinary dispatch without changing ownership,
the result slot or exception sink. Object validity and every subsequent access,
full PIC key, Program, body/readiness and debugger check remain live. No public
type API, ABI, serialized format or generated caller changes. The native fixture
warms real required/nullable caller caches and invokes the helper from the
compiled caller's parse-policy context. It requires actual admission for strong
and opaque objects, rejects scalar/boxed/string/container/enum/weak values, and
checks context restoration and successful reuse after misses. The same fixture
passes on the pre-change runtime, so it checks preserved behavior rather than
new semantics.

The native fixture also warms an int-backed enum argument entry before an
ordinary-int entry. Although both resolve to the same int(int) method, their
dispatch argument keys differ: an enum-only cache must reject the integer
continuation, then admit it after the later integer entry is published. The
enum remains first in the PIC, and ordinary enum dispatch still succeeds after
integer calls. A separate publication-time argument-key flag was tested but
not retained: it reduced generated helper size without a measured target
speedup at either tested workload size. The existing full-key checks remain.

Unresolved name-dispatched calls can also use this continuation after ordinary
dispatch has populated the full-key PIC. This includes nullable-class and dynamic
receivers; it does not resolve an AST/IR target or narrow its unknown return type.
Only an already-native integer argument and a sole exact native-int local store
are eligible. Generic instantiations, boxed/optional arguments and escaping
results keep the original path. The shared normal/invoke emitter builds boxed
arguments only on fallback and preserves invalidation, cleanup and sink checks
on merge. Cold caches, absent native admission and every failed live guard fall
back to unchanged name dispatch. No new ABI or cache layout is introduced.
`QORE_DISABLE_AOT_NAME_METHOD_NATIVE_CONTINUATION=1` disables this compiler-only
extension; compile separate artifacts for comparison. The broader native-method
entry/continuation controls disable it too.

`AOTNameMethodContinuation.qtest` covers normal/invoke and dynamic calls, wide
integers, mismatched returns, NOTHING, deletion, private access, overloads,
generic and methodGate fallback, weak receivers, cross-Program restoration,
concurrent frame readers and debugger attachment. It checks stripped O0/O3,
C++ EH, parallel compilation and disabled controls against AST/IR/JIT/tiered
results. See `results-name-method-continuation-macos-arm64.json` for measurements,
including adverse control movement and the remaining Java gap.

`QORE_DISABLE_AOT_NATIVE_METHOD_CONTINUATION=1` at compile and runtime selects
the v1 control path. New callers require the additive helper symbols; old
artifacts continue to run. Before/after compiler comparisons must compile both
artifacts separately. This only covers proven unary integer bodies, not arbitrary
methods, multiple arguments or cross-Program calls.

The resolved fast-method fallback also rejects attached debuggers before entering
native code, allowing `evalTiered()` to use AST or lazy source-stripped debug IR.
`AOTNativeMethodEntry.qtest` covers registration/exclusions, normal/invoke calls,
wide ownership, foreign Programs, concurrent first calls, concurrent stack readers
and debugger attachment after warming the cache. The debugger regression checks
function entry/exit events; pure scalar debug IR need not retain a separate
statement temporary marker.

`QORE_DISABLE_AOT_NATIVE_METHOD_ENTRY=1` suppresses binding at compilation and
native admission at runtime. Compiler comparisons require separate artifacts:
old runtimes cannot load a new executable referencing the additive helper.

Once the native companion is available, PIC construction records immutable
admission facts for the actual resolved override: unary integer signature,
static fast-method eligibility, execution Program and defining class. A prepared
hit bypasses repeated RTTI, signature scans and execution-Program lookup. These
three borrowed pointers are initialized before release publication and have the
same retained-entry lifetime as the existing method/variant pointers; there is
no additional allocation or serialized ABI change.

The full PIC key, actual integer value, receiver validity, native-body identity,
context/PC-map readiness, current Program and debugger state remain live guards.
If the PIC is populated before companion publication (for example, on its first
call under a debugger), it retains the ordinary native-admission path, which can
discover the entry after debugger detachment. Regressions cover this order and
receiver deletion after warming the same native call site.
`QORE_DISABLE_AOT_NATIVE_METHOD_ADMISSION_CACHE=1` disables only this immutable
admission cache, retaining the native companion. This runtime-only follow-up can
reuse an identical AOT executable against before/after runtimes that both support
the versioned binding.

Native method object/class substitution also reuses the invocation's explicit
`RuntimeConfig` and live thread view. `RuntimeConfigNativeObjectHelper` saves
and restores both the live compatibility fields and the snapshot fields
independently, without looking up either context again through TLS. It changes
no object ownership, frame publication, stack check or admission guard. The
generic `ObjectSubstitutionHelper` remains unchanged. The native context must
belong to this thread and remain valid for the helper's entire scope.

### Guarded AOT single-member scalar entries

AOT can emit a separate native companion for a non-synchronized `int(int)` or
`float(float)` method whose complete body contains one member read and otherwise
only homogeneous scalar arithmetic. The proof is bounded to one block and at most
64 instructions. It permits the sole parameter, same-family constants, add/subtract/
multiply and integer bitwise operations; it excludes calls, stores, extra member
reads, explicit self-local access, opaque AST access, body locals, generic type
substitution and unsupported instructions. The ordinary body and serialized debug
IR are unchanged. A member's declared type alone never authorizes a native read.

The companion receives the native argument, borrowed receiver and defining-class
context, live `RuntimeConfig`, an accepted flag, AOT context and exception sink.
The normal body publishes the typed companion only for its own body identity.
Dispatch retains the complete PIC key, receiver validity, access context,
same-Program/options, dispatch epoch, live body/map and debugger checks. The
existing integer-forwarding proof is tried first. Companion pointers are appended
to the runtime-owned context with release/acquire publication; no mutable member
value or receiver is cached there.

At the source read position, the helper selects the member's internal storage
context and acquires the ordinary object read lock. It rejects deleted objects,
missing/NOTHING fields, references, enums and the wrong scalar family without
evaluating, referencing or discarding the value. Immediate values and canonical
wide-integer/escaped-float nodes are copied to native scalars while locked. A miss
clears the accepted flag and retries ordinary execution; only effect-free work
can precede that miss. A successful native return follows the separately proven
homogeneous type, without invoking a dynamic return conversion.

Member-name XXH64 and XXH32 hashes are computed at compilation; the helper selects
the runtime target width. Builds without the hash-map implementation use the same
ordered-map lookup as ordinary reads. This removes per-read name hashing, not
synchronization. The diagnostic frame and its reader
handshake remain published. The callee checks its own stack through the explicit
runtime view; overflow materializes object/class context before raising and restores
it afterward. The existing eager-context diagnostic control remains supported.

A separate positive declaration proof can remove member-context lookup: for a
user defining class, `QoreMemberInfo::isLocalInternal()` and exact declaring-class
identity establish that `runtimeGetMemberContext()` always returns that defining
class, even when a receiver subclass shadows the name. The companion receives the
live variant's defining context; it never embeds the compile-time pointer. Class
copies preserve the local/Internal declaration and retarget its declaring class.
Parsing cannot modify committed user classes; native member addition cannot replace
an existing declaration under its API contract, and inherited-member merging does
not replace a local Internal declaration. AOT loading skips methods as well as
members for pre-existing classes. No absent-member fact or metadata pointer is
cached, so this does not rely on method epochs invalidating member additions.
Public, ordinary private, inherited non-local and unknown declarations retain the
ordinary context lookup. Both paths use the same locked live scalar extraction.

`QORE_DISABLE_AOT_SCALAR_MEMBER_CONTEXT=1` disables this declaration proof during
compilation or restores ordinary lookup in the generated helper at runtime.
Disabling member prehash also restores ordinary context lookup. Tests exercise
both storage paths, including private shadowing, internal concurrent writers and
direct native integer/float hit, miss, ownership and stack-context probes.

`QORE_DISABLE_AOT_SCALAR_MEMBER_ENTRY=1` disables companion generation or runtime
admission. `QORE_DISABLE_AOT_SCALAR_MEMBER_PREHASH=1` independently disables hash
generation or uses the ordinary read helper at runtime; the broader compiler
control `QORE_DISABLE_AOT_PREHASHED_KEYS=1` also suppresses prehash generation.
New artifacts require matching additive runtime helpers and private identity.
`AOTScalarMemberEntry.qtest` covers live/inherited/shadowed reads, reference and enum
fallbacks, unusual scalar encodings, debugger attachment, frame readers and concurrent
field replacement. `NativeLeafContext.qtest` additionally proves real native hits,
untouched miss outputs, boxed fallback ownership, forced callee overflow and context
restoration on initial and foreign threads. These are AOT-specific companions;
IR/JIT execution remains a correctness reference, not a claimed performance gain.

Runtime dispatch acquires the typed member companion once and passes it to a
context-policy-specialized helper. Deferred and eager policies retain their
respective frame/context protocols, but the eager policy's larger local storage
does not inflate the deferred helper's stack. The non-deferred pure scalar-method
invocation is also outlined for this reason. All PIC, live-body, Program, debugger,
stack and ownership guards remain in place. Diagnostic controls are sampled on
first use of their path; set them before starting the process. A packed shared
options snapshot was measured and rejected, not retained.

### Guarded AOT member-update prefixes

An additional `int(int)` companion admits a bounded prefix of standalone implicit
self-member increment/decrement statements followed by the existing single-read
scalar proof. The whole body must fit one block and 64 IR instructions, with no
body locals, explicit self-local access, opaque AST access or generic signature
substitution. Each prefix statement is exactly a matching push/update/discard
triplet with no exception target or dynamic path operand. Update results cannot
feed the suffix; the scalar proof starts with an empty SSA set at that boundary.
Unsupported methods keep their ordinary entry.

The normal body publishes an immutable, module-owned fast-entry/continuation pair
for its exact body identity. Release/acquire publication installs the descriptor
before the shared integer member-companion pointer. Read-only and update-prefix
proofs are mutually exclusive; neither binder replaces an existing binding.
No receiver, mutable member value or per-invocation progress is shared there.

The fast entry receives the argument, receiver, defining class and live
`RuntimeConfig` explicitly. Each update resolves access/storage against the live
receiver and takes its own ordinary object write lock. Only existing inline
integer storage whose unit-step result also fits inline may be changed. A miss
does not create storage, evaluate a reference, release ownership or change a
value. XXH64/XXH32 member-name hashes are computed at compilation, but access,
storage selection, deletion, representation and locking checks remain live.
The suffix read has its own lock at its source position: reads are not hoisted
and the statement sequence is not made atomic by combining locks.

The accepted result distinguishes success (one), an effect-free read-only miss
(zero), and an update-prefix miss (completed updates plus two). The latter pops
the compact borrowed frame before installing an ordinary observable method
frame, self/argument slots and object/class context. The cold entry skips exactly
the completed statements, not their remaining suffix, and never requests a
whole-body replay. PIC-owned diagnostic metadata is copied before generic
reentry can retire the caller cache. Return declaration validation remains
inside the callee frame; normal cleanup and the post-cleanup filter still run.
Wide arguments and cold returns retain ordinary ownership.

Ordinary AOT bodies containing generic lvalue operations now keep runtime-backed
parameter slots and use the existing publication/reload protocol. Such operations
can release a node whose destructor reflects into the active frame. The cold
continuation therefore observes parameter replacement instead of retaining a
stale native argument. This is not a general solution for every private direct
entry or other hidden reentry path. Caller-PIC retirement tests likewise do not
establish arbitrary active-callee-context replacement safety.

`QORE_DISABLE_AOT_EFFECT_MEMBER_ENTRY=1` disables generation or publication of
the pair. The existing deferred-context disable also prevents publication.
`QORE_DISABLE_AOT_EFFECT_MEMBER_PREHASH=1` separately disables generated update
hashes or routes the new helper through its ordinary lookup implementation.
`QORE_DISABLE_AOT_PREHASHED_KEYS=1` suppresses prehash generation globally.
Read-only companion controls remain independent. Set controls before startup.
These new additive helpers and companion metadata require matching compiler/
runtime artifacts; an old executable does not gain a companion by relinking.
The optimization is AOT-specific, with AST/IR/JIT used as correctness references.

`AOTEffectMemberEntry.qtest` covers partial-prefix misses, representation boundaries,
references, internal shadowing, destructor reflection/throws, return-error frames,
cross-Program calls, debugger/frame readers, concurrency and cancellation.
`NativeLeafContext.qtest` additionally intercepts the real compiled pair to prove
fast hits and exact resume counts, verifies v1/v2 result ownership and caller-PIC
retirement, and forces stack errors on initial and registered foreign threads.
Measured gains and adverse controls are recorded in the runtime-comparison results.
The corrected revision passed selected release/debug qualification and fresh
matched AOT/Java measurements: member-update work takes about 55% less time than
the preceding committed baseline, with a roughly 1% boxed-integer regression and
slower native-integer medians retained as explicit tradeoffs. No Java parity,
whole-suite, sanitizer or cross-platform claim is made.

### Checked inline self-member integer updates

The shared LLVM lowering already routes direct self-member assignment, compound
assignment and increment/decrement through runtime helpers in AOT and JIT code.
The update helper now resolves access/type/storage once and takes the ordinary
object write lock and existence pin. If both the current value and its incremented
or decremented result fit the inline integer representation, it updates in place
without constructing an object-count hierarchy. This is a representation proof,
not a declared-type shortcut: ordinary integer updates also accept an existing
integer before consulting the declaration. No node ownership, recursive graph
count or container count changes on this path, and it cannot call user code.

Every miss hands the same locked lvalue to the ordinary operation, including
reference resolution and conversion; it never restarts lookup or repeats a write.
Wide integers, inline-boundary transitions, enums, floats, numbers, references,
missing values and node conversions retain ordinary behavior. Access checks,
internal/inherited storage selection, deletion checks and one lock per operation
remain. No lock is held across source statements. The shared self-member lvalue
helper also stops recording the object context twice: its lookup already records
it. Assignment, compound and generic update continuations retain that single
context and their normal deferred destruction/graph bookkeeping.

`QORE_DISABLE_SELF_MEMBER_INT_UPDATE=1` disables only the inline integer update,
not context deduplication or scalar-method helper specialization. It is sampled
on the first representation-eligible operation, so set it before process startup.
Initialization reads the environment only; no Qore callback or second object lock
is acquired. Non-integer and wide-result misses do not initialize this control.
`AOTSelfMemberIntUpdate.qtest` covers all four operations, representation boundaries,
internal shadowing, generic/optional/soft declarations, destructor reentry,
references, removed/deleted storage and concurrent cold/shared-object updates.

These changes are runtime implementations of existing exported helpers, not new
helper signatures, serialized layouts or a new calling convention. Compatible
existing AOT artifacts that call those helpers can benefit from the updated
runtime; artifacts using other lowering paths do not acquire new calls merely by
relinking. The self-member changes also apply to JIT-generated helper calls; the
IR interpreter is unchanged. Performance acceptance is based on AOT measurements,
not an inferred IR/JIT gain. The measured tiny dynamic-integer control retains a
1.3–1.6% regression against the qualified baseline; Java parity remains open.

### Guarded AOT method entries after typed assignment

Exact boxed-local analysis can prove a receiver assigned and non-NOTHING while
recording only the generic object representation in its value facts. Its separate
`exact_assigned_boxed_local_types` map retains the enforced declared class.
Normal and exception-routed dot-eval fast-entry selection consult that type via
`qore_ir_value_input_type`, rather than losing the class proof at this boundary.
The assignment/non-NOTHING proof, target/signature/body eligibility, no-binding
argument proof and runtime receiver-validity/exact-class guards remain required.

This only expands admission to the existing context-independent native method
entry; it changes neither its ABI nor runtime dispatch. An overridable target's
exact-class hit may import its arithmetic summary, but other receiver classes
still use normal dispatch. Their return contract can differ from the base's,
so speculative merges preserve the fallback's boxed result and do not propagate
the base's non-NOTHING or local-effect summary across that fallback. Native
fast-hit results are boxed with their normal ownership before merging.
Missing/deleted receivers and unassigned arguments retain the checked fallback.
`AOTAssignedMethodFastEntry.qtest` checks both
normal and exception-routed code generation, subclass/throwing overrides,
optional and changed return contracts, captured-variable mutation, full-width
scalar ownership, missing receiver/argument behavior, and disabled-fast-entry controls.
Existing AOT artifacts must be recompiled to use the changed guards and result
merging; runtime ABI compatibility does not rewrite their native code.

## Opaque Function-Reference Calls

IR/JIT/AOT runtime dispatch can bypass `QoreListNode` and dynamic binding for a
same-Program `FunctionCallReferenceNode` with exactly one user variant and
exactly matching, already evaluated arguments. This guard excludes defaults,
reference parameters, synchronized/generic/no-narrow binding, domain-bearing
functions, and attached debuggers. The variant and reference must both belong
to the current Program. It is deliberately bounded to 16 arguments. Other
references, including methods and foreign-Program references, retain normal
dispatch. The existing IR direct-parameter convention is reused when eligible.

For AOT references, a new optional internal adapter has the C++ ABI
`uint64_t entry(const uint64_t* args, QoreAOTContext* ctx, ExceptionSink* xsink)`: the argument array
contains borrowed NaN-boxed values and the return value is owned. It converts
to the existing per-function typed fast-entry ABI; parameters do not pass
through TLS. Only compiler-proven context-independent, non-throwing scalar
expressions qualify (bounded integer/boolean or floating-point summaries of
at most 64 nodes; division, shifts, container access, and boxed returns are
excluded). The adapter supplies the callee's own AOT context for stripped-debug source
locations and return-type caches. Context-dependent bodies, imported fast
entries, specializations, and closures are excluded. Integer/float boxing uses
the full-width helpers, not a truncated inline encoding. Other eligible
references still avoid argument-list binding through the runtime fast path.

The scalar-only proof is required because the lazy PC location registry does
not yet describe adapter symbols. Bodies that can raise ordinary Qore
exceptions must enter their registered normal function, preserving exact
throwing locations. Expanding eligibility requires location-map registration
for adapters first; the explicit context argument alone is not sufficient.

The compiler attaches the adapter to a newly created reference before it can
escape. Attachment and each invocation compare the registered normal AOT
entry's address with the adapter's normal-entry identity. A matching name or
signature is not sufficient. The pointer fields are immutable after reference
publication; the reference's existing Program lifetime rules still apply.
This is additive: no serialized descriptor, public ABI, or existing AOT entry
changes. Previously compiled artifacts use the guarded runtime path without
an adapter. Newly compiled artifacts require the new runtime helper.

Native reference calls retain stack checks, the caller Program context,
diagnostic frames, runtime parse options/location, lazy PC maps, and the C++
exception boundary. They omit TLS parameter/body-local/argv instantiation only
under the existing context-independence proof. An attached debugger forces the
generic path. Argument cleanup occurs after the borrowed call has completed,
with its owned result protected across cleanup. `IRFunctionRefCalls.qtest`
checks all execution modes and both disabled paths. Compile-time
`QORE_DISABLE_AOT_FUNCREF_NATIVE_ENTRY` suppresses adapters; runtime
`QORE_DISABLE_FUNCREF_FAST_CALL` disables the complete guarded path (both are
diagnostic presence flags, the latter sampled at first use).

References now prevalidate the finalized, single-variant signature at creation
and retain immutable pointers and a two-bit scalar tag per parameter. A
reference created before parse finalization remains on the generic path.
Execution mode, compiled-code readiness, active Program, debugger state and
actual argument types are still checked at invocation; they are not cached.
This removes repeated static eligibility scans without a shared target cache.

AOT also provides an additive typed-reference ABI v1 for zero or one strict
`int`, `float` or `bool` argument and a strict scalar result. It retains the
same non-throwing, context-independent body proof and normal-entry identity
guard. The native entry receives scalar arguments followed by the callee's
explicit AOT context and exception sink; boolean arguments/results use an
`int64_t` 0/1 carrier. Twelve signature-specific runtime helpers return a
handled flag and write the native result through an output pointer. Rejection
leaves the general call responsible for invocation and ownership. Attachment
validates the signature against the immutable reference metadata.
Native integer adapters also reuse complete scalar-leaf summaries instead of
requiring a duplicate integer-expression proof: add/subtract/multiply and
bitwise and/or/xor, affine compositions, and integer comparison/select bodies.
The opcode allowlist excludes throwing operations and unknown kinds. Re-emitted
callee v2/v3 bodies use the existing checked scalar-leaf emitter, including
operand/type validation and summary-disable controls. Signed arithmetic wraps
without LLVM no-overflow promises. No runtime guard or serialized format changes.
Native float adapters also consume the complete `FloatBinary` leaf summary
for add, subtract, and multiply. The float-expression builder intentionally
does not duplicate that summary; ignoring it previously left these simple
bodies on generic dispatch. Complete float-expression summaries also admit
typed comparisons and non-throwing branch-return trees or a single four-block
ternary diamond. Equality and ordering use ordered comparisons; inequality
is unordered, preserving NaN behavior. Only a proven boolean may select two
float results, including signed zero. Division, side effects, generic reentry,
loops and handlers cannot be speculated across branches. Optional imported
summary nodes are checked for operand types and topological bounds; older
readers decline the new node tags and retain ordinary calls. No serialized
record layout changes. The existing context-independence and full-body
proof still exclude TLS locals, captures, generic reentry, and throwing work.

Ordinary AOT call sites with a typed callable use this route after proving or
checking the argument representation. Unassigned values, container holes,
non-matching signatures and references without a typed entry fall back. The
caller normally reboxes the successful result at the merge with generic
dispatch so that fallback results, including NOTHING, retain their original semantics.
Arguments remain borrowed through execution and consumed temporaries are
released afterward. Invoke-form calls retain the previous dispatch. Thus this
is a native scalar call boundary; exact native-local assignments can also
retain an unboxed continuation as described below.
Assignments from boxed results to exact native `int`, `float`, and `bool`
locals now check the immediate value tag and unpack matching values inline.
That path skips type-path resolution, general assignment coercion, and the
out-of-line scalar conversion. It does not assume that a typed callable's
fallback returns its declared type: NOTHING, mismatched values, wide integers,
heap-encoded floats, weak references, and soft/timeout destinations retain the
existing coercion and exception handling. Cleanup slots are cleared before
reuse on either edge; the slow edge still registers owned coercion results.
This applies to JIT and AOT lowering, not the IR interpreter.
`QORE_DISABLE_NATIVE_SCALAR_ASSIGN_GUARD` selects the previous checks at
compilation for comparison. No callback ABI or runtime eligibility is changed.
After the main LLVM optimization pipeline, native scalar callback and unary-integer
method callers receive inline int48, bool and NOTHING cleanup guards. Other functions are
left unchanged: a broader pass regressed direct-call and dispatch-control
loops. Only local instruction/CFG simplification follows, avoiding the
substantial code growth observed when the full inliner
and loop unroller processed these guards. Heap-backed and opaque values keep
the original release helper, ordering and exception sink. This targets AOT,
including O0: JIT/IR do not emit the typed native callback/method helpers that
select the pass. There is no runtime ABI change; it is not a general
proof that non-pointer tags need no cleanup. The comparison switch is
`QORE_DISABLE_LLVM_SCALAR_DECREF_GUARD`, and code-size growth remains a tradeoff.
`QORE_DISABLE_LLVM_METHOD_DECREF_GUARD` suppresses only selection by the native
method try-helper; functions independently selected by callback helpers are
unchanged. Selection does not prove that every cleanup is empty: each value still
gets the exact immediate-representation guard, retaining owned fallback cleanup
at its original point. The name-method regression includes owned-object return
rejection/destruction and both compiler controls.

AOT now identifies callback results whose only IR consumer is a normal,
non-reference, non-closure StoreLocal into an exact same-typed native `int`,
`float`, or `bool` local. Such calls carry the helper's native payload and a
handled flag alongside the ordinary boxed fallback result. The native edge
puts NOTHING in the boxed cleanup slot, avoiding result boxing even for wide
integers and tag-colliding doubles. At assignment the handled edge uses the
payload directly; the generic edge retains full assignment checking and
conversion. The callback's declared type alone is never proof of its actual
fallback return. Cleanup-slot reuse, exception checks, and assignment location
updates stay in their original order. No runtime helper or serialized ABI changes.
Generic immediate scalars retain the inline assignment guard; a declined
native call does not force them through general coercion.

For arguments loaded from exact native locals, typed callback lowering also
reuses the existing flow-sensitive native-local assigned-state proof. This
avoids boxing a wide native integer only to reject it at the int48 tag guard
when flow-insensitive producer facts still say boxed/maybe-NOTHING. The local
must be proven safe from unassigned/lvalue-invalidated reads, non-reference,
non-closure, and an exact type match with an actual native LLVM carrier.
Boxed sources, unassigned locals and container holes retain representation
checks. This is a compile-time proof reuse, not a new runtime assumption.

Multiple-use results, boxed/optional/soft destinations, outer or captured
variables, weak/reference stores, and non-matching destination types retain
ordinary lowering. Invoke-form callbacks still use the generic call path.
JIT and the IR interpreter are unchanged. The compile-time comparison switch
is `QORE_DISABLE_AOT_FUNCREF_NATIVE_CONTINUATION`; the focused
`IRNativeFunctionRefContinuation` test covers O0/O3, disabled paths, EH,
source stripping, split codegen, wide values, NaNs, owned rejected returns,
cleanup reuse, foreign Programs, and captured callbacks.

Parallel AOT code generation recovers native PC/source-location maps from
each partition before the partial link. This matters on Mach-O, where `ld -r`
drops the DWARF line tables. Function-relative maps and literal inlined-callee
locations accumulate on the compiler thread and are serialized into the final
object's linker-surviving section and trailer. Single-part fallback still
recovers maps after emission. Forced-split, source-stripped regression tests
check exact exception lines; compiler cancellation propagates during recovery.

`QORE_DISABLE_AOT_TYPED_FUNCREF_CALL` suppresses both typed-entry attachment and
typed caller lowering at compile time, leaving the boxed adapter available.
The ABI is additive and does not change serialized descriptors; old artifacts
remain supported, while newly compiled artifacts require these runtime helpers.

The runtime recognizes the exact internal FunctionCallReferenceNode type before
downcasting, avoiding a general RTTI inheritance search. Derived or other node
types keep virtual dispatch; no public node layout or virtual table is extended.
The Program identity guard remains first. Native reference diagnostic frames
borrow QoreFunction's stable name instead of constructing and copying strings
on every call. Only this frame specialization borrows: ordinary JIT/AOT frames
still own potentially synthesized names. The active invocation keeps the
function alive, and stack_helper remains the last member so the frame is
unpublished before any name storage can disappear. No frame, location update,
debugger fallback, exception boundary or policy check is removed.

### Explicit execution context

Typed callback-helper ABI v3 adds a per-call-site, invocation-local prepared
target descriptor after v2's RuntimeConfig argument. On the first eligible
call it retains the exact FunctionCallReferenceNode and caches only immutable
binding, signature, name and source-metadata pointers. A positive hit bypasses
boxed type decoding, exact RTTI and repeated metadata extraction. Current
Program identity, debugger attachment, normal compiled-entry identity, context
readiness and PC-map readiness remain live checks on every call. No body is
materialized merely to prepare a reference.

Each slot belongs to one native signature. Population validates that the actual
reference has that signature; its bindings are immutable after publication.
Prepared-only v2/v3 callee-entry accessors therefore use debug assertions rather
than repeat the signature comparison on release calls. Non-prepared accessors
retain their checks. Wrong-signature references and NOTHING still fail admission
before entry selection: they neither populate an empty slot nor invalidate an
existing valid target. The fixed-signature cache and native helper ABI are unchanged.

The descriptor's first word is an owned boxed reference and its validity token.
It starts as NOTHING and participates in the existing persistent invocation
cleanup, including exception exits; clearing that word invalidates all borrowed
fields even when a catch resumes execution. A different eligible reference
replaces the owner. Keeping the old reference alive until replacement prevents
allocator address reuse from producing false identity hits. Ineligible values
are never retained: captures, method-reference object lifetimes and generic
callback destruction timing stay unchanged. Exact function references have no
captures, user destructor or weak-reference semantics and retain only their
existing weak Program dependency. Slots never escape their native invocation
or cross threads. Frame publication and stack/cancellation checks are unchanged.

The descriptor layout and helper names are versioned native ABI, not serialized
AOT metadata. The compiler describes pointers using the target LLVM layout for
cross compilation. Existing v1/v2 helpers remain available; new v3 artifacts
require the updated runtime. `QORE_DISABLE_AOT_FUNCREF_PREPARED_TARGET` selects
v2 at compilation, independently of the v1 RuntimeConfig comparison switch.
The continuation regression additionally checks target replacement, foreign
targets, caught-exception reuse and debugger attach/detach after cache warm-up.

Typed callback-helper ABI v2 adds a borrowed `RuntimeConfig*` after the v1
arguments. Generated functions acquire it once in their logical entry, after
the native stack guard, and reuse it for their callback sites. The callee's
original scalar entry ABI and its `QoreAOTContext*` metadata argument remain.
`QORE_DISABLE_AOT_FUNCREF_RUNTIME_CONFIG` selects v1 helpers at compilation.
Both versions remain exported and registered; serialized metadata and the
public RuntimeConfig object layout are unchanged.

Typed callee-entry ABI v2 is separate from the helper version: it appends a
borrowed RuntimeConfig pointer after the AOT context and sink. V2/v3 helpers
prefer this optional entry; legacy v1 helpers and references compiled without
it still use callee-entry v1. Both bindings are immutable after publication.
The new binder requires the validated v1 signature and matching normal-body
identity. Rebinding v1 clears the optional v2 adapter so it cannot name a stale
body. The prepared-target v3 descriptor layout and old helper ABIs are unchanged.

The compiler re-emits only the already admitted complete, bounded, non-throwing
scalar summary, with a mandatory `qore_rt_check_stack_v2` entry guard using the
explicit context. It does not call the old fast entry, whose guard would still
look up TLS. Boolean carriers are normalized; summary-disable controls retain
the old adapter. No reference/capture, generic reentry, allocating expression
or throwing expression is newly admitted. Adapters do not inherit the creating
caller's debug scope; the existing published frame and lazy callee PC context
remain responsible for observation and stack-failure diagnostics.
`QORE_DISABLE_AOT_FUNCREF_CALLEE_RUNTIME_CONFIG` suppresses the new binding as
an independent compilation control. On these eager callee-v2 paths, all
caller/frame/callee stack checks, live guards, empty-frame markers and observer
synchronization remain intact. Pure callee-v3 calls use the deferred contract below.

Optional callee-entry v3 appends a borrowed live stack-limit address to v2.
Helper v2/v3 prefers it when present; prior callee entries and helper ABIs are
retained. The compiler emits a null check, a live 64-bit limit load and LLVM's
native stacksave SP comparison. A null view or failed comparison tail-calls the
v2 entry, which executes the full qore_rt_check_stack_v2 guard and uses its result,
including the original overflow exception. This cold return does not merge back
into the v3 body, so native arguments need not survive a helper call on the hot
path. The runtime supplies a non-null view only for managed,
downward-growing 64-bit x86-64/AArch64 stacks; other layouts/configurations
return null. This fixed-width view does not guess the host/default data layout
before the cross-compilation target is finalized. Neither the limit nor SP is
cached across calls. The view is borrowed only for the current thread's call.

The v3 binder requires the existing validated v2 binding and normal-body identity;
rebinding v1 or v2 clears v3. The admitted body and observation/lifetime proofs
are unchanged. QORE_DISABLE_AOT_FUNCREF_INLINE_STACK_CHECK suppresses only this
entry at compilation, leaving the explicit-context v2 callee available.
New artifacts that bind v3 require its runtime helper; old artifacts continue
to use their existing entries. No serialized metadata or prepared-cache layout
changes. Caller and frame guards remain unchanged; the callee guard is inlined,
not removed.

`RuntimeConfigNativeAccess` provides a live, invocation-scoped thread-state
view within the existing PIMPL. It is not a copied Program/object snapshot:
nested generic calls may refresh the TLS RuntimeConfig and enter another
Program. Native helpers read current state through the live view, so neither
that refresh nor a subsequent return makes the view stale. The pointer is
reacquired for each generated invocation and never attached to a shared
function/reference, accessed after thread teardown, or moved to another thread.
Ordinary compatibility entry points continue to refresh the legacy snapshot
fields with `rc_get_current_ref()`.

Generated JIT/AOT loop polls also use the live view. A function containing a
poll acquires an optional borrowed pointer once in its logical entry, after
the stack guard. The full `qore_rt_check_cancel_v2` helper remains available
at every existing poll site.
`qore_rt_get_loop_runtime_config` returns null when thread data is unavailable,
without accessing possibly destroyed TLS RuntimeConfig state. A null pointer
selects the ordinary TLS poll; it is not a cached proof that cancellation is
impossible. Non-null views must not outlive the same thread's invocation.

Both polling paths share the same implementation. Deferral precedes thread
cancellation; pending thread requests retain their Program-scope check and
priority over Program interruption. The global pending-interrupt load remains
seq_cst, and actual pending requests still resolve the governing context.
Exception construction, manager lookup and the optional TLS fallback are
outlined so their register saves do not burden the common no-pending poll.
No request state, manager identity or deferral state is cached, and polling
frequency is unchanged for ordinary and fused loops. The compiler comparison
switch is `QORE_DISABLE_LLVM_CANCEL_RUNTIME_CONFIG`. The new helper symbols are
additive: old artifacts work on the updated runtime; new artifacts require it.

The compiler now calls `qore_rt_get_cancel_poll_state_v1` once in that entry
to borrow the deferral-depth, thread-request and global pending-count addresses.
Its versioned descriptor contains three target-sized pointers, not copied
values. On matching native primitive layouts, each checkpoint checks deferral
first, then loads the thread byte and Program count using seq_cst atomic
operations. A pending value calls the full helper: a set flag alone cannot
prove that cancellation is in scope. A null descriptor also calls that helper.
The final exception-sink check remains on every merged path.

The runtime exposes addresses only for the documented i32/i8/i64 sizes,
alignment and lock-free atomic representation; other targets return all-null
fields. Borrowed thread-slot and deferral addresses are valid only during the
same thread's current native invocation, never across teardown or TID reuse.
No Program or manager pointer is cached. Language loops still poll on every
iteration; fused internal loops retain their existing 100-element interval.
`QORE_DISABLE_LLVM_INLINE_CANCEL_POLL` retains v2 helper calls as a compiler
control. This additive ABI does not change RuntimeConfig layout or old helpers.

The native frame helper performs the existing Program-call-context,
closure-environment, local/closure-frame-boundary, diagnostic-frame, and
location/parse-option save/restore through that explicit state. It retains
the runtime parse-option override and the normal stack-reader publication/pop
handshake. The helper is the last member of the diagnostic location; all
reader-visible fields, including the caller location, are initialized before
publication and remain alive until readers finish. Stack protection and
cancellation points are unchanged. The callee still uses its own AOT context.

Prepared calls to the re-emitted callee v3 use a deferred variant of this
helper. Their complete scalar-body proof permits only native arithmetic/select
operations and the full stack-guard edge: no TLS context access, allocation or
generic reentry. Successful calls therefore leave Program-call, closure,
statement/location, parse-option and saved-SP fields untouched. The independent
diagnostic frame is still published on every call, with the existing reader
handshake and caller location. Integer/bool-result entries also defer empty
local/closure markers: these stacks are thread-private, and the proven body
cannot inspect them or reenter Qore. Floating-result entries retain eager
markers after measured regressions from deferring them. A compile-time cleanup
policy avoids a runtime marker-policy branch in successful floating calls.
Both deferred markers are installed before overflow exception work; failed
closure-marker allocation rolls back the local marker, and closure frame counts
are committed only after successful insertion. A three-state enum records eager
markers, deferred markers, or fully materialized context. Successful deferred
calls only restore the previous helper and unpublish their diagnostic frame;
materialized calls restore context and pop both markers in the original order.
Neither the exported native helper/cache ABI nor serialized metadata changes.

This pure-leaf path keeps the prepared helper's entry stack check and the
callee-v3 live stack check (including its full v2 fallback). The intermediate
diagnostic-frame constructor does not repeat the check: it publishes the
deferred context before entering the guarded callee, so an overflow still
materializes the target context before exception construction. Sink checks,
eager legacy-frame guards and every cancellation point remain. This contract
does not permit unguarded or reentrant bodies to use the deferred helper.

A same-thread invocation-local pointer identifies the deferred helper. Before
constructing a stack-overflow exception (including the IA-64 register-stack
case), the full guard materializes the original context exactly once. It reads
the target's current parse options and runtime override at that point; neither
is cached across invocations. Destruction restores materialized state in the
original order and restores the previous deferred pointer. Unmaterialized calls
have no context writes to undo. The descriptor remains pinned and immutable
while published. Older callee entries and helper v2 use eager context setup;
disabling callee v3 also provides an eager control. No native helper ABI or
prepared-descriptor layout changes.

The native frame helper and enclosing diagnostic-frame constructors/destructor
carry inlining hints so the C++ runtime compiler can allocate their save/restore
state together with the typed-call helper. The compile-time eager/deferred
selector lambda carries the same hint: an outlined selector otherwise restores
a wrapper boundary even when its selected helper is inline. State and lifetime
semantics are unchanged. The publishing helper's hints are
on its implementation only: default single-compilation-unit builds can inline
it, while split builds retain ordinary out-of-line symbols. The small positive
target-cache lookup stays inline; reference decoding, ownership replacement
and metadata extraction are outlined as the miss path. Inlining itself removes
none of the guards, frame markers, policy restoration or reader handshakes;
pure-leaf deferral is the separate policy described above. This is a runtime
inlining boundary change; inlining itself does not alter the callee
ABI or introduce a new compact-frame representation.

Prepared v3 calls now use a smaller diagnostic frame that borrows the prepared
descriptor for its name, statement block and Program instead of copying those
three pointers. The caller location remains frame-local. The exact scalar leaf
cannot reenter generic execution or replace the descriptor while it is
published; the existing pop handshake waits for readers before any subsequent
replacement or invocation cleanup. The state-saving helper remains the last
member; its deferred policy is described above. V2 keeps its original frame;
the compact representation alone changes no cache layout, helper ABI, guard,
marker or restoration behavior. A dedicated concurrent
reader fixture checks cache hits, alternating targets and repeated invocation
cleanup under native and generic execution.

The admitted scalar leaf cannot instantiate TLS locals or reenter generic
execution. Where markers are installed (eager calls or overflow materialization),
its local-variable marker uses a specialized push/pop pair: it retains the
ordinary marker and frame count for inspection, but does not
append a variable-cleanup record to the local marker vector. Both local and
closure markers are popped directly as empty frames. The native closure-marker
push writes the same null/zero/non-owner entry inline when the current block
has space. Only full-block growth/reuse calls an outlined helper, which uses
the original insertion path. Entry contents are unchanged; the closure frame
count is committed only after insertion succeeds. Diagnostic publication order
is unchanged.
Debug assertions verify
the top marker and unassigned local slot. Ordinary calls still use the general
cleanup path, including cleanup of residual variables. This relies on the
existing whole-body scalar-expression admission proof; widening native-entry
eligibility must revisit this contract. The diagnostic frame is unpublished and
stack readers finish before either marker is popped, as before.

Context and PC-map readiness use their existing acquire-published state before
calling the slow materialization helpers. Materialization remains lazy: an
unused reference cannot start failing earlier just because its target was
prepared. Initial materialization retains the normal Program-call context.

This is an incremental explicit-context boundary, not a completed replacement
of TLS throughout the runtime. Shared thread state is still published for
compatibility and inspection; v1, generic callbacks, and other native helpers
retain their current conventions. Callback results outside the exact native
assignment proof still merge boxed with generic fallback, and adapter
eligibility remains non-throwing/context-independent.
The next stages are cheaper observable native frames, extending unboxed
continuations to other proven consumers, and guarded native method dispatch. Those require
separate semantic tests and retained-call measurements before expanding scope.

`IRFunctionRefCalls` additionally tests repeated local/foreign-Program calls
inside one invocation, captured state after native calls, both helper versions,
and concurrent stack walks over shared callbacks.
It also mixes native leaves with recursive ordinary frames, captured locals,
owned objects and exception unwinding across multiple TLS stack blocks.

## Tiered Execution

Tiered mode starts functions on AST and promotes by call count:

- AST -> IR after `--jit-ir-threshold` calls.
- IR -> JIT after `--jit-jit-threshold` calls.

Native compilation always runs asynchronously on a background thread; promotion
is non-blocking, so a function keeps executing through the IR interpreter until
its compiled form is ready. Program teardown drains only that Program's own
pending background compiles (matched via `QoreIRFunction::pgm`; a compile with no
owning program is treated as matching any teardown), so destroying one Program
never serializes behind unrelated Programs' compilation.

Process shutdown has two ordered JIT phases. It first permanently stops and drains
the background compiler, making its raw references to Program-owned IR safe before
Program teardown begins. Published native code remains mapped while user-module
delete callbacks, registered shutdown handlers, atexit callbacks, and static
destructors can still execute Qore functions. Normal `qore_cleanup()` releases the
LLVM execution engine only after all module Programs have been destroyed. A direct
`exit()` stops compilation but leaves the immortal engine for the operating system to
reclaim, because callback registration order cannot prove that no later process
callback will invoke a published entry point. This boundary prevents both compiler
work from outliving its source IR and cached native function pointers from outliving
their executable mapping, without adding checks to the runtime dispatch path.

Threads that run LLVM are started through `q_start_llvm_compile_thread()` with an
explicit 8MB stack (`QORE_LLVM_COMPILE_STACK_SIZE`), never with `std::thread`, which
cannot request a stack size and takes the platform default: `RLIMIT_STACK` (8MB) on
glibc but a fixed 128KB on musl.  LLVM's analyses recurse over the IR without a depth
bound -- ScalarEvolution recurses once per chained affine add-recurrence -- so a
default-sized musl thread overflows its stack inside LLVM, with no qore frame beneath
the LLVM frames.  Recursion depth follows the number of chained induction variables
rather than function size, so the native-compilation budget is no substitute.  This
applies to the JIT background compile worker and to the AOT codegen thread pools alike.

Failed lowering or compilation disables promotion for that variant rather than
retrying on every call. Debug attach can force dispatch back to AST so debugger
semantics remain predictable; JIT-compiled native code also emits
statement-boundary debug events (gated on `PO_ALLOW_DEBUGGER` and dormant until a
debugger attaches) so an already-running native frame stays debuggable.

### Publication of Shared Tiering Caches

Promotion writes into state that every thread executing the function shares, while
other threads are already running it. Two caches carry that traffic, and both follow
the same contract: one writer publishes with a release store, and no reader touches
the payload without first taking the matching acquire edge. The caches are strictly
optimizations, so a thread that loses a race takes the generic path for that call
rather than blocking.

`UserVariantBase::cached_ir` holds the variant's lowered IR body. It is published
exactly once, by whichever writer wins a CAS from `nullptr`
(`UserVariantBase::setCachedIR()` and `UserVariantBase::attemptIRLowering()` are both
writers, and the `ir_lower_once` flag orders lowerings against each other but gives
no edge to `setCachedIR()`, which publishes without holding it). The winner's body is
never replaced: a published generation is kept alive by `jit_owned_ir` because JIT
code embeds raw pointers into its instructions, so overwriting the pointer would
strand readers on a body nothing owns. A losing writer deletes its own body, which is
safe precisely because that body was never published. Readers go through
`getCachedIR()`, whose acquire load is what orders the pointee's initialization; a
reader that dereferences the result repeatedly hoists it into a local, because the
value cannot change once published.

The interpreter's per-instruction inline call caches (`inline_ir_state` and the
`cached_callee_ir` / `cached_uvb` / `cached_return_type` / `cached_object_class` /
`cached_class_ctx` / `cached_method` payload on the direct-call instructions) are
governed by `QoreIRInlineStateClaim` in `include/qore/intern/QoreIR.h`. A thread may
write the payload only while holding the claim it takes by winning a CAS from
`QORE_IR_INLINE_UNCHECKED` to `QORE_IR_INLINE_CLAIMED`, and may read it only after an
acquire load of the state returns a value greater than zero. A claim holder writes the
payload only immediately before publishing its verdict, so a holder that bails out
early leaves the cache untouched and returns the state to `QORE_IR_INLINE_UNCHECKED`
for a later attempt; the release is itself a CAS from `QORE_IR_INLINE_CLAIMED`, so it
cannot undo a verdict that was published. Because nothing may read the payload at
`QORE_IR_INLINE_UNCHECKED`, IR lowering does not pre-seed it -- the interpreter
resolves the same values from the variant on first execution instead. The claim is
never held across execution of the callee, and a failed claim never blocks, so a call
site that re-enters itself recursively during resolution simply takes the generic
path.

## AOT Compiler

`qcc` is the dedicated AOT compiler. It supports:

- Script executables.
- User modules as `.qmod`.
- Per-file `.qo` object files.
- `.qoa` archives for C/C++ hosts.
- Link mode: `qcc -o binary *.qo`.
- Metadata inspection: `qcc --dump-info`, `--dump-symbols`,
  `--dump-sections`.

AOT metadata uses the QORD binary format. Feature flags protect runtime
compatibility: readers reject artifacts requiring unsupported IR/AOT features.

For the object-file and module artifact contract, see
[`aot-object-files-and-module-artifacts.md`](aot-object-files-and-module-artifacts.md).

For multi-file script/application AOT, see
[`aot-script-context.md`](aot-script-context.md).

### Parallel Backend Codegen (split-after-opt)

`qcc` can parallelize LLVM backend codegen for a single compilation. It optimizes
the whole module once (so cross-function optimization is unaffected), then
`llvm::SplitModule(PreserveLocals=false)` partitions the optimized module into N
balanced pieces along whole-function boundaries, codegens each piece concurrently on
a worker thread (each with its own `LLVMContext` via a bitcode round-trip, since a
`Value*` may not cross contexts, plus its own `TargetMachine`), and `ld -r`
partial-links the per-partition objects back into one relocatable object.
`SplitModule` externalizes internal symbols, so after the merge qcc re-internalizes
the extras (ELF: `objcopy --localize-symbols`) to keep the split object
symbol-equivalent to the single-object build. Output is byte-deterministic wherever
the single-object build is.

- `--jobs=N` selects the number of codegen threads (default: CPU count, capped at
  32; `1` = single-object, the reference path). `qcc` propagates it to the backend
  via the `QCC_JOBS` environment variable.
- Splitting is gated by a size threshold (`QCC_SPLIT_THRESHOLD`, default `50000`
  optimized-IR instructions — a CPU-speed-invariant metric) so trivially small
  modules are never split.
- Concurrency is bounded by a **GNU make jobserver client**: qcc parses `MAKEFLAGS`
  and acquires tokens *before* splitting; if no spare tokens are available (a
  saturated `make -jN`), it falls back to single codegen so a parallel build is
  never oversubscribed. The client supports both the make 4.4+ named-pipe form
  (`--jobserver-auth=fifo:PATH`) and the older inherited-fd form
  (`--jobserver-auth=R,W` / `--jobserver-fds=R,W`); each qcc process owns one
  implicit job slot and uses extra codegen threads only for tokens it can acquire
  non-blockingly. This makes parallel codegen safe to leave default-on under
  `make -j`.
- `QORE_AOT_SIZE_DEBUG` traces the module instruction count and the split decision.

## User Module Build Behavior

`QORE_BUILD_AOT_MODULES` defaults to `ON`. The CMake build compiles user
modules to `.qmod` artifacts with `qcc` and installs those artifacts alongside
the source modules. Module lookup prefers the compiled `.qmod` over the `.qm`
when both are present in the same module location.

This makes the installed default fast while preserving source modules for
documentation, inspection, and development.

## Lowering And Fallback Policy

IR/JIT/AOT lowering must be fail-closed. Unsupported expressions,
instructions, or metadata combinations must produce diagnostics with enough
opcode, node, type, and source-location context to implement native lowering;
they must not silently emit `EXPR_TREE`, `GENERIC_EVAL`, or source fallback.

AOT artifacts require complete serialized metadata. Source text can be embedded
for diagnostics or inspection, but it is not a runtime fallback for missing AOT
metadata.

## Variables Other Threads Can Change

A global variable, and a local variable that is passed by reference or captured by a
closure (see `closure-bound-locals.md`), can be changed by other threads at any time.
The AST operators hold the variable's lock for the whole of an operation that reads and
writes it (`++`, `+=`, `-=`, ...), so no change made by another thread is lost, and they
read the variable again for every access. Every engine must give the same guarantees:

- IR lowering never lowers such an operation on such a variable as a load followed by a
  store (`isSharedVar()` in `lib/QoreIRLowering.cpp`). The compound assignment operators
  take the lvalue path (`LValuePathCompound`, or the AST-evaluated lvalue operation),
  typed integer pre- and post-increments and decrements take `IncrementLocalInt` (for a
  captured local, which every engine applies under the variable's lock) or
  `LValuePathUnary`, and the subscript fast paths (`isConstKeyHashSubscript()`,
  `isDynamicKeyHashSubscript()`, `isConstIndexListSubscript()`), which load a container,
  change it and store it back, only apply to plain locals.
- The IR interpreter caches no value of such a variable: `LoadGlobal` and `LoadClosure`
  read the variable every time, and `StoreGlobal` and `StoreClosure` only write it. Only
  thread-local variables are cached (other threads cannot change them; calls invalidate
  the cache). Before this, a thread's loop incrementing a global worked on its own cached
  copy and overwrote the other threads' increments, and a loop polling a captured variable
  never saw another thread's write.

`examples/test/qore/threads/shared-var-updates` checks each of these operations from
several threads at once, and a loop seeing another thread's write, in every execution
mode and in a program compiled with `qcc`.

## IR Analysis And Optimization

Lowered functions carry transient facts for each SSA value: known type,
assigned state, NOTHING exclusion, and runtime representation. Assignment is a
separate fact from the declared type. In particular, a typed lvalue is not
assigned before its first write and becomes NOTHING again after remove/delete;
reference parameters retain reference assignment semantics while value reads
use the declared target type.

The LLVM `ListSize` lowering hoists typed-list data pointers only on the
nonempty path. A declared list may still be `NOTHING`, including a local that
is conditionally assigned inside an outer loop. The zero-size path carries a
null data pointer, which the loop's bounds check prevents from being read.
This preserves zero-iteration behavior without adding a check to each element
read. `AOTTypedForeachNothing.qtest` covers these locals, removed/deleted lists,
empty and populated lists, and sparse-list type errors across execution tiers
and source-stripped AOT artifacts.

`QoreIRAnalysis` provides normalized SSA-operand and normal-successor visitors,
plus reusable reachability, predecessor/successor, dominator, and natural-loop
analysis. Generic operands carry phi inputs and dynamic lvalue-path and slice
values, while the visitor also covers operands stored only in dedicated
instruction fields. The IR interpreter uses it for ownership use counts,
preventing dedicated instruction fields from diverging from generic operand
handling.

LLVM call-argument cleanup must preserve a temporary while its SSA value has
remaining uses. Virtual implicit arguments can share one mapped value across
several calls, as in `map (f($1), g($1)), (map make($1), xs)`. Only the final
use may pass the temporary's cleanup slot to a consuming call helper; earlier
calls borrow it. `AOTSharedMapArgs.qtest` covers this lifetime through calls,
branches, duplicate arguments, and exception unwinding.

The first generic optimization built on this layer is conservative scalar
loop-invariant code motion. It moves only native scalar constants, proven
assigned and non-NOTHING non-reference IR-only local loads, and non-throwing
native scalar operations. A local written in the loop is never hoisted. Loops
must have a unique unconditional preheader, and the preheader must precede the
loop in IR block-list order while the LLVM emitter still resolves ordinary SSA
operands in that order. Every hoisted result must have only repeated,
non-consuming scalar users inside the loop. Loads are not hoisted across calls
or other operations that can invalidate interpreter local slots. These rules
preserve the runtime ownership and assigned-state lifetime represented by each
SSA slot. The post-optimization verifier is mandatory for both runtime and AOT
source lowering. `QORE_DISABLE_IR_LICM=1` disables only LICM, while
`QORE_DISABLE_IR_OPT=1` disables all IR passes; `QORE_IR_OPT_STATS=1` reports
analyzed loops and hoisted instructions.

After LICM, a same-basic-block scalar pass forwards repeated loads of IR-only
locals and eliminates repeated native scalar constants and arithmetic or
comparison expressions. Load forwarding requires the value facts to prove an
assigned, non-NOTHING native `int`, `float`, or `bool`; declared type alone is
not sufficient. Explicit local writes invalidate that local, while references,
calls, lvalue operations, scope exits, and other operations that can mutate an
unknown local invalidate all available loads. Discovery is cancellation-safe
and no definition is erased until all replacements have been selected. A value
is merged only when every consumer is a non-consuming scalar operation or an
IR-only local store; call arguments, runtime-visible stores, phi inputs, and
lvalue paths retain distinct SSA ownership. Set `QORE_DISABLE_IR_CSE=1` to
retain the unoptimized same-binary path; the `QORE_IR_OPT_STATS=1` output
includes forwarded-load and eliminated-expression counts.

The optimizer also replaces a conditional branch whose SSA condition is a
literal boolean with an unconditional branch. It deliberately leaves the
unreachable block and phi structure intact; native backends remove that code,
while the IR interpreter avoids repeated condition dispatch in constant-path
loops. `QORE_DISABLE_IR_CONST_FOLD=1` retains the conditional form for
same-binary comparisons, and optimization statistics report folded branches.

Recognized builtin and pseudo-method operations also carry a stable
`QoreIRIntrinsic` identity. The builder assigns the identity when it creates a
resolved pseudo call, and artifact readers reconstruct it once from serialized
method metadata. Interpreter and LLVM fast-path selection dispatch on this
identity instead of repeatedly comparing class and method names. Runtime name
dispatch remains the fallback for unresolved or unsupported calls and retains
precedence for objects and hash-like values whose members can override pseudo
methods.

The IR interpreter also computes a transitive external-cache effect summary for
resolved direct user-function calls. Local writes to globals, thread-locals,
closures, references, and unknown call forms remain immediately effectful.
Known direct-call edges are solved to a fixed point, so pure recursive call
components are recognized without recursive-analysis guesses. Missing callee
IR, reference arguments, direct methods, and closures remain conservative. The
summary is published atomically after the complete reachable call graph is
known; until then callers invalidate caches as before.
`QORE_DISABLE_IR_EFFECT_SUMMARY=1` keeps the conservative direct-call policy for
same-binary comparisons, and `QORE_IR_EFFECT_SUMMARY_STATS=1` reports each
resolved graph's size and result.

## Validated Registries

The current implementation relies on explicit registries for extension safety:

- Opcode metadata is validated before IR verification.
- IR expression lowering uses explicit claim predicates; a handler that claims
  a node must either return IR or report why native lowering is impossible.
- AOT expression-slot, expression-node, and instruction-group registries are
  validated before serialization; unknown instruction subclasses fail
  serialization with the dynamic C++ type name.
- JIT runtime helpers are listed in one registry in `JITRuntime.cpp`; the JIT
  validates names, addresses, and duplicates before registering ORC symbols.
- Type-spec match dispatch validates complete, duplicate-free coverage of the
  closed built-in `q_typespec_t` set before dispatch.

## Current Hardening Rules

- Runtime-only changes should not force all `.qmod` files to rebuild. The
  `qcc-format` stamp tracks sources that affect emitted qmod format or compile
  semantics.
- Split-directory module qmods must live beside their resource files so
  `get_script_dir()` remains valid during AOT initialization.
- AOT local-slot tables preserve IR local slot IDs for deferred handler
  execution.
- Expression-tree member defaults are materialized after constants are
  registered so class and namespace constants can be referenced safely.
- `.qo` metadata preload creates declaration shells only; it must not execute
  user code during compile-time name/type resolution.
- Batch AOT registration must keep cross-session barriers: shells, type/base
  resolution, constants, members, functions, then final class fixups.
- A constant whose `__const_init` did not populate it at load must be
  recoverable from its first read, and a read that cannot recover must report
  why the initializer failed rather than only that the value is missing. See
  [`aot-constant-init-recovery.md`](aot-constant-init-recovery.md).

## Exception Source Locations

AOT-compiled code reports real per-frame source file and line numbers in
exceptions, matching interpreted execution. The throwing location and full
backtrace are reconstructed lazily at throw from line-number tables embedded in
the artifact (emitted by default with debug info), so non-throwing code carries
no per-statement location-updater cost. `--strip-debug-info` emits no line
tables and retains the per-statement updater instead. Source text is not
required: stripped-source artifacts still report file and line from the line
tables.

For the innermost-frame detection mechanism that decides, at throw, whether the
innermost user frame is AOT (and therefore whether to resolve lazily), see
[`aot-lazy-loc-innermost-frame.md`](aot-lazy-loc-innermost-frame.md).

## Exception Cleanup Dominance

LLVM `invoke` results that own Qore references may be cleaned up directly from
SSA only when the defining block dominates every cleanup use. The general path
therefore keeps the safer cleanup-alloca representation unless dominance is
proven.

Do not relax this rule without a real dominator-tree or equivalent
scope/finalization design. See
[`aot-eh-cleanup-dominance.md`](aot-eh-cleanup-dominance.md).

## Block-Scoped IR-Only Local Lifetime

An IR-only body local never has a runtime local-stack entry: the call wrapper
skips it and `StoreLocal` neither instantiates nor publishes it. In LLVM code a
boxed IR-only local that is neither a fast-entry parameter nor backed by a
pre-instantiated cleanup slot owns its current value in its local alloca; every
write that replaces that value stores an owned reference.

Two rules keep its lifetime lexical, matching the AST interpreter:

- `UninstantiateLocal` releases the local at every scope exit (normal end,
  `break`, `continue`, `return`, exception cleanup): it drops the owned alloca
  value and the local's cleanup slots and resets the alloca, but touches no
  runtime stack slot. The lazy-instantiation path, which pops a runtime stack
  entry, must never handle these locals: it returns early for a local that
  `StoreLocal` did not instantiate, which delayed destructors to function exit.
- Ownership is registered when `preCreateLocalAllocas()` creates the alloca,
  before any block is lowered, not when the first owning store is lowered.
  Blocks are lowered in layout order, so an exit can be lowered before a store
  that executes ahead of it (a `return` or `throw` after a `switch` whose case
  assigns the local); its inline cleanup would otherwise omit the local and leak
  the value.

Object-typed locals whose type names a unique class stay AST-visible, but
`*Class`, `auto`, and container-typed locals holding objects are IR-only, so
destructor timing depends on these rules. `IRBlockScopedObjectLifetime.qtest`
checks exact constructor/destructor order for these shapes in every tier and in
AOT.

## Large Int Ownership in Fused Local Updates

With NaN boxing an int outside the 48-bit immediate range is a heap node, so any
code that produces a boxed int must give that reference an owner. Fused local
int updates (`AddAssignLocalInt`, `IncrementLocalInt`) follow `StoreLocal`:

- LLVM code hands a boxed result to whatever owns the local's value: the alloca
  of an IR-only local or fast-entry parameter (releasing the value it replaces),
  or the pre-instantiated cleanup slot the alloca borrows from. For a
  runtime-stack local the alloca only caches: an immediate is stored as is, a
  heap node is retained by the local's reload tracker with the usual deferred
  release, and the runtime assignment takes its own reference.
- The IR interpreter releases the slot-cache value it overwrites, uses the
  transferring assignment helpers so the variable consumes the new value, and
  registers the result slot for cleanup.

Code that only inspects an lvalue must not read it through `getValue()`, which
creates a new heap node for a large int stored natively;
`LValueHelper::getInternalNode()` returns the stored node without creating one.
`IRFusedIntLocalOwnership.qtest` checks values across the immediate boundary and
bounds resident set growth over repeated updates in every tier and in AOT.
Both this test and `NanBoxDoubleTagCollision.qtest` read Linux's
`/proc/self/smaps_rollup` for these measurements. `VmRSS` in `/proc/self/status`
uses asynchronous accounting, so delayed updates can look like allocation growth.
Platforms without readable `smaps_rollup` skip memory measurements while retaining
value checks. The 2 MiB leak threshold is unchanged; a negative control retains
200,000 boxed doubles and verifies that the measurement exceeds that threshold.

## Large Function Bodies and Cleanup Scaling

Every per-statement exception and thread-exit check in a function branches to
one shared `error_return` block, and that block releases each IR-only local that
owns its value in its own alloca. Emitting one `load` / `store NOTHING` /
`qore_rt_decref` triple per local there is correct but does not scale: SROA
promotes those slots to SSA, so each local then needs a PHI in `error_return`
for *every* edge reaching it. The PHI count is therefore
`IR-only locals x exit edges`, PHI elimination expands each PHI operand into a
machine `COPY`, and LLVM's register coalescer and allocator go superlinear on
the result. A 150-statement body produced 151 PHIs across 751 predecessors —
113k PHI operands and 235k `COPY`s — and took over ten seconds to code-generate;
at 400 statements it did not finish in two minutes. The same body lowered
without the LLVM optimizer, where the slots stay in memory, code-generated in
0.7 seconds, which is what identifies the cleanup shape rather than the function
size as the cause.

This is the same shape clang avoids in its own EH cleanups: clang keeps cleanup
state addressed through memory (`cleanup.dest.slot`, destructor objects
referenced by their alloca address) and emits each cleanup once, so no
cleanup-referenced value ever becomes a promoted SSA value needing a PHI per
exit. Qore now does the same thing where it matters.

`QoreIRToLLVM::prepareBoxedExitCleanupArray()` measures both factors at function
finalization — the number of boxed exit-cleanup slots and the predecessor count
of `error_return_block` — and, only when their product exceeds
`getCleanupPhiBudget()` (`QORE_JIT_CLEANUP_PHI_BUDGET`, default 4096), records
the slot addresses in an entry-block array and replaces the whole triple
sequence with a single `qore_rt_cleanup_run_allocas()` call. Taking the slot
addresses keeps them in memory, so no PHI is needed and code generation stays
linear. The slot set is everything `emitPreinstantiatedCleanup()` releases, in
emission order: pre-instantiated entry cleanup slots, fast-entry parameter
slots, then the IR-only locals that own their value directly — covering only one
of the three leaves the other two to rebuild the same web. Every function below
the budget — which is every ordinary function — keeps the inline triples and
full SSA promotion of its slots, so the trade-off is confined to bodies where
promotion could not pay off anyway.

`prepareIteratorCleanupArray()` applies the identical treatment to
`iterator_cleanup_allocas` and `qore_rt_iterator_cleanup_allocas()`. Iterator
cleanup is written both in the shared error-return block and, by
`emitLateExitCleanup()`, at every `ret`/`resume`, so its multiplier is the same
edge count.

A third instance of the pattern lives on the temp-boundary path.
`flushLocalReloadStateAtTempBoundary()` emitted a two-block NOTHING test per
tracked local at *every* temp boundary, so a function paid
`tracked locals x temp boundaries` basic blocks: the reflection test's
1621-line `namedCallReflectionTests()` reached 88062 rotations — 95% of its
184933 basic blocks and 74% of its instructions, before the optimizer ran at
all — and could not be code-generated in half an hour. Above
`getReloadChainCallThreshold()` (`QORE_JIT_RELOAD_CHAIN_CALL_THRESHOLD`, default
32 tracked locals) the same test and rotation run inside
`qore_rt_reload_chain_rotate()` / `qore_rt_reload_chain_clear()`, which emits one
call and no extra block. Smaller functions keep the inline form so LLVM can fold
it away.

Together these took that method from an unbounded hang to 29s.

Two details matter for correctness. The runtime helper releases slots in LIFO
order, so the addresses are stored in reverse and the values are released in
exactly the order the inline sequence used; destruction order of locals is
observable through destructors. And the choice is made before any cleanup is
written, so the shared error-return block and the shared EH common-cleanup block
agree on which form they emit.

The IR-side dataflow analyses that feed this path (`qore_ir_mark_local_list_pushes()`
and `qore_ir_mark_in_place_string_appends()`) use `QoreIRLocalBitSet`, a dense
bit vector, as their lattice element. A hash set copies one node per element on
every block merge and transfer, which costs `O(blocks x candidate locals)`
allocations and was the largest remaining cost of lowering a very large body.

### Scalar method register-result experiment (2026-10-14)

The AOT typed method helpers retain their output-pointer convention. Two
uncommitted register-result prototypes were tested against the post-merge
`6b4590ba7` baseline and rejected after 792 measured windows. Splitting native-only
from matched-boxed dispatch restored the native helper's 320-byte frame and kept
the result in registers through cleanup, but native integer workloads still took
9-12% longer; boxed workloads showed no useful gain. This was an AOT experiment,
not an IR/JIT optimization or a fresh Java comparison.

The removed implementation and raw measurements are preserved in
`bench/runtime-comparison/results-aot-register-result-rejected-macos-arm64.json`;
see the corresponding `RESULTS.md` section for controls and limits. Prioritize
cheaper proven dispatch/admission and member access over another result-layout
variant without a new mechanism or profile.

### Receiver-type admission proof experiments (2026-10-14)

Two AOT receiver-type proofs were rejected after 792 measured windows against
the restored `fd6aabe6c` baseline. A boolean proof in PIC padding slowed native
integer calls by 2-4%. Folding the proof into the existing scalar comparison key
left native integer calls effectively flat and improved native float by 1-2%,
but regressed boxed float by about 3% and effectful member calls by about 2%.
No runtime/test change is retained; these are not new Java or IR/JIT results.

Both versions preserved complete checks for non-null instantiated receiver
types and all live dispatch/observability guards. Entry layout and native-int
stack-frame size stayed unchanged, but generated helper bodies grew. Moving a
fact to admission is therefore not by itself evidence of a faster call path.

Patches, all raw windows, selected release validation and restoration evidence
are in `bench/runtime-comparison/results-aot-receiver-proof-rejected-macos-arm64.json`.
The original runtime and matching AOT modules are restored and validated. Next
investigate per-call configuration/static-initialization guards and more granular
dispatch costs, preserving cold-call and runtime-control behavior; do not repeat
receiver-proof/layout variants without new evidence.

### Single-load typed-method policy experiment (2026-10-14)

A cached boolean combined with its initialized state in one atomic byte was
rejected after 1056 AOT measurement windows. It preserved first-use environment
semantics through per-object cold `std::call_once` initialization and kept all
live guards. Actual arm64 helpers used fewer policy loads without larger stack
frames, but native integer calls had no repeatable gain. Confirmation improved
native floating calls about 1-3% while slowing effectful calls about 1-3%.
No runtime optimization is retained, and these are not new Java/IR/JIT results.

The original runtime and matching AOT modules were restored and validated.
`bench/runtime-comparison/results-aot-native-policy-rejected-macos-arm64.json`
contains both complete matrices, source/assembly, release and standalone
sanitizer evidence, and the audit. The initial timing parent's terminal status
was unavailable; all child outputs were independently verified, and the larger
confirmation completed with observed successful timing/verifier exits.

The next distinct target is effectful member declaration/context lookup. The
prehashed update helper still performs this lookup before its locked storage
lookup. Existing exact-class object-member descriptors are a useful precedent,
but a polymorphic descriptor must prove the live receiver/context and metadata
lifetime. Mutable member values and raw storage pointers must not be cached;
retain the lock, deletion check, scalar/fallback checks and exact-once resume
semantics. The follow-up experiment below measures that proposal; no version of
it is retained.

### Effectful-member metadata cache experiment (2026-10-14)

A bounded positive metadata cache was tested against merged `b2c7f9143b` and
rejected after 1056 AOT measurement windows. It replaced repeated declaration
lookup with a four-entry context cache keyed by operation slot, live receiver,
defining context and a new member generation. Native member additions and class
destruction required invalidation beyond the method-dispatch epoch. Storage
remained live and locked; no receiver, mutable value or raw member pointer was
retained, and overflow/fallback/exact-once behavior remained unchanged.

The longer confirmation improved effectful integer calls from 90.129-90.374 to
86.843-86.886 ns/element (3.60-3.91%), but integer callbacks regressed from
6.948-6.997 to 7.217-7.233 (3.14-4.10%). List construction also regressed
1.45-1.74%; both regressions appeared in the initial matrix. Boxed floating calls
improved, but do not use the new helper, so that movement cannot be attributed
to member metadata caching. These are whole-workload measurements with repeated
baseline/candidate A/A controls, not isolated call latency or a new Java ratio.

The candidate passed 13 release files, 25 cases and 12126 assertions plus both
reference grids. Its code and tests were removed; the original runtime and 62
matching AOT modules were rebuilt and validated with five original files,
14 cases, 6009 assertions and both reference grids. No new debug/sanitizer/Java
qualification was pursued. The implementation, all raw windows, source and
artifact identities, validation and audit are preserved in
`bench/runtime-comparison/results-aot-member-metadata-rejected-macos-arm64.json`.

Next investigate an immutable operation descriptor attached to the admitted
method-dispatch entry and passed through a versioned effectful companion ABI.
The current scalar-method wrapper already follows full PIC-key and live
body/Program/debugger checks; the effectful binder admits at most 21 updates.
This suggests a way to avoid duplicate per-update cache/slot/context work, but
the metadata-generation and lifetime proof across the non-reentrant prefix
must be established first. Keep live locking, storage lookup and exact-once
continuation. Require same-build controls and short-call/metadata-churn checks
as well as frozen before/after measurements before retaining a replacement.

### Admitted effect-member descriptor experiment (2026-10-14)

The proposed versioned companion was implemented and rejected after 756 AOT
measurement windows. An admitted method PIC entry owned a lazy immutable plan
for at most 21 compiler-described operations. A v2 companion received that plan
explicitly, replacing per-update declaration lookup with one generation check
per invocation. The original v1 companion/continuation remained the fallback.
Plans retained no receiver owner, mutable value or storage address; live locking,
deletion, representation/overflow and exact-once continuation stayed intact.
Native member/context changes and class/AOT-context lifetime invalidated metadata;
64 retained plans bounded allocation. This did not enable racing class mutation.

The eleven-workload screen and four-workload focused confirmation both showed a
small target gain with a larger integer-callback loss. Confirmation improved
effectful calls from 90.815-90.919 to 87.380-87.537 ns/element (3.61-3.89%), but
callbacks regressed from 6.986-7.035 to 7.501-7.511 (6.63-7.52%). Native integer
results and integer member reads also regressed 0.58-1.00% and 1.59-2.70%.
These are whole-workload figures and A/A median ranges, not isolated call latency
or confidence intervals. No fresh Java comparison or IR/JIT speed claim is made.

Same-executable runtime-off returned effectful calls to baseline-like timing but
left callbacks slow. Compile-off recovered most callback loss with the same
runtime. Frozen callback v2/v3 bodies and ordinary/fast callers have identical
normalized instruction sequences at different addresses. This supports a layout
sensitivity hypothesis without establishing the exact microarchitectural cause.
The regression is not discarded; no runtime change from this prototype remains.

The candidate passed 13 release files, 25 cases and 12156 QUnit assertions, with
actual v2-hit/continuation/invalidation/cap probes and both reference grids. After
restoration, the original core and 62 AOT modules were rebuilt; five original
files passed 14 cases/6009 assertions and both reference grids. Broader debug,
sanitizer, platform, short-call/churn timing and Java qualification were not
pursued after rejection. The complete implementation, measurements, controls,
assembly, validation/restoration and audit are in
`bench/runtime-comparison/results-aot-effect-descriptor-rejected-macos-arm64.json`.

Next measure emitted-code layout sensitivity on the restored baseline with a
predeclared set of deterministic layout variants and identical-instruction/output
checks. Same-binary A/A runs do not sample this dimension. Future small gains
need symmetric baseline/candidate ensembles without selecting favorable padding
or addresses. Profile the hot caller/runtime/companion boundary before choosing
the next structural change; retain stack/frame-reader/Program guards, ownership
and cleanup ordering. Do not remove frame pointers or unwind tables: the AOT
compiler documents real external Abseil/V8 unwinder failures without them.


### Layout calibration and generated-entry alignment (2026-10-14)

The planned calibration completed: five deterministic function orders plus a
same-executable A/A configuration, 360 AOT windows on one frozen runtime. All
mapped pre-link sections match; selected callback/caller instructions match
after relocation normalization. Integer callback medians span 6.881-6.974
ns/element, floating callbacks 9.045-9.161, native integer methods 10.469-10.720,
and effectful calls 88.479-89.828. This set does not reproduce or explain the
preceding descriptor prototype's 6-8% callback regression. Metadata/allocation
changes and unsampled placements remain possible contributors, not established
causes.

A subsequent compiler-only 64-byte alignment prototype for generated scalar
method-v3/callback-v3 companions was rejected. The frozen baseline, candidate
enabled and same-runtime compile-off control each used the same five orders;
540 windows were retained. Median-of-layout-median on/off changes were -0.56%
for integer callbacks, -0.58% for floating callbacks, -0.15% for native integer
methods and +0.94% for effectful calls. Paired layouts moved both ways; effectful
samples were noisy. This is not a validated performance gain. Guards, native
instructions and ABI remained unchanged; the compiler edit was removed.

Separate profiles still locate most fast-call samples in dispatch/frame work,
not tiny callee arithmetic. The next structural investigation is an additive
register-preserving callee convention, starting with generated-assembly evidence
before expanding runtime ABI/qualification. Preserve compatibility, all live
guards, diagnostic frame lifetime, unwind tables and frame pointers. Require
broader before/after controls and a retained AOT gain before fresh matched Java
timing. These experiments make no IR/JIT speedup or Java-parity claim.

Candidate release checks passed six files/13 cases/3364 assertions and both
reference grids. The original core and 62 modules were restored and checked;
full measurements, profiles, setup diagnostics, validation and all audit items
are in `bench/runtime-comparison/results-aot-layout-calibration-macos-arm64.json`.


### Reusable AOT integer callback frames (2026-10-14)

Retained additive runtime-v4 dispatch reuses caller-owned diagnostic state for
loop-local int()/int(int) callback sites. Existing v3 cache/callee ABI remains;
at most four 208-byte, 16-aligned slots per invocation. Construction is deferred
until a positive admitted hit, no heap allocation occurs, every call refreshes
context/location and performs existing reader publication/removal, and every
exit destroys inactive scratch before its borrowed descriptor owner. Live
Program/debugger/body/readiness/signature/pending/stack guards are retained.
Only AOT lowering changes; no IR/JIT performance benefit is claimed.

Final five-layout baseline/on/off matrix interleaved with fresh Java/JMH:
integer callbacks 6.905 -> 6.300 ns/element (-8.76% frozen; -11.66% same-runtime off),
Java 1.958 ns/element (3.22x gap). All five target layouts improve with disjoint
fork ranges. Short controls retain approximately1% aggregate costs; some
member-read control layouts are adverse. Native integer/floating methods remain
3.30x/3.58x Java, boxed methods 6.15x/7.57x, member reads 12.10x/12.48x,
effectful methods 47.75x, list build 26.97x and mutation 57.88x. These are matched
whole-workload ratios, not isolated call overhead or general language claims.

Release and debug each pass 13 files/29 cases/7322 assertions and both reference
grids; release artifacts and 62 qmod aliases restored. The evidence includes
6057 windows, rejected heap-frame variants, short controls, negative
register-preserving ABI probes and fresh diagnostic profiles. No performance
ceiling or parity claim: hot integer dispatch still holds 39.80% of exclusive
main-thread samples, frame removal 8.59%; floating/method dispatch remains large.

Next: prove or reject warm reusable-frame duplicate stack-check elimination,
then separately evaluate method/floating reusable contexts. Preserve physical
guarding before fallible/reentrant work, current limits, debugger transitions,
overflow materialization, reader lifetimes and legacy entries. Larger member
and list gaps require targeted lookup/lvalue/reference/synchronization work.
See the reusable-frame section of `bench/runtime-comparison/RESULTS.md` and
`results-aot-frame-reuse-macos-arm64.json` for controls, caveats and full audit.


## Warm callback guard experiment rejected (2026-10-14)

The additive v5 warm stack-guard trial is removed. Its smaller64-byte dispatch
frame improved the final integer callback workload -2.03% against frozen v4
(-3.09% against same-runtime v4off), but member-read regressions were repeatable.
Identical frozen-v4 executables against both runtime libraries show+1.88%
integer and+1.51% floating member-read costs. This isolates an adverse runtime-
wide effect, not its exact cause. No v5ABI/control or relaxed entry guard remains.

The fresh matched matrix measures retained v4 at5.930ns/element and Java at
1.879ns/element for the integer callback workload (3.16x). The rejected5.810ns
candidate is not production performance. These are whole-workload numbers;
they do not generalize to direct/static calls or establish a performance ceiling.

Candidate release/debug and restored-release qualification, all4785windows,
raw Java/control measurements, probes and the removed patch are recorded in
`bench/runtime-comparison/results-aot-warm-guard-macos-arm64.json`; the narrative
and full13-workload table are in `bench/runtime-comparison/RESULTS.md`.

Next proof/measurement targets are persistent floating callback frames (retain
physical guards, full overflow/context materialization and reader handshake),
then native-method frame reuse and compiler-visible invariant admission. Do not
infer that floating marker deferral alone helps: earlier automatic-frame trials
were negative. Any persistent design must cover floating edge values, generic
fallback, foreign Program/debugger changes, stack readers, cold/short invocations
and same-runtime controls before being retained.
