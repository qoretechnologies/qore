# Qore Phase 3 (LLVM JIT) Checklist

Tracks the work needed to add LLVM-based JIT execution, build configuration, runtime helpers,
logging, and verification.

## Build integration
- [ ] Add `WITH_JIT` CMake option that finds LLVM via `find_package(LLVM REQUIRED CONFIG)` and exposes
      the components needed by the lowering/ORC stack (`core`, `orcjit`, `native`, `support`).
- [ ] Wrap the new LLVM sources with `QORE_JIT_ENABLED` only when `WITH_JIT` is ON; fall back to
      IR interpreter when LLVM is unavailable.
- [ ] Document the build/test steps for the optional JIT target.

## JIT runtime
- [ ] Implement `include/qore/intern/LLVMJITCompiler.h` & `lib/LLVMJITCompiler.cpp` to manage `llvm::orc::LLJIT`,
      symbol registration, thread-safe compilation queue, and cache of compiled `IRFunction`s.
- [ ] Provide C ABI helpers in `lib/JITRuntime.cpp` for refcount (incref/decref), exception raising,
      and the deopt entrypoint the generated code will call when guards fail.
- [ ] Update `command-line.cpp`/`RuntimeConfig` to default to `--exec-mode=ast` but allow `--exec-mode=jit`.

## IR→LLVM lowering
- [ ] Create `include/qore/intern/LLVMLowering.h` and `lib/LLVMLowering.cpp` to translate `QoreIRInstruction`s to LLVM IR:
      * Map `const`/arithmetic/comparison operations to typed LLVM code.
      * Lower local loads/stores, branch, branch-if, return/throw.
      * Lower `invoke` to LLVM `invoke`/landingpad with c++ personality that integrates with `ExceptionSink`.
- [ ] Emit guard blocks for `GuardInt`, `GuardFloat`, `GuardType`, `GuardNotNothing`, plus explicit refcount cleanup as modeled in the interpreter.
- [ ] Generate metadata for deopt points and the state snapshot so the fallback interpreter can resume when a guard fails.

## Verification
- [ ] Add `examples/test/ir/IRExecModeSmoke.qtest` run under `--exec-mode=jit` to the test suite.
- [ ] Extend the Valgrind smoke test run (with `qore -b`) to cover the JIT path as well.
- [ ] Benchmark sample numeric loop with `--exec-mode=jit` vs `--exec-mode=ir` to ensure basic speedup and correctness.
