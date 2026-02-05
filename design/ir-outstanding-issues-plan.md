# Plan: Address Outstanding IR/JIT Issues

## Status: ALL ISSUES COMPLETED (2026-02-05)

All three issues in this plan have been implemented and tested.

## Overview

This plan addressed three outstanding TODOs in the IR/JIT implementation:

| Issue | File | Status | Completed |
|-------|------|--------|-----------|
| InvokeMethodDirect | QoreIRLowering.cpp | ✅ DONE | 2026-02-05 |
| is_error flag for on_block_exit | QoreIRToLLVM.cpp | ✅ DONE | 2026-02-05 |
| Native IR lowering for functional operators | QoreIRLowering.cpp | ✅ DONE | 2026-02-05 |

### Implementation Summary

- **Issue 1 (InvokeMethodDirect):** Added dedicated opcode for devirtualized method calls in try/catch blocks, avoiding AST evaluation overhead. Tests in `testInvokeMethodDirect()`.
- **Issue 2 (is_error flag):** Verified runtime helper correctly uses `xsink->isException()`. Tests in `testOnBlockExitHandlers()`.
- **Issue 3 (Native IR):** Implemented native IR lowering with iterator-based loops. See `design/native-ir-functional-operators-plan.md`.

---

## Issue 1: InvokeMethodDirect for Proper Exception Handling

### Problem

When a devirtualized method call (on a final class) occurs inside a try/catch block, the current implementation:

1. Creates a `QoreIRInvokeInstruction` but sets `invoke_opcode = CallMethodDirect`
2. The invoke handler in LLVM lowering treats this as a generic call
3. Falls back to AST reconstruction via `qore_rt_call_with_args()`
4. Loses the compile-time optimization (direct method pointer)

### Solution

Add a dedicated `InvokeMethodDirect` opcode that:
- Stores the method pointer and class directly (like `CallMethodDirect`)
- Includes exception handling target blocks (like invoke instructions)
- Avoids AST reconstruction overhead

### Implementation

#### Phase 1.1: Add IR Opcode and Instruction Class

**File: include/qore/intern/QoreIR.h**

Add to enum (after `CallMethodDirect`, around line 328):
```cpp
InvokeMethodDirect,  //!< Exception-safe devirtualized method call
```

Add instruction class (after `QoreIRCallMethodDirectInstruction`):
```cpp
//! Exception-safe direct method call instruction for devirtualized calls in try/catch
class QoreIRInvokeMethodDirectInstruction : public QoreIRInstruction {
public:
    QoreIRInvokeMethodDirectInstruction(const QoreMethod* n_method, const QoreClass* n_qc,
            QoreIRBasicBlock* n_normal_target, QoreIRBasicBlock* n_exception_target,
            const QoreProgramLocation* n_loc = nullptr)
            : QoreIRInstruction(QoreIROpcode::InvokeMethodDirect, n_loc),
              method(n_method),
              qc(n_qc),
              normal_target(n_normal_target),
              exception_target(n_exception_target) {
    }

    const QoreMethod* method = nullptr;
    const QoreClass* qc = nullptr;
    QoreIRBasicBlock* normal_target = nullptr;
    QoreIRBasicBlock* exception_target = nullptr;
};
```

#### Phase 1.2: Add IR Builder Method

**File: include/qore/intern/QoreIRBuilder.h**

Add declaration:
```cpp
QoreIRInvokeMethodDirectInstruction* createInvokeMethodDirect(
    const QoreMethod* method, const QoreClass* qc,
    const std::vector<QoreIRValue>& operands,
    QoreIRBasicBlock* normal_target, QoreIRBasicBlock* exception_target,
    const QoreProgramLocation* loc = nullptr);
```

**File: lib/QoreIRBuilder.cpp**

Add implementation:
```cpp
QoreIRInvokeMethodDirectInstruction* QoreIRBuilder::createInvokeMethodDirect(
        const QoreMethod* method, const QoreClass* qc,
        const std::vector<QoreIRValue>& operands,
        QoreIRBasicBlock* normal_target, QoreIRBasicBlock* exception_target,
        const QoreProgramLocation* loc) {
    auto* inst = new QoreIRInvokeMethodDirectInstruction(method, qc,
        normal_target, exception_target, loc);
    inst->operands = operands;
    inst->result = createValue();
    addInstruction(inst);
    return inst;
}
```

#### Phase 1.3: Update Lowering

**File: lib/QoreIRLowering.cpp** (around line 4841-4854)

Replace:
```cpp
if (should_invoke) {
    // For invoke path, we still need to use the expr-based path for now
    // because the exception handling needs the AST node for context
    // TODO: Add InvokeMethodDirect for proper exception handling
    QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
    if (!normal_block) {
        error = "IR builder failed to create invoke continuation block";
        return QoreIRValue();
    }
    QoreIRBasicBlock* handler = exception_stack.back();
    auto* inst = builder.createInvoke(expr, operands, normal_block, handler, call->loc);
    inst->invoke_opcode = QoreIROpcode::CallMethodDirect;
    builder.setBlock(normal_block);
    result = inst->result;
}
```

With:
```cpp
if (should_invoke) {
    QoreIRBasicBlock* normal_block = createBlock("invoke.cont");
    if (!normal_block) {
        error = "IR builder failed to create invoke continuation block";
        return QoreIRValue();
    }
    QoreIRBasicBlock* handler = exception_stack.back();
    auto* inst = builder.createInvokeMethodDirect(method, qc, operands,
        normal_block, handler, call->loc);
    builder.setBlock(normal_block);
    result = inst->result;
}
```

#### Phase 1.4: Add Interpreter Handler

**File: lib/QoreIRInterpreter.cpp**

Add case in `execute()`:
```cpp
case QoreIROpcode::InvokeMethodDirect: {
    auto* inv = dynamic_cast<QoreIRInvokeMethodDirectInstruction*>(inst);
    if (!inv) {
        if (xsink) {
            xsink->raiseException("IR-INTERPRETER-ERROR",
                "invalid invoke method direct instruction");
        }
        cleanupValues(values, cleanup, xsink, true, cleanup_log);
        return false;
    }

    const QoreMethod* method = inv->method;

    // Get self object from runtime stack
    QoreObject* self = runtime_get_stack_object();
    if (!self) {
        if (xsink) {
            xsink->raiseException("IR-INTERPRETER-ERROR",
                "no self object in invoke method direct");
        }
        cleanupValues(values, cleanup, xsink, true, cleanup_log);
        return false;
    }

    // Build argument list from operands
    ReferenceHolder<QoreListNode> arg_list(
        inv->operands.empty() ? nullptr : new QoreListNode(autoTypeInfo), xsink);
    for (const auto& operand : inv->operands) {
        QoreValue arg_val = getIRValue(values, operand);
        if (arg_val.hasNode()) {
            arg_val.refSelf();
        }
        arg_list->push(arg_val, xsink);
    }

    // Call the method directly
    RuntimeConfig& rc = rc_get_current_ref();
    QoreValue res = qore_method_private::eval(*method, xsink, rc, self, *arg_list);

    // On exception, jump to exception target
    if (xsink && *xsink) {
        cleanupValues(values, cleanup, xsink, true, cleanup_log);
        cleanupStoredValues(stored_values, xsink);
        prev_block = block;
        block = inv->exception_target;
        ip = 0;
        break;
    }

    // Store result and continue to normal target
    setValueSlot(values, inv->result.id, res, xsink);
    if (res.hasNode()) {
        cleanup.push_back(inv->result.id);
    }
    prev_block = block;
    block = inv->normal_target;
    ip = 0;
    break;
}
```

#### Phase 1.5: Add LLVM Handler

**File: lib/QoreIRToLLVM.cpp**

Add case in `genInstruction()`:
```cpp
case QoreIROpcode::InvokeMethodDirect: {
    const auto* inv = static_cast<const QoreIRInvokeMethodDirectInstruction*>(inst);

    // Build args array from operands
    int nargs = static_cast<int>(inst->operands.size());
    llvm::Value* args_array;
    if (nargs > 0) {
        args_array = builder->CreateAlloca(i64_type,
            llvm::ConstantInt::get(i32_type, nargs));
        for (int i = 0; i < nargs; ++i) {
            auto* arg_val = getVal(inst->operands[i].id, error);
            if (!arg_val) {
                return false;
            }
            llvm::Value* arg_boxed = boxValue(arg_val, inst->operands[i].id);
            llvm::Value* gep = builder->CreateGEP(i64_type, args_array,
                llvm::ConstantInt::get(i32_type, i));
            builder->CreateStore(arg_boxed, gep);
        }
    } else {
        args_array = builder->CreateIntToPtr(
            llvm::ConstantInt::get(i64_type, 0), ptr_type);
    }

    // Pass method pointer as constant
    llvm::Value* method_ptr = builder->CreateIntToPtr(
        llvm::ConstantInt::get(i64_type, reinterpret_cast<uint64_t>(inv->method)),
        ptr_type);

    // Call the direct method runtime helper
    auto helper = module.getOrInsertFunction("qore_rt_call_method_direct",
        llvm::FunctionType::get(i64_type, {ptr_type, ptr_type, i32_type, ptr_type}, false));
    llvm::Value* result = builder->CreateCall(helper,
        {method_ptr, args_array, llvm::ConstantInt::get(i32_type, nargs), xsink_arg});

    // Reload locals (method calls can have side effects)
    reloadAllLocalsFromRuntime(module, llvm_func);

    values[inst->result.id] = result;
    nanboxed_values.insert(inst->result.id);
    trackResultForCleanup(result, inst->result.id, llvm_func);

    // Check for exception and branch
    auto has_ex = module.getOrInsertFunction("qore_rt_has_exception",
        llvm::FunctionType::get(i64_type, {ptr_type}, false));
    llvm::Value* ex_check = builder->CreateCall(has_ex, {xsink_arg});
    llvm::Value* has_exception = builder->CreateICmpNE(ex_check,
        llvm::ConstantInt::get(i64_type, 0));

    auto normal_it = block_map.find(inv->normal_target);
    auto except_it = block_map.find(inv->exception_target);
    if (normal_it == block_map.end() || except_it == block_map.end()) {
        error = "invoke method direct target block not found";
        return false;
    }
    builder->CreateCondBr(has_exception, except_it->second, normal_it->second);
    return true;
}
```

#### Phase 1.6: Update IR Printer and Verifier

**File: lib/QoreIRPrinter.cpp**

Add case for printing:
```cpp
case QoreIROpcode::InvokeMethodDirect: {
    auto* inv = dynamic_cast<const QoreIRInvokeMethodDirectInstruction*>(inst);
    if (inv) {
        os << "invoke_method_direct @" << (inv->method ? inv->method->getName() : "null")
           << " normal:" << (inv->normal_target ? inv->normal_target->name : "null")
           << " except:" << (inv->exception_target ? inv->exception_target->name : "null");
    }
    break;
}
```

**File: lib/QoreIRVerifier.cpp**

Add verification:
```cpp
case QoreIROpcode::InvokeMethodDirect: {
    auto* inv = dynamic_cast<const QoreIRInvokeMethodDirectInstruction*>(inst);
    if (!inv) {
        errors.push_back("InvokeMethodDirect instruction has wrong type");
        return false;
    }
    if (!inv->method) {
        errors.push_back("InvokeMethodDirect has null method pointer");
        return false;
    }
    if (!inv->normal_target || !inv->exception_target) {
        errors.push_back("InvokeMethodDirect missing target blocks");
        return false;
    }
    break;
}
```

### Files to Modify

| File | Changes |
|------|---------|
| include/qore/intern/QoreIR.h | Add opcode and instruction class |
| include/qore/intern/QoreIRBuilder.h | Add builder method declaration |
| lib/QoreIRBuilder.cpp | Add builder method implementation |
| lib/QoreIRLowering.cpp | Replace TODO with new instruction |
| lib/QoreIRInterpreter.cpp | Add execution handler |
| lib/QoreIRToLLVM.cpp | Add LLVM code generation |
| lib/QoreIRPrinter.cpp | Add printing support |
| lib/QoreIRVerifier.cpp | Add verification |

---

## Issue 2: Handle is_error Flag for on_block_exit

### Problem

The `ScopeExit` instruction has an `is_error` flag that indicates whether the block is exiting due to an exception. This flag is currently ignored in QoreIRToLLVM.cpp:

```cpp
// TODO: Handle is_error flag - for now we pass false (normal exit)
```

This means `on_error` and `on_success` handlers may not execute correctly when:
- An exception occurs and `on_error` should run but doesn't
- An exception occurs and `on_success` runs but shouldn't

### Solution

The runtime helper `qore_rt_exec_on_block_exit` already derives error state from `xsink->isException()`. However, for more precise semantics matching AST mode, we should pass the `is_error` flag explicitly.

**Two options:**

1. **Option A (Recommended):** Check `xsink->isException()` at runtime in JIT code
2. **Option B:** Add `is_error` parameter to runtime helper

Option A is simpler and matches current runtime behavior without API changes.

### Implementation

#### Phase 2.1: Update LLVM ScopeExit Handler

**File: lib/QoreIRToLLVM.cpp** (around line 4788-4805)

Replace:
```cpp
case QoreIROpcode::ScopeExit: {
    const auto* sinst = static_cast<const QoreIRScopeExitInstruction*>(inst);
    auto it = scope_obe_counts.find(sinst->scope_id);
    if (it != scope_obe_counts.end()) {
        llvm::Value* saved_count = it->second;
        // TODO: Handle is_error flag - for now we pass false (normal exit)
        auto helper = module.getOrInsertFunction("qore_rt_exec_on_block_exit",
                llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
        builder->CreateCall(helper, {saved_count, xsink_arg});
    }
    // ...
}
```

With:
```cpp
case QoreIROpcode::ScopeExit: {
    const auto* sinst = static_cast<const QoreIRScopeExitInstruction*>(inst);
    auto it = scope_obe_counts.find(sinst->scope_id);
    if (it != scope_obe_counts.end()) {
        llvm::Value* saved_count = it->second;
        // The runtime helper checks xsink->isException() to determine error state,
        // which correctly handles on_error/on_success semantics
        auto helper = module.getOrInsertFunction("qore_rt_exec_on_block_exit",
                llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
        builder->CreateCall(helper, {saved_count, xsink_arg});
    }
    // ScopeExit produces NOTHING as its result
    if (inst->result.isValid()) {
        values[inst->result.id] = llvm::ConstantInt::get(i64_type, VAL_NOTHING);
        nanboxed_values.insert(inst->result.id);
    }
    return true;
}
```

The key insight is that the runtime helper already handles this correctly by checking `xsink->isException()`. The TODO comment is misleading - the current implementation is correct.

#### Phase 2.2: Add Test Coverage

**File: examples/test/ir/JITSmoke.qtest**

Add test for on_error/on_success:
```qore
testOnBlockExitHandlers() {
    # Test on_exit (unconditional)
    list<string> log = ();
    {
        on_exit log += "exit";
        log += "body";
    }
    assertEq(("body", "exit"), log, "on_exit runs unconditionally");

    # Test on_success (no exception)
    log = ();
    {
        on_success log += "success";
        on_error log += "error";
        log += "body";
    }
    assertEq(("body", "success"), log, "on_success runs on normal exit");

    # Test on_error (with exception)
    log = ();
    try {
        on_success log += "success";
        on_error log += "error";
        log += "body";
        throw "TEST", "test exception";
    } catch (hash<ExceptionInfo> ex) {
        log += "caught";
    }
    assertEq(("body", "error", "caught"), log, "on_error runs on exception");

    # Test nested scopes
    log = ();
    try {
        on_error log += "outer_error";
        {
            on_error log += "inner_error";
            throw "TEST", "inner exception";
        }
    } catch (hash<ExceptionInfo> ex) {
        log += "caught";
    }
    assertEq(("inner_error", "outer_error", "caught"), log, "nested on_error handlers");
}
```

### Files to Modify

| File | Changes |
|------|---------|
| lib/QoreIRToLLVM.cpp | Update comment (implementation is correct) |
| examples/test/ir/JITSmoke.qtest | Add on_block_exit test cases |

---

## Issue 3: Native IR Lowering for Functional Operators

### Problem

The comment at line 5430 notes that native IR lowering for functional operators with implicit arguments ($1, $2, $#) is "complex and deferred for future work."

### Analysis

**Current state:**
- All IR infrastructure exists (opcodes, builder, runtime, LLVM lowering)
- Optimized patterns (MapScaleInt, SelectPositiveInt, etc.) already handle simple cases
- Complex expressions delegate to AST via MapAny/SelectAny/FoldlAny opcodes

**What native IR would require:**
1. Generate loop structure in IR for each element
2. Push/pop implicit argument context per iteration
3. Lower the expression with context available
4. Handle exceptions and cleanup correctly

**Benefit assessment:**
- Simple patterns already optimized (5-20x faster than AST)
- Complex expressions still dominated by expression evaluation cost
- Limited benefit for additional complexity

### Recommendation: LOW PRIORITY - Document and Defer

The current implementation is correct and performant for most use cases. Native IR lowering would provide marginal benefit for high complexity.

### Implementation

#### Phase 3.1: Update Comment to Document Status

**File: lib/QoreIRLowering.cpp** (line 5430)

Replace:
```cpp
// Native IR lowering with context setup is complex and deferred for future work
```

With:
```cpp
// Native IR lowering for non-optimized patterns is deferred.
// Rationale:
// - Optimized patterns (MapScaleInt, etc.) handle simple cases with 5-20x speedup
// - Complex expressions are dominated by expression evaluation cost
// - IR infrastructure exists (PushImplicitArg, etc.) but loop generation is complex
// - AST delegation via MapAny is correct and sufficient for edge cases
// See: design/ir-outstanding-issues-plan.md for full analysis
```

#### Phase 3.2: Document in Design Directory

This plan document serves as the documentation.

### Files to Modify

| File | Changes |
|------|---------|
| lib/QoreIRLowering.cpp | Update comment with rationale |

---

## Implementation Order

| Phase | Description | Estimated Effort |
|-------|-------------|------------------|
| 2.1-2.2 | is_error flag (comment + tests) | Low |
| 3.1 | Document native IR deferral | Low |
| 1.1-1.6 | InvokeMethodDirect full implementation | Medium |

Recommended order: 2 → 3 → 1 (simplest first, then documentation, then feature)

---

## Testing Strategy

### Unit Tests
- Run JITSmoke.qtest after each phase
- Add specific tests for on_block_exit handlers
- Add tests for devirtualized method calls in try/catch

### Integration Tests
- Run full test suite with all execution modes (ast, ir, jit)
- Verify cross-mode consistency

### Memory Validation
- Run valgrind on debug build after Phase 1 (C++ changes)

### Commands
```bash
# After each phase
LD_LIBRARY_PATH=build build/qore --enable-debug examples/test/ir/JITSmoke.qtest -v

# All execution modes
LD_LIBRARY_PATH=build build/qore --enable-debug --exec-mode=ast examples/test/ir/JITSmoke.qtest
LD_LIBRARY_PATH=build build/qore --enable-debug --exec-mode=ir examples/test/ir/JITSmoke.qtest
LD_LIBRARY_PATH=build build/qore --enable-debug --exec-mode=jit examples/test/ir/JITSmoke.qtest

# Valgrind (after Phase 1)
LD_LIBRARY_PATH=build valgrind --leak-check=full build/qore -b --exec-mode=ir examples/test/ir/JITSmoke.qtest
```

---

## Success Criteria

| Phase | Criteria |
|-------|----------|
| 1 | InvokeMethodDirect opcode works, TODO removed, tests pass |
| 2 | on_block_exit tests pass for on_error/on_success, TODO resolved |
| 3 | Comment updated with rationale, plan documented |

---

## Risk Assessment

| Risk | Severity | Mitigation |
|------|----------|------------|
| InvokeMethodDirect breaks existing code | Medium | Extensive testing, maintain fallback path |
| on_block_exit tests reveal bugs | Low | Current runtime implementation is correct |
| Performance regression | Low | Benchmarking before/after |

---

## Summary

All issues have been completed:

- **Issue 1 (InvokeMethodDirect):** ✅ Implemented dedicated opcode with full IR/interpreter/LLVM support. Tests pass in all modes (ast, ir, jit). No memory leaks.
- **Issue 2 (is_error flag):** ✅ Runtime implementation verified correct. Tests added for on_exit/on_success/on_error handlers.
- **Issue 3 (Native IR):** ✅ Implemented with iterator-based loops. See `design/native-ir-functional-operators-plan.md` for details.

All tests pass. Valgrind shows no memory leaks.
