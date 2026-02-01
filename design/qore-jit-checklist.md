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

## Phase 3: LLVM Integration & IR -> LLVM Lowering — COMPLETE ✓

- [x] Add LLVM build configuration and ORC JIT bootstrap.
      LLVM 16+ required as a mandatory build dependency (like nghttp2, Brotli, etc.).
- [x] Implement IR -> LLVM lowering for initial hot paths (int/float arith, comparisons, control flow).
      40+ IR opcodes lowered: constants, arithmetic, bitwise, comparisons, locals, control flow,
      phi nodes, refcount, guards, calls/invoke, exception handling, string constants.
- [x] Implement invoke/landingpad + personality integration with Qore exception model.
      Uses ExceptionSink polling model (not C++ exceptions); `qore_rt_invoke_expr` for AST dispatch.
- [x] Provide deopt fallback to IR interpreter for failed guards.
      Guards branch to deopt targets; `qore_rt_deopt` placeholder; `executeWithFallback` chain.
- [x] Add basic benchmarks and validation against IR interpreter.
      JITSmoke.qtest: 22 tests; JITBenchmark.q: numeric loops in all 3 modes; identical results.
- [x] Enforce PO_MODERN at JIT entry (non-modern falls back to interpreter/IR with warning).
- [x] Make LLVM a required build dependency (remove `WITH_JIT` option and `QORE_JIT_ENABLED` guards).

Deliverables:
- [x] `include/qore/intern/JITRuntime.h` / `lib/JITRuntime.cpp` — C ABI runtime helpers
- [x] `include/qore/intern/QoreJIT.h` / `lib/QoreJIT.cpp` — LLVM ORC JIT compiler + function cache
- [x] `include/qore/intern/QoreIRToLLVM.h` / `lib/QoreIRToLLVM.cpp` — IR→LLVM lowering with NaN-boxing
- [x] `examples/test/ir/JITSmoke.qtest` — 22 JIT-specific tests (23 assertions)
- [x] `examples/test/ir/JITBenchmark.q` — benchmark comparing all three modes
- [x] Valgrind clean: 0 errors, 0 leaks on all JIT tests

---

## Phase 4: Tiered Compilation

### Phase 4a: Per-Function Tiered Execution — COMPLETE ✓

- [x] Execution counters and tier transitions (AST → IR → JIT) per `UserVariantBase`.
- [x] `QEM_TIERED = 3` exec mode: automatic per-function promotion based on call counts.
- [x] Configurable thresholds: `--jit-ir-threshold=N` (default 100), `--jit-jit-threshold=N` (default 1000).
- [x] Thread-safe promotion: `std::atomic<uint64_t>` counters, `std::call_once` for IR lowering and JIT compilation.
- [x] Failure permanence: IR lowering failure → stays on AST; JIT compilation failure → stays on IR.
- [x] Cached IR/JIT: `QoreIRFunction` and `JitFunctionPtr` cached per variant; written once, then read-only.
- [x] Non-%modern code never promoted (stays on AST).
- [x] Nested block locals: recursive statement tree walk collects all locals from fully-lowered
      statements (if/for/while/try/switch) for pre-instantiation during IR/JIT execution.
- [x] Deopt policy and failure handling: graceful fallback on IR lowering or JIT compilation failure.

Deliverables:
- [x] `include/qore/common.h` — `QEM_TIERED` enum value
- [x] `include/qore/intern/Function.h` — tier state fields on `UserVariantBase`
- [x] `lib/Function.cpp` — `evalTiered()`, `attemptIRLowering()`, recursive local collection
- [x] `include/qore/intern/QoreJIT.h` / `lib/QoreJIT.cpp` — static threshold accessors
- [x] `command-line.cpp` — `--exec-mode=tiered`, `--jit-ir-threshold`, `--jit-jit-threshold`
- [x] `examples/test/ir/TieredSmoke.qtest` — 12 test cases (66 assertions)
- [x] Valgrind clean on tiered mode tests

### Phase 4b: Type Profiling, Guard Refinement, OSR (future)

- [ ] Type profiling: record actual runtime types to improve guard placement on recompilation.
- [ ] Guard refinement: use deopt feedback to specialize guards.
- [ ] OSR for hot loops (on-stack replacement: promoting a running loop mid-execution).
- [ ] Recompilation: re-lowering/re-JITing a function with better type info after deopt.

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

## Phase 7: AOT Compilation (Ahead-of-Time)

Compile Qore source to standalone executables that link against libqore for runtime services.

Prerequisites: Phase 5 (native lowering of most ops) and Phase 6 (hardening) should be substantially
complete. The more ops lowered natively, the less the AOT binary depends on the AST interpreter at
runtime.

- [ ] Object code emission via LLVM `TargetMachine::addPassesToEmitFile` (reuse IR→LLVM modules from
      Phase 3 lowering, emit `.o` files instead of feeding to ORC JIT).
- [ ] Generate `main()` entry point that initializes the Qore runtime (threading, timezone manager,
      module loading, signal handling) and calls the compiled Qore entry function.
- [ ] Link against libqore for runtime services (`qore_rt_*` helpers, refcounting, exception handling,
      module loading, I/O, threading).
- [ ] Handle module imports: compile `%requires` dependencies or link them as shared libraries; resolve
      user modules and binary modules at link time or via libqore's module loader at startup.
- [ ] Eliminate or reduce `qore_rt_invoke_expr` calls: ops that still delegate to AST evaluation either
      need native lowering (Phase 5) or the AST interpreter must be linked as a fallback runtime
      component.
- [ ] CLI interface: `qore --compile script.q -o script` or similar; produce ELF/Mach-O/PE binary.
- [ ] Cross-compilation support: target architecture selection via LLVM triple.
- [ ] Strip/embed Qore source and debug info: options for including source maps, DWARF debug info,
      or stripping all metadata for minimal binary size.
- [ ] Test: compiled executables produce identical output to `qore script.q` for the full test suite.
- [ ] Test: valgrind clean on compiled executables.

---

## qore-llvm Reference Pointers (do not reuse codebase)

- Invoke/landingpad patterns: `lib/cg/FunctionCompiler.cpp` (qore-llvm)
- NRT wrapper approach: `include/qore/nrt/nrt.h` (qore-llvm)
- Instruction/block design as inspiration only (not compatible with main Qore)
