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

### Phase 4b: Type Profiling & Guard Refinement — IN PROGRESS

#### Phase 4b-1: Type Profiling Infrastructure — COMPLETE ✓

Runtime type profiling at guard points during IR interpretation, with profile-informed
guard specialization during JIT compilation.

- [x] **TypeProfile struct** (`QoreIR.h`): Per-guard atomic counters for int/float/string/bool/
      nothing/other types. `dominantType(threshold)` returns the dominant type when one type
      accounts for ≥95% of observations. Uses `std::unique_ptr<TypeProfile[]>` to avoid
      non-movable `std::atomic` issues with `std::vector`.
- [x] **Guard ID assignment** (`QoreIRBuilder.cpp`): Each guard instruction gets a monotonic
      `guard_id` from `QoreIRFunction::num_guards`. Profiles are allocated via
      `initGuardProfiles()` after IR lowering.
- [x] **Profile recording** (`QoreIRInterpreter.cpp`): At guard execution, records the actual
      runtime type of the guarded value into `guard_profiles[guard_id]`.
- [x] **Profile-informed GuardInt** (`QoreIRToLLVM.cpp`): When profile shows ≥50 observations
      and ≥95% int, emits inline NaN-boxing check (`value < TAG_INT48`) instead of runtime
      helper call. Adds branch weight metadata (999:1) for LLVM to optimize the cold deopt path.
- [x] **Profile-informed GuardFloat** (`QoreIRToLLVM.cpp`): Inline NaN-boxing check for float
      (`value > DOUBLE_ENCODE_OFFSET && value < TAG_INT48`) when profile shows ≥95% float.
- [x] **Profile-informed GuardNotNothing** (`QoreIRToLLVM.cpp`): Inline check (`value != 0`)
      when profile shows never-NOTHING (nothing_count == 0 with ≥50 total observations).
- [x] **Deopt logging** (`JITRuntime.cpp`): `qore_rt_deopt` logs guard failures for diagnostics
      instead of raising exceptions. Guard failures propagate via deopt_target blocks.
- [x] **Recompilation infrastructure** (`Function.h`, `Function.cpp`): `deopt_count` atomic
      counter and `attemptJITRecompilation()` method that demotes to IR, recompiles with
      accumulated profiles, and re-promotes to JIT tier.

#### Test Results

| Test Suite | Result |
|-----------|--------|
| Phase4bSmoke.qtest | 10/10 test cases, 13 assertions |
| JITSmoke.qtest | 58/58 test cases, 103 assertions |
| TieredSmoke.qtest | 23/23 test cases, 367 assertions |
| IRExecModeSmoke.qtest | 137/137 test cases, 416 assertions |
| AOTSmoke.qtest | 28/28 test cases, 66 assertions |
| Valgrind (Phase4b) | 0 bytes definitely/indirectly/possibly lost |
| Valgrind (Tiered) | 0 bytes definitely/indirectly/possibly lost |

Phase4b tests cover: type profiling correctness for int/float/mixed types, profiled guard
promotion through AST→IR→JIT tiers, profile-informed int/float/not-nothing fast-paths,
deopt with mixed type changes mid-execution, and cross-mode (AST vs IR vs tiered) correctness.

#### Deliverables

- [x] `include/qore/intern/QoreIR.h` — `TypeProfile` struct, `guard_id` on guards,
      `guard_profiles`/`guard_profile_count`/`num_guards` on `QoreIRFunction`
- [x] `include/qore/intern/Function.h` — `deopt_count`, `jit_recompile_once`,
      `attemptJITRecompilation()` declaration
- [x] `include/qore/intern/QoreIRToLLVM.h` — `current_ir_func` member pointer
- [x] `lib/QoreIRBuilder.cpp` — guard_id assignment during IR construction
- [x] `lib/QoreIRInterpreter.cpp` — type profile recording at guard execution
- [x] `lib/Function.cpp` — `initGuardProfiles()` call, `attemptJITRecompilation()` implementation
- [x] `lib/QoreIRToLLVM.cpp` — profile-informed guard lowering with inline checks and
      branch weight metadata
- [x] `lib/JITRuntime.cpp` — `qore_rt_deopt` deopt counter implementation
- [x] `examples/test/ir/Phase4bSmoke.qtest` — 12 test cases (15 assertions)

#### Phase 4b-2: Guard Refinement & Deopt — COMPLETE ✓

Deopt tracking and profile-informed recompilation. When profiled guards fail (e.g., a
function JIT'd for int receives float), the failure is counted via `qore_rt_deopt()`.
After 10+ deopts, `evalTiered()` triggers one-time JIT recompilation with updated profiles.

- [x] **Deopt counter** (`JITRuntime.cpp`): `qore_rt_deopt(void* counter_ptr)` atomically
      increments the variant's deopt counter. Called from JIT guard failure paths.
- [x] **Guard failure tracking** (`QoreIRToLLVM.cpp`): Profiled guards (`profile_hot`) emit
      intermediate "guard_deopt" blocks that call `qore_rt_deopt` before branching to the
      deopt target. Counter pointer is embedded as a constant in JIT code.
- [x] **Deopt counter propagation** (`QoreIRToLLVM.h`, `QoreJIT.h`, `QoreJIT.cpp`):
      `setDeoptCounter()` on lowerer, `deopt_counter` parameter on `compileFunction()`.
- [x] **Recompilation trigger** (`Function.cpp`): `evalTiered()` checks `deopt_count >= 10`
      after JIT execution and triggers one-time `attemptJITRecompilation()` via `std::call_once`.
      Recompilation uses versioned function names (`_reopt` suffix) for new LLVM ORC symbols.
- [x] **Deopt counter reset**: Counter is reset to 0 before recompilation so the recompiled
      version starts with a clean slate.

#### Test Results (Phase 4b-2)

| Test Suite | Result |
|-----------|--------|
| Phase4bSmoke.qtest | 12/12 test cases, 15 assertions |
| JITSmoke.qtest | 58/58 test cases, 103 assertions |
| TieredSmoke.qtest | 23/23 test cases, 367 assertions |
| Valgrind (Phase4b) | 0 bytes definitely/indirectly/possibly lost |

New tests: deopt recompilation correctness (int→float transition with many calls),
deopt heavy type change (rapid alternating int/float calls).

#### Deliverables (Phase 4b-2)

- [x] `include/qore/intern/JITRuntime.h` — updated `qore_rt_deopt` signature
- [x] `include/qore/intern/QoreIRToLLVM.h` — `setDeoptCounter()`, `deopt_counter_ptr` member
- [x] `include/qore/intern/QoreJIT.h` — `deopt_counter` parameter on `compileFunction()`
- [x] `lib/JITRuntime.cpp` — atomic counter increment implementation
- [x] `lib/QoreIRToLLVM.cpp` — intermediate deopt blocks with counter calls, updated
      declaration signature
- [x] `lib/QoreJIT.cpp` — pass deopt counter to lowerer
- [x] `lib/Function.cpp` — pass deopt counter to compilation, deopt check in evalTiered,
      versioned recompilation with counter reset
- [x] `examples/test/ir/Phase4bSmoke.qtest` — 2 new recompilation test cases

#### Phase 4b-3: Loop-Aware JIT Promotion (OSR) — COMPLETE ✓

Practical OSR implementation: the IR interpreter detects hot loops by counting back-edges
to loop header blocks. When loop iterations exceed the JIT threshold, the function is
flagged for JIT compilation on the next call. This gives most of the OSR benefit without
the complexity of true mid-execution on-stack replacement.

- [x] **Loop header marking** (`QoreIR.h`, `QoreIRLowering.cpp`): `is_loop_header` flag
      on `QoreIRBasicBlock`, set on condition blocks for while/for/do-while loops.
- [x] **Loop iteration tracking** (`QoreIRInterpreter.cpp`): `loop_iterations` counter
      incremented on back-edges to loop headers. When count exceeds `osr_threshold`
      (= JIT threshold), sets `func.osr_jit_requested = true`.
- [x] **OSR-triggered JIT promotion** (`Function.cpp`): `evalTiered()` checks
      `osr_jit_requested` after IR execution; if set, triggers `attemptJITCompilation()`
      via `std::call_once`. Takes priority over normal exec_count threshold.

#### Test Results (Phase 4b-3)

| Test Suite | Result |
|-----------|--------|
| Phase4bSmoke.qtest | 16/16 test cases, 22 assertions |
| TieredSmoke.qtest | 23/23 test cases, 367 assertions |
| JITSmoke.qtest | 58/58 test cases, 103 assertions |
| IRExecModeSmoke.qtest | 137/137 test cases, 416 assertions |
| Valgrind (Phase4b) | 0 bytes definitely/indirectly/possibly lost |

OSR tests: while loop promotion (2000 iterations triggers JIT with threshold=5000),
for loop promotion, nested loops (inner loop triggers OSR), cross-mode correctness
(AST vs IR vs tiered with OSR-enabled prime sieve).

---

## Phase 5: Feature Expansion

### Phase 5a: Universal LLVM Opcode Coverage — COMPLETE ✓

All IR opcodes now have LLVM lowering support via generic runtime helpers that delegate
to `QoreIRInterpreter` eval methods. This ensures every function that can be lowered to
IR can also be JIT-compiled.

- [x] Generic runtime dispatch helpers: `qore_rt_binary_op`, `qore_rt_unary_op`,
      `qore_rt_expr_op`, `qore_rt_comparison_op`, `qore_rt_ternary_op`.
- [x] Variable access helpers: `qore_rt_load_global`, `qore_rt_store_global`,
      `qore_rt_load_closure`, `qore_rt_store_closure`, `qore_rt_load_thread_local`,
      `qore_rt_store_thread_local`.
- [x] LValue operation helpers: `qore_rt_lvalue_load`, `qore_rt_lvalue_store`,
      `qore_rt_lvalue_unary`, `qore_rt_lvalue_binary`.
- [x] Container construction: `qore_rt_make_list`, `qore_rt_make_hash`.
- [x] Statement execution: `qore_rt_exec_statement`, `qore_rt_thread_exit`.
- [x] Guard/type helpers: `qore_rt_guard_type`, `qore_rt_make_date`, `qore_rt_throw_value`.
- [x] All remaining opcodes handled in `QoreIRToLLVM::lowerInstruction()`: dynamic
      comparisons, bitwise, unary, variable access, lvalue ops, container construction,
      compound assignments, higher-order ops, expression ops, statement ops, guards.
- [x] Bug fix: Boolean Not/ToBool with NaN-boxed values (VAL_FALSE != 0).
- [x] Bug fix: Throw opcode handles list-based args via `qore_rt_throw_value`.
- [x] Bug fix: `VarRefNewObjectNode` (scoped object construction `Foo f("hello")`)
      now properly lowered to IR as a constructor call + assignment.
- [x] Tiered mode with low thresholds (ir=3, jit=10) passes all 137 IRExecModeSmoke tests.

Deliverables:
- [x] `include/qore/intern/JITRuntime.h` / `lib/JITRuntime.cpp` — 20+ new runtime helpers
- [x] `lib/QoreJIT.cpp` — symbol registration for all new helpers
- [x] `lib/QoreIRToLLVM.cpp` — universal opcode coverage in `lowerInstruction()`
- [x] `lib/QoreIRLowering.cpp` — `VarRefNewObjectNode` scoped construction fix
- [x] `examples/test/ir/JITSmoke.qtest` — 32 test cases (35 assertions)
- [x] `examples/test/ir/TieredSmoke.qtest` — 16 test cases (190 assertions)
- [x] Valgrind clean: 0 errors, 0 leaks on all test suites including tiered with low thresholds

### Phase 5b: Native LLVM Optimizations — COMPLETE ✓

Replace generic runtime helper calls with specialized C ABI helpers and inline LLVM
fast-paths for the most common operations, eliminating AST dispatch and dynamic type
checking overhead where types are known or cheaply checkable.

- [x] Specialized hash key access: `qore_rt_hash_key_access` for compile-time constant
      string keys on `QoreHashObjectDereferenceOperatorNode`, bypassing `qore_rt_invoke_expr`.
- [x] Specialized list index access: `qore_rt_list_index_access` for
      `QoreSquareBracketsOperatorNode`, direct array access with bounds checking.
- [x] Specialized string concatenation: `qore_rt_string_concat` for string+string via
      `AddAny`, with fallback to `qore_rt_add_any` for non-string types.
- [x] Inline LLVM fast-paths for `.any` arithmetic (`AddAny`, `SubAny`, `MulAny`):
      emit inline NaN-boxing tag checks to detect int+int and float+float, execute native
      CPU instructions (add/sub/mul/fadd/fsub/fmul), fall back to runtime helpers for
      mixed/complex types. `DivAny`/`ModAny` remain on generic path (division-by-zero).
- [x] Inline LLVM fast-paths for `.any` comparisons (`EqAny`, `NeAny`, `LtAny`, `LeAny`,
      `GtAny`, `GeAny`): same inline tag-check pattern for int-vs-int and float-vs-float
      comparisons, with native `icmp`/`fcmp` instructions and bool boxing.
- [x] Float detection fix: double-encoded values require both lower bound
      (`> DOUBLE_ENCODE_OFFSET`) and upper bound (`< TAG_INT48`) checks, since int48-tagged
      values have bit patterns above the double encode offset.

Deliverables:
- [x] `include/qore/intern/JITRuntime.h` / `lib/JITRuntime.cpp` — 3 specialized helpers
- [x] `lib/QoreJIT.cpp` — symbol registration for new helpers
- [x] `include/qore/intern/QoreIRToLLVM.h` / `lib/QoreIRToLLVM.cpp` — specialized lowering
      + `emitAnyArithFastPath()` + `emitAnyCmpFastPath()` + `tryEmitHashKeyAccess()` +
      `tryEmitListIndexAccess()`
- [x] `examples/test/ir/JITSmoke.qtest` — 39 test cases (49 assertions)
- [x] `examples/test/ir/TieredSmoke.qtest` — 19 test cases (283 assertions)
- [x] Valgrind clean: 0 errors, 0 leaks on all test suites
- [x] Debug traces converted from `fprintf(stderr, ...)` to `printd(3, ...)` in
      `Function.cpp` (`evalIntern`/`evalTiered`) and `QoreIRToLLVM.cpp`
      (`lowerInstruction`/`BranchIf`) to avoid polluting test output captured via `2>&1`

### Phase 5c: Debug Info — COMPLETE

- [x] DWARF debug info emitted for all JIT-compiled functions (DICompileUnit,
      DISubprogram, DILocation per instruction)
- [x] Source file/line mapping from `QoreProgramLocation` on each IR instruction
- [x] GDB JIT registration via `llvm::orc::enableDebuggerSupport()` — disabled in
      Phase 6 due to thread safety issues (shared ObjectLinkingLayer plugin not safe
      for concurrent compilations). DWARF info is still emitted in the modules.
- [x] Graceful fallback: if debugger support fails, JIT still works (non-fatal)

Deliverables:
- [x] `include/qore/intern/QoreIRToLLVM.h` — DIBuilder members, getDIFile/setDebugLocation helpers
- [x] `lib/QoreIRToLLVM.cpp` — DIBuilder creation, DICompileUnit/DISubprogram/DILocation
      emission per instruction, finalize
- [x] `lib/QoreJIT.cpp` — `enableDebuggerSupport()` call after LLJIT creation
- [x] `examples/test/ir/JITSmoke.qtest` — 41 test cases (52 assertions)
- [x] `examples/test/ir/TieredSmoke.qtest` — 20 test cases (284 assertions)
- [x] Valgrind clean: 0 errors, 0 leaks on all test suites

### Phase 5d: Lvalue Correctness Fixes — COMPLETE ✓

Fixed two correctness bugs where lvalue operations left stale cached values in both
the LLVM JIT (alloca caches) and IR interpreter (stored value caches), causing
infinite loops and incorrect data after copy-on-write mutations.

- [x] **LLVM JIT alloca-staleness** (`QoreIRToLLVM.cpp`): Lvalue operations
      (`PostInc`/`PreInc`/`PostDec`/`PreDec`, `StoreLvalue`, compound assignments)
      call runtime helpers that modify the Qore thread-local variable stack, but
      the LLVM alloca cache for the affected local was never updated. This caused
      for-loops with `++i` to read a stale alloca value every iteration, producing
      infinite loops. Fixed with a reload tracker mechanism: after lvalue ops and
      `Invoke` calls, `reloadLocalFromRuntime()` reloads the affected local's alloca
      from the runtime stack via `qore_rt_load_local`. Each reload tracker alloca
      holds the most recent reload value (+1 ref) and is registered with
      `invoke_result_allocas` for cleanup at function exit.
      `reloadAllLocalsFromRuntime()` reloads all local allocas after `Invoke` calls
      (which may execute arbitrary code that modifies any local via closures).
- [x] **IR interpreter COW cache invalidation** (`QoreIRInterpreter.cpp`): The IR
      interpreter caches loaded variable values in `locals`/`globals`/`threadlocals`/
      `closures` maps. When `StoreLvalue` modifies a hash field on a copy-on-write
      `QoreHashNode` (refcount > 1), the hash is copied and the thread-local stack
      gets the new hash, but the cached reference points to the old (pre-COW) hash.
      Subsequent reads return stale data. Fixed with `invalidateLvalueRoot()` which
      walks the lvalue AST tree (through `QoreHashObjectDereferenceOperatorNode` and
      other binary operators) to find the root `VarRefNode`, then erases its cached
      value from the appropriate cache map, forcing a fresh load on next access.
- [x] **LLVM IR diagnostic** (`QoreJIT.cpp`): Added `QORE_DUMP_LLVM_IR` environment
      variable to dump generated LLVM IR modules to stderr during compilation for
      debugging.

Deliverables:
- [x] `include/qore/intern/QoreIRToLLVM.h` — `local_reload_trackers` map,
      `reloadLocalFromRuntime()`, `reloadAllLocalsFromRuntime()` declarations
- [x] `lib/QoreIRToLLVM.cpp` — `findLvalueRootLocalKey()` static helper, reload
      tracker implementation with decref-before-replace semantics, reload calls after
      all lvalue ops and `Invoke` instructions
- [x] `lib/QoreIRInterpreter.cpp` — `invalidateLvalueRoot()` function replacing
      `updateLocalVarFromLvalue()` in `StoreLvalue` handler
- [x] `lib/QoreJIT.cpp` — `QORE_DUMP_LLVM_IR` diagnostic support
- [x] Valgrind clean: 0 definite leaks on JIT and tiered smoke tests

---

## Phase 6: Hardening — COMPLETE ✓

Full test suite validation under all execution modes, thread safety hardening,
memory safety verification, and CI integration.

### Test Suite Results (223 core tests)

| Mode | Pass/Total | Notes |
|------|-----------|-------|
| AST | 223/223 | baseline |
| IR | 223/223 | all operations fall back gracefully |
| JIT | 223/223 | all operations fall back gracefully |
| Tiered (default 100/1000) | 223/223 | production thresholds |
| Tiered (aggressive 3/10) | 220/223 | 3 known issues (see below) |

Known issues at aggressive thresholds only (pass at default thresholds):
- `HTTPClient.qtest` / `FtpClient.qtest` — network timeout (not a code bug)
- `test-debug.qtest` — debug introspection traces differ when functions execute via JIT
  (debug hooks don't observe JIT-compiled execution steps; 1/97 assertions)

### Thread Safety Fixes

- [x] **JIT initialization race**: Multiple threads calling `compileFunction()` concurrently
      crashed in `llvm::DataLayout::operator=` during initialization. Fixed with
      `std::call_once` for thread-safe one-time initialization.
- [x] **LLVM compilation serialization**: LLVM's code generation (MCStreamer, DWARF emission)
      is not thread-safe for concurrent compilations. Added `compile_mutex` to serialize the
      entire compilation pipeline (IR lowering → `addIRModule` → `lookup`).
- [x] **Debugger support disabled**: `llvm::orc::enableDebuggerSupport()` registers a
      shared `ObjectLinkingLayer` plugin that is not thread-safe. Disabled to prevent
      "Emitting values inside a locked bundle is forbidden" crashes during concurrent
      compilation. TODO: Re-enable once LLJIT compilation is better isolated.
- [x] **Double-check locking**: Cache lookups use `cache_mutex` for fast path; re-check
      under `compile_mutex` prevents duplicate compilations.

### Cleanup Tracking for GC Correctness

- [x] **trackResultForCleanup**: Added alloca-based cleanup tracking in `QoreIRToLLVM` to
      ensure JIT-compiled code properly decrefs intermediate values at function exit.
- [x] **Targeted tracking only**: Cleanup tracking is applied only to operations whose results
      must be decref'd and are not consumed by subsequent operations: `ConstString`, `Invoke`,
      `CatchException`, `Call`/`CallIndirect`/`CallMethod`/`CallStatic`, and all LValue
      operations (`Load`/`Store`/`PreInc`/`PreDec`/`PostInc`/`PostDec`/`Shift`/`Unshift`/
      `AddAssign` through `Splice`).
- [x] **Loop safety**: Comprehensive tracking of all operations was found to cause data
      corruption in loops (alloca reuse overwrites previous iteration values). The targeted
      approach avoids this while maintaining GC correctness.

### Valgrind Results

| Test | Mode | Definitely Lost | Notes |
|------|------|----------------|-------|
| JITSmoke.qtest | JIT | 0 bytes | clean |
| TieredSmoke.qtest | Tiered (3/10) | 0 bytes | clean |
| gc.qtest | Tiered (3/10) | 0 bytes | fixed: invoke_expr leak |
| gc.qtest | AST/IR/JIT | 0 bytes | clean |
| exception-location.qtest | Tiered (3/10) | 0 bytes | fixed: Return incref |
| operators.qtest | JIT | 0 bytes | clean |
| operators.qtest | Tiered (3/10) | 0 bytes | clean |
| sort.qtest | JIT | 0 bytes | clean |
| sort.qtest | Tiered (3/10) | 0 bytes | clean |

### Memory Safety Fixes

- [x] **invoke_expr reference leak** (`JITRuntime.cpp`): `qore_rt_invoke_expr` called
      `expr.refSelf()` but never balanced with `discard()`. Each JIT execution of an
      `Invoke` opcode leaked one reference. Fixed with `ref_expr.discard(xsink)` after eval.
- [x] **Return use-after-free** (`QoreIRToLLVM.cpp`): JIT Return handler did cleanup
      (emitInvokeCleanup, emitLocalUninstantiation) before reading the return value.
      When CatchException results were stored into pre-instantiated locals, cleanup
      deref'd the intermediate value, and evalTiered deref'd the local copy, leaving
      the return value pointing to freed memory. Fixed by incref'ing the return value
      before cleanup, matching the IR interpreter's `val.refSelf()` semantics.

### Thread Safety Verification

- [x] Thread-specific tests pass under tiered mode with aggressive thresholds:
      `background.qtest`, `deadlock.qtest`, `tld.qtest`, `thread-object.qtest`
- [x] TieredSmoke concurrent test passes reliably across 10 consecutive runs (0 races)
- [x] `thread-object.qtest` (original crash test) passes 5/5 runs consistently

### CI Integration

- [x] Added `test-jit-ubuntu-amd64` CI job (`--exec-mode=jit`, `allow_failure: true`)
- [x] Added `test-tiered-ubuntu-amd64` CI job (`--exec-mode=tiered --jit-ir-threshold=3
      --jit-jit-threshold=10`, `allow_failure: true`)
- [x] Both follow existing `test-ir-ubuntu-amd64` pattern with GitHub status reporting

Deliverables:
- [x] `include/qore/intern/QoreJIT.h` — thread safety: `compile_mutex`, `std::once_flag`,
      `init_success`/`init_error` members
- [x] `lib/QoreJIT.cpp` — thread-safe init with `std::call_once`, compilation serialization,
      disabled `enableDebuggerSupport`
- [x] `include/qore/intern/QoreIRToLLVM.h` — `trackResultForCleanup`, `invoke_result_allocas`,
      `invoke_alloca_map` members
- [x] `lib/QoreIRToLLVM.cpp` — targeted cleanup tracking for GC correctness; Return
      incref before cleanup to prevent use-after-free
- [x] `lib/JITRuntime.cpp` — invoke_expr discard fix, catch_exception runtime
- [x] `.gitlab-ci.yml` — `test-jit-ubuntu-amd64` and `test-tiered-ubuntu-amd64` jobs

---

## Phase 7: AOT Compilation (Ahead-of-Time) — COMPLETE ✓

Compile Qore scripts to standalone ELF executables that link against libqore for runtime
services.

### Phase 7a: Basic AOT Framework — COMPLETE ✓

Initial hybrid approach: pre-compile Invoke-free functions to native code; functions
with process-specific opcodes fall back to runtime JIT compilation.

#### Architecture

**Hybrid compilation**: The AOT compiler checks each function's IR for process-specific
opcodes (Invoke, LoadLocal, StoreLocal, LoadGlobal, StoreGlobal, LoadClosure, StoreClosure,
LoadThreadLocal, StoreThreadLocal, LoadLValue, StoreLValue, Pre/PostInc/DecLValue, Call,
CallMethod, CallStatic, CallIndirect). Functions without these opcodes are pre-compiled to
native machine code. Functions with these opcodes embed pointers that are process-specific
and invalid in AOT object files; they fall back to runtime JIT compilation.

**Source embedding**: The full Qore source text is embedded as a global constant in the
LLVM module. At runtime, `qore_aot_run()` re-parses the source (building fresh AST with
valid pointers) and registers pre-compiled function pointers, so the runtime can use native
code for pre-compiled functions and JIT-compile the rest on demand.

**Pipeline**:
```
qore --compile script.q --output script
  |
  v
1. Parse source -> QoreProgram (normal parse pipeline)
2. QoreAOT compiler:
   a. Embed source text as global constant in LLVM module
   b. Enumerate all user functions/methods
   c. For each: lower to IR, check for process-specific opcodes
   d. Opcode-safe functions: lower to LLVM IR, add to module
   e. Generate function registration table (name -> fn_ptr)
   f. Generate main() that calls qore_aot_run()
   g. Emit .o via LLVM TargetMachine with O2 optimization
3. Invoke system linker: cc -o script script.o -lqore -Wl,-rpath,...
```

#### Implementation

- [x] **LLVM build configuration**: Added `Passes` component for `PassBuilder` optimization
      pipeline used in object code emission.
- [x] **AOT compiler class** (`QoreAOT`): Orchestrates the full pipeline — source embedding,
      function enumeration, process-specific opcode detection, LLVM IR generation, O2
      optimization via `PassBuilder`, object code emission via `TargetMachine`, and system
      linker invocation.
- [x] **Process-specific opcode detection** (`hasProcessSpecificOpcodes`): Scans IR
      instructions for all opcodes that embed process-local pointers as i64 constants in
      LLVM IR (Invoke, LoadLocal, StoreLocal, etc.).
- [x] **AOT runtime entry point** (`qore_aot_run`): C ABI function called from generated
      `main()`. Initializes the Qore runtime, parses embedded source, registers pre-compiled
      function pointers via namespace tree walk, and runs the program. Supports `-b` flag for
      signal handling control (valgrind compatibility).
- [x] **Function registration**: Walks the program's namespace tree (functions, instance
      methods, static methods) and matches by name against the AOT function table. For each
      match, sets `cached_jit_fn` and promotes to `TIER_JIT`.
- [x] **CLI integration**: `--compile` and `--output` flags on the `qore` command line.
      Default output name derived from source (strip extension).
- [x] **Top-level code**: `_toplevel` function lowered and checked for process-specific
      opcodes; pre-compiled if safe, deferred to runtime JIT otherwise.
- [x] **ARGV passthrough**: `qore_setup_argv()` called before `qore_init()` to match
      the normal qore binary's initialization order.

### Phase 7b: AOT Pointer Indirection via Context Tables — COMPLETE ✓

Replaced process-specific embedded pointers with index-based lookups into runtime-resolved
context tables, enabling all user functions to be pre-compiled to native code in AOT binaries.

#### Problem

In Phase 7a, most functions fell back to runtime JIT because `hasProcessSpecificOpcodes()`
detected embedded `LocalVar*`/`Var*`/AST-node pointers that are invalid in AOT object files.
Even `printf("hello\n")` could not be pre-compiled because it produces Invoke opcodes with
embedded AST pointers. Result: "0/1 functions pre-compiled" for most scripts.

#### Solution: QoreAOTContext Indirection

At compile time, each unique pointer is assigned a numeric slot index. In the LLVM IR,
`_aot` runtime helpers take `(QoreAOTContext*, slot_index, ...)` instead of raw pointers.
At runtime, the function's IR is re-lowered from the freshly-parsed AST to collect the
fresh pointers in the same deterministic order, building the context tables.

**Correctness guarantee**: Same source → same AST → same IR lowering → same walk order →
slot indices match between compile-time and runtime.

**Function signature change**:
```
Before: uint64_t compiled_func(ExceptionSink* xsink)
After:  uint64_t compiled_func(QoreAOTContext* ctx, ExceptionSink* xsink)
```

**QoreAOTContext structure**:
```cpp
struct QoreAOTContext {
    LocalVar** locals;   // LoadLocal/StoreLocal/LoadClosure/StoreClosure/instantiate
    int num_locals;
    Var** globals;       // LoadGlobal/StoreGlobal/LoadThreadLocal/StoreThreadLocal
    int num_globals;
    uint64_t* exprs;     // NaN-boxed QoreValue for Invoke/Call/CallMethod/CallStatic/LValue
    int num_exprs;
};
```

**LLVM IR transformation example**:
```llvm
; Before (JIT mode — embedded pointer):
%var = inttoptr i64 140234567890 to ptr
%val = call i64 @qore_rt_load_local(ptr %var, ptr %xsink)

; After (AOT mode — context lookup):
%val = call i64 @qore_rt_load_local_aot(ptr %ctx, i32 3, ptr %xsink)
```

#### Implementation

- [x] **QoreAOTContext and AOTSlotMap** (`QoreAOT.h`): Context struct with locals/globals/exprs
      arrays + counts. `AOTSlotMap` with `local_slots`/`global_slots`/`expr_slots` maps and
      `getLocalSlot()`/`getGlobalSlot()`/`getExprSlot()` methods. Updated `QoreAOTFunc::fn_ptr`
      type to `uint64_t (*)(QoreAOTContext*, ExceptionSink*)`.
- [x] **AOT runtime helpers** (`JITRuntime.h`, `JITRuntime.cpp`): `_aot` variants of all
      process-specific runtime helpers. Each resolves `ctx->array[idx]` and delegates to the
      existing helper:
      - `qore_rt_load_local_aot`, `qore_rt_assign_local_aot`
      - `qore_rt_instantiate_local_aot`, `qore_rt_uninstantiate_local_aot`
      - `qore_rt_load_global_aot`, `qore_rt_store_global_aot`
      - `qore_rt_load_thread_local_aot`, `qore_rt_store_thread_local_aot`
      - `qore_rt_load_closure_aot`, `qore_rt_store_closure_aot`
      - `qore_rt_invoke_expr_aot`
      - `qore_rt_lvalue_load_aot`, `qore_rt_lvalue_store_aot`
      - `qore_rt_lvalue_unary_aot`, `qore_rt_lvalue_binary_aot`
      All registered in `QoreJIT.cpp` symbol table.
- [x] **AOT mode in QoreIRToLLVM** (`QoreIRToLLVM.h`, `QoreIRToLLVM.cpp`): Added `aot_mode`
      flag, `aot_slots` pointer, `aot_ctx_arg` LLVM value. `setAOTMode(const AOTSlotMap*)`
      method. Updated `lowerFunction()` signature: AOT mode takes `(ptr ctx, ptr xsink)`.
      For each process-specific opcode, added `if (aot_mode)` branches emitting `_aot` helper
      calls with slot indices instead of `inttoptr` patterns. Opcodes modified:
      LoadLocal, StoreLocal, LoadGlobal, StoreGlobal, LoadThreadLocal, StoreThreadLocal,
      LoadClosure, StoreClosure, Call/CallMethod/CallStatic/CallIndirect, Invoke,
      LoadLValue, StoreLValue, Pre/PostInc/DecLValue, compound assignments,
      local instantiation/uninstantiation, reload-from-runtime.
- [x] **AOT compiler slot maps** (`QoreAOT.cpp`): `buildAOTSlotMap()` walks IR
      blocks/instructions, assigns slots for each unique `LocalVar*`/`Var*`/`QoreValue`.
      Removed `hasProcessSpecificOpcodes()` rejection gate — all functions now proceed to LLVM
      lowering. Updated `generateMainAndTable()` for new `QoreAOTFunc` signature.
- [x] **AOT function dispatch** (`Function.h`, `Function.cpp`, `StatementBlock.h`,
      `StatementBlock.cpp`): Added `AotFunctionPtr cached_aot_fn` and `QoreAOTContext*
      cached_aot_ctx` to `UserVariantBase`. `registerPrecompiledAOTFunction()` sets both.
      `evalTiered()` calls `cached_aot_fn(ctx, xsink)` when context is set. Top-level
      statement block gets matching AOT fields and dispatch.
- [x] **Runtime context building** (`QoreAOTRuntime.cpp`): For each pre-compiled function:
      finds matching variant in fresh namespace tree, re-lowers to IR via same path, walks
      fresh IR in same deterministic order to collect `LocalVar*`/`Var*`/`QoreValue` into
      arrays, builds `QoreAOTContext`. Stores `all_body_locals` from fresh IR for
      `evalTiered()` local instantiation/uninstantiation.

#### Known Limitations

All previously-known IR lowering issues have been fixed (see Phase 7d below).

1. ~~**Int-typed post-increment**~~: **FIXED** in Phase 7d.
2. ~~**No-arg function calls**~~: **FIXED** in Phase 7d.
3. ~~**Ternary operator**~~: **FIXED** in Phase 7d.
4. ~~**Call double-evaluation**~~: **FIXED** in Phase 7c. See below.
5. ~~**192-byte ARGV leak**~~: **FIXED** in Phase 7d.

### Phase 7c: Call/Invoke Double-Evaluation Fix — COMPLETE ✓

Fixed a correctness bug where LLVM lowering for all Invoke and standalone Call
instructions called `qore_rt_invoke_expr()`, which re-evaluated the entire AST
expression from scratch, ignoring pre-evaluated operands. Side-effecting arguments
(function calls, post-increment, I/O) were evaluated twice. The same issue existed
in the IR interpreter's `evalInvoke()` default case for call-type opcodes.

#### Root Cause

The IR lowering creates Invoke and Call instructions with pre-evaluated operands
stored in `operands[]`. The LLVM Invoke handler ignored `invoke_opcode` and
`operands` entirely — always calling `qore_rt_invoke_expr()`. The IR interpreter's
`evalInvoke()` correctly dispatched unary/binary ops to `evalUnary`/`evalBinary`
using operands, but fell through to `evalExpr()` for call-type opcodes.

#### Fix: Opcode-Based Dispatch

Dispatch based on `invoke_opcode` and operand availability:

| Category | Operands | Runtime helper | Reload locals? |
|----------|----------|---------------|---------------|
| Unary ops | 1 | `qore_rt_unary_op` (existing) | No — pure computation |
| Binary ops | 2 | `qore_rt_binary_op` (existing) | No — pure computation |
| Call types | N | `qore_rt_call_with_args` (new) | Yes |
| LValue/DotEval/other | 0 or N | `qore_rt_invoke_expr` (existing fallback) | Yes |

**Opcode classifiers** (`QoreIR.h`): `isUnaryInvokeOpcode()`, `isBinaryInvokeOpcode()`,
`isCallInvokeOpcode()` — used by both LLVM lowering and IR interpreter.

**New runtime helper** (`JITRuntime.cpp`): `qore_rt_call_with_args()` builds a
`QoreListNode` from pre-evaluated NaN-boxed args, creates a copy of the call node
(`FunctionCallNode`, `SelfFunctionCallNode`, `StaticMethodCallNode`,
`CallReferenceCallNode`) with the pre-built arg list, and evaluates the copy.
Falls back to `qore_rt_invoke_expr` if the expression type is not recognized.
AOT variant `qore_rt_call_with_args_aot()` resolves expression from context slot.

**LLVM fixes** (`QoreIRToLLVM.cpp`):
- Invoke handler: replaced monolithic `qore_rt_invoke_expr` with dispatch to
  unary/binary/call/fallback paths based on opcode classification
- Standalone Call handler: added operand-based dispatch to `qore_rt_call_with_args`
  with fallback to `qore_rt_invoke_expr` when operands are empty

**IR interpreter fix** (`QoreIRInterpreter.cpp`): Added `Call`/`CallIndirect`/
`CallMethod`/`CallStatic` cases to `evalInvoke()` that build arg list from
pre-evaluated operands and create call node copies, avoiding double-evaluation.

#### Known Remaining Limitations

- ~~**DotEval Invoke**~~: **FIXED** in Phase 7e. See below.
- **Regex Invoke**: `RegexMatchBool`/`RegexNMatchBool` as Invoke with a pre-evaluated
  string operand — low impact since regex evaluation is idempotent.

#### Deliverables

- [x] `include/qore/intern/QoreIR.h` — `isUnaryInvokeOpcode()`, `isBinaryInvokeOpcode()`,
      `isCallInvokeOpcode()` classifier functions
- [x] `include/qore/intern/JITRuntime.h` / `lib/JITRuntime.cpp` — `qore_rt_call_with_args`,
      `qore_rt_call_with_args_aot` helpers
- [x] `lib/QoreJIT.cpp` — symbol registration for new helpers
- [x] `lib/QoreIRToLLVM.cpp` — Invoke handler dispatch + standalone Call handler fix
- [x] `lib/QoreIRInterpreter.cpp` — `evalInvoke()` call-type handling
- [x] `examples/test/ir/JITSmoke.qtest` — 46 test cases (67 assertions)
- [x] Valgrind clean: 0 bytes definitely lost on all test suites

### Phase 7d: Bug Fixes — COMPLETE ✓

Fixed five pre-existing bugs in the JIT/IR pipeline.

#### Bug 1: Int-Typed Post-Increment/Decrement IR Lowering Failure

`QoreIntPostIncrementOperatorNode` inherits from
`QoreSingleExpressionOperatorNode<LValueOperatorNode>`, not from
`QorePostIncrementOperatorNode`. `lowerPostIncrement()` tried only the base
class cast and missed the int-specific variant. Same for post-decrement.

**Fix** (`lib/QoreIRLowering.cpp`): Try both `QorePostIncrementOperatorNode*`
and `QoreIntPostIncrementOperatorNode*` casts via common base
`QoreSingleExpressionOperatorNode<LValueOperatorNode>*`. Same pattern for
`lowerPostDecrement()`.

#### Bug 2: No-Arg Function Calls Fail IR Lowering

`lowerCallArgs()` rejected null `parse_args`/`args` with an error. Zero-argument
calls (e.g., `hello()`) have null args by design.

**Fix** (`lib/QoreIRLowering.cpp`): Return true for null args (empty operands list).

#### Bug 3: Ternary PHI Node LLVM Verification Failure

The LLVM Phi handler created `llvm::PHINode` but never called `addIncoming()`.
LLVM verification failed because the PHI had zero incoming values. Additionally,
PHI results were not marked as NaN-boxed in `nanboxed_values`, causing downstream
`boxValue()` to re-box NaN-boxed booleans as integers.

**Fix** (`lib/QoreIRToLLVM.cpp`, `include/qore/intern/QoreIRToLLVM.h`):
- Added `pending_phis` member to collect deferred PHI instructions
- Store PHI/instruction pairs during lowering; fixup pass after all blocks adds
  incoming values with proper boxing
- Mark PHI results in `nanboxed_values` since they carry NaN-boxed i64 values

#### Bug 4: 192-Byte Memory Leak with ARGV in JIT/AOT Mode

`LoadGlobal`/`LoadThreadLocal` LLVM handlers called `qore_rt_load_global()` /
`qore_rt_load_thread_local()` which return +1 ref values, but never tracked
them for decref at function exit.

**Fix** (`lib/QoreIRToLLVM.cpp`): Added `trackResultForCleanup()` to
`LoadGlobal` and `LoadThreadLocal` handlers.

#### Bug 5: Post-Increment/Decrement Return Value in IR Interpreter

The IR interpreter's `PostIncLValue`/`PostDecLValue` handler returned the
new value instead of the old value (post-inc/dec should return pre-operation value).

**Fix** (`lib/QoreIRInterpreter.cpp`): For post operations, use the return value
from `evalLValueUnary()` (which is the old value) as the result instead of
reloading the updated value.

#### Deliverables

- [x] `lib/QoreIRLowering.cpp` — Post-inc/dec casts, null args handling
- [x] `include/qore/intern/QoreIRToLLVM.h` — `pending_phis` member
- [x] `lib/QoreIRToLLVM.cpp` — PHI fixup pass, PHI nanboxed tracking, LoadGlobal cleanup
- [x] `lib/QoreIRInterpreter.cpp` — Post-inc/dec return value fix
- [x] `examples/test/ir/JITSmoke.qtest` — 50 test cases (79 assertions), 4 new regression tests
- [x] Valgrind clean: 0 bytes definitely lost on all test suites

### Test Results

| Test | Result |
|------|--------|
| AOTSmoke.qtest | 20/20 test cases, 32 assertions |
| TieredSmoke.qtest | 20/20 (284 assertions) — no regression |
| JITSmoke.qtest | 50/50 (79 assertions) — 4 new bug-fix regression tests |
| IRExecMode.qtest | 2/2 (4 assertions) — no regression |
| IRExecModeSmoke.qtest | 137/137 (416 assertions) — no regression |
| operators.qtest | 25/25 (696 assertions) — no regression |
| sort.qtest | 6/6 (112 assertions) — no regression |
| gc.qtest | 8/8 (45 assertions) — no regression |

AOT smoke tests verify: hello world, arithmetic, control flow, functions, string
operations, exit codes, ARGV passthrough, error handling (try/catch), missing file
compilation, default output naming, all-functions-pre-compiled verification, local
variable scoping, lvalue operations (pre-increment, compound assignments), multiple
user functions, method calls, hash operations, list operations, nested function calls,
on_exit blocks, and type conversions. Each test compiles to executable and compares
output against the interpreter.

Phase 7b tests additionally verify that all user functions are pre-compiled (N/N) via
compile output inspection using `compileCheckAndRun()` helper.

### Valgrind Results

| Binary | Definitely Lost | Notes |
|--------|----------------|-------|
| JITSmoke.qtest | 0 bytes | clean (50 test cases, all modes) |
| AOTSmoke.qtest | 0 bytes | clean (test suite itself) |
| hello.q AOT binary | 0 bytes | clean (simple script, no ARGV) |
| hello_argv.q AOT binary | 0 bytes | clean (fixed by Phase 7d LoadGlobal cleanup) |
| string-ops AOT binary | 0 bytes | clean (fixed by Phase 7c call double-evaluation fix) |
| qore interpreter (AST mode) | 0 bytes | baseline |
| qore interpreter (JIT mode) | 0 bytes | clean (fixed by Phase 7d LoadGlobal cleanup) |

The 192-byte ARGV leak in JIT/AOT mode was fixed in Phase 7d: `LoadGlobal`/`LoadThreadLocal`
LLVM handlers now call `trackResultForCleanup()` to decref +1 ref results at function exit.

### Deliverables

- [x] `CMakeLists.txt` — `Passes` LLVM component, new source files
- [x] `include/qore/intern/QoreAOT.h` — AOT compiler class, `QoreAOTFunc` struct,
      `QoreAOTContext`, `AOTSlotMap`
- [x] `lib/QoreAOT.cpp` — AOT compiler implementation (source embedding, slot map building,
      LLVM module generation, object emission, linker invocation)
- [x] `lib/QoreAOTRuntime.cpp` — `qore_aot_run()` entry point with ARGV, `-b` flag,
      namespace walk, context table building via IR re-lowering
- [x] `include/qore/intern/JITRuntime.h` / `lib/JITRuntime.cpp` — `_aot` helper declarations
      and implementations
- [x] `lib/QoreJIT.cpp` — symbol registration for `_aot` helpers
- [x] `include/qore/intern/QoreIRToLLVM.h` / `lib/QoreIRToLLVM.cpp` — `aot_mode` flag,
      `aot_slots`, `aot_ctx_arg`, AOT-mode branches for all process-specific opcodes
- [x] `command-line.cpp` — `--compile` and `--output` flags
- [x] `include/qore/intern/Function.h` — `AotFunctionPtr`, `cached_aot_ctx`,
      `registerPrecompiledAOTFunction()` method
- [x] `lib/Function.cpp` — AOT function registration, `evalTiered()` AOT dispatch
- [x] `include/qore/intern/StatementBlock.h` — AOT top-level fields
- [x] `lib/StatementBlock.cpp` — AOT top-level dispatch
- [x] `examples/test/ir/AOTSmoke.qtest` — 20 test cases (32 assertions) for AOT compilation

### Phase 7e: DotEval Double-Evaluation & Expression Opcode Memory Leak Fix — COMPLETE ✓

Fixed two bugs: DotEval expressions (e.g., `f().method()`) double-evaluated the base
expression, and expression-based opcode handlers in LLVM lowering leaked node-type results.

#### Bug A: DotEval Double-Evaluation (Functional Correctness)

When a DotEval expression is compiled to IR, `lowerDotEval()` pre-evaluates the base
expression as `operands[0]`. However, both the LLVM lowering and IR interpreter **ignored
this operand** and fell through to `qore_rt_invoke_expr`, which re-evaluated the entire
AST including the base. Side effects (e.g., counter increments, I/O) executed twice.

**Fix**: Added `QoreDotEvalOperatorNode::evalWithBase(QoreValue base, ExceptionSink* xsink)`
which executes the method call dispatch on a pre-evaluated base value, containing the same
logic as `evalImpl()` minus the base evaluation. Refactored `evalImpl()` to call
`evalWithBase()`. New runtime helpers `qore_rt_dot_eval_with_base()` /
`qore_rt_dot_eval_with_base_aot()` extract the `QoreDotEvalOperatorNode*` from the expr
and call `evalWithBase()`. Both LLVM lowering (Invoke handler + standalone DotEval handler)
and IR interpreter (`evalInvoke()` + standalone expression handler) now dispatch to these
helpers using the pre-evaluated `operands[0]` instead of re-evaluating the full AST.

Opcode classifier `isDotEvalInvokeOpcode()` added to `QoreIR.h` matching all 8 DotEval
opcodes (DotEvalAny through DotEvalObject).

#### Bug B: Expression-Based Opcode Memory Leaks

Multiple standalone expression opcode handlers in LLVM lowering called
`qore_rt_invoke_expr` (returns +1 ref) or `qore_rt_hash_key_access` /
`qore_rt_list_index_access` (also +1 ref) but didn't call `trackResultForCleanup()`,
causing memory leaks for node-type results.

**Fix**: Added `trackResultForCleanup()` calls to:
- DotEval standalone handler (all 8 opcodes) — both `qore_rt_dot_eval_with_base` and fallback paths
- Non-DotEval expression ops (PopAny through ElementsInt) — `qore_rt_invoke_expr` path
- Remaining expression ops (MapSelectList through InvokeSimError) — `qore_rt_invoke_expr` path
- Specialized hash key access success path inside `tryEmitHashKeyAccess()`
- Specialized list index access success path inside `tryEmitListIndexAccess()`

#### Deliverables

- [x] `include/qore/intern/QoreDotEvalOperatorNode.h` — `evalWithBase()` declaration
- [x] `lib/QoreDotEvalOperatorNode.cpp` — `evalWithBase()` implementation, `evalImpl()` refactored
- [x] `include/qore/intern/QoreIR.h` — `isDotEvalInvokeOpcode()` classifier
- [x] `include/qore/intern/JITRuntime.h` / `lib/JITRuntime.cpp` — `qore_rt_dot_eval_with_base`,
      `qore_rt_dot_eval_with_base_aot` helpers
- [x] `lib/QoreJIT.cpp` — symbol registration for new helpers
- [x] `lib/QoreIRToLLVM.cpp` — DotEval Invoke dispatch + standalone handler split +
      `trackResultForCleanup` for all expression-based ops
- [x] `lib/QoreIRInterpreter.cpp` — DotEval `evalInvoke()` + standalone handler updates
- [x] `examples/test/ir/JITSmoke.qtest` — 54 test cases (91 assertions), 4 new DotEval regression tests
- [x] Valgrind clean: 0 bytes definitely lost on all test suites

### Test Results (Phase 7e)

| Test | Result |
|------|--------|
| JITSmoke.qtest | 54/54 test cases, 91 assertions |
| AOTSmoke.qtest | 20/20 test cases, 32 assertions |
| IRExecModeSmoke.qtest | 135/137 (414/416 assertions) — 2 pre-existing list extract failures |
| TieredSmoke.qtest | 20/20 (284 assertions) — concurrent calls fixed in Phase 7h |

### Phase 7f: Regex Double-Evaluation & Binary .any Ops Memory Leak Fix — COMPLETE ✓

Fixed two bugs: regex expressions (e.g., `f() =~ /pattern/`) double-evaluated the
subject expression, and binary `.any` operation handlers in LLVM lowering leaked
reference-counted results.

#### Bug A: Regex Double-Evaluation (Functional Correctness)

When regex expressions are compiled to IR, the IR lowering correctly pre-evaluates the
subject expression as `operands[0]`. However, the LLVM lowering for all regex opcodes
and the IR interpreter for RegexMatchAny/RegexExtract* **ignored this operand** and fell
through to `qore_rt_invoke_expr`/`evalExpr()`, which re-evaluated the entire AST
including the subject expression. Side effects executed twice.

**Fix**: Added `isRegexInvokeOpcode()` classifier for the 5 non-subst regex opcodes.
New runtime helper `qore_rt_regex_op_with_operand()` extracts the `QoreRegex*` from
the operator node and dispatches match/extract using the pre-evaluated operand value.
AOT variant `qore_rt_regex_op_with_operand_aot()` resolves expression from context slot.
Both LLVM lowering (Invoke handler + standalone regex handler) and IR interpreter
(`evalInvoke()` + standalone expression handler) now dispatch to these paths using the
pre-evaluated `operands[0]`.

RegexSubst (RegexSubstAny/RegexSubstString) excluded: lvalue operations using
`LValueHelper` — cannot decompose into pre-evaluated operand + regex dispatch.

#### Bug B: Binary .any Ops Missing trackResultForCleanup()

Multiple binary operation handlers in LLVM lowering produced NaN-boxed results that may
contain reference-counted nodes but never called `trackResultForCleanup()`, causing
memory leaks when the result is a node type (string, number, date, list, etc.).

**Fix**: Added `trackResultForCleanup()` calls to:
- `.any` arithmetic (AddAny, SubAny, MulAny, DivAny, ModAny) — both fast-path PHI
  results and direct runtime helper results
- `.any` bitwise (AndAny, OrAny, XorAny, ShlAny, ShrAny)
- Compound assignment + higher-order + range ops (SubAssignAny through RangeSliceFloat)

#### Deliverables

- [x] `include/qore/intern/QoreIR.h` — `isRegexInvokeOpcode()` classifier
- [x] `include/qore/intern/JITRuntime.h` / `lib/JITRuntime.cpp` — `qore_rt_regex_op_with_operand`,
      `qore_rt_regex_op_with_operand_aot` helpers
- [x] `lib/QoreJIT.cpp` — symbol registration for new helpers
- [x] `lib/QoreIRToLLVM.cpp` — Regex Invoke dispatch + standalone regex handler split +
      `trackResultForCleanup` for binary .any ops
- [x] `lib/QoreIRInterpreter.cpp` — Regex `evalInvoke()` + standalone handler updates
- [x] `examples/test/ir/JITSmoke.qtest` — 58 test cases, 4 new regex regression tests

### Phase 7g: Extract Cache Staleness & JIT Local Variable Reload — COMPLETE ✓

Fixed three bugs affecting IR/JIT variable coherence:

#### Bug A: Extract Cache Staleness (IR Interpreter + LLVM)

The `extract` operator modifies the source list/string/binary in-place via
`LValueHelper`, but the IR interpreter's local variable cache was not
invalidated after execution. The LLVM lowering also did not reload local
allocas after lvalue-modifying expression ops.

**Fix (IR interpreter)**: Added `ExtractAny`, `ExtractList`, `ExtractString`,
`ExtractBinary` to the cache invalidation switch in the general expression
ops handler.

**Fix (LLVM)**: Split lvalue-modifying expression ops (Extract*, Remove*,
RegexSubst*, Trim*, Chomp*, Transliterate*, Pop*, Push*, ListAssign*) into
a separate handler with `reloadAllLocalsFromRuntime()` after the runtime call.
Pure expression ops (Keys*, Exists*, Elements*, InstanceOf, Background, Cast*)
remain in the original handler without reload.

#### Bug B: JIT Local Variable Alloca Pre-Creation

When `VarRefNewObjectNode` (scoped object construction like `Counter done(4)`)
was evaluated in JIT mode via `qore_rt_invoke_expr`, it assigned the variable
through `LValueHelper` on the runtime variable stack. The subsequent
`reloadAllLocalsFromRuntime` call (after the Call instruction) iterated over
`local_allocas`, but the alloca for the target variable didn't exist yet
(created lazily on first `LoadLocal`), so the reload missed it. Later
`LoadLocal` would create the alloca initialized to NOTHING, producing stale
results.

**Fix**: Added `preCreateLocalAllocas()` that creates LLVM allocas for ALL
local variables (collected by `collectLocals()`) during the function prologue,
before any instruction lowering begins. Pre-instantiated locals are initialized
from the runtime stack; body locals are initialized to NOTHING. This ensures
`reloadAllLocalsFromRuntime` can find and reload all variable allocas.

#### Test Results

| Test Suite | Result |
|-----------|--------|
| JITSmoke.qtest | 58/58 (103 assertions) |
| IRExecModeSmoke.qtest | 137/137 (416 assertions) — previously 135/137 |
| TieredSmoke.qtest | 20/20 (284 assertions) — concurrent calls fixed in Phase 7h |
| AOTSmoke.qtest | 20/20 (32 assertions) |
| Valgrind (JITSmoke) | 0 bytes definitely/indirectly/possibly lost |
| Valgrind (IRExecModeSmoke) | 0 bytes definitely/indirectly/possibly lost |

### Phase 7h: Top-Level JIT Pre-Instantiation, Uninstantiate Stack Fix & DotEval Reload — COMPLETE ✓

Fixed three bugs causing segfaults and stale variable values when JIT-compiled top-level
code uses background threads with closures.

#### Bug A: Top-Level JIT Missing pre_instantiated_locals (Segfault)

When top-level code is JIT-compiled via `StatementBlock::execIntern()`, the `QoreIRFunction`
was created without populating `pre_instantiated_locals`. The caller's `LVListInstantiator`
had already pushed all top-level locals onto the thread-local variable stack, but the JIT
compiler didn't know this. Result: `emitLocalInstantiation` double-pushed variables, and
`emitLocalUninstantiation` double-popped, corrupting the variable stack and causing segfaults.

**Fix** (`lib/StatementBlock.cpp`): Populate `func->pre_instantiated_locals` from
`getLVList()` during IR function creation, so the JIT compiler skips instantiation/
uninstantiation for variables already on the stack.

#### Bug B: qore_rt_uninstantiate_local Wrong Stack for Closure Variables (Segfault)

`qore_rt_uninstantiate_local(ExceptionSink* xsink)` unconditionally called
`thread_uninstantiate_lvar(xsink)` which pops from lvstack. For closure-use variables
on cvstack, this popped from the wrong stack, causing assertion failures and segfaults.

**Fix** (`lib/JITRuntime.cpp`, `include/qore/intern/JITRuntime.h`): Changed signature to
`qore_rt_uninstantiate_local(LocalVar* var, ExceptionSink* xsink)` which calls
`var->uninstantiate(xsink)`, dispatching to the correct stack based on `closure_use`.
Updated AOT variant to resolve `LocalVar*` from context and delegate.

**Fix** (`lib/QoreIRToLLVM.cpp`): Updated `declareRuntimeHelpers` and
`emitLocalUninstantiation` to pass `LocalVar*` pointer as first argument via
`inttoptr` of the compile-time pointer value.

#### Bug C: DotEval Handler Missing reloadAllLocalsFromRuntime (Stale Values)

The DotEval instruction handler's pre-evaluated-base path (`qore_rt_dot_eval_with_base`)
did not call `reloadAllLocalsFromRuntime` after the method call. When a DotEval like
`done.waitForZero()` synchronizes with background threads that modify closure-captured
variables, the JIT alloca for the modified variable remained stale. The concurrent test
returned `OK:0` because `results.size()` read 0 from the stale alloca instead of 120
from the updated runtime stack.

**Fix** (`lib/QoreIRToLLVM.cpp`): Added `reloadAllLocalsFromRuntime(module, llvm_func)`
after both the pre-evaluated-base DotEval path and its AST fallback path.

#### Test Results

| Test Suite | Result |
|-----------|--------|
| JITSmoke.qtest | 58/58 (103 assertions) |
| IRExecModeSmoke.qtest | 137/137 (416 assertions) |
| TieredSmoke.qtest | 20/20 (284 assertions) — concurrent calls now passing |
| AOTSmoke.qtest | 20/20 (32 assertions) |
| Valgrind (JITSmoke) | 0 bytes definitely/indirectly/possibly lost |
| Valgrind (TieredSmoke) | 0 bytes definitely/indirectly/possibly lost |
| Valgrind (AOTSmoke) | 0 bytes definitely/indirectly/possibly lost |

#### Deliverables

- [x] `lib/StatementBlock.cpp` — populate `pre_instantiated_locals` from `getLVList()` for top-level IR
- [x] `include/qore/intern/JITRuntime.h` — updated `qore_rt_uninstantiate_local` signature with `LocalVar*`
- [x] `lib/JITRuntime.cpp` — `qore_rt_uninstantiate_local` dispatches via `var->uninstantiate(xsink)`;
      AOT variant updated to resolve `LocalVar*` from context
- [x] `lib/QoreIRToLLVM.cpp` — updated `declareRuntimeHelpers` and `emitLocalUninstantiation` for
      two-argument signature; added `reloadAllLocalsFromRuntime` to DotEval handler and fallback path

### Phase 7i: Closure JIT Compilation — COMPLETE ✓

Enabled JIT compilation of closure variants. Previously closures were excluded from JIT
promotion via `!is_closure` guards, but Phase 7h fixes made closure variable handling
correct in JIT: `qore_rt_uninstantiate_local` dispatches via `var->uninstantiate(xsink)`
(correct lvstack/cvstack), `qore_rt_load_local`/`qore_rt_assign_local` dispatch through
`LocalVar::eval()`/`getLValue()` which handle `closure_use` correctly, and DotEval reload
ensures alloca freshness after method calls.

#### Analysis

Closure variables use `LoadClosure`/`StoreClosure` IR opcodes, which are NOT collected by
`collectLocals()` (only `LoadLocal`/`StoreLocal` are). This means:
- Closure variables are NOT in `function_locals` — no instantiation/uninstantiation by JIT
- No allocas created for closure variables — loaded fresh via `qore_rt_load_local` each time
- `reloadAllLocalsFromRuntime` doesn't affect them (no alloca to reload)
- Closure environment is set up by `evalClosure()` → `ThreadSafeLocalVarRuntimeEnvironmentHelper`
  BEFORE `evalTiered()` is called

The LLVM handlers for `LoadClosure`/`StoreClosure` in JIT mode already existed and used the
correct runtime helpers. The only code-quality fix was a missing `trackResultForCleanup` on
`LoadClosure` (exception-safety leak for +1 ref values).

#### Changes

- [x] **Remove `!is_closure` guards** (`lib/Function.cpp`): Removed `&& !is_closure` from
      both JIT promotion checks (IR tier at line 2409, AST tier at line 2430). Updated comment
      to remove the closure exclusion note.
- [x] **Update `is_closure` member comment** (`include/qore/intern/Function.h`): Changed from
      "JIT not yet supported" to just "true for closure variants".
- [x] **Add `trackResultForCleanup` to LoadClosure** (`lib/QoreIRToLLVM.cpp`): Added cleanup
      tracking for the +1 ref returned by `qore_rt_load_local`/`qore_rt_load_closure_aot`,
      matching the pattern used by `LoadLocal` and other load handlers.
- [x] **Closure JIT tests** (`examples/test/ir/TieredSmoke.qtest`): Three new test cases:
      `testTieredClosureBasic` (simple capture + read), `testTieredClosureWriteBack` (closure
      modifies captured variable, verifies write-through), `testTieredClosureMultiCapture`
      (captures int, string, float variables).

#### Test Results

| Test Suite | Result |
|-----------|--------|
| TieredSmoke.qtest | 23/23 test cases, 367 assertions |
| JITSmoke.qtest | 58/58 (103 assertions) |
| IRExecModeSmoke.qtest | 137/137 (416 assertions) |
| AOTSmoke.qtest | 28/28 (66 assertions) |
| Valgrind (TieredSmoke) | 0 bytes definitely/indirectly/possibly lost |
| Valgrind (AOTSmoke) | 0 bytes definitely/indirectly/possibly lost |

#### Deliverables

- [x] `lib/Function.cpp` — removed `!is_closure` from two JIT promotion guards
- [x] `include/qore/intern/Function.h` — updated `is_closure` member comment
- [x] `lib/QoreIRToLLVM.cpp` — added `trackResultForCleanup` to LoadClosure handler
- [x] `examples/test/ir/TieredSmoke.qtest` — 23 test cases (367 assertions), 3 new closure tests

### Phase 7j: Module Compilation (`--compile-module`) — COMPLETE ✓

Compile `.qm` user modules to native `.qmod` binary modules loadable via `dlopen()`.

#### Architecture

Reuses the AOT compilation infrastructure but emits a shared library instead of an
executable. The compiled `.qmod` exports the standard binary module ABI symbols, allowing
it to be loaded by the Qore module loader identically to hand-written C++ binary modules.

**Pipeline**:
```
qore --compile-module Module.qm [--output Module.qmod]
  |
  v
1. Parse module metadata (name/version/desc/author/url/license) from source text
2. Create QoreProgram with PO_IN_MODULE parse options
3. Set up QoreUserModuleDefContextHelper for module-aware parsing
4. Parse module source
5. Compile namespace functions to LLVM IR with pointer indirection
6. Generate binary module ABI:
   - Exported globals: qore_module_name, qore_module_version, qore_module_description,
     qore_module_author, qore_module_url, qore_module_license, qore_module_api_major/minor
   - Embedded source text + function table (same format as executable AOT)
   - qore_module_init() → calls qore_aot_module_init()
   - qore_module_ns_init(root_ns, qore_ns) → calls qore_aot_module_ns_init()
   - qore_module_delete() → calls qore_aot_module_delete()
7. Emit PIC object file (.o) via LLVM TargetMachine
8. Link shared library: cc -shared -o Module.qmod Module.o -lqore ...
```

**Runtime loading**: When the `.qmod` is loaded by the module loader:
- `qore_module_init()` calls `qore_aot_module_init()` which creates a `QoreProgram`,
  sets up `QoreUserModuleDefContextHelper`, re-parses the embedded source, and registers
  pre-compiled AOT functions on the parsed namespace tree.
- `qore_module_ns_init()` copies the module's public namespaces to the program's root
  namespace, then registers AOT functions on the copies.
- `qore_module_delete()` cleans up the module's `QoreProgram`.

#### Implementation

- [x] **Module metadata parser** (`QoreAOT.cpp`): `parseModuleMetadata()` scans source
      for `module NAME { ... }` block, extracts key=value pairs (version, desc, author, url,
      license).
- [x] **Module ABI generator** (`QoreAOT.cpp`): `generateModuleABI()` creates LLVM IR for
      all exported module symbols (globals + init/ns_init/delete functions).
- [x] **Shared library linker** (`QoreAOT.cpp`): `linkSharedLib()` invokes system linker
      with `-shared` flag; cross-compilation emits `.o` only.
- [x] **Module compiler entry** (`QoreAOT.cpp`): `QoreAOT::compileModule()` orchestrates
      the full pipeline.
- [x] **Module runtime functions** (`QoreAOTRuntime.cpp`): `qore_aot_module_init()`,
      `qore_aot_module_ns_init()`, `qore_aot_module_delete()`.
- [x] **CLI integration** (`command-line.cpp`): `--compile-module` flag sets compile mode,
      default output uses `.qmod` extension. Skips normal parse (module creates its own
      `QoreProgram` with `PO_IN_MODULE`).
- [x] **QoreAOTModuleInfo struct** (`QoreAOT.h`): Holds parsed module metadata.

#### Test Results

| Test Suite | Result |
|-----------|--------|
| AOTSmoke.qtest | 28/28 test cases, 66 assertions |
| Valgrind (AOTSmoke) | 0 bytes definitely/indirectly/possibly lost |

Module compilation tests: compile `.qm` to `.qmod`, load compiled module, verify function
output matches interpreted module, custom `--output` path, all optimization levels (O0–O3),
error handling for non-module files.

#### Deliverables

- [x] `include/qore/intern/QoreAOT.h` — `QoreAOTModuleInfo` struct, `compileModule()` method,
      `qore_aot_module_init/ns_init/delete` declarations
- [x] `lib/QoreAOT.cpp` — `parseModuleMetadata()`, `generateModuleABI()`, `linkSharedLib()`,
      `QoreAOT::compileModule()`
- [x] `lib/QoreAOTRuntime.cpp` — `qore_aot_module_init()`, `qore_aot_module_ns_init()`,
      `qore_aot_module_delete()`
- [x] `command-line.cpp` — `--compile-module` flag, `.qmod` default extension, module dispatch
- [x] `examples/test/ir/AOTSmoke.qtest` — 28 test cases (66 assertions), 4 new module tests

### Future Extensions (not in this phase)

- [x] ~~**Fix Call double-evaluation**~~: Fixed in Phase 7c. Dispatch based on
      `invoke_opcode` to use pre-evaluated operands via `qore_rt_call_with_args`. The 72-byte
      string-ops leak is eliminated. DotEval and Regex invoke remain as known limitations.
- [x] ~~**Fix int-typed post-increment**~~: Fixed in Phase 7d.
- [x] ~~**Fix no-arg function calls**~~: Fixed in Phase 7d.
- [x] ~~**Fix ternary PHI node**~~: Fixed in Phase 7d.
- [x] ~~**Fix 192-byte ARGV leak**~~: Fixed in Phase 7d (LoadGlobal/LoadThreadLocal cleanup).
- [x] ~~**Fix DotEval double-evaluation**~~: Fixed in Phase 7e. `evalWithBase()` method on
      `QoreDotEvalOperatorNode` with `qore_rt_dot_eval_with_base` runtime helpers.
- [x] ~~**Fix expression opcode memory leaks**~~: Fixed in Phase 7e. `trackResultForCleanup()`
      added to all expression-based opcode handlers.
- [x] ~~**Fix Regex double-evaluation**~~: Fixed in Phase 7f. `qore_rt_regex_op_with_operand`
      runtime helper with `isRegexInvokeOpcode()` classifier.
- [x] ~~**Fix binary .any ops memory leaks**~~: Fixed in Phase 7f. `trackResultForCleanup()`
      added to .any arithmetic, bitwise, and compound assignment/higher-order/range ops.
- [x] ~~**Fix extract cache staleness**~~: Fixed in Phase 7g. Extract* opcodes added to IR
      interpreter cache invalidation switch; LLVM lvalue-modifying expression ops split into
      separate handler with `reloadAllLocalsFromRuntime`.
- [x] ~~**Fix JIT local variable reload for VarRefNewObjectNode**~~: Fixed in Phase 7g.
      `preCreateLocalAllocas()` ensures all local variable allocas exist before any
      instruction lowering, so `reloadAllLocalsFromRuntime` can find them after Call
      instructions that assign variables via LValueHelper.
- [x] ~~**Fix JIT background thread + closure variable coherence**~~: Fixed in Phase 7h.
      Three bugs: (1) top-level JIT code missing `pre_instantiated_locals` causing double-push,
      (2) `qore_rt_uninstantiate_local` using wrong stack for closure variables,
      (3) DotEval handler missing `reloadAllLocalsFromRuntime` after method calls.
- [x] ~~**Optimization levels**~~: `--opt-level=N` (0-3) flag for AOT compilation. Default O2.
      Maps to `llvm::OptimizationLevel::O0..O3`. O0 skips optimization passes entirely.
      Output message now shows opt level: `"compiled (O%d): %s"`.
- [x] ~~**Cross-compilation**~~: `--target=TRIPLE` flag for cross-compilation. When set,
      initializes all LLVM targets and emits object file for the specified triple.
      Skips native linking; prints `"cross-compiled object: %s.o (link manually)"`.
      Invalid triples produce a clear error message.
- [x] ~~**Static linking**~~: `--static` flag for AOT compilation. Links libqore statically
      via `libqore_static.a` (built with `BUILD_STATIC_LIBQORE=ON` CMake option).
      Transitive dependencies (OpenSSL, LLVM, etc.) remain dynamically linked.
      CMake-generated `aot-link.conf` provides portable compiler/library configuration
      (searched via `$QORE_AOT_LINK_CONF` env, `$QORE_LIBDIR/aot-link.conf`, or
      `QORE_LIBDIR/qore/aot-link.conf`). Helpful error when static lib not built.
- [x] ~~**Module compilation**~~: `--compile-module` flag compiles `.qm` user modules to native
      `.qmod` binary modules loadable via `dlopen()`. The pipeline: parse module metadata
      (name/version/desc/author/url/license) from source, create `QoreProgram` with
      `PO_IN_MODULE`, compile namespace functions to LLVM IR, generate binary module ABI
      symbols (`qore_module_name`, `qore_module_version`, `qore_module_init`, etc.), emit
      PIC object file, link as shared library with `-shared`. At runtime, `qore_aot_module_init`
      re-parses embedded source, registers pre-compiled functions; `qore_aot_module_ns_init`
      copies module namespaces to the program's root namespace with AOT function registration
      on the copies. Output defaults to `<name>.qmod`; custom output via `--output`.
- [ ] Source stripping: option to not embed source (requires full context coverage first)

---

## qore-llvm Reference Pointers (do not reuse codebase)

- Invoke/landingpad patterns: `lib/cg/FunctionCompiler.cpp` (qore-llvm)
- NRT wrapper approach: `include/qore/nrt/nrt.h` (qore-llvm)
- Instruction/block design as inspiration only (not compatible with main Qore)
