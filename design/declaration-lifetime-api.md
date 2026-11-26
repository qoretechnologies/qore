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
- A child Program references the mortal declarations of its parent
  (`isTypeOwnerSystem()` is false: user declarations and those of the next item)
  that the copy of the parent's namespaces did not copy (`PO_NO_INHERIT_USER_CLASSES`,
  `PO_NO_INHERIT_SYSTEM_CLASSES`, `PO_NO_ENUM`, a declaration that is not public),
  as the functions, constants, typedefs and global variables that it inherits can
  name them and the child can outlive its parent
  (`qore_root_ns_private::refLeftOutDecls()`, released when the child's root
  namespace is destroyed; qore#7607). Its enums are referenced also when they are
  copied, as the copy of a user enum does not keep its original, and the
  declarations that the parent keeps so are passed on to the child, whose own
  children inherit the same functions. A child thus keeps every enum copy of its
  parent, and a child created with `PO_NO_INHERIT_SYSTEM_HASHDECLS` every hashdecl
  copy (a copy is never a builtin type owner), for its lifetime.
- Static system declarations have no Program owner. Copies of builtin declarations
  in a Program do have that Program as their owner, even though `isSystem()` is true.
- An unattached declaration has no Program owner. A reference-counted declaration
  that survives its namespace also has no owner after namespace teardown.
  Consequently, a null result alone does not prove process-wide lifetime.
- An original builtin declaration in a namespace tree that a binary module attaches to
  a namespace that a Program owns at run time (`QoreNamespace::addNamespace()`, outside
  the static system namespace and the namespace of a module) is owned by that Program
  and is mortal: it is marked `program_local` before the tree is published
  (`qore_ns_private::markProgramLocal()`), and
  the declaration lifetime system treats it as a user declaration
  (`isTypeOwnerSystem()` is false; `isSystem()` is unchanged). The import copy of a
  builtin class and the instances of a mortal generic hashdecl are mortal too. A
  builtin variant of a function of such a namespace references the mortal
  declarations that its signature names
  (`BuiltinFunctionVariantBase::refMortalDecls()`), so that the variant can be
  imported into another Program or copied into a child Program that does not inherit
  the classes and outlive the Program; such a declaration can therefore outlive its
  Program (qore#7540). An attach refreshes the references of the variants that it
  adds and of the variants added before it that name a declaration that it marks,
  which the Program indexes by the declarations that they name that are not mortal
  yet (`qore_root_ns_private::unmarked_decl_variants`), not those of every variant
  of the Program (qore#7608); a variant of one Program that names a declaration
  that a module attaches to another Program is not refreshed by that attach. A builtin declaration that a module adds one at a time to a
  namespace already attached to a Program (`addSystemClass()`, `addSystemHashDecl()`,
  `addSystemEnum()`, qore#7649) is marked as well. A declaration marked at run time references the binary
  module declarations that it names when it is marked, not at its Program's first
  `QoreTypeOwnerBatch`, as a module can be deleted before the Program at the end of the
  process (`qore_type_owner_take_module_refs()`). One that something keeps to the end of the process (a value
  kept in static storage, a Program with static storage duration) is freed after
  the modules' `del` functions have run, as the static namespace's module classes
  are: binary modules are never unloaded, and a module's `del` function must leave
  what the destructors, methods and module data of its classes need valid until then
  (`doxygen/lib/70_qore_modules.doxygen`, Releasing Module State at Shutdown;
  `examples/test/qore/namespaces/runtime-builtin-survivor`).
- A declaration that another Program comes to name keeps such a declaration with a counted
  reference that it takes when it starts naming it and releases when it is freed (qore#7680):
  - the import copy of a class references its source when the source is such a declaration
    of another Program (`qore_class_private::import_source`, `refMortalImportSource()`), as it
    references the source of a user class from another Program;
  - the copy of such an enum references its original (`orig_ref`), as the copy of a binary
    module's enum does;
  - a builtin variant added to a function that a Program's namespace shares with a namespace that
    no Program owns (or with another Program's) is the Program's: the entry gets its own copy of the
    function first (copy on write, qore#7588), and the variant is a run-time variant of the Program
    (`qore_root_ns_private::addRuntimeVariant()`, `refMortalDecls()`), which references the mortal
    declarations that it names and is freed with the Program;
  - a builtin variant added to a namespace that no Program owns itself (the static system namespace
    or a module's) is shared by every Program and lives until library cleanup or until its module
    is deleted: if its signature names
    such a declaration (as `QoreTypeDeclRefs::addMortal()` finds them), it is refused before
    anything is changed, with a `QoreStandardException` (`RUNTIME-DECLARATION-ERROR`) that names
    the variant and the declaration and says to add the variant to that Program's own namespace
    (`qore_ns_private::checkSharedVariant()`; a debug build counts the refusals:
    `qore_dbg_refused_shared_variants()`), as a reference would keep the declaration until then.
    In a binary module's namespace initialization (a module context is set and the additions are
    committed together), the check is made when the variant is added
    (`addBuiltinModuleVariant()`), so that the module gets the exception; a variant that names a
    declaration that a namespace committed before it in the same context marks is refused by
    `qore_root_ns_private::commitModule()`, which raises the exception in the module's sink and
    still commits the other additions and rebuilds the indexes.
    A variant added before a declaration that it names is marked is not refused; no binary module
    of this repository or of the external module repositories adds such variants at run time;
  - a declaration that becomes mortal at run time (one marked when it is added or attached,
    the import copy of a builtin class, an instance of a mortal generic hashdecl) references the
    mortal declarations of other Programs that its member, static variable and signature types
    name directly (`QoreTypeOwner::type_owner_xrefs`), with the binary module declarations that
    it names (`type_owner_module_refs`), when it becomes mortal
    (`qore_type_owner_take_runtime_refs()`): in the marking event, under the runtime namespace
    write lock of its Program, with the type lock (`ctl`) for the references; the declarations of
    its own Program are left to its Program's `QoreTypeOwnerBatch`.  A builtin declaration that is
    not mortal yet is referenced as a module's, also if a later attach marks it;
  - a global variable imported into another Program (`Program::importGlobalVariable()`)
    references the mortal declarations that its type names (`Var::refTypeDecls()`), as the
    importing Program is parsed with its type after the source Program can be deleted (the source's
    variable itself refuses writes then); the typedefs, constants and global variables that a child
    Program inherits are covered by the references of `refLeftOutDecls()` (qore#7607).
  The references are all or nothing: the lists are made before any reference is taken.  When an
  allocation fails, the declarations are flagged (`type_owner_refs_pending`, counted by the root
  namespace without an allocation) and the completion of a marking event of the Program takes them:
  that of the event itself, or, if it fails too (the event then fails), the next one
  (`qore_root_ns_private::retryPendingRuntimeRefs()`, which also finds the instances of generic
  hashdecls in their caches); an instance of a mortal generic hashdecl takes them before it is
  published, and a failure fails its creation, unless its base is marked while it is made, when a
  failure is recorded as above.  A builtin declaration that another declaration of the same Program
  referenced as a module's before a later attach marked it is left to the Program's batch, which
  releases that reference.  Two Programs whose declarations name each other (a module can make two such classes
  before it attaches them to two Programs) keep each other's until library cleanup:
  `delete_qore_types()` lists these references among those held inside a cycle of survivors and
  releases them with the cycle (`qore_dbg_cross_program_cycle_count()` counts such cycles in a debug
  build).  A declaration that a binary module adds at run time to a namespace that no Program owns
  takes no reference to the mortal declarations that it names, which would keep them until library
  cleanup whatever Programs are created and deleted: a module must add such a declaration to the
  namespace of the Program whose declarations it names (a debug build counts the others:
  `qore_dbg_unowned_program_local_names()`).  No binary module of this repository does so; module-jni's
  classes for a Program's class loader are qore#7650.

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

`examples/test/qore/namespaces/runtime-builtin-naming.qtest` checks each path by which another
Program names a declaration added to a Program at run time, in each execution mode, after the
Program is deleted, with the failures of the references and the cycles; the cases `cycle` and
`shared` of `runtime-builtin-survivor` check that what is kept until library cleanup is freed then.

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
