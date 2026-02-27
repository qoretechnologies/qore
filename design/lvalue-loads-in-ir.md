# Lvalue Loads in Qore IR — COW Invariant and the auto_ref=false Contract

## Fundamental Invariant

> **All lvalue operations MUST see the variable's natural (unreferenced) refcount
> and ONLY execute COW if the value is truly not unique.**

This rule must be enforced through every lvalue path — IR caches, SSA value slots,
guard LoadLocal instructions, and AST-delegated StoreLValue evaluation.  If any
extra reference inflates the refcount at the point `LValueHelper::ensureUnique()`
runs, COW creates an unnecessary copy that leaks (the copy is discarded, the
original retains the old value, and the mutation is silently lost).

### Sources of Refcount Inflation

| Source | Location | Mitigation |
|---|---|---|
| `locals` map cache | `storeValue()` does `refSelf()` | `cleanupStoredValues(locals, ...)` before lvalue ops |
| `locals_slot_cache` | Phase 3 fast cache, `refSelf()` | `discard()` all slot cache entries before lvalue ops |
| `values[]` (SSA slots) | LoadLocal stores result | Discard values[] entries for the lvalue target variable |
| `ClosureVarValue::eval()` owned ref | Returns `val.getReferencedValue()` (always +1) | Deref owned ref after caching in LoadLocal auto_ref=true |
| `SlotCacheCleanup` missing | Slot cache refs leaked on function exit | RAII guard discards all entries on any exit path |

## Rule

Any IR instruction that reads a container variable as an **lvalue** (i.e., to modify it in-place via COW or direct mutation) MUST use `LoadLocal(var, auto_ref=false)`. This suppresses refcount inflation so that `is_unique()` in the mutation handler reflects the container's actual sharing state.

## Why

COW (copy-on-write) works by checking `is_unique()` on the container. If refcount > 1, a copy is made. Every extra IR reference — alloca cache slots, `values[]` init slots — falsely inflates the refcount, causing unnecessary copies and, worse, leaving the mutation applied to the copy while the caller still sees the original.

The fundamental problem: if a container held a refcount of 1 (truly unique), but the IR machinery holds extra borrowed references, `is_unique()` returns false, triggering a copy. The mutation is then applied to the copy, and the mutation is lost when the copy is discarded at function exit.

## Affected Opcodes (current)

| Opcode | Operation | Lvalue operand |
|---|---|---|
| HashKeyStore | `h{key} = val` (with COW) | operands[0] (hash) |
| ListIndexStore | `l[i] = val` (with COW) | operands[0] (list) |

## Rules for future mutation opcodes

When adding any opcode that modifies a container in-place (e.g., `HashRemove`, `ListSort`, etc.):

1. **Lowering** (`QoreIRLowering.cpp`):
   - Emit `LoadLocal(container, auto_ref=false)` when fetching the container for mutation.
   - This is the lowering layer's responsibility; downstream layers trust this contract.

2. **Interpreter handler** (`QoreIRInterpreter.cpp`):
   - When processing `LoadLocal` with `auto_ref=false`, do NOT add the result slot to `cleanup` or `local_load_slots`.
   - These slots hold owned references; borrowed references (from `auto_ref=false` LoadLocal) must not be scheduled for deref.
   - **Example**:
     ```cpp
     if (out.hasNode() && local_inst->auto_ref) {
         cleanup.push_back(local_inst->result.id);
     }
     if (local_inst->local && local_inst->auto_ref) {
         local_load_slots[local_inst->local].insert(local_inst->result.id);
     }
     ```

3. **Interpreter COW path**:
   - When COW creates a new copy and updates the LocalVar:
     - Invalidate any cached references (both `locals` cache and `locals_slot_cache`).
     - Call `assignLocalVarValue(lv, new_copy, xsink)` to write the new copy to the thread-local stack.
     - Use **direct slot assignment** (not `setValueSlot`) to update `values[]`:
       ```cpp
       values[container_operand_id] = QoreValue(new_copy->refSelf());
       cleanup.push_back(container_operand_id);  // Now owns a +1 ref
       ```
     - Why not `setValueSlot`? Because `setValueSlot` discards the old slot (which was never ref'd by `auto_ref=false` LoadLocal), causing a spurious deref of a reference that was never taken.

4. **JIT handler** (`QoreIRToLLVM.cpp`):
   - Call the COW store helper (e.g., `qore_rt_hash_key_store_cow`).
   - **After the call**, reload the container from the LocalVar and update `values[container_operand_id]`:
     ```cpp
     // Use qore_rt_load_local (JIT) or qore_rt_load_local_aot (AOT)
     llvm::Value* updated_container = builder->CreateCall(load_fn, {...});
     values[container_operand_id] = updated_container;
     nanboxed_values.insert(container_operand_id);
     trackResultForCleanup(updated_container, container_operand_id, llvm_func);
     ```
   - Why? For single-pass operations like `h{"x"} += 3`, there is no next LoadLocal to reload the hash. We must update `values[]` immediately so subsequent IR instructions see the COW copy.

5. **JIT runtime helpers** (`JITRuntime.cpp`):
   - Implement both JIT and AOT variants (suffixed with `_cow` and `_cow_aot`).
   - **JIT path** signature: `uint64_t fn(LocalVar* var, uint64_t container_bits, ..., ExceptionSink*)`
   - **AOT path** signature: `uint64_t fn(QoreAOTContext* ctx, uint32_t local_slot, uint64_t container_bits, ..., ExceptionSink*)`
   - Both must:
     - Check `is_unique()` on the container.
     - If not unique, call `copy()` to create a copy.
     - Update the LocalVar / context slot with the copy (JIT: `qore_rt_assign_local`; AOT: `qore_rt_assign_local_aot`).
     - Apply the mutation to the (possibly new) container.
     - Return the new value bits (or a void indicator).

6. **AOT feature flag** (`QoreAOT.cpp`, `QoreAOTBinary.h`):
   - Define a new feature flag (e.g., `QORE_AOT_FEAT_LIST_MUTATION = 1ULL << N`).
   - Add routing in `opcodeToFeatureFlag()` to return the new flag for the mutation opcode.
   - Add slot pre-registration in the pre-pass loop to call `slots.getLocalSlot()` for the container's LocalVar pointer.
   - This ensures the AOT context allocates a slot for the container so the COW path can update it.

7. **Verification** (`QoreIRVerifier.cpp`):
   - Add the new opcode and its operand count to `expectedOperands()`.
   - If the opcode has 2+ operands, ensure verifier checks they are valid IR value IDs.

## Refcount Accounting

### No-COW Path
```
h = {...}  // container created, refcount = 1 (thread-local holds it)
LoadLocal(h, auto_ref=false) → returns raw pointer, no +1 taken
is_unique() → true (refcount still 1)
h->setKeyValue(...) → modifies in place
values[h] has no cleanup entry
Function exit: thread-local deref (refcount → 0, freed) ✓
```

### COW Path
```
h = {...}  // container created, refcount = 1
LoadLocal(h, auto_ref=false) → returns raw pointer
is_unique() → false (refcount > 1 due to external alias)
new_h = h->copy() → refcount = 1 (new object, thread-local will hold it)
qore_rt_assign_local(lv, new_h) → thread-local takes +1 (refcount = 2)
values[h_operand_id] = new_h->refSelf() → refcount = 3, cleanup.push_back(h_operand_id)
new_h->setKeyValue(...) → modifies copy in place
Function exit:
  - cleanupValues discards values[h_operand_id] → refcount = 2
  - UninstantiateLocal via lvar->del() drops thread-local → refcount = 1
  - Original h_operand_id value (old hash) eventually deref'd → refcount = 0, freed ✓
```

## Examples

### HashKeyStore (current)
- Lowering: `LoadLocal(h, auto_ref=false)` + `HashKeyStore(h, key, val)`
- Interpreter: auto_ref=false LoadLocal → no cleanup, no local_load_slots entry
- Interpreter COW: direct assign `values[h_op_id] = new_h->refSelf()` + cleanup.push_back
- JIT: call `qore_rt_hash_key_store_cow`, then reload hash from LocalVar into values[]

### ListIndexStore (current)
- Lowering: `LoadLocal(l, auto_ref=false)` + `ListIndexStore(l, i, val)`
- Interpreter: same as HashKeyStore, but calls `l->setEntry(i, val)`
- JIT: call `qore_rt_list_index_store_cow`, then reload list from LocalVar into values[]

### Future: HashRemove
- Lowering: `LoadLocal(h, auto_ref=false)` + `HashRemove(h, key)`
- Interpreter: check unique, COW if needed, call `h->removeKey(key)`
- JIT: helper `qore_rt_hash_remove_cow`, reload after call

---

## Slot Cache Invalidation Rule for Lvalue Operations

### The Problem

The `locals_slot_cache` (Phase 3 vector-based fast cache) holds a `refSelf()` reference to
local variable values. This extra reference inflates the refcount just like `auto_ref=true`
LoadLocal does. When any operation modifies a variable through lvalue semantics
(`LValueHelper`), `ensureUnique()` sees refcount > 1 and triggers COW:

1. A copy of the container is created
2. The copy is assigned as the variable's new value
3. The modification (push, remove, trim, +=, etc.) is applied to the **copy**
4. The slot cache still holds the **original** (stale, unmodified) value
5. The next LoadLocal reads from the stale slot cache → **mutation is lost**

### The Rule

**For ALL operations that modify a local variable through lvalue semantics, the slot cache
(and the `locals` map cache) MUST be invalidated BEFORE the modification, not after.**

Invalidating after the modification is too late — COW has already created a copy and the
slot cache retains the stale original. Invalidating before ensures:
- The variable's refcount is not inflated by the cache
- `is_unique()` reflects the true sharing state
- In-place modification works correctly without unnecessary copies

### Affected Code Paths

1. **Compound assignment lvalue opcodes** (`AddAssignLValue`, `SubAssignLValue`,
   `MulAssignLValue`, `DivAssignLValue`, `ModAssignLValue`, `AndAssignLValue`,
   `OrAssignLValue`, `XorAssignLValue`, `ShlAssignLValue`, `ShrAssignLValue`,
   `UnshiftLValue`):
   - These call `evalLValueBinary()` which uses `LValueHelper` internally
   - Invalidate all caches BEFORE calling `evalLValueBinary()`

2. **AST-evaluated lvalue opcodes** (`RemoveAny`, `TrimAny`, `ChompAny`,
   `RegexSubstAny`, `PopAny`, `PushAny`, `ListAssignAny`, `TransliterateAny`,
   `ExtractAny`, etc.):
   - These fall through to `evalExpr()` which evaluates the AST expression
   - The AST expression internally uses `LValueHelper` for in-place modification
   - Invalidate all caches BEFORE calling `evalExpr()`

3. **Dedicated lvalue opcodes** (`ShiftLValue`, `SpliceLValue`):
   - These have their own handlers that may use `LValueHelper` internally
   - Invalidate the specific variable's cache entry BEFORE the operation
   - For `ShiftLValue`: only cache invalidation is needed (no re-assignment)

4. **Invoke instructions with lvalue opcodes** (try/catch blocks):
   - Inside try/catch blocks, the lowering emits `Invoke` instructions with
     `invoke_opcode` set to the lvalue opcode (e.g., `AddAssignLValue`)
   - `evalInvoke()` has **native handling** for these opcodes — it extracts
     the lvalue from the AST expression node and calls `evalLValueBinary()`,
     `evalLValueUnary()`, or `evalLValueTernary()` with pre-evaluated operands
   - The Invoke handler in the main loop invalidates all caches BEFORE calling
     `evalInvoke()` for lvalue invoke_opcodes
   - **Critical**: without native handling, the default `evalExpr()` fallthrough
     re-evaluates the full AST expression, which re-evaluates the RHS and loses
     the IR operand value, and the slot cache references cause COW to silently
     discard mutations

#### Native Invoke Lvalue Handling (evalInvoke)

Binary lvalue opcodes extract the lvalue from `QoreBinaryOperatorNode<LValueOperatorNode>`
via `getLeft()` and use `inv->operands[0]` for the pre-evaluated RHS:
```cpp
case QoreIROpcode::AddAssignLValue: // ... and all binary lvalue opcodes
{
    auto* binop = dynamic_cast<const QoreBinaryOperatorNode<LValueOperatorNode>*>(
        inv->expr.getInternalNode());
    QoreValue right = getIRValue(values, inv->operands[0]);
    return QoreIRInterpreter::evalLValueBinary(op, binop->getLeft(), right, xsink);
}
```

Unary lvalue opcodes extract the lvalue from `QoreSingleExpressionOperatorNode<LValueOperatorNode>`
via `getExp()`:
```cpp
case QoreIROpcode::ShiftLValue: // PreInc, PostInc, PreDec, PostDec
{
    auto* unaryop = dynamic_cast<const QoreSingleExpressionOperatorNode<LValueOperatorNode>*>(
        inv->expr.getInternalNode());
    return QoreIRInterpreter::evalLValueUnary(op, unaryop->getExp(), xsink);
}
```

Ternary lvalue opcodes (SpliceLValue) extract the lvalue from `QoreSpliceOperatorNode`
via `getLValue()`:
```cpp
case QoreIROpcode::SpliceLValue:
{
    auto* spliceop = dynamic_cast<const QoreSpliceOperatorNode*>(
        inv->expr.getInternalNode());
    return QoreIRInterpreter::evalLValueTernary(op, spliceop->getLValue(),
        offset, length, replacement, xsink);
}
```

### Implementation Pattern

```cpp
// BEFORE any lvalue-modifying evaluation:
cleanupStoredValues(locals, nullptr);
cleanupStoredValues(globals, nullptr);
cleanupStoredValues(threadlocals, nullptr);
cleanupStoredValues(closures, nullptr);
for (size_t i = 0; i < locals_slot_cache.size(); ++i) {
    locals_slot_cache[i].discard(nullptr);
    locals_slot_cache[i] = QoreValue();
}

// THEN perform the lvalue operation:
QoreValue res = evalLValueBinary(...);  // or evalExpr(...)
```

### Interaction with auto_ref=false

The `auto_ref=false` contract and the slot cache invalidation rule address the same
fundamental problem (refcount inflation defeating COW) at different levels:

- `auto_ref=false` prevents refcount inflation in the `values[]` array (IR operand slots)
- Slot cache invalidation prevents refcount inflation in the `locals_slot_cache`
- Both must be respected for correct lvalue semantics

---

## ClosureVarValue::eval() — Owned Reference Asymmetry

`LocalVarValue::eval(bool& needs_deref, ...)` and `ClosureVarValue::eval(bool& needs_deref, ...)`
handle non-reference values differently:

- **`LocalVarValue::eval()`** calls `val.getReferencedValue(needs_deref)` — the parameterized
  version sets `needs_deref=false` for `QV_Node` types, returning a **borrowed** reference
  (no refcount change).

- **`ClosureVarValue::eval()`** calls `val.getReferencedValue()` — the no-parameter version
  always does `v.n->ref()`, returning an **owned** reference (`needs_deref` stays true).
  This is intentional for thread safety: closure variables may be shared across threads,
  so a borrowed reference could become dangling if another thread modifies the variable
  after the read lock is released.

### Impact on IR

When a reference parameter points to a closure variable (VT_IMMEDIATE), the IR LoadLocal
with `auto_ref=true` receives an owned reference from `eval()`.  If the code does not
account for `needs_deref=true`, the owned reference leaks:

- `storeValue()` does `refSelf()` (+1 for locals map)
- Slot cache does `refSelf()` (+1 for slot cache)
- `out = val.refSelf()` (+1 for values[])
- `val`'s owned reference from eval() is **never discarded** → permanent leak

**Fix**: After caching, when `needs_deref=true`, deref `val` to release the owned ref:
```cpp
if (needs_deref && val.hasNode()) {
    val.getInternalNode()->deref(nullptr);
}
```

---

## StoreLValue Pre-Invalidation — values[] Cleanup

The `StoreLValue` handler (and the Invoke handler for lvalue opcodes) invalidates all
caches (locals map, globals, threadlocals, closures, slot cache) **before** calling
`evalLValueStore()`.  However, `values[]` entries from guard LoadLocal instructions
also hold +1 references that inflate the refcount.

**Fix**: Extract the base VarRefNode from the lvalue expression using
`extractLValueBaseVarRef()`, then discard all `values[]` entries tracked in
`local_load_slots` for that variable:

```cpp
const VarRefNode* base_var = extractLValueBaseVarRef(lval_inst->lvalue);
if (base_var && base_var->ref.id) {
    auto it = local_load_slots.find(base_var->ref.id);
    if (it != local_load_slots.end()) {
        for (uint32_t slot_id : it->second) {
            if (slot_id < values.size()) {
                values[slot_id].discard(nullptr);
                values[slot_id] = QoreValue();
            }
        }
    }
}
```

This is safe because:
- Guard LoadLocal results are consumed by GuardNotNothing/BrIf before StoreLValue executes
- `cleanupValues()` at function exit will no-op on NOTHING entries in the cleanup vector
- For Invoke StoreLValue, `inv->expr` is the full assignment expression (e.g.,
  `QoreAssignmentOperatorNode`); `extractLValueBaseVarRef()` recurses through operator
  nodes via `QoreBinaryLValueOperatorNode::getLeft()` to find the base VarRefNode

---

## SlotCacheCleanup RAII Guard

The `locals_slot_cache` vector holds `refSelf()` references to local variable values
for O(1) cache access.  `std::vector<QoreValue>` destructor does NOT call `discard()`
on its elements.  Without cleanup, every IR function execution leaks all cached references.

**Fix**: RAII guard constructed after `LocalInstantiationCleanup` so it's destroyed BEFORE
it — slot cache refs are released while the underlying locals are still valid:

```cpp
struct SlotCacheCleanup {
    std::vector<QoreValue>& cache;
    ~SlotCacheCleanup() {
        for (auto& v : cache) {
            v.discard(nullptr);
        }
    }
} slot_cache_cleanup{locals_slot_cache};
```

This single guard covers ALL exit paths (Return, ReturnNothing, exception, error bailouts).

---

**Rationale**: The `auto_ref=false` parameter was introduced precisely to express "this is an lvalue read, not an rvalue read." Respecting this distinction at every level (lowering, interpreter, JIT, runtime) is essential for correct COW semantics and ensures mutations are not silently lost.
