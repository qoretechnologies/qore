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
After the main LLVM optimization pipeline, native scalar callback callers
receive inline int48, bool and NOTHING cleanup guards. Other functions are
left unchanged: a broader pass regressed direct-call and dispatch-control
loops. Only local instruction/CFG simplification follows, avoiding the
substantial code growth observed when the full inliner
and loop unroller processed these guards. Heap-backed and opaque values keep
the original release helper, ordering and exception sink. This targets AOT,
including O0: JIT/IR do not emit the typed native callback helpers that select
the pass. There is no runtime ABI change; it is not a general
proof that non-pointer tags need no cleanup. The comparison switch is
`QORE_DISABLE_LLVM_SCALAR_DECREF_GUARD`, and code-size growth remains a tradeoff.

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
an independent compilation control. All caller/frame/callee stack checks,
live guards, empty-frame markers and observer synchronization remain intact.

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
statement/location, parse-option and saved-SP fields untouched. Empty variable
frame markers and the independent diagnostic frame are still pushed/published
on every call, with the existing reader handshake and caller location.

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
and metadata extraction are outlined as the miss path. None of the guards,
frame markers, policy restoration or reader handshakes is removed. This is a
runtime inlining boundary change; inlining itself does not alter the callee
ABI or introduce a new compact-frame representation.

Prepared v3 calls now use a smaller diagnostic frame that borrows the prepared
descriptor for its name, statement block and Program instead of copying those
three pointers. The caller location remains frame-local. The exact scalar leaf
cannot reenter generic execution or replace the descriptor while it is
published; the existing pop handshake waits for readers before any subsequent
replacement or invocation cleanup. The state-saving helper remains the last
member and is unchanged. V2 keeps its original frame; no cache layout, helper
ABI, guard, marker or restoration behavior changes. A dedicated concurrent
reader fixture checks cache hits, alternating targets and repeated invocation
cleanup under native and generic execution.

The admitted scalar leaf cannot instantiate TLS locals or reenter generic
execution. Its local-variable marker therefore uses a specialized push/pop pair:
it retains the ordinary marker and frame count for inspection, but does not
append a variable-cleanup record to the local marker vector. Both local and
closure markers are popped directly as empty frames. The native closure-marker
push writes the same null/zero/non-owner entry inline when the current block
has space. Only full-block growth/reuse calls an outlined helper, which uses
the original insertion path; frame counts and publication order are unchanged.
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
