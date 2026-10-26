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
