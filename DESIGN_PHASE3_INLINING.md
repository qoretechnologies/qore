# Phase 3.2 Design: Aggressive IR-Level Inlining for Small Callees

## Overview

Phase 3.2 implements LLVM-level inlining of small, frequently-called methods to eliminate dispatch overhead entirely. This complements Phase 1 (fast dispatch) and Phase 2 (exception check inlining) by removing the final call overhead for simple callees.

**Target**: self_method benchmark (method called 1M times in tight loop)
- Current: 2.05x speedup (579.9ms AOT vs 1189.5ms AST)
- Target: ~3.5x speedup (340ms AOT vs 1189.5ms AST)
- Mechanism: Inline the 3-instruction method body directly at call site

**Scope**: Methods with ≤20 IR instructions and ≤2 basic blocks (already detected and cached in Phase 3.1)

---

## Architecture

### 1. Value Remapping Layer

When inlining callee into caller, all callee value IDs must map to caller's LLVM values.

```cpp
// Allocate at start of inlining
std::unordered_map<uint32_t, llvm::Value*> callee_values_map;

// For each callee value ID, compute corresponding LLVM value:
// - Constants: create new llvm::ConstantInt/ConstantFP
// - Operands: map from call_inst->operands (function args)
// - Result: special handling (return value becomes call result)
// - Intermediate: emit the generating instruction inline
```

**Key insight**: We're translating QoreIR value IDs to LLVM values, NOT translating QoreIR instructions to LLVM IR. The callee is already in QoreIR; we're using the existing emission logic with remapped values.

### 2. Instruction Emission Strategy

Rather than re-implementing instruction emission, **reuse existing `lowerInstruction()` logic**:

```cpp
// For each instruction in callee:
1. Remap its operands using callee_values_map
2. Remap its result ID to fresh value ID in caller context
3. Call lowerInstruction(remapped_inst) to emit LLVM code
4. Store result in callee_values_map[original_result_id]
```

This avoids duplicating 1000+ lines of instruction-lowering code and ensures consistency.

### 3. Block Mapping & Control Flow

```cpp
// Create LLVM basic blocks for each callee block
std::unordered_map<const QoreIRBasicBlock*, llvm::BasicBlock*> callee_block_map;
for (const auto& callee_block : callee_ir->blocks) {
    llvm::BasicBlock* new_bb = llvm::BasicBlock::Create(ctx, "inline_" + callee_block->name);
    callee_block_map[callee_block.get()] = new_bb;
}

// For branch/jump instructions:
// - Remap target block references using callee_block_map
// - For Return: return the result value instead of jumping
// - For exceptions: route to caller's active exception_target
```

**Special case: exception routing**
- If callee can throw and caller has try/catch, route exception branches to caller's exception_target
- If caller has no try/catch, route to caller's error_return_block

### 4. Local Variable Allocation

Callee may have local variables (closure captures, loop counters, etc.):

```cpp
// Allocate LLVM allocas in caller's entry block for callee's locals
for (const auto& local : callee_ir->locals) {
    llvm::AllocaInst* alloca = builder->CreateAlloca(
        ptr_type,  // all locals are pointers in NaN-boxed format
        nullptr,
        "inline_local_" + local->name
    );
    callee_local_allocas[local.get()] = alloca;
}

// When callee emits LoadLocal/StoreLocal:
// - Use callee_local_allocas instead of outer function_locals
```

### 5. Integration Points

#### A. Dispatch Decision in CallMethodDirect Handler

```cpp
case QoreIROpcode::CallMethodDirect: {
    const auto* direct_inst = static_cast<const QoreIRCallMethodDirectInstruction*>(inst);

    // Check if we should inline
    if (aot_mode && direct_inst->cached_callee_ir && direct_inst->inline_ir_state == 1) {
        // Inline the callee
        llvm::Value* result = emitInlinedCallee(*direct_inst->cached_callee_ir,
                                                 direct_inst, llvm_func, module, error);
        if (!result) return false;
        values[inst->result.id] = result;
        nanboxed_values.insert(inst->result.id);
        trackResultForCleanup(result, inst->result.id, llvm_func);
        emitExceptionCheck(module, llvm_func, inst);
        return true;
    }

    // Fall back to existing dispatch logic (fast path or direct call)
    // ... [existing code]
}
```

Same for `InvokeMethodDirect`, `CallStaticDirect`, `InvokeDotEvalMethodDirect`.

#### B. Exception Handling Within Inlined Code

```cpp
// Save and restore exception context during inlining
struct InliningContext {
    const QoreIRBasicBlock* saved_exception_target;
    std::unordered_map<const QoreIRBasicBlock*, llvm::BasicBlock*> saved_block_map;
};

// During inlining:
// 1. Save current exception_target
// 2. Set exception_target to caller's active exception block
// 3. Emit callee blocks with exception branches routed correctly
// 4. Restore exception_target
```

---

## Implementation Steps

### Step 1: Implement emitInlinedCallee() Skeleton (1-2 days)

```cpp
llvm::Value* QoreIRToLLVM::emitInlinedCallee(
        const QoreIRFunction& callee_ir,
        const QoreIRInstruction* call_inst,
        llvm::Function* llvm_func,
        llvm::Module& module,
        std::string& error) {

    // 1. Save and setup state
    auto saved_values = values;
    auto saved_block_map = block_map;
    std::unordered_map<uint32_t, llvm::Value*> callee_values;
    std::unordered_map<const void*, llvm::AllocaInst*> callee_locals;

    // 2. Map operands (call arguments) to callee parameter value IDs
    // Callee's first block expects values for parameters in its initial values

    // 3. Allocate LLVM allocas for callee's locals in caller entry block

    // 4. Create LLVM basic blocks for callee blocks

    // 5. Emit callee blocks

    // 6. Return the result value

    // 7. Restore state
    values = saved_values;
    block_map = saved_block_map;
    return result_value;
}
```

### Step 2: Value & Block Remapping (3-4 days)

- Implement `mapCalleeValue()` helper to compute LLVM value for any callee value ID
- Implement `mapCalleeBlock()` to get LLVM block for callee IR block
- Handle special cases: constants, operands, return values

### Step 3: Instruction Emission with Remapping (4-5 days)

- For each callee instruction:
  1. Clone the instruction with remapped operands
  2. Remap the result ID to a fresh ID in the caller's values map
  3. Call existing `lowerInstruction()` logic
  4. Handle ReturnValue → return conversion

### Step 4: Control Flow & Exception Handling (3-4 days)

- Emit branch instructions with block remapping
- Route exception paths to caller's handler
- Handle early returns (convert to jumps to exit block)

### Step 5: Integration with Dispatch (1-2 days)

- Modify CallMethodDirect, InvokeMethodDirect handlers
- Add dispatch logic: inline vs. call decision
- Add AOT compilation support

### Step 6: Testing & Validation (3-4 days)

- Unit tests: simple inlined methods
- Integration tests: inlined method calling another method
- Performance tests: self_method benchmark
- Valgrind: no memory leaks
- Full test suite: 9/9 IR tests still pass

**Total: 2-3 weeks of focused work**

---

## Key Design Decisions

### 1. Reuse Existing Instruction Lowering

**Decision**: Emit callee instructions by calling existing `lowerInstruction()` logic with remapped operands.

**Alternative**: Duplicate instruction-lowering code for inlined context (bad: 1000+ LOC of duplication).

**Rationale**: Ensures consistency, avoids duplication, leverages proven logic.

### 2. Allocate Callee Locals in Caller Entry Block

**Decision**: Create allocas for callee's locals in the caller's entry block (before any actual inlining).

**Alternative**: Create allocas on-demand as instructions reference them (harder to track).

**Rationale**: Simpler, matches LLVM best practices, easier to debug.

### 3. Route Exceptions to Caller's Handler

**Decision**: Exceptions raised in inlined code route to the caller's active exception handler.

**Alternative**: Create new exception handlers within inlined blocks (complex, duplicates error handling).

**Rationale**: Simpler, more consistent with try/catch semantics.

### 4. Conservative Inlining Threshold

**Decision**: Only inline if ≤20 instructions AND ≤2 basic blocks (already computed in Phase 3.1).

**Alternative**: Inline larger callees (bloats caller, degrades LLVM optimization).

**Rationale**: Balances benefit (dispatch overhead elimination) with cost (code size).

---

## Testing Strategy

### Unit Tests

```qore
# Test 1: Simple inlined method
class Foo {
    int get_x() { return 42; }
}
Foo f();
assert(f.get_x() == 42);  // Should inline, return constant

# Test 2: Inlined method with arguments
class Bar {
    int add(int a, int b) { return a + b; }
}
Bar b();
assert(b.add(3, 5) == 8);  // Should inline, compute sum

# Test 3: Inlined method accessing member
class Baz {
    int x = 10;
    int get_x() { return x; }
}
Baz z();
assert(z.get_x() == 10);  // Should inline, load member
```

### Performance Tests

```qore
# Benchmark: self_method (1M iterations, tight loop)
# Expected:
#   Phase 1 only: 579.9ms (2.05x speedup)
#   Phase 1 + 3.2: ~340ms (3.5x speedup)
```

### Regression Tests

- Full IR test suite (9/9 files)
- AOTSmoke (85/85)
- JITSmoke (153/153)
- Valgrind on affected tests

---

## Risk Mitigation

### Risk 1: LLVM Verification Failures

**Mitigation**: Test with `llvm::verifyFunction()` after each inlining.

### Risk 2: Value Remapping Bugs

**Mitigation**: Comprehensive unit tests for value mapping logic.

### Risk 3: Exception Path Bugs

**Mitigation**: Test inlined methods in try/catch blocks extensively.

### Risk 4: Performance Regressions

**Mitigation**: Benchmark before/after; fall back to direct calls if inlining hurts performance.

---

## Future Extensions

### Extension 1: Larger Inlining Threshold

Increase threshold from 20 to 50+ instructions for very hot paths (requires profiling data).

### Extension 2: Cross-Function Inlining

Inline calls within inlined methods (if both are small enough).

### Extension 3: JIT Inlining

Apply same inlining at JIT compile time for additional gains.

### Extension 4: Inline Caching

Cache inlined LLVM IR blocks to avoid re-emitting for repeated calls (if same callee called multiple times).

---

## References

- Phase 3.1: Inlining infrastructure (cached_callee_ir, inline_ir_state)
- Phase 1: Fast dispatch (variant pre-resolution)
- Phase 2b: Exception check inlining (direct memory loads)
- QoreIRToLLVM.cpp: Instruction emission logic (~10650 LOC)
- QoreIRLowering.cpp: IR instruction creation (~7100 LOC)
