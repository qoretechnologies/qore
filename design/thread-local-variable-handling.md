# Thread-Local Variable Handling (Design Notes)

This document describes how Qore manages thread-local variables, with particular focus on the two-stack architecture for local and closure-captured variables, and the destruction order guarantees.

## Overview

Qore maintains thread-local variable storage through `ThreadLocalProgramData`, which contains two separate stacks:

- **`lvstack`** (`ThreadLocalVariableData`): Regular local variables
- **`cvstack`** (`ThreadClosureVariableStack`): Closure-captured variables

This separation exists because closure-captured variables have different lifetime and reference-counting semantics than regular local variables.

## Key Data Structures

### ThreadLocalProgramData

Located in `include/qore/intern/qore_program_private.h`:

```cpp
struct ThreadLocalProgramData {
    ThreadLocalVariableData lvstack;        // local variable stack
    ThreadClosureVariableStack cvstack;     // closure variable stack
    uint64_t var_order_counter = 0;         // declaration order tracking
    // ...
};
```

### LocalVarValue and ClosureVarValue

Both inherit from `VarValueBase` (in `include/qore/intern/LocalVar.h`):

```cpp
class VarValueBase {
public:
    QoreLValueGeneric val;
    const char* id;
    uint64_t decl_order = 0;    // tracks declaration order (used by LocalVarValue)
    bool finalized : 1;
    bool frame_boundary : 1;
    // ...
};
```

- `LocalVarValue`: Used for regular local variables, stored directly in `lvstack`. The `decl_order` field is used directly.
- `ClosureVarValue`: Reference-counted, can be shared between closures and threads. Stored via `ClosureStackEntry` in `cvstack`.

### ClosureStackEntry (Thread-Safety Fix)

The `cvstack` stores `ClosureStackEntry` structs (in `include/qore/intern/ThreadClosureVariableStack.h`):

```cpp
struct ClosureStackEntry {
    ClosureVarValue* cvv;
    uint64_t decl_order;
    // ...
};
```

**Important**: The declaration order is stored per-stack-entry, NOT on the shared `ClosureVarValue` object. This is critical because:

1. `ClosureVarValue` objects are reference-counted and can be shared across multiple closures and threads
2. The same `ClosureVarValue` can be pushed onto multiple stacks (different threads, or multiple times in the same thread)
3. Storing the order on the shared object would cause race conditions and incorrect destruction ordering

### LocalVar (Parse-Time)

The `LocalVar` class (in `include/qore/intern/LocalVar.h`) represents a variable at parse time and tracks whether it's captured by a closure:

```cpp
class LocalVar {
    std::string name;
    bool closure_use = false;   // true if captured by any closure
    // ...
};
```

When `closure_use` is true, the variable is instantiated on `cvstack` instead of `lvstack`.

## Variable Lifecycle

### Instantiation

Variables are instantiated via functions in `lib/thread.cpp`:

```cpp
// For regular local variables
LocalVarValue* thread_instantiate_lvar() {
    ThreadLocalProgramData* tlpd = thread_data.get()->tlpd;
    LocalVarValue* var = tlpd->lvstack.instantiate();
    var->setDeclOrder(tlpd->getNextVarOrder());  // track declaration order
    return var;
}

// For NEW closure-captured variables
ClosureVarValue* thread_instantiate_closure_var(const char* n_id, ...) {
    ThreadLocalProgramData* tlpd = thread_data.get()->tlpd;
    uint64_t order = tlpd->getNextVarOrder();
    // Order is passed to stack entry, not stored on the shared object
    ClosureVarValue* cvv = tlpd->cvstack.instantiate(n_id, typeInfo, nval, assign, order);
    return cvv;
}

// For EXISTING closure-captured variables (e.g., shared across threads)
void thread_instantiate_closure_var(ClosureVarValue* cvar) {
    ThreadLocalProgramData* tlpd = thread_data.get()->tlpd;
    // Order is stored per-stack-entry, NOT on the shared ClosureVarValue
    uint64_t order = tlpd->getNextVarOrder();
    tlpd->cvstack.instantiate(cvar, order);
}
```

The `LocalVar::instantiateIntern()` method decides which stack to use:

```cpp
void LocalVar::instantiateIntern(QoreValue nval, bool assign) {
    if (!closure_use) {
        LocalVarValue* val = thread_instantiate_lvar();
        val->set(name.c_str(), typeInfo, nval, assign, false);
    } else {
        thread_instantiate_closure_var(name.c_str(), typeInfo, nval, assign);
    }
}
```

### Block Scope Destruction

At block scope exit, variables are uninstantiated one-by-one in LIFO order via `LocalVar::uninstantiate()`. This naturally produces correct reverse-declaration-order destruction.

### Program Scope Finalization

At program termination, `ThreadLocalProgramData::finalize()` handles destruction. This is where issue #5168 manifested.

## Issue #5168: Destruction Order Bug

### The Problem

Prior to the fix, `finalize()` was implemented as:

```cpp
void finalize(SafeDerefHelper& sdh) {
    lvstack.finalize(sdh);  // All lvstack variables first
    cvstack.finalize(sdh);  // Then all cvstack variables
}
```

This caused closure-captured variables to be destroyed **after** all regular local variables, violating reverse-declaration-order semantics.

**Example:**
```qore
Tracked t1("t1");
Tracked t2("t2");     // captured by closure -> goes to cvstack
code closure = sub() { t2.getId(); };
Tracked t3("t3");
```

Expected destruction: `t3, t2, t1`
Actual (buggy): `t3, t1, t2` (t2 destroyed last because it's on cvstack)

### Real-World Impact

This caused deadlocks with ZMQ and similar libraries where destruction order matters:

```qore
ZContext ctx();           // context
ZSocketClient cli(ctx);   // socket depends on context
code closure = sub() { cli.send("test"); };  // captures cli
// At exit: ctx destroyed before cli -> zmq_ctx_term() blocks forever
```

### The Fix

The fix introduces declaration order tracking with proper thread-safety:

1. **Counter in ThreadLocalProgramData**: `var_order_counter` increments on each variable instantiation
2. **Order storage**:
   - For `LocalVarValue`: stored in the `decl_order` field of the object (safe, not shared)
   - For `ClosureVarValue`: stored in the `ClosureStackEntry` struct (per-stack-entry, thread-safe)
3. **Sorted finalization**: Collect all variables, sort by `decl_order` descending, finalize in that order

```cpp
void finalize(SafeDerefHelper& sdh) {
    std::vector<std::pair<uint64_t, QoreValue>> ordered_values;

    lvstack.collectForFinalize(ordered_values);
    cvstack.collectForFinalize(ordered_values);  // Uses per-entry order

    // Sort by declaration order descending (reverse declaration order)
    std::sort(ordered_values.begin(), ordered_values.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });

    for (auto& ov : ordered_values) {
        sdh.deref(ov.second);
    }
}
```

## Frame Boundaries

Both stacks support frame boundaries for function calls:

- `pushFrameBoundary()`: Marks the start of a new scope
- `popFrameBoundary()`: Removes the marker

Frame boundaries are represented as:
- `LocalVarValue` with `frame_boundary = true` in lvstack
- `ClosureStackEntry` with `cvv = nullptr` in cvstack

The stack iterators skip frame boundaries during finalization.

## Closure Variable Sharing

When a closure is created, it captures references to `ClosureVarValue` objects. Multiple closures can share the same captured variable. The `ClosureVarValue` uses reference counting to manage lifetime:

```cpp
struct ClosureVarValue : public VarValueBase, public RObject {
    mutable std::atomic_int references;
    // ...
};
```

When a closure is instantiated in a new context (e.g., passed to another thread), `thread_instantiate_closure_var(ClosureVarValue* cvar)` is called to push it onto the new thread's cvstack. The declaration order is stored in the stack entry, not on the shared object, ensuring thread-safety.

## Common Pitfalls

1. **Assuming destruction order without tracking**: Before #5168 fix, the two-stack architecture implicitly caused wrong destruction order at program scope.

2. **Storing order on shared objects**: Declaration order for closure variables must be stored per-stack-entry (in `ClosureStackEntry`), not on the shared `ClosureVarValue`. Mutating shared objects from multiple threads causes race conditions.

3. **Block scope vs program scope**: Block scope uses `uninstantiate()` which naturally reverses order. Program scope uses `finalize()` which must explicitly sort.

4. **Frame boundary handling**: When iterating stacks, always check for and skip frame boundaries (`ClosureStackEntry::isFrameBoundary()` or `LocalVarValue::frame_boundary`).

## Files Involved

- `include/qore/intern/LocalVar.h`: `VarValueBase`, `LocalVarValue`, `ClosureVarValue`, `LocalVar`
- `include/qore/intern/qore_program_private.h`: `ThreadLocalProgramData`
- `include/qore/intern/ThreadLocalVariableData.h`: `ThreadLocalVariableData` (lvstack)
- `include/qore/intern/ThreadClosureVariableStack.h`: `ClosureStackEntry`, `ThreadClosureVariableStack` (cvstack)
- `include/qore/intern/QoreLibIntern.h`: `ThreadBlock`, `ThreadLocalDataIterator`, `SafeDerefHelper`
- `lib/thread.cpp`: `thread_instantiate_lvar()`, `thread_instantiate_closure_var()`
- `lib/ThreadClosureVariableStack.cpp`: Frame/variable lookup methods
- `lib/QoreLib.cpp`: `ThreadBlock::frameBoundary()` specializations

## Testing

Comprehensive tests are in `examples/test/qore/closures/closure_destruction_order.qtest`, covering:

- Basic and edge-case capture patterns
- Multiple closures capturing same/different variables
- Nested blocks with closures
- Program scope destruction
- Dependent object destruction order (simulates ZMQ scenario)

## Related Issues

- Issue #5168: Original bug report for closure destruction order
- The ZMQ deadlock scenario that motivated the fix
