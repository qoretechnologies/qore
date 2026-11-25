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
- An original builtin declaration that a binary module adds to a namespace that a
  Program owns at run time (`QoreNamespace::addNamespace()`, `addSystemClass()`,
  `addSystemHashDecl()`, `addSystemEnum()` outside the static system namespace and the
  namespace of a module) is owned by that Program and is mortal: it is marked
  `program_local` before it is published (`qore_ns_private::markProgramLocal()`), and
  the declaration lifetime system treats it as a user declaration
  (`isTypeOwnerSystem()` is false; `isSystem()` is unchanged). The import copy of a
  builtin class and the instances of a mortal generic hashdecl are mortal too. A
  builtin variant of a function of such a namespace references the mortal
  declarations that its signature names
  (`BuiltinFunctionVariantBase::refMortalDecls()`), so that the variant can be
  imported into another Program or copied into a child Program that does not inherit
  the classes and outlive the Program; such a declaration can therefore outlive its
  Program (qore#7540). One that something keeps to the end of the process (a value
  kept in static storage, a Program with static storage duration) is freed after
  the modules' `del` functions have run, as the static namespace's module classes
  are: binary modules are never unloaded, and a module's `del` function must leave
  what the destructors, methods and module data of its classes need valid until then
  (`doxygen/lib/70_qore_modules.doxygen`, Releasing Module State at Shutdown;
  `examples/test/qore/namespaces/runtime-builtin-survivor`).

The accessors follow the existing namespace back-pointer; they add exported
methods without changing the public object layout. No reference is acquired.
Namespace assignment also binds generic instances cached before the base declaration
was attached, as can happen during C++ module initialization.
Namespace teardown clears enum back-pointers and visits the generic hashdecl
cache as well as the declarations directly in the namespace's hashdecl list.
These cleanup traversals must complete even after cancellation.

Enum values retain their declaration, including its members and type information
(issue #5519). Their inline member pointer still needs no value-node allocation,
but an owned `QoreValue` now acquires/releases a declaration reference through
`ref()`/`discard()`. `makeEnum()` creates an owned value; `makeEnumBorrowed()` is
only for borrowed views whose declaration is already retained. `hasNode()` still
describes node storage, while `needsRef()` includes enums for ownership and cleanup
bookkeeping in the interpreter and JIT/AOT runtime. Lvalues transfer or acquire
the same ownership, and release it when overwritten or converted to a scalar.

An enum declaration retains its source location strings and namespace path after
the namespace is gone; `getProgram()` and `getNamespace()` then return null.
Keeping a value alive does not keep the Program alive. Binary modules that create
or transfer enum values using earlier development headers must be rebuilt for the
new ownership contract.

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
rollback, callback ordering, late namespace assignment, and hash and enum values
surviving namespace teardown. The `declaration-owner.qtest` driver runs this
executable in the normal test suite. `examples/test/qore/enums/enum-lifetime.qtest`
checks escaped enum values, copies, scalar conversions, repeated calls and casts,
nested Programs, and exception cleanup across the execution modes.

The module-python follow-up must use the protocol for its hashdecl and enum type
caches, class wrappers, and static-member descriptors, and add the issue's saved
Python-type reproducer. Adding the accessors to Qore does not itself change those
Python caches.
