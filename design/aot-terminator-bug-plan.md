# Plan: Fix AOT LLVM Terminator Bug

## Status: COMPLETED (2026-02-05)

## Problem

AOT compilation fails with:
```
LLVM module verification failed: Basic Block in function '_toplevel' does not have terminator!
label %for.exit.3
```

The root cause has two parts:

1. **Missing `UninstantiateLocal` handler in QoreIRToLLVM.cpp**: The IR uses this opcode to clean up local variables at scope exit, but there's no LLVM lowering for it.

2. **Incomplete function left in module on failure**: When `lowerInstruction` fails, `lowerFunction` returns `false`, but the LLVM function created at line 702 remains in the module. Module verification then fails on this incomplete function.

## Solution Implemented

### Part 1: Add UninstantiateLocal LLVM Handler

Added handler for `QoreIROpcode::UninstantiateLocal` in `lib/QoreIRToLLVM.cpp` after the `StoreLocal` handler. The implementation:

1. Skips if the local was pre-instantiated by the caller (tiered compilation)
2. Skips entry-block locals (handled by `emitLocalUninstantiation` at function return)
3. Skips non-entry locals that were never instantiated (never had a first store)
4. For AOT mode: calls `qore_rt_uninstantiate_local_aot(ctx, slot_idx, xsink)`
5. For JIT mode: calls `qore_rt_uninstantiate_local(var, xsink)`

Note: The runtime helpers already existed; no new helpers were needed.

### Part 2: Add RAII Function Cleanup

Added RAII struct `FunctionCleanup` in `lowerFunction()` to automatically remove incomplete LLVM functions from the module when lowering fails:

```cpp
struct FunctionCleanup {
    llvm::Function* func;
    bool committed = false;
    ~FunctionCleanup() {
        if (!committed && func) {
            func->eraseFromParent();
        }
    }
} func_cleanup{llvm_func};
```

At success (before `return true`), set `func_cleanup.committed = true`.

### Part 3: Add Explicit Block Terminator Checking

Added explicit check for all LLVM blocks having terminators before LLVM verification:

```cpp
for (auto& bb : *llvm_func) {
    if (!bb.getTerminator()) {
        error = "missing terminator in LLVM block '" + bb.getName().str() + "'";
        return false;
    }
}
```

## Files Modified

| File | Changes |
|------|---------|
| lib/QoreIRToLLVM.cpp | Added UninstantiateLocal handler, RAII cleanup, explicit terminator check |

## Test Results

| Test | Result |
|------|--------|
| Simple AOT (no loop) | ✅ Pass |
| Loop AOT compilation | ✅ Pass |
| Try-catch JIT mode | ✅ Pass |
| AOTSmoke.qtest | ✅ 36/36 tests pass |
| Valgrind (JIT try-catch) | ✅ 0 errors, 0 leaks |
| Valgrind (JIT loop) | ✅ 0 errors, 0 leaks |

## Key Implementation Details

The critical insight was that the `UninstantiateLocal` handler needed to:
1. Check `entry_locals_set` to avoid double-uninstantiation of entry-block locals
2. Check `instantiated_non_entry_locals` to avoid uninstantiating variables that were never instantiated during lazy instantiation

Without these checks, the release build would segfault because the local variable stack would become corrupted.

## Invariant Note

The lazy instantiation logic assumes that the first `StoreLocal` encountered during IR lowering order is the first one to execute at runtime. This holds because:
1. Block-scoped variables in Qore are declared at a single lexical point
2. IR generation emits `InstantiateLocal`/`StoreLocal` at the declaration site
3. Qore's scoping rules prevent storing to a variable before its declaration

If IR generation ever allows multiple entry points to a block-scoped variable (e.g., computed gotos), this assumption would need revisiting. The invariant is documented in `QoreIRToLLVM.cpp` at the `StoreLocal` handler.
