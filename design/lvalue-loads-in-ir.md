# Lvalue Loads in Qore IR — The auto_ref=false Contract

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

**Rationale**: The `auto_ref=false` parameter was introduced precisely to express "this is an lvalue read, not an rvalue read." Respecting this distinction at every level (lowering, interpreter, JIT, runtime) is essential for correct COW semantics and ensures mutations are not silently lost.
