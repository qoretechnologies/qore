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
when attached. QORD version 22 preserves each closure's entry file/source label,
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
version-22 artifacts. No prepared scalar callback helper ABI changes. Compiler
comparisons must use separate before/after AOT compilation, not a newly compiled
executable against the older runtime. `IRComposedIntLeaf.qtest` covers full-width
compositions, effects and exception fallback, concurrent first calls, Program
churn/crossings, and warmed/cold debugger attachment through source-stripped AOT.

### Develop integration: eager locations and slot allocation

The integration of develop `b7e1367f5f` removes lazy native-PC maps and preserves
the branch's exact normal-body guards with `QoreAOTContext::native_body_fn`.
Receiver forwarding publishes one immutable image-owned v3 descriptor containing
the companion, slot and direct source location. Its release/acquire publication
prevents readers from observing a partially initialized proof. Legacy companions
that require a PC map are not admitted. Receiver and dynamic-closure lowering use
`qore_rt_get_native_entry_slots_v2` to obtain location slots from the explicit
`RuntimeConfig`, without another TLS lookup.

Deferred scalar frames save the caller's statement and location on every entry,
install the callee's image-owned body location, and restore the caller on every
exit, including an unmaterialized successful call and a null parse location.
They retain the stack-reader publication handshake and live admission guards.
Native call-name interning participates in the fork lock protocol.

QORD 22 combines the branch's closure-entry/default-constructor payloads with
develop's v20 background/block-exit helpers and v21 removal of location tables.
Earlier performance-branch v20/v21 artifacts must be rebuilt against the matching
runtime; those experimental payload versions are not aliases for develop's
versions. Historical benchmark results below describe their recorded revisions,
not the merged runtime. A fresh runtime checkpoint remains pending.

AOT local-slot allocation keeps a cached next index and recomputes it once after
bulk IR-slot seeding, preserving sparse identities and replacement of the highest
seed. This removes the per-insertion maximum scan. In an isolated optimized
macOS arm64 benchmark (seven independent forks per size, median), 128/1024/8192
slots take 2.62/25.2/257 microseconds versus 8.23/409/69994 microseconds with the
merged pre-optimization header. Exact assignments and checksums match. These are
slot-table construction costs, not whole-compiler or runtime-call speedups.

### Constructor defaults

QORD version 22 retains the exact parser-selected constructor signature in
`VT_NEW_OBJECT` member, static-member and hashdecl defaults. This includes the
empty signature of a zero-argument constructor. The loader binds an available
constructor before publishing the default, so ordinary evaluation uses the
same prebound call as source execution. A forward reference whose methods are
not registered yet retains an immutable signature and resolves that exact
variant when evaluated; it never substitutes overload selection by value.
Defaults without a parser-selected variant retain runtime dispatch.

This fixes source-stripped modules whose escaped module-init callbacks create
objects with builtin constructor defaults such as `Thread::Mutex mutex()`.
Losing the original binding made AOT repeat domain selection under the
callback's module-header context, rejecting a construction accepted by source
execution. No caller parse options or dynamic-call checks are relaxed.
`AOTObjectDefaultBinding.qtest` compares source modes and source-stripped
`-O0`/`-O3` modules, including inherited, static and hashdecl defaults, forward
constructor declarations and overloaded constructors. Supported older artifacts
remain readable with their previous unbound behavior; recompile them to obtain
the binding. Older runtimes reject version-22 artifacts.

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
The binding checks the normal body's identity against the context's immutable `native_body_fn`.
An append-only context field holds the acquire/release-published entry; repeated
generic calls use a relaxed already-bound check before attempting publication.
No `QoreAOTFunc` table stride or serialized symbol-index format changes, and old
native artifacts have no binding and retain their existing path.

A polymorphic cache hit still validates the resolved override, receiver validity,
dispatch epoch/access key, exact integer argument and actual `int(int)` signature.
The native path additionally requires a published context, unchanged normal
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
The experiment's stress tests exposed an independent diagnostic lifetime bug.
The earlier fix made the native-PC registry own immutable location snapshots.
The develop integration replaces that registry with image-owned constant
locations. The regression still checks location lifetime across Program teardown
and registration on foreign threads.

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
context readiness, current Program and debugger state remain live guards.
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

The scalar-only proof limits the adapter's context and exception requirements.
Bodies that can raise ordinary Qore exceptions retain the normal entry and its
full context setup. With eager image-owned locations, widening eligibility still
requires proving the body's context, ownership and diagnostic contracts; an
explicit context argument alone is not sufficient.

The compiler attaches the adapter to a newly created reference before it can
escape. Attachment and each invocation compare the registered normal AOT
entry's address with the adapter's normal-entry identity. A matching name or
signature is not sufficient. The pointer fields are immutable after reference
publication; the reference's existing Program lifetime rules still apply.
This is additive: no serialized descriptor, public ABI, or existing AOT entry
changes. Previously compiled artifacts use the guarded runtime path without
an adapter. Newly compiled artifacts require the new runtime helper.

Native reference calls retain stack checks, the caller Program context,
diagnostic frames, runtime parse options/location, and the C++
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

Parallel AOT code generation carries image-owned location constants with each
partition. Locations do not depend on DWARF or a separate native PC map, so the
Mach-O partial link and stripped-debug builds retain exact exception locations.
Forced-split, source-stripped regression tests check those lines.

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
readiness remain live checks on every call. No body is
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
caller's debug scope; the existing published frame and image-owned callee body location
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

Context readiness uses its existing acquire-published state before
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
`llvm::SplitModule(PreserveLocals=false)` partitions the optimized module into 4
(`QORE_AOT_SPLIT_PARTS`) balanced pieces along whole-function boundaries, codegens the
pieces concurrently on worker threads (each piece with its own `LLVMContext` via a
bitcode round-trip, since a `Value*` may not cross contexts, plus its own
`TargetMachine`), and `ld -r` partial-links the per-partition objects, in partition
order, back into one relocatable object.
`SplitModule` externalizes internal symbols, so after the merge qcc re-internalizes
the extras (ELF: `objcopy --localize-symbols`) to keep the split object
symbol-equivalent to the single-object build.

- The partition count is fixed, so the object depends only on the module: identical
  source gives a byte-identical qmod whatever the build's load, the machine's CPU count
  or the number of codegen threads. When the partition count followed the threads
  available (the jobserver tokens free at that moment, or the CPU count), 33 of 391 qlib
  qmods differed between two clean builds of the same source. Measured on DataProvider,
  the largest qlib module: 4 parts compile in 27.4 s on free cores against 39.3 s for a
  single object (16 and 32 parts: 25.5 s and 25.8 s), and 4 parts compiled one after
  another on one core take 43.3 s.
- `--jobs=N` selects the maximum number of codegen threads (default: CPU count, capped
  at 32; `1` compiles the parts one after another). `qcc` propagates it to the backend
  via the `QCC_JOBS` environment variable. It never changes the output.
- Splitting is gated by a size threshold (`QCC_SPLIT_THRESHOLD`, default `50000`
  optimized-IR instructions — a CPU-speed-invariant metric) so trivially small
  modules are never split.
- Concurrency is bounded by a **GNU make jobserver client**: qcc parses `MAKEFLAGS`
  and acquires tokens for the codegen threads; if no spare tokens are available (a
  saturated `make -jN`), it compiles the parts one after another on its own slot so
  a parallel build is never oversubscribed. The client supports both the make 4.4+
  named-pipe form
  (`--jobserver-auth=fifo:PATH`) and the older inherited-fd form
  (`--jobserver-auth=R,W` / `--jobserver-fds=R,W`); each qcc process owns one
  implicit job slot and uses extra codegen threads only for tokens it can acquire
  non-blockingly. This makes parallel codegen safe to leave default-on under
  `make -j`, and the tokens never change the output.
- `QORE_AOT_SIZE_DEBUG` traces the module instruction count and the split decision.
- On macOS a module is linked with its final path as its install name and without a
  debug map (`-Xlinker -S`): it is linked to a temporary file whose name has the process
  id in it, and its object files are deleted after the link, so the default install
  name and the debug map (object paths and modification times) made every link
  different. ELF modules have no soname and keep their debug information.

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

### Lvalues

Every lvalue that `check_lvalue()` accepts is lowered to an lvalue path
(`extractLValuePath()` in `lib/QoreIRLowering.cpp`, executed by
`LValueHelper::navigatePath()`): a root (local, closure-bound, global, thread-local
or static variable, or a member of `self`), then navigation steps (a hash key or object
member, a list index) and `Cast` steps, which check the value navigated to so far as the
AST checks `cast<T>(lvalue)` and keep the position. Every operator that modifies an
lvalue, apart from the fast paths for plain locals and their subscripts, emits an
`LValuePath*` instruction; if the path cannot be built, lowering fails. Lowering emits no
AST-evaluated lvalue operation (`StoreLValue`, `AddAssignLValue`, ...): those only
evaluated the lvalue's AST, keys included, at runtime.

A slice (a hash slice, a list slice or a range) is an lvalue only as the whole lvalue of
`remove` and `delete`, which take it as the terminal step of the path; `check_lvalue()`
rejects any other slice lvalue at parse time, and a key or index that is a list only at
runtime raises `ILLEGAL-SLICE` in every engine (`qore_check_lvalue_key_not_slice()`).

An expression statement is lowered as any other expression, also in a try block: the
operators that can raise an exception there are lowered with their own opcodes by
`lowerExprOpOrInvoke()`.

## Variables Other Threads Can Change

A global variable, and a local variable that is passed by reference or captured by a
closure (see `closure-bound-locals.md`), can be changed by other threads at any time.
The AST operators hold the variable's lock for the whole of an operation that reads and
writes it (`++`, `+=`, `-=`, ...), so no change made by another thread is lost, and they
read the variable again for every access. Every engine must give the same guarantees:

- IR lowering never lowers such an operation on such a variable as a load followed by a
  store (`isSharedVar()` in `lib/QoreIRLowering.cpp`). The compound assignment operators
  take the lvalue path (`LValuePathCompound`),
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

AOT-compiled code reports the source file and line of an exception and of each
call stack frame as AST execution does, with or without debug info and source
text. Each source location is a constant `QoreProgramLocation` in the image,
one per distinct location of the module (`QoreIRToLLVM::getAotLocationConstant()`),
and the code stores its address in the thread's runtime location when the line
changes, as JIT code stores its location pointer; the location does not depend
on the AOT context the code runs with, so code that runs with its caller's
context (a context-independent fast entry) reports its own lines. Every direct
call of user code and every call that the code runs inline as a summary of the
callee's body pushes the callee's frame; see "Frames of native calls" in
[`call-stack-publication.md`](call-stack-publication.md).

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


## Reusable AOT floating callback frames (2026-10-15)

Retained additive float()/float(float)-v4 dispatch reuses invocation-owned
diagnostic storage in loops. It shares the existing four-slot/832-byte budget
with integer-v4. The frame stores no scalar payload; arguments/results remain
native doubles. Both physical stack guards, all live admission/readiness checks,
per-call publication/removal and reader completion remain. No new heap or TLS
operation; every exit releases scratch before its borrowed cache owner. Ordinary
floating-v3/eager-marker and legacy paths remain. This is an AOT optimization,
not a measured IR/JIT speedup.

Fresh five-fork/five-layout Java comparison: floating callback 8.874 -> 7.029
ns/element (-20.80% frozen/-19.78% same-runtime off), Java2.788 (2.52x gap).
All five target ranges are disjoint. Integer callbacks remain6.053 versus
Java1.885 (3.21x); native integer/floating methods3.32x/3.38x. These are matched
whole-workload numbers, not isolated call latency or general language ratios.

This is a targeted tradeoff, not universal non-regression. Final uncaptured/
captured closure aggregates cost1.78%/1.03% versus frozen and2.64%/2.09% versus
same-runtime off; some layouts are worse. Identical frozen executables against
both runtimes confirm+1.72% uncaptured-closure and+2.17% integer-callback
aggregate costs; captured closures-0.57%, legacy floating callbacks+0.06%.
The uncaptured-closure cost spans all five medians, with three disjoint ranges.
The exact runtime-wide cause is not established. Retain the much larger target
gain with these costs explicitly tracked; no more layout selection is inferred.

Release/debug each pass13files29cases8170assertions and both555reference grids;
all64owned release-module aliases/artifacts are restored. The full7500-window
evidence, both revisions, short controls, setup failure, rawJava, runtime-only
control and individual audit are in
`bench/runtime-comparison/results-aot-float-frame-reuse-macos-arm64.json`.
The full13-workload table and caveats are in `bench/runtime-comparison/RESULTS.md`.

Next: persistent native-method contexts, compiler-visible admission and closure
frame costs, retaining live debugger/body invalidation and reader/exception
semantics. A split admission/callee/cleanup ABI needs an assembly and EH proof
before implementation. Member/list gaps need broader execution optimization;
direct/static calls require a separate measurement milestone. Neither parity
nor a performance ceiling has been reached.

The full 62-item audit records 21 Pass, 41 N/A and 0 Fail; every measurement
window is independently replayed. The audit retains the regression caveats,
not a claim that all call paths improve.


## Persistent native-method frame trial rejected (2026-10-15)

The method-frame prototype is removed. Persistent initialized storage reduced
helper frames, but the complete 1755-window screen did not show a worthwhile
broad benefit: native integer 10.138 -> 9.850 ns/element (-2.84% frozen/-3.16% off),
native floating +0.83% frozen/+1.64% off, boxed integer essentially flat, boxed
floating -0.84% frozen/-0.75% off. One native-integer layout regressed 3.89% off;
uncaptured closure aggregate cost 2.55% frozen, with mixed layout results. These are whole
workloads, not isolated calls or a fresh Java comparison.

No method-v3/v4 helper, method scratch, reuse flag or test adaptation remains.
Existing integer/floating callback gains are retained. Candidate release passed
16 files/35 cases/13029 assertions; restored release passed 15 files/34 cases/12069 assertions;
both rebuilt 64 qmods and verified both 555-result reference grids. Restored executable text
and text-symbol addresses match frozen baseline; full binaries embed different
Git revisions. Candidate debug/short/Java confirmation was not pursued after
the inadequate screen. No full-suite/sanitizer/cross-platform claim.

The rejected patch, fixtures, probes, failures, raw windows and full 62-item audit are
in `bench/runtime-comparison/results-aot-method-frame-reuse-rejected-macos-arm64.json`.
The table and per-layout caveats are in `bench/runtime-comparison/RESULTS.md`.
Sharing one method slot is deferred: measured target callers already had one;
mixed-budget callback priority still needs proof. Next distinct target is
deferred scalar-closure context/marker setup, retaining explicit capture reads,
lexical context, stack guards, debugger/fallback and reader semantics. Broader
direct/typed/nullable/monomorphic measurements remain pending. Neither parity
nor a performance ceiling has been reached.

The final docs/data-only audit records 7 Pass, 55 N/A and 0 Fail across all
62 checks, with every measured window independently replayed.


## Scalar-closure context deferral retained (2026-10-15)

The existing bounded integer closure-v1 compiler proof now permits deferred empty
local/capture boundaries and parse/call context. Runtime dispatch still checks
live Program/debugger/body/map/signature state and its physical stack boundary;
the actual native callee checks its own boundary before scalar work. Overflow
materializes context before raising. Per-read capture locking, eager object/class
and optional closure-self context, diagnostic publication/removal and reader waits
are unchanged. No new ABI, compiler lowering, closure ownership or caller storage
is introduced. Previously compiled eligible AOT images benefit from the runtime
change. This is not a measured speedup for ordinary IR/JIT closure execution.

Five-fork/five-layout confirmation: noncapturing closures go from 25.271 to 19.701
ns/element (-22.04% frozen, -23.48% same-runtime eager); captured closures go from
31.410 to 25.545 (-18.67% both). All closure layout ranges are disjoint. Fresh Java
is 1.663/1.738 ns/element, leaving 11.85x/14.70x workload gaps. These are amortized
whole-workload figures, not isolated call latency. Integer/floating callback
controls cost 1.26%/2.01% versus frozen with overlapping ranges; the initial integer
4.83% aggregate and 26.71% adverse layout are preserved. One-element closure
workloads do not benefit (+0.98%/+2.33% frozen).

Release/debug each pass 21 files, 54 cases and 12260 assertions, 64 qmods and the
555-result IR/AOT grids.
The new fixture tests actual native entries, forced post-publication overflow,
caller-state restoration, pending/readiness misses and foreign Program/thread use.
A test-only portability/explicit-AST follow-up passes 18 assertions in each build
without changing measured runtime artifacts; all 64 release qmod aliases are restored.
All 3765 windows, initial setup failure, adverse results and artifact identities are
in `bench/runtime-comparison/results-aot-closure-deferred-context-macos-arm64.json`.
The full 62-item audit accompanies the retained change: 21 Pass, 41 N/A and 0 Fail.

Next: prove whether eager closure object/class/self context and immutable signature
checks can move off the hot path, then evaluate callee-guard and capture costs.
Each step requires frozen and same-runtime controls, observability/fallback tests
and fresh measurements. Closure setup at short counts and the broader call matrix
remain open. Neither Java parity nor a performance ceiling has been reached.


## Exact-type closure admission trial rejected (2026-10-15)

Skipping the generic-substitution query after exact built-in `int(int)` admission
is semantically valid, but this standalone trial is not retained. Initial plain/
captured closure gains of 2.25%/2.13% versus frozen reduce to 2.06%/0.81% in the
five-fork confirmation; list mutation costs 1.26% initially and 1.76% in confirmation.
Confirmation has wide fork variation, and almost all ranges overlap. No samples
are discarded and no causal explanation is inferred. All runtime/test edits and
the trial control flag are removed; the earlier closure-context gains remain.

Fresh comparison of the frozen retained runtime: plain/captured closures
20.592/26.558 ns/element versus Java 1.740/1.811, or 11.84x/14.67x. These are matched
whole-workload numbers, not isolated calls or a general Qore/Java ratio. The rejected
candidate's slightly lower figures do not represent installed performance.

Candidate release/debug each pass 21 files, 54 cases and 12292 assertions; restored
release passes 21 files, 54 cases and 12260 assertions. Each rebuilds 64 qmods and
passes both 555-result IR/AOT reference grids.
Active module aliases point to restored release; debug is rebuilt before further
debug qualification. All 2955 windows, controls, candidate patch, assembly and the
full 62-item docs/data audit (7 Pass, 55 N/A, 0 Fail) are in
`bench/runtime-comparison/results-aot-closure-admission-rejected-macos-arm64.json`.
Full tables, uncontrolled host variation and terminal-recovery details appear in
`bench/runtime-comparison/RESULTS.md`. No short-invocation, full-suite, sanitizer or
cross-platform claim.

Next distinct target: eager closure object/class context, preserving exception
materialization, closure-self state, per-read capture locks and reader waits.
Callee guards, capture lookup, setup and the broader call matrix remain open.
Neither parity nor a performance ceiling has been reached.

## Optimized branch-wide checkpoint (2026-10-15)

Fresh, interleaved release/AOT comparison with current develop `347db1015`
uses identical Apple Clang `-O3 -DNDEBUG`, LLVM 22.1.8 and qcc `-O3` settings.
Feature `c980bb3bc` improves direct/final kernels 4.56x/4.39x, typed callbacks
roughly 28–42x, and plain/captured closures 6.77x/6.05x versus develop.
These are whole-workload gains, not isolated call latency.

Broader synthetic processing is mixed: callback pipeline 3.47x, object orders
1.112x, text indexing 1.044x; CSV report processing takes 2.96% more time.
List mutation regresses 15.88%, with all three layout ranges disjoint.
Temporary hashes are aggregate-flat but one layout regresses 9.53%.
There is no single general Qore application speedup.

A separate fresh optimized Java checkpoint puts direct/final Qore at
1.565/1.642 ns per element versus Java 1.022/1.025; plain/captured closures
remain 12.05x/14.60x Java time. All 37 call/collection rows and all four mixed
workloads appear in `bench/runtime-comparison/RESULTS.md`. All 7020 timing
windows, build provenance, raw samples and the complete 62-item audit are in
`results-aot-optimized-checkpoint-macos-arm64.json` in that directory.

The next priority is the list-mutation regression, then CSV/hash layout
sensitivity; defer further closure object/class-context work until those are
understood. No Java parity, performance ceiling, cross-Program, general IR/JIT,
production-application, startup or I/O speedup is claimed.

## List mutation: attributed regression and first recovery (2026-10-15)

Optimized compiler/runtime crossover isolates generated-code cost: develop's
mutation executable takes 207.321 ns/element on develop and 168.489 ns on the
feature runtime; feature-generated code on that runtime takes 245.164 ns.
Ordinary scalar parameters become runtime-backed in generic lvalue-mutating
functions. Generic coercion/publication, reloads and reference cleanup therefore
remain important targets. The old faster code is not a safe visibility proof.

The first retained recovery is a runtime-only exact scalar coercion shortcut.
`qore_rt_coerce_value_aot()` checks the live canonical declaration and immediate
int/float/bool representation, retaining identical cleanup publication and full
fallback for every other case. It does not remove runtime stores, reloads,
reflection, ownership or cancellation checks and adds no ABI/helper symbol.
Compatible existing AOT executables benefit; ordinary IR/JIT helpers are unchanged.
`QORE_DISABLE_AOT_EXACT_SCALAR_COERCE=1` retains the generic path for comparison.

In a fresh interleaved release/qcc-O3 session, mutation improves from 245.123 to
233.802 ns/element (-4.62%, -6.76% versus same-binary disabled), but remains 11.22%
slower than develop's 210.224 ns. All three layout medians improve; one range
overlaps. Controls are mixed: boxed-effect +0.76%, closure +0.57% aggregate and
default-layout closure +1.73% with disjoint observed ranges. This is a narrow
retention, not a general non-regression or resolved list-mutation claim.

Selected release/debug each pass 17 files, 61 QUnit cases / 8581 assertions plus
four silent fixtures within the file total, 64 qmods, and 555 reference checks
per IR/AOT mode. Release aliases are restored. All 1530 windows, diagnostic
sampling/assembly, wrapper-output-parser failures, raw evidence, corrected drivers
and all 62 audit items (21 Pass, 41 N/A) are in
`bench/runtime-comparison/results-aot-list-mutation-coerce-macos-arm64.json`.
Full tables and limitations are in `bench/runtime-comparison/RESULTS.md`.
Debug/O0 runs are correctness checks, never performance data; Java was not rerun.

Next: optimize typed-parameter publication/reload with live-state guards or a
sound no-reentry proof, then CSV/hash layout sensitivity, then closure object/class
context. Preserve eager closure-self state, capture locks, reader waits and cold
exception materialization. Java parity and a performance ceiling remain open.

## Compiler-selected exact scalar publication (2026-10-15)

Ordinary exact `int`/`float`/`bool` declarations now select the additive
`qore_rt_assign_exact_scalar_local_aot_v1()` helper (and native-EH counterpart)
for pre-coerced normal AOT assignments. Selection is compile-time work; the helper
still repeats live declaration checks and admits only matching immediate values
into already-assigned fixed primitive storage. It rejects captured, special,
read-only, no-narrow, reference and substituted declarations; finalized, cleared,
unassigned or static storage; wide/owning representations and mismatches. Every
miss retains ordinary weak normalization, lvalue preparation and assignment.
The actual runtime slot remains authoritative and visible. No binding is cached.

New AOT artifacts require the matching additive helper symbols. Existing general
helpers, serialized descriptors, RuntimeConfig layout and ordinary IR/JIT are
unchanged. Compile control `QORE_DISABLE_AOT_EXACT_SCALAR_STORE_LOWERING=1` selects
old helpers; runtime control `QORE_DISABLE_AOT_EXACT_SCALAR_STORE=1` delegates new
calls to the old path. General list/hash stores pay no new scalar entry check.

Two preceding runtime-only wrappers are rejected and fully recorded: they gained
about 8% on mutation but regressed list building 4.02%/2.15%, pipeline 2.14%/2.72%,
and, for the second version, closure 3.07%. Compiler selection restores the old
general helper's seven-instruction tail path and avoids those broad checks.

In the final optimized five-configuration, three-layout/five-fork trial, mutation
falls from 221.731 to 206.127 ns/element (-7.04%, -8.55% against runtime-disabled).
All three before/after ranges are disjoint. Mutation remains 3.17% slower than
develop's 199.802 ns. List building +0.98% and pipeline +1.21% remain reported,
with overlapping before/after ranges; this is not a universal non-regression
claim. Closure is -0.60% before/after. Mixed feature-versus-develop results are
CSV +0.35%, object orders -10.59%, pipeline -71.14%, and text index -3.80%.

Final release/debug each pass 17 selected files / 61 cases / 8653 assertions,
64 qmods and 555 references per IR/AOT mode. Native debug checks pass 960/960 in
enabled/disabled modes, including admission misses, exact bits, pending exceptions,
native EH and node ownership fallback. Reflection, recursive bindings, captures,
references and reentry are tested; exactly 64 release aliases are restored.

All 6435 windows across three separate epochs, rejected variants, the corrected
early debug declaration failure, provenance clarification, raw evidence and all
62 audit items are in
`bench/runtime-comparison/results-aot-exact-scalar-store-macos-arm64.json`.
Full tables and limitations are in `bench/runtime-comparison/RESULTS.md`.
No Java rerun, isolated-latency, full-suite, sanitizer, cross-platform or general
IR/JIT performance claim. Continue with typed-parameter reload/ownership costs,
CSV/hash sensitivity, then closure object/class-context deferral. Java parity and
a justified performance ceiling are still open.

## Immediate scalar reload avoidance by publication witness (2026-10-15)

New `qore_rt_assign_exact_scalar_local_aot_v2()` and native-EH helpers return one
only after exact non-reentrant immediate scalar publication to the live runtime
slot. Ordinary runtime-backed parameters then reuse the same bits in the existing
reload state transition. Failed admission uses ordinary assignment and reload.
Only a direct exception check intervenes; tracker rotation, deferred cleanup,
epoch publication, operand cleanup and all later reloads are unchanged. There is
no cached binding or relaxation of reflection, captures, references or weak values.
New AOT artifacts require matching additive helpers; old artifacts, v1/general
helpers, RuntimeConfig/serialized layouts and ordinary IR/JIT remain unchanged.

`QORE_DISABLE_AOT_EXACT_SCALAR_STORE_RELOAD=1` at runtime keeps fast publication
but suppresses its witness, enabling an identical-binary reload control. At
compilation it selects v1 and ordinary reload. Older store/lowering controls remain.

The optimized three-fork screen gains 6.01% on mutation. Independent five-fork,
three-layout confirmation reduces mutation from 202.764 to 193.163 ns/element
(-4.74%, -4.10% versus same-binary disabled, -2.94% versus develop 199.012).
All mutation layout medians improve, with two disjoint before/after ranges.
The prior mutation regression is not observed versus develop in this run.

This is a narrow retention: temporary hash +1.40%, boxed-effect +1.04%, closure
+0.47%, CSV +0.37%, and text +0.81% remain. Default-layout closure is +5.08% with
disjoint ranges, also present with lowering disabled; its cause is not isolated
to reload removal. Both closure caller/body retain their instruction counts.
Mutation grows four instructions but skips an actual load and retains its frame.
Mixed versus develop: CSV +2.50%, orders -10.37%, pipeline -71.43%, text -2.48%.
No whole-application or universal non-regression claim follows.

Release/debug each pass 17 files / 61 cases / 8713 assertions, 64 qmods and
555 reference checks per IR/AOT mode. Native debug passes 986/986 in three modes;
wide-to-immediate replacement, ownership fallback, pending exception/EH, reflection,
reentry and recursive binding are covered. Exactly 64 release aliases restored.
All 3015 windows, both epochs and controls, raw output, qualification, disassembly,
drivers and 62 audit items are in
`bench/runtime-comparison/results-aot-exact-scalar-witness-macos-arm64.json`.
Full tables and limitations are in `bench/runtime-comparison/RESULTS.md`.

Continue by profiling remaining mutation/CSV/hash costs, then consider prepared
scalar parameter slots only with same-invocation binding/frame-lifetime proof;
later resume closure object/class-context work with eager closure-self, capture
locks, reader lifetime, physical stack guards and cold materialization preserved.
No Java rerun, full-suite, sanitizer, cross-platform, general IR/JIT or startup
claim. Java parity and a justified ceiling are still open.

### Compile-selected canonical string split

The AOT compiler now admits only assigned string operands for the immutable
public builtin `<string>::split(string, bool = False, bool = False)` with one
supplied argument, no generic instantiation and no source/namespace Program.
`qore_rt_pseudo_string_split_aot_v1` avoids generic argument-list/default
preparation and inline-string materialization. It reuses the existing split and
encoding implementation, preserving the builtin frame, physical stack check,
return type, flags, code context and normal result/local cleanup. Type/enum misses
and the `QORE_DISABLE_AOT_STRING_SPLIT` runtime control enter ordinary consuming
dispatch; the same control at compile time retains ordinary lowering. No IR/JIT,
RuntimeConfig layout or serialized-layout change is introduced.

The helper is compiled separately from the SCU. The initial SCU prototype
regressed closure calls 5.19% despite no closure source change; isolation restored
the earlier closure helper instruction shape and confirmation closure time is
+0.34%. No alignment tuning was used and causation is not reduced to instruction
count alone.

Optimized confirmation (three layouts, five forks, 2475 windows): CSV processing
1136.115 -> 969.456 ns/input record, -14.67% before/after, -15.35% versus the
identical-binary disabled control, -12.81% versus optimized develop 347db1015.
All CSV layout ranges improve without overlap. Retain the caveats: temporary
lists +0.84% overall, shuffle layout +3.05% with disjoint ranges; temporary hashes
+0.45%; text indexing +0.14%. No universal non-regression or text-index gain.

Release/Debug each pass seven selected files, 7010 QUnit assertions and 555
reference checks per IR/AOT mode; native checks pass 1028/1028 in both control
modes. The pre-existing AST embedded-NUL/empty-separator hang remains outside
this optimization. All three trials, raw evidence and the full audit are in
`bench/runtime-comparison/results-aot-string-split-macos-arm64.json`; tables and
limitations are in `bench/runtime-comparison/RESULTS.md`.

Continue with remaining generic pseudo calls and container allocation costs,
then closure-context work under the existing lifetime/stack/capture constraints.
There is no new Java comparison or evidence that performance is maximized.

### Guarded string receivers for AOT split

The next checkpoint admits a known-string receiver without claiming it is
assigned. The existing helper already guards actual value types and enum tags,
falling back to ordinary consuming dispatch on a miss. Separator assignment
proof and all canonical overload/default/flags/domain checks remain. No new IR
fact, runtime helper, ABI, serialized layout or RuntimeConfig change is needed;
ownership, exception frames, physical stack checks and runtime-local reloads are
unchanged. `QORE_DISABLE_AOT_STRING_SPLIT_GUARDED_BASE` restores the preceding
assigned-receiver-only selection at compile time. Ordinary IR/JIT is unchanged.

This reaches the hot `trim(tolower(line)).split(" ")` call: all three layouts
have two specialized text-index split sites versus one in baseline/control;
CSV remains two. Optimized confirmation (five forks, three layouts, 2475
windows) gives text indexing 1697.431 -> 1532.624 ns/item, -9.71% before/after,
-9.34% versus the receiver-only compile control, and -10.42% versus frozen
optimized develop `347db1015`. All text layout ranges are disjoint and faster
than before and that control; the three-fork screen gave -9.96% before/after.
Before is a fresh frozen post-merge `d779bf456` executable/runtime pair,
including upstream `53d1e32bd`; the develop measurement is not against that tip.

Retain the adverse results: temporary list +1.48% overall, with all three layout
medians up +1.27% to +1.76%; indirect calls +0.72% overall, shuffled layout
+2.81%. These observed ranges overlap, which does not prove non-regression.
Temporary hash remains +1.71% slower than develop. Other movements are not
attributed to this call; CSV is +0.007% versus the receiver-only control.
Ranges are not confidence intervals and host frequency/load are uncontrolled.

Candidate Release/Debug each pass seven selected files, 7106 assertions and
555 reference checks per IR/AOT mode; native checks remain 1028/1028 in both
runtime control modes. The expanded split fixture has 164 assertions, including
positive normal/invoke selection, optional/missing receiver fallback and
negative separator-proof cases. Post-merge baseline tests also cover upstream
loop cleanup and retry policy. Each Debug wrapper restores 64 Release aliases.
Both trials, all 3015 windows, provenance, preparation history and the full audit
are in `bench/runtime-comparison/results-aot-guarded-string-split-macos-arm64.json`;
all workload/control numbers are in `bench/runtime-comparison/RESULTS.md`.

Continue with the remaining generic trim call and container allocation/cleanup.
Broader assignment facts require an independent proof; a declared return type
alone is not sufficient. No fresh Java, full-suite, sanitizer, cross-platform
or startup result is claimed. Java parity and a global ceiling remain open.

### Bounded inline-integer pair scalar replacement

The next allocation checkpoint extends read-only fixed-list scalar replacement
to a two-element `list<int>` read through an assigned native `index & 1` (either
operand order). Both elements must be assigned and proven to fit the inline
integer representation. Existing alias, escape, reflective visibility,
dominance and exception checks remain. Selection becomes
`first ^ ((first ^ second) & -index)` using existing IR operations; both
constructor expressions still execute. No opcode, runtime helper, calling
convention, ABI or serialized layout is added. Recompile AOT artifacts to benefit;
the shared pass also affects IR/JIT, whose performance is not measured here.
`QORE_DISABLE_IR_BOUNDED_PAIR_SCALAR_REPLACEMENT` is the compile-only control.

The lifetime restriction matters: NativeInt arithmetic facts do not imply an
unboxed interpreter value. An unrestricted prototype lost wide integers after
`DiscardTemps` and was never timed. The retained candidate keeps allocation for
unproven/wide elements. Bounds can come from existing facts, masks, exact
addition/subtraction, or a preceding exact scalar store. The latter proof is
limited to 64 same-block instructions and recursion depth 8, rejects referenced
or captured locals, and stops at possible reentry, cleanup or unknown operations.
Signed inline boundaries and full 64-bit fallback are covered by tests.

Optimized confirmation: temporary-list processing 134.014 -> 18.229 ns/item,
**86.40% less time (7.35x speedup)**; -86.49% versus the compile-disabled control,
-87.12% versus frozen optimized develop `347db1015`. All three layout ranges are
disjoint and faster than before/control. The screen gave -86.45%. Before is the
validated frozen `4cb6acb0a` pair, including merged develop `4f4130fca`; the develop
measurement is not against that newer tip. The changed internal fast-entry
contract symbol is remapped once in each predeclared order file, without moving
an entry; actual function order and allocation removal are checked.

Retain the adverse findings: callback pipeline +1.00% overall and +0.54% to +1.48%
in all three layouts; list mutation +0.32%, boxed-effect calls +0.21%; temporary
hash shuffled layout +4.50% (aggregate -0.61%, still +0.97% versus develop),
indirect-call default layout +1.54%. These ranges overlap; this does not prove
non-regression. No confirmation before/after range is disjoint and adverse.
Unrelated movements are not attributed to the pair transform.

Release/Debug each pass 15 selected files, 3869 harness assertions, 64 qmods and
555 reference checks per IR/AOT mode. The new fixture has 224 harness assertions
plus 2491 explicit value checks per execution, covering mode parity, disabled
selection, O0/O3, EH, strip-source, split compilation and no-debug builds. Native
checks pass 1028/1028 with the control enabled/disabled; 64 Release aliases are
restored after Debug. Both trials and all 2412 windows are retained in
`bench/runtime-comparison/results-aot-bounded-pair-macos-arm64.json` with the full
audit. Numbers and qualifications are in `bench/runtime-comparison/RESULTS.md`.

The investigation also reproduced a wide-integer lifetime failure in the
existing constant-index scalarizer, which does not use this new path. Resolve
that before widening scalarization. Runtime-local accumulation/reload and
boxing/cleanup still remain in the optimized loop; preserve reflective-local
correctness when investigating them. Generic trim setup is another candidate.
No fresh Java comparison or global performance ceiling is established.

### Scalarized numeric lifetime correction

The wide-value follow-up above is resolved by exact numeric snapshots. Removing
an aggregate must not expose constructor values to `DiscardTemps`: even native
integer/float arithmetic can own boxed nodes. New `SnapshotInt`/`SnapshotFloat`
instructions preserve these values in an interpreter frame-owned ledger and
force native unboxing at their definitions. Mutation/PHI inputs and results are
covered; snapshot aliases of native PHIs are predeclared for block storage order.
Aggregate-return summaries retain their existing proven numeric projections.
No object lifetime is extended. See `design/qore-ir-spec.md` for ownership and
serialization details. The earlier bounded inline-pair path remains selected.

Release and Debug each pass 21 selected files, 28 QUnit cases, 4973 assertions,
64 qmod rebuilds and 555 reference checks per IR/AOT. Native checks pass
1082/1082 with the bounded-pair control enabled/disabled. The new fixture has
151 explicit value checks per execution and 104 harness assertions, including
O0/O3, EH, strip-source, split and no-debug variants. Supplemental scalar-CSE
lifetime tests pass in both builds; 64 Release module aliases are restored.

This is retained as a correctness fix, not a general performance win. Optimized
AOT screening (three layouts, three forks) and confirmation (same artifact pairs,
five forks) retain all 2097 windows. Before is frozen `ac156224f` source; the
screen's develop comparison remains frozen optimized `347db1015`, not current
develop. Confirmation direct calls are 1.551 -> 1.548 ns/item (-0.19%), indirect
6.874 -> 6.830 (-0.64%), temporary list 18.072 -> 18.236 (+0.91%), and text index
1543.998 -> 1475.007 (-4.47%). These are whole-workload rates, not call latency.

Adverse findings are explicit: ordinary closures +1.49%, captured closures
+0.95%, callback pipeline +1.46%. Captured closure's shuffled layout slows
+4.44%, with disjoint observed ranges, repeating the screen's +4.61%. Coarse
closure instruction structure matches, but that does not explain the movement
or prove identical machine code. No layout tuning was attempted. Overlapping
ranges in other cases do not establish non-regression. Host load/frequency and
thermals are uncontrolled, and epochs are not pooled. Full results, preparation
failures and audit are in `bench/runtime-comparison/RESULTS.md` and
`results-aot-scalar-snapshot-macos-arm64.json` in that directory.

The next concrete issue is independent and pre-existing: fixed-aggregate
constant-local cleanup misses fused local reads/writes and can remove a live
loop-counter initializer. With a typed-local loop bound, `for (int i = 7;
i < count; ++i)` processes ten instead of three iterations for count ten in
frozen before/current IR/JIT; disabling fixed-list scalarization restores AST
behavior. In the zero-start temporary-list benchmark, the missing initializer
also prevents definite-assignment proof and leaves the counter runtime-backed;
the accumulator is already native. Preserve all fused local accesses in that
analysis, add regressions, then measure whether the counter becomes native.
Do not relax reflective-local or boxed-value ownership checks. Generic trim
setup remains a subsequent target. No fresh Java ratio, full-suite certification
or global performance ceiling is established.

### Fused-local visibility restores native loop execution

The counter issue above is fixed by including the metadata-only local accesses
of AddAssignLocalInt, IncrementLocalInt and BranchIfLtLocalInt in the scalarizer's
constant-local eligibility check. Their locals are excluded from that shortcut.
This preserves live initializers and prevents changing indices from being
substituted as constants. There is no new runtime helper, opcode or ABI, and no
relaxation of reflection, ownership, exception or cancellation checks.

The post-merge baseline `38856b968` includes develop `6f46bbc9b`. It reproduces
eight wrong results in nine targeted probes in IR/JIT/tiered and AOT O0/O3; AST
and disabled-scalarization controls are correct. The candidate fixes all probes.
Release/Debug each pass 22 selected files, 29 cases, 5041 assertions, 555 reference
checks per IR/AOT and 64 qmod rebuilds. Native checks remain 1082/1082 enabled and
disabled. The new fixture checks 343 expected values and 68 harness assertions,
including captures/references, exceptional cleanup, boxed-width values, changing
indices, native-local-disabled controls, split, no-debug and source-stripped AOT.
Upstream's abrupt socket-drop regression passes four assertions in Debug
(skipped in Release); both Debug wrappers restore exactly 64 Release aliases.

Optimized AOT confirmation: temporary list **18.351 -> 1.639 ns/item**, **91.07%
less time (11.20x faster)**. All three layouts have disjoint faster ranges in both
screening and confirmation. The initializer now satisfies the existing native
definite-assignment proof; both temporary-list entry points retain zero list
allocations and remove their runtime counter-update call. Context-independent
fast-entry eligibility is restored too, restoring its original contract symbol
and original predeclared order-file entry. No function is reordered.

Confirmation direct calls are unchanged (1.561 ns/item); indirect calls are
+0.89% (6.866), ordinary closures -1.25% (20.582), captured closures -3.75%
(26.561). The closure movement's cause is not isolated. Retain adverse CSV
+0.54%, text index +0.92%, object orders +0.07%; their observed ranges overlap,
which does not prove non-regression. Three disjoint adverse screen layout pairs
do not repeat as disjoint adverse confirmation pairs. Temporary hash remains
+1.28% versus frozen optimized develop in the screen.

All 2052 windows, before/after source/artifact provenance, qualifications,
reproducers, failed symbol-inventory preparation and audit are recorded in
`bench/runtime-comparison/results-aot-fused-local-macos-arm64.json`; full numbers
are in that directory's `RESULTS.md`. Runtimes are Release -O3/NDEBUG and binaries
qcc -O3. Values are whole-workload rates, not call latency. The screen's develop
reference is still `347db1015`, not the current target tip. No fresh Java result
or IR/JIT timing is claimed; do not manufacture a ratio across timing epochs.

Next inspect residual native-loop overhead, including whether cancellation flag
polling can be rearranged without changing checks or semantics, then generic
trim/builtin-call setup. Each needs independent proof and measured controls.
Refresh paired optimized Java/develop measurements at a stable checkpoint.
Selected tests are not full-suite, sanitizer or cross-platform certification;
Java parity and a global performance ceiling remain open.

### Combined cancellation polling rejected after measurement

The post-`ffa3e8019` experiment combined atomic pending flags before the
thread-private deferral check while preserving availability, full handling and
checkpoint frequency. The optimized three-layout/three-fork screen retained all
972 windows. Against a compile-disabled guard in the identical candidate runtime,
direct calls changed -0.18%, arithmetic +0.07%, temporary list +0.29%, captured
closures +1.02%. The temporary-list candidate is slower in every layout, with
overlapping ranges. Default-layout object orders has disjoint slower ranges
(+2.68%) against the control; unrelated aggregate movements are not causal proof.

The implementation and its experimental test additions are reverted exactly.
Release was rebuilt and requalified after restoration: 24 selected files, 31
cases, 5167 assertions, plus 22 cases/110 cancellation assertions, 64 qmods and
555 references per IR/AOT. Candidate Release had 5205 selected assertions. No
candidate Debug run or five-fork confirmation was justified by the screening
result. No Java/develop rerun occurred, and no cross-epoch ratio is valid.

Evidence and the complete audit are preserved in
`bench/runtime-comparison/results-aot-combined-poll-rejected-macos-arm64.json`;
the results directory documents every workload and adverse observation. This
does not establish a global performance ceiling. The next target is generic
trim/builtin-call setup with canonical overload and exception/context semantics.

### Canonical AOT trim setup validated

The canonical one-argument value overload of trim now has a guarded AOT CallDirect
helper. Exact builtin signature/flags/default/domain proof is done at compilation;
non-string/enum arguments and differing Program/caller contexts retain ordinary
dispatch. The specialized frame preserves runtime parse-option and statement-
location transitions, flags, return type, diagnostic stack, class/object context
and stack checks. It calls canonical Unicode-aware trim on an owned copy, retaining
caller cleanup, exceptions and local reloads. Invoke lowering is unchanged.

An initial prepared candidate was never timed: its parse-option equality guard
always rejected the text-index workload. A debugger confirmed the mismatch. The
retained frame reproduces ordinary dispatch's transition, then restores it. Fresh
qualification and untimed sampling confirm the fast body actually runs. The new
fixture also exposes a pre-existing JIT trim(reference) bug; removing the old
name-only shortcut restores canonical reference mutation. No IR/JIT speedup claim.

Optimized confirmation: text index **1423.941 -> 1288.218 ns/item**, **9.53% less
time**, and **8.73% less than the same-runtime compile-disabled control**. All
three layouts have disjoint faster ranges in both screening and confirmation.
Basic calls change little: direct 1.502, method 1.563, indirect 6.605, closure
19.699, captured closure 25.351 ns/item. These are whole-workload rates, not call
latencies. Adverse indirect +0.80%, direct +0.19%, list mutation +0.21% and CSV
+0.13% versus before remain recorded; closures +1.34% and callbacks +0.64% versus
disabled are also retained. Two adverse callback screen layout pairs do not repeat
as disjoint adverse pairs in confirmation. Overlap does not prove non-regression.

Release/Debug each pass 25 files, 32 cases, 5256 assertions (89 new), plus 22
cancellation cases/110 assertions, 555 references per IR/AOT and 64 qmod rebuilds.
Debug native checks remain 1082/1082 in both control states; upstream abrupt-drop
regression passes four Debug assertions and is skipped in Release. Exactly 64
owned Release aliases are restored after Debug qualification.

All 2592 windows, controls, artifacts, histories, profiles, fixtures and audit are
in `bench/runtime-comparison/results-aot-string-trim-macos-arm64.json`; complete
tables are in that directory's RESULTS.md. Host frequency/load are uncontrolled,
and unrelated movements are not attributed to the optimization. No fresh Java or
develop timing is claimed. Next integrate origin/develop at a clean checkpoint,
refresh paired optimized Java/develop comparisons, then continue profile-led
builtin and list-mutation work. Selected tests are not full-suite/sanitizer
certification; Java parity and a global performance ceiling remain open.

## Current optimized branch/develop/Java checkpoint (2026-10-16)

One fresh interleaved epoch compares feature `5a23d43e0`, current develop
`6f46bbc9b` and warmed Temurin 25.0.2+10/JMH 1.37. Both Qore builds use matching
Release `-O3 -DNDEBUG` flags and `qcc -O3`. All 37 microbenchmarks, four mixed
workloads, three fixed Qore layouts, five forks and 4245 windows are retained.

Direct/final workloads improve 4.85x/4.53x over develop, but take 1.64x/1.72x
Java time. Direct is near its own arithmetic control; these are whole-workload
costs, not isolated call latency. Stable native methods are about 2.05x Java,
typed callbacks 3.88x, plain/captured closures 11.89x/14.60x. Temporary-list
scalarization reaches 1.537 ns/element versus Java 1.161 and develop 135.361.

Mixed CSV/object/callback/text processing improves 1.151x/1.156x/3.467x/1.287x
over develop. List mutation improves 2.43%, with all three layout medians
improved, but still takes 47.86x Java time. Temporary hashes remain 1.46%
slower overall; shuffle-487 is +3.66% with disjoint observed fork ranges.
No universal non-regression or production-application speedup is inferred.

The committed Release rebuild passes 25 files / 32 cases / 5256 assertions,
64 qmods, thread/cancellation tests and IR/AOT references. Every benchmark
artifact and Java also passes its independent reference grid. Full tables,
methodology and limitations are in `bench/runtime-comparison/RESULTS.md`;
raw timing, optimized-build provenance, separate untimed profiles and all
62 audit items are in `results-aot-current-checkpoint-macos-arm64.json` there.

Continue with exact-integer fresh-list push and mutation/local publication
costs, then closure context lifetime work. No cross-Program, new IR/JIT,
full-suite or cross-platform claim. Java parity and a global ceiling are open.

## Exact integer list-push lowering validated

Existing fresh/exclusive in-place list pushes now bypass repeated element
validation only for assigned, non-NOTHING, exact builtin integer source and
list element facts. Native i64 and NaN-boxed i64 use distinct versioned helpers;
the latter shares an existing wide-integer box. Enum base types, soft/optional
values and unproven cases retain normal checks. No new opcode, RuntimeConfig
layout or serialized metadata; borrowed roots, COW, snapshots, paired store/
coercion and cancellation are unchanged. LLVM AOT/JIT both use the lowering;
ordinary IR interpretation is unchanged and only AOT speed is measured.

Optimized five-fork confirmation improves list building **86.529 -> 79.591
ns/item**, **8.02% less time**, and **7.68% less** versus the same-library
compile-disabled control. All three fixed layouts improve with disjoint faster
observed ranges in both three-fork screening and confirmation. The screening
indirect-call adverse shuffled layout does not repeat as disjoint adverse in
confirmation. Small aggregate slowdowns remain (captured closure +1.01%, direct
+0.52%, method +0.09%, temporary hash +0.14% versus before). List mutation changes
only -0.17%; no general call-speed or indexed-mutation improvement is claimed.

Release/Debug each pass 32 fixtures, 34 cases, 5333 assertions, plus 22 thread/
cancellation cases and 110 assertions, 555 references per IR/AOT and 64 qmod
rebuilds. All 1082 Debug native tests pass in both bounded-pair states; four abrupt-drop
assertions pass in Debug (Release skips). All 3024 timing windows, complete
control tables, source/artifact identity, untimed profiles, earlier revisions
and audit are in `bench/runtime-comparison/results-aot-native-int-list-push-macos-arm64.json`.

Next: paired typed-list coercion/publication remains a larger list-building
cost, and indexed mutation still uses generic lvalue navigation. Prove a narrow
elision or specialized publication contract before changing those paths. The
prior optimized checkpoint remains the separate Java/develop comparison; no
cross-epoch ratios, Java parity or global performance ceiling are claimed.

## Exact integer AOT paired-store coercion validated

After merging develop `2dffa9a52` into feature at `1a709c1bc`, a qualified frozen
baseline includes the previous integer append specialization. AOT now skips
repeated coercion only when the exact in-place integer helper's borrowed result
returns to the same ordinary canonical `list<int>` declaration through its
paired normal store. Runtime assignment, ownership/cache bookkeeping, weak
normalization, reflection and cancellation remain. A per-function compiler map
provides the proof; no runtime state, opcode or ABI changes. The compile control
is `QORE_DISABLE_AOT_IN_PLACE_INT_LIST_STORE`. JIT/interpreter are unchanged.

Optimized confirmation improves list building **77.551 -> 51.913 ns/item**,
**33.06% less time** versus the merged baseline and **32.88% less** versus the
identical-runtime disabled control. All three fixed layouts improve with
disjoint faster observed ranges in both screening and confirmation. All 3024
windows remain, without pooling, retries or overlapping owned workloads.

Retained as a targeted gain, with unresolved non-target slowdowns: plain
closures/object workloads are +2.97%/+1.85% versus before in confirmation,
and +1.84%/+1.50% versus disabled. Six adverse disjoint pairs in screening and
two different pairs in confirmation remain in the report. No identical pair
repeats, but aggregate closure/object slowdowns occur in both epochs. Untimed
instruction comparison finds unchanged closure, object and callback hot bodies;
that does not exclude layout/cache/allocator effects or prove non-regression.
No general call-speed or indexed-mutation improvement is inferred.

Release/Debug each pass 38 selected fixtures / 44 cases / 5723 assertions,
including 121 focused checks. Upstream HTTP tests add 10 cases / 30 assertions;
thread/cancellation tests add 22 / 110; IR/AOT each pass 555 reference checks.
Debug native tests pass 1082 in both bounded-pair states. All 64 owned Release
qmod aliases are restored. Full timing, qualification, profile, diagnostic
history and audit evidence is in
`bench/runtime-comparison/results-aot-in-place-int-list-store-macos-arm64.json`.

Next: the retained runtime assignment is now the largest sampled list-building
cost. Prove an unchanged-root publication optimization separately, preserving
runtime ownership, cache freshness and reflective replacement; do not relax
general paired COW writeback. Then revisit closure context costs. The previous
Java/develop checkpoint is a separate older epoch; no new Java/develop ratio,
global ceiling or full-suite/cross-platform certification is claimed.

## Unchanged integer-list AOT publication validated

Building on `ae773cf87`, AOT now avoids redundant assignment of an unchanged
exact integer-list root to its already-owning runtime body-local slot. Besides
the paired coercion proof, admission requires ordinary runtime-backed body-local
ownership, a redundant-store marker, no closure use or weak assignment, and no
fast-entry parameter/pre-instantiated cleanup owner. Common cache ownership,
cleanup, freshness and exception handling remain; generic COW replacements
still publish. The control is `QORE_DISABLE_AOT_IN_PLACE_INT_LIST_PUBLICATION`.
There is no new runtime ABI/opcode or ordinary IR/JIT change.

Optimized confirmation improves list building **52.801 -> 25.269 ns/item**,
**52.14% less time** versus the exact qualified frozen committed baseline and
**51.97% less** versus the identical-runtime disabled control. All three fixed
layouts improve with disjoint faster ranges against both controls in two
epochs; all 3024 windows remain without pooling or retries.

Retained as a targeted gain, not universal non-regression. Screening's one
disjoint adverse pair (arithmetic/reverse +1.071% versus disabled) does not
repeat in confirmation, which has no disjoint adverse pair. Confirmation still
has indirect calls +1.93% versus before/+2.74% versus disabled, plain closures
+3.82% versus disabled, object processing +0.55% versus before and text +0.32%.
Unchanged generated indirect/closure/object/callback hot bodies do not rule out
layout/cache/allocator or host effects. All movements are retained in the report.

Release/Debug each pass 39 fixtures / 45 cases / 5866 assertions, including
143 publication checks, plus HTTP 10 cases / 30 assertions, thread/cancellation
22 / 110 and 555 references per IR/AOT. Debug native tests pass 1082 in each
bounded-pair state. All 64 Release qmod aliases are restored. Earlier successful
qualification before comment-only proof clarifications is also preserved.
Evidence and all audit items are in
`bench/runtime-comparison/results-aot-in-place-int-list-publication-macos-arm64.json`.

Repeated runtime assignment is no longer a major sampled list-building cost;
indexed reads, append/growth and reference cleanup remain. Next investigate
rejected ordinary-function-reference lookup on the dynamic closure route,
without retaining closures or weakening live Program/debugger/stack guards.
Refresh matched optimized Java/develop measurements after the next retained
batch. Earlier Java/develop numbers remain a separate epoch, not a new ratio;
Java parity and a global performance ceiling are still unestablished.


## Borrowed closure miss route validated

Based on `4a2960cfa`, the prepared `int(int)` callback route bypasses ordinary
function-reference preparation for a live runtime closure on a cache miss.
That preparation always rejects closures without cache mutation or ownership
replacement. The closure remains borrowed; all native continuation admission,
live Program/debugger/body/readiness/signature/stack checks, self/class context,
capture locks, reader waits and fallback behavior remain. Positive ordinary
function-reference hits skip the new type/gate probe; other signatures compile
out the change. No new ABI/layout/opcode or cancellation behavior.

Optimized confirmation improves plain closures **20.402 -> 19.516 ns/item
(-4.34%)**, captured closures **26.491 -> 25.892 (-2.26%)** versus the exact
qualified frozen baseline. Runtime-disabled control
`QORE_DISABLE_AOT_CLOSURE_MISS_ROUTE=1` uses the identical candidate executable
and library; changes versus it are -8.92%/-5.41%. The disabled control includes
the new branch/inlining decisions and is not the old baseline. All 4752 windows
remain across separate three/five-fork trials, three fixed layouts and 22
workloads. Both targets meet predeclared thresholds; some ranges overlap.

Retain narrowly, not as universal non-regression. Six disjoint adverse pairs
in screening do not recur in confirmation, but aggregate list mutation
(+1.71%), tagged-float callbacks (+1.41%), affine-wide callbacks (+0.68%) and
temporary hashes (+0.40%) remain slower versus before. Generated benchmark
bodies are instruction-identical; runtime inlining changes. Neither observation
eliminates layout/cache/host effects or explains away adverse measurements.

Candidate Release/Debug each pass 56 fixtures / 88 cases / 10591 assertions,
plus HTTP 10 / 30, thread/cancellation 22 / 110 and 555 references per IR/AOT.
Debug native 1082 passes in both bounded-pair states; all 64 Release qmod links
are restored. The unchanged baseline also passes 17 expanded call fixtures /
4725 assertions. A pre-existing macOS object/link target warning was resolved
by aligning test/benchmark linking to macOS 28, without runtime rebuild flags,
warning suppression or weakened assertions; the original failure is retained.
Full evidence and all audit items are in
`bench/runtime-comparison/results-aot-closure-miss-route-macos-arm64.json`.

The rejected lookup disappears from the candidate profile, but closure context
and synchronized capture reads remain costly. Next prove whether immutable
signature checks can be moved to compilation/entry binding, preserving every
live guard and capture lifetime rule; measure this separately. Refresh the
matched optimized Java/develop comparison after the next retained batch.
No fresh Java/develop ratio, IR/JIT speed claim, full-suite/cross-platform
certification or global performance ceiling is inferred.


## Rejected dynamic closure signature-proof shortcut (post-906a1d5cf)

The compiler's dynamic integer closure eligibility already proves canonical
builtin `int(int)` without type substitution, and AOT signature reconstruction
preserves that proof. A measured candidate trusted the published companion in
Release, retained a Debug assertion and offered a same-artifact runtime recheck
control. It did not cache captures or relax live Program, body, readiness,
debugger, physical-stack, exception, lexical-context or capture-lock checks.

**The shortcut is not retained.** In optimized three-layout confirmation, plain
closures improve 19.556 to 18.716 ns/item (-4.296%) and captured closures improve
25.603 to 24.714 (-3.471%), but list building worsens 26.527 to 27.534 (+3.797%).
List building has a repeated >=3% adverse median with disjoint ranges in all
three layouts across screen and confirmation, failing the predeclared rule.
All four production/header files are restored exactly to `906a1d5cf`; dispatch
continues to recheck the exact signature. The experimental
`QORE_DISABLE_AOT_CLOSURE_SIGNATURE_PROOF` control has no effect in the restored
runtime. The new 208-assertion signature-selection/fallback regression remains.

All 4752 windows, including smaller adverse results, are preserved in
`bench/runtime-comparison/results-aot-closure-signature-proof-rejected-macos-arm64.json`
and summarized in `bench/runtime-comparison/RESULTS.md`. Timed benchmark bodies
are instruction-identical; only the micro `_main` metadata-length constant
differs. Runtime disassembly verifies the intended bypass. The repeated
non-target slowdown is unexplained, not dismissed as a layout effect. No third
rescue trial or alignment search was used.

Candidate and freshly restored Release/Debug each pass 57 selected fixtures,
89 cases and 10799 assertions, plus upstream HTTP/offline LLM, cancellation,
native and IR/AOT reference checks. This does not establish full-suite or
cross-platform qualification, fresh Java/develop ratios, or a global ceiling.
Next: refresh the matched optimized branch/develop/Java checkpoint, then choose
a distinct AOT-first target from remaining live admission/context/capture costs.

## Optimized post-list/closure checkpoint (2026-10-17)

One fresh interleaved epoch compares feature `dcdb7548b`, merged develop
`14f418ea7` and warmed Temurin 25.0.2+10 / JMH 1.37. Matching Release
`-O3 -DNDEBUG`, LLVM 22.1.8, qcc `-O3`, allocation tracking off. Feature uses
the restored, qualified `906a1d5cf` binaries; the later commit only added
tests/docs/evidence, and runtime source and artifact identities are verified.
All 37 microbenchmarks, four mixed workloads, three fixed layouts, five forks
and 4245 windows are retained. No best-layout selection or cross-epoch ratios.

Direct/final workloads improve 4.62x/4.51x over develop but take 1.57x/1.66x
Java time; direct is near its own arithmetic control. Stable native methods
are 2.05x Java, typed callbacks 3.97x, plain/captured closures 11.32x/14.24x.
The effectful boxed method remains 46.90x Java. Whole-workload costs are not
isolated call latency; cross-Program calls are not measured.

List construction improves 4.08x over develop but remains 10.06x Java. List
mutation improves 4.27% but remains 47.58x Java. Mixed CSV/object/callback/text
processing improves 1.150x/1.148x/3.519x/1.276x. Temporary hashes are still
1.35% slower overall; all three layout medians are adverse. No disjoint
adverse observed fork ranges, but overlap does not prove non-regression.

The unchanged Release runtime qualification covers 57 files / 89 cases /
10799 assertions plus HTTP, offline LLM, cancellation/thread and IR/AOT
reference checks. Every benchmark layout and Java also passes its reference
grid. Methodology, full tables, provenance/replay limitations and raw evidence
are in `bench/runtime-comparison/RESULTS.md` and
`results-aot-post-closure-checkpoint-macos-arm64.json` there.

Untimed profiles distinguish generic push/coercion/publication in
`listMutate` from the specialized `listBuild` path. Next establish whether
later lvalue mutation unnecessarily blocks earlier fresh-list construction,
then improve indexed mutation with exact semantics. Native RuntimeConfig
snapshot/context and effectful method overhead remain followups. No new
full-suite, sanitizer, cross-platform or general IR/JIT speed claim. Java
parity and a justified global performance ceiling remain open.


## Rejected guarded integer-list COW append (post-4428adbb2)

The measured guarded AOT append is not retained. An initial declaration-only
coercion proof failed Debug when reflection replaced a `list<int>` local with
plain-list runtime metadata; it was removed before timing. The corrected
candidate guards actual list validity/integer element type and retains ordinary
COW, owned results, paired-store coercion and replacement publication. New tests
check metadata before return filtering, plus wide boxes, aliases, sparse slots
and fallback conversion. Baseline AOT O0/O3 verifies the strengthened check;
AST's existing internal metadata behavior differs but ordinary values agree.

One optimized three-fork, three-layout screen retains all 1782 windows across
18 micro/four mixed workloads. List mutation goes 198.465 to 198.957 ns/item
(+0.248%), versus disabled 199.048 (-0.046%). All baseline layout medians are
slower; neither predeclared 2% target passes. No confirmation/rescue rerun.
Indirect calls are +3.557% in this single screen; every smaller/control/adverse
result is retained in RESULTS and the JSON evidence. Generated-body equality
does not invalidate timing differences. No new Java/develop ratio is inferred.

All three production files are restored exactly; neither the new helpers nor
their diagnostic compile gate remain. Only regression tests, documentation and
evidence are retained. Candidate Release/Debug each pass 58 files / 91 cases /
11139 assertions; fresh restored builds each pass 58 / 91 / 10969, plus HTTP,
offline LLM, cancellation/thread and 555 references in each IR/AOT mode. Debug
native checks pass 1082 in both bounded-pair modes, abrupt-drop four. Exactly
64 Release aliases are restored. The 62-item audit reports 21 Pass / 41 N/A /
zero Fail. Full failed-proof, corrected-candidate, rollback and replay evidence:
`bench/runtime-comparison/results-aot-exact-int-list-cow-rejected-macos-arm64.json`.

Untimed profiles confirm selection of the guarded append but retain major
general lvalue/coercion/local-cleanup paths. Lvalue single-index navigation also
allocates its object-count vector; any inline-storage followup must preserve
all tracking and moved-from/destructor semantics. Next evaluate native-only
RuntimeConfig snapshot access; nullable public accessors and independent live /
snapshot restoration must remain intact. No retained speedup from this failed
candidate, full-suite/cross-platform certification, Java parity or global
performance ceiling is claimed.


## Rejected native-only snapshot access (post-799ddfaa5)

The RuntimeConfigNativeObjectHelper non-null accessor candidate is not retained.
It removed nullable/default allocation guards without changing public behavior,
independent live/snapshot pairs, current-Impl restoration, admission, ownership,
capture locks, stack observation or cancellation. Optimized three-layout,
three-fork screening found plain closures 19.260 -> 19.236 ns/item (-0.127%) and
captured closures 25.572 -> 25.413 (-0.624%); one layout per target was slower.
Both miss the predeclared 2%/all-layout criterion. All 1188 windows are retained,
including list read +3.181% and every smaller adverse result. No confirmation
or rescue trial. No fresh Java/develop comparison is inferred.

Assembly confirms the intended guard removal, but not sufficient measured gain.
Both production files are restored exactly. Stronger context tests remain:
separate live/snapshot state, current Impl replacement, public moved-from
semantics, owned live and deleted receivers, private-member access, native miss,
overflow/recovery, pending sink and readiness/Program rejection. Two corrected
baseline test assumptions and all original outputs are archived.

Candidate and freshly restored Release/Debug each pass 59 selected fixtures /
92 cases / 11621 assertions, plus HTTP, offline LLM, cancellation/thread,
555 IR + 555 AOT references and Debug native/abrupt-drop checks. Exactly64
Release aliases restored. The 62-item audit records21 Pass/41 N/A/zero Fail;
all1188 windows independently replayed. Full results and provenance:
`bench/runtime-comparison/results-aot-native-snapshot-access-rejected-macos-arm64.json`.

Next target: inline the first lvalue object-count record, preserving all nested
tracking, reverse propagation, clear/reuse and moved-from emptiness. This
addresses an allocation visible in the list-mutation profile. Deferred closure
context needs a distinct versioned contract and proof, not an unannounced change
to v1. Neither Java parity nor a global performance ceiling is established.


## Object-removal baseline correction before inline lvalue records

Expanded baseline testing found an existing Debug assertion in the AOT
`DgcRemoveScanChecks` module before any inline-record implementation or timing.
Removing a child's last object-valued member incorrectly decremented the
parent's scan count although the parent still held the child. Empty objects
remain scan-relevant values. A minimal repeated-removal case reproduced this
in AST, IR, JIT, tiered mode and AOT, for both single members and slices.

The correction removes those two false parent-delta updates. Child member
accounting, access checks, locks, removed-value lifetime and recursive-set
scans remain intact. Regression cases repeatedly remove through object,
hash and list parents and the Debug reference API, then create a back edge
and check exact destruction counts. Additional strong/weak/opaque edge probes
preserve live objects and release them exactly once; four AOT cases abort in
the original frozen runtime and all 30 cases complete after the correction.
These cases are also added to the permanent fixture before final qualification.
Compiled-module tests cover O0/O2/O3;
source tests include tiered mode. Native record-order/clear/move/borrowed-owner
tests add 789 assertions without changing the vector representation.

Fresh Release and Debug each pass 59 core fixtures /
92 cases / 11621 assertions. Additional
lvalue/DGC qualification passes 13 fixtures /
82 cases / 1330 assertions
in Release and 13 / 82 /
2663 in Debug, plus HTTP, offline LLM, thread and
cancellation checks and 555 IR + 555 AOT references. Debug native tests pass
1871 assertions in each bounded-pair mode. Exactly 64 Release module aliases
are restored. This is selected macOS arm64 qualification, not full-suite or
sanitizer certification.

The 62-item audit-changes review records 19 Pass / 43 N/A / zero Fail.

|!Audit group|!Status|!Evidence
|Fixture structure and bookkeeping coverage|Pass|Modern executable test, shipped dependencies, exact destruction counts, native borrowed-record tests
|Exceptions, ownership and thread behavior|Pass|Child accounting and existing locking/scanning preserved; only the false parent delta removed
|Performance claims and provenance|Pass|Original failures preserved; no timing or speedup attributed to this correctness prerequisite
|Documentation and regression qualification|Pass|Strict Qore tables, fresh Release/Debug suites and identical minimal before/after sources
|New module, QPP, DataProvider and JNI registration|N/A|No such change

Evidence, original failures, corrected minimal cases and the complete
audit-changes checklist are in
`results-lvalue-object-removal-correction-macos-arm64.json` (under
`bench/runtime-comparison/`). No timing or speedup is claimed for this
correctness prerequisite. The last matched Java/develop performance checkpoint
predates this change. Next measure inline first-record storage against this
corrected baseline; Java parity and a global performance ceiling remain open.


## Validated inline lvalue object-count record (post-1356f556a)

The storage optimization is retained after both predeclared epochs pass. Optimized confirmation list mutation is
198.897 -> 185.712ns/item (-6.629%).
All 3168 windows across 22 workloads and three fixed layouts are retained;
the complete target/regression rule, all gains/losses and both epochs when run
are in RESULTS and `results-aot-lvalue-inline-record-macos-arm64.json` under `bench/runtime-comparison/`.
No new Java/develop or general IR/JIT ratio is inferred.

The candidate stores only the first borrowed object-count snapshot inline.
All record sites, scan decisions, locks, ownership and reverse propagation
remain unchanged. Move explicitly empties the source; clear preserves vector
overflow capacity. The previously committed 789 record checks pass against both
the corrected vector baseline and candidate. The separately audited object-removal
correction is identical in both runtimes. Release/Debug pass 59 core fixtures, 92 cases, 11621 assertions plus 13
lvalue/DGC suites, upstream HTTP/LLM/thread tests, 555 IR + 555 AOT references and
Debug 1871 native checks in both bounded-pair modes. All 64 Release aliases restored.
The 62-item audit records 16 Pass / 46 N/A / zero Fail. Selected qualification only;
no full-suite/sanitizer/cross-platform or global-ceiling claim.

Next refresh the matched optimized branch/develop/Java checkpoint, then investigate residual indexed-mutation and native-context costs. Java parity remains open.


## Optimized post-lvalue checkpoint (2026-10-17)

Fresh interleaved feature `ed93cc675`, merged develop `14f418ea7` and warmed Java.
Matched Release `-O3 -DNDEBUG`, qcc `-O3`, LLVM22.1.8, five forks and three
fixed Qore layouts. All37micro/fourmixed and4245windows retained.
Whole-workload costs, not isolated calls; no cross-Program results.

|!Workload|!Develop / feature time|!Feature / Java time
|direct_call|4.658x|1.611x
|method_call|4.540x|1.705x
|monomorphic_call|3.386x|2.017x
|indirect_call|33.398x|3.973x
|polymorphic_call|13.531x|8.846x
|closure_call|7.211x|11.386x
|closure_capture|6.382x|14.330x
|list_build|4.040x|10.090x
|list_mutate|1.099x|44.459x

Full tables, every adverse layout, semantic caveats and exact qualification
provenance are in RESULTS and `results-aot-post-lvalue-checkpoint-macos-arm64.json`
under `bench/runtime-comparison/`. No universal non-regression, production
throughput, general IR/JIT, full-suite or sanitizer claim. The effectful Java
field-update workload is unsynchronized; Qore object locks remain unchanged.

The feature runtime exactly matches the audited Release/Debug candidate
qualification. No runtime code change accompanies this evidence checkpoint.
Audit:7Pass/55N/A/zeroFail; all4245windows independently replayed.
Next investigate residual indexed mutation and versioned deferred closure
context with separate correctness proof and matched optimized measurements.
Java parity and a justified global performance ceiling remain open.


## Fully deferred scalar closure context v2 (post-a0e895a56)

Retained: optimized confirmation plain closure 19.735 -> 18.021 ns/item (-8.682%),
captured closure 25.825 -> 24.815 (-3.911%). Both improve every fixed layout in the
three-fork screen and five-fork confirmation; all 3168 windows retained. Effectful
boxed methods regress 2.316% / 2.918% in aggregate, with a 5.021% disjoint adverse
confirmation/default result. The predeclared rule passes, not universal non-regression.
See RESULTS and `results-aot-closure-context-v2-macos-arm64.json` under `bench/runtime-comparison/` for all gains/losses.

Append-only v2 binding adds a complete lowered-call proof and defers object/class/
self substitution on successful eligible closure calls. V1 semantics, same-Program/
identity/debugger/stack/sink guards, synchronized capture reads, immutable per-call
diagnostic publication and reader waits remain. Exception materialization/restoration
preserves independent live/snapshot views and current-Impl moves. No extra ownership.
Release/Debug pass 59 core files / 92 cases / 11703 assertions plus 13 lvalue/DGC suites,
HTTP/LLM/thread checks and555 IR + 555 AOT references; Debug 1985 native checks in both
bounded-pair modes. All 64 Release aliases restored. Audit 21 Pass / 41 N/A / zero Fail.
Selected qualification only; no fresh Java/develop or general IR/JIT ratio.

Next diagnose effectful-method local/reference/member costs; then evaluate a
separate additive closure companion with an inline live stack-limit check and
the old full-guard fallback. Java parity/global performance ceiling remain open.


## Rejected closure inline live-stack guard v3 (post-609a53eb9)

Three-fork optimized screen: plain closure 17.229 -> 17.035 ns/item (-1.126%)
fails the >=2% target and default layout is +0.491%; captured closure improves
23.573 -> 22.663 (-3.862%). No confirmation/rescue epoch. Broad observed losses
include list reads +3.345%, list mutation +2.035%, CSV +2.235%, object orders +3.395%.
All 1188 windows retained. See RESULTS and `results-aot-closure-inline-stack-v3-rejected-macos-arm64.json` under `bench/runtime-comparison/`.

All four production and four v3-dependent fixture files are archived and restored
exactly; the committed v2 implementation remains. Fresh restored Release/Debug
pass 59 core files / 92 cases / 11703 assertions plus lvalue/DGC, HTTP/LLM/thread,
555 IR + 555 AOT and Debug 1985 native checks per mode; 64 Release aliases restored.
Candidate-specific qualification was 11817 assertions per build, not claimed for
the restored code. Evidence-only audit: 7 Pass / 55 N/A / zero Fail. No v3 runtime API/control
is retained. This is selected qualification, not a general performance ceiling.

The inline check works in generated code, but dispatcher identity/type/signature,
frame publication and capture synchronization remain. Next verify conditional
receiver/Phi provenance as profitability input for the existing guarded object
parameter cache, preserving epoch/ownership and weak-load cleanup semantics.
Measure short invocations and broad optimized workloads. Java parity remains open.


## Rejected conditional receiver object-parameter cache (post-5d3fc6e39)

The targeted ordinary conditional-object loop improves 6.881% in the optimized
screen and 12.852% in confirmation (314.087 -> 273.719 ns/item), all layouts faster.
Reject nevertheless: boxed_int_method_call/shuffle-487 is repeatedly 4.268% / 4.198%
slower with disjoint adverse ranges, triggering the predeclared broad-regression
gate. All 4464 windows and smaller losses are retained; no rescue or rule change.
See RESULTS and `results-aot-conditional-object-param-rejected-macos-arm64.json` under `bench/runtime-comparison/`.

The compiler source is restored exactly; two new candidate-only fixtures are
archived with executable modes and removed from the working tree. No runtime API
or new control remains. Fresh restored Release/Debug each pass 63 core files /
96 cases / 13287 assertions plus lvalue/DGC, HTTP/LLM/threads, 555 IR + 555 AOT and
1985 Debug native checks per mode; 64 Release aliases restored. Candidate-specific
14407 assertions are not claimed for restored code. Evidence-only audit: 7 Pass /
55 N/A / zero Fail across 62 items.

Generated code adds cache admissions/epoch guards and grows the boxed-int frame
432 -> 512 bytes; the target frame grows 768 -> 848 bytes. Untimed profiles still
show generic zero-argument method/frame/context/member-load work. Next investigate
an append-only zero-argument scalar getter/pure-method companion while preserving
the unary contract and all live guards/synchronized reads. This needs a new
predeclared optimized campaign; it is not a gain claim. Java parity remains open.


## Rejected zero-argument scalar getter entries (post-849454696)

Integer getter loop: 280.434 -> 157.997 ns/item (-43.660% time). Floating getter loop: 314.270 -> 165.712 ns/item (-47.271% time). The candidate is not retained. Repeated adverse non-target layouts: boxed_float_method_call/default, boxed_int_method_call/default, closure_call/default, closure_call/shuffle-487. Complete source/patch/modes are archived; production is restored and independently requalified. No rescue epoch or retention-rule change.

All 4752 optimized windows are preserved across 33 workloads/three fixed layouts.
Six-argument zero-argument getter companions have separate signature admission;
the unary ABI/witness is unchanged. Full live dispatch/body/Program/debugger/stack
guards, synchronized member reads, eager explicit context and reader-safe logical
frames remain. No member value is cached and no lock is elided.

Candidate Release/Debug each pass 69 core files / 112 cases / 17427 assertions, including
885 getter assertions, plus lvalue/DGC, HTTP/LLM/threads, 555 IR + 555 AOT and 1985 Debug native
checks per mode. Baseline core 63/96/13287 plus five supplemental suites 12/3255 per
build is separately fingerprinted. All 64 owned qmods rebuilt and Release aliases
restored. Audit: 7 Pass / 55 N/A / zero Fail across 62 items. See RESULTS and
`results-aot-zero-arg-getter-rejected-macos-arm64.json` under `bench/runtime-comparison/` for every gain/loss, provenance,
development attempt and qualification limit. No fresh Java/develop ratio is claimed.

Remaining work includes measuring caller-native getter continuations and separately
proven context deferral, then a matched optimized Java/develop checkpoint. Java parity
and a justified global performance ceiling remain open.


## Rejected caller-selected isolated zero-argument scalar getter entries (post-e9b2c7235)

Integer getter loop: 280.403 -> 137.344 ns/item (-51.019% time). Floating getter loop: 313.892 -> 146.731 ns/item (-53.254% time). The candidate is not retained. Repeated adverse non-target layouts: boxed_float_method_call/default, boxed_float_method_call/reverse, default_loop/default, default_loop/reverse, default_loop/shuffle-487, default_short/default, default_short/reverse, monomorphic_call/default, string_result_loop/default, string_result_loop/shuffle-487. Complete source/patch/modes are archived; production is restored and independently requalified. No rescue epoch or retention-rule change.

All 5904 optimized windows are preserved across 41 workloads/three fixed layouts.
Six-argument zero-argument getter companions use caller-selected isolated dispatch;
the shared PIC layout and unary ABI/witness are unchanged. Full live dispatch/body/Program/debugger/stack
guards, synchronized member reads, eager explicit context and reader-safe logical
frames remain. No member value is cached and no lock is elided.

Candidate Release/Debug each pass 69 core files / 112 cases / 17563 assertions, including
1021 getter assertions, plus lvalue/DGC, HTTP/LLM/threads, 555 IR + 555 AOT and 1985 Debug native
checks per mode. Baseline and restored core 68/108/16542 per build are separately fingerprinted. All 64 owned qmods rebuilt and Release aliases
restored. Audit: 7 Pass / 55 N/A / zero Fail across 62 items. See RESULTS and
`results-aot-zero-arg-getter-isolated-rejected-macos-arm64.json` under `bench/runtime-comparison/` for every gain/loss, provenance,
development attempt and qualification limit. No fresh Java/develop ratio is claimed.

Remaining work starts with complete already-resolved ordinary fallback and outlined
native getter invocation, then separate proof/check removal and a matched optimized
Java/develop checkpoint. Java parity
and a justified global performance ceiling remain open.


## Rejected zero-argument scalar getters with complete matched fallback (post-5a067c2a8)

Integer getter loop: 281.553 -> 131.036 ns/item (-53.459% time). Floating getter loop: 313.602 -> 137.777 ns/item (-56.066% time). The candidate is rejected; complete candidate sources/modes are archived and production is independently restored/requalified. Repeated adverse non-target layouts: boxed_float_method_call/default, boxed_float_method_call/reverse, monomorphic_call/default, string_result_loop/reverse, string_result_loop/shuffle-487.

All 5904 optimized windows across 41 workloads/three fixed layouts are preserved.
The candidate completes matched ordinary fallback and outlines getter invocation;
full live guards, synchronized reads, eager explicit context and reader-safe frames
remain. Shared PIC layout and unary ABI/witness are unchanged. No lock elision or
member-value caching. Release/Debug candidate each pass 69 core files / 112 cases /
17595 assertions, including 1053 getter assertions, plus lvalue/DGC, integration,
native/reference controls and exact 64-qmod alias restoration. Baseline/restored
qualification is separately fingerprinted. See RESULTS and `results-aot-zero-arg-getter-complete-rejected-macos-arm64.json` for
all gains/losses, provenance, diagnostics, limitations and the full 62-item audit.

Next test positive getter admission within already-matched ordinary dispatch,
then separate compile-time exclusion/proof checks or caller-native context deferral.
The context grew from 880 to 896 bytes but both use a 896-byte allocator bucket
on this host; metadata compaction is not selected on a bucket-growth hypothesis. A retained
change requires fresh matched optimized feature/develop/Java measurements. No fresh
Java ratio or global performance ceiling is established by this AOT experiment.


## Rejected first-use native scalar-call control caching (post-5a2017418)

Boxed integer method calls: 16.223 -> 16.748 ns/item (+3.237% time). Boxed floating method calls: 19.963 -> 20.946 ns/item (+4.927% time). The candidate is rejected; complete candidate sources/modes are archived and production is independently restored/requalified. Failed targets: screen/boxed_int_method_call/aggregate +3.237%, screen/boxed_int_method_call/default +3.375%, screen/boxed_int_method_call/reverse +3.282%, screen/boxed_int_method_call/shuffle-487 +2.861%, screen/boxed_float_method_call/aggregate +4.927%, screen/boxed_float_method_call/default +6.475%, screen/boxed_float_method_call/reverse +4.654%, screen/boxed_float_method_call/shuffle-487 +6.111%.

All 2214 optimized windows across 41 workloads/three fixed layouts are retained.
The candidate changes only five first-use control caches inside existing scalar
native method dispatch. Exact environment scope/presence and C++ one-time/retry
semantics remain; a typed atomic state avoids repeated hot guard/value loads.
Compiler/caller conventions, PIC/AOT-context layout, live call guards, observable
context, ownership and locking remain unchanged. Static state/outlined code do add
footprint; section sizes are diagnostics, not heap or latency measurements.

Baseline and candidate Release/Debug qualification includes merged DPQL coverage,
core/context/native checks, lvalue/DGC/integrations, references and 64 qmods. See
RESULTS and `results-aot-native-control-cache-rejected-macos-arm64.json` for all gains/losses, source/artifact provenance,
standalone concurrency tests, raw diagnostics and the full 62-item audit.
A retained improvement requires fresh matched optimized feature/develop/Java numbers.
Positive already-matched getter admission and proved-clean sink handling with unchanged
per-iteration cancellation checks remain
separate hypotheses. No Java parity or global performance ceiling is claimed.


## Retained zero-argument scalar getters through admitted ordinary dispatch (post-18880ba81)

Integer getter loop: 284.205 -> 168.731 ns/item (-40.631% time). Floating getter loop: 330.261 -> 175.215 ns/item (-46.947% time). The candidate passes both target epochs and the fixed broad-regression gate.

All 5904 optimized windows across 41 workloads and three fixed layouts are
preserved. Positive compiler-published zero-argument bindings are attempted inside
already-matched ordinary fast dispatch. No generated caller rewrite or shared PIC
growth; exact live guards, eager context, synchronized reads and reader-safe frames
remain. RuntimeConfig is passed within the invocation, but this path still acquires
it through the current runtime view. No lock elision or member-value cache.

Release/Debug qualification includes core/getter, merged DPQL/local HTTPS,
lvalue/DGC, native/reference and integration coverage, with 64 rebuilt qmods and
exact alias restoration. See RESULTS and `results-aot-zero-arg-getter-admitted-macos-arm64.json` for all gains/losses,
provenance, diagnostics, limitations and the complete audit. A retained improvement
needs fresh matched optimized feature/develop/Java measurements. No fresh Java ratio
or global performance ceiling is claimed here.


## Optimized post-getter checkpoint

Fresh matched feature `bdac4c284`, merged develop `12cbac529` and warmed Java/JMH:
55 Qore workloads, 37 existing Java microbenchmarks and six separately labelled
getter controls; all 5595 windows retained across three fixed Qore layouts and
five forks. Both runtimes are Release `-O3 -DNDEBUG` with qcc `-O3`.

See RESULTS and `results-aot-post-admitted-getter-checkpoint-macos-arm64.json`
for every call/container/mixed result, loss, qualification and audit. Getter
plain-Java, non-inlined and read-lock controls have explicit semantic limits;
ratios are whole-workload costs, not isolated call latency or parity evidence.
Next investigate reuse of the caller's already-acquired live RuntimeConfig in
ordinary dispatch, with separate correctness and before/after measurements.
No global performance ceiling is established.


## Rejected explicit RuntimeConfig for ordinary AOT dispatch (post-bf4b4ebdd)

Integer getter loop: 163.871324 -> 156.566027 ns/item (-4.457947% time).
Floating getter loop: 171.850485 -> 166.210816 ns/item (-3.281730% time).

Rejected by conservative manual review despite passing the original mechanical rule: repeated monomorphic and side-effecting method losses are not dismissed because ranges overlap. Candidate sources archived and production restored/requalified. All 7920 optimized windows and every loss are preserved across 55 workloads / three fixed layouts. This is matched branch before/after, not fresh Java/develop or isolated-call timing.

The experiment tested passing an explicit borrowed live view through ordinary AOT dispatch, retaining old ABIs, complete live guards, fallback snapshots, argument ownership, cancellation and observable frames. See RESULTS and `results-aot-ordinary-dispatch-runtime-config-rejected-macos-arm64.json` for all results, qualification, diagnostics and audit. Next separately investigate immutable signature proof at admission. Java parity and a global ceiling remain unestablished.


## Rejected immutable getter signature admission (post-e92fe83c3)

Integer getter loop: 164.550678 -> 160.241391 ns/item (-2.618820% time).

Floating getter loop: 166.518251 -> 165.396790 ns/item (-0.673476% time).

Rejected; candidate sources archived and the runtime/fixtures restored and requalified. All 2970 optimized measured windows and all losses are preserved across 55 workloads / three fixed layouts. This is matched branch before/after, not fresh Java/develop or isolated-call timing. Baseline binaries predate the intervening evidence-only commit; exact lineage and artifact hashes are recorded.

The experiment moved only immutable zero-argument signature facts into existing PIC admission. Caller lowering/ABI, positive live companion binding, body/readiness/Program guards, TLS acquisition, locks, observable frames and cancellation were unchanged. See RESULTS and `results-aot-getter-signature-admission-rejected-macos-arm64.json` for footprint, all results and the complete audit. Java parity and a global ceiling remain unestablished.

Next: investigate extending the existing guarded object-parameter cache through boxed PHIs used as method receivers, retaining all ownership and reflective invalidation checks. This is distinct from the rejected all-auto heuristic and is not yet implemented or measured.


## Rejected guarded conditional-receiver parameter cache (post-815eeb761)

Confirmation integer getter loop: before 160.142621, compile-disabled 158.885164, candidate 124.405986 ns/item (-22.315505% vs before, -21.700691% vs disabled).

Confirmation floating getter loop: before 165.721997, compile-disabled 166.835780, candidate 130.870006 ns/item (-21.030396% vs before, -21.557591% vs disabled).

Rejected; the three candidate source/fixture files are archived, restored and requalified. Monomorphic calls regress +5.312952% / +5.879396% against before in screen / confirmation. Monomorphic calls regress +4.582140% / +4.315208% against compile_disabled in screen / confirmation. Repeated disjoint adverse layout comparisons also occur for indirect calls and integer/floating monomorphic calls; all rejection gates and losses are preserved in RESULTS. All 11880 optimized windows across 55 workloads / three layouts / three modes are preserved. This is whole-workload branch before/after/control, not fresh Java/develop or isolated-call timing.

The rejected compiler candidate followed boxed PHIs used as receivers to enable existing guarded object-parameter reuse. Ownership, strong-slot identity, reflective invalidation, reference/weak fallback, dispatch, frames and locks remain unchanged. It was an AOT code-generation optimization requiring recompilation; no IR/JIT gain is claimed. See RESULTS and `results-aot-phi-receiver-param-cache-rejected-macos-arm64.json` for all results, qualification, diagnostic limits and audit. Java parity and a global ceiling remain unestablished.

Next independently investigate a cheap exact-`NOTHING` exit in the existing late AOT scalar cleanup guard, without changing real cleanup ordering or exception behavior. Inspect generated code before committing to another timing campaign; no benefit is established yet. Preserve the separate baseline IR reflective-frame limitation.


## Rejected exact-empty AOT cleanup exit (post-d7e14c3af)

Optimized screen integer monomorphic calls: 3.402371 -> 3.469285 ns/item (+1.966691% versus before; +3.006283% versus compile-disabled). Floating monomorphic calls: 3.443219 -> 3.305041 ns/item (-4.013055% / -1.214391%). Both fail the predeclared 5% target against both controls; not every fixed layout improves.

Rejected with no confirmation or rescue run. All 4455 windows across 55 workloads / three layouts / three modes are preserved. Sources are archived and restored; fresh selected Release/Debug builds pass 69 core files / 112 cases / 17595 assertions each plus lvalue/DGC, merged-module, thread, native and reference coverage, with 64 rebuilt qmods and exact Release-alias restoration. No runtime change is retained.

The candidate tested an exact-empty shortcut in late AOT cleanup guards, retaining all nonempty cleanup ordering and existing ABI/context/lock/cancellation contracts. Generated functions grew, and indirect float/integer-result calls also lost in the screen. See RESULTS and `results-aot-empty-cleanup-rejected-macos-arm64.json` for all data, baseline lineage, qualification, inspection erratum and audit. This is branch before/after/control, not fresh Java/develop or isolated-call timing; no Java parity or global ceiling is claimed.

Next: independently investigate fixed-hash scalar replacement for a finite PHI of literal keys, with all-key presence, dominance and inline-value lifetime proofs. Preserve existing missing/arbitrary-key and mutable/reflective/reference fallbacks. No implementation or measured benefit is established yet; the baseline IR reflective-frame discrepancy remains a separate correctness follow-up.


## Rejected finite literal-key hash projection (post-6a4e2ef92)

The optimized prototype reduces temporary-hash cost from 339.732199 to 40.988696 ns/item in the screen and from 338.724517 to 40.685528 in confirmation (about 88% less time, 8.3x faster). Both controls and every fixed layout show the target benefit. This is not a retained branch improvement: reverse-layout `indirect_affine` is +6.140863% / +10.222115% slower than compile-disabled in the two phases, with disjoint observed fork-mean ranges, triggering the predeclared rejection gate despite an essentially flat three-layout aggregate.

All 11880 optimized windows across 55 workloads / three layouts / three modes are preserved, with no changed threshold, dropped samples, epoch pooling or rescue run. The compiler change and two new fixtures are archived and restored. Fresh selected Release/Debug qualification passes 69 core files / 112 cases / 17595 assertions each plus lvalue/DGC, module, cancellation, native and reference checks; 64 qmods are rebuilt and their Release aliases restored. Candidate qualification passes 72 / 115 / 18401 each.

The rejected analysis selects inline integer values using a literal-key PHI while preserving selector strings. It keeps private-local/noescape, predecessor/dominance, inline-lifetime and exceptional-flow exclusions; no helper ABI, TLS, frame, lock, epoch or cancellation-poll change. Audit corrections avoid repeated key-set scans and exclude exceptional-flow functions. See RESULTS and `results-aot-finite-hash-key-rejected-macos-arm64.json` for all measurements, adverse layouts, qualification, development attempts and the complete audit. Static code comparisons do not prove the cause of the unrelated timing loss. No fresh Java/develop or IR/JIT timing, parity or global ceiling is claimed.

Next: independently remove only provably exclusive literal-selector graphs after successful hash projection, preserving observable/linked/shared strings, partial projections and ownership/cancellation behavior. This materially different candidate needs fresh validation and predeclared measurements. Do not retry the rejected version unchanged. The baseline IR reflective-frame discrepancy remains a separate correctness follow-up.

## Rejected finite hash selector elision (post-c80d44b03)

The exclusive-selector experiment is **rejected and not retained**. In the final measured epoch, `temporary_hash` used 340.723007 ns/item before and 1.623138 with the prototype (-99.524%); the projection-only control used 41.266645 ns/item. Repeated aggregate loss: `monomorphic_call` versus `compile_disabled` is +3.125223% / +3.548359% in screen / confirmation. There are also 3 repeated disjoint adverse layout comparisons. The full 55-workload results, all three controls and the fixed non-regression decision are recorded in `bench/runtime-comparison/results-aot-finite-key-selector-rejected-macos-arm64.json` and `bench/runtime-comparison/RESULTS.md`. These optimized AOT workload measurements are not isolated call latency or a fresh Java/develop comparison. The compiler/fixtures were recoverably archived and restored, and the restored baseline passed fresh Release/Debug qualification. No Java-parity or global-ceiling conclusion follows. Next investigate a distinct AOT-only proof of redundant exception-sink checks on already-clear fast cancellation paths, preserving all polls, fallible-operation checks, thread exit and cleanup; only an ignored, uncompiled recognizer draft exists so far.

## Rejected proved-clear AOT exception-sink checks (post-02ce653ed)

Rejected and not retained: direct-call workload time is 1.552219 -> 1.555521
ns/item (+0.212729% versus before; +0.713660% versus compile-disabled). The
predeclared 5% target fails, so no confirmation or rescue run follows. All 4455
optimized windows across 55 workloads / three fixed layouts / both controls
are retained in `bench/runtime-comparison/results-aot-clear-sink-rejected-macos-arm64.json` and RESULTS.md.

The AOT-only proof removed redundant sink-check instructions on already-clear
edges while retaining every cancellation poll and unknown-effect/entry/cleanup
check. Actual direct fast-loop instruction count falls from 18 to 12, but no
throughput benefit is observed. Method-call code remains unchanged. This is not
an isolated-call or fresh Java/develop comparison, or a global-ceiling result.
All candidate source/fixtures were archived recoverably and restored. Fresh
selected Release/Debug qualification passes 70 files / 117 cases / 17609
assertions each, plus module/lvalue/cancellation/native/reference coverage and
79 rebuilt qmods with exact Release-alias restoration.

Next investigate a guarded indexed-list integer compound update with live slot,
metadata, uniqueness and bounds checks plus the unchanged generic fallback.
This is a distinct unimplemented/unmeasured hypothesis, not permission to omit
COW, references, locks, metadata, reflection, exceptions or cancellation.

## Rejected indexed integer update and two-member scalar getter (post-cee5710b3)

Both candidates are rejected and not retained. Optimized confirmation
whole-workload ns/item:

|!Candidate / target|!Before|!Candidate|!Compiler selector off|!Change vs before
|Indexed add / list_mutate|191.558033|154.734074|191.314456|-19.223%
|Two-member getter / two_read_loop|384.420212|200.881701|381.809141|-47.744%
|Two-member getter / two_read_short|1302.320531|1083.466556|1298.677836|-16.805%

All target checks passed, but fixed non-regression gates rejected repeated
disjoint adverse layout comparisons. Indexed add regressed effectful-method
reverse/shuffle487 and two-read-getter reverse layouts; the getter regressed
captured-closure default layout (+3.408% screen, +4.148% confirmation). Neither
had a repeated aggregate warning. Every workload/layout/control and all 23760
windows are retained in RESULTS.md's linked
`results-aot-indexed-add-and-multi-getter-rejected-macos-arm64.json`. No rescue
run or threshold change followed. The cause is not isolated; unchanged
non-target LLVM bodies do not prove runtime-layout causality.

Both prototypes and fixtures were recoverably archived and removed. Fresh
restored Release/Debug qualification each passed 70 core fixtures / 117 cases /
17609 assertions, plus upstream/module/lvalue/cancellation/reference coverage,
1985 native Debug assertions in each of two modes, and exact restoration of
all 79 owned Release qmod aliases. Failed test assumptions, pre-existing
weak-root AST/compiled behavior, the original indexed archive/restore failure
and its successful recovery remain recorded. Independent replay validates raw
windows, decisions, qualification logs and source/artifact lineage. These are
not fresh Java/develop ratios, isolated-call latencies or a global-ceiling
result; no new runtime performance gain is retained by this checkpoint.

First merge fetched develop `3f0019a6a` and qualify a fresh optimized baseline;
the experiments above precede those upstream runtime/compiler changes. Then
inspect and serially profile captured-closure binding. The native helper
still looks up capture storage before each synchronized live integer read;
investigate removing lookup work only with a complete invocation/context and
lifetime proof. Do not cache captured values, drop locking, extend closure
ownership, or trust reused addresses. This is an unimplemented/unmeasured
hypothesis, distinct from both rejected candidates.

## Rejected first-capture lookup (post-5e73080ca)

Develop `3f0019a6a` is merged and the exact merged tree passed selected full
Release/Debug qualification. A fresh optimized AOT screen then tested an exact
first-capture identity check before ordered lookup, preserving normal capture
locking and all fallback/admission rules.

The shortcut is rejected and removed: captured-closure time improved only
1.934% versus the frozen merged baseline and 1.361% versus a compile-disabled
control on the same candidate runtime, below the predeclared 3% target.
Default-layout times were 23.128278 / 22.761392 / 22.688409 ns/item respectively.
All 189 runs / 567 measured windows, three layouts and two multiple-capture
probes are preserved in RESULTS.md and
`results-aot-capture-first-entry-rejected-macos-arm64.json`. This is not a
retained speedup or a new Java/develop comparison.

All 18 executables passed 5265 reference checks. Candidate and freshly restored
Release each passed 15 focused fixtures / 40 QUnit cases, respectively
702 / 642 assertions, with no skips, plus three exit-status fixtures. All six
experimental source files match the merged baseline again. Existing Debug
artifacts are unchanged; no fresh candidate Debug or broader retention gate is
claimed. Two outer reporting/equivalence failures and fresh successful retries
remain distinct in the evidence.

Historical qualification case totals include explicitly skipped cases despite
QUnit's misleading summary. In the merged core suite, 117 registered means
109 passed + eight Linux-only skips; in Release's lvalue suite, 84 registered
means 81 passed + three Debug-only skips. Debug executes all 84 lvalue cases.
Use explicit skip ledgers; this does not change timing values.

Next investigate duplicate callback-tag/enable decoding on the already-proven
closure miss route, starting with machine-code inspection. An internal
known-closure continuation could reuse an immediate checked identity without
retaining a closure or caching its values. All live guards and current signature
checks must remain. This distinct hypothesis is unimplemented and unmeasured;
do not retry the rejected signature proof or map shortcut unchanged. Java parity
and a global performance ceiling remain open.


## Known-closure AOT dispatch validated (post-27cca0da3)

Develop `26f5581d5` is merged. The additive v5 reusable `int(int)` callback
route checks a live closure tag once and avoids the generic prepared-call
frame before unchanged native admission. It retains all live guards, locked
capture reads, ownership/fallback rules and v3/v4 ABIs. `RuntimeConfig` is
passed explicitly; no new per-call TLS acquisition is introduced. The AOT-only
compiler selector can be disabled with
`QORE_DISABLE_AOT_KNOWN_CLOSURE_ROUTE=1`; other signatures/non-reusable sites,
IR interpretation and JIT lowering are unchanged.

All four predeclared targeted/broad gates pass. Broad confirmation reduces
plain closure time by 13.962% versus baseline and 14.554% versus the
same-runtime compiler-disabled control; captured closures improve by 11.013%
and 10.846%. Default-layout baseline/candidate times are 17.059028/14.801069
and 22.938591/20.592098 ns/item. Ordinary indirect calls remain modestly slower:
their broad aggregate is +3.787%/+1.705% versus the same-runtime control in
screen/confirmation. No adverse >=3% gate repeats in both broad epochs, but
this is not zero-regression or general-application-speedup evidence.

The report preserves 4464 runs / 13392 measured windows, three fixed layouts,
both controls and every adverse result. All timings use optimized Release
runtimes and `qcc -O3`; none are Debug timings. The 55-workload broad pair,
7047 broad and 5265 targeted reference checks, independent replay and selected
full Release/Debug qualification are complete. Core counts per runtime are
109 passes + eight platform skips / 18021 assertions. Debug additionally
executes the Release-skipped GC/native-accessor cases. All 125 owned Release
qmod aliases are restored. Detailed skip, interrupted/failed attempt and
on-host coverage limits are in RESULTS.md and
`results-aot-known-closure-route-macos-arm64.json`.

Older AOT artifacts need recompilation to select v5. This checkpoint is not a
fresh Java/develop comparison and does not establish parity or a global
performance ceiling. Next synchronize upstream and refresh the matched
optimized branch/develop/Java view, then profile remaining call/collection
costs. Do not remove capture synchronization or revive previously rejected
signature/lookup shortcuts without a distinct proof and new fixed measurement
plan.


## Optimized post-known-closure checkpoint (2026-10-21)

Fresh optimized feature `0beee48a1`, develop `26f5581d5` and Temurin 25.0.2+10
comparison retains all 1865 jobs / 5595 windows. Qore Release `-O3 -DNDEBUG`,
own `qcc -O3`, three fixed layouts and five forks; Java warmed JMH 1.37.
Whole-workload ns/item: direct 1.554 versus Java 1.030; indirect 6.910 versus
1.721; polymorphic 22.955 versus 2.663; captured closure 21.525 versus 1.799.
Mixed processing is 1.13x to 3.63x faster than develop. List mutation remains
about 46x Java time; temporary hashes about 21x and 3.791% slower than develop.
All results, including adverse layouts and semantic limitations, are in RESULTS
and `results-aot-post-known-closure-checkpoint-macos-arm64.json`.

These are cumulative branch results, not isolated call latencies, production
throughput or cross-Program measurements. No Java parity or global maximum is
established. Prior runtime qualification is reused by exact source/artifact
identity; fresh reference and measurement checks are separately recorded.
Next profile-guided call/collection changes require distinct before/after/control
experiments and broad regression qualification; old rejected changes remain rejected.


## Rejected loop-only scalar cleanup (post-b5652e77c)

Optimized AOT list mutation improved from a geometric layout mean of
196.159 to 187.359 ns/item: 4.486% versus before and 4.496% versus the
compiler-disabled control. All 297 runs / 891 windows are retained. This
misses the fixed 5% target gate, so no confirmation retry was run and the
prototype was archived and removed. No new runtime improvement is retained.

Candidate focused Release qualification passes 15 fixtures / 21 QUnit cases /
6054 assertions; restored focused Release passes 14 / 20 / 5974. Each fixture
total includes two exit-status fixtures; there are no skips. The original harness
accounting failure and its fresh successful rerun are preserved. These are
not full Release/Debug reruns or fresh Java/develop comparisons. All raw
results, adverse layouts, controls and audit are in RESULTS.md and
`results-aot-lvalue-loop-cleanup-rejected-macos-arm64.json`.

Next investigate prepared-root refresh/publication after compound mutation,
using the already-admitted invocation-local slot with unchanged live reference
evaluation, cleanup order and generic fallback. The current profile shows
repeated generic root lookup; no gain is assumed. Neither Java parity nor a
global ceiling has been established.

## Rejected prepared compound-root refresh (post-a22f87615)

An existing prepared AOT local slot can eliminate repeated root lookup while
preserving evaluation and cleanup, but this prototype failed its fixed 2% broad
performance gate. Targeted screen/confirmation improve list mutation by
2.171%/2.333% versus before; the broad screen improves only 1.598% versus before
and 0.976% versus the disabled control (192.622/191.414 -> 189.545 ns/item).
All 2277 runs / 6831 windows and adverse comparisons are retained. No broad
confirmation retry or new runtime optimization is retained.

Candidate focused Release: 16 fixtures / 22 QUnit cases / 9640 assertions;
restored: 15 / 21 / 9422. Each fixture count includes two silent fixtures.
Debug native assertions are 2068 per setting for the candidate and 2024 for
restored HEAD. Indexed compound/weak references pass, and 125 Release module
aliases are restored. The two original harness failures remain documented.
These are focused gates, not full candidate retention qualification.

Next examine first-public-base identity acceptance in the pinned receiver store,
retaining ordinary fallback, private-access ordering, raw/parameterized checks,
live slot state and real-reference ownership. No new call optimization has been
implemented or measured yet. See RESULTS.md and
`results-aot-prepared-lvalue-refresh-rejected-macos-arm64.json`. Neither Java parity nor a global ceiling is established.

## Rejected first-public-base acceptance (post-99e3268cb)

The AOT pinned receiver prototype passes all four fixed 5% target gates. Broad
confirmation reduces integer polymorphic-call time from 22.789 to 20.044 ns/item
(12.048% versus before, 11.996% versus disabled), and float polymorphic calls
from 22.816 to 20.909 (8.359%/9.652%). However, list building is 3.203%/3.010%
slower than before in broad screen/confirmation, failing the repeated >=3%
aggregate regression gate. The candidate is archived and removed, not retained.

All 4896 runs / 14688 windows and adverse observations are preserved. New v4
only substitutes the existing checked receiver initializer; ordinary private
lookup ordering, raw/parameterized checks, live slot state and real ownership
remain. The rebuilt runtime also changes v3 inlining despite unchanged v3 source.
List-building time is nearly identical with the new compiler selector disabled,
so runtime compilation/layout is the next diagnostic lead, not yet a proven cause.

Candidate focused Release: eight fixtures/cases, 9816 assertions, no skips;
Debug: 2119 per setting. Restored: eight/9634 and Debug 2024 per setting. The first
native fixture failure (unset structural hashes) is retained with its correction.
Indexed compound/weak-reference supplements pass and 125 Release aliases are
restored. No full candidate retention qualification or fresh Java/develop ratio.

Next diagnose frozen list-building code/profile differences and consider an
isolated translation unit for a changed implementation; do not retry the same
binary or waive the fixed regression gate. See RESULTS.md and
`results-aot-first-public-base-rejected-macos-arm64.json`. Neither Java parity nor a global ceiling is established.

## Isolated first-public-base helper rejected (post-65c5a5400)

Separate-TU placement preserved addresses and normalized instructions of nine
inspected list hot helpers and existing class initializers. List building is flat
in the early screen: 27.179 -> 27.130 ns/item. But integer polymorphic calls improve
only 22.517 -> 22.220 ns/item (1.320% versus before, 3.117% versus disabled), below
the fixed 5% gate; reverse layout is 1.592% slower. All 378 runs / 1134 windows are
archived. The prototype was removed without an unchanged retry or later stages.

Untimed sampling and ARM64 show three new out-of-line operations in the isolated
initializer: sink test, generic node ref and real-reference bookkeeping. This is
the next lead, while preserving custom handlers, tracing, atomics and publication
order. It is not yet a measured causal attribution. Runtime-slot cleanup and
dynamic dispatch remain substantial costs. No Java parity or global ceiling.

Corrected a harness mistake before timing: the native-test entry is Debug-only;
the entire ql_debug.cpp was already excluded from Release. The initial failed
Release-native invocation and original driver are retained. Fresh candidate
focused qualification passed 9816 Release assertions and 2119 native assertions
per setting; restored source passes 9634 and 2024, plus indexed compound/61 weak
references. All 125 Release aliases restored; no full selected retention rerun.
See RESULTS.md and `results-aot-isolated-public-base-rejected-macos-arm64.json` for the complete evidence and audit.

## Shared-inline AOT class initializer (post-a62f36ab8)

The retained v4 pinned-class initializer uses a separate runtime translation
unit and shared internal reference/real-reference primitives. Exact self or the
first direct public base can bypass ordinary ordered lookup; all other identity,
access and raw/parameterized cases retain live ordinary acceptance. No decision
is cached. The slot's actual owner, live tracksRealRefs/closure_lent rule and
publication order remain intact; Debug uses the traced wrappers, Release inlines
their shared primitive. Pending exception and thread exit are both checked.
The compiler-only QORE_DISABLE_AOT_FIRST_PUBLIC_BASE_ACCEPT selector retains v3.
Old helper ABIs and runtime admission controls remain available.

Four fixed target gates and the 55-workload repeated-regression gate pass.
Broad confirmation: integer polymorphic 22.304733 -> 19.868732 ns/item (10.921%
lower versus before, 10.817% versus disabled); floating polymorphic 22.502134 ->
21.105132 (6.208%/5.926%). All 4968 runs / 14904 windows, including nonrepeating
slowdowns, are preserved. Direct/simple method calls are essentially unchanged.
These are whole optimized AOT workloads, not isolated call latency or fresh
Java/develop comparisons. Selected Release/Debug integration and native ownership
tests pass; named unavailable checks are excluded and 125 Release aliases restored.
No full-suite/sanitizer, Java parity or global-ceiling claim.
See RESULTS.md and `results-aot-inline-public-base-macos-arm64.json` for measurements, qualification and all 62 audit items.


## Optimized post-merge/package-update checkpoint (2026-10-22)

Fresh isolated Release `-O3 -DNDEBUG` builds compare feature `5b3fdbe22` and
develop `d704a0126`, each using `qcc -O3`, against fresh Temurin 25.0.2+10/JMH1.37.
All 1865 runs / 5595 windows and three fixed layouts pass independent replay.
Whole-workload ns/item: direct 1.546 versus Java 1.008; method 1.607 versus 0.997;
indirect 6.897 versus 1.706; polymorphic 22.186 versus 2.611; plain closure 16.710
versus 1.704; captured closure 22.555 versus 1.781. These are same-Program
workloads, not isolated call latencies or cross-Program measurements.

Mixed processing is 1.14x to 3.58x faster than develop. List mutation remains
about 46x Java time; temporary hashes about 21x and 1.954% slower than develop.
All adverse layouts and separately labelled getter controls are retained. Neither
Java parity, production throughput nor a global maximum is established.

The interrupted pre-update campaign is fully excluded. Fresh dependency hashes
and per-process identity checks remained stable; all child exits are recorded
before result parsing. Fresh references pass; earlier selected Release/Debug
merge qualification is historical source evidence, not identical-artifact reuse
after the rebuild. No runtime change is retained in this evidence checkpoint.
See RESULTS and `results-aot-post-packages-checkpoint-macos-arm64.json` for all
numbers, raw evidence, profiling, limitations and all62 audit items. Next pursue
a distinct measured dispatch specialization while preserving live checks and
observable frame/ownership/cancellation semantics; rejected shortcuts stay rejected.

## Native-call cleanup experiments rejected (2026-10-23)

No production change is retained from late native-call cleanup specialization.
Separate optimized screens contain432 runs /1296 windows each, three layouts and
before/candidate/compiler-disabled controls. A coercion capture annotation improves
the six-target geometric mean2.201% versus before but only0.516% versus disabled;
bounded PHI-edge cleanup predicates improve1.855% and0.928%. Both miss the declared
3% aggregate /5% indirect target gate. Floating indirect calls and boxed-float
result controls are adverse. Temporary hashes raise screen flags; no independent
confirmation or broad regression run was warranted. No new Java comparison.

Unannotated jump threading, speculative error-edge predicates and a later
wrapper-identity runtime probe were rejected before timing. The latter's two
removed compares cause extra stack-guard calls/register saves and an80-to96-byte
floating helper frame on this compiler. Neither fewer static instructions nor a
cleaner native-success path establishes a throughput benefit.

Final bounded-edge correctness checks pass35 cases /3137 assertions; all candidate
preparations pass4995 references each. Both timed campaigns pass independent raw
replay. After removing every experimental source/test edit, the unchanged optimized
checkpoint runtime passes35 cases /2895 assertions;125 module aliases are unchanged.
These are selected Release checks, not full-suite/Debug/cross-platform qualification.
See RESULTS and `results-aot-native-cleanup-rejected-macos-arm64.json` for the complete
measurement, failure, restoration and62-item audit evidence.

Next specify and inspect compiler-visible prepared dispatch as a distinct
structural experiment: invocation-owned metadata, live debugger/body/readiness
checks, compatible versioned entry and complete stack/frame/reader/fallback
semantics. The aim is to remove a C++ dispatch frame, not change call observability.
The inspected reusable path already passes RuntimeConfig and has no TLS lookup.
This future design is unimplemented; Java parity and a global ceiling remain
unestablished. Keep the rejected cleanup and guard-shape experiments rejected.

## Rejected reusable callback inline pop (2026-10-23)

A compiler-selected v6 prototype inlined the full per-call reader handshake
for reusable scalar callbacks, retaining the existing scratch/release ABI and
all guards. It removed an outlined pop call without growing helper/caller
stack or register saves, but the optimized three-layout screen was 3.300%
slower across four targets versus before and 2.996% slower versus the disabled
control. It failed the fixed benefit gate and was removed; v4/v5 remain.
The integer direct-result case improved while its floating counterpart
regressed about 25%, so no universal benefit is inferred from instruction
counts. Candidate and restored selected Release checks pass; full details,
limitations and the 62-item retained-evidence audit are in
`bench/runtime-comparison/results-aot-reusable-pop-rejected-macos-arm64.json`.
A separate result-initialization compile probe is next, not a retained change
or speedup. Java parity and a global performance ceiling remain unestablished.

## Develop integration through 8c4350a87 (2026-10-23)

The restored performance branch includes develop through `8c4350a87`. A native
socket fixture needed a setup correction: keep its dual-stack reservation while
binding both fixture sockets natively, then clear temporary port reuse before
the unchanged wrapper listen-order assertions. No production socket behavior
or new call optimization changes. Final selected Release/Debug qualification
each passes 42 of 43 fixtures; the known cross-process SocketSpecificBind case
remains failed. There are 2220 passing IR/AOT reference comparisons and 11744
passing Debug native assertions. Optional service cases remain skipped, and
reflection-module provenance is verified in both builds. All 252 qmod targets
are rebuilt per build; 253 source aliases are restored to Release.

RESULTS and `results-merge-8c4350a87-macos-arm64.json` preserve failures,
final validation and the complete 62-item audit. Next inspect explicit cold
result defaults without a per-success flag or branch. This is a compile-only
hypothesis; no performance benefit or Java parity is claimed.

## Rejected reusable callback stack-address experiment (2026-10-23)

After merged baseline `86934cf2e` / develop `8c4350a87`, additive v7 helpers
reused the frame's existing thread pointer for the live stack-limit address.
One hot load was removed without changing guards, frame sizes or caller
instruction counts. The optimized three-layout screen improves only 0.921%
versus before and 1.940% versus disabled across four callback targets, below
the fixed 3% requirement. The candidate is removed; v4/v5 remain. All source
bytes are restored and fresh selected Release tests/references pass.

Two result-default compile-only probes also failed their code gates because
they added hot branches. No probe establishes a speedup or a global ceiling.
Full evidence and the 62-item retained-evidence audit are in
`bench/runtime-comparison/results-aot-reusable-stack-address-rejected-macos-arm64.json`.
Next inspect a private-helper owner-match proof without changing reader
synchronization. Deferred frame retirement still needs a complete proof of
ancestor/location/cache ownership and all generic/error cleanup paths.


## Develop integration through 28a6bbcb14 (2026-10-23)

The branch integrates develop through `28a6bbcb14`. A reproduced inherited
DBI typed-cache crash is fixed by abandoning the parent cache without walking
it, including for closed connections; fresh child caches retain normal
ownership. Native tests cover close, lookup and destruction. Long date suffix
scans gain cancellation checks and boundary/cancellation regressions.

The rebuilt Release and Debug runtimes each pass 54 of 55 selected fixtures;
the known macOS cross-process socket case remains failed. Optional services
and platform cases remain skipped. Additional checks pass 2220 IR/AOT reference
comparisons and 11,768 Debug native assertions. All 252 qmod targets are rebuilt
per configuration, with 253 aliases restored to Release. RESULTS and
`bench/runtime-comparison/results-merge-28a6bbcb14-macos-arm64.json` preserve
raw evidence, limitations and the 62-item audit. No call-path speedup is retained
by this merge, and no new Java/develop timing was run.

The private owner-proof compile probe is declined untimed because removing
comparisons caused stack-check outlining, more register saves and a larger
floating unary frame. Next test a scoped inlining hint; require favorable
machine code before semantic qualification and optimized before/candidate/
disabled measurements. Separately, invocation-invariant LLVM callback arguments
may support a stronger cache-owner proof. That requires a distinct helper,
complete cache lifetime proof and actual path verification. Reader synchronization
remains unchanged; Java parity and a global ceiling remain unestablished.

## Rejected general callback owner proof (2026-10-23)

The qualified merged baseline is `1d6f7c8d03` / develop `28a6bbcb14`. An
additive v8 owner-proof/cold-init experiment improves the optimized four-target
screen 4.009% versus before and 4.902% versus disabled, but ordinary integer/float
callbacks miss its separate 5% criterion. Native-result cases improve 5.6–7.8%
versus before in this screen. The combined gate fails; no confirmation/broad 55
was run, and all 12 source/fixture changes are removed. Restored Release core,
all 252 qmods, selected fixtures and references are requalified.

RESULTS and `bench/runtime-comparison/results-aot-reusable-owner-proof-rejected-macos-arm64.json`
preserve all probes, measurements, limits and the 62-item retained audit.
Next prove invocation-invariant compiler operands and cache ownership before
removing the wrapper check. Mutable guards and reader synchronization remain.
A native-result-only candidate needs its own prospective validation; no parity
or global ceiling is established.

## Compiler-proven invariant callback identity (2026-10-24)

A late AOT pass selects additive v9 prepared callbacks only after checking
every descriptor address use, identical producer Argument/signature, and
dominating empty initialization. The wrapper tests only nonempty ownership.
All live mutable guards, physical stack checks, reader synchronization,
fallback and ownership remain; old helper and scratch/release ABIs remain.
The full physical guard is inlined inside the new private helper, with cold
frame initialization. Recompile AOT artifacts to use the proof; IR/JIT
selection is unchanged.

Fixed targeted screening and confirmation gates pass; the 55-workload broad
confirmation has no repeated regression flags. Integer callbacks improve
6.7%, floating callbacks 3.9% and native integer results 5.7% versus before
in broad confirmation. Native floating results are 2.7% slower versus before
(1.1% faster versus disabled); mixed workloads are essentially unchanged.
No universal improvement, fresh Java/develop ratio, parity or ceiling claim.

Selected Release/Debug validation and the full 62-item audit are recorded in
RESULTS and `bench/runtime-comparison/results-aot-invariant-callback-macos-arm64.json`,
including known platform failure/skips and the repaired native fixture list.
The correction expands 29 to 30 native cases on both unchanged runtimes.

Next investigate callable-input subset acceptance for literal references
passed to internal calls. Typed locals match the destination accepted type
set; literals already belong to a subset accepted unchanged by ordinary
binding. Preserve binding semantics without inferring stronger callable
signatures, and reuse existing exact-callref specialization where applicable.
Treat this as a new hypothesis with separate before/disabled measurements.

## Develop integration through b7e1367f5f (2026-10-24)

Eager locations replace lazy PC recovery. Fast contexts retain exact native-body
identity and caller location restoration; receiver forwarding uses an immutable
image-owned descriptor. Receiver/closure entry-slot setup takes RuntimeConfig
explicitly. Live Program/debugger/stack/ownership and reader guards remain.

This integration changes the native QoreAOTFunc layout by adding body location.
Rebuild all native artifacts with the matching runtime. Earlier experiment
statements about unchanged layouts or compatible artifacts are historical and
do not apply across this integration. Binary format 22 preserves develop 20/21
meanings; experimental performance-branch20/21 payloads require recompilation.

Compiler local-slot insertion now uses a cached bound with one linear seed
refresh, avoiding repeated scans. Optimized isolated measurements and selected
Release/Debug qualification, including a later compiler error-lifetime fix and
its separate focused checks, are in RESULTS and
`bench/runtime-comparison/results-merge-b7e1367f5f-macos-arm64.json`.
The known socket-bind platform failure and Linux packaging limitation remain
explicit. No runtime-speed gain is inferred from slot assignment timings.

Next run the planned matched optimized feature/develop/premerge/Java checkpoint,
then measure the callable-input subset candidate. Java parity and a global
performance ceiling remain unestablished.

## Optimized execution after b7e1367f5f integration (2026-10-24)

A fresh checkpoint compares feature 0f59428d8, develop b7e1367f5f, frozen
premerge 23de678115 and Java/JMH, using optimized runtimes. Direct/method
workloads measure 1.453/1.549 ns per item versus Java 0.894/0.886. Integer
callbacks measure 7.449 ns versus develop 218.185 and Java 1.667; captured
closures 22.535 versus 155.872/1.758. List mutation remains 179.638 versus
Java 4.078. Mixed processing improves 1.12x–3.51x versus develop. These
whole-workload numbers include loop/computation, not isolated call latency.

The merge raises several callback workloads by 20–31% against the frozen
premerge branch. Investigate the additional cost before the callable-input
subset experiment; retain eager location/frame correctness. All 980 runs,
2940 windows, code-layout/fork ranges, adverse outcomes and independent replay
are recorded in RESULTS and
`bench/runtime-comparison/results-aot-post-b7-checkpoint-macos-arm64.json`.
No Java parity, universal speedup or global ceiling is established.

## Rejected native location snapshots (2026-10-24)

Per-call location snapshots reduce generated reloads, but the optimized fixed
screen improves the four-target geometric mean only 2.75% versus baseline and
2.33% versus a compile-disabled control. Native integer-result time increases
6.80%, with adverse ranges in all three layouts. The 5% acceptance gate fails;
the entire production change is restored and Release requalified. No broad or
confirmation campaign is claimed. RESULTS and
`bench/runtime-comparison/results-aot-native-location-snapshot-rejected-macos-arm64.json`
retain all 108 runs, 324 windows, code checks, patch and restoration evidence.

Resume the narrowly scoped callable-input subset proof for literal references.
This negative experiment establishes neither Java parity nor a global ceiling.

## Rejected callable-input subset proof (2026-10-24)

Compile-time input-kind subset acceptance selects existing fast entries for
literal callback arguments. Four targeted loops improve about 64% in both
optimized screen and confirmation. An existing typed-local floating-result
control nevertheless regresses about 6.6%, with repeated disjoint-range flags.
The prospective regression gate fails; compiler and fixtures are removed and
the baseline Release core/all 252 qmods requalified. No production gain retained.

RESULTS and `bench/runtime-comparison/results-aot-callable-input-subset-rejected-macos-arm64.json`
retain all 576 runs / 1,728 windows, source and code-path evidence, earlier harness
failures, baseline restoration and the 62-item audit. Identical control
instructions/paths do not prove the slowdown's cause. No alignment search,
broad 55 qualification, new Java timing or ceiling claim.

Next investigate the missing fast entry for floating accumulation followed
by an integer return cast, then test any structural fix as a separate candidate.

## Native private floating accumulation (2026-10-24)

Exact assigned private float locals with proven native-float RHS values now
lower += to existing AddAssignFloat. Three AOT loops gain fast entries and
improve 11–57x; direct-call RHS proof remains insufficient and falls back.
RHS ordering, shared/reference atomicity, optional/unassigned behavior and
runtime call guards are preserved. The lowering also applies to IR/JIT.

The targeted no-regression gate failed; the user explicitly chose broader
tradeoff evaluation. A fixed 55-workload screen/confirmation found essentially
flat mixed workloads and a repeated closure/reverse-layout +6.4% regression.
Two targeted result-call controls lose about 0.21–0.26 ns/item. The change is
retained under that explicit tradeoff, not a claim that either no-regression
check passed. All adverse measurements remain in RESULTS and
`bench/runtime-comparison/results-aot-float-local-add-macos-arm64.json`.

Release/Debug each pass 9,793 QUnit assertions plus five standalone fixtures;
eight Linux-only RSS cases per build are skipped. Native suites pass 11,756
assertions and IR/AOT references total 2,220. All 252 qmods/build match their
runtime and 253 Release aliases are restored. Next investigate exact
float-returning call facts, then remaining callback/closure and container costs.
No fresh Java/develop ratio, parity or global ceiling is established.

## Resolved required-float call accumulation (2026-10-24)

Same-Program required-float user-call results now admit existing native local +=
after successful return validation. This is a consumer-only proof: no ABI,
global ownership facts or call checks change. Expression-local definite
assignment is required; optional, runtime-overloaded, explicit-generic,
cross-Program and shared/reference/capture cases retain the existing path.

Optimized direct-float accumulation improves about 75x. A fresh matched
comparison measures Qore AOT 0.796 versus Java 0.640 ns/item,
or 1.24x Java's time, with ordinary inlining permitted. These are whole-loop
times, not isolated call latency. A targeted callback-result control repeats
a 4.9% slowdown; wide-callback flags occur in different layouts across phases.
Four mixed workloads are essentially flat. Retained under the user's explicit
overall-tradeoff preference; all adverse data and original wrapper errors remain.

RESULTS and `bench/runtime-comparison/results-aot-float-direct-add-macos-arm64.json` retain
4,596 timing runs, 13,788 windows, fresh Release/Debug 40-fixture qualification,
391 qmods/build, 2,220 references and the complete audit. Java parity and a
global ceiling remain unestablished. Next inspect repeated fast-loop diagnostic
stores and cancellation/exception bookkeeping, then method consumers/callbacks.

## Static-method float accumulation (2026-10-24)

The same-Program required-float consumer proof now covers resolved user static
methods with nongeneric receivers and no explicit method type arguments or
runtime overload dispatch. Expression-local assignment and existing private
local checks remain; no return validation, ABI or ownership changes.

The static-float accumulation checkpoint improves 74.13x:
Qore AOT 0.763 versus Java 0.574 ns/item, or
1.328x Java's time with ordinary inlining allowed.
The baseline already inlined the method; the gain removes generic += mutation
and synchronization. These are whole-loop times, not isolated call latency.
RESULTS and `results-aot-float-static-add-macos-arm64.json` preserve all controls and adverse results,
996 timing processes, 2,988 measured windows and the complete 62-check audit.

Release/Debug each pass 27 selected fixtures and 7,756 QUnit assertions, with
391 qmods/build, 2,220 IR/AOT references and separate native-suite checks.
Eight Linux RSS cases/build remain skipped. No global ceiling or general Java
parity is claimed. Next examine loop diagnostics/cancellation and unnecessary
native scalar return boxing, preserving exceptional and cancellation behavior.


## AOT native scalar snapshots (2026-10-24)

Physical native double/bool RefSelf values now retain their evaluated SSA value
across cleanup, removing redundant boxing and reference-count operations from
native returns. Boxed boundaries retain ownership; integer handling, declared
return validation, call admission and cancellation are unchanged. The compile
control is QORE_DISABLE_AOT_NATIVE_REFSELF. This change applies to AOT only.

Retained: the four target workloads improve in both phases against both
comparators, with a 24.97% confirmation geometric-mean time reduction versus
before. Direct local/loop returns improve 48.33%/33.75%; callback local/loop
returns improve 3.60%/3.98%. No target confirmation disjoint-range regression
flag remains, and mixed confirmation workloads stay within 0.7% of baseline.
The broad screen has no disjoint-range flags, but four aggregates exceed +3%
against at least one comparator: indirect_call, monomorphic_call, temporary_hash
and object_short. A separate five-fork confirmation of all four, with arithmetic
and callback_pipeline controls, has no >3% aggregate loss or disjoint-range
flag. The four are 0.94–1.83% slower than before in that follow-up. All original
screening losses remain recorded; this is an overall tradeoff decision, not a
claim that every control is faster. All broad generated function bodies match
after the documented normalization; that does not establish the cause of
observed differences. Callback samples still execute generic callee entries,
which is the next optimization target. No general Java parity is established.

RESULTS and `results-aot-native-refself-macos-arm64.json` retain the target screen/confirmation and fresh broad
55-workload screen, including every adverse result. Release/Debug each pass
38 selected fixtures, 18,330 QUnit assertions and 391 qmod builds. Eight Linux
RSS checks per build remain skipped; native Debug and reference suites pass.
No new Java/develop ratio, global ceiling or general Java parity is claimed.


### Develop integration checkpoint (2026-10-24)

The performance branch integrates develop `7e7de64fbd` after the native scalar
snapshot optimization `4e5be43486`. The merge has no conflicts. Release and Debug
all-target builds each rebuild 391 qmods and pass 45 selected fixtures; the
combined checks include 2,220 IR/AOT reference results and 11,756 native
assertions. Release records 10,941 QUnit assertions with 26 skipped cases;
Debug records 10,979 with 17 skips. Three fixtures affected by audit corrections
pass again after rebuilding their affected modules; core runtime hashes stay
unchanged. The schema digest reference passes unchanged. Details and all 62
checklist outcomes are in `bench/runtime-comparison/results-merge-7e7de.json`
and `bench/runtime-comparison/audit-merge-7e7de.md`.

No new performance ratio is inferred from this merge. Earlier measurements
remain labeled with their own baselines. Next extend bounded scalar-body proof
for initialized private float locals in straight-line functions, to admit the
existing native callback entry and direct expression inlining. Loops, reference
or captured locals, parameter mutation, handlers and unknown effects remain
outside this first extension. Freeze the merged optimized baseline and compare
fresh before/candidate/compile-disabled artifacts. General Java parity and a
global performance ceiling remain unestablished.


## Float expressions through private locals (2026-10-25)

The shared AOT/JIT bounded float-expression collector now accepts native float
return snapshots and inert cleanup after all stores/loads of private locals
are promoted away. The new admission is restricted to one block and at most 80 locals; cleanup
requires an exact, noncaptured, nonparameter float local with a slot. Existing
whole-body validation and all stack/cancellation/call-admission checks remain.
Unresolved local accesses, reference cleanup, loops and observable effects keep
their fallback paths. QORE_DISABLE_AOT_FLOAT_LOCAL_EXPRESSION is the compile
control. No runtime ABI or fast-math change is introduced.

Retain under the user-authorized broader tradeoff. The four target workloads
improve 9.5–17.8x in confirmation, with a 92.14% reduction in their geometric-mean
time versus before and 92.10% versus compile-disabled. All 55 broad aggregates
stay within 3% of both comparisons; the three flagged broad workloads have no
repeated >3% disjoint-range flag in their independent follow-up. This is not a
claim that every workload or layout improves.

The target integer callback has a repeated reverse-layout cost: confirmation
flags +5.90% versus before and +5.94% versus disabled; a separate five-fork
follow-up measures 7.607662 ns/item versus 7.271440/7.184455, a +4.62%/+5.89%
loss (0.336222/0.423207 ns/item). Its all-layout aggregate improves 1.61%/1.90%
in that follow-up; the initial screen had no such flag. All adverse results are
retained. Unchanged control LLVM does not establish the cause of timing changes.
This result is accepted as an overall tradeoff, not a passed no-regression gate.

The fresh matched optimized Java comparison puts direct local-float return
and result loops at 1.34x/1.60x Java time; callbacks remain 15.70x/14.58x Java time.
HotSpot inlines the callback targets while Qore still dispatches typed callbacks.
These are whole-loop observations, not isolated call latency, a production-wide
speedup or a global performance ceiling.

|!Workload|!Before ns/item|!Candidate ns/item|!Java ns/item|!Candidate / Java time
|direct_local_float_return|7.471732|0.783256|0.582978|1.344x
|callback_local_float_return|134.218060|7.576398|0.482715|15.695x
|direct_local_float_result|7.246338|0.724532|0.451429|1.605x
|callback_local_float_result|102.601749|6.574099|0.450782|14.584x

RESULTS and `results-aot-float-local-expression-macos-arm64.json` contain the matched optimized AOT/Java
checkpoint, target screen/confirmation, all 55 broad workloads and every adverse
result. Only AOT performance was measured. Release/Debug each pass 41 selected
fixtures and 19,533 QUnit assertions, with eight Linux-only RSS skips per build.
No pure-develop ratio, general Java parity or performance ceiling is claimed.


## Known callback native float arguments (2026-10-25)

AOT known same-program callbacks now accept an already-lowered native double
when IR value facts predate body-local promotion. Preserved boxed provenance
(including sparse NOTHING) keeps the old guards; only all-proven native floats
avoid the redundant boxing/NOTHING check. Existing call shape, context, lifetime,
stack, cancellation and fallback rules remain. No runtime ABI or fast-math change.
QORE_DISABLE_AOT_KNOWN_CALLREF_NATIVE_FLOAT is the compile control.

The lexical unary callback loop confirms 18.169134 to 1.273714 ns/item, 14.26x
faster, with a disabled control at 18.167233. Callbacks passed through parameters
remain about 7.5 ns/item. Retain under the authorized broader tradeoff: a float
callback control repeatedly loses 5.8% (0.44 ns/item) in the default layout,
while its all-layout aggregate is +1.47%/+0.69% versus before/disabled. The broad
55-workload screen has no regression flags and all aggregates stay within 3%
of both controls; its two-control follow-up also has no flags. This is not a
universal no-regression result, and unchanged control LLVM does not prove cause.

RESULTS and results-aot-known-callref-native-float-macos-arm64.json retain all
2,394 timed processes/7,182 windows, references and adverse results; the manual
source-localization feasibility experiment is labeled separately. Release passes
18 fixtures, 6,137 QUnit assertions and 391 qmod builds, with eight Linux RSS
skips. Only AOT performance is measured; no new Java/develop ratio or performance
ceiling is claimed. Next investigate bounded specialized entries that preserve
known callback targets across same-program function calls while keeping ordinary
dynamic/cross-program entry behavior, then take a fresh optimized Java checkpoint.


## Bounded AOT callback argument specialization (2026-10-25)

Standalone AOT compilation can select a private entry for a proven literal callback
passed through a same-Program function parameter. Initial scope is exact single-variant
pure float zero/unary targets and immutable private callback values/parameters. The
ordinary entry remains; code growth is limited to four private entries per callee and
64 per compilation, with bounded callee IR. qmod/split/preloaded, generic, method,
reference/closure/opaque and unsupported paths retain ordinary behavior. The control
is QORE_DISABLE_AOT_CALLBACK_SPECIALIZATION. Public APIs/serialization do not change.

Resolved function references retain immutable shared qualified/alias paths through
evaluation, native lowering and serialization, correcting the pre-existing AOT
namespace collision exposed by this work. Internal node layout gains a shared field.

Native construction of a resolved function reference uses the module-owned expression
slot, bound once to the parsed target at load time. Looking up the qualified name in
the executing caller's Program is insufficient: a private module function is absent
from the importing Program. Evaluating the bound reference still captures the current
calling Program and provenance; explicitly deleting that Program invalidates its
returned references. Deferred, unresolved references retain their dynamic lookup.
This reuses the existing expression serialization and runtime helper ABI.

Five-fork, three-layout optimized confirmation gives unary callbacks 7.560356 to
1.033154 ns/item (7.32x), and zero-argument callbacks 6.604806 to 1.061565 (6.22x).
Broad 55-workload screening flags boxed-integer fallback and temporary-hash controls;
the independent three-layout follow-up does not repeat a >3% disjoint-range flag.
The boxed fallback aggregate remains +1.62% versus before. All costs are retained
in RESULTS and results-aot-callback-specialization-macos-arm64.json.

Fresh optimized Temurin 25.0.2 / JMH 1.37 comparisons put direct unary/zero-argument
float loops at 1.28x/1.43x Java time and parameter-callback loops at 1.96x/2.07x.
These are whole-kernel measurements, not isolated call latency. Before is merged
feature/performance commit 892d9c93bd, not pure develop. Release passes 21 fixtures,
6,267 assertions and 391 qmods, with eight Linux RSS skips; 1,701 timed jobs / 5,103 windows
are independently replayed. No general Java parity or performance ceiling.

Next test removal of the redundant exception check after an actually inlined,
proven-nothrow known callback summary with no cleanup or guarded fallback. The
complex loop-callback case still costs about 149 ns/item and needs separate analysis.


## Known AOT callback summary exception checks (2026-10-25)

The existing known-reference AOT path skips its post-call exception check only
for an actually inlined, proven-nothrow summary with no argument cleanup,
guarded fallback, boxed result or runtime-local invalidation. One-shot EH
bookkeeping is still consumed. All existing stack and per-iteration cancellation
checks remain. Control: QORE_DISABLE_AOT_KNOWN_CALLREF_EXCEPTION_ELISION.
No new helper ABI or serialization schema; IR/JIT execution is unchanged.

Optimized three-layout, five-fork confirmation gives unary callbacks 1.048266
to 0.869634 ns/item (-17.04%) and zero-argument callbacks 1.030569 to 0.842893
(-18.21%). Both gains repeat in every layout versus before and disabled.
Under the user-authorized workload tradeoff, retain the improvement with known
reverse-layout costs: typed float callback +5.60% / +4.85% versus before/disabled
in the control follow-up, and direct zero-argument result +7.67% / +9.38%.
Their across-layout aggregates are +1.81% / -0.44% and +0.86% / +0.02%.
The three median losses in the broad 55-workload screen do not repeat.

Fresh optimized Temurin 25.0.2/JMH 1.37 comparison gives direct unary/zero-argument
float loops 1.37x/1.57x Java time and callbacks 1.78x/1.81x. Whole-kernel times,
not isolated call latency. Before is merged performance 07786f59d8, not pure
develop. Release passes 13 fixtures / 6451 assertions / 391 qmods, with eight
Linux RSS skips. All 1881 timed jobs / 5643 windows are independently replayed.
See bench/runtime-comparison/RESULTS.md and
results-aot-callref-summary-exception-macos-arm64.json for all costs and evidence.

Next test ordinary context-independent native fast-entry targets in the bounded
callback specialization, starting with floating-point callbacks whose bodies
contain loops and therefore have no expression summary. The complex callback
still costs about 148 ns/item. Preserve its actual call-frame, stack, cancellation
and exception semantics. Prior combined-poll and general proved-clear sink
experiments were rejected; neither is revived by this change. No global ceiling.


## Native float callback body targets (2026-10-25)

Bounded standalone callback entries now admit known context-independent native
zero/unary float targets without expression summaries. Ordinary native call-frame,
exception, stack and cancellation behavior remains. AddAssignFloat is admitted
by the context proof as SSA arithmetic, with existing independent local-safety
checks unchanged. QORE_DISABLE_AOT_FLOAT_BODY_FAST_PATHS disables both additions.
Same-Program, immutable-reference, signature and code-growth limits remain.
No public helper ABI or serialization change; IR/JIT execution is unchanged.

Optimized confirmation: complex callbacks 146.749303 to 12.775376 ns/item
(11.49x), typed float callbacks 7.904420 to 0.879619 (8.99x), repeatable across
three layouts versus before and disabled. Three specific targeted layout costs
repeat (about 7% integer callback, 7% float-result, 17% lexical direct-result).
Temporary hashes remain +5.90% versus before / +4.16% versus disabled across
layouts. Candidate/disabled default micro file-backed sections match before relocation and
use the same runtime; the cause of that observed cost remains unresolved. These
costs are retained under the authorized workload tradeoff, not discarded noise.

Fresh optimized Java comparison: simple direct unary/zero-argument loops take
1.40x/1.61x Java time; corresponding callbacks 1.73x/1.81x. Loop-containing
functions take 21.92x direct / 8.93x callback time. Whole-kernel comparisons,
not isolated call latency. No Java parity or production-wide throughput claim.
Release passes 19 fixtures, 7460 assertions, 391 qmods, 1110 IR/AOT references,
with eight Linux skips. 2007 jobs / 6021 timing windows independently replayed;
see RESULTS.md and results-aot-float-callback-bodies-macos-arm64.json.

Next examine a private native entry that forwards live RuntimeConfig and native
entry data instead of reacquiring them through TLS on each same-Program call.
The loop-callback profile still samples context/entry acquisition, stack checking
and named call-frame helpers. Preserve current stack, cancellation and exception
contracts, and retain ordinary entries for unsupported/cross-Program paths.
No global performance ceiling has been established.


## Private callback RuntimeConfig entries (2026-10-25)

Bounded known same-Program native float loop targets can receive a borrowed live
RuntimeConfig through a private companion. The caller acquires optional context
once, guards non-null and otherwise retains the ordinary entry. Existing v2 stack
and native-entry helpers consume the supplied context; cancellation descriptors
retain live addresses and every poll. O3 inlines the measured target into its
private callback consumer. Frames, exception checks and stack behavior remain.
The companion is bounded to 32 blocks/512 instructions, one per target, within the
existing 64-specialization cap. QORE_DISABLE_AOT_CALLBACK_RUNTIME_CONFIG disables
it; QORE_CONTROL_AOT_NULL_CALLBACK_RUNTIME_CONFIG exercises null fallback.
Private BatchCalleeInfo layout changes; ordinary native ABI, public helpers,
serialized metadata and IR/JIT execution remain unchanged.

Optimized confirmation: loop callback 13.431923 to 8.569874 ns/item, 36.20% less
time (1.57x), with the gain repeated in all layouts versus before and disabled.
The broad boxed-float method aggregate cost repeats at +3.76% versus before,
-0.33% versus disabled; reverse layout is +5.72% before with disjoint ranges.
Mixed callback pipeline remains +2.15% before/+0.47% disabled in broad follow-up.
The screen's large boxed-float fallback and CSV flags do not repeat. All controls
are retained under the authorized tradeoff, including prior committed costs.
Broad candidate/disabled file-backed sections match byte-for-byte before
relocation and use the same runtime; this does not explain observed differences.

Fresh Java ratios: simple direct unary/result loops 1.27x/1.47x, callbacks
1.62x/1.69x; loop-containing direct/callback cases 20.14x/5.26x Java time.
Whole-kernel comparisons, not isolated calls; direct loops still use ordinary
entries. Java parity and production-wide throughput gains are unestablished.
Release passes 20 fixtures, 7598 assertions, 391 qmods and 1110 IR/AOT references,
with eight Linux skips. All 2232 jobs/6696 windows independently replayed; see
RESULTS.md and results-aot-callback-runtime-config-macos-arm64.json.

Next use the updated profile to evaluate sharing already-acquired native-entry
and cancellation-descriptor data and reducing named frame setup/teardown. Keep
live stack/exception visibility, null fallback and every cancellation checkpoint.
Consider extending the same private-context proof to direct calls. No global
performance ceiling is established.


## Shared private callback entry state (2026-10-25)

Private same-Program native float loop companions now borrow one stack-owned
six-pointer descriptor beside RuntimeConfig. It carries native location slots
and live cancellation addresses; every stack check, atomic poll, frame and
exception path remains. QORE_DISABLE_AOT_CALLBACK_ENTRY_STATE selects the
previous companion ABI. Admission and limits are unchanged. Ordinary/public
native ABIs, serialized metadata and IR/JIT execution are unchanged; internal
compiler C++ layouts change.

Optimized confirmation reduces loop callback time 8.439409 to 6.817269 ns/item,
19.22% before/18.94% disabled, repeated in all layouts. Reverse-layout integer
callback costs remain +3.65% to +5.71% in flagged comparisons. Broad closure
calls retain +4.97% before/-0.33% disabled and two-read object loops +3.24%/-0.27%;
mixed callback pipeline +0.74%/+0.46%. The target benefit is retained under the
user-authorized broader workload tradeoff; all current and prior costs remain.
No production-wide throughput gain is claimed.

Fresh optimized Java ratios are 1.27x/1.47x for simple direct unary/result loops,
1.63x/1.70x for callbacks, and 19.71x/4.20x for loop-containing direct/callback
kernels. Whole-kernel results are not isolated call latency. Direct loops still
use ordinary entries. Java parity remains open.

Release passes 21 fixtures, 7754 assertions, 391 qmods and 1110 IR/AOT references,
with eight Linux-only skips. Twenty-two standalone variants also verify 44
interruptions after observing the target frame and successful recovery. All
2277 jobs/6831 windows are independently replayed. See RESULTS.md and
results-aot-callback-entry-state-macos-arm64.json for raw evidence and limits.

Next investigate descriptor load redundancy and named frame setup/teardown,
keeping asynchronous stack visibility, null fallback, exceptions and every
cancellation checkpoint. Extend the proven private-context path to direct
calls. No global performance ceiling is established.


## Direct native RuntimeConfig forwarding (2026-10-25)

Standalone AOT now admits same-Program direct zero/unary native float loop targets
to the existing private context/entry-state companion. Admission is independent
of callback discovery, bounded to 64 direct targets (callback budget unchanged),
32 blocks/512 instructions per target, one companion per target. Only single
variant context-independent local functions qualify; methods, generics, opaque
bodies, reference arguments and preloaded entries remain outside this extension.
QORE_DISABLE_AOT_DIRECT_RUNTIME_CONFIG disables it. Shared lowering retains live
polls, stack checks, frame publication, exceptions and ordinary null fallback.
Public/ordinary native ABIs and serialized metadata are unchanged; private
compiler C++ layout changes. New direct admission is not yet in qmod compilation.

Audit fixed a null original IR variant in late-resolved optional/auto local calls
by using the already-resolved AOT variant. The corrected source passes Release
qualification (14 fixtures, 1736 assertions, 1110 references, 391 qmods) and fresh
measurements. Direct loop time falls 13.634904 to 7.149034 ns/item (-47.57%). Shared
companion inlining changes cost the callback loop about 5.5%; reverse-layout
integer cost repeats at 6.99% versus disabled. Broad indirect_affine is+4.78%
before/-0.75% disabled; mixed callback pipeline -1.52%/+0.68%. Costs are retained
under the authorized overall tradeoff, with no production-wide speedup claim.

Fresh optimized Java ratios are 1.28x/1.48x for simple direct unary/result kernels,
1.66x/1.73x for callbacks, 10.51x for direct loop bodies and 4.45x for callback loop
bodies. These are whole-kernel ns/item on one host, not isolated call latency.
All 2988 timing jobs/8964 windows across initial and final campaigns are retained
and replayed; see RESULTS.md and results-aot-direct-runtime-config-macos-arm64.json.

Next recover callback inlining under a bounded code-growth policy, then optimize
native frame setup/teardown without losing asynchronous stack visibility. Extend
proven admission to qmods and additional scalar signatures after measurement.
No global performance ceiling has been established.


## Shared callback inlining and target alignment (2026-10-25)

Eligible standalone AOT private callback specializations force at most one call site
inline when their native companion is shared with direct callers. Shared companions
receive minimum 16-byte alignment; ordinary direct sites keep LLVM's cost policy.
QORE_DISABLE_AOT_SHARED_CALLBACK_INLINE disables both. Existing admission bounds,
frames, live cancellation polls, stack checks, exceptions and null fallback remain.
Public/ordinary ABI and serialized metadata stay unchanged; private compiler layout
changes. This does not extend qmod admission.

Fresh Release confirmation recovers callback loop time from 7.255059 to 6.857313
ns/item (-5.48%). The initial unaligned candidate's repeated 11% direct-layout cost
is resolved; final direct aggregate is +0.82% with every layout within 3% of both
controls. Integer callback aggregate costs 3.14%, with default cost 7.68% before/
7.29% disabled. Broad boxed fallbacks and captured closures remain 3.25-5.64% above
before, only 0.16-1.19% above disabled. Costs are published under the authorized
broader tradeoff; mixed pipeline is essentially unchanged.

Optimized Java ratios are 1.28x/1.48x for simple direct unary/result, 1.67x/1.73x
for callbacks, 10.45x direct loop and 4.29x callback loop. These are whole-kernel
ns/item on one host. Validation: 1827 assertions, 1110 IR/AOT references, 391 qmods;
2538 timing jobs/7614 windows including initial and diagnostic trials are retained.
See RESULTS.md and results-aot-shared-callback-inline-macos-arm64.json for raw evidence,
code growth, layout costs and all 62 audit checks. Next reduce native-frame overhead
and investigate qmod/signature coverage; no global ceiling has been established.

### Develop 54a6a86070 integration checkpoint

The Release merge preserves the branch's SnapshotInt/SnapshotFloat IDs and reads upstream v21 return-check opcode 409 using the header maximum to distinguish the histories. Eight binary round trips cover v20/v21/v22 snapshots and old/new return checks. Native cache integration keeps weak-reference reads separate from raw storage ownership. Apple embedded-runtime tests and static AOT linking include the frameworks used by module loading. See `bench/runtime-comparison/results-merge-54a6.json` and its audit for selected regression evidence. Prior performance numbers precede this merge.

## Native frame destructor devirtualization (2026-10-25)

The internal native-call frame class is final, allowing the removal helper to perform the existing restoration directly. Publication, reader synchronization and exception/stack behavior remain live; exported signatures and frame layout are unchanged. This removes virtual destructor dispatch in the shared native AOT/JIT helper; AOT measurements are confirmed, with no measured IR/JIT speedup claim.

Against optimized merged performance b83c2cdac1, direct loop time falls 6.822840 to 6.578522 ns/item (-3.58%), callback loop 6.577569 to 6.343294 (-3.56%). Both improve in all three layouts in two independent rounds. A 52-workload screen finds no repeated >3% disjoint-range cost; temporary_hash +3.88% screens at +0.77% on repeat. Fresh optimized Java ratios are 1.41x/1.65x for simple direct unary/result, 1.84x for callbacks, 11.15x for direct loop bodies and 4.45x for callback loop bodies. These are whole-kernel observations on one host, not isolated latency or pure-develop comparisons.

Seventeen targeted cases/451 assertions, 1110 IR/AOT references, 391 qmods, 4680 frozen-runtime reference checks and 90 Java references pass. All 1134 timing jobs/3402 windows are retained. See RESULTS.md, results-native-frame-final-macos-arm64.json and audit-native-frame-final.md. Next examine preparation of immutable frame entry metadata and call names, then qmod and scalar-signature coverage. No global ceiling is established.

## Cached native AOT frame construction (2026-10-25)

A nullable cached-name accessor keeps the existing acquire-publication contract while outlining lazy initialization and variant fallback. Both bound AOT frame pushes become leaf code; frame size/alignment, exported signatures, stack visibility and cancellation behavior are preserved. Against optimized performance 196f148ced, direct-loop time improves 4.59%, 5.92% and 5.92% across three rounds, with gains in every layout. Final repeat is 6.801502 to 6.398919 ns/item. Callback gain is unestablished. A simple-float-result reverse-layout cost (+5.01%, +2.21%, +3.56%; final about 0.026 ns/item) is accepted under the broader-workload tradeoff; aggregate costs vary +0.68%, +4.83%, +0.13%. These are whole-kernel observations, not isolated latency or a pure-develop comparison.

Release rebuilds 391 qmods; 17 targeted cases/451 assertions, 1110 IR/AOT references and 2340 candidate frozen-executable checks pass. The baseline is the preceding qualified runtime whose tracked sources match 196f, retaining the older embedded b83 revision. All 1224 timing jobs/3672 windows are replayed. See RESULTS.md, results-native-frame-cache-macos-arm64.json and audit-native-frame-cache.md. No fresh Java or IR/JIT ratio is claimed. Next investigate the named-frame cache used by callbacks, then redundant metadata with artifact compatibility review; preserve per-iteration language-loop cancellation. No global ceiling is established.

## Develop merge a9df2443b6 after cached frame construction

The 15 incoming commits add provider type-table/schema fixes, loader-warning handling, pipeline measurement controls and the macOS dual-stack reuse-port fix. The cached native-frame implementation remains identical to 5ac3369d66. The only merge conflict was the generated schema fixture: 156 incoming entries disagreed with current behavior. Full digests for all 276 actions agree across merged native/source, exact upstream source on the old and merged runtimes, and a fresh optimized unmodified develop runtime. Parent source still matches its saved fixture; 221 actions intentionally change from that parent. The fixture is refreshed to verified current behavior. No runtime semantics were changed to satisfy it.

Release qualification rebuilds all 391 qmods. Across 41 QUnit fixture runs, 722 cases are reported: 719 executed, 718 succeeded and one known pre-existing failure; 7653 assertions, 7652 succeeded. Totals include PipelineMemory child and parent QUnit summaries. Three skips cover two unavailable-dependency scenarios when those dependencies are installed, and static AOT linking with no static library built. Six CI image-check tests and 1110 IR/AOT references pass. SocketSpecificBind's cross-process case reports a successful loopback bind where rejection was expected; the same single failure occurs on the merged runtime, exact unmodified develop and the previously installed b83 runtime. The test remains unchanged. Other socket bind/handoff and native stack/call fixtures pass.

Presentation retains source fallbacks for the separately installed GrpcUtil and SalesforcePubSubDataProvider qmods in the older metadata format; those external qmods still need rebuilding. Two loader warnings in the harness are deliberate synthetic test inputs. Initial digest-test failure, warning-reviewer errors and the known socket failure are preserved in the evidence. These are correctness/integration checks, not fresh performance measurements. The isolated develop build is available for subsequent optimized comparisons.

[Audit](../bench/runtime-comparison/audit-merge-a9df.md); [evidence](../bench/runtime-comparison/results-merge-a9df.json).

## Cached named native AOT frame construction (2026-10-25)

First-use name interning is outlined from the exported named-frame helper used by specialized callbacks. The cached path loses its 160-byte stack reservation and twelve register saves/restores. Acquire/release cache publication, interned-name ownership and mutex/fork handling, frame publication, reader synchronization and exception behavior are preserved. No exported signature or frame-layout change; size/alignment remain 80/8.

Against optimized merged performance 08979af265, the callback loop confirms 6.476363 to 5.740818 ns/item (-11.36%), with >=2% improvement in every layout in screen and confirmation. Retain under the authorized broader-workload tradeoff. Callback-loop aggregate time improves 10.12%, 11.36% and 11.56% in screen, confirmation and the longer repeat, with >=2% gains in every layout in every round; the final repeat saves about 0.743 ns/item. Boxed-float method fallback is repeatedly slower: +3.39%, +3.09%, +3.19% aggregate, adding about 1.082 ns/item in the final repeat. The final default/reverse/shuffle layout costs are +3.46%, +3.19%, +1.43%. This persistent cost is not dismissed because fork ranges overlap. The tradeoff depends on the application call mix; no universal overall throughput gain is established. Direct-loop movement is -0.20%, +0.32%, +0.22%. The dynamic-integer fallback screen cost (+9.59%) falls to -0.27% in confirmation, and the wide-callback cost falls from +1.13% to +0.46%. Preserve all observations and carry the boxed-float fallback cost into the next broader checkpoint. Whole-kernel timings use identical AOT executables and frozen runtimes; this is not a pure-develop or new Java comparison. The all-target Release build verifies 391 qmods; 17 targeted cases/451 assertions, 1110 IR/AOT references and 2340 checks per runtime pass. All 1272 jobs/3816 windows are replayed. [Results](../bench/runtime-comparison/results-native-frame-named-macos-arm64.json) and [all 62 audit checks](../bench/runtime-comparison/audit-native-frame-named.md).

Next refresh the optimized Java/develop checkpoint and investigate wrapper-scoped stack-check outlining, avoiding the method-dispatch expansion in the rejected 2026-10-09 experiment. No global ceiling is established.

## Optimized current develop and Java checkpoint (2026-10-25)

The 820afd57c1 branch is compared with pure develop a9df2443b6 using matching optimized Release builds and separately compiled identical source kernels across three layouts. The checkpoint includes 52 workloads and four synthetic mixed CPU scenarios; no application-wide score is inferred. A separate fresh Java C2 comparison puts simple floating fast calls at 1.37-1.86x Java and loop-containing direct/callback targets at 10.01x/3.92x. These are whole-kernel measurements, not isolated call latency or a performance ceiling.

All 4680 AOT/1560 IR and 90 Java reference checks pass; 1044 timing jobs/3132 measured windows are replayed. The Java and develop checkpoints use different Qore executable sets and remain separately paired. [All tables and evidence](../bench/runtime-comparison/results-current-a9df-macos-arm64.json) and [62-check audit](../bench/runtime-comparison/audit-current-a9df-checkpoint.md). The named-frame change retains its documented boxed-float fallback cost against the prior branch baseline; cumulative gains do not negate that cost. Next investigate wrapper-scoped stack-check setup with that fallback as a control.

## Develop integration through c0bca49308 (2026-10-25)

Merged the Gemini/Vertex incomplete-stream fix onto performance checkpoint eea8311c66 without conflict. GeminiBodyShape matches upstream exactly; the Vertex fixture retains the branch module-name imports so it exercises compiled modules. EOF, empty/usage-only responses and a transport-only [DONE] marker raise LLM-ERROR without manufacturing successful completion or silently retrying. Valid token-limit and safety-block endings remain accepted. Core performance sources are unchanged.

The optimized all-target build verifies 391 qmods and explicitly recompiles GeminiBodyShape. Eight focused fixtures report 83 cases, 82 executed, 1167 assertions; the live Vertex test is skipped without credentials. All 1110 IR/AOT references pass, with no module-source fallback in these fixtures. Build warnings for optional TLS libraries, STL slist detection, CMake symlink policy and duplicate static libraries are preserved. This is integration qualification, not a new performance measurement or a full-suite pass. The previously documented macOS SocketSpecificBind failure is not rerun or reclassified here.

[Audit](../bench/runtime-comparison/audit-merge-c0bc.md); [evidence](../bench/runtime-comparison/results-merge-c0bc.json).

The initial documentation-check driver wrongly passed whole Qore source to qpp --table, which flagged unchanged logical-OR code lines. The failed attempt is preserved. Qdx --strict-tables correctly parses module documentation and passes; no source was changed to satisfy the wrong checker.

## Generated-code stack-check wrapper (2026-10-25)

The exported qore_rt_check_stack_v2 helper reads the supported live downward-stack limit and current SP directly; failed comparisons and unsupported views use a private noinline wrapper around the unchanged full guard. Its definition moves beside the live-limit accessor in thread.cpp, making the accessor visible even in split compilation-unit builds. The successful arm64 path changes from 35 instructions and a 48-byte stack reservation to nine instructions without stack reservation or register saves. Internal dispatch guards, deferred materialization, exception diagnostics, cancellation and every native/RSE predicate remain. No exported signature, layout or code-generator change. Five inspected method-dispatch controls retain their instruction counts and stack reservations.

The original NativeLeafContext fixture passed on the frozen baseline but failed on the candidate because its static limit sweep required an interval where a published method, rather than an earlier guard, overflowed. A smaller wrapper can legitimately eliminate that interval. The regression now forces the live limit to fail after publication for both v2 and v3 entries, including null-view fallback, and verifies actual compiled-entry overflow, object/class materialization, method/call-site diagnostics and successful reuse. The early-overflow and successful-call sweep checks remain. The strengthened fixture passes against the baseline and candidate Release runtimes and the candidate Debug runtime. The original failure is preserved. All Debug test processes passed; its driver then failed restoring module aliases removed by CMake. A separate recorded recovery recreated 388 missing links and restored 3 Debug links, verifying all 391 Release module hashes before timing.

Retain for the repeated callback-loop improvement: 5.823054 to 5.602200 ns/item (-3.79%) in the screen and 5.821274 to 5.533448 (-4.94%) in confirmation, with at least 2% improvement in every layout in both rounds. Direct-loop aggregate time falls 2.19% then 2.83%, but its every-layout 2% criterion is not met (one layout per round is below 2%). The boxed-float fallback control falls 3.22% then 1.39%; this does not establish a repeatable >=2% gain across layouts or erase the preceding named-frame change's separately recorded cost. The other confirmed controls also have lower aggregate time. No layout meets the prospective >3% disjoint-fork-range regression flag. The largest aggregate screen cost is indirect_affine_wide at +2.21%; it is below the prospective confirmation threshold and is unconfirmed. All 52 screen results remain visible; no universal improvement or fresh Java/develop claim. Validation: Release 18 cases/1103 reported assertions and Debug 8 cases/956 assertions, no skips; the strengthened baseline fixture also passes 652 assertions. The Release all-target build verifies 391 qmods. 1110 IR/AOT reference checks and 2340 frozen-executable checks per runtime pass. All 1086 timing jobs/3258 windows are independently replayed. Debug alias-restoration recovery is verified separately from the successful build/test child processes. No new Java/develop comparison; Java parity and a global ceiling remain unestablished. [Results](../bench/runtime-comparison/results-stack-check-wrapper-macos-arm64.json) and [all 62 checks](../bench/runtime-comparison/audit-stack-check-wrapper.md).

## Develop integration through 88f1a38482 (2026-10-25)

Merged seven develop commits onto performance 9a2128dc30. HTTP streaming responses abandoned on error, timeout or redirect now release their connection; failed fresh connects report the connect error instead of a capacity error. Public requestStreaming overloads remain available. The i18n tool limits re-keying/loading to selected app owners, retaining full contributing-owner loading for connection catalogs; the performance branch's best_by_type cache remains. Wire-contract tests distinguish schema rejection from later input checks; offline workers support a PATH-resolved interpreter; async-connect tests use operation states/events. CI link farms use per-run directories, and Alpine setup requires its CI-image marker.

The merge is conflict-free. Local audit fixes give three new test callback handles explicit signatures and replace an inherited invalid null-enum unit fixture with real enum declaration ownership checks. Release -O3 -DNDEBUG, allocation tracking off: all-target build and 391 qmods verified; StabilityAiRestClient-qmod explicitly rebuilt. Twelve Release runs execute 84 cases/1270 assertions, excluding 27 unrelated presentation cases by filter. Focused reruns execute 15 cases/181 assertions with 29 filter exclusions, including the worker launched as qore through PATH. Debug runs execute 8 cases/37 assertions plus 3776 native C++ checks, including the new HTTP regressions. 1110 IR/AOT references pass. Qdx strict tables passes StabilityAiRestClient documentation; all three changed CI scripts pass bash -n. No destructive CI setup script, full catalog synchronization, live provider or sanitizer run is claimed.

The focused driver initially misread QUnit's filtered summary: its child tests passed, but the driver expected only executed cases in the registered-case total. The recorded recovery verifies the worker's one executed case/nine assertions and two exclusions, then runs Qdx. The original Debug unit process crashed in ut_local_discard, and the saved pre-merge Debug binary reproduced the same crash. LLDB identified a fabricated assigned enum with a null member. The corrected fixture retains a real enum through its local, checks release of the declaration/member on deletion, and checks repeated deletion. Production behavior is unchanged; ql_debug.cpp is compiled only with DEBUG. Two initial debugger launch attempts failed on LLDB options before target execution; the recorded process-launch command produced the backtrace. The first fixture correction selected fixed scalar storage and asserted on enum assignment; using an ordinary local selects enum storage through the normal assignment path. The next run passed all native checks but the command-line wrapper incorrectly used set_return_value outside exec-class mode; the final requalification uses a normal top-level return. The crash, fixture assertion, wrapper error, and failed launch receipts are retained separately from the passing requalification. Debug module-link cleanup restores all 391 recorded Release aliases and verifies their hashes. No new performance timing is attributed to this merge; the preceding stack-wrapper measurements remain a 9a2128dc30 snapshot.

[All 62 audit checks](../bench/runtime-comparison/audit-merge-88f1.md); [evidence](../bench/runtime-comparison/results-merge-88f1.json).

## Private AOT companion stack guard (2026-10-25)

Eligible standalone AOT direct/callback native loop entries borrow a live stack-limit address in the private caller-owned entry descriptor. The descriptor has seven pointer fields; its callers and InternalLinkage callees are generated together, so this is not a public runtime ABI change. Each companion loads the current limit and its own native SP before its body. A successful comparison avoids qore_rt_check_stack_v2; null/unsupported views and overflow retain the complete existing guard. Cancellation, deferred context, call frames and ordinary callable entries remain. The observed target frame remains 96 bytes. General qmod compilation does not yet invoke these companion-discovery passes, so these measurements do not establish a new qmod speedup.

Both target loop kernels improve at least 2% in every layout in screen and confirmation; confirmation direct 6.260895 to 6.001758 ns/item (-4.14%), callback 5.557361 to 5.261014 (-5.33%). Whole-invocation counts 1 and 16 move less than 1% in aggregate. Original matched-build regressions are retained: typed integer unary +5.46%, boxed integer method call +2.77%, boxed integer fallback +3.34% in confirmation. Crossed runtime tests attribute boxed costs to runtime placement and typed cost to executable choice; seven typed-integer LLVM definitions are unchanged. Relinking identical candidate runtime objects with unused 64/128 bytes of text shifts boxed fallback from 35.176931 to 33.312813/33.831975 ns/item without changing the executable or runtime work. No padding or alignment tuning is retained. This demonstrates layout sensitivity, not a universal cacheline explanation and does not erase costs of the actual matched build. Four mixed workloads move -0.92% to +0.34% in the screen; these are unconfirmed. Retain the generated guard improvement under user authorization to assess the broader workload tradeoff, while reporting the measured costs.

Release validation passes 20 cases/1899 assertions; Debug passes 13 cases/1730 assertions plus 3776 native checks, with no skips. All 391 Release qmod aliases and hashes are restored after Debug. The generated-entry probe uses the actual optimized companion, forces a changed live limit after descriptor acquisition on the main thread and three foreign registrations, and verifies overflow/recovery and null fallback. Across 48 calls, baseline invokes the full helper 24 times and candidate 16 times. Its first driver failed because LLVM 22 clang++ was absent before a compiler child could start; the preserved v2 driver uses installed llc22 and system clang++, and succeeds. All 1680 jobs/5040 measured windows are independently replayed. No product code change was required during these validations.

Next: Extend existing discovery to owned, filtered, same-image monolithic module maps; freeze and measure actual qmod execution before enabling; shared/per-file maps remain a separate design. Java parity and a global ceiling remain unestablished. [Results](../bench/runtime-comparison/results-companion-stack-macos-arm64.json); [audit](../bench/runtime-comparison/audit-companion-stack.md).

## Develop integration through 5a787725d4 (2026-10-25)

Merged the deferred-handler local-slot fix onto companion guard checkpoint 0fae3afe8d without conflict. Body-local metadata now uses the complete shared AOT slot map, including deferred handlers, so a handler loop variable cannot be rebound by name to a same-named variable in an earlier completed loop. The regression checks rollback completion and retention of the original exception without a chained handler exception. The performance branch changes remain.

Optimized Release -O3 -DNDEBUG, allocation tracking off: all-target build and all 391 qmods verified. Five Release fixtures pass 12 cases/1300 assertions; four Debug fixtures pass 11 cases/378 assertions, with no skips. Handler execution covers AST, IR, JIT, tiered and source-stripped AOT modules, with related block-exit/rethrow checks and companion-call controls. All 1110 IR/AOT reference checks pass. Debug cleanup restores all 391 Release module aliases and hashes. No new timing, full-suite, module-python, module-xml or sanitizer result is claimed; preceding companion measurements remain a 0fae3afe8d source snapshot.

[Audit](../bench/runtime-comparison/audit-merge-5a78.md); [evidence](../bench/runtime-comparison/results-merge-5a78.json).

## Native call specialization inside qmod modules (2026-10-25)

Eligible monolithic AOT modules now use the same bounded known-callback and direct RuntimeConfig entry discovery as standalone executables. Only a complete module-owned callee map participates; shared maps, per-file filters and metadata-only passes retain their existing paths. The control is QORE_DISABLE_AOT_MODULE_CALL_SPECIALIZATION. Existing admission budgets, same-Program/effect/type proofs, ordinary entries, context, cancellation and stack handling remain. Public runtime APIs and serialization do not change.

Before is optimized feature/performance 620d490386 after the private-reference correctness fix, not pure develop. Actual stripped qmods wrap the same 52 kernels in three modules; timed work occurs inside the native module. Release -O3 -DNDEBUG, allocation tracking off, qcc -O3/LLVM 22.1.8, Apple M4 Pro/macOS 27.0.1. Three default/reverse/shuffled layouts, randomized paired runs, 4096 items, one warmup and three 150ms samples per fork; 3 screening forks and 5 confirmation forks. Whole-kernel ns/item, not isolated call latency.

|!Confirmed workload|!Before ns/item|!Candidate ns/item|!Speedup
|float:direct_loop_float_return|12.861214|5.966316|2.16x
|float:callback_loop_float_return|236.736636|5.230739|45.26x
|float:callback_local_float_return|223.468245|0.879878|253.98x
|float:callback_local_float_result|141.908820|0.832370|170.49x

All four targets improve in every layout in both rounds. No >3% disjoint-fork-range regression flag occurs. Costs remain: integer callback control 214.623 to 217.433 ns/item (+1.31%), boxed-float fallback 35.165 to 35.725 (+1.59%), boxed-int fallback 33.973 to 34.158 (+0.54%). The 52-workload screen also has direct-local-float-result +2.70% (not confirmed); mixed CSV +1.51%, object orders -0.89%, callback pipeline +0.29%, text index +0.01% (screen only). These controls do not establish general application speedup.

Short invocation controls (counts 1 and 16, 3 forks/3 layouts) have no regression flags: at 1, callbacks improve 21-28% and direct loop calls 1.72% in aggregate; the latter does not meet a 2% all-layout threshold. At 16, direct loop calls improve 17.57% and callbacks 78-84%. These measurements include fixed harness/dispatch/reference-construction costs and are not isolated call latency. In the default layout, the float module grows 184376 to 185544 bytes; micro 475720 and mixed 180424 bytes stay unchanged. This is not a whole-library code-size claim.

Release passes 23 QUnit cases/1049 assertions plus the non-QUnit batch-callee fixture; Debug 15 cases/397 assertions. All 391 Release qmods, 1110 IR/AOT and 780 additional module checks pass, with no skips or source fallback. Prepared before/candidate artifacts pass 6240 source/native references; actual loaded qmods and frozen runtimes are verified. All 1290 jobs/3870 windows are independently replayed. No owned build/test/profile overlaps timing; this shared host has no affinity, thermal or frequency control. No fresh Java/develop comparison or global performance ceiling. Next investigate remaining integer/dynamic module callback overhead and reference construction before revisiting cold-path register preservation.

[Audit](../bench/runtime-comparison/audit-aot-module-call-specialization.md); [evidence](../bench/runtime-comparison/results-aot-module-call-specialization-macos-arm64.json).

## Known integer callback specialization (2026-10-25)

AOT can now specialize an internal consumer for an immutable known callback returning an exact integer with zero or one exact integer parameter. It reuses existing bounded proofs and ordinary native integer entries in executables and eligible monolithic modules. Floating RuntimeConfig companion admission remains unchanged. QORE_DISABLE_AOT_INTEGER_CALLBACK_SPECIALIZATION disables this extension. No runtime Program ownership guard, public ABI or serialization changes.

Before is optimized feature/performance 8a7945a6eb, not pure develop. Actual stripped modules, Release -O3 -DNDEBUG, allocation tracking off, qcc -O3/LLVM 22.1.8, Apple M4 Pro/macOS 27.0.1. The same 53 kernels use default/reverse/shuffled layouts, randomized paired runs, 4096 items, one warmup and three 150ms samples per fork; three screening and five confirmation forks. Whole-kernel ns/item, not isolated call latency.

|!Confirmed target|!Before ns/item|!Candidate ns/item|!Change
|float:typed_local_int_unary|209.571828|208.919665|-0.31%
|float:typed_local_int_result|132.319946|0.805643|-99.39%
|micro:indirect_call|243.479753|240.323126|-1.30%
|micro:indirect_affine|241.625893|240.479001|-0.47%
|micro:indirect_branch_int|244.713837|240.877698|-1.57%

Retain: zero-argument integer callbacks improve in all three layouts in screening, confirmation and both short-call rounds. Confirmation is 132.319946 to 0.805643 ns/item (164.24x); unary 209.571828 to 208.919665 and forwarded callbacks around 240 ns/item do not meet the all-layout 2% criterion. The unary specialization is selected but retains dynamic per-iteration dispatch because a later lowering check misses its assigned native integer local; that gap is the next separate experiment. No regression flags occur in confirmation or short runs. The screen reverse-layout result_float_add cost (+7.91%) does not repeat (confirmation aggregate -0.34%). Existing float controls retain small aggregate costs: local-result +0.22%, local-return +1.07%, callback-loop +0.65%, direct-loop +0.13%; boxed float/int fallback improve 1.33%/0.38%. Screen-only direct_float_add +2.39% and mixed object orders +1.21% are unconfirmed; mixed CSV -0.06%, text index -0.85%, callback pipeline -1.57% are also screening observations. Short zero-argument calls improve 15.93% at count 1 and 75.27% at count 16. Unary short costs are +0.87%/+0.53%, retained in the evidence. This supports the specific zero-argument optimization, not overall application speedup or Java parity.

|!Short-call target|!Count|!Before ns/item|!Candidate ns/item|!Change
|float:typed_local_int_unary|1|899.759304|907.599449|+0.87%
|float:typed_local_int_result|1|833.711217|700.869083|-15.93%
|micro:indirect_call|1|2674.546645|2663.579338|-0.41%
|micro:indirect_affine|1|2702.886313|2691.664816|-0.42%
|micro:indirect_branch_int|1|2714.044206|2725.965886|+0.44%
|float:typed_local_int_unary|16|253.150093|254.487931|+0.53%
|float:typed_local_int_result|16|178.137471|44.047451|-75.27%
|micro:indirect_call|16|399.883172|393.187549|-1.67%
|micro:indirect_affine|16|400.905867|399.338551|-0.39%
|micro:indirect_branch_int|16|402.930805|394.973644|-1.97%

Short runs include fixed harness, dispatch, reference construction and cleanup costs. The micro dispatcher forwards callback parameters and has no new specialized entries; locally known integer consumers are selected. Reference construction once per invocation remains a follow-up lead.

All 391 standard-library qmods total 344786872 bytes before and 344786872 after. Default float module grows 185544 to 185640 bytes; micro 476296 and mixed 180424 bytes stay unchanged.

Release passes 20 cases/5073 assertions and Debug 5 cases/4123 assertions, with no skips or source fallback. All 391 Release aliases/hashes are restored after Debug; 1110 standard IR/AOT references, 780 additional module references and 6360 prepared before/candidate references pass. All 1524 jobs/4572 measured windows are independently replayed. The first two new test drafts failed parsing (bool bitwise assignment, then missing auto return types); both failures are retained and corrected before timing. No owned build/test/profile overlaps timing. Shared host, no affinity/thermal/frequency control; no fresh Java/develop comparison or global performance ceiling.

[Audit](../bench/runtime-comparison/audit-aot-integer-callback-specialization.md); [evidence](../bench/runtime-comparison/results-aot-integer-callback-specialization-macos-arm64.json).

## Native integer local arguments in known callbacks (2026-10-25)

AOT known-callback lowering now reuses the flow-sensitive assigned-state and exact type proof for an integer LoadLocal. Earlier flow-insensitive facts can describe a value produced by a callback as boxed or maybe-NOTHING even after native local promotion. This extension excludes boxed values, retained boxed-source provenance, references, closures and mismatching declared types; it retains ordinary guarded fallback. QORE_DISABLE_AOT_KNOWN_CALLREF_NATIVE_INT disables it. No runtime ABI, Program ownership or floating-point eligibility change.

Before is optimized feature/performance bffec5dd4a, including integer callback admission. Actual stripped modules, Release -O3 -DNDEBUG, allocation tracking off, qcc -O3/LLVM 22.1.8, Apple M4 Pro/macOS 27.0.1. The same 53 kernels use default/reverse/shuffled layouts and randomized paired runs; 4096 items, one warmup and three 150ms samples per fork, three screening and five confirmation forks. Whole-kernel ns/item, not isolated call latency.

|!Confirmed target|!Before ns/item|!Candidate ns/item|!Change
|float:typed_local_int_unary|214.526487|1.202459|-99.44%

Retain under the user-authorized broader-workload tradeoff: unary integer callbacks improve in every layout in screen, confirmation and both short-call rounds. Confirmation is 214.526487 to 1.202459 ns/item (178.41x), saving 213.324 ns/item. The zero-argument control remains 0.807217 to 0.805504 ns/item. Repeated costs are preserved: boxed-float fallback +3.22% in screening and +3.14% in confirmation (35.178897 to 36.283012 ns/item, +1.104 ns/item). Default-layout monomorphic float calls remain slower: +7.10% screen, +5.86% confirmation (4.617953 to 4.888427 ns/item), even though their confirmation aggregate is -0.48%. Default dynamic-float-result calls repeat +3.23%/+3.16% (12.205634 to 12.591918 ns/item), with confirmation aggregate +0.95%. Overlapping fork ranges remove disjoint-range flags in confirmation but do not erase these costs. Other confirmation controls move between -0.51% and +0.45% in aggregate; forwarded integer calls remain around 247-250 ns/item. Screen-only mixed CSV +1.05%, object orders +1.09%, callback pipeline +0.09%, text index -0.40% do not establish application speedup. The micro timed LLVM bodies and all 33 mixed definitions are identical; only micro module-init metadata length differs. This narrows investigation but is not a proven cause or dismissal of runtime placement costs. No padding or alignment tuning is retained. At count 1, unary time is 904.614495 to 694.669002 ns/item (-23.21%). At count 16, unary time is 262.692908 to 44.549172 ns/item (-83.04%). Short runs include fixed dispatch/reference-construction costs. No fresh Java/develop comparison or global performance ceiling is claimed.

|!Short-call target/control|!Count|!Before ns/item|!Candidate ns/item|!Change
|float:typed_local_int_unary|1|904.614495|694.669002|-23.21%
|float:typed_local_int_result|1|726.061504|723.542743|-0.35%
|float:typed_local_int_unary|16|262.692908|44.549172|-83.04%
|float:typed_local_int_result|16|46.075473|46.166728|+0.20%

Short invocations include fixed harness, dispatch, reference construction and cleanup costs. Three generated module layouts confirm removal of both typed and generic dynamic callback helpers from the unary specialized consumer, retaining cancellation checks. The micro dispatcher still forwards unknown callbacks and has no new specialized entries.

All 391 standard-library qmods total 344786872 bytes before and 344786872 after. Default benchmark module sizes (before/candidate): float 185640/185560, micro 477464/477464, mixed 180424/180424 bytes.

Release passes 26 cases/5376 assertions and Debug 7 cases/4415 assertions, plus the known-reference non-QUnit fixture in both builds, with no skips or source fallback. The expanded native/stripped-module regression has 233 assertions, including enabled/disabled structural checks, wide/negative integers, unassigned/optional/sparse values, exceptions and cancellation/recovery. All 391 Release aliases/hashes are restored after Debug; 1110 standard IR/AOT references, 780 additional module references and 6360 prepared before/candidate references pass. All 1446 jobs/4338 measured windows are independently replayed. No owned build/test/profile overlaps timing. Shared host, no affinity/thermal/frequency control; no fresh Java/develop comparison or global performance ceiling.

[Audit](../bench/runtime-comparison/audit-aot-integer-native-local-specialization.md); [evidence](../bench/runtime-comparison/results-aot-integer-native-local-specialization-macos-arm64.json).

## Optimized public-module checkpoint (2026-10-25)

The installed d09dc74b15 branch is compared with pure develop 5a787725d4 using matching optimized Release builds and 53 identical public-module kernels across three layouts. Public helper declarations keep pure develop runnable without the branch private-reference fix; function bodies are unchanged. Fresh optimized Java covers 45 kernels, including known integer/float calls and existing dynamic-boundary controls. All 6360 Qore and 675 Java reference checks pass, with C2 diagnostics and independent replay of 1089 jobs/3267 measured windows. Whole-kernel results, layout costs, Java inlining policy and limitations are retained in the [checkpoint evidence](../bench/runtime-comparison/results-current-module-macos-arm64.json) and [62-check audit](../bench/runtime-comparison/audit-current-module-checkpoint.md). No universal Java parity or overall performance ceiling is inferred.

Next address the demonstrated native-binding admission gap for module-owned references while retaining caller Program semantics, live body identity, debugger checks and ordinary fallback. The diagnostic is untimed and changes no eligibility.

## Native scalar module callbacks with full Program context (2026-10-26)

AOT module references capture the importing Program while their body belongs to the module Program. Exact zero/unary scalar references can now use compiler-published native bindings. Calls inside the owning module enter the captured importing Program with the existing full thread-count/TLPD/context helper, invoke the typed native body, and restore the caller. Cross-program calls never populate the prepared cache. Caller and body debugger checks, live normal-body identity, diagnostic publication, physical stack checks and fallback remain. Every different-owner native invocation applies the same caller parse-option override as ordinary evaluation, including calls whose captured Program is already current. QORE_DISABLE_AOT_MODULE_FUNCREF_FAST_CALL disables metadata admission; QORE_DISABLE_AOT_CROSS_PROGRAM_FUNCREF_FAST_CALL disables cross-program dispatch. Both disable switches are sampled on first use through the existing process-cached QoreEnvFlag mechanism. No public ABI or cache layout change.

Before is optimized production d09dc74b15 (repository parent b9afd33296 is documentation only). Both use Release -O3 -DNDEBUG, allocation tracking off, qcc -O3/LLVM 22.1.8 and actual stripped private modules on Apple M4 Pro/macOS 27.0.1. The unchanged 53 kernels use default/reverse/shuffled layouts and randomized paired runs: 4096 items, one warmup, three 150 ms samples per fork, three screening and five confirmation forks. Each target must improve at least 10% in every layout in both phases. These are whole-kernel ns/item, not isolated call latency.

|!Confirmed target|!Before ns/item|!Candidate ns/item|!Change
|micro:indirect_call|242.408031|67.649498|-72.09%
|micro:indirect_float|239.949652|65.735602|-72.60%
|micro:indirect_wide|316.887035|68.298120|-78.45%
|micro:indirect_affine_wide|317.578409|68.201052|-78.52%

Retain under the authorized broader-workload tradeoff. All four targets pass the 10% gate in every layout in both phases; confirmation improves them by 72.1–78.5%. The mixed callback pipeline improves 47.9% (245.854817 to 128.112013 ns/item). Other confirmed mixed workloads change by -0.41% (CSV), -0.23% (orders) and -0.05% (text). Repeated aggregate costs are direct local float result (+4.79% screen, +1.12% confirmation; +0.008274 ns/item confirmed), polymorphic float expression (+1.70%, +1.42%; +0.158748 ns/item confirmed), known integer unary (+0.46%, +0.07%) and direct float loop (+0.13%, +0.29%). No >3% disjoint-range layout flag repeats between phases. Confirmation alone flags typed local integer result/default (+5.11%) and closure/reverse (+3.85%); the former has an aggregate +8.16% cost (+0.063609 ns/item), while aggregate closure time improves 1.25%. Monomorphic float has +3.35% aggregate confirmation cost after -2.34% in screening. Preserve all results rather than treating these as proven zero-cost paths. Short indirect calls improve 7.61% at count 1 and 44.28% at count 16; the count-1 result is below 10%, as allowed by the prospective short-call characterization plan. The known-integer short control changes +1.35% at count 1 (+9.240452 ns/invocation) and -0.44% at count 16 (-3.093845 ns/invocation), with no disjoint-range flags. This removes the prior candidate's repeated roughly 100 ns setup cost without pooling measurement epochs. The full Program transition remains material overhead; these results do not establish Java parity.

|!Same candidate binary, default layout|!Enabled ns/item|!Cross path disabled ns/item
|micro:indirect_call|67.847782|243.118974
|micro:indirect_float|65.850049|238.901774
|micro:indirect_wide|68.325805|316.167802
|micro:indirect_affine_wide|67.346337|318.260592

|!Short-call target/control|!Count|!Before ns/item|!Candidate ns/item|!Change
|micro:indirect_call|1|2677.604570|2473.761521|-7.61%
|float:typed_local_int_unary|1|683.545974|692.786426|+1.35%
|micro:indirect_call|16|393.892218|219.477218|-44.28%
|float:typed_local_int_unary|16|43.610594|43.417229|-0.44%

Short invocations include fixed harness, dispatch, reference construction and cleanup costs. The internal header change updates the AOT runtime identity, so matching modules are rebuilt: 391 standard-library qmods total 344786872 bytes before and 344786872 after; 391 hashes change. Old identity rejection was preserved. Default benchmark module sizes (before/candidate): float 185560/185560, micro 475720/475720, mixed 180424/180424 bytes.

The first metadata-only candidate failed all four target gates in 954 jobs/2862 windows. Actual-workload interposition then showed that the module loop and captured callback are different Programs. The accepted candidate retains the full transition and the diagnostic confirms native handling and Program restoration. Before timing, review also found a missing caller-option override on the newly admitted same-Program path; that was fixed and covered by native fixture instrumentation. A complete 1740-job/5220-window intermediate candidate confirmed the callback gains but exposed about 100 ns per-invocation overhead in the known-integer short control. That candidate is retained separately in evidence; the final candidate replaces a locked environment lookup on every reference construction with the existing first-use cached flag and is measured in fresh runs. Earlier helper compilation/parse-order failures, a stale-qmod identity rejection and the intentionally stopped obsolete Debug build remain in evidence.

Release: 46 cases/7224 assertions; Debug: 27 cases/6263 assertions, plus native checks and no skips or source fallback in accepted checks. The focused fixture has 199 QUnit assertions in each build plus native assertions. All 391 Release aliases/hashes are restored; 1110 standard, 780 additional module and 6360 prepared references pass. All 1710 timing jobs/5130 windows are separately replayed. No owned build/test/profile overlaps timing. Shared host without affinity/thermal/frequency control. No fresh Java/develop comparison or global ceiling claim.

[Audit](../bench/runtime-comparison/audit-aot-module-function-reference-context.md); [evidence](../bench/runtime-comparison/results-aot-module-function-reference-context-macos-arm64.json).

## Finite literal hash keys revisited (2026-10-26)

A private immutable plain integer hash can be replaced by scalar values when a key join has two to four literal incoming edges, every key is present and selected integers fit the inline representation. A separate complete-use proof removes selector strings only when their whole graph is private to eliminated reads. Observable, escaped, mutable, wide-integer and exceptional-reentry cases retain the existing path. Current otherwise-written-local exclusions are preserved. QORE_DISABLE_IR_FINITE_HASH_KEY_SCALAR_REPLACEMENT disables projection; QORE_DISABLE_IR_FINITE_HASH_KEY_SELECTOR_ELISION keeps projection but disables selector removal. These are compiler controls. Runtime context, lifetime synchronization, ABI and per-iteration cancellation remain unchanged.

This is a fresh trial against 208e3b0bc2 after the native module callback optimization. The previous rejected finite-selector trial remains unchanged; this decision evaluates absolute costs and broader workload benefits as subsequently authorized. Both runtimes use Release -O3 -DNDEBUG, allocation tracking off and qcc -O3/LLVM 22.1.8 on Apple M4 Pro/macOS 27.0.1. Actual stripped private modules cover 53 unchanged kernels, randomized paired before/candidate/compile-disabled runs, default/reverse/shuffle-487 layouts, 4096 items, one warmup and three 150 ms samples, three screening forks and five confirmation forks. The target must improve at least 10% against both controls in every layout in both phases. Values are whole-kernel ns/item, not isolated call latency.

|!Confirmed workload|!Before ns/item|!Candidate ns/item|!Compile-disabled ns/item|!Change vs before
|micro:temporary_hash|329.384651|1.674527|326.554215|-99.49%
|mixed:csv_report|938.548761|938.608056|941.698071|+0.01%
|mixed:object_orders|1719.678474|1741.180640|1723.200546|+1.25%
|mixed:callback_pipeline|127.841010|127.833241|127.746500|-0.01%
|mixed:text_index|1462.068810|1456.330128|1465.098533|-0.39%

Retain under the authorized broader-workload tradeoff. The target improves 99.49% against baseline (329.384651 to 1.674527 ns/item, 196.7x) and against the compile-disabled control (326.554215), passing the 10% gate in every layout in both phases. Closure costs repeat versus baseline: closure_call +5.36% screening/+5.47% confirmation (+1.043172 ns/item confirmed), and closure_capture +4.79%/+4.69% (+1.153187 ns/item). Confirmation flags closure/default and shuffle and captured closure/all three layouts; default captured closure was not flagged in screening. Similar closure costs appear with the optimization disabled in the candidate runtime; candidate versus disabled changes only +0.24% and +0.49% in confirmation. This suggests a runtime-build effect, but does not establish its cause or erase the baseline costs. Other repeated aggregate baseline costs are callback local float result (+0.27%/+1.36%, +0.011164 ns/item), callback local float return (+1.66%/+0.07%, +0.000624), boxed integer method call (+2.16%/+0.26%, +0.045548), boxed integer fallback (+0.14%/+0.70%, +0.227765), monomorphic call (+0.11%/+0.22%, +0.009098) and temporary list (+0.85%/+0.35%, +0.005766). Repeated costs against compile-disabled are callback local float result (+1.20%/+1.13%, +0.009296), result float add (+1.20%/+0.67%, +0.005548), and boxed float fallback (+0.28%/+0.07%, +0.022174). All individual forks/layouts remain in the evidence. Confirmed mixed workload changes versus baseline are CSV +0.006%, orders +1.25%, callback pipeline -0.006% and text -0.39%; the orders cost did not repeat its screening direction. This establishes a large benefit for eligible temporary hashes, not a measured overall application speedup. Short hash invocations improve 11.91% at count 1 and 66.38% at count 16; the known-integer control changes +0.44% and -0.05%, without flagged layout regressions. Existing module indirect integer and float performance remains near 67.8 and 65.5 ns/item in this campaign. Java parity and a global performance ceiling are not established.

|!Short-call target/control|!Count|!Before ns/item|!Candidate ns/item|!Compile-disabled ns/item
|micro:temporary_hash|1|3055.332859|2691.534139|3080.213469
|float:typed_local_int_unary|1|686.556679|689.559972|691.031250
|micro:temporary_hash|16|502.834672|169.042477|499.395838
|float:typed_local_int_unary|16|43.471453|43.448178|43.804917

Short invocations include fixed dispatch, setup and cleanup costs. The separately measured default-layout target diagnostic is not pooled with the three-layout campaign:

|!Form|!Before|!Candidate|!Compile-disabled|!Projection only
|standalone|330.531716|1.618685|333.853833|41.024727
|module|327.549871|1.671943|323.384457|39.821696

All 391 rebuilt standard-library qmods total 344786872 bytes before and 344786872 after; 391 hashes change. Release 36 cases/5291 assertions; Debug 9 cases/5053 assertions, plus non-QUnit/native checks. The focused fixture has 512 assertions per build. All 391 Release aliases/hashes are restored. All 2574 broad timing jobs/7722 windows and 24 diagnostic jobs/72 windows are separately replayed. Shared host, without affinity/frequency/thermal controls; no owned build/test/profile overlaps timing. No fresh Java/develop comparison, production-wide weighted speedup or global performance ceiling claim.

[Audit](../bench/runtime-comparison/audit-aot-finite-key-selector-revisit.md); [evidence](../bench/runtime-comparison/results-aot-finite-key-selector-revisit-macos-arm64.json); [historical rejection](../bench/runtime-comparison/results-aot-finite-key-selector-rejected-macos-arm64.json).

## Explicit native call context reuse (2026-10-26)

Native scalar function-reference paths now pass their existing RuntimeConfig to Program-entry and parse-option scopes. Both helpers retain their same-thread ThreadData for restoration. Generic entry still acquires the current thread context normally. Full Program locking, thread counts, local-data initialization, debugger behavior, caller parse options, stack checks and cancellation remain. Each internal helper gains one pointer; the public ABI is unchanged. Program snapshot synchronization still uses TLS, so this does not make cross-Program calls TLS-free.

Fresh optimized before/after measurements use parent 334dd7e622, Release -O3 -DNDEBUG with allocation tracking off, qcc -O3/LLVM 22.1.8 on Apple M4 Pro/macOS 27.0.1. All 53 actual private-module kernels are unchanged. Paired randomized runs use default/reverse/shuffle-487 layouts, 4096 items, one warmup and three 150 ms samples, three screening forks and five confirmation forks. At least two of four predeclared indirect-call targets must improve at least 2% in aggregate and improve in every layout in both phases. Values are whole-kernel ns/item, not isolated call latency.

|!Confirmed workload|!Before ns/item|!Candidate ns/item|!Change
|micro:indirect_call|68.685881|65.507456|-4.63%
|micro:indirect_float|65.885714|63.938988|-2.95%
|micro:indirect_wide|68.230406|65.405699|-4.14%
|micro:indirect_affine_wide|67.875093|65.361728|-3.70%
|mixed:csv_report|940.960433|938.906104|-0.22%
|mixed:object_orders|1742.197364|1743.591925|+0.08%
|mixed:callback_pipeline|129.421262|126.236229|-2.46%
|mixed:text_index|1466.082357|1453.444261|-0.86%

Retain under the user-approved broader workload tradeoff. All four predeclared targets pass the >=2% aggregate and every-layout improvement gate in both phases; confirmation saves 1.95-3.18 ns/item (2.95-4.63%). The mixed callback pipeline improves 2.31% in screening and 2.46% in confirmation (3.19 ns/item); closures improve 4.78%/5.44% and captured closures 2.34%/2.51%. Costs are retained: boxed integer method calls increase 5.50%/2.82% (+0.932070 ns/item in confirmation), boxed float method calls 3.26%/2.13% (+0.729674), list building 1.05%/2.45% (+0.690015), monomorphic float calls 0.71%/4.13% (+0.172687), known unary integer callbacks 0.34%/0.17% (+0.001925), and direct method calls 0.16%/0.02% (+0.000314). Historical boxed-fallback kernel names now exercise guarded scalar-member entries; these are real costs, not evidence that only cold fallback is affected. Boxed-integer disjoint-range flags occur in both phases but different layouts; list-building flags do not repeat, and boxed-float flags appear only in confirmation. No single global workload-weighted speedup is inferred. Source and static machine review verify the intended lookup removals; all nine paired qmod code sections are byte-identical. Runtime code layout or the larger internal helpers may contribute to control costs, but no root cause is established. Count-1 and count-16 targets and the known-integer control have no >3% disjoint-range regression flags; fixed invocation costs dilute the gain. No per-call diagnostic flag is introduced.

|!Short-call target/control|!Count|!Before ns/item|!Candidate ns/item
|micro:indirect_call|1|2475.248849|2469.802888
|micro:indirect_float|1|2549.088130|2520.740733
|float:typed_local_int_unary|1|690.840686|687.244650
|micro:indirect_call|16|219.527690|215.723837
|micro:indirect_float|16|221.085431|218.340690
|float:typed_local_int_unary|16|44.182859|44.115182

Short invocations include fixed dispatch, setup and cleanup costs. Static disassembly shows parse-option restoration reduced from 50 instructions with direct/indirect calls to 10 instructions without calls; full Program restoration remains and falls from 96 to 41 instructions. These instruction counts are not timing estimates.

All 391 standard-library qmods total 344786872 bytes before and 344786872 after; 0 hashes change. Release 52 cases/7498 assertions; Debug 30 cases/7388 assertions, plus non-QUnit/native checks. All 391 Release aliases/hashes are restored. All 1722 timing jobs/5166 windows are separately replayed. Shared host, without affinity/frequency/thermal controls; no owned build/test/profile overlaps timing. No fresh Java/develop comparison, production-wide weighted speedup or global performance ceiling claim.

[Audit](../bench/runtime-comparison/audit-aot-explicit-native-context.md); [evidence](../bench/runtime-comparison/results-aot-explicit-native-context-macos-arm64.json).

## Optimized module checkpoint after explicit context reuse (2026-10-26)

Installed branch 5825d409b4 is compared with pure develop 5a787725d4 using matching optimized Release builds and 53 identical public-module kernels across three layouts. Fresh optimized Java covers 45 kernels. All 6360 Qore and 675 Java reference checks pass, with C2 diagnostics and independent replay of 1089 jobs/3267 measured windows. Whole-kernel results, layout costs, Java inlining policy and limitations are retained in the [checkpoint evidence](../bench/runtime-comparison/results-post-context-module-macos-arm64.json) and [62-check audit](../bench/runtime-comparison/audit-post-context-checkpoint.md). No universal Java parity or overall performance ceiling is inferred.

Next evaluate the redundant Program-data map lookup. Any broader ancestor-context reuse must preserve observable thread counts and teardown behavior.

## Program thread-data lookup reuse (2026-10-26)

Program context entry now uses the iterator already found under tlock when its thread-local data exists, avoiding a second linear vector_map search. Initial creation, locking, clearing waits, top-level catch-up, thread counts, debugger behavior, cancellation and context restoration remain unchanged. The production change is one line and adds no public API, ABI or per-call diagnostic flag.

Fresh optimized before/after measurements use installed parent b328d3b129, Release -O3 -DNDEBUG with allocation tracking off, qcc -O3/LLVM 22.1.8 on Apple M4 Pro/macOS 27.0.1. All 53 actual private-module kernels are unchanged. Paired randomized runs use default/reverse/shuffle-487 layouts, 4096 items, one warmup and three 150 ms samples, three screening forks and five confirmation forks. The prospective gate requires at least two ordinary call targets to improve at least 1% in aggregate and improve in every layout in both phases, or both 64-worker diagnostic targets to improve at least 5% in every layout in both phases. Values are whole-kernel ns/item, not isolated call latency.

|!Confirmed ordinary workload|!Before ns/item|!Candidate ns/item|!Change
|micro:indirect_call|65.279846|64.788426|-0.75%
|micro:indirect_float|64.143809|63.019310|-1.75%
|micro:indirect_wide|65.478552|64.972614|-0.77%
|micro:indirect_affine_wide|65.385013|64.611916|-1.18%
|mixed:csv_report|932.574463|928.047689|-0.49%
|mixed:object_orders|1733.908561|1736.943233|+0.18%
|mixed:callback_pipeline|125.460719|126.116494|+0.52%
|mixed:text_index|1457.886994|1451.140575|-0.46%

Retain under the user-approved broader workload tradeoff. The same two ordinary targets (indirect_float and indirect_affine_wide) satisfy the prospective 1% aggregate/every-layout gate in both phases; both 64-worker targets satisfy the separate 5% every-layout gate in both phases. All four ordinary targets improve in confirmation by 0.75-1.75%; 16-worker diagnostic reductions are 6.95-7.07%, and 64-worker reductions are 12.38-12.88%. Repeated ordinary aggregate costs are boxed_float_method_fallback +0.016/+0.238% (confirmation +0.082873 ns/item), list_mutate +0.799/+0.525% (+1.017717 ns/item), temporary_hash +0.180/+0.299% (+0.004995 ns/item), and mixed callback_pipeline +0.588/+0.523% (+0.655774 ns/item), screen/confirmation respectively. The floating-add screen flag does not repeat; confirmation, both diagnostic phases and both short-call phases have no greater-than-3% disjoint-range regression flag. Short calls show mixed changes below 1%, including count-1 indirect integer +0.335%, so no general short-call speedup is claimed. These measurements support the small lookup optimization despite those costs; they do not establish zero regression, statistical equivalence of controls, a weighted production-wide gain, or Java parity.

The separate native harness holds 0, 16 or 64 registered workers asleep in Program context and starts the benchmark thread last. Its map index is verified before timing (1, 17 or 65). Workers remain idle during samples and are joined before cleanup. The same unchanged module kernels run; setup and teardown are outside their timed windows. This measures a larger existing map, not actively contending-worker throughput, and is not pooled with the ordinary harness.

|!Confirmed diagnostic|!Before ns/item|!Candidate ns/item|!Change
|workers-0:micro:indirect_call|65.767543|65.014943|-1.14%
|workers-16:micro:indirect_call|85.062892|79.152582|-6.95%
|workers-64:micro:indirect_call|130.603685|114.438226|-12.38%
|workers-0:micro:indirect_float|64.397445|63.005402|-2.16%
|workers-16:micro:indirect_float|83.012250|77.142314|-7.07%
|workers-64:micro:indirect_float|128.463183|111.913428|-12.88%

|!Short-call target/control|!Count|!Before ns/item|!Candidate ns/item
|micro:indirect_call|1|2449.646328|2457.863497
|micro:indirect_float|1|2527.667852|2515.097633
|float:typed_local_int_unary|1|685.768887|683.519313
|micro:indirect_call|16|216.164785|214.855411
|micro:indirect_float|16|219.727826|219.906725
|float:typed_local_int_unary|16|43.725884|43.727303

Short invocations include fixed dispatch, setup and cleanup costs. Static disassembly shows the successful-entry second scan is absent and the complete function decreases from 261 to 247 instructions. These instruction counts are not timing estimates.

All 391 standard-library qmods total 344786872 bytes before and 344786872 after; 0 hashes change. Release 52 cases/7498 assertions; Debug 30 cases/7388 assertions, plus non-QUnit/native checks. All 391 Release aliases/hashes are restored. All 2100 timing jobs/6300 windows are separately replayed. Shared host, without affinity/frequency/thermal controls; no owned build/test/profile overlaps timing. No fresh Java/develop comparison, production-wide weighted speedup or global performance ceiling claim.

[Audit](../bench/runtime-comparison/audit-aot-program-data-lookup.md); [evidence](../bench/runtime-comparison/results-aot-program-data-lookup-macos-arm64.json).

## Pre-entry function-reference decoding reuse (2026-10-26)

The native cross-Program callback path reuses its already decoded exact function reference for the immediately following pre-entry binding check. The generic decoder retains flag, tag, captured-Program and RTTI checks; a shared inlined helper preserves typed-entry, debugger, metadata and live-body checks. Full Program entry and the complete post-entry decoder remain. No live metadata is reused across entry; no public ABI, calling convention or compiler eligibility rule changes.

Fresh optimized before/after measurements use installed parent 8bc8015683, Release -O3 -DNDEBUG with allocation tracking off, qcc -O3/LLVM 22.1.8 on Apple M4 Pro/macOS 27.0.1. All 53 actual private-module kernels are unchanged. Each before/candidate pair uses exactly the same optimized module file, with the frozen runtime selected and its loaded path verified separately. Paired randomized runs use default/reverse/shuffle-487 layouts, 4096 items, one warmup and three 150 ms samples, three screening forks and five confirmation forks. The prospective gate requires the same two or more indirect targets to improve at least 1% in aggregate and improve in every layout in both phases. Values are whole-kernel ns/item, not isolated call latency.

|!Confirmed workload|!Before ns/item|!Candidate ns/item|!Change
|micro:indirect_call|66.807328|63.736720|-4.60%
|micro:indirect_float|65.781224|62.204317|-5.44%
|micro:indirect_wide|66.026978|63.538779|-3.77%
|micro:indirect_affine_wide|66.630249|63.297025|-5.00%
|mixed:csv_report|984.777213|985.122760|+0.04%
|mixed:object_orders|1743.767368|1740.328438|-0.20%
|mixed:callback_pipeline|128.787645|128.127342|-0.51%
|mixed:text_index|1518.425293|1513.727214|-0.31%

Retain under the user-approved broader workload tradeoff. All four targets pass the unchanged prospective 1% aggregate/every-layout gate in both common-module phases and both initial phases. With identical module binaries, confirmation saves 3.77-5.44% (2.49-3.58 ns/item) across those calls. The common-module screen floating-add flag does not repeat; confirmation and both short-call phases have no greater-than-3% disjoint-range regression flag. This does not establish zero cost or equivalence of controls. Every repeated positive common-module aggregate cost is listed below.

|!Repeated common-module cost|!Screen change|!Confirmation change|!Confirmation ns/item cost
|float:callback_local_float_return|+1.493%|+0.009%|+0.000077
|float:direct_loop_float_return|+0.212%|+0.164%|+0.009759
|micro:boxed_int_method_fallback|+0.839%|+0.484%|+0.167207
|micro:closure_call|+1.358%|+1.134%|+0.213786
|micro:closure_capture|+1.262%|+0.635%|+0.159664
|micro:dynamic_int_result_fallback|+1.661%|+1.534%|+0.320304
|micro:list_mutate|+0.528%|+0.394%|+0.790876
|micro:monomorphic_call|+0.297%|+0.187%|+0.008065

The largest repeated percentage cost is dynamic_int_result_fallback (+1.53%, +0.320304 ns/item); ordinary and captured closures cost +1.13%/+0.64% (+0.213786/+0.159664 ns/item). List mutation costs +0.39% (+0.790876 ns/item). Mixed confirmation changes are callback_pipeline -0.51%, CSV +0.04%, orders -0.20%, and text -0.31%; no weighted production-wide improvement is inferred. Short count-1 integer calls improve 0.57%, while float calls cost 0.61% and the known-integer control costs 0.24%. Count-16 integer/float aggregates improve 1.04%/1.30%, but not every individual layout improves, so no universal short-call gain is claimed. The initial captured-closure reverse-layout cost remains recorded separately; its absence from common-module flags does not prove the runtime-path layout difference was its sole cause. The repeatable call gains and smaller measured control costs justify retaining this narrow change; Java parity and a global performance ceiling remain unestablished.

|!Short-call target/control|!Count|!Before ns/item|!Candidate ns/item
|micro:indirect_call|1|2572.509482|2557.803962
|micro:indirect_float|1|2603.970189|2619.872133
|float:typed_local_int_unary|1|722.125648|723.870446
|micro:indirect_call|16|222.369489|220.065820
|micro:indirect_float|16|226.408747|223.472170
|float:typed_local_int_unary|16|45.832617|45.495215

Short invocations include fixed dispatch, setup and cleanup costs. Static disassembly verifies removal of the repeated pre-entry decoder call and retention of complete post-entry validation. Function sizes alone do not estimate executed instruction counts or speedups.

All 391 standard-library qmods total 344786872 bytes before and 344786872 after; 0 hashes change. Release 52 cases/7498 assertions; Debug 30 cases/7388 assertions, plus non-QUnit/native checks. All 391 Release aliases/hashes restored. All 1962 timing jobs/5886 windows separately replayed. Shared host, without affinity/frequency/thermal controls; no owned build/test/profile overlaps timing. No fresh Java/develop comparison, production-wide weighted speedup or global performance ceiling claim.

An initial separately linked module campaign is preserved separately. Its optimized LLVM matched in all nine module pairs, but LC_RPATH command sizes were 120 versus 128 bytes, shifting text-section addresses by 8-16 bytes. The common-module follow-up was declared before its timing and retains the original thresholds and all three layouts. Initial and common-module observations are not pooled.

|!Initial confirmation target (unequal runtime-path layout)|!Before ns/item|!Candidate ns/item|!Change
|micro:indirect_call|64.662564|61.575312|-4.77%
|micro:indirect_float|63.445994|59.584210|-6.09%
|micro:indirect_wide|64.091159|61.698083|-3.73%
|micro:indirect_affine_wide|65.090126|61.639910|-5.30%

All four initial targets pass both phases, with 3.73-6.09% lower confirmation time per item. Captured-closure reverse-layout costs repeat (+5.99/+5.66%); all aggregate costs and short-call observations are retained. All nine optimized LLVM dumps match, but LC_RPATH sizes of 120/128 bytes shift module code. The separate common-module campaign is required before attributing gains/costs to the runtime change or retaining it.

|!Repeated initial aggregate cost|!Screen change|!Confirmation change|!Confirmation ns/item cost
|float:callback_local_float_return|+0.249%|+0.481%|+0.004072
|float:callback_loop_float_return|+0.763%|+0.020%|+0.000988
|float:direct_loop_float_return|+0.337%|+0.005%|+0.000301
|micro:boxed_int_method_call|+1.124%|+0.448%|+0.077660
|micro:boxed_int_method_fallback|+1.591%|+0.451%|+0.152683
|micro:closure_call|+2.755%|+1.035%|+0.196067
|micro:closure_capture|+0.737%|+2.646%|+0.643108
|mixed:object_orders|+0.050%|+0.336%|+5.837885

The initial campaign contains 1812 jobs/5436 windows, independently replayed and retained alongside the common-module campaign. Static layout differences do not establish the cause of any timing change.

[Audit](../bench/runtime-comparison/audit-aot-cross-decode.md); [evidence](../bench/runtime-comparison/results-aot-cross-decode-macos-arm64.json).

## Prepared indexed integer addition (2026-10-26)

AOT compilation selects a versioned prepared helper for a two-step local/list-index += on an invocation-owned noncaptured body-local list<int> with a literal or native integer index. The compiler proves the operation shape; runtime still checks current slot identity/lifetime, readonly state, list type/validity/uniqueness, actual element metadata, bounds and inline operands/result. Inline success updates the existing entry without a new owner or scan-count change. Every miss uses the ordinary compound implementation with the original borrowed/consumed RHS rules. Root reload, exception dispatch, cleanup, debugger reflection and cancellation remain. Parameters, captured/reference roots, nested paths, boxed indices and other operations retain their existing lowering. IR/JIT lowering is unchanged.

Fresh optimized before/after measurements use parent 052e1afb5d, Release -O3 -DNDEBUG with allocation tracking off, qcc -O3/LLVM 22.1.8 on Apple M4 Pro/macOS 27.0.1. All 67 actual-module kernels use identical source before/after, frozen compiler/runtime sets and the same explicit RPATH. Paired randomized runs use default/reverse/shuffle-487 layouts, 4096 items, one warmup and three 150 ms samples, three screening forks and five confirmation forks. The prospective gate requires list mutation to improve at least 5% in aggregate and improve in every layout in both phases; control costs are assessed under the user-approved broader tradeoff. Values are whole-kernel ns/item, not isolated operation latency.

|!Confirmed workload|!Before ns/item|!Candidate ns/item|!Change
|micro:list_mutate|195.930282|143.523896|-26.75%
|micro:list_build|29.382242|28.967207|-1.41%
|micro:list_read|2.922190|2.926492|+0.15%
|micro:indirect_call|62.629475|63.043597|+0.66%
|micro:closure_call|19.356970|19.377864|+0.11%
|mixed:csv_report|954.531799|950.681562|-0.40%
|mixed:object_orders|1761.703249|1748.780178|-0.73%
|mixed:callback_pipeline|124.646089|125.853718|+0.97%
|mixed:text_index|1477.094601|1480.205078|+0.21%
|probe:effect_loop|454.053409|458.216224|+0.92%
|probe:two_read_loop|495.139802|496.524863|+0.28%

Retain under the user-approved broader tradeoff. List mutation improves 26.04% in screening and 26.75% in confirmation, in every layout in both phases; confirmation saves 52.406386 ns/item (195.930282 to 143.523896). Short one-item and 16-item invocations improve 2.66% and 12.38%; the 5% gate applies to the long screening/confirmation phases, not the short diagnostic. Repeated positive aggregate control changes are all disclosed below and range from 0.108% to 1.953% in confirmation. The largest percentage cost is object_loop (+7.742320 ns/item, 1.953%); effect_loop adds 4.162815 ns/item (0.917%), two_read_loop 1.385061 ns/item (0.280%), and callback_pipeline 1.207630 ns/item (0.969%). These are observed costs, not established as noise or attributed to a specific layout effect. Other mixed workloads change by -0.403% (CSV), -0.734% (orders), and +0.211% (text). At count 16, list_build adds 0.442608 ns/item (0.205%) and typed_local_int_unary adds 0.509422 ns/item (1.175%); both improve at count 1. No layout in any phase meets the predeclared >3% disjoint-fork-range flag. The large repeatable targeted gain and modest measured control costs justify this narrow path, without claiming an overall weighted application gain, Java parity or a performance ceiling.

|!Repeated aggregate control cost|!Screen change|!Confirmation change|!Confirmation ns/item cost
|float:callback_local_float_result|+0.318%|+0.980%|+0.008014
|float:typed_local_int_result|+0.224%|+0.157%|+0.001249
|micro:closure_call|+0.335%|+0.108%|+0.020895
|micro:monomorphic_call|+0.057%|+0.126%|+0.005339
|mixed:callback_pipeline|+0.595%|+0.969%|+1.207630
|mixed:text_index|+1.336%|+0.211%|+3.110477
|probe:effect_loop|+1.113%|+0.917%|+4.162815
|probe:object_loop|+0.478%|+1.953%|+7.742320
|probe:two_read_loop|+1.244%|+0.280%|+1.385061
|probe:two_read_short|+0.435%|+0.163%|+2.733635

|!Short target/control|!Count|!Before ns/item|!Candidate ns/item
|micro:list_mutate|1|3463.719192|3371.427055
|micro:list_build|1|3031.961165|3019.819144
|float:typed_local_int_unary|1|712.599345|702.988884
|micro:list_mutate|16|400.105026|350.584429
|micro:list_build|16|216.071482|216.514090
|float:typed_local_int_unary|16|43.362635|43.872057

Short invocations include fixed dispatch, setup and cleanup costs. Separate profiles observe generic mutation/navigation before and the prepared helper after; sampling does not establish exact hit counts. All three full machine functions shrink from 849 to 827 instructions, retaining root reload and cancellation sites; function sizes are not executed instruction counts or speed estimates. The final helper keeps one actual-element-type query on success. Compile-time QORE_DISABLE_AOT_PREPARED_INDEXED_INT_ADD and compile/runtime QORE_DISABLE_AOT_PREPARED_LOCAL_RELOAD controls qualify the ordinary path.

All 391 standard-library qmods total 344786872 bytes before and 344803496 after (+16624 bytes); all hashes change with the internal header identity, which does not imply 391 performance gains. Only QUnit imports this narrow helper in this standard-library build. Release 45 cases/2075 assertions, Debug 36 cases/3523 assertions, 2188 native checks, 1110 standard and 7704 prepared numerical references pass. One Release-only deterministic-DGC-wait skip runs successfully in Debug. All 391 Release aliases/hashes restored. All 2244 timing jobs/6732 windows independently replayed. Shared host, without affinity/frequency/thermal controls; no owned build/test/profile overlaps timing. No fresh Java/develop comparison, production-wide weighted speedup or global performance ceiling claim.

Preparation history is retained: an initial compile cast correction, an added fixture expectation corrected against AST behavior, recognition of the documented Release-only DGC skip, and a pre-timing helper refinement removing redundant generic integer conversions/setter/type calls. The final source was requalified from scratch. An initial module preparation passed correctness but failed the equal-RPATH preflight because libdir is not an AOTLinkConfig field. All comparison modules were freshly recompiled with emit_rpath=0 and an explicit common RPATH before any timing. No timing from failed preparations is pooled. Probe controls were adapted from a historical standalone fixture by namespace qualification and exporting three dynamic targets; their current module timings are not numerically compared with historical standalone runs.

[Audit](../bench/runtime-comparison/audit-aot-prepared-indexed-int-add.md); [evidence](../bench/runtime-comparison/results-aot-prepared-indexed-int-add-macos-arm64.json).

## Optimized module checkpoint after prepared indexed integer addition (2026-10-26)

Installed branch b8d1fc2a9e is compared with pure develop 5a787725d4 using matching optimized Release builds and 53 identical public-module kernels across three layouts. Fresh optimized Java covers 45 kernels. All 6360 Qore and 675 Java reference checks pass, with C2 diagnostics and independent replay of 1089 jobs/3267 measured windows. Whole-kernel results, layout costs, Java inlining policy and limitations are retained in the [checkpoint evidence](../bench/runtime-comparison/results-post-indexed-module-macos-arm64.json) and [62-check audit](../bench/runtime-comparison/audit-post-indexed-checkpoint.md). No universal Java parity or overall performance ceiling is inferred.

Next evaluate prepared immediate local reload after mutation while preserving evaluation, ownership, reflection, cleanup ordering and cancellation.

## Prepared immediate local reload (2026-10-26)

AOT immediate reloads reuse invocation-owned prepared storage for eligible body locals. Actual assigned strong-list node storage returns the same node with exactly one acquired owner, omitting ordinary local lookup and two evaluator calls. A null prepared slot keeps generic lookup; references, weak references and all other representations retain the existing live evaluator and loaded-reference ownership protocol. No pending-exception early exit or list-validity check changes load semantics. Old tracker/deferred owners are captured before loading; cache/tracker/deferred publication, deferred release and epoch publication keep their order. Parameters, captures, self, top-level and IR-only exclusions remain. No public calling-convention or IR/JIT lowering change.

Optimized baseline b8d1fc2a9e and parent aa471fd56c have identical production sources; the intervening commit contains only the recorded Java/develop checkpoint. Fresh private-module before/after runs use Release -O3 -DNDEBUG with allocation tracking off, qcc -O3/LLVM 22.1.8, Apple M4 Pro/macOS 27.0.1. Both variants compile identical sources with equal explicit RPATH; exact loaded images are checked. The 67-workload screen has three forks; confirmation has five. Each uses default/reverse/shuffle-487 layouts, 4096 items, one warmup and three 150 ms samples. The prospective gate requires at least 5% mutation improvement and improvement in every layout in both phases; controls follow the user-approved broader tradeoff. Values are whole-kernel ns/item.

|!Confirmed workload|!Before ns/item|!Candidate ns/item|!Change
|micro:list_mutate|149.953661|138.148504|-7.87%
|micro:list_build|29.506592|29.295277|-0.72%
|micro:list_read|2.976833|2.978393|+0.05%
|float:typed_local_int_unary|1.203787|1.200822|-0.25%
|micro:indirect_call|63.757668|63.389066|-0.58%
|micro:closure_call|19.426148|19.828503|+2.07%
|micro:closure_capture|25.073877|25.459506|+1.54%
|mixed:csv_report|973.292301|981.128358|+0.81%
|mixed:object_orders|1848.027886|1841.080729|-0.38%
|mixed:callback_pipeline|127.719558|128.153612|+0.34%
|mixed:text_index|1521.225179|1516.221110|-0.33%

Retain under the user-approved broader workload tradeoff: mutation improves 7.54% in screening and 7.87% in confirmation, with gains in every layout. This is not a universal improvement. Ordinary closures cost 2.071% (+0.402356 ns/item) and captured closures 1.538% (+0.385629 ns/item) in the confirmed aggregate. Captured closures also have a repeated disjoint-range default-layout regression: 6.401% screen and 5.939% confirmation (+1.502222 ns/item confirmed). All seven closure/indirect-call LLVM bodies are exactly unchanged in each layout, but the cause of the cost is not established and it is not dismissed as noise. The four mixed controls confirm -0.376% orders, -0.329% text, +0.805% CSV (screen improved) and +0.340% callback pipeline (repeated). Mutation improves 0.919% at count1 and 3.381% at count16. Short controls cost 0.630%/0.722% for list construction and 1.225%/0.011% for typed integer calls at counts1/16. The retention gate applies to long screen/confirmation; short results include dispatch/setup/cleanup and all costs are disclosed. No weighted application-wide benefit or performance ceiling is claimed.

|!Repeated aggregate control cost|!Screen change|!Confirmation change|!Confirmation ns/item cost
|micro:closure_call|+1.131%|+2.071%|+0.402356
|micro:monomorphic_float_call|+2.190%|+1.778%|+0.077946
|micro:closure_capture|+0.799%|+1.538%|+0.385629
|float:direct_loop_float_return|+0.317%|+0.377%|+0.022361
|mixed:callback_pipeline|+0.078%|+0.340%|+0.434054
|micro:indirect_float|+0.040%|+0.279%|+0.171183
|micro:boxed_int_method_effect|+0.684%|+0.215%|+0.195802
|micro:method_call|+0.135%|+0.036%|+0.000617

|!Short target/control|!Count|!Before ns/item|!Candidate ns/item
|micro:list_mutate|1|3434.962991|3403.399624
|micro:list_build|1|3060.255901|3079.545639
|float:typed_local_int_unary|1|713.983286|722.732508
|micro:list_mutate|16|361.614094|349.389479
|micro:list_build|16|221.277067|222.873582
|float:typed_local_int_unary|16|45.440624|45.445730

Short invocations include dispatch, setup and cleanup. The compile-time QORE_DISABLE_AOT_PREPARED_IMMEDIATE_RELOAD control and existing compile/runtime QORE_DISABLE_AOT_PREPARED_LOCAL_RELOAD control exercise fallback behavior. Separate profiles observe generic local loading before and prepared loading after. Across three layouts the complete mutation function grows from 827 to 833 ARM64 instructions; its optimized LLVM differs only at the immediate-load call. The runtime helper directly retains a strong list while preserving normal node-ref rules; all other values retain evaluation. Static sizes and sampling do not estimate speed or exact hit counts.

The 391 standard-library qmods total 344803496 bytes before and 344922552 after (+119056 bytes); 365 modules import the new helper. All hashes change with internal-header identity. Selection coverage and size changes do not establish per-module speedups. Release 52 cases/6804 assertions, Debug 43 cases/8252 assertions, 2220 native checks, 1110 standard and 7704 comparison reference results pass. The documented Release-only DGC skip passes in Debug. All 391 Release aliases/hashes are restored. All 2064 timing jobs/6192 measured windows are independently replayed. Shared host without affinity/frequency/thermal controls; no owned build/test/profile overlaps timing. No new Java/develop ratio or weighted production-wide speedup is inferred.

The first prepared-load implementation retained both evaluator calls. Its completed 67-workload screen measured 149.650431 to 145.401303 ns/item (-2.84%), missing the 5% gate; a captured-closure layout was also flagged, without confirmation. That source and all 1206 jobs/3618 windows are preserved and are not pooled with this refinement. The refined source was freshly qualified and timed. Earlier qualification preserved a bad auxiliary test-path failure, then verified same-source Release receipts and reran corrected tests; refined qualification reruns everything. An initial machine-review assertion expected an out-of-line generic loader; inspection established that the compiler inlined it. These driver corrections do not change measurements.

[Audit](../bench/runtime-comparison/audit-aot-prepared-immediate-reload.md); [evidence](../bench/runtime-comparison/results-aot-prepared-immediate-reload-macos-arm64.json).

## Nonnegative indexed-read policy lookup (2026-10-26)

The common JIT/AOT indexed-read helper queries the live negative-offset policy only for negative indices. Nonnegative indices cannot use that policy in normalizeIndex. Evaluation and pending-exception handling still happen first; negative reads retain the current runtime policy. Container dispatch, bounds, owned element acquisition, weak-reference normalization, strings and cleanup remain unchanged. No compiler, header, ABI or public calling-convention change. Actual optimized AOT modules are measured here; no separate IR/JIT timing benefit is claimed.

The frozen installed baseline is ccac5ed65e. Both variants use Release -O3 -DNDEBUG with allocation tracking off and exactly the same 15 native qmods compiled with qcc -O3/LLVM 22.1.8 on Apple M4 Pro/macOS 27.0.1. Exact loaded images, dependencies and module hashes are verified. The 73-workload screen has three forks; 33-workload confirmation has five. Default/reverse/shuffle-487 layouts use 4096 items, one warmup and three 150 ms samples. The prospective gate requires at least one positive-read target to improve >=3% aggregate and in every layout in both phases. All three pass. Values are whole-kernel ns/item, not isolated call latency.

|!Confirmed workload|!Before ns/item|!Candidate ns/item|!Change
|float:callback_local_float_result|0.826148|0.837652|+1.39%
|float:callback_local_float_return|0.876601|0.885950|+1.07%
|float:callback_loop_float_return|5.200551|5.219035|+0.36%
|float:direct_loop_float_return|5.941258|5.959429|+0.31%
|float:typed_local_int_result|0.813871|0.807593|-0.77%
|float:typed_local_int_unary|1.200885|1.200270|-0.05%
|index:binary_negative|8.513828|9.240150|+8.53%
|index:binary_positive|8.518322|7.654027|-10.15%
|index:buffer_negative|15.700107|15.821836|+0.78%
|index:buffer_positive|16.010259|15.215062|-4.97%
|index:list_negative|10.176419|10.339310|+1.60%
|index:list_positive|10.075625|9.309175|-7.61%
|micro:boxed_float_method_fallback|34.779262|34.399951|-1.09%
|micro:boxed_int_method_effect|90.971713|91.079466|+0.12%
|micro:boxed_int_method_fallback|33.466388|33.705716|+0.72%
|micro:closure_call|19.283347|19.259468|-0.12%
|micro:closure_capture|25.166528|25.437062|+1.07%
|micro:dynamic_int_result_call|11.580826|11.655708|+0.65%
|micro:indirect_call|61.569016|62.573758|+1.63%
|micro:indirect_float|61.291890|61.300650|+0.01%
|micro:list_build|29.420854|28.534753|-3.01%
|micro:list_mutate|137.007539|136.143615|-0.63%
|micro:list_read|2.977048|2.983287|+0.21%
|micro:method_call|1.689008|1.694867|+0.35%
|micro:monomorphic_call|4.295164|4.282003|-0.31%
|micro:monomorphic_float_call|4.366121|4.467277|+2.32%
|micro:optional_method_call|11.293221|11.473718|+1.60%
|micro:temporary_hash|1.714266|1.715038|+0.05%
|micro:temporary_list|1.715716|1.726585|+0.63%
|mixed:callback_pipeline|127.239896|128.384572|+0.90%
|mixed:csv_report|978.625919|964.899706|-1.40%
|mixed:object_orders|1826.212178|1842.311702|+0.88%
|mixed:text_index|1511.139323|1513.216146|+0.14%

Retain as a targeted positive-index improvement under the authorized broader tradeoff. All three targets pass the prospective >=3% and every-layout gate in both phases. Confirmation reduces positive list/binary/buffer whole-kernel time by 7.61%/10.15%/4.97%. Negative binary reads regress 8.53% (+0.726322 ns/item), with disjoint fork ranges in all layouts in both phases; this is a confirmed cost, not dismissed as noise. Negative list/buffer reads also show smaller repeated aggregate costs (+1.60%/+0.78%). List construction improves 3.01% and CSV 1.40%, while other mixed workloads have repeated +0.14% to +0.90% costs. Every repeated positive aggregate change is tabulated without a significance claim. Short count-1 gains are under 1%; count-16 list/binary gains are about 3.3%, buffer under 1%, and the short list target is not better in every layout. No universal, zero-regression, workload-weighted, Java-parity or overall-performance-ceiling claim. Next measure an explicit live-context read path to remove negative-read TLS lookup; preserve ordinary compatibility entry points and live runtime policy.

|!Repeated aggregate cost|!Screen change|!Confirmation change|!Confirmation ns/item cost
|index:binary_negative|+9.335%|+8.531%|+0.726322
|micro:monomorphic_float_call|+0.032%|+2.317%|+0.101156
|index:list_negative|+2.952%|+1.601%|+0.162891
|micro:optional_method_call|+2.393%|+1.598%|+0.180498
|micro:closure_capture|+0.212%|+1.075%|+0.270534
|mixed:callback_pipeline|+0.353%|+0.900%|+1.144676
|mixed:object_orders|+0.147%|+0.882%|+16.099524
|index:buffer_negative|+1.091%|+0.775%|+0.121729
|micro:boxed_int_method_fallback|+0.799%|+0.715%|+0.239328
|micro:dynamic_int_result_call|+3.063%|+0.647%|+0.074882
|micro:temporary_list|+0.405%|+0.634%|+0.010869
|float:callback_loop_float_return|+0.096%|+0.355%|+0.018484
|micro:method_call|+0.308%|+0.347%|+0.005859
|mixed:text_index|+0.252%|+0.137%|+2.076823
|micro:temporary_hash|+0.198%|+0.045%|+0.000773

|!Short target/control|!Count|!Before ns/item|!Candidate ns/item
|index:list_positive|1|486.074483|485.297449
|index:binary_positive|1|501.519278|496.762259
|index:buffer_positive|1|513.130840|508.505215
|micro:list_mutate|1|3445.032053|3428.325567
|index:list_positive|16|40.359742|39.029313
|index:binary_positive|16|39.597937|38.304081
|index:buffer_positive|16|47.256504|46.909772
|micro:list_mutate|16|348.507221|345.803621

Short invocations include dispatch, setup and cleanup. Separate positive/negative profiles observe the intended compatibility helper on both runtimes. Machine review verifies the nonnegative branch bypasses the policy/TLS lookup; the negative branch retains it. The full helper changes from 327 to 333 ARM64 instructions; this is neither a hot-path count nor a speed estimate. The compiler already coalesces source-level container type queries into one call, so caching the type in source is not a further demonstrated optimization.

All 391 standard-library qmods are byte-identical before/after (344922552 bytes). Release 110 reported cases/2462 assertions include eight documented Linux-only RSS skips and one Debug-only DGC skip; Debug 101 reported cases/3910 assertions include eight RSS skips. These reported case totals include skips. Native checks pass 2276/2276. Standard 1110 and common-module 8424 reference results pass. All Release aliases/hashes are restored. All 2448 timing jobs/7344 measured windows are independently replayed. The first baseline fixture used an invalid empty-binary literal; it was corrected to binary() and passed before production edits. The first qualification wrapper rejected documented platform skips despite successful child exits; v2 recognizes only those exact skips, verifies reused same-source Release receipts and runs remaining Release/all Debug checks. All failures remain archived.

Shared host without affinity/frequency/thermal controls; no owned build/test/profile overlaps timing. No fresh Java/develop ratio or weighted application-wide speedup is inferred. The next experiment will reuse an available live RuntimeConfig for negative reads while retaining legacy entry points and null-context fallback.

[Audit](../bench/runtime-comparison/audit-aot-indexed-read-policy.md); [evidence](../bench/runtime-comparison/results-aot-indexed-read-policy-macos-arm64.json).

## Indexed reads with an existing live runtime context (2026-10-26)

JIT/AOT lowering passes an existing native or loop RuntimeConfig to a versioned internal indexed-read helper. It acquires no context solely for a read. Negative indices read current ThreadData policy through that view after container evaluation and pending-exception handling. Nonnegative indices still omit policy lookup. The compatibility entry and null-context fallback preserve existing behavior; QORE_DISABLE_LLVM_INDEX_RUNTIME_CONFIG selects the legacy entry at compile time. Container dispatch, signed bounds, ownership, weak references, strings, cancellation and cleanup remain unchanged. The private helper ABI is additive; no public calling convention changes. Actual optimized AOT modules are measured; no separate IR/JIT timing claim.

The frozen installed baseline is 75fc3c08f5. Both variants use Release -O3 -DNDEBUG with allocation tracking off, fresh corresponding qcc -O3/LLVM 22.1.8 native modules from identical sources and equal explicit RPATHs, on Apple M4 Pro/macOS 27.0.1. Loaded images, dependencies and hashes are verified. The 73-workload screen uses three forks; 34-workload confirmation uses five. Default/reverse/shuffle-487 layouts use 4096 items, one warmup and three 150 ms samples. The prospective gate requires at least one negative-read target to improve >=3% aggregate and in every layout in both phases. All three pass. Values are whole-kernel ns/item, not isolated call latency.

|!Confirmed workload|!Before ns/item|!Candidate ns/item|!Change
|float:callback_local_float_result|0.836942|0.832073|-0.58%
|float:callback_local_float_return|0.875591|0.877121|+0.17%
|float:callback_loop_float_return|5.186970|5.213218|+0.51%
|float:direct_loop_float_return|5.936663|5.962985|+0.44%
|float:typed_local_int_result|0.810842|0.803756|-0.87%
|float:typed_local_int_unary|1.201429|1.201043|-0.03%
|index:binary_negative|9.353709|8.298802|-11.28%
|index:binary_positive|7.794119|7.696115|-1.26%
|index:buffer_negative|16.270001|15.158883|-6.83%
|index:buffer_positive|15.142781|15.194391|+0.34%
|index:list_negative|10.408925|9.265543|-10.98%
|index:list_positive|9.292664|9.336514|+0.47%
|micro:boxed_float_method_fallback|34.814794|35.477210|+1.90%
|micro:boxed_int_method_effect|91.489524|92.913768|+1.56%
|micro:boxed_int_method_fallback|33.630720|33.810618|+0.53%
|micro:closure_call|19.739115|19.508390|-1.17%
|micro:closure_capture|25.370648|25.241328|-0.51%
|micro:dynamic_int_result_call|11.730832|11.253120|-4.07%
|micro:dynamic_int_result_fallback|21.477529|22.019421|+2.52%
|micro:indirect_call|63.747559|64.281169|+0.84%
|micro:indirect_float|61.384549|61.996909|+1.00%
|micro:list_build|28.613697|28.680112|+0.23%
|micro:list_mutate|137.043435|137.249844|+0.15%
|micro:list_read|2.970901|2.985562|+0.49%
|micro:method_call|1.693549|1.695225|+0.10%
|micro:monomorphic_call|4.271318|3.962312|-7.23%
|micro:monomorphic_float_call|4.503318|4.263762|-5.32%
|micro:optional_method_call|11.563693|11.594826|+0.27%
|micro:temporary_hash|1.722569|1.726425|+0.22%
|micro:temporary_list|1.719165|1.719285|+0.01%
|mixed:callback_pipeline|128.493435|129.799466|+1.02%
|mixed:csv_report|980.903492|970.062791|-1.11%
|mixed:object_orders|1839.276220|1859.330241|+1.09%
|mixed:text_index|1519.988064|1513.387994|-0.43%

Retain under the authorized broader workload tradeoff. All three negative-read targets pass the prospective >=3% and every-layout gate in screen and confirmation. Confirmed negative list/binary/buffer time improves 10.98%/11.28%/6.83%; positive reads change +0.47%/-1.26%/+0.34%. Dynamic-int-result fallback has a repeated reverse-layout regression with disjoint fork ranges: +3.84% screen and +3.55% confirmation (+0.762598 ns/item), with +2.52% confirmed aggregate cost. Boxed-float fallback costs +1.90% aggregate and is flagged in the reverse layout only in confirmation (+3.51%). Eight selected call-control LLVM bodies, including these fallbacks and observed gain cases, are byte-identical before/after in every layout; timing changes are observed, their cause is not established and they are not dismissed as noise. Mixed workloads confirm CSV -1.11%, text -0.43%, callback pipeline +1.02%, orders +1.09%; the last two are repeated costs. Short count1 negative-list time costs0.93%, while negative binary/buffer improve2.71%/2.20%; count16 negative list/binary/buffer improve2.64%/4.15%/2.55%. Short mutation costs1.03%/0.94% at counts1/16, with a count16 shuffle-layout flag(+3.22%). Every repeated positive aggregate change and short result is reported. No weighted application-wide, universal zero-regression, fresh Java/develop ratio or performance-ceiling claim. Next refresh the optimized cumulative Java/develop checkpoint, then investigate generic scalar list-entry ownership overhead.

|!Repeated aggregate cost|!Screen change|!Confirmation change|!Confirmation ns/item cost
|micro:dynamic_int_result_fallback|+1.037%|+2.523%|+0.541892
|micro:boxed_float_method_fallback|+2.202%|+1.903%|+0.662416
|micro:boxed_int_method_effect|+2.153%|+1.557%|+1.424245
|mixed:object_orders|+0.071%|+1.090%|+20.054021
|mixed:callback_pipeline|+1.237%|+1.016%|+1.306031
|micro:indirect_float|+1.065%|+0.998%|+0.612360
|micro:indirect_call|+1.938%|+0.837%|+0.533609
|micro:boxed_int_method_fallback|+0.887%|+0.535%|+0.179898
|float:callback_loop_float_return|+0.144%|+0.506%|+0.026248
|micro:list_read|+0.257%|+0.493%|+0.014661
|index:list_positive|+0.831%|+0.472%|+0.043850
|float:direct_loop_float_return|+0.120%|+0.443%|+0.026322
|micro:optional_method_call|+1.105%|+0.269%|+0.031132
|micro:temporary_hash|+0.246%|+0.224%|+0.003857
|micro:method_call|+0.749%|+0.099%|+0.001676
|micro:temporary_list|+0.069%|+0.007%|+0.000120

|!Short target/control|!Count|!Before ns/item|!Candidate ns/item
|index:list_negative|1|494.877518|499.477812
|index:binary_negative|1|508.567294|494.786624
|index:buffer_negative|1|526.215209|514.657530
|index:list_positive|1|496.335735|491.113240
|index:binary_positive|1|506.320089|500.117727
|index:buffer_positive|1|505.983435|506.784648
|micro:list_mutate|1|3449.633572|3485.166783
|index:list_negative|16|41.297978|40.209402
|index:binary_negative|16|40.851047|39.155299
|index:buffer_negative|16|48.286122|47.056294
|index:list_positive|16|40.704604|39.520055
|index:binary_positive|16|39.586616|38.727280
|index:buffer_positive|16|46.423512|46.216388
|micro:list_mutate|16|347.426351|350.690355

Short invocations include dispatch, setup and cleanup. Separate profiles observe the intended helper and loaded images. In all six selected positive/negative fast bodies, LLVM text is identical after replacing only the helper call and adding the existing context argument. ARM64 function sizes grow by one instruction for the extra argument load; existing context acquisition and cancellation remain unchanged. Full helper instruction/static-call counts are 333/50 before, 341/50 candidate compatibility, and 318/46 explicit context; these are not hot-path counts or speed estimates. Only the explicit negative-policy lookup avoids TLS discovery; nested evaluation and other helper work may still use TLS. Eight selected call-control LLVM bodies are identical across variants and layouts; their timing changes have no established cause.

All 391 standard-library qmods change hash with the internal header identity; total bytes grow from 344922552 to 344952008 (+29456). No import-coverage or per-module speed claim. Release 111 reported cases/2560 assertions include eight Linux-only RSS skips and one Debug-only DGC skip; Debug 102 cases/4008 assertions include eight RSS skips. Case totals include skips. Native checks pass 2388/2388. An additional 196 assertions compare legacy substring indexing and explicit result types against AST in Release and Debug, both offset policies and all fixture modes. Generated supplementary sources are archived; measured sources/runtimes stay unchanged. Standard 1110 and prepared 8424 reference results pass. All Release aliases/hashes restored; all 2586 timing jobs/7758 measured windows independently replayed.

Shared host without affinity/frequency/thermal controls; no owned build/test/profile overlaps timing. No fresh Java/develop ratio or weighted application-wide speedup is inferred. Next refresh the optimized cumulative Java/develop checkpoint, then investigate generic scalar list-entry ownership overhead.

[Audit](../bench/runtime-comparison/audit-aot-indexed-read-context.md); [evidence](../bench/runtime-comparison/results-aot-indexed-read-context-macos-arm64.json).

## Optimized module checkpoint after live-context indexed reads (2026-10-26)

Installed branch 223ff981c9 is compared with pure develop 5a787725d4 using matching optimized Release builds and 53 identical public-module kernels across three layouts. Fresh optimized Java covers 45 kernels. All 6360 Qore and 675 Java reference checks pass, with C2 diagnostics and independent replay of 1089 jobs/3267 measured windows. Whole-kernel results, layout costs, Java inlining policy and limitations are retained in the [checkpoint evidence](../bench/runtime-comparison/results-post-live-index-module-macos-arm64.json) and [62-check audit](../bench/runtime-comparison/audit-post-live-index-checkpoint.md). No universal Java parity or overall performance ceiling is inferred.

Next evaluate generic scalar list-entry ownership overhead while preserving enum, opaque, node and weak-reference lifetime, evaluation, bounds, exceptions and cancellation.

## Inline scalar indexed reads (2026-10-26)

The common JIT/AOT indexed-read helper borrows the actual list entry to test needsRef(). Inline non-owning values return directly without element reference acquisition or weak-type normalization. Enum declarations, opaque values and all other owners retain the original getReferencedEntry protocol, including its entry reread after virtual reference handlers, then existing weak normalization. The evaluated container remains owned throughout; pending sinks, live negative-offset policy, signed bounds, strings, binary/buffer dispatch and cleanup are unchanged. No compiler/header/helper ABI change. Measurements use actual optimized AOT modules; no separate IR/JIT timing claim.

The frozen installed baseline is 223ff981c9; parent 0405d53d93 differs only in checkpoint evidence. Both variants use Release -O3 -DNDEBUG, allocation tracking off, and exactly the same 15 qcc -O3/LLVM22.1.8 native modules on Apple M4 Pro/macOS 27.0.1. Exact images and dependencies are verified. The 73-workload screen uses 3 forks; confirmation uses 5 forks with prospectively selected controls plus screen flags and aggregate costs. Default/reverse/shuffle-487 layouts use 4096 items, 1 warmup and 3 x 150 ms measured windows. Acceptance requires at least one target improve >=3% aggregate AND >=3% in EACH layout in screen and confirmation. Values are whole-kernel ns/item, not isolated call latency.

|!Confirmed workload|!Before ns/item|!Candidate ns/item|!Change
|float:callback_local_float_result|0.825009|0.831005|+0.73%
|float:callback_local_float_return|0.869862|0.862610|-0.83%
|float:callback_loop_float_return|5.034913|5.034901|-0.00%
|float:direct_loop_float_return|5.779125|5.779921|+0.01%
|float:typed_local_int_result|0.801383|0.788302|-1.63%
|float:typed_local_int_unary|1.162359|1.164001|+0.14%
|index:binary_negative|8.085100|8.066249|-0.23%
|index:binary_positive|7.571499|7.595129|+0.31%
|index:buffer_negative|14.789445|14.869517|+0.54%
|index:buffer_positive|14.873287|14.727715|-0.98%
|index:list_negative|9.095301|7.819649|-14.03%
|index:list_positive|9.102430|7.832448|-13.95%
|micro:boxed_float_method_call|20.557954|21.228604|+3.26%
|micro:boxed_float_method_fallback|34.033686|34.981898|+2.79%
|micro:boxed_int_method_call|16.995486|17.824134|+4.88%
|micro:boxed_int_method_effect|90.165855|89.717573|-0.50%
|micro:boxed_int_method_fallback|33.620851|33.374418|-0.73%
|micro:closure_call|19.123257|18.962549|-0.84%
|micro:closure_capture|24.685207|24.529348|-0.63%
|micro:dynamic_int_result_fallback|21.564335|21.411209|-0.71%
|micro:indirect_call|62.168315|61.465156|-1.13%
|micro:indirect_float|59.986186|59.184509|-1.34%
|micro:list_build|27.442122|26.157716|-4.68%
|micro:list_mutate|133.083272|132.410440|-0.51%
|micro:list_read|2.894585|2.883782|-0.37%
|micro:method_call|1.652237|1.657228|+0.30%
|micro:monomorphic_call|3.891511|3.899745|+0.21%
|micro:monomorphic_float_call|4.147237|4.119168|-0.68%
|micro:temporary_hash|1.679408|1.679366|-0.00%
|micro:temporary_list|1.679536|1.670356|-0.55%
|mixed:callback_pipeline|125.211366|123.437713|-1.42%
|mixed:csv_report|933.328430|938.655129|+0.57%
|mixed:object_orders|1740.164444|1743.421414|+0.19%
|mixed:text_index|1453.998272|1438.748873|-1.05%
|probe:string_loop|368.066029|363.678591|-1.19%

Retain the actual-representation scalar skip under the authorized broader workload tradeoff. Both generic list targets pass >=3% aggregate AND >=3% in every layout in screen and confirmation. Confirmation reduces positive/negative read time by 13.95%/14.03%. The boxed-integer method control has a repeated aggregate cost of 4.88% (+0.828648 ns/item), with reverse/shuffle disjoint fork ranges in both phases and all layouts flagged in confirmation. Boxed-float method calls also have a repeated 3.26% aggregate cost (+0.670650 ns/item), although they do not meet the disjoint-range flag. These are retained costs, not dismissed as noise. All benchmark modules and compiler/header sources are identical across runtimes; boxed-method bodies do not call the changed indexed-read helper. Their cost mechanism is unresolved. List construction improves 4.68%, indirect integer/float calls 1.13%/1.34%, callback pipeline 1.42% and text index 1.05%; these broader observations are not attributed to removed scalar helper work. CSV has a repeated 0.57% cost; orders +0.19% does not repeat its screen direction. Every repeated aggregate cost is tabulated without a significance claim. Short count 1 list changes range -0.04% to -1.12%, and count 16 gains are 3.51%/3.85% aggregate without meeting the >=3% every-layout gate; short controls have no disjoint-range flags. No universal, zero-regression, weighted application, fresh Java-ratio or performance-ceiling claim. Next inspect remaining cross-Program RuntimeConfig synchronization with a fresh post-merge baseline.

|!Repeated aggregate cost|!Screen change|!Confirmation change|!Confirmation ns/item cost
|micro:boxed_int_method_call|+6.339%|+4.876%|+0.828648
|micro:boxed_float_method_call|+3.296%|+3.262%|+0.670650
|micro:boxed_float_method_fallback|+0.355%|+2.786%|+0.948212
|float:callback_local_float_result|+0.588%|+0.727%|+0.005996
|mixed:csv_report|+0.066%|+0.571%|+5.326700
|index:buffer_negative|+1.900%|+0.541%|+0.080073
|index:binary_positive|+0.655%|+0.312%|+0.023630
|micro:method_call|+0.713%|+0.302%|+0.004991
|float:typed_local_int_unary|+0.028%|+0.141%|+0.001642

|!Short target/control|!Count|!Before ns/item|!Candidate ns/item
|index:list_negative|1|468.455986|468.264329
|index:binary_negative|1|471.721477|471.419378
|index:buffer_negative|1|482.677295|484.549743
|index:list_positive|1|468.511363|463.276499
|index:binary_positive|1|464.634142|469.503039
|index:buffer_positive|1|485.564156|477.709209
|micro:list_mutate|1|3287.322921|3293.747131
|index:list_negative|16|38.003257|36.540878
|index:binary_negative|16|37.443819|37.571213
|index:buffer_negative|16|44.936664|44.472885
|index:list_positive|16|37.587154|36.268408
|index:binary_positive|16|36.666229|36.605333
|index:buffer_positive|16|44.349145|44.060009
|micro:list_mutate|16|336.638507|333.438518

Short invocations include dispatch, setup and cleanup. Separate profiles observe the live-context helper on both runtimes. Native machine review confirms scalar entries skip element ref/getType, while owners retain actual-entry ref/reread/type normalization. Full compatibility helper instructions increase 341 to 351 and context helper 318 to 328, with static call counts unchanged 50/46; these are neither hot-path counts nor speed estimates. The baseline already inlines getReferencedEntry. The separate typed inline-list-data lowering is a control, not a presumed beneficiary.

All 391 standard-library qmods are byte-identical (344952008 bytes). Release 117 reported cases/3299 assertions include 8 Linux-only RSS skips and 1 Debug-only DGC skip; Debug 108 cases/4747 assertions include 8 RSS skips. Case totals include skips. Native 2604 checks and standalone optimized baseline/candidate 216 checks each pass. Standard 1110 and prepared 8424 references pass; all Release aliases/hashes restored. All 2616 timing jobs/7848 windows independently replayed.

The initial baseline input used reserved identifier elements; renaming it preceded production edits. Initial native qualification had 6 failures because normal list insertion normalizes opaque tags; both runtimes exhibited the same behavior. Corrected fixtures explicitly store opaque representations and verify stored bits. Two diagnostic harness compile attempts lacked internal declarations; a Release builtin attempt found the Debug-only registration unavailable. All failures and corrections are archived. An unmeasured first implementation referenced a copied owned value; final code preserves the original getReferencedEntry protocol around virtual customRef. Only the final candidate is timed.

Shared host without affinity/frequency/thermal control; no owned build/test/profile overlaps timing. No fresh Java/develop ratio or weighted application speedup is inferred from this experiment. The separate post-live-index checkpoint compares 223ff against develop/Java. Dynamic and cross-Program calls remain optimization leads; neither Java parity nor a maximum-performance ceiling is established.

[Audit](../bench/runtime-comparison/audit-aot-indexed-read-scalar.md); [evidence](../bench/runtime-comparison/results-aot-indexed-read-scalar-macos-arm64.json).

## Develop integration through eb6af43dbf (2026-10-26)

Merged 30 upstream commits onto scalar-read checkpoint 04a948519b. Five conflicted files were resolved. SnapshotInt/SnapshotFloat retain 409/410, CheckReturnValue remains 411, and IteratorRelease/ReleaseOwnedTemp are appended as 412/413. Upstream v21 maximum 409 or 411 identifies its return-check encoding; maximum 411 also remaps its two cleanup encodings. Performance v21 maximum 410 and v22 snapshots keep their meanings. Fifteen actual serializer/reader round trips cover these histories. Format 22 payload boundaries remain unchanged; the maximum-opcode guard rejects newer instructions on older readers.

Cancellation keeps the sequentially consistent no-pending-interrupt guard and explicitly resolves the governing All context in its slow path, preserving upstream policy-barrier behavior. The upstream schema-digest fixture failed on this host. All 276 full action/type digests agree across merged native/source, upstream source on merged and prior runtimes, and a fresh unmodified optimized develop runtime. The generated fixture is refreshed to those observed upstream results; the initial failure and comparison evidence are retained. Program deletion retains its new deferred-destruction rules, exception sink and caller-context restoration; no lifetime count or lock is omitted.

Full Release -O3 -DNDEBUG build with allocation tracking off and all 391 qmods passes. Release 141 reported cases/3762 assertions and Debug 42 cases/3171 assertions pass. Five Release-only probes are skipped because they require library-debug instrumentation; all five execute in Debug. Native unit checks pass 2604/2604; 1110 IR/AOT reference results are independently replayed. Selected integration tests cover scalar snapshots/reads, prepared mutations, fast-call context and bindings, new temporary/loop-local lifetimes, running-Program deletion, cancellation, AOT defaults/typedefs, schema digests/directions/defaults and REST endpoints. Debug testing restores all 391 Release aliases and hashes.

These are integration correctness checks, not post-merge performance measurements. The preceding scalar-read results belong to 04a948519b before these compiler/runtime changes. A fresh installed merged baseline is required before further optimization; neither Java parity nor maximum performance is established. No full-suite, sanitizer, Linux-only, external module rebuild or JVM-host result is claimed.

[Audit](../bench/runtime-comparison/audit-merge-eb6a.md); [evidence](../bench/runtime-comparison/results-merge-eb6a.json).

## Explicit thread ID at Program entry (2026-10-26)

ProgramThreadCountContextHelper reuses its live ThreadData ID in internal parsingLocked and incThreadCount overloads. Generic callers keep q_gettid, including its shutdown fallback. No counts, locks, context fields, debugger behavior or caller-first deferred teardown are removed. Optimized machine review verifies the two avoided TLS discoveries; canonical synchronization still uses TLS.

Retain under the authorized broader workload tradeoff. All four indirect-call targets pass the prospective aggregate and every-layout gates in both phases. Confirmation improves integer, float, wide and affine-wide calls by 4.69%, 3.59%, 4.27% and 4.70%, saving 2.23–3.02 ns/item. Captured closures have a repeated 2.54% confirmation cost (+0.637337 ns/item), and ordinary closures a repeated 0.38% cost (+0.073162 ns/item). Smaller repeated costs are reported without significance claims. No workload meets the predeclared disjoint-range adverse flag in either phase. List building improves 1.35% and the callback pipeline 1.71%; these broader changes are observations, not attributed to removed TLS work. List mutation (+0.75%) and CSV (+0.64%) reverse their screening direction. Count-1 indirect integer/float workloads cost 0.74%/0.35%, while count-16 improves 1.26%/0.68%; these include dispatch/setup and do not pass the steady-state all-layout gate. No zero-regression, weighted application or universal speedup claim.

The earlier canonical RuntimeConfig pointer candidate is not retained because its repeated closure costs were not offset by broad workload gains. Native coverage now includes copied/canonical snapshots, nested and ancestor reentry, balanced/no-op counts, current Impl replacement, locations and pending exceptions. See [measurements](../bench/runtime-comparison/results-program-entry-tid-macos-arm64.json) and [audit](../bench/runtime-comparison/audit-program-entry-tid.md). No fresh Java ratio or global ceiling is established.

## Optimized module checkpoint after Program entry thread-ID reuse (2026-10-26)

Installed branch cbd16d80f5 is compared with pure develop eb6af43dbf using matching optimized Release builds and 73 identical public-module kernels across three layouts. Fresh optimized Java covers 45 kernels. All 8424 Qore and 675 Java reference checks pass, with C2 diagnostics and independent replay of 1449 jobs/4347 measured windows. Whole-kernel results, layout costs, Java inlining policy and limitations are retained in the [checkpoint evidence](../bench/runtime-comparison/results-post-program-tid-module-macos-arm64.json) and [62-check audit](../bench/runtime-comparison/audit-post-program-tid-checkpoint.md). No universal Java parity or overall performance ceiling is inferred.

Next revisit the existing compiler proof of exact dynamic-closure signatures under the authorized broader tradeoff, with fresh before/after measurements and list-building/mixed controls. Preserve live Program/body/readiness/debugger/stack guards, frames, synchronized captures, exceptions and cancellation.

## Compiler-proven AOT closure signatures (2026-10-26)

Native scalar closure dispatch trusts the exact builtin int(int), non-substituted signature proved when its companion is emitted. Publication remains bound to the matching body; live readiness, Program, debugger, stack, exception, lexical context and capture checks remain. Debug asserts the proof; QORE_DISABLE_AOT_CLOSURE_SIGNATURE_PROOF rechecks it at runtime. No ABI, context layout, runtime identity or generated kernel instructions change.

Retain under the user-authorized broader workload tradeoff. Plain and captured closures improve 3.097% and 3.468% in confirmation, with gains in every layout in both phases. Polymorphic integer calls cost 1.847% (0.454 ns/item) and polymorphic float calls cost 4.599% (1.128 ns/item); both have repeated adverse reverse-layout flags. Ordinary indirect float/wide calls cost 0.661%/0.898%, and other repeated smaller costs are retained below. The historical list-building regression does not repeat: current confirmation improves 0.702%. Mixed workloads show small changes, including CSV +1.380% after a flat screen. This is a targeted closure improvement with observed costs, not a universal application speedup.

See [results](../bench/runtime-comparison/results-aot-closure-signature-proof-macos-arm64.json) and [audit](../bench/runtime-comparison/audit-aot-closure-signature-proof.md). This is a targeted AOT gain; no fresh Java ratio or global ceiling is established. Empty callback-context application is the next isolated experiment.

## Empty callback-context application (2026-10-26)

The callback context guard keeps the existing getter and invokes apply only for a nonnull referenced context. Its null path avoids an out-of-line call; populated context ownership, registry locking, trusted boundaries and restoration remain unchanged. No ABI or context-layout change.

Retain under the user-authorized broader workload tradeoff. Plain/captured closures improve 3.395%/2.930% in confirmation; both improve in every layout in screen and confirmation. Polymorphic integer/float controls improve 3.771%/3.394%. Ordinary indirect call/float/wide/affine-wide controls cost 1.621%/0.657%/1.019%/1.735%, or 0.995/0.395/0.626/1.062 ns/item. Mixed confirmation changes are CSV -0.232%, orders +1.003%, callback pipeline +0.041% and text +0.527%; no weighted application gain is inferred. Populated-context confirmation has small repeated named-callback costs: 0.107% ordinary and 0.467% float; the screen captured-closure cost does not repeat. Retain the consistent empty-context closure and polymorphic improvements with these costs explicitly recorded. No causal claim is made that code placement explains the other workload changes.

See [results](../bench/runtime-comparison/results-empty-callback-context-macos-arm64.json) and [audit](../bench/runtime-comparison/audit-empty-callback-context.md). AOT timings establish a targeted improvement, not Java parity or a global ceiling. Decode-once closure admission is the next isolated candidate; explicit stack-limit/location-slot arguments remain a later calling-convention hypothesis.

## Decode-once closure admission (2026-10-26)

Known-closure miss dispatch decodes its borrowed live node once through existing value accessors. Pointer values bypass both general accessor calls; enum/inline/opaque fallback semantics and all native live guards remain. No ownership, helper ABI or generated-code change.

Retain under the user-authorized broader workload tradeoff. Plain/captured closures improve 9.828%/8.367% in confirmation, with both improving in every layout in screen and confirmation. Ordinary indirect integer/float/wide/affine-wide controls improve 1.846%/0.281%/1.239%/1.757%. Populated-context plain/captured closures improve 6.961%/7.474%; all six populated controls improve in confirmation. Repeated main-control costs include typed local integer result 3.046% (0.025514 ns/item), callback local float return 0.667% (0.005887 ns/item), typed local integer unary 0.435% (0.005344 ns/item), and CSV 1.120% (10.837132 ns/item). List-build/mutation/read screen costs do not repeat. Other confirmation-only costs, including polymorphic integer calls 1.541%, remain reported. Single-item plain/captured closures cost 0.327%/0.944%; 16-item gains are 0.441%/0.289%. Retain the consistent repeated-call gains with these costs; no weighted application gain or causal claim about unrelated code placement.

The related earlier borrowed-node candidate was rejected for a floating result callback regression; a separate historical-control confirmation here finds -0.238%. The main screen archival-search CPU overlap is disclosed and no samples removed; independent confirmation runs without that search. See [results](../bench/runtime-comparison/results-closure-node-decode-macos-arm64.json) and [audit](../bench/runtime-comparison/audit-closure-node-decode.md). No Java parity or global ceiling claim. Investigate immutable metadata lookup next; do not repeat the already-rejected stack-limit-only closure-v3 proposal unchanged.

## Concrete closure metadata (2026-10-26)

Known-closure miss dispatch uses the retained parse node and sole concrete UserClosureVariant established by parsing/AOT reconstruction. Debug validates identity against the original virtual getters. Both metadata indirect calls disappear; live admission, ownership, value decoding, context and helper ABI remain.

Retain under the user-authorized broader workload tradeoff. Ordinary plain/captured closures improve 5.974%/6.366% in confirmation, after screen gains 5.419%/4.186%; both improve in every layout in both phases. This is not a universal gain: populated-context plain/captured closures have repeated costs, with confirmation 0.795%/0.136%, and populated indirect floating calls cost 2.506%. Main repeated costs include buffer reads 1.113%, negative/positive list indexing 1.044%/1.814%, boxed integer fallback 1.424%, indirect floating calls 2.029%, monomorphic floating calls 0.953%, callback pipeline 0.083% and text indexing 0.816%. Ordinary indirect integer calls improve 1.431%, CSV 0.864% and order processing 1.021%. The static-float screen cost does not repeat; the buffer shuffle-layout flag does not repeat, though its smaller aggregate cost does. Callback local float result costs 3.783% (0.031354 ns/item) in confirmation after a screen gain; this remains reported. Short closure results do not pass the cross-layout target gate. Retain the repeated ordinary-closure gains with these measured costs, without claiming a weighted application improvement or attributing unrelated costs to placement.

See [results](../bench/runtime-comparison/results-closure-metadata-macos-arm64.json) and [audit](../bench/runtime-comparison/audit-closure-metadata.md). Refresh the optimized cumulative Java/develop comparison next. Investigate reusable invocation-owned closure-frame storage only with explicit lifetime, ABA, per-call publication and fallback proofs. No Java parity or global ceiling claim.

## Optimized module checkpoint after closure metadata optimization (2026-10-26)

Installed branch 004be48136 is compared with pure develop dc2461a769 using matching optimized Release builds and 73 identical public-module kernels across three layouts. Fresh optimized Java covers 45 kernels. All 8424 Qore and 675 Java reference checks pass, with C2 diagnostics and independent replay of 1449 jobs/4347 measured windows. Whole-kernel results, layout costs, Java inlining policy and limitations are retained in the [checkpoint evidence](../bench/runtime-comparison/results-post-closure-metadata-module-macos-arm64.json) and [62-check audit](../bench/runtime-comparison/audit-post-closure-metadata-checkpoint.md). No universal Java parity or overall performance ceiling is inferred.

Next probe reusable invocation-owned closure-frame storage after lifetime/ABA and per-call observability review. Use a compile-only probe before a bounded additive helper/compiler prototype; preserve old ABIs, live guards, synchronized captures, exceptions and cancellation. Any retained prototype needs measured gains, broader controls and full audit.

## Automatic closure-frame inlining (2026-10-26)

Explicitly inline construction and defaulted destruction of the existing closure-v2 automatic frame. Preserve all fields, admission guards, publication/readers, fallback and exception restoration. No ABI, compiler or caller-storage change.

Retain under the user-authorized broader workload tradeoff. Ordinary plain/captured closures improve 3.699%/4.197% in independent confirmation, after screen gains 4.335%/3.514%; both improve in every layout in both phases. Populated plain/captured closures improve 0.520%/2.458% in confirmation after 2.408%/2.888% in screen. Costs remain: ordinary indirect floating calls are 1.248% slower and CSV 1.100% slower in confirmation, both repeated from screen. Other repeated aggregate costs are listed below, including text indexing 0.491% and monomorphic calls 0.607%. The callback pipeline improves 0.429%; order processing costs 0.326% after a screen gain. Four adverse individual-layout screen flags do not repeat in confirmation, but this does not imply every control improves. One-item plain/captured closures cost 1.191%/0.894%; 16-item costs are 0.144%/0.662%. No short closure target passes its cross-layout gain gate. Populated indirect wide calls cost 1.388% in confirmation after a screen gain; populated floating calls cost 0.191% after a screen gain. Retain the repeated sustained-closure gains with these costs, without claiming a weighted application improvement or explaining unrelated costs by code placement.

See [results](../bench/runtime-comparison/results-closure-inline-macos-arm64.json) and [audit](../bench/runtime-comparison/audit-closure-inline.md). A corrected compile-only reusable-frame probe remains unqualified. Further work should first establish invocation-lifetime and binding proofs, with ownership, mutation, debugger and fallback behavior preserved. No Java parity or global ceiling claim.

## Closure context admission experiment rejected (2026-10-27)

Reject the candidate under the prospective sustained-call gate. Plain closures improve only 0.446% aggregate (14.604366 to 14.539198 ns/item), below the required 1%; captured closures change +0.002% (20.225536 to 20.226016), with a small default-layout cost. Plain closures improve in all three layouts, but neither target passes the aggregate threshold. No confirmation, short-call or populated-context timing is run after this failed screen, as planned. All 73 workloads, 1314 jobs and 3942 measured windows remain available. The candidate passes correctness qualification, but removing four hot-path instructions does not establish a useful speedup.

The runtime binding-time candidate and 20 C++ admission checks are restored to c583a53ac6 and were never installed. The two flag checks move as intended in actual machine code, but the performance gate fails. The installed merged Release baseline remains in use. All 73 screened workloads and host limitations are preserved in [evidence](../bench/runtime-comparison/results-closure-context-admission-macos-arm64.json) and the [audit](../bench/runtime-comparison/audit-closure-context-admission.md).

Next inspect nonvirtual closure Program lookup with ownership, Program-writer, layout and compile-only checks before a new runtime candidate. No Program equality, ownership count, capture lock or diagnostic publication may be omitted. Java parity and a global performance ceiling remain unestablished.

## Captured closure Program lookup experiment rejected (2026-10-27)

Reject under the prospective two-target gate. Plain closure calls improve 2.133% aggregate (14.536458 to 14.226415 ns/item), improving in every layout. Captured calls improve only 0.261% (20.185195 to 20.132528), below the required 1%, and their default layout is 4.715% slower. The plain target passes; the captured target fails. As planned, no independent confirmation, short-call or populated-context timing follows this failed screen. Preserve all 78 workloads, 1404 jobs and 4212 measured windows, including adverse controls. The private layout cost and inconsistent captured-call benefit do not justify retaining this candidate.

The nonvirtual owner snapshot and its C++ ownership assertions are restored exactly to f0a325b086; the candidate was never installed. It removes six static helper instructions but grows object closures by eight bytes, and captured-call performance does not pass the predefined gate. All 78 workloads, lifecycle controls and limitations are retained in [evidence](../bench/runtime-comparison/results-closure-program-lookup-macos-arm64.json) and the [audit](../bench/runtime-comparison/audit-closure-program-lookup.md).

After the queued test-only upstream merge and optimized installation, inspect captured-value immutability and complete escape/write tracking as a compiler optimization lead. No immutable-capture optimization is implemented or measured here; mutable capture synchronization and all live call/lifetime requirements remain. Java parity and a global performance ceiling remain unestablished.

Profile clarification: the new live/deleted-object controls enter scalar admission but sample generic closure fallback, unlike the primary plain/captured micro controls that sample native companions. This does not alter the failed candidate decision or any timings. Diagnose Program/context admission for those object closures before changing fast-path eligibility; a specific failed guard is not established by samples alone.

## Native closure retry after Program entry (2026-10-27)

Existing compiler-proven scalar closure companions can run after the generic call performs its full Program transition. Diagnostic frames retain caller Program identity while execution remains in the owner. Counts, locks, TLPD, capture synchronization, lifetime, live admission, stack publication and fallback remain; existing v3-v9 foreign-Program direct admission still misses. No public ABI or compiler-body change.

Retain under the authorized broader-workload tradeoff. Both object closure targets exceed the prospective 5% gate in every layout in the narrow screen, broad screen and independent confirmation. Confirmation reduces live-object calls from 197.228572 to 86.156379 ns/item (-56.316%) and deleted-object calls from 198.357880 to 85.932170 (-56.678%). Same-Program plain/captured closures change -0.053%/-0.153%; direct calls change -0.125%. Repeated aggregate control costs include indirect float +2.094% (+1.213220 ns/item), affine-wide indirect +1.986% (+1.158733), dynamic floating result +1.747% (+0.211889), plain-closure creation +1.592% (+5.103234), indirect floating result +1.343% (+0.781661), wide indirect +1.036% (+0.608285), callback pipeline +0.830% (+1.010317), list mutation +0.575%, ordinary indirect +0.106% and CSV +0.139%. Confirmation-only list construction costs 1.034%; orders/text improve 0.202%/0.931%. The broad callback-local-float-result cost of 4.023% does not repeat (confirmation -0.627%). No confirmation aggregate costs 3% or more, and no adverse disjoint-fork layout flag repeats. The approximately 111-112 ns/item object-call reduction justifies these measured costs; this is not a universal speedup. Single-item live/deleted-object invocations improve 28.133%/27.649%, and 16-item invocations improve 51.546%/52.700%. Short controls retain indirect costs +0.143%/+0.507%; at 16 items direct calls cost 0.433%, plain/object creation cost 1.906%/1.592%. Populated-context confirmation improves live/deleted-object calls 41.158%/42.777%; its plain/captured closure controls cost 0.660%/1.147% after improving in screening, with no repeated aggregate cost or flagged layout. No fresh Java/develop ratio, weighted application benefit or global performance ceiling is inferred.

See [complete measurements](../bench/runtime-comparison/results-cross-program-closure-entry-macos-arm64.json) and [62-check audit](../bench/runtime-comparison/audit-cross-program-closure-entry.md). Further work should examine duplicated pre-transition admission and binding with a typed continuation; do not skip observable Program counts or retain closures for caching. No Java parity or performance-ceiling claim.

## Generic closure entry context reuse rejected (2026-10-27)

Reject: neither target reaches the prospective 1% benefit or improves in every layout. Live-object closure calls change 85.152719 to 85.842914 ns/item (+0.810538%); deleted-object calls 85.393778 to 85.519726 (+0.147491%). Disabled-native plain and captured controls cost 4.029132% and 3.134086% aggregate, with disjoint adverse fork ranges in all three layouts. These are screening observations, not independently confirmed cost estimates. No broad, confirmation, short, populated or full Release/Debug qualification follows the failed primary gate.

The single JITRuntime.cpp edit is restored exactly to c467ccd013 and was never installed. A smaller linked function is not evidence of a speedup. See all ten workloads, 180 jobs/540 windows and the limited qualification in [results](../bench/runtime-comparison/results-closure-entry-context-macos-arm64.json) and [audit](../bench/runtime-comparison/audit-closure-entry-context.md). Next inspect earlier foreign-Program admission rejection with live-context validity and pure-miss proofs; preserve full Program transition and all generic controls. No Java parity or maximum-performance claim.

## Pure native closure admission before context setup (2026-10-27)

Native closure metadata/body/entry validation and the live foreign-Program rejection now precede temporary captured-context setup. None can execute Qore. RuntimeConfig access retains its prior metadata preconditions; every actual call retains context, full Program entry, lifetime, capture synchronization and caller diagnostic identity. Three unlikely-branch hints preserve ordinary-path inlining in the measured build; an unhinted variant with adverse same-Program costs is preserved as superseded. No ABI, object-layout, compiler-body or test-source change.

Retain the refined three-hint candidate under the authorized broader workload tradeoff. Five-fork confirmation improves live/deleted-object calls 1.994%/2.883%, same-Program plain/captured closures 1.406%/2.134%, and populated object calls 20.616%/20.929%. Both primary targets pass the prospective 1% aggregate and every-layout gate, including short controls. Costs remain: main confirmation includes callback-local-float-return +1.428%, text indexing +1.246%, monomorphic calls +1.171%, indirect float +1.150% and indirect float result +1.110%; all repeated smaller costs are recorded. CSV +1.045%, orders +1.089%, captured/plain construction +1.586%/+0.776% do not repeat the broad direction. Populated plain/captured closure controls cost 1.160%/1.276%; short captured controls cost 0.575%/0.844%; generic plain/live-object controls repeat smaller costs of 0.369%/0.215%, and generic captured calls cost 0.383% after a faster screen. No confirmation aggregate costs>=3% and no confirmation disjoint-range flags remain. These are whole-workload observations on a shared host, not component-cost attribution or an application-weighted overall speedup.

See [all measurements and initial probes](../bench/runtime-comparison/results-closure-foreign-admission-macos-arm64.json) and [62-check audit](../bench/runtime-comparison/audit-closure-foreign-admission.md). Next refresh the optimized cumulative checkpoint and examine duplicate admission in typed closure fallback, preserving argument compatibility misses and post-transition retry. Java parity and a global performance ceiling remain unestablished.

## Optimized cumulative checkpoint after native closure admission (2026-10-27)

All 78 branch aggregates are faster than unmodified develop, with no adverse disjoint-range layout flag. Mixed CSV, orders, callback pipeline and text workloads reduce time by 14.033%, 14.306%, 63.293% and 14.954%. Known integer callbacks take 1.181535 ns/item versus Java 0.891199 (1.326x); direct calls 1.573034 versus 0.893707 (1.760x). Same-Program plain/captured closure kernels take 14.277509/19.783361 versus Java 1.663967/1.731255 (8.580x/11.427x). Monomorphic methods take 3.814136 versus 1.670283 (2.284x); polymorphic methods 23.343311 versus 2.518512 (9.269x). Module indirect calls take 58.558954 versus develop 263.120766, but the fresh live-Program diagnostic confirms a cross-Program transition, so their 35.362x ordinary-Java ratio is not a same-Program overhead ratio. Live/deleted-object closure calls take 83.278397/83.488772 versus develop 222.033165/225.177469. List mutation remains 132.593188 versus develop 242.393260 and Java 4.253547. These are cumulative workload observations, not a weighted application score or evidence that all call categories approach Java. Incremental control costs from retained experiments remain documented.

Matching optimized builds compare installed branch 86c0d7759a with pure develop 8dc8bdf6fb, covering 78 Qore workloads and 45 fresh optimized Java counterparts. All 9024 Qore/675 Java references and 1539 jobs/4617 windows are independently verified. Units are whole-kernel ns/item; constant folding, dynamic-boundary controls, Program transitions and host limits are documented in [results](../bench/runtime-comparison/results-post-foreign-admission-module-macos-arm64.json) and the [62-check audit](../bench/runtime-comparison/audit-post-foreign-admission-checkpoint.md). Next evaluate duplicate native admission in AOT typed closure fallback with complete binding, cleanup and Program transition preserved. No universal Java parity or global maximum claim.

## Typed closure fallback after prepared native admission (2026-10-27)

The AOT compiler routes an exact int(int) callback to a specialized fallback only after its prepared native attempt actually misses. Four additive versioned internal helpers share ordinary closure dispatch and omit its redundant initial native attempt. Argument-compatibility misses and legacy typed helpers v1/v2 retain the original helper, including wide-integer native admission. Full binding, cleanup, exceptions, Program transitions, counts/locks/TLPD, context, capture synchronization, stack publication and post-transition native retry remain. No object/frame layout change. QORE_DISABLE_AOT_TYPED_CLOSURE_FALLBACK=1 selects original compiler routing. This is an AOT optimization; shared-runtime regression tests do not establish an IR/JIT speed benefit.

Retain under the authorized broader workload tradeoff. Five-fork confirmation improves live-object closure calls 87.828278 to 80.378538 ns/item (-8.482%) and deleted-object calls 87.853687 to 79.437466 (-9.580%), with improvement in every layout. Plain closures cost 14.802122 to 15.330583 (+3.570%, +0.528461 ns/item), and captured closures 20.426190 to 20.826995 (+1.962%, +0.400805 ns/item); the plain cost repeats the earlier screens and is accepted, not dismissed because confirmation fork ranges overlap. No other confirmation aggregate costs 2% or more. Populated-context object calls improve 7.209%/7.715%; other populated confirmation costs are at most 0.824%. Native-disabled confirmation is within 1.204% either direction. Short count 16 improves both object cases about 8%; count 1 improves only 0.725%/0.801% aggregate and fails the one-percent/every-layout target predicate. Therefore no single-call improvement is established. Short controls cost at most 1.085%. Mixed-workload confirmation ranges from a 1.328% improvement to a 1.180% cost; no weighted application gain is claimed. Both repeated-call primary targets pass the prospective screen, broad and confirmation gates. Remaining costs warrant follow-up, not a global maximum or Java-parity claim.

See [measurements and superseded diagnostics](../bench/runtime-comparison/results-typed-closure-fallback-macos-arm64.json) and [62-check audit](../bench/runtime-comparison/audit-typed-closure-fallback.md). Next investigate boxed-wide typed admission and remaining ordinary closure cost; Java parity and a global ceiling remain unestablished.

## Checked boxed integer arguments for typed AOT callbacks (2026-10-27)

Prepared exact int(int) callbacks can accept an unknown-representation heap integer after a non-coercive argument/callable capability check. Inline integer routing is unchanged. The new versioned private helper borrows values, allocates nothing, retains no pointer and shares the existing scalar result slot. Runtime closures require enabled, published native companions; exact function references require a typed entry. Cold/missing/disabled cases, other values and legacy v1/v2 keep ordinary binding. Final live invocation admission, complete Program transition, ownership, stack, debugger, context, capture and exception semantics remain. No object/frame layout change. QORE_DISABLE_AOT_TYPED_BOXED_INT_ARGUMENT=1 selects original compiler routing. This is AOT-specific.

Five-fork, three-layout optimized confirmation improves same-Program wide closure arguments 61.237936 to 56.726822 ns/item (-7.37%), pure function references 84.204731 to 49.907299 (-40.73%), and imported-module function references 296.117671 to 106.170835 (-64.15%). The module path retains the captured Program transition; it is not a strict same-Program measurement. Accepted same-Program costs include pure inline functions +2.26%, wide effectful closures +2.54% and wide effectful functions +3.46%. Broad 38-workload confirmation has no disjoint-layout flags; its maximum aggregate cost is 1.92%, and mixed workloads cost 0.34–0.96%. Short counts do not establish the sustained 5% target. Disabled-native, populated-context, superseded iterations and all measured costs are preserved.

Release and Debug each pass 33 QUnit runs / 216 cases / 8491 assertions plus three standalone fixtures, native 2917 and standard references 1110. All 391 Release aliases are restored. Independently replayed evidence covers 4848 current jobs / 14544 windows, 432 superseded jobs / 1296 windows and 108 rejected alignment jobs / 324 windows. See [results](../bench/runtime-comparison/results-typed-boxed-int-argument-macos-arm64.json), [tradeoff and all phase tables](../bench/runtime-comparison/RESULTS.md) and [62-check audit](../bench/runtime-comparison/audit-typed-boxed-int-argument.md). No new Java/develop ratio or global ceiling. Next test scalar result continuation into a sole AddInt consumer, preserving conversion order, full use-count/dominance proof and cleanup; evaluate caller-local invalidation separately with deferred destruction accounted for.


## Native integer callback results in compound addition (2026-10-27)

AOT lowering retains an exact-int typed callback result for a sole AddAssignInt RHS
in the same IR block, after its producer. Complete value-use counting and dominance
restrict the optimization; ordinary +, methods, escaping/multiple uses and cross-block
arithmetic remain excluded. The native payload avoids a temporary heap integer;
fallback conversion, owned cleanup, exceptions, caller-local invalidation and all
live Program/context/lifetime/stack checks remain. Explicit ToInt/ToFloat continuations
have independent controls. QORE_DISABLE_AOT_NATIVE_CALL_ARITHMETIC=1 restores the previous
arithmetic lowering. No runtime-helper or object/frame ABI change; the private compiler
method signature and AOT runtime identity change, so matching artifacts are rebuilt.

Retain under the authorized broader workload tradeoff. Five-fork, three-layout optimized
same-Program wide-result closure/function accumulation improves 52.584%/62.369%
(28.619360 to 13.570241 / 21.462216 to 8.076496 ns/item), with improvement in every layout.
Small inline results have no consistent all-layout gain. Effectful controls cost 1.985–2.759%.
Broader confirmation costs include pipeline 1.233%, CSV1.449%,text 0.389%,polymorphic floating
expressions 4.100% and object-closure creation 3.797%; orders improve 0.653%. The focused
same-runtime disabled control retains most larger costs: polymorphic floating expressions
cost 3.275% enabled/ 2.887% disabled, plain closures 2.670%/2.629%. Exact causes remain
unproven; these costs are accepted, not dismissed as noise. Short/context/disabled controls
and every phase are reported; no weighted application improvement is claimed.

Release/Debug each pass 202 cases / 10080 assertions plus three standalone fixtures; Debug
also passes 70 additional assertions and 2917 native checks. All 391 Release aliases are
restored. Reviewed evidence contains 4575 paired jobs / 13725 windows plus 72 initial
baseline jobs / 216 windows. See [all phase tables](../bench/runtime-comparison/RESULTS.md),
[raw evidence](../bench/runtime-comparison/results-native-call-int-accumulation-macos-arm64.json)
and [62-check audit](../bench/runtime-comparison/audit-native-call-int-accumulation.md).
No new Java/develop comparison or performance-ceiling claim. After the upstream merge
and optimized installation, probe an additive scalar result/status register ABI, preserving
legacy entries and live admission/context/stack/lifetime semantics; measure before retaining.

## Optimized cumulative checkpoint after native integer accumulation (2026-10-27)

All 78 workloads have lower aggregate elapsed time than develop; no adverse layout flag (>3% cost with disjoint observed fork ranges) fires. This cumulative result does not remove the incremental regressions disclosed for retained optimizations. Known integer callbacks take 1.261480 ns/item versus Java 1.035918 (1.218x); direct calls take 1.670999 versus 1.025716 (1.629x), and simple methods take 1.731942 versus 1.023697 (1.692x). Plain/captured closures remain 8.605x/11.468x Java; polymorphic calls 9.234x, list mutation 30.480x, and imported-module callbacks 35.977x with an additional QoreProgram transition. Mixed workloads reduce elapsed time by 13.673% for CSV, 14.412% for orders, 15.784% for text, and 62.734% for the callback pipeline. These are separate workloads, not an application-weighted overall score.

Matching optimized builds compare installed branch 8a13977556 with pure develop 71944301c9, covering 78 Qore workloads and 45 fresh optimized Java counterparts. All 9024 Qore/675 Java references and 1539 jobs/4617 windows are independently verified. Units are whole-kernel ns/item; constant folding, dynamic-boundary controls, Program transitions and host limits are documented in [results](../bench/runtime-comparison/results-post-native-accumulation-module-macos-arm64.json) and the [62-check audit](../bench/runtime-comparison/audit-post-native-accumulation-checkpoint.md). Next investigate bounded scalar-loop native eligibility with complete observable behavior and cancellation responsiveness preserved. No universal Java parity or global maximum claim.

## Runtime stack-pop cold wait split (2026-10-27)

The runtime inlines the unchanged pop-publication handshake and outlines its existing reader wait. Atomic ordering, LIFO checks, publication, context restoration and reader completion remain; no public ABI, object layout or compiler change. Existing native modules benefit through the runtime library.

Retain under the authorized broader workload tradeoff. Final five-fork, three-layout confirmation improves plain closures 14.591571 to 13.821401 ns/item (-5.278%), populated plain closures 29.131308 to 28.072442 (-3.635%), and the two small-loop call targets about 2.05%/2.08%; all improve in every layout. Accepted repeated costs include buffer reads 2.494%/2.692%, captured closures 1.170%, boxed-float fallback 1.542%, list building 1.024% and the string-result short probe 1.849%. Mixed confirmation stays between -0.554% and +0.966%. No weighted overall improvement is claimed.

Release/Debug each pass 208 cases / 10350 assertions and three standalone fixtures; Debug adds 70 control assertions and 2917 native checks. All 391 Release module aliases are restored. Evidence independently replays 2862 jobs / 8586 windows across narrow and broad screens/confirmations, with 98 broad workloads and explicit short/populated/native-disabled controls. See [all phases and costs](../bench/runtime-comparison/RESULTS.md), [raw evidence](../bench/runtime-comparison/results-stack-pop-cold-wait-macos-arm64.json) and [62-check audit](../bench/runtime-comparison/audit-stack-pop-cold-wait.md).

The prior optimized Java/develop checkpoint remains separate; this experiment provides no new Java ratio or global ceiling. Next merge develop and install the optimized runtime as requested, then test a compact reusable closure frame without retaining closures or changing old helper/cleanup contracts. That follow-up is currently an uncompiled layout hypothesis.

## Compact automatic closure frame experiment (2026-10-27)

Reject automatic metadata compaction after optimized confirmation: plain closures improve 0.833%, captured closures 0.267% with a 2.472% reverse-layout cost. All 576 jobs / 1,728 windows are retained. Restore both production files and the byte-identical optimized 7bc52fd305 build; the verified system installation remains unchanged. The 192-byte compact shape fits the existing 208-byte caller budget in Release/Debug layout probes, so invocation-owned reusable storage remains a separate lead requiring an additive cleanup ABI and compiler-use proof. No Java parity or global maximum is claimed. See the [results](../bench/runtime-comparison/results-compact-automatic-closure-rejected-macos-arm64.json) and [audit](../bench/runtime-comparison/audit-compact-automatic-closure-rejected.md).

## Reusable closure storage experiments (2026-10-27)

Reject three complete runtime/compiler variants after 648 optimized jobs / 1,944 windows. Compact storage fits 192 bytes within the old 208-byte caller budget, and explicit variant/thread arguments remove dependent loads, but no revision passes the prospective target gate across layouts. Final plain closures improve 0.240%; captured closures improve 1.055% in aggregate but regress 1.475% in the default layout; indirect calls regress 1.862% and in every layout. All nine runtime/compiler/test changes are reverted, leaving the merged baseline and its installed runtime intact.

Each variant passes 16 Release cases / 7,014 assertions and reference checks. A targeted concurrent-reader fixture explicitly selects the new helpers in revision 3; the pre-existing reader fixture did not and its passing runs are not treated as coverage of the new frame. Debug runtime and broader timing were not pursued after the performance gates failed. See [results and limits](../bench/runtime-comparison/RESULTS.md), [lossless evidence](../bench/runtime-comparison/results-reusable-closure-storage-rejected-macos-arm64.json), and [audit](../bench/runtime-comparison/audit-reusable-closure-storage-rejected.md). No global maximum or new Java comparison is claimed; move to a distinct overhead source rather than retaining this storage ABI.

## Existing call-context pointer snapshots (2026-10-27)

Reject the four-method runtime candidate after 1,008 optimized runs / 3,024 windows. Const-qualifying the member pointer has no machine-code effect; explicit local snapshots remove nine loads in six reusable entries without adding calls or frame space. Corrected opaque same-Program workloads prove actual reusable native hits. No direct target meets the 1% all-layout gate in both screen and confirmation, while imported indirect controls consistently cost roughly 1–3%. Ordinary closure improvements accompany unchanged instruction sequences and are not attributed uniquely to reduced call work.

The first floating-loop targets already used earlier static callback specialization; those measurements remain controls. All 14 Release cases / 6,912 assertions pass, but no Debug/broad expansion follows rejection. Runtime edits are reverted, the full optimized baseline 0f3795889d is byte-identical, and its verified system installation (including develop 42ae5a5181) is unchanged. The new opaque_callbacks.qr benchmark and exact reference validator preserve the useful coverage improvement. Next investigate generic float += typed-callback-result lowering, with callback-return/mutation/cleanup proofs before implementation. See [results](../bench/runtime-comparison/RESULTS.md), [evidence](../bench/runtime-comparison/results-call-context-pointer-rejected-macos-arm64.json) and [audit](../bench/runtime-comparison/audit-call-context-pointer-rejected.md). No global maximum or fresh Java ratio is claimed.

## Single-local compound descriptor borrowing (2026-10-28)

AOT/JIT generic compound assignments with one LocalVar step now borrow their immutable instruction-owned path. The shared helper keeps full live lookup, reference/lock handling, conversion, mutation and ownership behavior. Other paths are unchanged; IR interpretation has its own implementation. No ABI or calling-convention change.

The opaque same-Program floating callback accumulation target improves 75.197→61.496 ns/item in the screen and 75.247→61.474 ns/item in independent confirmation (18.2%/18.3%), improving in all three code layouts. These are whole-loop measurements, not call latency. The 94-workload expansion and its prescribed confirmation capture broader tradeoffs; see the complete tables and decision in [results](../bench/runtime-comparison/RESULTS.md) and [lossless evidence](../bench/runtime-comparison/results-local-compound-path-macos-arm64.json).

Both Release and Debug pass 81 QUnit cases / 7,718 assertions plus two standalone fixtures; Debug also passes 70 fallback/context assertions and 2,917 native assertions. The stale optionalAdd codegen matcher is corrected after reproducing its failure on the frozen baseline. All 391 Release qmod aliases are restored and verified. Future native float lowering requires conversion/lifetime proofs because callable return compatibility does not enforce an exact float result. No fresh Java/develop ratio or global ceiling is claimed. [Audit](../bench/runtime-comparison/audit-local-compound-path.md).

## Scoped native float callback accumulation (2026-10-28)

A definitely assigned private required-float local += a zero-argument typed callback now converts the actual result and uses native addition. A temp scope encloses callback, addition and store, releasing the owned result before the enclosing expression continues and routing destructor exceptions to the current handler. Signature compatibility alone never proves an exact float. Shared, weak-capable, optional and uncertain-assignment cases retain generic handling. IR, JIT and AOT share this lowering; no ABI or runtime helper is added.

Optimized AOT confirmation covers opaque same-Program float callback loops at counts 4096, 17 and 1, three layouts, controls and mixed workloads. These are whole-kernel times; the new body also enables an existing outer function fast entry. All final numbers and the explicit workload tradeoff are recorded in [results](../bench/runtime-comparison/RESULTS.md). Native boolean-to-float conversion uses unsigned i1 conversion, verified by statically resolved True/False callback references. The first candidate exposed signed i1 conversion; its measurements are superseded by the corrected candidate.

Release and Debug each pass 83 cases / 7,949 assertions plus two standalone fixtures, with additional native, fallback/context and statement-lifetime checks. The admitted path fixes a reproduced baseline cleanup-order discrepancy against AST; generic cases are not claimed fixed globally. Next measure scalar preservation across cleanup and inline conversion independently. No Java parity or global ceiling claim. [Evidence](../bench/runtime-comparison/results-scoped-float-callback-macos-arm64.json); [audit](../bench/runtime-comparison/audit-scoped-float-callback.md).

## Native scalar retention across temporary cleanup (2026-10-28)

JIT/AOT DiscardTempsKeep preserves raw double/int64/bool SSA values when the result has zero or one consumers. NaN-boxed values and native conversions carrying an original boxed source keep the existing ownership path. Multiple consumers retain shared boxes to avoid repeated boxing; sparse typed-list NOTHING stays NOTHING. Other temporaries still drain and destructor exceptions retain the current handler. No interpreter, helper or ABI change.

Optimized before/after AOT measurements use baseline 273f782ebe, three layouts, independent confirmation and 57 broader workloads, including short cases that regressed in focused timing. See the explicit tradeoff and all figures in [results](../bench/runtime-comparison/RESULTS.md). Release/Debug qualification, generated-code and raw timing replay are in [evidence](../bench/runtime-comparison/results-native-scalar-temp-keep-macos-arm64.json), with the [62-check audit](../bench/runtime-comparison/audit-native-scalar-temp-keep.md). Next evaluate native float callback continuation and inline conversion independently; Java parity and a global ceiling remain unproven.

## Native float callback continuation into accumulation (2026-10-28)

AOT AddAssignFloat can use the existing native callback-result side channel for a matching typed float callback defined earlier in the same IR block with one result consumer. Only the runtime handled flag proves native data; arbitrary boxed fallback values retain conversion, ownership, exceptions and caller-local invalidation. The integer continuation match is unchanged. IR/JIT selection and the runtime helper/ABI are unchanged.

Generated target bodies avoid fast-path float reboxing and call qore_rt_to_float only on fallback. Optimized baseline 7eaf11e3e7 already includes scalar retention. Three layouts, independent confirmation, 58 broader workloads and the explicit tradeoff are in [results](../bench/runtime-comparison/RESULTS.md). See [evidence](../bench/runtime-comparison/results-native-float-call-arithmetic-macos-arm64.json) and the [62-check audit](../bench/runtime-comparison/audit-native-float-call-arithmetic.md). Refresh optimized develop/Java comparisons next to identify remaining costs; no parity or global ceiling claim follows from this kernel.

## Optimized cumulative checkpoint after native float callback continuation (2026-10-28)

All 78 aggregate workload medians improve versus develop, with no adverse layout flags under the stated >3%/disjoint-range diagnostic. The four mixed workloads take 14.805% less time for CSV, 13.221% less for object orders, 62.495% less for the callback pipeline and 15.586% less for text indexing. These are four distinct CPU workload results, not a production-wide weighted average. Known integer callbacks take 1.182833 ns/item versus Java 0.890219 (1.329x); direct functions take 1.579531 versus 0.887655 (1.779x); simple methods take 1.654139 versus 0.887153 (1.865x). Known floating callback bodies take about 0.82–0.85 ns/item versus Java 0.44–0.47. Fast scalar workloads are close to Java, but have not reached universal parity. Plain/captured closures still take 13.414908/19.089743 ns/item, imported callbacks about 59–61, and list mutation 130.594422. These retain large gaps even against the non-inlined Java dynamic-call controls. Polymorphic receiver expressions take 10.640902 ns/item while the equivalent per-iteration receiver local takes 22.900857, identifying local materialization/type/lifetime work in addition to dispatch. Default/string short object-call probes improve by only 8.6–10.4%, showing that cumulative gains vary substantially by workload.

Matching optimized builds compare installed branch 33b6b9735a with pure develop a5af92e73c, covering 78 Qore workloads and 45 fresh optimized Java counterparts. All 9024 Qore/675 Java references and 1539 jobs/4617 windows are independently verified. Units are whole-kernel ns/item; constant folding, dynamic-boundary controls, Program transitions and host limits are documented in [results](../bench/runtime-comparison/results-post-float-continuation-module-macos-arm64.json) and the [62-check audit](../bench/runtime-comparison/audit-post-float-continuation-checkpoint.md). Next investigate the remaining costs ranked in the checkpoint assessment, preserving observable behavior and cancellation responsiveness. No universal Java parity or global maximum claim.

## Finite pure callback targets in scalar AOT callers (2026-10-28)

A private callback initialized once from a two-way choice of literal same-Program function references can retain both exact targets through the boxed IR PHI. A parallel boolean PHI follows the same validated LLVM predecessor edges and selects the result of the two complete pure scalar summaries. Both targets must have the same native int/float return and zero arguments or one exact required int/float argument. Each summary is bounded to 16 expression nodes. Defaults, varargs, captures, implicit receivers, imported/preloaded contexts, side effects and throwing bodies remain on ordinary dispatch.

Admission proves one normal initial assignment, definite assignment, no aliases/captures/weak storage/AST exposure, and initializer/store/load dominance including exceptional edges. Boxed locals can still have runtime slots: syntactic immutability alone is insufficient. An additional bounded-opcode proof requires scalar caller storage and no reentry except the same proven callbacks, including exact argument binding. Unknown calls, reflective access, object/container operations and user destructor paths decline the optimization. Call-reference creation, binding, boxed PHI, runtime store/reload and cleanup remain unchanged. Source-loop cancellation checks remain at their existing points. Compiler metadata is gathered lazily only after finding a viable pair, with cancellation checks throughout unbounded scans.

LLVM emission rechecks the actual native argument representation and rejects boxed provenance or pending owned argument cleanup. Disabled scalar-summary imports keep ordinary callback dispatch. The compilation presence flag QORE_DISABLE_AOT_FINITE_CALLREF_TARGETS disables finite target selection; no runtime helper, ABI or IR/JIT path is added.

The callback-local reload follow-up below uses this stronger no-reentry proof. Repeated source-location stores, short invocation setup, and dynamic closure/method and imported callback costs remain separate investigations. These results do not establish general Java parity or a global performance ceiling.

## Finite callback local cache refresh omission (2026-10-28)

An admitted `finite_call_ref_loads` entry now skips `ensureLocalCacheFresh`. The existing proof establishes a strongly owned, definitely assigned private callback local, one assignment, initializer/store/load dominance including exceptional edges, no aliases/captures/weak or opaque visibility, and no arbitrary reentry anywhere in the scalar caller. This is stronger than syntactic immutability: a boxed local still has a runtime slot, but no admitted operation can reflectively replace its value. AOT debug-step hooks remain no-ops; this proof is not extended to JIT debugging.

Original reference creation and binding, boxed PHI, runtime store, ordinary cached load and ownership/cleanup remain. Disabling scalar-summary import can retain ordinary dispatch through the same pure targets, so its fallback remains valid without refresh. Other locals retain their existing refresh path. Source locations, cancellation points, RuntimeConfig, TLS and runtime ABI are unchanged. `QORE_DISABLE_AOT_FINITE_CALLREF_RELOAD_ELISION=1` is a compilation presence flag that restores the old refresh lowering while retaining finite target selection.

Optimized confirmation against merged `3080c23bc1` reduces the four 4096-iteration target kernels by 6.6–37.0%. Fresh natural-inlining Java comparisons and broader controls are in `bench/runtime-comparison/RESULTS.md`. Next measure batches of short invocations against inline controls, then use the resulting setup/entry evidence to choose the next optimization. No general Java parity or maximum-performance claim.


## Short callback invocation checkpoint (2026-10-28)

The initial batched short wrappers exposed setup work that the 4096-iteration callback loops amortize: the one-callback Qore cases take about 242–245 ns/invocation, against 0.90–1.05 ns for Java with normal inlining. Algebraically equivalent Qore inline controls take 1.15–1.68 ns. This is whole-batch cost, not a subtraction-based estimate of call latency. The exact reference, generated code, three-layout/three-fork measurements and C2 diagnostics are in the batched short callback report.

## Finite callback adapter binding omission (2026-10-28)

After finite-target admission, the compiler records the original native and typed binding calls for each creation whose only use is its matching PHI edge. Existing use accounting includes both the operand list and incoming-edge list; both must match without any additional use. The group tracks expected call sites and sites which actually emitted complete scalar summaries. Only when all sites in the group inlined are the recorded void binder calls erased, after body and PHI lowering. A declined call retains all generated adapters. Metadata is private to one compiler/function and reset with the other finite-target maps; unbounded group cleanup checks cancellation.

Reference creation, runtime target resolution, boxed PHI/store, ownership and cleanup remain. This does not change runtime ABI, RuntimeConfig, TLS, IR or JIT behavior. `QORE_DISABLE_AOT_FINITE_CALLREF_BIND_ELISION=1` is a compilation presence flag restoring all binders. The [performance evidence](../bench/runtime-comparison/results-finite-callback-binding-macos-arm64.json) and [audit](../bench/runtime-comparison/audit-finite-callback-binding.md) record the short-call gains and repeated polymorphic/long-loop costs. General Java parity and a global performance ceiling remain unestablished.

## Exact finite callback assignment coercion omission (2026-10-28)

An already admitted private finite-target callback pair can skip assignment coercion when both concrete signatures exactly match the local's nonoptional, nonvariadic typed-code declaration. The comparison is bounded to two targets and at most one required scalar parameter each. These freshly created strong function references need neither weak-reference normalization nor conversion to that declaration. Plain code, optional code, different signatures, aliases, captures and other excluded locals retain their existing paths.

The omitted coercion helper would take an additional reference and publish it to a temporary cleanup slot. Omitting the helper also omits that temporary owner; the original creation owner, runtime publication owner and cached-local owner remain. Publication uses the existing no-coerce helper, preserving its ordinary ownership and exception behavior. This does not reclassify boxed locals as IR-only or remove call-reference creation. The proof remains valid if later native summary emission declines and the ordinary callback call is retained. No runtime helper, ABI, RuntimeConfig, TLS, IR or JIT change is made. `QORE_DISABLE_AOT_FINITE_CALLREF_COERCE_ELISION=1` restores the original assignment lowering at compilation.

Against optimized merged baseline `86e08dfea8` (including develop `62a415e603`), independent confirmation reduces eight short callback workloads by 18.2–20.2%, with gains in all 24 target/layout combinations. Single-callback invocations take 174–178 ns versus fresh Java 0.91–1.07 ns; Qore inline controls take 1.17–1.71 ns. These are whole-invocation measurements, not isolated call latency. The broader confirmation records direct-float-result +1.534%, boxed-method +1.340%, and mixed changes from -0.305% to +1.018%, with overlapping adverse fork ranges. No application-weighted overall gain or global performance ceiling is claimed.

Release and Debug each pass 38 cases/5378 assertions plus one standalone test; Debug also passes 2917 native assertions and 70 fallback-control assertions. The expanded 652-assertion fixture covers multiple groups, conditional initialization, exact and nonexact declarations, ownership exclusions and disabled summary imports. All 1608 timing jobs/4824 windows replay independently. See [results](../bench/runtime-comparison/RESULTS.md), [evidence](../bench/runtime-comparison/results-finite-callback-coerce-macos-arm64.json), and the [62-check audit](../bench/runtime-comparison/audit-finite-callback-coerce.md).

Next refresh the cumulative optimized develop comparison, then investigate an early IR representation that can remove nonescaping callback creation and runtime-local work coherently. LLVM local instantiation precedes finite-target analysis, and actual native-call emission can still decline later; simply marking the callback local IR-only is insufficient. Preserve both ownership and fallback behavior before attempting selector-only storage.

## Optimized cumulative checkpoint after finite callback coercion omission (2026-10-28)

All 78 measured workload aggregates are faster on this branch than pure develop in this checkpoint. The mixed CSV/report, object/order, callback-pipeline and text-index workloads take respectively 14.6%, 13.9%, 63.1% and 15.0% less time. Direct calls measure 1.671 ns/item versus 11.819 on develop and 1.018 for Java; simple methods are 1.741 versus 11.850 and 1.020. The known integer callback is 1.242 versus 247.681 and 1.027, about 21% above Java for this whole kernel. Simple floating-body kernels measure 0.751–0.894 ns/item versus Java 0.500–0.633, with matched Qore/Java ratios of 1.31–1.65. These scalar paths approach Java, but do not establish universal parity. Plain/captured closures remain 14.097/19.882 ns/item versus Java 1.729/1.809; polymorphic calls are 24.053 versus 2.625; imported indirect callbacks are 62.637 versus 1.721; list mutation is 138.053 versus 4.568. The imported callback includes a QoreProgram transition, so its ratio does not isolate same-Program dispatch cost. The dynamic Java targets retain the documented DONT_INLINE controls. No workload/layout meets the predefined adverse flag (>3% extra branch time with separated observed fork ranges); this does not prove zero cost on other workloads or erase previously recorded incremental tradeoffs.

Matching optimized builds compare clean branch 8276756a7a with pure develop 62a415e603, covering 78 Qore workloads and 45 fresh optimized Java counterparts. All 9024 Qore/675 Java references and 1539 jobs/4617 windows are independently verified. Units are whole-kernel ns/item; constant folding, dynamic-boundary controls, Program transitions and host limits are documented in [results](../bench/runtime-comparison/results-post-coerce-module-macos-arm64.json) and the [62-check audit](../bench/runtime-comparison/audit-post-coerce-checkpoint.md). Next investigate the remaining costs ranked in the checkpoint assessment, preserving observable behavior and cancellation responsiveness. No universal Java parity or global maximum claim.

## Native constant callback scalar replacement (2026-10-28)

AOT native lowering can replace a private required zero-argument constant callback pair with a native integer/float PHI and scalar snapshots. Indexed uses/writes, single assignment, dominance, exact target signatures/binding, immutable body-contract dependencies and a complete non-reentrant scalar body are required. Both targets must be single-variant context-independent user constant entries; generic/preloaded/deferred/imported targets, observable/escaping locals, handlers, effects and unsupported operations retain the original representation. The complete slot map is constructed first.

A preallocated transaction saves replaced/removed instructions and original SSA facts. Native lowering retains the existing entry ABI and body-local metadata/lifecycle. The original instructions and facts are restored after outlining/lowering, before metadata extraction and debug serialization. This avoids exposing an empty callback local or losing callback events when a debugger dispatches through serialized IR. The ordinary and method compilation paths both restore their original bodies; cancellation is polled during analysis, coherent commit and restoration. `QORE_DISABLE_AOT_CONSTANT_CALLBACK_SCALAR_REPLACEMENT=1` is the compilation disabling control. Existing finite-target/import controls are respected. This is native AOT only; no RuntimeConfig, TLS, ABI, IR or JIT convention change is made.

Independent optimized confirmation reduces four short constant-callback targets 73.0–78.4%, with improvement in all twelve target/layout combinations. Single integer/float callback invocations take 37.24/41.30 ns versus fresh Java 1.03/0.90 ns, so substantial setup costs remain. Long constant-callback loops improve about 9.6%. Mixed confirmation ranges from 0.44% faster to 0.46% slower. A direct-float-result control is 3.45% slower (+0.026ns/item) after improving in the first run; that reversal and its adverse default layout are retained. No application-weighted benefit or global performance maximum is claimed.

Release/Debug each pass 47 cases/7450 assertions plus one standalone test; Debug adds 2917 native and 70 fallback assertions. The 896-assertion fixture covers original debugger locals/events and disabled/split/EH paths. All 1578 timing jobs/4734 windows independently replay; all 391 Release module aliases are restored. See [results](../bench/runtime-comparison/RESULTS.md), [full evidence](../bench/runtime-comparison/results-finite-constant-callback-macos-arm64.json) and the [62-check audit](../bench/runtime-comparison/audit-finite-constant-callback.md). Next investigate native integer RefSelf snapshots, empty local lifecycle, and consistent fast-entry discovery/emission before attempting broader context-independent callback bodies.

## Imported native closure execution context (2026-10-28)

Module functions execute in their caller's Program after the execution-context correction merged at `0736da0138`. A closure created there captures that executing Program while its compiled code and AOT metadata still belong to the module. The previous native closure guard required both identities to match and rejected an available scalar companion. This is distinct from a call that actually needs to change the executing Program.

The imported path admits existing compiler-proven integer unary closures through the v2 explicit-context entry. It verifies body-owner metadata and both Programs' debugger state, then retains the ordinary signature, body identity, live Program data, capture, provenance and stack checks. A scoped runtime-option override uses the supplied RuntimeConfig thread view and the execution Program's restrictions. Its saved state stays off the same-owner call frame. The cold v2 frame materializes the closure's execution Program and captured class/object context, restoring the caller on exit. The older v1 context contract remains unchanged; disabling v2 or deferred context retains generic execution for imported bodies.

An initial attempt from a different execution Program still declines. The ordinary path performs its complete Program transition and can then retry native admission, preserving counts, locks, TLPD, lifetime and diagnostic caller Program. No native ABI, code-generation eligibility or ordinary IR/JIT body calling convention changes. IR/JIT callers can benefit when they invoke a qualifying AOT body. `QORE_DISABLE_AOT_IMPORTED_CLOSURE_ENTRY=1` is a runtime presence control for attribution and fallback tests.

Optimized five-fork confirmation against merged `0736da0138` reduces plain/captured closures from 123.82/146.58 to 19.70/25.20 ns/item and the mixed callback pipeline from 179.13 to 95.39. Short unary controls cost 2.23%/1.69%; other mixed changes range from −0.09% to +0.91%. All 882 candidate jobs / 2646 windows independently replay, including disabled-path attribution. Release passes 29 cases / 1453 assertions plus one standalone; Debug passes 10 cases / 1354 assertions plus one standalone and 2917 native assertions. All 391 Release aliases are restored. See [results](../bench/runtime-comparison/RESULTS.md), [evidence](../bench/runtime-comparison/results-imported-native-closure-macos-arm64.json) and [audit](../bench/runtime-comparison/audit-imported-native-closure.md). Genuine Program transitions, imported-body local-slot guards and short-call setup still need optimization; Java parity and a performance ceiling are not established.


## Deferred imported closure runtime options (2026-10-28)

The imported closure-v2 path now defers its execution-Program option override until cold context materialization. The existing complete lowered-body proof permits only the dominating stack guard, fixed location slots and non-evaluating scalar capture reads; a successful body cannot inspect options or reenter Qore. Materialization installs the override before exception work. Restoration restores the saved override before applying the caller's saved runtime options. Optional invocation-owned snapshots avoid initializing the two option values on the hot path, at the cost of 40 additional context bytes on macOS ARM64. Same-owner contexts keep the optional disengaged. Marker allocations still precede option mutation, and the existing frame unpublication/reader handshake precedes destruction of borrowed metadata.

`QORE_DISABLE_AOT_IMPORTED_CLOSURE_DEFERRED_OPTIONS=1` retains native imported admission but restores eager option setup through an outlined helper. No native ABI or eligibility change is made; all actual Program transitions and live guards remain. The expanded regression verifies live option restoration independently of the legacy RuntimeConfig snapshot, including cold overflow after a real Program transition.

Fresh optimized confirmation against `fb2327d9cf` reduces plain/captured imported closures from 19.03/24.37 to 18.16/23.32 ns/item and the mixed callback pipeline from 91.96 to 90.47. A direct-float control costs 1.36%; same-Program closure repeats cost 0.37%/1.33% after small first-run gains. These costs and mixed layout signs are retained. All 1122 jobs / 3366 windows replay; all 15 matched LLVM pairs are byte-identical. Release passes 29 cases / 1513 assertions plus standalone; Debug passes 10 cases / 1414 assertions plus standalone and 2917 native assertions. All 391 Release aliases are restored. See [results](../bench/runtime-comparison/RESULTS.md), [evidence](../bench/runtime-comparison/results-deferred-closure-options-macos-arm64.json) and [audit](../bench/runtime-comparison/audit-deferred-closure-options.md). Program-transition method costs and imported-body local-slot guards still need work; Java parity and a performance ceiling are not established.


## Native scalar methods after a Program transition (2026-10-28)

Imported functions execute in the caller Program while objects retain the class source Program. The AOT dispatcher now retries an existing pure scalar method companion after its full ordinary Program transition and before parameter/self/argv setup. A complete PIC match proves exact canonical arguments; positive preflight requires a ready homogeneous integer or float companion and excludes member/effect bodies. Only that boolean crosses Program initialization. The retry borrows Program-owned method/variant metadata rather than a replaceable caller PIC, reacquires current body/context/entry state, and retains the original caller Program in its diagnostic frame. Existing uncached admission still checks exact signature, body identity, execution/body/object Program equality, debugger state and stack limits. The same-Program compact key, full transition counts/locks/TLPD/lifetime and native ABI are unchanged. `QORE_DISABLE_AOT_CROSS_PROGRAM_METHOD_ENTRY=1` is the runtime presence control; native misses retain ordinary binding without consuming arguments.

Fresh optimized five-fork results against `e0220f4f91` reduce integer polymorphic calls from 159.35/129.17 to 94.45/65.81 ns/item. Floating polymorphic calls take 94.24/66.29 instead of 165.45/135.38, and boxed float methods take 126.40 instead of 224.07. An initial effectful-method cost of 2.74% is removed by positive preflight; final confirmation improves 1.21%. Plain closures cost 0.39%, monomorphic float costs 1.00%, and mixed workloads range from 0.38% faster to 0.84% slower. There is no clear overall application gain from this change alone.

All 1038 prototype/final jobs / 3114 windows replay, including same-binary attribution; all 12 matched LLVM pairs are identical. Release passes 25 cases / 5450 assertions; Debug passes 14 cases / 3036 assertions plus 2917 native assertions. All 391 Release aliases are restored. See [results](../bench/runtime-comparison/RESULTS.md), [evidence](../bench/runtime-comparison/results-cross-program-method-retry-macos-arm64.json) and [audit](../bench/runtime-comparison/audit-cross-program-method-retry.md). AOT caller bodies benefit, including when invoked from an IR/JIT host; IR/JIT-only bodies gain no new companion. Member/effect transitions and imported-body local-slot guards remain priorities. Current Java parity and a performance ceiling are not established.

## Native member methods after a Program transition (2026-10-28)

The post-transition scalar retry also admits existing compiler-proven unary integer/float member-read companions and integer effect-prefix companions. A complete dispatch-key match proves canonical arguments; preflight records one mutually exclusive retry kind and positive companion availability. Only that value crosses potentially reentrant Program initialization. The retry reacquires the live context, body and typed entry, verifies execution/body/object Program identity and debugger state, and borrows invocation metadata from the live method variant. It does not borrow the caller's replaceable dispatch cache. Member admission is outlined separately from the pure scalar helper.

Foreign member calls establish eager object/class context and retain the original caller Program in their diagnostic frame. A read-only miss returns to ordinary binding without consuming arguments. A partial effect miss first unpublishes the native frame, snapshots method metadata, instantiates the ordinary self/parameter slots, and resumes after the completed updates. The existing return filter, cleanup, frame-reader synchronization and full Program transition remain. Same-Program compact dispatch and deferred frames retain their existing protocol. No native ABI or public API changes.

Compilation conservatively leaves read-only unary/getter bodies with strict reference member declarations on ordinary evaluation, including ordinary-private, public and inherited declarations. This avoids speculating on their primitive representation; ordinary runtime resolution remains authoritative. Dynamic and optional declarations retain live guards, and effect prefixes remain eligible because completed updates can still avoid ordinary work before continuation. The bounded body proof and physical value guard remain required for every positive companion. `QORE_DISABLE_AOT_CROSS_PROGRAM_MEMBER_ENTRY=1` disables the new runtime member retry independently; the existing cross-Program method control disables both pure and member retries. These changes benefit native AOT bodies, including calls made through an IR/JIT host, and do not add companions to IR/JIT-only bodies.

Fresh optimized confirmation against `b3bf884fce` reduces cross-Program integer/float member reads from 241.94/259.03 to 123.51/132.99 ns/item and integer effects from 345.68 to 184.60. The final compiler exclusion removes the earlier 14–16% ordinary-reference read regression. Retained costs include pure boxed-integer calls +2.19% and ordinary-private reference effects +2.05%; mixed changes range from -0.73% to +0.67%. No weighted application gain is inferred. Same-binary disabling restores the generic cost for the positive targets.

Release/Debug each pass 33 cases/6644 assertions; Debug adds 2917 native assertions. All 391 Release qmod aliases are restored. The final 1599 jobs/4797 windows and all 3534 prototype/final jobs/10602 windows replay independently. See [results](../bench/runtime-comparison/RESULTS.md), [evidence](../bench/runtime-comparison/results-cross-program-member-retry-macos-arm64.json), and the [62-check audit](../bench/runtime-comparison/audit-cross-program-member-retry.md). Imported-body local-slot guards and reference-valued effect tails remain optimization leads; current Java parity and a performance ceiling are not established.


## Imported AOT body-local prepared storage (2026-10-29)

Prepared reload, auto and class helpers require an active execution Program but no longer equate it with the AOT code owner. Imported functions execute in the caller Program. Compiler body-local membership and post-instantiation emission dominate lookup: ordinary entries pre-instantiate AOT locals, and private fast entries instantiate runtime-visible locals before preparation. RuntimeConfig selects the active thread's TLPD and exact LocalVar identity. The lookup can cross frame boundaries, so it does not by itself prove current-invocation ownership. Parameters, captures, self, top-level and IR-only exclusions remain. Nested stack growth preserves live parent addresses; block clears retain storage, with finalized/boundary and live evaluation guards unchanged. No cached value, new ABI, Program transition elision or policy change is introduced.

Existing runtime presence controls independently disable reload, auto and class preparation. Source-only IR/JIT bodies do not gain native lowering; AOT bodies called through those hosts benefit. Optimized confirmation against `12107bdb79` reduces imported list mutation from 197.18 to 132.05 ns/item, virtual integer/float calls from 91.92/92.77 to 76.34/77.79, and member reads from 123.17/132.52 to 111.59/112.84. Same-binary controls attribute the large gains. Local integer-member reads cost 1.31% in confirmation, local virtual dispatch 0.35% and temporary lists 0.17%; all adverse confirmation ranges overlap. No object-orders gain or weighted application gain is claimed.

Release/Debug each pass 26 cases/9790 assertions; Debug adds 2917 native assertions. The new fixture passes 1735 assertions on unchanged baseline and candidate. All 391 Release aliases are restored. All 2742 timing jobs/8226 windows replay independently, and all nine before/after LLVM pairs are byte-identical. See [results](../bench/runtime-comparison/RESULTS.md), [evidence](../bench/runtime-comparison/results-imported-body-local-slots-macos-arm64.json), and the [62-check audit](../bench/runtime-comparison/audit-imported-body-local-slots.md). A fresh cumulative develop/Java checkpoint and further virtual/transition/list profiling remain; no global performance ceiling is established.

## Optimized cumulative checkpoint after imported body-local preparation (2026-10-29)

All 86 aggregate workloads improve against develop in this checkpoint. Standalone same-Program direct and simple-method loops use 1.515 and 1.603 ns/item versus Java 0.899 and 0.901 (1.69x and 1.78x), and are 7.27x and 6.91x faster than develop. Same-Program virtual dispatch is 23.017 ns/item versus Java 2.527 (9.11x), improving 13.33x against develop. Member-read methods remain 32.392/34.615 ns/item for integer/float, 12.74x/13.56x Java; the effectful method remains 89.373 ns/item, 47.13x Java. Imported virtual calls cost 76.179 ns/item (30.07x Java); imported indirect callbacks cost 9.990 (6.00x), plain closures 17.666 (10.61x), captured closures 23.446 (13.47x). Module and standalone Java counterparts are fresh separate forks, not pooled. The four mixed workloads improve 1.173x CSV, 1.104x object orders, 3.540x callback pipelines and 1.304x text indexing. Imported list mutation improves 1.816x to 131.824 ns/item, still 29.03x its Java counterpart. There are no adverse aggregate results or disjoint >3% adverse layout flags in this campaign; earlier incremental costs remain documented in their own reports.

Matching optimized builds compare installed branch d088fffea1 with pure develop cced3d9bd0, covering 86 Qore workloads and 53 fresh Java comparison rows (45 distinct methods). All 13464 Qore/1230 Java references and 1707 jobs/5121 windows are independently verified. Whole-kernel units, constant folding, dynamic-boundary controls, Program transitions and host limits are documented in [results](../bench/runtime-comparison/results-post-imported-slots-checkpoint-macos-arm64.json) and the [62-check audit](../bench/runtime-comparison/audit-post-imported-slots-checkpoint.md).

Next profile the exact frozen branch/develop binaries for standalone and imported virtual, member/effect and list-mutation workloads. Inspect prepared-method stack checks and frame/context work. If profiles and machine code support it, evaluate scoped inlining of the existing full physical stack guard, preserving every guard and cold overflow/materialization path; do not omit guards or Program transitions. Retain direct, callback, closure, short-invocation and mixed controls and take fresh optimized before/after measurements before retaining a candidate. Reference-valued effect continuations and residual list ownership/navigation remain subsequent leads. Java parity and a global performance ceiling have not been established.

## Rejected prepared method stack-check inlining (2026-10-29)

Local/imported integer virtual calls improve 2.17%/2.09%, but floating-member reads, indirect/captured callbacks, orders and text show small costs. The 0.498 ns/item local target gain and 0.489 ns/item floating-member cost do not justify the broader tradeoff. A default floating-member layout is 2.66% slower with disjoint observed ranges. All 32 workloads/960 jobs/2880 windows remain in the [evidence](../bench/runtime-comparison/results-method-stack-inline-rejected-macos-arm64.json), with the [62-check audit](../bench/runtime-comparison/audit-method-stack-inline-rejected.md).

The complete stack guard was force-inlined without semantic omissions. All 15 LLVM module pairs are identical, and member helpers already inlined before. Production sources are restored; full optimized build/391 qmods and 17 Release fixtures/42 cases/8008 assertions pass. Candidate Debug was not run. Installed ff12d02f2b includes develop 5a4e629506 and remains the qualified baseline. Next investigate shared runtime scalar cleanup, preserving enum/opaque/node ownership, followed by proved parameter visibility or member lookup changes. No fresh Java ratio or global performance ceiling is claimed.

## Retained runtime cleanup for non-owning values (2026-10-29)

The existing decref helpers now skip discard for values with no owned payload, using the existing needsRef predicate and a shared noinline owning helper. The measured ARM64 wrappers have no scalar stack frame; enum/opaque/node release, original/null sinks, ABI, caller lowering, Program context and cancellation remain. Existing AOT/JIT artifacts can benefit without recompilation; no separate IR/JIT timing is claimed.

Two optimized epochs show 7-12% less time for list building/mutation and 1-3% for CSV. Retained costs include imported boxed-integer calls (+1.70%/+2.59%) and a reverse captured-closure layout (+5.42%/+6.24%, disjoint observed ranges in both epochs). The broader tradeoff justifies retention without a zero-regression or application-weighted claim. All 32 workloads/1152 jobs/3456 windows are preserved and independently replayed using 15 exact shared native artifacts.

Release/Debug each pass 19 fixtures (two silent), 52 cases/12110 assertions, plus Debug focused 2962 and broader native 4620 assertions; 391 Release qmod aliases are restored. See [results](../bench/runtime-comparison/results-runtime-scalar-cleanup-macos-arm64.json) and the [62-check audit](../bench/runtime-comparison/audit-runtime-scalar-cleanup.md) for all gains, costs, prototype preflight failures and limits. Next merge/install, then separately investigate owning cleanup and explicit host-CPU AOT tuning. No fresh Java ratio or global ceiling is established.

## Retained general value-discard cleanup (2026-10-29)

QoreValue::discard now delegates its existing enum and node owning releases to two private noinline helpers, leaving scalar clearing frame-free on the measured ARM64 build. Release-before-clear order, opaque deregistration, sinks, object layout, public ABI, AOT identity and cancellation stay unchanged. ValueHolder destructors still have frames; existing AOT/JIT artifacts can benefit from the runtime replacement, with no separate IR/JIT timing claim.

Two optimized epochs repeat direct/simple-method loop gains of 2.1-2.5%, list read gains of 2.7%, list build gains of 3.9-4.6%, temporary-container gains of 3.9-4.6% and typed-integer callback gains of 2.1%. Local/imported list mutation is essentially flat. Retention uses the broader workload tradeoff: local effectful methods cost 2.08%/3.33%, imported effectful methods 2.77%/2.51%, local integer-member methods 1.53%/1.01%, and imported floating-member methods 0.48%/1.38%. Every workload and adverse layout remains documented; no zero-regression or application-weighted claim is made.

All 1152 jobs/3456 windows independently replay using 15 exact shared native artifacts. Release/Debug each pass 21 fixtures (two silent), 58 cases/12138 assertions; Debug adds 3005 focused and 4663 broader native assertions. A Debug-only test access error was corrected; exact source and byte-identical optimized rebuild proofs preserve the original screen. All 391 Release aliases are restored. See [results](../bench/runtime-comparison/results-general-discard-macos-arm64.json) and [audit](../bench/runtime-comparison/audit-general-discard.md).

Next merge/install, then investigate redundant source-location stores and exception-state checks remaining in already-inlined fast loops. Any omission requires a dominating clean-sink proof across every path, including deferred checks, handlers and destruction; cancellation polls and slow handling remain intact. Host-CPU AOT tuning is a separate lead. No fresh Java/develop ratio or global performance ceiling is established.


## Rejected AOT native backend optimization-level mapping (2026-10-29)

Passing requested levels 0-3 to both AOT native target-machine factories, in addition to the existing IR pipeline, did not demonstrate a useful broader gain. The -O3 screen has flat direct calls, +1.83% simple-method time and +1.95%/+1.58% local/imported effectful-method time. Apparent temporary-list -9.16% and indirect-call +23.87% shifts have very broad overlapping ranges and are not reliable improvement/regression conclusions. No workload has separated better ranges in every layout. The candidate is rejected as unproven and QoreAOT.cpp restored; no Debug or confirmation epoch was run after rejection.

All 32 workloads/576 jobs/1728 windows and 12144 references remain in the [evidence](../bench/runtime-comparison/results-backend-opt-level-rejected-macos-arm64.json). Workload LLVM bodies are identical between separately compiled artifacts; expected startup metadata lengths and native assembly change. The retained test extends split determinism to all four qcc levels, checking exit status and execution with source removed. Baseline passes 2 cases/160 assertions; candidate Release has 61 executed cases/12312 assertions and two silent fixtures, excluding 237 filtered cases. See the [62-check audit](../bench/runtime-comparison/audit-backend-opt-level-rejected.md).

Next evaluate explicit host-CPU AOT tuning with generic portability retained and CPU/features propagated consistently to IR and ordinary/split native code generation. A fast-loop clean-sink proof remains a separate investigation; all cancellation and exception semantics remain. Installed 698e retains the prior validated optimizations. No fresh Java comparison or global performance ceiling is claimed.


## Module call-policy integration (2026-10-29)

The performance branch integrates develop b30dbfbca1. Same-thread RuntimeConfig scopes preserve module identity and inherited restrictions, including deferred closure materialization and cross-Program native retries. IR inline static methods now establish the same callee option/location scope as ordinary inline methods. Source/AOT module fixtures explicitly select each child Program execution mode; native probes inspect deferred policy only after materialization. Optimized Release and Debug qualification passes; see `bench/runtime-comparison/results-module-own-state-merge-macos-arm64.json` and its 62-check audit. No updated speedup or performance ceiling is inferred from this correctness merge. Measure the installed merged baseline before the reusable callback policy-deferral experiment.


## Reusable callback policy deferral (2026-10-29)

Proven native-v3 scalar callbacks reached through reusable v4/v5/v9 wrappers now defer their function-reference option override until the existing error materialization path. A private invocation-owned context saves and restores all policy fields, including inherited restrictions and module identity, before restoring caller runtime options. Same-owner non-module behavior matches the ordinary override helper. Ready-context, signature, exact-body, Program, debugger, pending-exception and physical-stack checks remain live, as do diagnostic publication and reader completion. Other typed entries and fallbacks retain eager policy.

The generated frame ABI remains 208 bytes with 192 bytes of opaque storage; the diagnostic frame borrows its cache through the cold context instead of storing a duplicate pointer. Both Release and Debug enforce the size bound. Optimized shared-artifact measurements show 36.8–38.0% less time for the imported same-Program indirect-call workload, from about 13.90 to 8.62–8.78 ns/item. Preserve the measured reverse-layout captured-closure cost (about 1.2–1.4 ns/item) in tradeoff assessments. See `bench/runtime-comparison/results-callback-policy-deferral-macos-arm64.json` and its full audit. Cross-Program native method policy and local dispatch admission remain investigation targets; Java parity and a global ceiling are not established.


## DGC/HTTP upstream integration (2026-10-29)

The performance branch integrates develop 38c885a6ff while retaining the reusable callback policy optimization. Collector scan-completion waits are registered with the fork handlers before threads start; HTTP graph edges avoid scanning all open connections per request. Release and Debug qualification and an optimized interleaved merge checkpoint are recorded in `bench/runtime-comparison/results-post-callback-merge-macos-arm64.json`. Imported pure-method context and finite receiver specialization remain next leads; no Java parity or global performance ceiling is established.


## Method-cache layout experiment rejected (2026-10-29)

Grouping the native scalar cache key reduced its footprint but not the optimized helper instruction count. The 576-process/1728-window screen showed no useful target benefit: local polymorphic 23.8441 to 24.1236 ns/item, all three layout ranges overlapping. The original header and byte-identical optimized runtime were restored. Release/Debug each pass 25 fixtures (42 cases/10099 assertions plus three silent fixtures), with 6072 references per runtime. QoreAOT.h changes require a new AOT identity and matching module compilation even when workload LLVM is unchanged.

Profiles still identify receiver-local ownership and cross-Program entry/exit as major costs. A combined RuntimeConfig getter offers no call removal because the getters already inline in the unity build. See `bench/runtime-comparison/results-method-cache-layout-rejected-macos-arm64.json` and its 62-check audit. Refresh the optimized develop/branch/Java comparison before the next distinct hypothesis; parity and a global ceiling remain unestablished.


## Fresh optimized branch/develop/Java checkpoint (2026-10-29)

The targeted 32-workload checkpoint compares branch 85795742dc against develop 38c885a6ff and 25 fresh Java rows. All 651 processes/1953 windows replay; all 96 Qore layout cells favor the branch with separated observed ranges. Known integer callbacks are 1.18 times Java, direct/final kernels about 1.52 to 1.58 times, local polymorphic 9.14 times, imported polymorphic 32.07 times. Mixed CSV/orders/callback/text workloads take 16.43%, 8.56%, 71.68% and 25.04% less time than develop. Whole-kernel units, optimizer policies, Program contexts and shared-host limits are documented in `bench/runtime-comparison/results-post-layout-rejection-checkpoint-macos-arm64.json` and its 62-check audit. No universal parity or global maximum is claimed.

Next investigate the repeated exception-sink reads in fast scalar loops. Only bypass a read when an existing successful check dominates the path and no intervening operation can change the sink. Keep all cancellation polls, handler-path checks, thread-exit/deferral semantics and diagnostic materialization. No invariant-load annotation or blanket check removal. Then revisit receiver-local ownership and cross-Program pure native context with explicit lifetime and cleanup proofs.


## Proven-clear cancellation checks in AOT (2026-10-29)

Retain the conservative AOT pass for consecutive cancellation checks with no successor PHIs. Unknown calls and writes invalidate the clear-sink proof; all polls, slow handling and diagnostics remain.

Floating-loop accumulation improves 1.69% in the 32-workload screen (5.1333 to 5.0467 ns/item) and 2.29% in independent confirmation (5.1720 to 5.0537). The same-runtime disabled/enabled comparison improves 1.55% (5.1333 to 5.0537). This is a narrow kernel benefit; direct/simple and indirect calls gain no established additional speed.

The former reverse indirect regression does not repeat: final confirmation is -1.23% versus original and -0.04% versus disabled, both overlapping ranges. Local effect time rises 2.75% in the screen but falls 0.46% in the adverse follow-up; the earlier separated default-layout cost does not repeat. CSV rises 1.48% and 1.00%, with all layout ranges overlapping; imported boxed-integer time rises 0.54% and 1.18% versus original, and 2.58% versus the disabled control in the follow-up, again with overlapping ranges. Preserve these possible costs without a neutrality or weighted application-speedup claim. No slower separated-range control in the screen repeats as a slower separated-range cell in its confirmation.

Release/Debug each pass 32 fixtures (56 cases/10371 assertions plus three silent), including 37 native proof cases; Debug 3237 focused/4899 broader checks overlap. All 391 Release aliases restored. All 954 final measured processes/2862 windows independently replay, and broader rejected prototype evidence is retained. See `bench/runtime-comparison/results-clear-cancellation-sink-macos-arm64.json` and its 62-check audit. Next merge/install, then evaluate the redundant deferred-method sink check; no fresh Java ratio or global ceiling.


## Deferred-call application identity after develop integration (2026-10-29)

Develop 9fd78f4803 adds the application identity used by module-owned asynchronous calls and timers. Deferred scalar callbacks and closures save and restore that identity only when they materialize runtime context. Reusable callback frames retain their fixed generated-code storage: the saved ID occupies existing leaf-helper padding. The interpreter continues to capture the actual caller before switching Programs.

The integration also includes DGC root-lock release while waiting for a non-scan holder and restoration of the HTTP controller logger when a server stops. Module-owned asynchronous regression probes now set child Program execution modes explicitly, check child exits and native traces after module initialization, and verify a third repeated call through the native tier. See `bench/runtime-comparison/results-post-clean-sink-merge-macos-arm64.json` and its 62-check audit. This integration adds no new performance measurement; the next experiment targets the redundant deferred native-method sink check.


## Redundant native-method sink check rejected (2026-10-29)

The optimized merged baseline 6c2228b28f and candidate share 15 AOT artifacts. Removing a source-proven redundant deferred-method sink check saves five or six static instructions but gives no target benefit: same-Program polymorphic 24.4386 to 24.4580 ns/item (+0.08%), all three observed layout ranges overlapping. Reject the runtime change after the 32-workload/576-process/1728-window screen; no confirmation or speedup claim. Preserve all control observations.

The added C++ pending-exception/thread-exit controls remain and pass on the baseline. Release and Debug candidate qualification each passes 31 cases/6483 assertions plus two silent fixtures; native Debug 3395/5057 checks overlap. Restored Release qore/qcc/libqore exactly match baseline bytes, restored Debug passes the new controls, and 391 Release aliases are unchanged. System installation remains 6c2228b28f. See `bench/runtime-comparison/results-native-method-sink-rejected-macos-arm64.json` and its 62-check audit. Next examine receiver-local ownership bookkeeping with runtime slots and destructor order preserved; no global ceiling or new Java ratio.


## Debugger and diagnostic integration (2026-10-29)

Develop c0391665d7 adds explicit source IR/JIT statement events, debugger callback caller options and hidden runtime temporaries, corrected debugger command/session handling, and compiler-checked printf-style APIs. The merge keeps branch call paths and rejects native shortcuts when either the execution Program or code owner has an attached debugger. DebugStatement is opcode 414; branch opcodes 409-413 remain unchanged.

AOT debug IR through binary format v22 carries line-based PushTempMark boundaries. The interpreter preserves that format's existing stepping behavior with an immutable reader-set compatibility flag; source-lowered IR/JIT uses the new explicit statement events. This does not extend the AOT format or claim statement-complete AOT debugging. Four standalone binaries compiled before the compatibility fix fail with 32 entries/32 exits/0 local-step observations and pass unchanged afterwards. Fresh stripped and embedded AOT callback artifacts also preserve all 32 observations.

See `bench/runtime-comparison/results-post-debug-format-merge-macos-arm64.json` and its complete 62-check audit. After full-suite qualification, two branch counter diagnostics exposed by the printf annotations use matching unsigned-long-long conversions. A targeted Debug rebuild and IR/JIT level5 probes validate that diagnostic-only amendment; Release printd removes both calls. This merge has no new performance measurement. The next optimization experiment targets class-local receiver release while retaining ordinary runtime ownership, destructor order, custom reference handlers and tracing semantics; it requires a fresh optimized baseline after installation.


## Combined class-local real-reference release (2026-10-29)

`qore_rt_clear_class_local_slot_aot_v1` publishes cleared/unassigned state, then tries the canonical combined real-reference release for an exact ordinary, reference-counted C++ QoreObject with another real reference and no active reference diagnostics. The real-count load is an admission hint; the existing release algorithm handles races and final deletion. A successful attempt consumes ownership and forbids further object/slot access. Custom C++ handlers, singleton nodes, non-real owners, tracing/debug hooks and rejected admission keep the split release. Runtime local ownership and destructor order are preserved. The AOT entry signature and serialized identity are unchanged; compatible existing AOT artifacts benefit from the runtime upgrade.

Two optimized same-artifact campaigns retain the narrow gain: confirmation local polymorphic integer/float times improve 3.91%/3.07%; imported counterparts improve 1.15%/1.36%. Plain closures rise 1.71%, captured closures 0.83% in aggregate, with layout-dependent effects; application kernels stay within 0.6% without an established aggregate gain. The broader tradeoff is explicit, and no Java parity or global ceiling is claimed. See [measurements and controls](../bench/runtime-comparison/results-aot-class-local-release-macos-arm64.json) and [the complete audit](../bench/runtime-comparison/audit-aot-class-local-release.md).

Further work should first quantify TLS in the canonical release routine, then consider an explicit same-thread invocation context. Thread cleanup unregisters its hazard slot before destroying ThreadData, so RuntimeConfig validity or TID alone must never admit an unregistered slot. Receiver borrowing and finite-target inlining need separate proofs for fallback materialization, reflection, custom handlers, destructor ordering and concurrent deletion. Any inlining comparison with Java must include natural Java inlining as well as the existing forced-boundary controls.


## Program IDs and warmed AOT closures (2026-10-29)

Develop 8004006ee2 is integrated after the class-local release optimization. Deferred native callback and closure contexts preserve the new 64-bit Program IDs, including materialized slow paths. Debug activity tracking moves into existing helper padding so reusable AOT callback storage remains 192 bytes within the 208-byte public frame.

Warmed closure IR can assign local slots differently from fresh native lowering. Native lowering now seeds its slot identities from that cached IR before lowering the body and nested handlers. The regression exercises mixed local types, captures and same-named locals in sibling cleanup scopes. This changes compile-time metadata alignment and adds no per-call lookup.

The reader disables legacy step synthesis when serialized IR contains explicit DebugStatement records. Native AOT debug IR without such records retains its existing cleanup-line boundaries; this change does not claim statement-complete native AOT debugging.

Release: 110 executed QUnit cases/12841 assertions, 12 runtime skips; Debug: 113 executed cases/12852 assertions, 9 runtime skips. Each tree runs 40 fixtures including 3 additional silent fixtures. AOTSmoke/HTTP filters exclude 263 cases in Release and 263 in Debug; these are not executed cases. Native Debug passes 3770 focused and 5435 broader assertions (overlapping). Trace, DGC lock-counter and compiled concurrent-reference controls pass; all 391 Release qmod aliases are restored.

See [integration evidence](../bench/runtime-comparison/results-post-program-id-merge-macos-arm64.json) and the [complete audit](../bench/runtime-comparison/audit-post-program-id-merge.md). Oracle database-dependent checks are explicitly skipped without a connection; no live query-plan result is claimed. The class-local release measurements remain premerge results, with no fresh Java/develop ratio here.

After the requested optimized installation, the next experiment separates the object-release slow path so successful releases need not allocate its large stack frame. Actual optimized assembly shows six saved register pairs, 912 additional stack bytes and a stack canary before the fast-release test. This is a hypothesis to measure; TLS hazard-context passing remains a separate lifetime-sensitive candidate.


## Object-release slow-body outlining (2026-10-29)

The ordinary object-release entry now handles diagnostics and the canonical fast release before entering a separate noinline cleanup helper. The helper retains the exact weak-allocation guard, recursive-set processing, locks, destructor ordering and exception handling. An explicit bool carries the already-released-last result, avoiding a repeated decrement. No refcount/hazard algorithm, object layout, AOT symbol signature or runtime identity changes. Compatible existing AOT artifacts benefit from the runtime upgrade; other execution modes share this runtime path but their gains are not measured here.

On this optimized Apple M4 Pro build, the successful-release stack frame falls from 1008 to 48 bytes. The slow path retains its 1008-byte frame and is reached by a tail branch. The compiler may inline the small wrapper; that decision remains enabled in the retained version.

The 33-workload screen uses 3 forks and the confirmation 5 forks per each of three link layouts. All 594/990 processes and 4752 measured windows replay independently, using identical O3 artifacts and frozen runtimes. Values below are whole-kernel ns/item; negative time change means faster. Before is the merged branch c735fd76a8, not pure develop.

|!Workload|!Before ns/item|!Retained ns/item|!Screen time change|!Confirmation time change
|local:direct_call|1.5136|1.5109|-0.71%|-0.18%
|local:method_call|1.5697|1.5554|-0.61%|-0.91%
|local:polymorphic_call|22.7711|21.3084|-4.49%|-6.42%
|local:polymorphic_float_call|22.5236|21.7183|-4.30%|-3.58%
|local:boxed_int_method_fallback|32.0834|32.7984|+2.35%|+2.23%
|local:boxed_float_method_fallback|34.7663|33.9481|-2.46%|-2.35%
|local:boxed_int_method_effect|88.7397|89.5435|+0.37%|+0.91%
|local:list_mutate|119.1616|118.6075|+0.09%|-0.46%
|local:temporary_list|1.5976|1.5890|-0.61%|-0.54%
|micro:polymorphic_call|81.6590|80.9551|-1.51%|-0.86%
|micro:polymorphic_float_call|83.3265|82.0476|-0.67%|-1.53%
|micro:boxed_int_method_call|105.1260|104.9044|-0.92%|-0.21%
|micro:boxed_float_method_call|112.6019|110.5392|-0.99%|-1.83%
|micro:boxed_int_method_fallback|117.1491|117.8624|-1.07%|+0.61%
|micro:boxed_float_method_fallback|123.6947|122.9297|-0.83%|-0.62%
|micro:boxed_int_method_effect|173.1315|171.7911|+0.57%|-0.77%
|micro:indirect_call|8.3453|8.5102|+0.51%|+1.98%
|micro:closure_call|17.9393|17.8330|+3.30%|-0.59%
|micro:closure_capture|23.3157|23.6800|-0.43%|+1.56%
|micro:list_read|2.8203|2.8173|-0.26%|-0.11%
|micro:list_build|23.0907|21.9532|-5.14%|-4.93%
|micro:list_mutate|118.7450|118.1417|-0.33%|-0.51%
|micro:temporary_hash|1.6022|1.5941|-0.56%|-0.51%
|float:callback_local_float_return|0.8342|0.8332|-0.81%|-0.12%
|float:typed_local_int_unary|1.1654|1.1565|-0.46%|-0.76%
|float:callback_loop_float_return|4.8398|4.8660|+0.03%|+0.54%
|mixed:csv_report|911.1886|909.1423|-0.46%|-0.22%
|mixed:object_orders|3078.5413|3115.1394|+2.56%|+1.19%
|mixed:callback_pipeline|91.4636|91.7313|+0.42%|+0.29%
|mixed:text_index|1315.7415|1324.0531|-0.47%|+0.63%
|probe:object_short|1603.2651|1593.5721|+0.36%|-0.60%
|probe:effect_short|1664.1896|1666.5668|-0.24%|+0.14%
|probe:string_result_loop|522.3263|521.6197|-1.60%|-0.14%

The narrow gains are retained under the authorized broader-tradeoff evaluation. Same-Program integer/float polymorphic calls improve in both full campaigns; list building also improves. The boxed integer fallback regression repeats in all layouts, and object-orders has a smaller aggregate regression with mixed layout directions. No weighted application benefit is inferred. The initial plain-closure slowdown does not repeat in confirmation.

A separate variant also marked the small wrapper noinline. In a 270-process interleaved comparison, its boxed-fallback aggregate improves relative to the first candidate, but local integer/float polymorphic calls are 1.08%/1.68% slower, list building 2.07% slower and object-orders 1.00% slower than the retained candidate. It is rejected at the predeclared early screen; no broad performance claim is made for it. A server restart interrupted the first attempt after 120 of 270 jobs. That incomplete campaign is preserved and excluded in full; the complete retry is independently verified.

Retained v1 Release and Debug each pass 20 fixtures: 45 QUnit cases/6807 assertions plus 2 silent fixtures. Native Debug passes 3770 focused and 5435 broader assertions (overlapping), with tracing, lock counters, three deterministic collection races and compiled concurrent references. Both optimized runtimes pass 9108 reference outputs against 15 identical artifacts. V2 separately passes 13662 references and eight focused fixtures per tree (23 cases/343 assertions), plus native/race controls. Restoring v1 produces byte-identical Release qore/qcc/libqore binaries. Debug byte identity failed despite exact production source restoration, so the complete Debug qualification was rerun successfully on the current binaries. No old Debug binary equivalence is claimed. All 391 Release qmod aliases remain intact.

See the [complete evidence](../bench/runtime-comparison/results-aot-release-outlining-macos-arm64.json) and [62-check audit](../bench/runtime-comparison/audit-aot-release-outlining.md). Shared-host conditions remain uncontrolled; observed ranges are not confidence intervals. There is no new Java/develop ratio or maximum-performance claim.

The next distinct experiment can pass the already available same-thread RuntimeConfig view to a versioned AOT cleanup helper, avoiding reacquisition of hazard context through TLS. It must preserve canonical registration/unregistration, teardown fallback, TID reuse and the shared release algorithm. ThreadData existence alone does not prove an active hazard slot. This remains research, with no implemented or measured saving yet.


## Deferred closures and debugger launch integration (2026-10-29)

Develop e1abc01cb4 is integrated after object-release outlining 45089b1a9e. Closures evaluated inside constant expressions complete deferred parsing before execution; the later deferred pass is idempotent. Debug-adapter launch now waits for configurationDone before allowing the target to run, preserving breakpoints installed during configuration. The request/event handshake uses the existing adapter lock. Incoming test improvements validate scheduler-independent socket affinity and debugger responses, and distinguish delayed adapter output from process exit.

The constant-closure regression now checks child exit status as well as expected values, and adds stripped-source AOT execution at O0/O3 to AST/IR/tiered/JIT coverage. The new AOT cases exposed undefined native closure dispatch symbols from constant initializers at both optimization levels. Typed closure lowering now honors the existing owner capability flag, as the boxed route already did. Initializer owners use the generic helper because they emit no native closure bodies; ordinary function owners retain native dispatch. The configurationDone handler documents its request contract. The changed VscDebugAdapter qmod is rebuilt in each tree before testing.

Repeated adapter checks also exposed a ProgramControl cache lifetime race: a lookup could increment a zero reference count while final release was already committed to deletion. The callback then received a deleted wrapper and the target ran without its initial debugger stop. Cached acquisition now uses a positive-count compare-and-swap; retiring wrappers are replaced, and their destructors clear the cache only if it still points to them. A deterministic Debug test holds final release before unregistering and covers both single lookup and enumeration, including replacement identity after the old destructor finishes. It failed on the old runtime before this fix. The ordinary strong-reference call path is unchanged.

build: 78 executed QUnit cases/1957 assertions, 4 additional silent fixtures, 0 QUnit skips, 1 Debug-hook skips; build-debug: 78 executed QUnit cases/1957 assertions, 5 additional silent fixtures, 0 QUnit skips, 0 Debug-hook skips. Each tree runs 25 fixtures; the deterministic ProgramControl fixture runs its two interleavings in Debug and explicitly skips in Release. Native Debug passes 3770 focused and 5435 broader assertions (overlapping); trace, DGC lock counters and compiled concurrent references pass. All 391 Release qmod aliases are restored. The unmodified Release adapter test passes 50 repeats (250 cases/3800 assertions) after the cache fix.

See [integration evidence](../bench/runtime-comparison/results-post-outlining-merge-macos-arm64.json) and the [62-check audit](../bench/runtime-comparison/audit-post-outlining-merge.md). These checks establish integration correctness, not a new performance comparison. The outlining figures remain premerge measurements. The next step is the requested optimized committed build and system installation, followed by the explicit RuntimeConfig release-context experiment with a newly frozen baseline.


## Explicit AOT release context rejected (2026-10-29)

The candidate passed the already acquired RuntimeConfig to a versioned AOT class-local cleanup helper. A same-thread live view followed canonical hazard registration/unregistration, including teardown and reused thread IDs. The fast-release algorithm stayed textually identical apart from slot selection. Optimized assembly removed its TLS resolver call, but added context loads and increased the cleanup helper frame from 64 to 80 bytes. Both helper versions and their fallbacks were tested.

The prospectively allowed early pilot does not justify retaining this implementation. Against optimized merged 0800e5e231, same-Program integer polymorphic time improves only 0.82% with mixed layout directions; floating-point polymorphic time rises 1.62%. Imported calls and boxed-float fallback improve, but indirect calls and list building cost 4.94%/4.70%, with separated observed ranges in all three layouts. This is an unfavorable target/control tradeoff, not a zero-regression rule or an application-weighted conclusion. The whole candidate is rejected and removed. No full 33-workload campaign or confirmation was run.

All 198 processes/594 windows replay independently. Both runtimes are Release (`-O3 -DNDEBUG`), allocation tracking off; each has 15 fresh identity-matched stripped O3 artifacts. Three layouts and three forks use three warmup and three 150 ms measurement windows per process. Values are whole-kernel ns/item, not isolated call latency.

|!Workload|!Baseline ns/item|!Rejected candidate ns/item|!Time change
|local:direct_call|1.4703|1.4863|+1.09%
|local:method_call|1.5447|1.5578|+0.85%
|local:polymorphic_call|21.5993|21.4220|-0.82%
|local:polymorphic_float_call|21.4939|21.8425|+1.62%
|local:boxed_int_method_fallback|32.5441|32.0878|-1.40%
|local:boxed_float_method_fallback|34.8888|33.6649|-3.51%
|micro:polymorphic_call|81.5456|79.7939|-2.15%
|micro:polymorphic_float_call|82.2594|81.3247|-1.14%
|micro:indirect_call|8.3771|8.7913|+4.94%
|micro:list_build|21.9850|23.0174|+4.70%
|mixed:object_orders|3067.2691|3103.5360|+1.18%

Baseline and candidate each pass 6,072 reference outputs. Release/Debug each pass 22 fixtures with 45 QUnit cases/6,807 assertions. Release additionally has 3 silent fixtures and 1 explicit Debug-hook skip; Debug has 4 silent fixtures. Native Debug passes 5,221 focused/6,886 broader assertions, with overlapping coverage. Live/null/empty/unregistered contexts, foreign-thread teardown and TID reuse, tracing, lock counters, collector races and compiled concurrent references pass. The first Debug build failed because the new test heap-allocated a stack-only foreign-thread helper; the corrected stack-based test passes. Release rebuilds after that test-only correction are byte-identical. The initial summary missed the uppercase SKIP: marker; the published review corrects that count from raw output.

Production source is restored exactly, Release qore/qcc/libqore are byte-identical to the validated baseline, and 391 Release qmods are rebuilt. The installed 0800e5e231 build is unchanged. Current build-debug is candidate output and must be rebuilt before reuse; no restored Debug binary equivalence is claimed. All failed attempts and final candidate patch remain in the [evidence](../bench/runtime-comparison/results-aot-release-context-rejected-macos-arm64.json); see the [62-check audit](../bench/runtime-comparison/audit-aot-release-context-rejected.md).

No fresh Java/develop ratio or global ceiling is established. The next investigation measures the existing expression-only/monomorphic controls alongside the unchanged original receiver-local kernels. That source-level decomposition can guide a separate borrowing proof, but is not itself a runtime speedup. Any ownership optimization must preserve live runtime locals, fallback materialization, debugger/reflection behavior, concurrent deletion, cancellation and destructor order; the earlier native-only local prototype failed exception-path destructor order.


## Receiver source-form decomposition (2026-10-29)

This diagnostic uses unchanged optimized merged 0800e5e231: Release `-O3 -DNDEBUG`, allocation tracking off, identity-matched stripped O3 AOT artifacts. The original receiver-local workloads remain alongside existing expression-only, monomorphic and optional-receiver controls. All 198 processes/594 windows replay independently: 22 workloads, three layouts and three forks, each with three warmup and three measured 150 ms windows at 4096 items. The frozen artifacts previously passed 6072 reference outputs; runtime/source/dependency identities are rechecked. Results are medians of layout medians of process means, in whole-kernel ns/item.

|!Source form|!Same Program ns/item|!Imported ns/item
|direct_call|1.4698|1.4820
|method_call|1.5493|1.5596
|polymorphic_call|21.9954|81.2933
|polymorphic_expression_call|10.5334|70.1805
|monomorphic_call|3.8218|3.7335
|optional_method_call|10.3024|98.3835
|polymorphic_float_call|22.1451|81.6951
|polymorphic_float_expression_call|10.6173|71.9654
|monomorphic_float_call|3.9943|4.0446
|optional_float_method_call|10.6716|99.1367
|dispatch_control|1.8127|1.8210

The integer/float receiver-local forms take 21.9954/22.1451 ns/item; their expression forms take 10.5334/10.6173. The roughly 11.5 ns/item difference motivates further local-slot work, but it is not an isolated measurement of reference-count cost or a runtime optimization. Source shape also changes generated branches, caching and cleanup. Static LLVM inspection finds prepared local assignment and cleanup in the original forms, absent from the expression forms; static site counts are not dynamic call counts.

Exact-class monomorphic calls take 3.8218/3.9943 ns/item locally and 3.7335/4.0446 when imported. Generated code can guard and inline a proven exact target. Polymorphic expressions retain dispatch; optional receivers lack the same parse-selected target. The existing immutable PIC already holds four entries, covering the three benchmark targets. Cache capacity is therefore not this workload's missing optimization. Native cached admission deliberately requires caller, receiver and execution Program identity; imported ordinary dispatch retains Program transition and native retry work.

The next experiment targets repeated class acceptance at pinned local initialization. A narrow positive proof may be prepared once for an already-owned entry receiver, with identity checked at every use; private/access-dependent and parameterized constraints must keep live acceptance. Runtime slot ownership, custom reference handlers, real-reference marks, reflection, exceptions and ordered cleanup must remain. Do not revive the rejected native-only local scheme. Pure method invocation already outlines member/eager paths; its remaining frame contains live diagnostic/context state, so no stack-frame reduction is inferred from outlining again.

The shared M4 Pro/macOS host has no affinity/frequency/thermal control. No owned build, test, profiler or heavy scan overlapped timing; observed process ranges are not confidence intervals. There is no new Java/develop comparison, implementation speedup or maximum-performance conclusion. The installed optimized 0800e5e231 runtime is unchanged. See [complete raw evidence](../bench/runtime-comparison/results-receiver-source-forms-macos-arm64.json) and the [62-check audit](../bench/runtime-comparison/audit-receiver-source-forms.md).


## Pinned class acceptance rejected after broad evaluation (2026-10-30)

The candidate prepared positive exact-class/first-public-base acceptance for independently retained entry receivers. An identity guard selected a versioned initializer; private, structural and parameterized cases retained live acceptance. Raw-generic metadata, slot ownership, real-reference marks, custom handlers and ordered cleanup remained live. This experiment moved a narrow class-relation check out of the loop; it did not eliminate ownership traffic.

The first measured version selected a per-object proof on each initialization and enlarged local polymorphic caller frames from 576 to 592 bytes. Refinement conjoined all possible source proofs at entry, restoring 576-byte frames and retaining live v4 fallback whenever any proof was unavailable. The initializer shrank from 137 instructions/64 frame bytes to 111/48. These static counts alone did not establish a performance benefit. The first build failed because a unity-only inline macro was unavailable in the separate runtime source. The second exposed invalid LLVM dominance after promoted closure captures moved the logical entry; the candidate was restricted to same-block definition order and a regression fixture added. Both failures and final patches are archived.

Two pilots (234 and 351 processes) showed modest polymorphic improvements and adverse indirect/list-building controls. A 162-process comparison rebuilt the refined artifacts from the baseline's exact source paths and used its host-script paths. That did not eliminate the costs. Normalized control LLVM bodies and inspected helper sizes matched, but this does not establish complete machine-code equivalence or explain the timing differences. All earlier variants and measurements remain in the evidence.

The final 35-workload screen uses those common-source artifacts: 630 processes/1890 windows, three layouts and three fresh processes per cell, three warmup and three measured 150 ms windows, 4096 items. Both runtimes use Release `-O3 -DNDEBUG`, allocation tracking off; artifacts are identity-matched stripped O3 builds. Baseline is merged performance-branch 0800e5e231, not pure develop. Values below are whole-kernel ns/item, computed as the median of layout medians of process means.

|!Workload|!Baseline ns/item|!Rejected candidate ns/item|!Time change
|local:direct_call|1.4744|1.4667|-0.52%
|local:method_call|1.5456|1.5525|+0.45%
|local:polymorphic_call|21.3921|20.8212|-2.67%
|local:polymorphic_float_call|21.9423|20.9771|-4.40%
|local:boxed_int_method_fallback|32.9367|31.8660|-3.25%
|local:boxed_float_method_fallback|34.4264|35.0415|+1.79%
|local:boxed_int_method_effect|88.1292|88.6285|+0.57%
|local:list_mutate|117.3033|119.7197|+2.06%
|local:temporary_list|1.5756|1.5736|-0.13%
|local:polymorphic_expression_call|10.3758|10.4333|+0.55%
|local:polymorphic_float_expression_call|10.5139|10.4797|-0.32%
|micro:polymorphic_call|81.9024|79.1801|-3.32%
|micro:polymorphic_float_call|82.2722|80.5191|-2.13%
|micro:boxed_int_method_call|107.3504|105.3829|-1.83%
|micro:boxed_float_method_call|113.3045|112.8659|-0.39%
|micro:boxed_int_method_fallback|117.9146|118.9746|+0.90%
|micro:boxed_float_method_fallback|122.8310|123.8013|+0.79%
|micro:boxed_int_method_effect|172.1677|172.6081|+0.26%
|micro:indirect_call|8.2509|8.6867|+5.28%
|micro:closure_call|17.8548|18.0345|+1.01%
|micro:closure_capture|23.4900|23.3839|-0.45%
|micro:list_read|2.7843|2.7691|-0.54%
|micro:list_build|22.1756|22.6544|+2.16%
|micro:list_mutate|116.8224|119.2051|+2.04%
|micro:temporary_hash|1.5804|1.5889|+0.54%
|float:callback_local_float_return|0.8221|0.8172|-0.60%
|float:typed_local_int_unary|1.1513|1.1550|+0.32%
|float:callback_loop_float_return|4.8071|4.8230|+0.33%
|mixed:csv_report|891.2790|889.1367|-0.24%
|mixed:object_orders|3042.4737|3111.8096|+2.28%
|mixed:callback_pipeline|91.5424|92.9246|+1.51%
|mixed:text_index|1294.7479|1289.1158|-0.43%
|probe:object_short|1587.4497|1569.2749|-1.14%
|probe:effect_short|1633.5449|1640.3915|+0.42%
|probe:string_result_loop|529.0078|521.6929|-1.38%

Reject this implementation under the authorized broader-tradeoff evaluation. Same-Program integer/float polymorphic calls improve 2.67%/4.40% and imported counterparts 3.32%/2.13%, but indirect calls cost 5.28%, list building 2.16%, list mutation about 2%, and the mixed callback pipeline 1.51%. List-building and callback-pipeline observed process ranges are separated adversely in all three layouts; indirect ranges separate adversely in two. Object-orders rises 2.28% with mixed layout directions. These narrow gains do not justify the broader costs and added mechanism. This is a judgment about this implementation, not a zero-regression rule, application-weighted conclusion or proof that compile-time acceptance cannot help.

All four campaigns replay independently: 1377 processes/4131 windows. Each of three candidate artifact preparations passes 6072 reference outputs (1518 IR, 4554 AOT). Both measured versions pass 26 Release fixtures with 50 cases/10632 assertions, 3 silent fixtures and 1 explicit Debug-only skip. Candidate native Debug qualification and a separate final confirmation were not run after rejection. The common-source preparation driver terminal response was not retained across compaction; its exit code is explicitly unknown, while all child process receipts, reference outputs and the completed marker were independently checked.

All seven candidate source/test files are restored exactly. Rebuilt Release qore/qcc are byte-identical to the validated baseline. The initial whole-library identity assertion failed: all library section addresses/sizes and bytes match except one 40-byte git revision string for the newer documentation commit. The AOT runtime identity matches, and all 391 Release qmod aliases are rebuilt. No whole-library byte equivalence is claimed. The installed optimized 0800e5e231 runtime is unchanged at this checkpoint. build-debug remains stale output from the preceding rejected explicit-release-context experiment and must be rebuilt before reuse. See the [complete evidence](../bench/runtime-comparison/results-pinned-class-acceptance-rejected-macos-arm64.json) and [62-check audit](../bench/runtime-comparison/audit-pinned-class-acceptance-rejected.md).

Next profile imported pure native-method setup after the ordinary Program transition. Current retry revalidates the live body and uses eager object/class and logical-frame setup. A deferred alternative must borrow stable Program-owned metadata rather than retain the caller PIC across reentrant Program entry, preserve caller identity, restrictions, application identity, diagnostics and cleanup, and avoid adding storage or traffic to the same-Program path. This is a new hypothesis, with no implemented or measured gain yet.

Shared-host frequency, affinity and thermal state remain uncontrolled; no owned build/test/profile or heavy scan overlapped timing. Observed ranges are not confidence intervals. No fresh Java/develop ratio or global performance ceiling is established.


## Deferred pure native context after Program entry (2026-10-30)

Pure native method-v2/v3 retries now defer object/class substitution and empty variable-frame markers after the full ordinary Program transition. An invocation-owned descriptor borrows live Program-owned variant, defining-class and receiver metadata; it never retains the caller's replaceable dispatch cache across reentrant initialization. The logical frame keeps the original caller Program. A failing native stack guard materializes both native ThreadData and RuntimeConfig context before constructing an exception, and each view restores its own saved values. Full Program thread counts, locks, initialization, caller option/application overrides, live body/signature/debugger checks, ownership and diagnostic reader synchronization remain.

`QORE_DISABLE_AOT_CROSS_PROGRAM_DEFERRED_CONTEXT=1` restores the eager path at runtime, as does the common native-method deferred-context control. These are presence flags sampled once; empty and `0` values also disable the optimization. Existing v1-only and unsupported entries retain eager setup. No exported helper signature or public RuntimeConfig layout changes. This benefits existing native AOT method bodies, including invocations through IR/JIT hosts; it adds no companion to IR/JIT-only bodies.

The cross-Program helper's optimized stack frame grows from 384 to 480 bytes. Existing leaf/method/callback/closure context storage remains 96/56/72/152 bytes, and the inspected same-Program integer/float helpers keep their 304-byte frames and instruction counts. Static counts alone are not a speedup estimate. Canonical scalar argument cleanup is tied to the same argument SSA value and cannot run a node destructor after the deferred frame exits; no general ownership or cleanup elision is introduced.

Measurements compare frozen performance-branch 06ef4cae3b against the candidate. Both runtimes are Release `-O3 -DNDEBUG`, with allocation tracking off. Both freshly compiled sets of fifteen stripped O3 artifacts are byte-identical and use the same runtime identity. Timed variants use the exact same executable/AOT artifact and host paths, changing only the runtime library and, for attribution, the disabling flag. Each campaign uses three layouts and three fresh processes per cell, three warmup and three measured 150 ms windows, 4096 items, seed 7, with randomized variant ordering. Values are whole-kernel ns/item, computed as median of layout medians of process means, not isolated call latency.

The initial 16-workload pilot (288 processes/864 windows) improves six imported scalar targets by 3.37–4.95%. A local integer-expression control rises 2.57%, so qualification and controls were broadened. The 37-workload screen follows:

|!Workload|!Baseline ns/item|!Candidate ns/item|!Time change
|local:direct_call|1.5392|1.5438|+0.30%
|local:method_call|1.6308|1.6184|-0.76%
|local:polymorphic_call|22.4087|22.5471|+0.62%
|local:polymorphic_float_call|22.7396|22.6750|-0.28%
|local:boxed_int_method_fallback|34.0893|34.1313|+0.12%
|local:boxed_float_method_fallback|36.7556|36.4746|-0.76%
|local:boxed_int_method_effect|92.3432|91.6076|-0.80%
|local:list_mutate|124.2945|122.4015|-1.52%
|local:temporary_list|1.6309|1.6496|+1.14%
|local:polymorphic_expression_call|11.0379|11.0985|+0.55%
|local:polymorphic_float_expression_call|11.0895|11.1472|+0.52%
|micro:polymorphic_call|85.3740|82.5547|-3.30%
|micro:polymorphic_float_call|86.4041|83.7563|-3.06%
|micro:boxed_int_method_call|114.2687|111.5053|-2.42%
|micro:boxed_float_method_call|118.2942|113.9617|-3.66%
|micro:boxed_int_method_fallback|123.7157|123.5944|-0.10%
|micro:boxed_float_method_fallback|130.0390|127.6060|-1.87%
|micro:boxed_int_method_effect|178.8605|180.6072|+0.98%
|micro:indirect_call|8.4477|8.8987|+5.34%
|micro:closure_call|18.6706|18.6198|-0.27%
|micro:closure_capture|24.2708|24.3947|+0.51%
|micro:list_read|2.8980|2.9077|+0.34%
|micro:list_build|23.4535|23.2867|-0.71%
|micro:list_mutate|123.7616|125.1953|+1.16%
|micro:temporary_hash|1.6467|1.6607|+0.85%
|micro:polymorphic_expression_call|74.6699|71.4640|-4.29%
|micro:polymorphic_float_expression_call|76.3207|72.3125|-5.25%
|float:callback_local_float_return|0.8729|0.8679|-0.58%
|float:typed_local_int_unary|1.1995|1.2261|+2.21%
|float:callback_loop_float_return|5.0187|5.0384|+0.39%
|mixed:csv_report|955.5184|960.4542|+0.52%
|mixed:object_orders|3367.6314|3446.8883|+2.35%
|mixed:callback_pipeline|96.6701|95.4257|-1.29%
|mixed:text_index|1370.6477|1398.0216|+2.00%
|probe:object_short|1699.2207|1700.1347|+0.05%
|probe:effect_short|1746.1189|1764.4961|+1.05%
|probe:string_result_loop|564.2240|553.5348|-1.89%

The screen's imported targets improve 2.42–5.25%, but indirect calls rise 5.34%, object-orders 2.35%, and text-index 2.00%. The independent repeat therefore expands to twenty workloads and three variants (540 processes/1620 windows). The disabled variant uses the same candidate library and artifacts, restoring eager context. It still executes the new flag checks, so it attributes the deferred path without exactly recreating the baseline binary.

|!Workload|!Baseline ns/item|!Candidate ns/item|!Candidate eager ns/item|!Candidate vs baseline
|local:direct_call|1.5359|1.5348|1.5417|-0.07%
|local:method_call|1.6099|1.6045|1.6137|-0.34%
|local:polymorphic_expression_call|11.0107|11.0084|11.0538|-0.02%
|local:polymorphic_float_expression_call|10.9263|10.9397|10.9415|+0.12%
|local:temporary_list|1.6333|1.6384|1.6423|+0.31%
|micro:polymorphic_call|84.4367|82.4028|85.4890|-2.41%
|micro:polymorphic_float_call|85.8767|83.0919|86.7559|-3.24%
|micro:polymorphic_expression_call|74.4426|71.1414|75.6048|-4.43%
|micro:polymorphic_float_expression_call|75.3620|72.3653|75.2389|-3.98%
|micro:boxed_int_method_call|111.7742|109.0543|114.6963|-2.43%
|micro:boxed_float_method_call|119.7752|113.2224|118.1203|-5.47%
|micro:indirect_call|8.7727|8.7817|8.6820|+0.10%
|micro:list_mutate|123.1914|122.7280|123.3450|-0.38%
|micro:boxed_int_method_effect|178.1504|181.2750|182.0719|+1.75%
|mixed:object_orders|3377.4192|3385.3045|3391.3858|+0.23%
|mixed:callback_pipeline|95.7336|95.5187|95.9288|-0.22%
|mixed:text_index|1358.3646|1384.6209|1364.8230|+1.93%
|mixed:csv_report|954.5773|963.0629|964.3488|+0.89%
|float:typed_local_int_unary|1.1932|1.1947|1.2006|+0.13%
|probe:effect_short|1745.6081|1742.7075|1740.9654|-0.17%

Retain this implementation under the authorized broader workload tradeoff. All six imported pure-scalar targets improve in every layout against both baseline and the same-candidate eager control. Aggregate gains are 2.41–5.47% against baseline and 3.61–5.90% against the eager control. The local-expression, indirect-call and object-orders concerns do not repeat at their earlier magnitude. Same-Program direct and method controls remain around 1.5–1.6 ns/item with changes below 0.4%.

Retained costs include the effectful integer method control (+1.75%), text-index (+1.93%) and CSV report (+0.89%). The effectful default layout has adversely separated process ranges; its other layouts overlap, and the eager candidate control is also slower than baseline. Text-index medians are adverse in all layouts but process ranges overlap in each. CSV layout directions are mixed, with a wider default-layout range. These observations do not establish their causes or a weighted application speedup. The full screen and all earlier observations remain in the evidence.

All three campaigns independently replay: 1494 processes/4482 measured windows. Fresh baseline and candidate preparations each pass 6072 reference outputs (1518 IR, 4554 AOT); the exact baseline artifacts run with the candidate and its eager control each pass 4554 outputs. Release qualification selects 34 fixtures: 33 successful, one explicit Debug-only skip, 61 QUnit cases/10743 assertions and three silent successes. Debug passes all 22 selected fixtures with 44 cases/4850 assertions, plus separate native suites with 3770 and 5435 checks (potentially overlapping). All 391 Release module aliases are restored and hash-checked.

The extended Release run exposed an existing stale compiler assertion in `AOTNativeMarkerGrowth`: it expected callback entry v3, while both frozen baseline and candidate emit v5. A reduced unchanged padded-caller source proves this in both runtimes and executes correctly. The corrected full 71-padding matrix passes source modes, O0/O3, native/eager stack checks and runtime fallback; failed assertions now avoid dumping the entire large LLVM module. The original failed process and its 293 MB output hash remain archived. A preliminary codegen-selector script also selected lambda symbols by mistake; its failed attempt and corrected review are retained. The same-artifact preparation driver's terminal response was lost across compaction; its exit code is explicitly unknown, while completion and all fifteen waited child receipts were independently verified.

`origin/develop` was fetched again and remains e1abc01cb4, already contained in this branch. This checkpoint does not provide a new Java/develop ratio or prove maximum performance. Shared-host frequency, affinity and thermal state remain uncontrolled; no owned build, test, profiler or heavy scan overlapped timing. Observed ranges are not confidence intervals. Qualification is targeted macOS Release/Debug coverage, not a full-project, sanitizer or other-platform claim.

Next profile the retained runtime and evaluate sharing one live RuntimeConfig reference through call-option setup and the internal native retry chain. The current retry still uses the TLS option-helper overload and separately acquires RuntimeConfig in scalar admission. Existing explicit-context helpers may remove redundant lookups, but lifetime, Program identity, overrides, fallback and frame size need their own source review and optimized measurements. See the [complete evidence](../bench/runtime-comparison/results-cross-program-deferred-context-macos-arm64.json) and [62-check audit](../bench/runtime-comparison/audit-cross-program-deferred-context.md).


## Deferred call context integration with develop (2026-10-30)

After the measured `e6839c5` optimization, `origin/develop` advanced to `9a071ec8d0` and was merged as `a557ceeab1`. The new module option replacement field must travel through the branch's deferred callback and closure contexts. They now save, install and restore it together with the other option fields. Callback save storage is owned by the invocation, allocated only before cold materialization pushes either marker, and reused until reader-safe frame destruction. The generated callback frame remains exactly 208 bytes, aligned to 16 with its original 192-byte storage region.

The first inline-storage attempt failed the generated-frame capacity assertion. The cold-owned storage version passes it. The first merged Release regression run then exposed a separate interaction: this branch seeded native closure slots from runtime-warmed IR, but develop now serializes a fresh lowering. Both O0 and O3 warmed binaries failed with an uninstantiated local while cold binaries passed. Removing the obsolete cached-slot seeding makes native and serialized closure lowering use the same fresh slot order; it also avoids modifying cached IR during compilation. Both the new warmed-slot and earlier debuggable-closure regressions remain selected.

This checkpoint is integration qualification. The preceding 2.41–5.47% cross-Program improvement belongs to the premerge runtime, not a newly measured merged build. No additional Java/develop ratio, application-wide gain or performance ceiling is claimed here.

Release and Debug each pass all 22 selected fixtures without skips: 34 QUnit cases/3842 assertions and five exact-output successes per build. The exact failed minimal source also passes all four cold/warm and O0/O3 combinations. Separate Debug native suites pass 3810 and 5475 checks (these suites may overlap). All 391 Release module aliases are restored with matching hashes. Release uses `-O3 -DNDEBUG` with allocation tracking disabled. The full 62-check audit covers the incoming merge and these integration fixes; failed attempts remain in the evidence.

The next optimization experiment shares one live RuntimeConfig through option setup and the pure/member native retry after complete Program entry. Freeze the installed merged baseline before applying that separate patch, then compare optimized runtimes with identical artifacts and broad controls.

See the [integration evidence](../bench/runtime-comparison/results-deferred-context-merge-macos-arm64.json) and [62-check audit](../bench/runtime-comparison/audit-deferred-context-merge.md).


## Shared RuntimeConfig after Program entry (2026-10-30)

Cross-Program AOT pure/member retries now share one live RuntimeConfig with call-option setup after complete Program entry. This removes duplicate option-helper TLS lookups while retaining the existing RuntimeConfig acquisition, all live admission checks, accounting, fallback and context restoration. The invocation reference never escapes its thread/call; no replaceable PIC crosses initialization. The generated AOT contract and all fifteen fresh artifact pairs are unchanged.

Against optimized `266db2c39f`, three campaigns independently replay 1404 processes/4212 windows. Confirmation improves six pure scalar workloads by 2.25–4.11% (17 of 18 target/layout medians improve; the remaining +0.19% overlaps). Same-Program direct/method loops stay near 1.53/1.59 ns/item. Retained confirmation costs include list mutation +0.72%, callback pipeline +0.86%, text index +0.62%, and the short effectful-object probe +0.55%. The generic/member frames grow by 16 bytes; pure and existing typed frames stay 480/304 bytes. Full controls, ranges and the earlier indirect-call slowdown that did not repeat remain in the linked report.

Release and Debug each pass nine focused fixtures / 14 cases / 2751 assertions, plus separate 3810- and 5475-check native suites. Baseline/candidate reference outputs, identical-artifact execution, LLVM verification, loaded-image UUIDs and restored Release module aliases are verified. All 62 audit checks are resolved. No new Java/develop comparison, weighted application gain or global performance ceiling is claimed. The next experiment should inspect forwarding an already-live caller context into ordinary dispatch and the generic-frame storage cost, retaining complete Program-entry semantics.

See [full results and controls](../bench/runtime-comparison/RESULTS.md), [raw evidence](../bench/runtime-comparison/results-cross-program-shared-context-macos-arm64.json), and [audit](../bench/runtime-comparison/audit-cross-program-shared-context.md).


## Floating-point forwarding proof (2026-10-30)

AOT now admits exact homogeneous `float(float)` self-forwarding bodies alongside the
existing integer proof. The entire body must be one block containing a temporary mark,
non-reference loads of `self` and the sole parameter, one `DotEvalMethodDirect` call,
and return of that call result. Static methods, body locals, opaque local access,
signature substitution, and outlined bodies are excluded. Multiple-call bodies remain
ordinary. No general receiver-aware floating companion is introduced.

The normal body publishes an immutable image-owned slot/location descriptor through
`qore_rt_bind_receiver_float_forward_v2`. It uses the existing
`QoreAOTReceiverIntForwardEntryV3` layout and `receiver_int_forward_entry` atomic field,
with a required null integer companion. The binder checks exact normal-body identity,
slot bounds and location before release publication. Integer and floating inner entries
retain separate C++ function types; the shared descriptor never supplies an integer
function to a floating call. The context layout does not grow. Experimental float-v1
artifacts used a different layout and were never installed or committed; the v2 symbol
distinguishes the final contract.

Every invocation still requires strict outer scalar admission and the ready, exact-body,
pure inner float-v2/v3 entry under matching receiver/class/type, Program, parse options
and dispatch epoch. Debugger attachment and explicit type instantiation retain ordinary
execution. A miss leaves ownership and the sink unchanged. The typed route borrows the
caller's live `RuntimeConfig`; the boxed route first requires the matching immediate
representation and retains cleanup/boxing. Unsupported or heap-backed inputs can still
use the ordinary path. Both logical compact frames and the explicit inner source call
site remain published. The existing deferred contexts materialize in caller order on
the leaf guard's failure, and RAII waits for diagnostic readers before releasing frames.

Floating member admission precedes the new forwarding probe; integer order is unchanged.
`QORE_DISABLE_AOT_RECEIVER_FLOAT_FORWARD=1` disables proof emission and runtime use.
Existing receiver, native, cache, context and forwarding controls remain effective.
The diagnostic flag is read only when the descriptor is present.

The accepted optimized comparison against `df0e7bce35` reduces the three-receiver floating
forwarding kernel from 184.3060 to 35.0420 ns/item, while retaining reported control costs
including two-call floating +2.52%, floating fields +1.03% and closures +1.83%. The context
is 776 bytes on this ARM64 host; existing dispatcher frames are unchanged and executable
code grows 5080 bytes. Release and Debug each pass eight fixtures / 15 cases / 4553 assertions,
with native suites and 391 restored Release aliases separately verified. See the
[full measurement and audit](../bench/runtime-comparison/results-scalar-forwarding-macos-arm64.json).
These are whole-loop costs; no new Java/develop comparison or performance ceiling is claimed.


### Rejected dot-call metadata cache (2026-10-30)

Moving parsed explicit dot-call type metadata into the existing target field removed per-call AST decoding, but the [optimized measurements](../bench/runtime-comparison/results-rejected-dot-metadata-macos-arm64.json) did not justify repeated field and side-effecting call costs. All four production files are restored; the existing getter and context construction remain. Next: investigate a dedicated reader-safe pair protocol for forwarding frames, preserving both logical frames and deferred contexts through materialization, exceptions and concurrent reads. The subsequent paired-frame experiment below implements and qualifies this protocol.


### Paired scalar forwarding frames (2026-10-30)

Admitted same-program integer and floating self-forwarding calls now use RuntimeConfigNativeMethodFramePairHelper. Its two unpublished base helpers save the original and intermediate thread views; fully initialized diagnostic frames are linked inner-to-outer-to-previous before one release publication. Fully deferred destruction removes exactly that pair with the existing reader completion handshake, retaining both frame objects and vtables until readers finish. Partial/full materialization uses the existing inner-then-outer restoration. The ordinary single-frame LIFO assertion and all live admission checks remain unchanged. RuntimeConfig is borrowed; no public/generated ABI, existing helper layout, ownership or allocation change is needed.

The final optimized [measurements](../bench/runtime-comparison/results-paired-frames-macos-arm64.json) find floating/integer forwarding reductions of 3.58%/2.90% with fresh callers and 3.60%/3.34% with identical standalone callers. Integer field, indirect, list-building, floating virtual and effectful controls include measured costs documented in the full results; no weighted application gain is inferred. Native frames remain 544/528 bytes and executable sections grow 992 bytes. Release and production-identical full Debug pass 8 fixtures/4553 assertions; after a Debug-only audit fix native runners pass 4003/5668 assertions. Concurrent readers, partial materialization, guard exceptions and C++ unwinding preserve diagnostics and context restoration.

The next checkpoint refreshes optimized branch/develop/Java comparisons, then profiles remaining dynamic call admission and compiler specialization opportunities. No Java parity or global performance ceiling has been established.


### Current cumulative optimized checkpoint (2026-10-30)

Branch 9cd852e16e and develop a3ee195ea2 are compared using matching optimized builds and fresh C2/JMH runs. The [complete 32-workload table](../bench/runtime-comparison/RESULTS.md#optimized-checkpoint-after-paired-forwarding-frames-2026-10-30) includes 25 Java comparison rows and all 651 processes /1953 windows. These cumulative results retain the earlier workload set; incremental forwarding measurements are documented separately. Whole-kernel units, non-inline controls, actual Program boundaries and observed host variability limit interpretation. No universal Java parity or global performance ceiling is established.

Next qualify a matched Java forwarding benchmark, distinguishing unrestricted C2 from a leaf-only no-inline control. Then test whether immutable explicit type-argument checks can move from each forwarding call to descriptor binding. Establish initialization and lifetime proofs first; retain all live program, object, options, debugger, epoch, ownership and stack checks, and measure broader controls before retention.
