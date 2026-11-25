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
- Before each deferred IR handler, the parent releases its duplicate caches of
  runtime locals. A handler's assignment must release the value it replaces
  immediately, including when a preceding handler wrote that value back into the
  parent cache. IR-only locals are unreachable by handlers and keep their owners.
- Dirty parent slots are written back after IR handler execution.
- Native/JIT/AOT handlers access the runtime locals after the parent publishes
  them and releases its own cached references; the parent reloads them afterward.

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
Since the slot ownership follows the final facts, the local loads that the
scalar replacement forwards with a native representation keep it when
`qore_ir_refine_local_value_facts()` runs after the replacement: the replacement
forwards only a value whose facts are native and never NOTHING, which the
lowering gives a local load only where the read itself is assigned (the parse
records the assignment state of each read; a read that may be unassigned, or of
a local that delete or remove can clear, has its type or NOTHING and stays
boxed), and the refinement's dataflow is less precise (a
handler entry starts with nothing known), so it found such a load after a try
statement only possibly assigned and gave it boxed facts; the interpreter then
kept the value as a temporary of the statement that loaded it, and the reads of
the members in later statements gave NOTHING (qore#7356).  The removal of the
dead loads of a local initialized with a constant keeps a load whose value
replaces a member's reads: after a try statement, the load is not forwarded from
the store, and the member's value is the load's.
`IRHandlerJoinScalarReplacement.qtest` checks members built after try, on_exit,
on_error and on_success statements in every execution mode and compiled code.
Native code keeps such a value as a native double or integer: a local whose
storage there is boxed (a local that may be unassigned somewhere, a parameter,
a local of the runtime stack) gives a read that is borrowed from the local, or
a reference of its own released with the temporaries of the statement, which a
later write to the local or the end of its statement releases.  The lowering
(`QoreIRToLLVM::findNativeLocalReads()`) therefore unboxes every read of a local
with native facts where it is read (qore#7372); in a debug build,
`QoreIRToLLVM::assertNativeLocalReadsUnboxed()` asserts it after the function's
blocks are lowered.  The facts must therefore be exact: the refinement takes a
local whose type accepts NOTHING as assigned only after a store of a value that
cannot be NULL, as such a local stores NULL as NOTHING.
`IRForwardedLocalRead.qtest` checks
members whose local is written after the aggregate is built (compound
assignments, increments, assignments, writes through a reference and by a
closure, writes in loops and branches, member and element stores) or goes out of
scope, in every execution mode and compiled code.

### Constant locals and fused instructions

Fixed-aggregate scalar replacement resolves a selector through a local that has
one store dominating its loads (`constant_local_assignments`), and removes the
store and the loads of such a local once the replacement leaves no read of it.
The fused integer instructions name their locals in their own fields instead of
taking the value of a `LoadLocal` as an operand: `BranchIfLtLocalInt` reads both
of its locals, `AddAssignLocalInt` reads its source and reads and writes its
target, and `IncrementLocalInt` reads and writes its local.  A local that one of
them accesses is therefore never such a constant local: its writes make it
variable, and its reads keep its store, which would otherwise be removed as
unused (the bound of a loop or the source of an addition then read a local that
was never assigned).  `IRScalarFusedLocals.qtest` checks loop bounds and
counters, additions and increments next to replaced lists and hashes in every
execution mode and compiled code.

### Integer pairs read with a bounded index

A local `list<int>` of two members whose reads have an index that is not a
constant but can only be 0 or 1 (a native integer with a known range of 0 to 1,
or the result of an AND with the constant 1, such as `pair[x & 1]`) is replaced
like a list read with constant indexes: the list is not built, and each such read
becomes `first ^ ((first ^ second) & -index)`, which selects a member without a
branch; `-index` is 0 or -1, so no operation can overflow, whatever the members'
values.  The members are native integers, which the interpreter's slots own
whether they are inline or boxed (see above), so the selection can use them in
later statements and loops.  Any other index (an AND with another mask, or an
index that can be negative or larger), other element types and every condition
of the replacement of a list read (one store of the list, no mutation, alias,
capture, reference or handler on the read) keep the list.
`QORE_DISABLE_IR_BOUNDED_PAIR_SCALAR_REPLACEMENT` disables it.
`IRBoundedPairScalarReplacement.qtest` checks members in and out of the inline
range, reads in one or more blocks and in a loop, and the lists that must stay,
in every execution mode and compiled code, with the optimizer's statistics and the
compiled code's calls to build lists.

### Hash reads with a key chosen from literal keys

A read of a local hash whose key is not a constant but a phi of literal keys of
the hash (`h{c ? "a" : "b"}`, or through string locals with one store that
dominates their loads, as in `string key = c ? "a" : "b"; h{key}`) is replaced by
a phi of the values of the selected members on the same edges, when:

- the members are native scalars (int, float or bool) of one representation and
  type: they are owned by the interpreter's slots (see above), so the phi can
  take them after the statement that built the hash;
- the store of the hash dominates the phi, which dominates the read, so on every
  path to the read the last join of the phi follows the last store of the hash;
- the phi has two to four incoming edges, from distinct predecessors that it does
  not dominate, each with a literal key that the hash has and whose member's
  value dominates the end of the predecessor;
- the function has no handlers: the graph that the replacement uses has no
  exception edges, so its joins are not every path to the read.

The hash is then removed as for constant keys.  The phi of keys, its literals and
the string local that holds it (with its store, loads and releases) are removed
too when nothing else uses them (`finite_hash_selector_removable()`); a key that
is used otherwise (measured, copied, read through another local) stays.  A key
computed before the hash, a key that the hash does not have, nested conditionals,
hashdecls and hashes that are changed, aliased, captured or passed by reference
keep the hash.  `QORE_DISABLE_IR_FINITE_HASH_KEY_SCALAR_REPLACEMENT` disables the
replacement, `QORE_DISABLE_IR_FINITE_HASH_KEY_SELECTOR_ELISION` only the removal
of the keys.  `IRFiniteHashKeySelectorElision.qtest` checks integer members in
and out of the inline range, float and bool members, `hash<auto>`, loops, keys
that stay and hashes that must stay, in every execution mode and compiled code,
with the optimizer's statistics and the compiled code's calls to build hashes and
keys.
