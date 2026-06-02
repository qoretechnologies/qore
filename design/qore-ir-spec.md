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
borrowed loads and invalidate local caches before mutation.

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
- Do not add runtime fallbacks that hide missing lowering in `%modern` code.
- Every owned value path needs a normal-exit and exception-exit cleanup.
- Any lvalue mutation must preserve the COW invariant.
