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

## 5a. Parser Analysis & Guard Semantics
- `QoreParseContext` is threaded through parsing so every node can note whether a declared local or expression is definitely assigned, still `NOTHING`, or has a statically known type. The context exposes helpers like `isLocalDefinitelyAssigned(LocalVar*)`, `guaranteedType(LocalVar*)`, and expression-level answers (`analysisIndicatesInt/Float`).
- Typed locals (e.g., `int i;`) begin life as `NOTHING` until the first assignment. Any operation that assumes a non-`NOTHING` payload must either insert a `GuardNotNothing` targeting the current exception unwind block or fall back to the `.any` variant that already accepts `NOTHING`.
- The lowering visitor consults `selectAnalysisType` and `needsNotNothingGuard` so every use site respects the parse analysis and keeps typed operations exception-safe. `GuardNotNothing` is a first-class opcode in `QoreIROpcode` so the interpreter/JIT can rely on it when deoptimization or fallback is required.
- `QoreIRVerifier` enforces that typed instructions acting on `NOTHING`-capable locals either follow a guard or are replaced by `.any` opcodes, preventing silent semantic gaps between AST and IR interpretations.
- Parse analysis also informs exception-awareness (whether an expression can throw) so lowering can emit `invoke`, landing pads, and cleanup sequences that mirror the AST interpreter’s behavior.

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
- Constants: int/float/bool/string/nothing/null.
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
- The lowering pipeline relies on `QoreParseContext` to carry metadata such as whether a declared local is "definitively assigned" or may still represent `NOTHING`. `QoreIRLowering` receives the context through its constructor or `setParseContext()` and passes it to every expression visitor so the IR emission can consult the context directly instead of building ad-hoc maps of analysis results.
- Typed locals (e.g., `int i;`) begin life as `NOTHING`; parse analysis tracks their state so typed operations either insert `GuardNotNothing` (using `needsGuardForLocal` / `maybeInsertNotNothingGuard`) or switch to `.any` instruction variants. The visitor keeps the context current by invoking `markLocalAssignmentFromExpression()` and `markLocalUnassignmentFromExpression()` around assignment-like expressions, so actions such as `remove`, `delete`, or catch-bound reassignments are reflected in the guard decisions.
- The context exposes helpers such as `isLocalDefinitelyAssigned`, `guaranteedType`, and `expressionAnalysisType`, enabling lowering to choose typed arithmetic/comparison opcodes, container access paths, or fall back to runtime helpers when the type is ambiguous. These helpers are defined alongside `QoreParseContextLvarHelper`, `QoreParseContextFlagHelper`, and the `analysis` member so parser passes can preserve and restore the right analysis flags across nodes and scopes.
- Parser analysis also tracks exception behavior. `QoreParseContext::expressionCanThrow()` (and `QoreIRLowering::expressionCanThrow()` which reads it) indicates whether a node may unwind, so lowering emits `invoke` instructions only when needed and wires landing pads/cleanup blocks that run `decref.nothrow` and rethrow exactly as the AST interpreter would.

## 17. Immediate Next Steps
- Finalize this spec by codifying the SSA semantics, exception edges, reference-count ops, guard rules, and parser-analysis helper requirements (per the Phase 0 checklist) so downstream passes share a stable contract and the `/tmp/qore-jit.md` checklist remains the single source of outstanding decisions.
- Ship the IR headers (`QoreIR.h`, `QoreIRBuilder.h`, `QoreIRPrinter.h`, `QoreIRVerifier.h`) with concrete enumerations of typed instructions, guard conventions, landing pads, cleanup semantics, and ownership annotations.
- Continue the AST→IR lowering work for the priority operator families (arithmetic, comparisons, logical, foldl/foldr/map, throw/try/catch, date/shift variants) while threading the parse context so exception-producing expressions lower through `invoke`/landingpad and typed guards respect `maybe NOTHING` locals.
- Steady the interpreter by finishing `StoreLocal` lowering, stabilizing pre-/post- increment/decrement lowering, expanding op/lvalue lowering coverage (hash/list/object/range/date) plus the map/fold families, and keep growing the exec-mode IR tests so every new operator is exercised.
- Keep rerunning the exec-mode smoke suite (the new `examples/test/ir/IRExecMode*.qtest` collection) under `qore -b`/Valgrind (`--leak-check=full --show-leak-kinds=definite,indirect,possible`) after each round of changes; capture the clean-heap baseline so regressions in refcounting or exception handling are visible.

## 18. Phase 0 Deliverables
- **Lock down the spec**: confirm the SSA shape, guard semantics, exception-linkage rules, and refcount operation contracts so all later passes reference a stable document.
- **Expose the IR APIs**: finalize `include/qore/intern/QoreIR*.h` and the builder/printer/verifier declarations so users of the IR can code against a concrete instruction set.
- **Parser analysis visibility**: extend `QoreParseContext` so every node/visitor can report definite assignment, type guarantees, and exception risks without external maps (see the hook inventory below).
- **Guard enforcement**: document how `GuardNotNothing`, typed instructions, and `.any` fallbacks interact with parser analysis, and verify that the verifier enforces the guard-or-fallback rule for typed operations.
- **Valgrind baseline**: wire up the `qore-ir-smoke` runner with `--exec-mode=ir`, validate it, and capture a `qore -b`/Valgrind pass to serve as the Phase 0 regression baseline.

## 19. Parser Analysis Hook Inventory
The lowering visitor must consume parse analysis data directly from `QoreParseContext`. The table below enumerates the hooks that the lowering pass currently depends on; entries annotated **missing** describe functionality that still needs to be added or stabilized to meet Phase 0 requirements.

| Hook / Query | Purpose | Status |
| --- | --- | --- |
| `bool isLocalDefinitelyAssigned(LocalVar* local)` | Guards typed locals (int, date, hash, etc.) so lowering emits `GuardNotNothing` or `.any` variants only when necessary. | **available** – `QoreParseContext::isLocalDefinitelyAssigned` exposes `LocalVar::isAssigned()` via `QoreLibIntern.h` and is already consumed by the lowering pass. |
| `bool needsGuardForLocal(LocalVar* local)` / `bool isLocalMaybeNothing(LocalVar* local)` | Fast-path for locals affected by `remove`, `delete`, or uninitialized declarations so guard placement is precise. | **available** – `QoreParseContext::needsGuardForLocal` is public and the lowering visitor calls `maybeInsertNotNothingGuard` when it reports `true`. |
| `QoreTypeInfo* guaranteedType(LocalVar* local)` | Drives typed lowering (e.g., `add.int`, `list.size`) when the parse-time type of a local is known. | **available** – `guaranteedType` returns `local->parseGetTypeInfo()` and feeds `selectAnalysisType`/typed opcode selection. |
| `bool canExpressionThrow(const AbstractQoreNode* node)` | Lets lowering choose between `call` and `invoke` so cleanup/refcounts are handled correctly on unwind. | **available** – `QoreParseContext::expressionCanThrow` (and the visitor helper `expressionCanThrow`) read `QoreParseAnalysis::NeverThrows` from `parse_context.analysis`. |
| `QoreTypeInfo* expressionAnalysisType(const AbstractQoreNode* node)` | Enables typed lowering when the expression result type is statically known (e.g., `foldl` with `int` accumulator). | **available** – `QoreParseContext::expressionAnalysisType`, `ParseNode::getParseAnalysis`, and `selectAnalysisType` now deliver typed hints to the lowering logic. |
| `void markLocalAssigned(LocalVar* local, bool definite, QoreTypeInfo* type)` | Allows parser nodes to update the context during conditionals/loops so the later lowering pass reads accurate state instead of duplicating analysis. | **available** – `QoreParseContext::markLocalAssignment`, `markLocalAssignmentFromExpression`, and `markLocalUnassignmentFromExpression` keep the analysis flags aligned with the AST changes. |
| `bool isExpressionDefinitelyAssigned(const AbstractQoreNode* node)` | Helps lowering skip redundant guards for expressions guaranteed to produce values (literal constants, `this`, etc.). | **available** – `QoreParseContext::isExpressionDefinitelyAssigned` reads the `DefinitelyAssigned` flag that parser visitors maintain. |

Keeping this data bound to `QoreParseContext` avoids ad-hoc external maps and stale state, and it lets the IR lowering visitor remain deterministic about guard insertion and exception modeling. Section 5a laid out the behavioral requirements; this inventory now calls out the concrete hooks we still need to finish Phase 0.

## 20. Tightened Phase 0 Checklist

1. **Document and verify the parse-context APIs for lowering**
   - Capture the `needsGuardForLocal`, `guaranteedType`, `markLocalAssignment` family, and `expressionCanThrow` helpers in the spec so downstream lowering developers know how to consult `QoreParseContext` instead of building duplicate analysis maps.
   - Double-check the include-order / header visibility (e.g., `QoreLibIntern.h`) so lowering files can include the context helpers without build breaks.
   - Walk through the parser passes and helpers (e.g., `QoreParseContextLvarHelper`, `QoreIRLowering::markLocalAssignmentFromExpression`) to confirm the context is forwarded through every branch/try/catch node before emitting `GuardNotNothing`/`invoke`.

2. **Define `GuardNotNothing` usage policy**
   - Document in the spec which typed instructions require `GuardNotNothing` versus `.any` fallbacks when the parse context reports `maybe NOTHING`.
   - Prove the policy via tests (update the Phase 0 spec tests to cover unassigned typed locals, `remove`/`delete` resets, and expression-level NOTHING propagation).
   - Make sure the IR verifier enforces the guard-or-fallback rule so new lowering paths can’t slip typed loads without guards.

3. **Tighten exception metadata flow**
   - Establish a unified `canExpressionThrow` query and document how it drives `invoke`/cleanup generation for calls, casts, and conversions.
   - Confirm the lowered AST traceback for try/catch nodes wires landing pads and cleanup sequences exactly like the AST interpreter (link to the new throw-expression lowering work).
   - Capture any include/build failures (e.g., missing parse-context header exposure used by the IR lowering files) in a short note in this section so they’re not forgotten before the next plan review.

4. **Publish validation baseline**
   - Update `design/qore-jit-checklist.md` to reflect these Phase 0 checkpoints and mention the Valgrind/`qore -b` smoke test expectation.
   - Run the expanded exec-mode IR smoke suite and record the first clean `valgrind --leak-check=full qore -b ...` result to serve as the Phase 0 regression baseline.
   - Add a paragraph summarizing the current build/test status and any TODOs (e.g., “needs include fix in QoreParseContext.h before lowering can include the new helper,” “valgrind uncovered handler leaks in GuardNotNothing cleanup”) so the next turn can pick up smoothly.

   **Current status (2026-01-28)**: `qore-ir-smoke` passes under Valgrind with **0 errors** and only expected “still reachable” allocations (mpfr/gmp init + dynamic loader). The exec‑mode IR smoke suite now covers cast/cast‑lvalue, dot‑eval call refs, regex extract/no‑match + subst, typed maybe‑NOTHING guards, range slice with maybe‑NOTHING bounds, op‑assign with maybe‑NOTHING lvalues, ternary mixed types, unary maybe‑NOTHING, and hash/list mixed ops. Re‑run and re‑record this baseline after any further lowering changes.
5. **Phase‑1 operator family checklist (execute sequentially)**
   - **Range & date helpers**
     * Emit `RangeInt`/`RangeFloat` when parse analysis confirms the bounds, with `RangeAny` as the conservative fallback.
     * Guard typed operands (locals or temporaries) before constructing the range so `GuardNotNothing` preserves semantics when `int`/`date` locals are unassigned.
     * Confirm exception lowering for `lowerRange`, `lowerSquareBracketsRange`, and the associated lvalue helpers uses `invoke` + landing pads consistently.
     * **Status (2026-01-28):** guard inserts added for range slice inputs; date shift operator and range-slice maybe-NOTHING smoke coverage added; Valgrind baseline recorded.
   - **Hash/List dereference & mutation**
     * Lower `hash`, `list`, and `object` access/mutation through the dedicated lvalue opcode set (`LoadLValue`, `StoreLValue`, `Pre/Post Inc/Dec`, shift/add assignments).
     * Apply `GuardNotNothing` before each container access when the parse context reports `maybe NOTHING`, ensuring typed container lvalues never interact with uninitialized locals.
     * Add smoke-test coverage for nested container operations plus the guarded operators, including both success and exception flows.
     * **Status (2026-01-28):** base-lvalue guards added for lvalue ops; container guard + nested deref smoke tests added; Valgrind baseline recorded.
   - **Shift/assign families**
     * Finish lowering for `shift`, `unshift`, `splice`, and all shift-assign operators so typed cases map to `ShlAssignInt`/`ShrAssignInt` while untyped falls back to `.any`.
     * Guard the lvalue before mutation when it comes from a typed local to prevent operating on `NOTHING` values.
     * Expand exec-mode IR tests to touch these operators individually under normal execution and simulated errors.
     * **Status (2026-01-28):** exec-mode smoke tests added for list shift/unshift/splice + lvalue shift-assign; Valgrind baseline recorded.
   - **Residual `.any` fallbacks (Phase‑1b backlog)**
     * Track helper-only ops that currently have **only** `.any` opcodes: `ExtractAny`, `RemoveAny`, `KeysAny`,
       `RegexExtractAny`, `RegexSubstAny`, `ExistsAny`, `ElementsAny`, `DotEvalAny`, `MapSelectAny`, `HashMapAny`,
       `HashMapSelectAny`.
     * Track operators where typed coverage is partial: `UnaryPlusAny` (no typed), `ModInt/ModAny` (no float),
       `And/Or/Xor` (int/any only), `Eq/Ne` (int/any only), `Cmp` (int/float/any but no string-specialized op).
     * **Phase‑1 decision:** keep these helper ops as `.any` endpoints for now; rely on exec‑mode smoke tests to
       ensure they execute in IR without fallback. Revisit for Phase‑1b/Phase‑2 typed opcode work.
     * Add exec‑mode IR smoke tests for any new typed opcodes so fallback warnings are never emitted.
     * **Status (2026-01-28):** helper ops smoke test added; `RegexMatchBool` opcode introduced and lowering now
       emits it; typed cast opcodes (`CastList`, `CastHash`, `CastObject`, `CastEnum`) added for resolved cast nodes;
       Valgrind baseline recorded.
   - **Range/date + container tests**
     * Extend `examples/test/ir/IRExecMode*.qtest` with cases that explicitly validate the guard policy and exception handling for each operator family.
     * After each family is widened, rerun `qore -b --exec-mode=ir` under Valgrind and capture the clean heap baseline so we know no new refcount leaks or unwinding regressions slipped in.
     * Keep updating this checklist so automation scripts can track the next family and whether its smoke tests/Valgrind results exist.
     * **Status (2026-01-28):** exec-mode IR smoke suite extended through shift/unshift/splice + lvalue shift-assign; Valgrind baseline recorded after each family (range/date, container, shift).
