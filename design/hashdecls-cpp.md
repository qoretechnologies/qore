# Hashdecls in C++ (Design Notes)

This document describes how to add and use typed hash declarations (hashdecls) in the Qore core when the implementation and construction happen in C++.

## Goals

- Make system hashdecls discoverable in C++ and Qore.
- Ensure the hashdecl is registered in the root Qore namespace.
- Clarify the correct C++ constructor for typed hashes.
- Highlight when hashdecl inheritance is worth refactoring.

## Example: StatInfo

The `StatInfo` hashdecl is defined in Qore source (see `lib/ql_file.qpp`) and exported to C++ as a system hashdecl.

### Registration points

1. **Public declaration in `TypedHashDecl.h`**
   Add an exported pointer for the hashdecl so C++ code can reference it.

   Example (already present for `StatInfo`):
   ```
   // include/qore/TypedHashDecl.h
   DLLEXPORT extern const TypedHashDecl* hashdeclStatInfo;
   ```

2. **Initialization in `QoreNamespace.cpp`**
   Register the hashdecl in the root `Qore` namespace during startup.

   Example (already present for `StatInfo`):
   ```
   // lib/QoreNamespace.cpp
   hashdeclStatInfo = init_hashdecl_StatInfo(qns);
   ```

3. **Init function declaration**
   The init function is declared in the corresponding internal header, e.g.:
   ```
   // include/qore/intern/ql_file.h
   DLLLOCAL TypedHashDecl* init_hashdecl_StatInfo(QoreNamespace& ns);
   ```

### C++ usage: creating a typed hash

To create a hash of a declared type (like `StatInfo`), use the constructor that takes a `TypedHashDecl*` and an `ExceptionSink*`:

```cpp
// Correct: typed hash with StatInfo layout and defaults
ReferenceHolder<QoreHashNode> h(new QoreHashNode(hashdeclStatInfo, xsink), xsink);
```

Avoid using the constructor that takes only `QoreTypeInfo*` for hashdecl instances. That path creates a value-typed hash (for example `hash<string, hash<...>>`), which is not the same as `hash<StatInfo>` and bypasses the hashdecl member initialization.

## Adding a new system hashdecl (checklist)

1. **Define the hashdecl in Qore source** (`lib/*.qpp` or `qlib/*.qm` depending on where the feature lives).
2. **Expose a global pointer** in `include/qore/TypedHashDecl.h`.
3. **Add an init function** in the relevant internal header (for example, `include/qore/intern/ql_<area>.h`).
4. **Register it** in `lib/QoreNamespace.cpp` so it is created in the root namespace.
5. **Use the typed-hash constructor** in C++ wherever you need instances.

## Inheritance: when to refactor

Typed hashdecls can inherit from each other in the runtime (see `TypedHashDecl::setParent()` and `TypedHashDecl::inheritsFrom()`), but system hashdecls have to wire this up explicitly in their init code.

Refactoring to use inheritance can benefit users when:
- A derived hashdecl is a strict superset of a base hashdecl.
- You want runtime compatibility checks to treat derived hashes as the base type.
- You want to reduce member duplication and keep docs consistent.

Example: `DirStatInfo`
- `DirStatInfo` previously duplicated most of `StatInfo` members.
- It has been refactored so that the init function for `DirStatInfo` sets its parent to `hashdeclStatInfo`, enabling `inheritsFrom()` checks and runtime casts.

When applying this pattern to other hashdecls, keep the Qore source declaration and the C++ init wiring in sync, and verify that any API expecting `hash<BaseDecl>` should accept `hash<DerivedDecl>` where appropriate.

## Common pitfalls

- Forgetting to add the exported pointer in `include/qore/TypedHashDecl.h` makes the hashdecl inaccessible to C++ code.
- Registering in `lib/QoreNamespace.cpp` after dependent code is initialized can lead to null or stale pointers.
- Using the `QoreHashNode(const QoreTypeInfo* valueTypeInfo)` constructor for hashdecls results in a `hash<string, hash<...>>` value-typed hash rather than a `hash<YourDecl>`.
