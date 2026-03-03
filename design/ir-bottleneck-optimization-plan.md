# IR Interpreter Bottleneck Optimization Plan

## Current State (2026-03-02)

| Benchmark | IR vs AST | Tiered vs AST | Root Cause |
|-----------|-----------|---------------|------------|
| loop | 0.58x | 1.92x | Dispatch overhead: 13 IR instructions/iteration vs 3 AST evals |
| self_method | 0.90x | 0.90x | Member lvalue indirection + AST re-evaluation in evalPlusEquals |
| map_hash_expr | 0.80x | 1.11x | build_data loop + ListGetValue refSelf per element |
| map_hash_body | 0.81x | 1.01x | build_data loop + function call + implicit arg push/pop |
| select_hash | 0.74x | 1.09x | build_data loop + predicate refSelf on ALL elements |

Average: IR 0.96x, tiered 1.28x, Swagger: IR +28%, tiered +24%

---

## Bottleneck 1: Loop Dispatch Overhead (loop 0.58x)

### Problem
The simple loop `while (i < N) { sum += i; ++i; }` generates 13 IR instructions
per iteration, each going through the switch dispatch. AST evaluates the same
loop body in 3 statement evaluations.

**Current IR per iteration:**
```
# Condition (4 instructions)
LoadLocal     i
ConstInt      1000000
LtInt         i < 1000000
BranchIf      → body or exit

# sum += i (4 instructions)
LoadLocal     sum
LoadLocal     i
AddAssignInt  sum + i
StoreLocal    sum

# ++i (4 instructions)
LoadLocal     i
ConstInt      1
AddAssignInt  i + 1
StoreLocal    i

# Back to condition (1 instruction)
Branch        → cond
```

### Solution: Fused Local Int Instructions

#### Phase 1a: `AddAssignLocalInt` opcode

Fuses `LoadLocal lhs + LoadLocal rhs + AddAssignInt + StoreLocal lhs` into 1 instruction.

**New instruction class** in `QoreIR.h`:
```cpp
class QoreIRAddAssignLocalIntInstruction : public QoreIRInstruction {
public:
    QoreIRAddAssignLocalIntInstruction(LocalVar* n_target, uint32_t n_target_slot,
            LocalVar* n_source, uint32_t n_source_slot)
        : QoreIRInstruction(QoreIROpcode::AddAssignLocalInt),
          target(n_target), target_slot(n_target_slot),
          source(n_source), source_slot(n_source_slot) {}
    LocalVar* target;
    uint32_t target_slot;
    LocalVar* source;
    uint32_t source_slot;
};
```

**Interpreter handler**: Read both values from slot cache, add, write result
back to slot cache and to the thread-local variable via `assignLocalVarValue`.

**Lowering**: In `lowerPlusEquals`, when both LHS and RHS are typed int locals
(detected via `QoreIntPlusEqualsOperatorNode` and both sides are VT_LOCAL),
emit `AddAssignLocalInt` instead of the 4-instruction sequence.

**Expected impact**: `sum += i` goes from 4 to 1 instruction. Saves 3 dispatch
cycles per iteration.

#### Phase 1b: `IncrementLocalInt` opcode

Fuses `LoadLocal var + ConstInt delta + AddAssignInt + StoreLocal var` into 1
instruction for the common `++i` / `--i` / `i += const` pattern.

**New instruction class**:
```cpp
class QoreIRIncrementLocalIntInstruction : public QoreIRInstruction {
public:
    QoreIRIncrementLocalIntInstruction(LocalVar* n_local, uint32_t n_slot, int64_t n_delta)
        : QoreIRInstruction(QoreIROpcode::IncrementLocalInt),
          local(n_local), slot_id(n_slot), delta(n_delta) {}
    LocalVar* local;
    uint32_t slot_id;
    int64_t delta;
};
```

**Lowering**: In `lowerPreIncrement`, when the variable is a typed int local,
emit `IncrementLocalInt(local, slot, 1)` instead of the 4-instruction sequence.
Also detect `i += const` in `lowerPlusEquals` when RHS is a constant int.

**Expected impact**: `++i` goes from 4 to 1 instruction. Saves 3 dispatch
cycles per iteration.

#### Phase 1c: `BranchIfLtLocalInt` opcode

Fuses `LoadLocal a + LoadLocal b + LtInt + BranchIf` into 1 instruction for
the common loop condition pattern `i < N` where both sides are locals.

**New instruction class**:
```cpp
class QoreIRBranchIfLtLocalIntInstruction : public QoreIRInstruction {
public:
    QoreIRBranchIfLtLocalIntInstruction(LocalVar* n_lhs, uint32_t n_lhs_slot,
            LocalVar* n_rhs, uint32_t n_rhs_slot,
            QoreIRBasicBlock* n_true_target, QoreIRBasicBlock* n_false_target)
        : QoreIRInstruction(QoreIROpcode::BranchIfLtLocalInt),
          lhs(n_lhs), lhs_slot(n_lhs_slot), rhs(n_rhs), rhs_slot(n_rhs_slot),
          true_target(n_true_target), false_target(n_false_target) {}
    LocalVar* lhs;
    uint32_t lhs_slot;
    LocalVar* rhs;
    uint32_t rhs_slot;
    QoreIRBasicBlock* true_target;
    QoreIRBasicBlock* false_target;
};
```

**Lowering**: Detect `while (intLocal < intLocal)` condition in `lowerWhile`
and emit the fused branch.

**Expected impact**: Condition check goes from 4+1 (including the Jump back)
to 1 instruction.

#### Combined Impact

Loop body goes from 13 instructions to 3:
```
AddAssignLocalInt  sum, i
IncrementLocalInt  i, 1
BranchIfLtLocalInt i, iterations, body, exit
```

**Expected speedup**: ~3-4x for the IR interpreter on this pattern, bringing
loop from 0.58x to ~1.5-2.0x. The remaining overhead vs JIT (1.92x) comes from
switch dispatch vs native code.

---

## Bottleneck 2: Self-Method Member Lvalue (self_method 0.90x)

### Problem
`self.total += i` in IR goes through:
1. `getIRValue` to read `i` from values[]
2. `evalLValueBinary` dispatch (function call)
3. `evalPlusEquals` wrapper (SafeDerefHelper construction)
4. `LValueHelper` construction (re-evaluates `self.total` through full AST)
5. `plusEqualsBigInt` operation
6. `getReferencedValue` return
7. `setValueSlot` to store result

AST mode does this in 3 steps: evaluate right, construct LValueHelper, +=.

### Solution: Inline evalPlusEquals for int types

#### Phase 2a: Inline int fast path in AddAssignLValue handler

Instead of calling `evalLValueBinary → evalPlusEquals`, handle the common
int case directly in the AddAssignLValue handler:

```cpp
case QoreIROpcode::AddAssignLValue: {
    auto* lval_inst = static_cast<QoreIRLValueInstruction*>(inst);
    QoreValue right = getIRValue(values, lval_inst->operands[0]);

    // Skip cache invalidation for non-local targets (already done)
    // ...

    // FAST PATH: direct int plus-equals without evalPlusEquals wrapper
    LValueHelper v(lval_inst->lvalue, xsink);
    if (!v) { /* error */ }
    QoreValue res;
    if (v.getType() == NT_INT) {
        res = v.plusEqualsBigInt(right.getAsBigInt(), "<+= operator>");
    } else {
        // Fall back to full evalPlusEquals for non-int types
        res = evalPlusEquals(lval_inst->lvalue, right, xsink);
    }
    // ...
}
```

This eliminates:
- `evalLValueBinary` dispatch (one function call)
- `evalPlusEquals` wrapper (SafeDerefHelper construction)
- For int types: `getReferencedValue` overhead

**Expected impact**: ~3-5% speedup on self_method (0.90x → 0.93-0.95x).

#### Phase 2b: Extend to other compound assignment ops

Apply the same inline pattern to SubAssignLValue, MulAssignLValue, etc.
for int types. This benefits any code with member compound assignments.

---

## Bottleneck 3: Hash-Heavy Benchmarks (0.74-0.81x)

### Problem Analysis

All three hash benchmarks share `build_data()` which creates 1M hash elements
in a for loop. This loop has the same dispatch overhead as the `loop` benchmark,
plus hash construction and `push` lvalue operations per iteration.

Additionally:
- **map_hash_expr**: `ListGetValue` does `getReferencedEntry()` → refSelf() per
  element, creating cleanup entries. AST uses internal iteration without refcounting.
- **select_hash**: Same refSelf overhead, applied to ALL elements even those
  that fail the predicate.
- **map_hash_body**: Function call with implicit arg push/pop per element.

### Solution: Multi-pronged approach

#### Phase 3a: Fused instructions benefit build_data

The `AddAssignLocalInt` and `IncrementLocalInt` from Phase 1 will speed up the
`for (int i = 0; i < n; ++i)` loop in `build_data`. The loop body still has
hash construction and push, but the loop overhead itself shrinks.

**Expected impact**: ~10-15% speedup on build_data portion.

#### Phase 3b: Add `ListGetValueNoRef` for read-only element access

When map/select body only reads from the element (no modification), use a
non-referencing list element access:

**New opcode**: `ListGetValueNoRef` — returns the element value without
`refSelf()`. Safe because the list outlives the loop iteration and the
element is only read, not stored.

**Interpreter handler**:
```cpp
case QoreIROpcode::ListGetValueNoRef: {
    // Same as ListGetValue but without getReferencedEntry()
    QoreValue list_val = getIRValue(values, inst->operands[0]);
    QoreValue index_val = getIRValue(values, inst->operands[1]);
    QoreListNode* l = list_val.get<QoreListNode>();
    QoreValue elem = l->retrieveEntry(index_val.getAsBigInt());
    // No refSelf — element is borrowed, not owned
    setValueSlotDirect(values, inst->result.id, elem);
    // Do NOT add to cleanup — no owned reference
    ++ip;
    break;
}
```

**Lowering**: In `lowerHashMapNative` and `lowerSelectNative`, when the loop
body is read-only (no modification of elements, no storage beyond the iteration),
emit `ListGetValueNoRef` instead of `ListGetValue`.

**Expected impact**: Eliminates 1 refSelf + 1 deref per element. For 1M
elements, this saves ~2M refcount operations.

#### Phase 3c: Optimize build_data's `push` + hash literal

The `push data, {...}` in build_data generates a `PushAny` lvalue operation
that goes through AST delegation. Two optimizations:

1. **Hash literal with known keys**: When all keys are string constants and
   values are simple expressions, emit a specialized `MakeHashFast` that
   pre-allocates the hash with known key count and populates directly.

2. **List push fast path**: When the list variable is a typed local and the
   push value is a new hash/list (refcount=1), use a direct `list->push()`
   instead of going through lvalue mechanics.

**Expected impact**: ~5-10% speedup on build_data.

---

## Implementation Priority

1. **Phase 1a+1b**: `AddAssignLocalInt` + `IncrementLocalInt` (highest impact,
   benefits all int-heavy loops including build_data)
2. **Phase 1c**: `BranchIfLtLocalInt` (completes the loop optimization)
3. **Phase 2a**: Inline int fast path in AddAssignLValue (targeted, low risk)
4. **Phase 3b**: `ListGetValueNoRef` (requires careful safety analysis)
5. **Phase 3c**: Hash/push fast paths (moderate impact, moderate complexity)

## Verification

After each phase:
1. Run all tests in ast/ir/tiered modes
2. Run with QORE_IR_THRESHOLD=3 for aggressive promotion
3. Run benchmark to measure impact
4. Run leaks tool to verify zero leaks
5. Valgrind on Linux CI after push

## Target Performance

| Benchmark | Current IR | Target IR | Notes |
|-----------|-----------|-----------|-------|
| loop | 0.58x | 1.5-2.0x | Fused instructions |
| self_method | 0.90x | 0.93-0.95x | Inline int compound assign |
| map_hash_expr | 0.80x | 0.90-0.95x | ListGetValueNoRef + fused loops |
| map_hash_body | 0.81x | 0.85-0.90x | Fused loops + reduced dispatch |
| select_hash | 0.74x | 0.85-0.90x | ListGetValueNoRef + fused loops |
| **Average IR** | **0.96x** | **~1.10-1.20x** | |
| Swagger IR | +28% | +15-20% | Distributed gains |
