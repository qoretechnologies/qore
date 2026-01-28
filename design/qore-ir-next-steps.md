# Qore IR Next Steps Checklist

This supplement to `qore-jit-checklist.md` focuses on the work we are doing *right now*. It captures the Phase 0/1 subtasks we need to finish before moving to LLVM lowering and tiered compilation.

## Phase 0 → 1 Immediate Work

- [x] Finish `design/qore-ir-spec.md` with SSA semantics, exception edges, guard expectations, refcount ownership, and parser-analysis requirements.
- [x] Ship the core IR API headers/builder/printer/verifier so downstream passes know the types of instructions we support.
  - [x] Audit `QoreIROpcode` to cover all typed/dynamic operators, control flow, lvalue ops, invoke/landing-pad, and refcount primitives required in Phase 0.
  - [x] Add builder helpers for any instructions the lowering visitor needs directly (invoke, landingpad, catch, call variants, refcount, load/store, etc.).
  - [x] Extend `QoreIRPrinter` so every opcode has a human-readable name and captures any new variants added in this phase.
  - [x] Ensure `QoreIRVerifier` validates terminators, operand counts, and cleanup state for the expanded instruction set.
- [~] Ensure `QoreParseContext` exposes the metadata (definite assignment, known type info) the lowering visitor needs to emit guards.
- [ ] Define the AST→IR visitor structure for prioritized operators (typed arithmetic, comparisons, logical ops, map/fold, date/shift variants) and statements (if/while/for/try/catch/throw).
- [ ] Add IR lowering coverage for throw expressions within try/catch so invoke/landingpad sequences are produced.
- [ ] Stabilize `StoreLocal` lowering plus pre/post increment/decrement handling and validate with the exec-mode try/catch smoke test.
- [x] Run the exec-mode smoke test under `qore -b` + Valgrind and document the process (see README updates below).

## Stabilization Tasks

- [x] Add the new `examples/test/ir/IRExecMode*.qtest` files (exec-mode and smoke) to cover interpreter + exception scenarios.
- [x] Update doc/note about running IR smoke under Valgrind (with `qore -b` to disable signals) and mention the new tests (README now documents the workflow).
- [ ] Track plan revisions after each milestone (spec done, APIs done, lowering started) to capture new requirements.

## Ongoing Phase 1 Readiness

- [ ] Evaluate the existing IR interpreter (`lib/QoreIRInterpreter.cpp`) to ensure it handles the newly defined opcodes, cleanup lists, and exception flows.
- [ ] Confirm `test/ir/` coverage grows with each new lowering capability (e.g., mage/fold operators, lvalue coverage, guard semantics).
- [ ] Audit remaining `.any` helper ops (regex/extract/remove/keys/exists/elements/dot-eval/cast/map-select/hash-map)
  and decide whether typed opcodes should be added or documented as Phase‑1 endpoints.
