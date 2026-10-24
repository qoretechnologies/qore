# Declaration ownership for binary modules

Issue: <https://github.com/qoretechnologies/qore/issues/5517>.

`TypedHashDecl::getProgram()` and `QoreEnumDecl::getProgram()` expose the same
namespace ownership as `QoreClass::getProgram()`. A language bridge can record
the owner while a declaration is alive and invalidate its foreign-language
handles in `qore_register_program_cleanup_callback()` before Qore frees the
Program's namespace data.

## Ownership rules

- A declaration parsed into a Program reports that Program, including declarations
  in nested namespaces and concrete instances of generic hashdecls.
- A declaration copied into a child Program or imported into another Program
  reports the receiving Program. Identity with the original declaration does
  not imply the same allocation or lifetime.
- Static system declarations have no Program owner. Copies of builtin declarations
  in a Program do have that Program as their owner, even though `isSystem()` is true.
- An unattached declaration has no Program owner. A reference-counted declaration
  that survives its namespace also has no owner after namespace teardown.
  Consequently, a null result alone does not prove process-wide lifetime.

The accessors follow the existing namespace back-pointer; they add exported
methods without changing the public object layout. No reference is acquired.
Namespace assignment also binds generic instances cached before the base declaration
was attached, as can happen during C++ module initialization.
Namespace teardown clears enum back-pointers and visits the generic hashdecl
cache as well as the declarations directly in the namespace's hashdecl list.
These cleanup traversals must complete even after cancellation.

## Consumer protocol

1. Register a Program cleanup callback before publishing cached types.
2. While the declaration's Program is alive, capture the declaration pointer and
   its `getProgram()` result together in a handle. Use the actual declaration's
   owner, which can differ from the bridge's own Program.
3. Synchronize handle publication and use with the cleanup callback. A module's
   cache lock alone does not retain Qore data; operations on a live Program must
   use the normal Qore Program execution or parse context, or otherwise prevent
   concurrent teardown.
4. In the callback, invalidate all handles for that Program before releasing the
   lock. Remove pointer-keyed cache entries, and also clear handles reachable from
   retained foreign types, instances, or descriptors. Removing only the map entry
   leaves the original dangling-pointer problem in those retained objects.
5. Reject later operations on invalid handles with the foreign language's normal
   exception mechanism. Do not call `getProgram()` on a cached pointer to check
   whether it is still alive.
6. Deregister the callback before unloading the module's code.

This API provides ownership information, not a general declaration reference or
an execution-lifetime guarantee. Exporting the existing internal reference counters
alone would keep the declaration allocation alive without necessarily retaining
the namespace, source locations, default expressions, and other referenced types.

## Coverage and Python follow-up

`examples/test/qore/misc/declaration_owner.cpp` exercises the public C++ API with
AST, IR, JIT, and tiered Programs. It checks nested declarations, child copies, explicit imports,
static and copied builtins, unattached copies, concrete generic instances, parse
rollback, callback ordering, late namespace assignment, and hash values surviving
namespace teardown. The `declaration-owner.qtest` driver runs this executable in
the normal test suite.

The module-python follow-up must use the protocol for its hashdecl and enum type
caches, class wrappers, and static-member descriptors, and add the issue's saved
Python-type reproducer. Adding the accessors to Qore does not itself change those
Python caches.
