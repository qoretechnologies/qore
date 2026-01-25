# Qore IR Spec Outline

## 1. Purpose and Non-Goals
- Purpose: Define a Qore-specific IR that preserves interpreter semantics and enables JIT/AOT.
- Non-goals: Performance tuning, full LLVM lowering details, or full language coverage in v1.

## 2. Terminology
- AST interpreter: existing `evalImpl()`-based execution.
- IR interpreter: new execution engine for IR.
- JIT: LLVM-backed native execution from IR.

## 3. Semantics Contract (AST / IR / JIT)
- Required equivalence: observable behavior, exceptions, refcounts, and side effects.
- Allowed differences: performance, internal temporaries, diagnostic detail.
- Error model: AST/IR interpreter continue to use `ExceptionSink`; JIT may use native C++ exceptions with stack unwinding.

## 4. Value Model and Type Tags
- QoreValue layout: NaN-boxed 64-bit word with tag in high 16 bits and payload in low 48 bits.
- Double encoding:
  - `DOUBLE_ENCODE_OFFSET = 0x0001000000000000` (2^48) and `DOUBLE_BOUNDARY = 0xFFF9000000000000`.
  - Encoded doubles are `< DOUBLE_BOUNDARY` and `bits != 0`, excluding short string tag range.
- Tag values (high 16 bits, `TAG_MASK = 0xFFFF000000000000`):
  - `TAG_INT48 = 0xFFF9000000000000`
  - `TAG_POINTER = 0xFFFA000000000000`
  - `TAG_SPECIAL = 0xFFFB000000000000`
  - `TAG_SHORTSTR_BASE = 0xFFFC000000000000` (short string len encoded in bits 48-50)
- Payload rules (`PAYLOAD_MASK = 0x0000FFFFFFFFFFFF`):
  - 48-bit signed integers in payload; `INT48_MIN = -(1LL << 47)`, `INT48_MAX = (1LL << 47) - 1`.
  - Pointer payload is an `AbstractQoreNode*` stored in low 48 bits.
  - Short string: `bits >> 52 == 0xFFC`, max 6 bytes.
- Special values:
  - `VAL_NOTHING = 0`
  - `VAL_NULL = TAG_SPECIAL | 1`
  - `VAL_FALSE = TAG_SPECIAL | 2`
  - `VAL_TRUE = TAG_SPECIAL | 3`
- Type introspection: IR may use tag/bit tests matching `QoreValue` helpers.
- Ownership model: value lifetimes and refcount responsibilities; define owned vs borrowed for each tag class.
- Cross-mode rule: no conversion or tagging differences between AST/IR/JIT.

## 5. IR Structure
- Functions: signature, argument binding, return conventions.
- Basic blocks: single terminator per block, no implicit fallthrough.
- SSA policy:
  - SSA for expression temporaries and instruction results.
  - Locals and args are modeled with `load.local` / `store.local` / `load.arg`.
- Phi rules:
  - Phi nodes only at block starts.
  - Each predecessor provides one value.
  - Verifier enforces dominance and complete predecessor coverage.
- Exception edges:
  - `invoke` creates an explicit unwind edge.
  - Cleanup blocks are explicit and have terminators.

## 6. Call ABI (IR Interpreter + JIT)
- Execution entry contract (logical IR ABI):
  - Inputs: argument array, closure/env array, local slots, and `ExceptionSink*`.
  - Output: `QoreValue` return value (or NOTHING for `return.nothing`).
- Argument access:
  - `load.arg` is index-based and reads from the incoming argument array.
  - For variadic calls, the callee sees the already-materialized argument list per AST semantics.
- Locals and closures:
  - `load.local` / `store.local` use a LocalVar-backed slot; IR interpreter/JIT maps `LocalVar*` to a slot index.
  - `load.closure` reads from the closure/env slots captured at call entry.
  - No implicit aliasing: a LocalVar maps to exactly one slot for the duration of the call.
- Exception propagation:
  - Interpreter path sets `ExceptionSink` and returns NOTHING.
  - JIT path throws the dedicated Qore exception type and relies on invoke/landingpad edges.
- Suggested C ABI entry stub (for JIT glue):
  - `extern "C" QoreValue qore_ir_entry(QoreValue* args, size_t nargs, QoreValue* closure, size_t nclosure,`
    `QoreValue* locals, size_t nlocals, ExceptionSink* xsink);`
  - Ownership: inputs are borrowed; return value is owned by the caller per normal QoreValue rules.

### ABI Ownership Table (Phase 0)

| Component | Representation | Ownership | Notes |
| --- | --- | --- | --- |
| args | `QoreValue* args`, `size_t nargs` | borrowed | Elements are read-only to the callee; callee must incref if it needs to retain beyond the call. |
| closure/env | `QoreValue* closure`, `size_t nclosure` | borrowed | Capture slots behave like args; no implicit refcount changes on load. |
| locals | `QoreValue* locals`, `size_t nlocals` | owned by frame | Frame owns local storage; stores follow Qore assignment semantics (incref as needed). |
| ExceptionSink | `ExceptionSink* xsink` | borrowed | JIT must throw immediately when `xsink` is set; IR interpreter propagates and returns NOTHING. |
| return | `QoreValue` | owned by caller | Caller must decref if refcounted; NOTHING is a valid return value. |

## 7. Instruction Set (Phase 1 Minimal)
- Constants: int/float/bool/nothing/string.
- Arithmetic: add/sub/mul/div/mod (typed + any).
- Comparisons: eq/ne/lt/le/gt/ge (typed + any).
- Control flow: br, br.if, return, unreachable.
- Locals: load.local, store.local, load.arg, load.closure.
- Calls: call, call.indirect, call.method, call.static.
- Exceptions: invoke, landingpad, catch.exception, rethrow, throw.
- Refcount ops: incref, decref, decref.nothrow.

## 8. Exception and Unwind Semantics
- JIT path uses real C++ exceptions with stack unwinding (modeled after qore-llvm).
- AST/IR interpreter keep `ExceptionSink` for backwards compatibility.
- JIT exception representation: a dedicated C++ exception type carrying the active Qore exception payload.
- Boundary rule: JIT code never allows a raw C++ exception to escape into non-JIT C ABI; all boundaries must catch and translate to `ExceptionSink` when required.
- `invoke` behavior: normal edge + unwind edge; any op that can throw must lower to `invoke` or a wrapper that enforces equivalent semantics.
- Cleanup on unwind: lvalue unlock + refdec temporaries in reverse order of acquisition/creation.
- Cleanup scope model: each scope with temporaries registers a cleanup list that is consumed on normal exit and unwind.
- Rethrow semantics: preserve original error identity and stack context.
- Required mapping to runtime helpers (NRT-style): helpers signal errors via `ExceptionSink` and a standardized throw bridge for the JIT path.
- Cross-mode rule: if a runtime helper sets `ExceptionSink`, JIT code must immediately throw, and IR interpreter must propagate `ExceptionSink` without further side effects.

### Example: invoke with cleanup and rethrow

```
try {
    tmp = call @f()
    use(tmp)
} catch (e) {
    throw e
}
```

IR sketch:

```
entry:
    %tmp = invoke @f() to normal unwind on_except
normal:
    use %tmp
    decref %tmp
    br done
on_except:
    landingpad
    ; cleanup list: decref temporaries in reverse order
    decref.nothrow %tmp
    %ex = catch.exception
    rethrow
done:
    return.nothing
```

## 9. Refcount Semantics
- Ownership rules for each instruction class.
- Each IR value is either owned or borrowed; lowering must declare ownership for all values it produces.
- Required ordering relative to exceptions: any incref/decref sequence must be exception-safe and deterministic.
- Temporaries created in a scope must be decref'd on normal exit and in unwind cleanup.
- `decref.nothrow` is only allowed in cleanup paths or other contexts where exceptions must not be raised.
- Exception-safety requirement: no leaks or premature frees on normal or unwind paths.
- Refcounted nodes returned from runtime helpers must define transfer semantics (owned vs borrowed) in the ABI.

### Ownership Table (Phase 0)

| Instruction class | Result ownership | Operand ownership | Notes |
| --- | --- | --- | --- |
| `const.int/float/bool/nothing` | immediate (no refcount) | n/a | No refcount operations required. |
| `const.string/const.date` | owned | n/a | Returns a refcounted node with +1 ref for the result. |
| `load.*` / `load.lvalue` | owned | borrowed | Loads return an owned value; if refcounted, result has +1 ref. |
| `store.*` / `store.lvalue` | no result | borrowed | Store follows Qore assignment semantics (incref as needed); does not consume operand. |
| `unary/binary/ternary/quaternary` | owned | borrowed | Typed ops yield immediates; `.any` ops use helpers that return owned values. |
| `call/call.*` / `invoke` | owned | borrowed | Return value is owned; operands are borrowed unless explicitly `incref`'d by lowering. |
| `phi` | inherited | borrowed | Result ownership matches incoming values; no implicit refcount ops. |
| `incref/decref/decref.nothrow` | no result | owned by context | Explicit refcount ops; `decref.nothrow` only in cleanup/unwind paths. |

### Example: temporary lifetime with unwind cleanup

```
%a = call @make_object()        ; owned
%b = call @maybe_throw(%a)      ; may throw
store.local %slot, %b
decref %a
```

IR cleanup rules:
- `%a` is added to the cleanup list on creation.
- If `maybe_throw` unwinds, cleanup runs `decref.nothrow %a`.
- If normal path continues, explicit `decref %a` consumes the cleanup entry.

## 10. Runtime ABI (NRT-Style)
- C ABI wrapper signatures for runtime helpers; no C++ name mangling.
- QoreValue passing/returning conventions:
  - POD layout, passed by value where possible.
  - Ownership semantics per function documented (owned vs borrowed).
- Exception bridge:
  - Helpers receive `ExceptionSink* xsink`.
  - If `xsink` is set, JIT throws a dedicated C++ exception immediately.
  - Interpreter path propagates `xsink` without further side effects.
- Required helpers (Phase 1):
  - Arithmetic and comparison for `.any` ops.
  - Conversions: `to.int`, `to.float`, `to.string`, `to.bool`.
  - String concat and size.
  - Refcount ops for nodes.
  - Exception throw helpers (create + raise).
- Thread-safety and reentrancy constraints: helpers must be safe under nested calls.

### Example: C ABI wrapper

```
extern "C" QoreValue qore_rt_add_any(QoreValue a, QoreValue b, ExceptionSink* xsink);
extern "C" QoreValue qore_rt_to_string(QoreValue v, ExceptionSink* xsink);
extern "C" void qore_rt_incref(QoreValue v);
extern "C" void qore_rt_decref(QoreValue v);
```

## 11. Deoptimization and State Mapping
- State snapshot format: locals, live SSA temps, program counter (block + instr index), exception state.
- Live value map: IR values mapped to interpreter slots or temporary stack entries.
- Legal deopt points: any instruction boundary with a defined state map.
- Recovery behavior: transfer to IR interpreter with identical semantics from the deopt point.
- Required metadata: per-block state map, value ownership state, and active cleanup list.

### Example: deopt state map record

```
deopt_state {
    block = 3
    ip = 5
    locals = [%l0, %l1, %l2]
    temps = {%t7 -> stack[0], %t9 -> stack[1]}
    cleanup = [%t7, %t3]
}
```

## 12. AST to IR Lowering Rules (Phase 1)
- Coverage list (Phase 1 minimum):
  - Constants: int/float/bool/string/nothing.
  - Variables: local references and assignments.
  - Arithmetic and comparisons for numeric types.
  - Logical operators with short-circuit control flow.
  - If/else, while, for loops (simple counter form) with break/continue.
  - Return statements.
  - Direct function calls (no dynamic dispatch).
- Try/catch with rethrow and cleanup semantics.
- Unhandled constructs: function-level fallback to AST interpreter (no mixed-mode in v1).
- Type info preservation: use `QoreTypeInfo*` where available to emit typed ops; otherwise emit `.any` ops.

## 13. IR Interpreter Contract
- Execution model: block-level instruction pointer, exception checks after any op that can throw.
- Block semantics: instructions execute in order until a terminator; terminators update the current block/ip.
- Consistency with AST interpreter: identical refcount behavior and exception propagation.
- Cleanup behavior: interpreter must honor cleanup lists on normal and unwind paths.
- Runtime calls: `.any` ops must dispatch through runtime helpers with `ExceptionSink`.

## 14. Verifier Requirements
- SSA dominance rules and phi correctness.
- Terminator placement and block structure.
- Type consistency (typed ops with typed inputs).
- Cleanup list validation: temporaries tracked and consumed correctly on all exits.
- Exception edges: every throwing op uses `invoke` or an explicitly modeled unwind edge.

## 14. Testing and Validation
- Parity tests: AST vs IR output equivalence.
- Exception tests: cleanup order, rethrow, nested try/catch.
- Refcount tests: leak checks and debug verification hooks.
- Parse-analysis propagation: ensure parse-time analysis drives opcode specialization (int/float) and mixed-type fallbacks.

### IR Smoke Test (CMake Optional)

Build:

```
cmake -S . -B build -DQORE_BUILD_IR_SMOKE=ON
cmake --build build --target qore-ir-smoke
```

Run:

```
./build/qore-ir-smoke
```

## 15. Open Questions / Decisions Log
- Exception model details: sink threading vs implicit state.
- Value tag encoding: bit layout for JIT compatibility.
- Deopt granularity: instruction-level vs block-level.

## 16. Parser Analysis Integration
- The lowering pipeline relies on `QoreParseContext` to track metadata such as whether a declared local is "definitively assigned" or may still represent `NOTHING`. This information should be threaded through the parser so the IR visitor can emit typed guards when the parser indicates a value is definitely an `int`, `float`, etc., and otherwise emit `.any` guards.  
- Locals declared with hard types (e.g., `int i;`) must be treated as unassigned/`NOTHING` until an assignment occurs; the parse context must distinguish between:
  - `definitely assigned`: can emit optimized, type-specialized `store.local`/`load.local` without extra `NOTHING` guards.
  - `maybe NOTHING`: lowering must insert guards before type-specialized uses or fall back to `.any` instructions.
- The visitor API should expose helpers such as `bool isLocalDefinitelyAssigned(LocalVar* slot)` and `TypeInfo* guaranteedType(LocalVar* slot)` so lowering does not keep external maps.
- Parser-provided analysis should also indicate when expressions can `throw` so the lowering can decide whether to emit `invoke` versus a simple `call` instruction.

## 17. Immediate Next Steps
- Finalize this spec by codifying the SSA semantics, exception edges, reference-count ops, guard rules, and parser data requirements (per Phase 0 checklist) before broader lowering work begins.
- Ship the IR headers (`QoreIR.h`, `QoreIRBuilder.h`, `QoreIRPrinter.h`, `QoreIRVerifier.h`) with concrete enumerations of typed instructions, guard conventions, landing pads, and cleanup semantics.
- Start the AST→IR lowering visitor focused on priority operator families, ensuring exception-producing expressions compile to `invoke` plus cleanup, and adjust parser analysis plumbing to provide definitively-assigned metadata.
- Stabilize the interpreter by finishing `StoreLocal` lowering, handling pre-/post- increments, adding the exec-mode smoke test with try/catch, and validating the smoke path under `qore -b` + Valgrind to prove refcount safety.
- After each milestone, revisit this spec/plan to capture new constraints (parser needs, valgrind findings, etc.) before progressing further.
