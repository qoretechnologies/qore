# Reflection Caller-Privilege Model

Copyright (C) 2026 Qore Technologies, s.r.o.

## Overview

The `reflection` module (`modules/reflection`) lets Qore code call functions and methods, construct objects, read and
write members, constants and global variables, and navigate namespaces, classes, functions and types — of its own
Program or of another Program it holds a reflection handle for. Every such operation must be checked against the
sandboxing restrictions of the **calling** code, not those of the reflection module, and must not let code reach
code or data it could not reach directly.

Before this work, reflection switched into the reflection object's Program before any check ran, so the caller's
restrictions were never seen; a pre-selected method/function variant skipped the functional-domain check entirely;
and navigation from a class a Program inherited could walk into the *declaring* Program's namespace tree and read or
overwrite its globals, constants, classes and functions. This document describes the model that closes those holes.

It builds on the cross-Program privilege model libqore already uses for `QoreProgram::callFunction()`
(`lib/Function.cpp`, the `defer_domain_po` path) and on the class-domain rule of `create_object()`
(`QoreClass::runtimeCheckDomain()`). See [module-sandboxing-audit-guide.md](module-sandboxing-audit-guide.md) for the
two sandbox mechanisms and [thread-sandbox-inheritance.md](thread-sandbox-inheritance.md) for the related thread
work.

## The model

Every reflection entry point that calls, constructs, reads, writes or navigates captures the caller first — a
`QoreReflectionCaller` (`qore/QoreReflection.h`), built **before** the Program-context switch — and then applies one
of three checks.

### 1. Calls (functions, method variants, constructors, copy, destructor)

The functional domain of the code to run must be allowed by **both** the caller and the Program the reflection
object belongs to, by the same rule the parser applies to a builtin named in source code
(`qore_parse_options_allow_domain()`): the parse options must not restrict a domain they can restrict (e.g.
`FILESYSTEM`/`PO_NO_FILESYSTEM`), and must allow every domain that must be allowed explicitly (e.g.
`DEBUGGER`/`PO_ALLOW_DEBUGGER`).

- When the variant is already fixed (a `FunctionVariant`/`MethodVariant`/`ConstructorMethodVariant` handle, or a
  class for a constructor/copy/destructor), the check is made directly against the variant's or class's domain.
- When the variant is selected at run time for the arguments (`Function::call`, `Method::call`, `newObject`, …), the
  domain check inside libqore's variant resolution must run against the caller + target rather than the module
  Program. `QoreReflectionCallCheckHelper` (and the `QoreExternalFunction::evalFunction()` overload that takes a
  `QoreReflectionCaller`) set the thread's runtime parse options to `QoreReflectionCaller::getCallCheckOptions()` for
  the duration of the call: the target Program's options, with every domain the caller restricts also restricted and
  every explicitly-allowed domain the caller lacks removed. The body then runs under the target Program's own
  options (as a cross-Program `callFunction()` does), so a call the caller is allowed to make still executes with the
  target's privileges.

This is stricter than a pure "caller only" rule (it also enforces the target Program's restrictions), which was
chosen (D1, David 2026-10-21) so that a call through reflection can never exceed either Program's restrictions while
nothing a trusted caller legitimately does is newly refused.

### 2. Reading or writing a class's members or constants

The class's functional domain is checked the same way (caller and target Program), via
`reflection_check_class_value_access()`. For a normal member, the object's own Program is also checked for navigation
access (below), because the value belongs to that object.

### 3. Navigation and value access across Programs

Navigation (`getNamespace`, `getParentNamespace`, `getClass(es)`, `getFunction(s)`, `getConstant(s)`,
`getGlobalVar(s)`, `getTypedHash(es)`, `getEnum(s)`, `getVariant(s)`, the `forName(pgm, …)`/`findAllRegex(pgm, …)`
lookups) and reading or writing values (`GlobalVar`/`StaticMember`/`NormalMember`/constant/enum-member values) are
allowed through a reflection object of **another** Program only when (`QoreReflectionCaller::checkProgramAccess()`):

- the caller runs in that Program; or
- the caller's Program created that Program, directly or through Programs it created (the creator chain recorded in
  `qore_program_private_base::creator_ids` when a Program is created, `lib/QoreProgram.cpp`); or
- the caller is **at least as privileged** as that Program: its runtime parse options restrict no functional domain
  the target allows, allow every domain the target allows explicitly, and no sandbox manager governs the caller
  other than the target's own.

Otherwise `REFLECTION-ACCESS-ERROR` is raised (D2/(i)+(b), David 2026-10-21).

Separately (**N1**), a class, typed hash or enum declared in one Program and inherited by or imported into another
keeps referring to the namespace where it was declared, which belongs to the declaring Program. `getNamespace()` on
such a symbol reached in Program P no longer returns the declaring Program's namespace: it returns the namespace with
the same path in **P's own** tree (`qore_program_find_namespace()`), or raises `UNKNOWN-NAMESPACE` if P has none. So
navigation never leaves the reflection object's own Program by following an inherited symbol's declaring namespace —
which was the namespace-navigation exposure.

### Sibling caveat

The "at least as privileged" rule lets **two sibling Programs with identical parse options and no sandbox manager**
reach each other through a reflection object one hands the other: each is as privileged as the other, neither has a
manager. This is deliberate and documented: such Programs can already call each other's code through handed-over call
references, so reflection grants nothing new. It matters only when one Program deliberately gives another a reflection
handle.

**Qorus tenants are not affected.** A sandboxed Qorus interface Program forbids reflection outright (`PO_NO_REFLECTION`
is part of `PO_LOCKDOWN`) and has its own `QoreSandboxManager`, so it is never "equally privileged and unmanaged"
with respect to another tenant or to qorus-core, and it cannot use reflection at all.

## Entry points

`QoreReflectionCaller` is captured in every entry point listed; the "check" column says which rule applies. A
constructor/`forName`/`findAllRegex` with a `Program` argument also checks navigation access to that Program.

| Entry point | File | Check |
|---|---|---|
| `Function::call` / `callArgs` | `QC_Function.qpp` | call (variant selected: caller+target) |
| `FunctionVariant::call` / `callArgs` | `QC_FunctionVariant.qpp` | call (fixed variant: caller+target) |
| `NormalMethod::call` / `callArgs` | `QC_NormalMethod.qpp` | call (class + variant selected) |
| `NormalMethodVariant::call` / `callArgs` | `QC_NormalMethodVariant.qpp` | call (fixed variant) |
| `StaticMethod::call` / `callArgs` | `QC_StaticMethod.qpp` | call (class + variant selected) |
| `StaticMethodVariant::call` / `callArgs` | `QC_StaticMethodVariant.qpp` | call (fixed variant) |
| `PseudoMethod::call` / `callArgs` | `QC_PseudoMethod.qpp` | call (variant selected) |
| `PseudoMethodVariant::call` / `callArgs` | `QC_PseudoMethodVariant.qpp` | call (fixed variant) |
| `Class::newObject` / `newObjectArgs` / `callConstructor` | `QC_Class.qpp` | call (class + constructor variant selected) |
| `ConstructorMethod::call` / `callArgs` | `QC_ConstructorMethod.qpp` | call (class + variant selected) |
| `ConstructorMethodVariant::call` / `callArgs` | `QC_ConstructorMethodVariant.qpp` | call (class + fixed variant) |
| `CopyMethod::call` | `QC_CopyMethod.qpp` | call (class + copy variant) |
| `CopyMethodVariant::call` | `QC_CopyMethodVariant.qpp` | call (class + fixed variant) |
| `DestructorMethod::call` | `QC_DestructorMethod.qpp` | call (class) |
| `DestructorMethodVariant::call` | `QC_DestructorMethodVariant.qpp` | call (fixed variant) |
| `GlobalVar::getValue` / `setValue` | `QC_GlobalVar.qpp` | value access (navigation) |
| `StaticMember::getValue` / `setValue` | `QC_StaticMember.qpp` | value access (class domain + navigation) |
| `NormalMember::getValue` / `setValue` | `QC_NormalMember.qpp` | value access (class domain + navigation of reflection and object Programs) |
| `AbstractConstant::getValue` (class and namespace constants) | `QC_AbstractConstant.qpp` | value access (class domain for a class constant + navigation) |
| `AbstractMember::getDefaultValue` | `QC_AbstractMember.qpp` | value access (navigation) |
| `EnumMember::getValue` | `QC_EnumMember.qpp` | value access (navigation) |
| `Class::getNamespace` | `QC_Class.qpp` | navigation + N1 namespace mapping |
| `Constant`/`GlobalVar`/`TypedHash`/`Enum`::`getNamespace` | respective `QC_*.qpp` | navigation + N1 namespace mapping |
| `Namespace::getParentNamespace`, `getConstant(s)`, `getNamespace(s)`, `getClass(es)`, `getFunction(s)`, `getVariant(s)`, `getGlobalVar(s)`, `getTypedHash(es)`, `getEnum(s)` | `QC_Namespace.qpp` | navigation |
| `Namespace::forName(pgm, …)`, `findAllRegex(pgm, …)` | `QC_Namespace.qpp` | navigation of the named Program |
| `Class`/`Constant`/`GlobalVar`/`Enum`/`TypedHash`::`forName(pgm, …)`, `findAllRegex(pgm, …)`; `PluginRegistry::get(pgm)` | respective `QC_*.qpp` | navigation of the named Program |

Lookups that resolve in the caller's own current Program (`forName(path)` without a `Program`, `Class::getClass(obj)`
for an object of the caller's Program) need no cross-Program check; metadata-only methods (names, modifiers, source
locations, type queries, `getDomain`, `isEqual`, …) read nothing and run as before.

## libqore support

| Symbol | Header | Purpose |
|---|---|---|
| `QoreReflectionCaller` | `qore/QoreReflection.h` | capture the caller (Program, runtime parse options, sandbox manager) at an entry point |
| `QoreReflectionCallCheckHelper` | `qore/QoreReflection.h` | set the runtime options for a run-time-selected call into a target Program |
| `QoreExternalFunction::evalFunction(…, const QoreReflectionCaller&, …)` | `qore/QoreReflection.h` | call a function checked against the caller and the function's Program |
| `qore_parse_options_allow_domain()` | `qore/QoreReflection.h` | the parser's domain rule, shared by all checks |
| `qore_program_find_namespace()` | `qore/QoreReflection.h` | map a namespace to the one with the same path in a Program's tree (N1) |
| `QoreClass::runtimeCheckDomain()` | `qore/QoreClass.h` | the class-domain rule (also used by `create_object()`) |
| `qore_program_private_base::creator_ids` / `programCreatedBy()` | internal | the creator chain for `checkProgramAccess()` |

The reflection module wraps these in `reflection_check_class_call()`, `reflection_check_class_value_access()`,
`reflection_check_variant_call()`, `reflection_check_program_access()` and `reflection_program_namespace()`
(`modules/reflection/src/AbstractReflectionObject.{h,cpp}`).

## Compatibility

New exceptions are raised only where the caller could not have reached the code or data directly:
`INVALID-FUNCTION-ACCESS` / `CREATE-OBJECT-ERROR` for a call or construction the caller's or target Program's
restrictions forbid (previously the variant path and cross-Program calls bypassed this), and
`REFLECTION-ACCESS-ERROR` / `UNKNOWN-NAMESPACE` for navigation or value access into a Program the caller is not
entitled to. Trusted, equally- or more-privileged callers — including Qorus building interface objects by class name
in an interface Program whose restrictions allow the class — are unaffected.
