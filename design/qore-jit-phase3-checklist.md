# Qore Phase 3 (LLVM JIT) Checklist

Tracks the work needed to add LLVM-based JIT execution, build configuration, runtime helpers,
logging, and verification.

## Build integration
- [x] Find LLVM via `find_package(LLVM REQUIRED CONFIG)` and expose the components needed by the
      lowering/ORC stack (`core`, `orcjit`, `native`, `support`).
      LLVM 16+ required; version check emits FATAL_ERROR for older versions.
- [x] LLVM is a required build dependency (like nghttp2, Brotli, Zstd, LZ4).
      The `WITH_JIT` CMake option and `QORE_JIT_ENABLED` preprocessor guards have been removed.
- [x] Document the build/test steps.
      Build: `cmake ..`; Run: `qore --exec-mode=jit script.q`

## JIT runtime
- [x] `QoreJIT` singleton in `include/qore/intern/QoreJIT.h` / `lib/QoreJIT.cpp` manages
      `llvm::orc::LLJIT`, symbol registration, thread-safe compilation queue, and cache of
      compiled `IRFunction`s → native function pointers.
- [x] C ABI helpers in `include/qore/intern/JITRuntime.h` / `lib/JITRuntime.cpp`:
      * Arithmetic: `qore_rt_add_any`, `qore_rt_sub_any`, `qore_rt_mul_any`, `qore_rt_div_any`, `qore_rt_mod_any`
      * Division with zero-check: `qore_rt_div_int`, `qore_rt_mod_int`, `qore_rt_div_float`
      * Conversions: `qore_rt_to_int`, `qore_rt_to_float`, `qore_rt_to_bool`
      * Refcount: `qore_rt_incref`, `qore_rt_decref`, `qore_rt_decref_nothrow`
      * Exceptions: `qore_rt_throw`, `qore_rt_has_exception`, `qore_rt_catch_exception`
      * Invoke: `qore_rt_invoke_expr`, `qore_rt_make_string`
      * Guards: `qore_rt_guard_not_nothing`, `qore_rt_guard_int`, `qore_rt_guard_float`
      * Deopt: `qore_rt_deopt`
- [x] `command-line.cpp` already supports `--exec-mode=jit`; `QoreJIT::executeWithFallback()`
      compiles + executes, falling back to IR interpreter on unsupported opcodes.

## IR→LLVM lowering
- [x] `include/qore/intern/QoreIRToLLVM.h` / `lib/QoreIRToLLVM.cpp` translates `QoreIRInstruction`s to LLVM IR:
      * Constants: `ConstInt`, `ConstFloat`, `ConstBool`, `ConstNothing`, `ConstNull`, `ConstString`
      * Typed integer arithmetic: `AddInt`, `SubInt`, `MulInt`, `DivInt`, `ModInt`
      * Typed float arithmetic: `AddFloat`, `SubFloat`, `MulFloat`, `DivFloat`
      * Dynamic (.any) arithmetic via runtime helpers: `AddAny`, `SubAny`, `MulAny`, `DivAny`, `ModAny`
      * Bitwise: `AndInt`, `OrInt`, `XorInt`, `ShlInt`, `ShrInt`
      * Unary: `UnaryMinusInt`, `UnaryMinusFloat`, `Not`, `ToBool`, `IsNullOrNothing`
      * Integer comparisons: `EqInt`, `NeInt`, `LtInt`, `LeInt`, `GtInt`, `GeInt`
      * Float comparisons: `EqFloat`, `NeFloat`, `LtFloat`, `LeFloat`, `GtFloat`, `GeFloat`
      * Local variables: `LoadLocal`, `StoreLocal` (via LLVM alloca)
      * Control flow: `Br`, `BrIf`, `Return`, `ReturnNothing`
      * Phi nodes: `Phi` (forward reference resolution)
      * Refcount: `Incref`, `Decref`, `DecrefNoThrow`
      * Exception handling: `Invoke` (with ExceptionSink polling), `LandingPad`, `CatchException`, `Throw`, `Rethrow`
      * Expression calls: `Call`, `CallIndirect`, `CallMethod`, `CallStatic` (via runtime helper)
- [x] Guard blocks for `GuardInt`, `GuardFloat`, `GuardNotNothing` with conditional branch to deopt target.
- [x] NaN-boxing encode/decode in LLVM IR: `boxInt`, `boxFloat`, `boxBool`, `boxNothing`,
      `unboxInt`, `unboxFloat`, `unboxBool` matching QoreValue bit layout.
- [x] Function signature: `uint64_t fname(ExceptionSink* xsink)` — returns NaN-boxed QoreValue.
- [x] LLVM IR verification pass after lowering (`llvm::verifyFunction`).
- [x] Graceful fallback: unsupported opcodes return error; `executeWithFallback` uses IR interpreter.

## Deopt framework
- [x] Guard failures branch to deopt target blocks in LLVM IR.
- [x] `qore_rt_deopt` placeholder for full state reconstruction (future: reconstruct IR interpreter state).
- [x] Deopt policy: `FallbackToInterpreter` (default) or `DisableJit`.

## Verification
- [x] `examples/test/ir/IRExecModeSmoke.qtest` runs 137/137 under `--exec-mode=jit` (416 assertions).
- [x] `examples/test/ir/JITSmoke.qtest` — 22 JIT-specific tests (23 assertions):
      integer/float arithmetic, comparisons, booleans, control flow, bitwise ops,
      division-by-zero, unary ops, string ops, loops, switch, try/catch,
      cross-mode correctness verification, fallback scenarios.
- [x] Valgrind clean: 0 errors, 0 leaks on all JIT tests (`qore -b`).
- [x] Benchmark: `examples/test/ir/JITBenchmark.q` — numeric loop comparing all three modes,
      identical observable behavior confirmed.
