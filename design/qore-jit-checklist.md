# Qore JIT Plan Checklist (IR-First)

This checklist tracks the full multi-phase plan and the tighter Phase 0–1 spec items. It incorporates the qore-llvm analysis: start fresh and use qoretechnologies/qore-llvm.git only as reference (landingpad/invoke patterns, NRT-style wrappers), not as a code base.

Status key:
- [ ] not started
- [~] in progress
- [x] complete
- [!] blocked / needs decision

Decision constraints:
- Exceptions: use real exceptions with stack unwinding (LLVM invoke/landingpad), with `ExceptionSink` compatibility for non-JIT paths.
- Refcounting & memory management: must be fully exception-safe.
- Typed locals can be unassigned (NOTHING); lowering must preserve that semantics.
- JIT only supports PO_MODERN (non-modern code must fall back to interpreter with a clear diagnostic).

---

## End Goal
- A production-ready JIT that preserves Qore semantics, uses an IR as the single source of truth, supports deopt, and has exception-safe refcounting and full test coverage.

---

## Phase 0: IR Design & Spec (tight checklist)

- [x] Define IR value model: QoreValue tag semantics, NOTHING, and explicit "maybe-NOTHING" analysis.
- [x] Define exception model: explicit exception edges for all potentially throwing ops; invoke/landingpad semantics.
- [x] Define refcount ops in IR (incref/decref/try_decref) and lifetime rules under exceptions.
- [x] Define control-flow IR (blocks, terminators, phi, unreachable, rethrow).
- [x] Define typed vs dynamic op variants (e.g., add.int/add.any) and guard strategy.
- [x] Define lvalue/rvalue semantics in IR (load/store, element/field ops, op-assign).
- [x] Define call ABI for IR interpreter and JIT (args, locals, closure/env).
- [x] Define parse-analysis propagation interface (QoreParseContext use) and dataflow expectations.
- [x] Write IR specification doc with examples and invariants.

Deliverables:
- [x] `design/qore-ir-spec.md`

---

## Phase 1: AST -> IR Lowering (tight checklist)

- [x] Lower all expression families with invoke where needed:
  - arithmetic, bitwise, comparisons, logical, regex (including nmatch), container ops (including pop/push),
    date/time ops, instanceof, trim/chomp/transliteration, background, list assignment
  - cast/conversion ops
  - deref / element / field access
  - map/fold/foldr/select
- [x] Lower all statements: if/else, loops, switch, try/catch, throw/rethrow, return, break/continue.
- [x] Lvalue lowering for op-assign and assignment to all containers and objects.
- [x] Preserve typed-local semantics where values can be NOTHING.
- [x] Integrate parse-analysis propagation (definite assignment, maybe-NOTHING) into lowering decisions.
- [x] Audit remaining `.any` helper ops (regex/extract/remove/keys/exists/elements/dot-eval/cast/map-select/hash-map)
  and document `.any` as the Phase‑1 endpoint.
- [x] Add IR verifier checks for invoke targets and terminator correctness.
- [x] Add IR printer updates for new ops and invoke forms.

Deliverables:
- [x] `lib/QoreIRLowering.cpp`
- [x] `include/qore/intern/QoreIRLowering.h`
- [x] `lib/QoreIRVerifier.cpp`
- [x] `lib/QoreIRPrinter.cpp`
- [x] `test/ir/`

### Phase 1 Execution Checklist (current)

- [x] Audit remaining lowering error paths ("unsupported ... for IR lowering") and cover or explicitly reject:
  - [x] Unsupported expression nodes in `lowerExpression()` (fill gaps or add explicit policy checks).
  - [x] Unsupported statement nodes in `lowerStatement()` (confirm all statement classes handled).
  - [x] Unsupported lvalue types/ranges in assignment/op-assign/inc/dec/shift/unshift/splice.
  - [x] Unsupported VarRef kinds in `loadVarRef()/storeVarRef()` (notably `VT_IMMEDIATE` / `VT_UNRESOLVED`).
- [x] Expand parse-analysis driven opcode selection where available for:
  - [x] Extract/Remove/Keys type-driven opcodes (list/hash/string/binary) via operand analysis.
  - [x] Map/Select/MapSelect/HashMap/HashMapSelect typed opcodes via element/return analysis.
  - [x] Regex extract/subst typed opcodes (list/string) via return analysis.
- [x] Expand tests in `examples/test/ir/` to cover any newly supported nodes or explicit rejection cases.

---

## Phase 2: IR Interpreter

- [x] Execute all IR ops (including invoke with exception paths).
- [x] Validate refcount behavior at all exits (normal and exceptional).
- [x] Add `--exec-mode=ir` or equivalent runtime switch.
- [x] Ensure all existing tests pass under IR.
- [x] Add IR smoke tests and coverage tests for operators and lvalues (including const/refcount ops).
- [x] Add parse-option gate: if not PO_MODERN, skip JIT and fall back to interpreter (with a warning).
- [x] Keep IR and JIT parse-option gating consistent (single policy, reused warning text).

---

## Current Gap Snapshot (2026-01-23)

- [x] Tighten Phase 0 spec items with concrete ABI + ownership tables (call ABI, refcount ownership per op).
- [x] Invoke coverage: ExpressionStatement + nested expressions now use `invoke` for expr-ops; added invoke for
  lvalue ops (load/store/op-assign/inc/dec/shift/unshift/splice), binary/unary ops, switch case comparisons,
  and condition/logical `ToBool` via `invoke_opcode`.
- [x] Parse-analysis propagation: ensure definite assignment/maybe-NOTHING is used for mixed-type comparisons and more ops (not only numeric); add dot-eval return-type coverage (int/string/date/object) and optional-return coverage for dot-eval.
- [x] Lvalue op-assign coverage: verify hash/object/array lvalues and range-slice lvalues have full op-assign support.
- [x] Negative tests: range/lvalue invalid ops and unsupported date shift-assign expectations.
- [x] IR interpreter: add tests for `invoke.sim.error`, `call`/`call.indirect`/`dot.eval` invoke paths, and unwind cleanup paths.
  - [x] invoke.sim.error + call/dot.eval invoke coverage.
  - [x] Unwind cleanup tests via minimal IR executor with cleanup list + catch.exception coverage.
- [x] IR interpreter: execute statement opcodes (foreach/on-block-exit/debug/thread-exit); add thread-exit executor smoke test.
- [x] Docs: add a short note on running IR smoke under valgrind with `qore -b`.
- [x] Statement lowering: do-while, foreach, on-block-exit, thread-exit, debug covered.
- [x] Added IR opcodes for regex nmatch, instanceof, trim/chomp/transliteration, background, list assignment, pop/push.
- [x] IR exec-mode smoke test for control-flow without fallback warnings.
- [x] IR opcode coverage includes const and refcount operations.

---

## Phase 3: LLVM Integration & IR -> LLVM Lowering

- [~] Add LLVM optional build configuration and ORC JIT bootstrap.
- [~] Implement IR -> LLVM lowering for initial hot paths (int/float arith, comparisons, control flow).
- [~] Implement invoke/landingpad + personality integration with Qore exception model.
- [~] Provide deopt fallback to IR interpreter for failed guards.
- [~] Add basic benchmarks and validation against IR interpreter.
- [~] Enforce PO_MODERN at JIT entry (non-modern falls back to interpreter/IR with warning).

---

## Phase 4: Tiered Compilation

- [ ] Execution counters and tier transitions (AST -> IR -> JIT).
- [ ] Type profiling and guard placement strategy.
- [ ] Deopt policy and failure handling.
- [ ] OSR for hot loops.

---

## Phase 5: Feature Expansion

- [ ] Strings, hash/list access, object ops, closures, method calls.
- [ ] Full operator coverage and mixed-type specialization.
- [ ] Debug info and source mapping.

---

## Phase 6: Hardening

- [ ] Full test suite under IR and JIT.
- [ ] Valgrind/ASAN for IR + JIT paths (with `qore -b` to disable signals).
- [ ] Thread safety, code invalidation, and concurrency.
- [ ] Performance regression checks.

---

## qore-llvm Reference Pointers (do not reuse codebase)

- Invoke/landingpad patterns: `lib/cg/FunctionCompiler.cpp` (qore-llvm)
- NRT wrapper approach: `include/qore/nrt/nrt.h` (qore-llvm)
- Instruction/block design as inspiration only (not compatible with main Qore)
