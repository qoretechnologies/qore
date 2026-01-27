# Reflection Type Object Reference Management (Issue #4816)

This document describes the reference management strategy for `Reflection::Type` objects to prevent crashes when modules fail during initialization and to avoid memory leaks from reference cycles.

## Problem Statement

When a Qore module's `init()` function fails after registering `Type` objects in foreign modules (e.g., DataProvider registries), those `Type` objects can hold dangling pointers to freed type data, causing crashes.

**Scenario:**
1. Module A loads and registers type providers in DataProvider (a foreign module)
2. Module A's `init()` fails and the module is destroyed
3. Module A's program data (including type info) is freed
4. Type objects registered in DataProvider now have dangling `typeInfo` pointers
5. Later access to those Type objects causes a crash

## Failed Approaches

### Approach 1: Weak References (`depRef`/`depDeref`)

The initial attempt used weak program references:
```cpp
source_pgm->depRef();   // keep program object alive, but data can be freed
```

**Problem:** `depRef` keeps the `QoreProgram` object alive but allows its data to be cleared. When the Type is accessed later, `typeInfo` points to freed memory, causing invalid reads detected by valgrind.

### Approach 2: Strong References for All Types

The second attempt used strong references unconditionally:
```cpp
source_pgm->ref();   // keep program and its data alive
```

**Problem:** Creates reference cycles for Type objects stored in their source program's globals:
```
Program -> globals -> Type QoreObject -> QoreType -> Program (cycle!)
```

This caused 5MB+ memory leaks in DataProvider tests because programs with locally-stored Type objects could never be freed.

## Solution: Hybrid Reference Strategy

The solution uses different reference types based on where the Type object is stored:

### Same-Program Storage (Weak Ref)
When a Type object is stored in the same program that owns the type data:
- Use weak ref (`depRef`) to avoid creating a cycle
- The Type will be destroyed when the program's globals are cleared anyway
- No need to keep the program alive via this path

### Cross-Program Storage (Strong Ref)
When a Type object escapes to a foreign program (e.g., registered in DataProvider):
- Use strong ref (`ref`) to keep the source program's type data alive
- Without this, the type data would be freed while the Type object still exists
- A cleanup callback breaks cycles before program data is cleared

### Detection Logic
The storage location is determined by comparing the containing object's program with the source program:
```cpp
if (container->getProgram() == source_pgm) {
    // Same program - use weak ref
    source_pgm->depRef();
} else {
    // Different program - use strong ref
    source_pgm->ref();
}
```

## Implementation Details

### Key Files

1. **`modules/reflection/src/QC_Type.h`** - Class definition with reference fields
2. **`modules/reflection/src/QC_Type.qpp`** - Reference management implementation
3. **`lib/QoreProgram.cpp`** - Cleanup callback mechanism
4. **`lib/ModuleManager.cpp`** - Module destruction changes
5. **`modules/reflection/src/reflection-module.cpp`** - Callback registration

### QoreType Class

```cpp
class QoreType : public AbstractPrivateData {
public:
    const QoreTypeInfo* typeInfo;   // pointer to type data
    QoreProgram* source_pgm;        // program that owns the type
    QoreObject* container;          // back-pointer for cycle detection
    bool ref_held;                  // whether we hold a ref
    bool strong_ref;                // strong (true) or weak (false)

    void setContainer(QoreObject* obj);  // called after wrapping in QoreObject
    void releaseSourceRef();              // release ref (for cycle breaking)
};
```

### Global Registry

A registry tracks Type objects with strong refs by source program:
```cpp
static std::map<QoreProgram*, std::set<QoreType*>> type_registry;
```

This enables efficient lookup during program destruction.

### Cleanup Callback

Before program data is cleared, a callback breaks cycles:
```cpp
void qore_release_local_type_refs(QoreProgram* pgm) {
    // Find Type objects that:
    // 1. Have a strong ref to pgm
    // 2. Are stored in pgm's globals (container->getProgram() == pgm)
    // Release their strong refs to break the cycle
}
```

**Important:** This only releases refs for Types stored in the same program. Types that escaped to foreign programs keep their strong refs, ensuring type data remains valid.

### Module Destruction

Changed from:
```cpp
pgm->waitForTerminationAndDeref(&xsink);  // data freed before deref returns
```

To:
```cpp
pgm->waitForTermination();
pgm->deref(&xsink);  // triggers clear(), which calls cleanup callback
```

This ensures the cleanup callback runs before type data is freed.

## Lifecycle

### Type Creation
1. `QoreType` constructed with `typeInfo` and `source_pgm`
2. `QoreObject` wrapper created
3. `setContainer()` called to acquire appropriate ref and register if strong

### Type Access
- Normal operation - `typeInfo` is always valid because:
  - Same-program Types: program still active (weak ref is sufficient)
  - Cross-program Types: strong ref keeps source program alive

### Program Destruction
1. Program's `deref()` called
2. `waitForTerminationAndClear()` called internally
3. Cleanup callback releases strong refs for same-program Types
4. Program data cleared (namespace data, etc.)
5. Globals cleared (destroys same-program Type objects)
6. Cross-program Types (if any) keep source program alive until they're destroyed

### Type Destruction
1. Destructor removes from registry (if strong ref)
2. Releases ref (`deref` for strong, `depDeref` for weak)

## Testing

The solution passes:
- All 409 existing tests
- Valgrind shows 0 memory leaks, 0 errors
- DataProvider tests (which register types in foreign modules)
- Reflection tests (which create Type objects locally)

## See Also

- GitHub Issue #4816: module init failure can leave dirty data in foreign modules
- `include/qore/QoreProgram.h` - `ref()`/`deref()` vs `depRef()`/`depDeref()` documentation
