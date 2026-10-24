# Qore IR Design

## Status

The Qore IR is the shared execution representation for the IR interpreter,
LLVM JIT, and AOT compiler. It is implemented in:

- `include/qore/intern/QoreIR.h`
- `lib/QoreIRLowering.cpp`
- `lib/QoreIRInterpreter.cpp`
- `lib/QoreIRToLLVM.cpp`
- `lib/QoreIRVerifier.cpp`

The current branch uses IR for `%modern` code in `ir`, `jit`, `tiered`, and
AOT paths. Non-modern code remains on the AST interpreter.

## Semantic Contract

IR execution must preserve AST-visible behavior:

- Same values, exceptions, side effects, and reference-count behavior.
- Same local, closure, global, thread-local, and object member semantics.
- Same `on_block_exit`, `try` / `catch`, `context`, `foreach`, lvalue, and
  destructor ordering.
- Different diagnostics are acceptable only when behavior remains equivalent.

The implementation may choose different internal execution paths:

- IR interpreter uses `ExceptionSink` directly.
- LLVM JIT and AOT paths use runtime helpers and C++ exception unwinding where
  needed to preserve Qore exception cleanup semantics.
- Tiered mode promotes hot functions AST -> IR -> JIT, but debug attach can
  force dispatch back to AST.

## Value Model

IR values use the same 64-bit NaN-boxed `QoreValue` representation as the
runtime:

- Inline int, bool, null, nothing, short-string, and double values are carried
  directly in the 64-bit word.
- Node values carry owned or borrowed `AbstractQoreNode*` references depending
  on the instruction contract.
- Runtime helpers must document whether returned node values are owned.
- Cleanup instructions or cleanup lists must release every owned node exactly
  once on normal, return, and exception paths.

The IR must never invent a second value representation. This is required so
AST, IR, JIT, and AOT frames can interoperate during fallback, handler
execution, and native helper calls.

## Function and Block Model

An IR function contains:

- A signature and local-variable metadata.
- Basic blocks with explicit terminators.
- Value slots for expression results.
- Local slot IDs for runtime/JIT/AOT local-cache identity.
- Exception targets for instructions that can throw.

Every block must end in an explicit terminator. The verifier checks operand
validity, terminators, block targets, local slot references, cleanup metadata,
and instruction-specific invariants.

## Deserialized Instruction Metadata

An instruction field that holds a process address — a `QoreVarInfo*`, a
`QoreClass*`, a variant pointer — cannot be serialized into an AOT binary. The
binary records the name or path instead, and the address exists only once the
target is resolved in the running Program. IR read back from a binary therefore
arrives with those fields empty, and the resolution can still fail: a class that
is not loaded yet leaves a deferred reference that has to be retried at run time.

Such a field is a cache, not the source of truth. The instruction's serialized
expression is authoritative, and every consumer must be able to work from it:

- Resolve an empty field from the expression before use; never dereference it.
- Serve a reference that stayed deferred by name, through the by-path runtime
  helper.
- Treat metadata that is neither resolvable nor deferred as a lowering failure,
  so the JIT keeps the function on the IR tier instead of emitting a call that
  cannot work.

This applies to the LLVM lowering as much as to the interpreter. Deserialized IR
reaches the JIT whenever a function or closure loaded from a binary is promoted
to the JIT tier, and the JIT bakes these addresses into the generated code as
constants: an unresolved field becomes a null pointer in native code that faults
the first time the instruction is reached, which can be long after the program
started and only on the first execution that takes that branch.

Resolution happens per execution, so a consumer must keep the resolved address in
a local. Every thread executing a function shares one instruction, and writing
the resolution back races with the other threads' reads of the same field.

## Locals and Parent Slot Identity

Locals are mutable runtime locations, not SSA variables. IR uses explicit
load/store instructions and a per-function `local_var_slots` map.

Slot identity is significant:

- Deferred handler IR inherits parent locals by IR slot ID.
- AOT local tables preserve IR slot IDs so handler inheritance remains exact.
- Native/JIT/AOT parents publish exact handler slot caches before executing
  deferred handlers.
- Dirty parent slots are written back after handler execution.

Name-based lookup is not sufficient for handler execution because nested
functions and native frames can have colliding local names.

## Exceptions and Cleanup

Any instruction that can raise must either:

- Execute through the interpreter with `ExceptionSink` checks, or
- Lower to LLVM `invoke` / landing-pad cleanup, or
- Call a throwing runtime helper whose exception path reaches the active
  cleanup block.

Cleanup must cover:

- Owned value slots.
- Instantiated locals.
- Iterators.
- Closure re-instantiation state.
- `on_block_exit` handlers.
- AOT/JIT runtime temporaries.

SSA-direct cleanup is intentionally conservative. Where dominance cannot be
proven safely during lowering, values are promoted to cleanup allocas. See
`design/aot-eh-cleanup-dominance.md`.

## Lvalues and COW

Lvalue operations must not inflate container refcounts before copy-on-write
checks. Mutation paths that need the natural container refcount must use
borrowed loads and invalidate local caches before mutation, and the temps of a
value operand are released before the mutation (`DiscardTempsKeep`).

See `design/lvalue-loads-in-ir.md` for the precise invariant and current
handler rules.

## Lowering Coverage

The current implementation supports the language constructs needed by the
standard library module AOT build, including:

- Function, method, static method, closure, and call-reference calls.
- Typed and dynamic arithmetic, comparisons, string, regex, hash, list, date,
  object, and pseudo-method operations.
- `try` / `catch`, `throw`, `rethrow`, `on_block_exit`, `foreach`, `context`,
  loop control, and returns.
- Lvalue assignment, compound assignment, path mutation, slices, delete/remove,
  and pattern-based path operations.

Unsupported parse-tree forms must fail explicitly during lowering or remain on
AST because the program is not eligible for IR execution. Silent semantic
fallbacks in `%modern` IR/JIT/AOT paths are bugs.

## Debugging and Instrumentation

IR interpreter debug hooks track source-line progress and support debugger
single-step behavior. Native JIT debug instrumentation is intentionally
limited: when debugging requires AST semantics, dispatch can force AST
execution for affected variants.

For deadlock or livelock investigations, use the existing SIGUSR1 stack-trace
instrumentation on Qore tests and services where enabled.

## Maintenance Rules

- Add verifier coverage with every new opcode or operand shape.
- Keep IR slot IDs stable across interpreter, JIT, and AOT paths.
- Resolve pointer-valued instruction metadata from the serialized expression
  before use, and keep the result in a local.
- Do not add runtime fallbacks that hide missing lowering in `%modern` code.
- Every owned value path needs a normal-exit and exception-exit cleanup.
- Any lvalue mutation must preserve the COW invariant.


### Scalar common expressions and temporary ownership

Scalar common-expression elimination distinguishes execution dominance from the
lifetime of the interpreter's `QoreValue` slots. A native IR integer outside the
inline range and a double whose bits collide with a value tag own boxed nodes.
`DiscardTemps` and `DiscardTempsKeep` can release those nodes even when all
explicit consumers are non-consuming. Such expressions and forwarded loads
remain available only within their current basic block, up to the next cleanup
boundary. Separate scoped maps make clearing them linear in the removed entries.
Only proven inline integers, booleans and non-colliding float constants are
published for cross-block reuse. Non-consuming-use checks remain necessary.
`IRScalarCSETemporaryLifetime.qtest` compares signed integer boundaries, computed
large products, boxed finite doubles, empty loops and ordinary small values in
AST, IR, tiered, JIT and AOT at optimization levels zero and three.

Values with a native representation (`NativeInt` and `NativeFloat` facts) are
owned by their slot in the interpreter, not by the temporary scope of the
statement that computes them: fixed-aggregate scalar replacement forwards them
across statements, around loops and into phis, as native code keeps them in
registers. `buildInterpreterAnalysis()` marks these slots
(`QoreIRFunction::interpreter_slot_owned_values`), and `IRCleanupSlots` keeps
their cleanup entries in a separate list that `DiscardTemps`, `DiscardTempsKeep`,
`UnwindTemps` and exception unwinding do not drain; a boxed node is released when
its slot is overwritten or when the function's values are cleaned up. Such
values have no destructors, so the later release is not observable. The result
of a compound or increment operation does not inherit the integer range of the
value it replaces. `IRScalarReplacementTemporaryLifetime.qtest` checks integers
outside the inline range and boxed doubles in fixed hashes and lists, including
loop-carried and branch values, in every execution mode and compiled code.

### Exact numeric snapshots after aggregate scalarization

`SnapshotInt` (409) and `SnapshotFloat` (410) preserve an assigned exact numeric
SSA value after its owning aggregate is removed. Native arithmetic facts alone
do not prove inline `QoreValue` storage. The fixed-aggregate scalarizer snapshots
possibly boxed operands, mutation results and merge values before statement
cleanup can invalidate them; it also invalidates stale integer bounds on new
mutation/PHI results. Proven inline values need no snapshot. This does not permit
extending object lifetimes or hoisting arbitrary conversions.

The interpreter retains snapshots in a separate frame-owned, unique-slot ledger.
Overwriting a snapshot releases its previous value; ordinary temporary cleanup
does not release it. Normal, exceptional and cancelled frame exits drain the
ledger, and returning a snapshot transfers its owned value. Only exact integers
or floats are accepted, not enum values. The ledger is initialized lazily and
has at most one registration per SSA slot, including repeated loop execution.

LLVM materializes the native value at the snapshot definition, never by delaying
unboxing until a later use. Snapshots of native PHIs, including snapshot alias
chains, are predeclared native aliases so basic-block storage order need not
match dominance order. Aggregate-return summaries propagate snapshots only with
matching assigned, never-Nothing native facts. The existing Base instruction
serialization group carries these unary instructions; the appended opcode
maximum rejects older readers. No runtime-helper ABI or RuntimeConfig layout
changes. AOT artifacts require a matching runtime and recompilation.

`IRScalarSnapshot.qtest` covers wide integers, tag-colliding doubles, mutation,
PHIs, loop overwrites, exceptional exits and serialized closures in AST, IR,
JIT, tiered and AOT O0/O3 modes. Native tests verify exact reference counts on
return, non-return and error exits, including pooled frame reuse.

### Constant-local proofs include fused accesses

Fixed-aggregate scalarization must not infer a constant local from a partial
LoadLocal/StoreLocal inventory. `AddAssignLocalInt` reads its source and reads and
writes its target; `IncrementLocalInt` reads and writes its local;
`BranchIfLtLocalInt` reads both comparison locals. These references live in
instruction metadata rather than SSA operands. Exclude all such locals from the
constant-local shortcut, including dead-selector/initializer removal. A fused
read keeps an initializer live even if there is no explicit LoadLocal; a fused
write invalidates a previously constant selector. The existing native-local
definite-assignment proof can optimize the preserved initialization normally.
`IRScalarFusedLocals.qtest` covers both correctness and native counter selection.

### Canonical one-argument trim in AOT CallDirect

One non-reference argument to the canonical Qore builtin trim value overload can
use `qore_rt_string_trim_aot_v1`. Admission proves a BuiltinFunctionValueVariant in
namespace Qore named trim, exact string return and string/*string parameters,
PURE/HOST_PORTABLE/NAMED_ARGS flags, default domain, no varargs/type substitution
or explicit type arguments, and two NOTHING default slots. This does not infer
assignment from a type hint: the helper guards a non-enum runtime string and the
same active Program/caller context; misses use ordinary consuming dispatch.

The helper omits argument-list construction and default/type binding only. Its
frame reproduces the canonical Program parse-option/statement-location transition
when runtime options differ, with restoration on error and success. It retains
runtime flags, return type, builtin diagnostics, class/object context and stack
checks. The result is an owned copy trimmed by the canonical Unicode-aware API;
caller argument cleanup, exception/result ownership and runtime-local reloads
are unchanged. No IR opcode, RuntimeConfig layout or serialized call-target layout
changes; the versioned internal helper uses the ordinary runtime symbol registry.

`QORE_DISABLE_AOT_STRING_TRIM` disables lowering at compile time and exercises
ordinary consuming dispatch at execution. Prelink-disabled execution also falls
back. Other overloads and Invoke lowering retain normal dispatch. The old JIT
name-only trim shortcut is removed because it did not preserve reference overload
semantics; the legacy byte-only helper is not used by this specialization.
`AOTStringTrim.qtest` covers overloads, encodings, exceptions, context restoration
and AOT compilation/control variants.
