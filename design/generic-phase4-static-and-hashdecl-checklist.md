# Generic Phase 4: Static Methods And Hashdecls Checklist

This checklist captures the remaining generic-type work that affects static
methods, generic hashdecls, and typed result records. It applies to the core
Qore repository first, then to binary module repositories such as
`~/src/qore/git/module-*` when module APIs are migrated.

## Static Generic Methods

Static methods in a generic class may use class type parameters only when the
call site supplies a parameterized class target:

```qore
class Box<T> {
    static Box<T> make(T value) {
        return new Box<T>(value);
    }
}

Box<int> b = Box<int>::make(1);
```

Implementation checklist:

- [x] Parse `Class<T>::method(...)` and `Namespace::Class<T>::method(...)` as static
  method calls, not as a parameterized type followed by an invalid scoped
  reference.
- [x] Resolve the receiver target as a parameterized class type and preserve that
  concrete type on the call node.
- [x] Use the concrete receiver type for static method overload selection,
  argument filtering, default arguments, return typing, local declarations, and
  body return checks.
- [x] Reject raw generic static calls when type parameters cannot be substituted,
  using an error that tells the user to call `Class<...>::method(...)`.
- [x] Reject instance-method calls through `Class<...>::method(...)` with an error
  that says the selected method is not static.
- [x] Preserve the receiver type through IR, JIT fallback, and AOT expression
  serialization. Direct static-call lowering may stay disabled for generic
  static calls until the IR has an explicit receiver-type operand.
- [x] Cover normal, IR, and JIT execution with tests that verify accepted and
  rejected argument and return types.
- [x] Add a focused source-stripped AOT test for parameterized static method
  calls after the AOT test harness can exercise this path directly.

## Generic Hashdecls And Result Records

Generic hashdecls introduce type parameters on hash declarations:

```qore
hashdecl Result<T> {
    string status;
    T value;
}

hash<Result<int>> r = {"status": "ok", "value": 1};
```

Implementation checklist:

- [x] Parse `hashdecl Name<T>` and scoped forms such as `hashdecl Ns::Name<T>`.
- [x] Store formal type-parameter metadata on `TypedHashDecl`.
- [x] Represent `hash<Name<int>>` as a distinct parameterized hashdecl type while
  preserving the base `TypedHashDecl*`.
- [x] Resolve type-parameter names in hashdecl member type positions.
- [x] Substitute hashdecl member types during parse-time initialization checks,
  assignment checks, casts, implicit hash construction, and runtime hashdecl
  casts.
- [x] Preserve parameterized hashdecl type identity on `QoreHashNode` values, not
  only the raw base hashdecl pointer.
- [x] Preserve parameterized hashdecl use sites through AOT type metadata.
- [x] Preserve source-defined generic hashdecl definitions in source-stripped
  AOT metadata.
- [x] Implement or explicitly reject parameterized parent hashdecl references;
  the initial implementation keeps existing non-generic hashdecl inheritance
  behavior.
- [x] Update reflection and API metadata so tools can distinguish raw
  `hash<Result>` from `hash<Result<int>>`.
- [x] Add tests for concrete member assignment, nested generic members, wrong
  member types, and normal/IR/JIT execution.
- [x] Add source-stripped AOT loading tests for generic hashdecl declarations
  and uses.

## Binary Module Rollout

When applying these changes to `~/src/qore/git/module-*`:

1. Search for hashdecls that describe result envelopes, iterator pairs, rows, or
   provider payloads with one logical value type.
2. Record the type-parameter meaning before editing, for example `T` for the
   payload and `Err` for an error-detail record.
3. Convert one public hashdecl family at a time and keep raw compatibility notes
   in the module release notes.
4. Prefer generic result records where they make action output or iterator
   payloads clearer without hiding heterogeneous provider metadata behind a
   misleading type parameter.
5. Update QPP metadata and examples so generated API metadata includes the
   concrete type spelling.
6. Run the module's focused tests plus any core tests that exercise the public
   API, then run `audit-changes` before committing.
