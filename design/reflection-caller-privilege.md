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

Every reflection entry point that checks cross-Program access captures the caller first — a
`QoreReflectionCaller` (`qore/QoreReflection.h`), built **before** the Program-context switch — and then applies one
of the checks below.

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
  `QoreReflectionCaller`) set the thread's runtime parse options to `QoreReflectionCaller::getCallRestrictionOverride()` for
  the duration of call checking: the target Program's options, with every domain the caller restricts also restricted and
  every explicitly-allowed domain the caller lacks removed. The body then runs under the target Program's own
  options (as a cross-Program `callFunction()` does), so a call the caller is allowed to make still executes with the
  target's privileges. Calls therefore also require Program access before dispatch, with the class-specific
  permission for imported public module classes described below.

This is stricter than a pure "caller only" rule (it also enforces the target Program's restrictions), which was
chosen (D1, David 2026-10-21) so that a call through reflection can never exceed either Program's restrictions while
nothing a trusted caller legitimately does is newly refused.

### 2. Reading or writing a class's members or constants

The class's functional domain is checked the same way (caller and target Program), via
`reflection_check_class_value_access()`. For a normal member, the object's own Program is also checked for navigation
access (below), because the value belongs to that object.

### 3. Access is classified per operation

`QoreReflectionCaller::Access` distinguishes structural navigation, value access and member-default evaluation.
Every checked operation first permits access from the target itself or from its creator chain (including independent
Programs). Other callers must restrict no functional domain the target allows and must have no governing sandbox
manager other than the target's own (see [Sandbox managers](#sandbox-managers)). `PO_IN_MODULE` is an
execution-context marker, not a restricting capability.

|!Operation|!Explicitly-allowed domains|!Execution and values
|Structural navigation (`Structural`)|Not compared|Only declarations and handles; no initializers or target state
|Value access and calls (`Value`)|Caller must allow every such domain the target allows, or return through an active module call chain|Full access; class calls also permit an exact imported public module class
|Member defaults and typed-hash casts (`MemberDefault`)|Full comparison except for trusted user-module code|Evaluates the initializer in the target context

Thus code without injection or debugger capability can inspect a foreign Program's structure without acquiring its
capabilities. Obtaining a function, method, member or typed-hash handle does not authorize calling it, reading its
values, assigning its state or constructing values with its initializers. Denial raises `REFLECTION-ACCESS-ERROR`.
The existing domain checks still apply to classes and callable variants. Builtins require full access too: a builtin
`Program` constructor can create a child with the target's options, and `create_object()` can execute a target class's
constructor. A builtin's own domain therefore does not prove that it cannot exercise the target's privileges.
Normal methods, copy and destructor calls also check the object's own Program, including inherited methods.

#### Callbacks through modules

A module may reflect back into a Program already participating in its active call chain without duplicating that
Program's explicitly-allowed options. For example, `RestClientIo::copyWithUrl()` constructs an application-defined
subclass through reflection while the application is calling the library. The check starts at the captured module
caller, excludes context switches introduced by the reflection API, and walks outward through module contexts.
The first non-module Program may be the target, but the walk never crosses it. An intervening application Program
therefore cannot borrow a more privileged outer caller's authority. An unrelated target, including a Program
created by that outer caller, is not authorized by this rule.

Restricting domains, the sandbox-manager rule, callable domains, and the runtime restriction override remain in
force. This rule grants no general module exemption and does not expose inactive Programs to module callbacks.
The context walk checks cooperative cancellation every 100 entries.

#### Calls to imported public module classes

A module can also construct or call a public class exported by another user module when the first application
Program on the active call chain has imported that exact class. This preserves
`AbstractDataProviderFactory::create()` and `createWithTemplate()`: DataProvider's module Program need not have
the injection or debugger options of the module defining the provider class, and the provider module need not
be active on the stack. The application could already name and construct the imported class directly.

This permission is limited to class calls (`QoreReflectionCaller::checkClassCallAccess()`). The class must be
non-builtin and module-public, its source Program must be the reflection handle's target module Program, and
the application must resolve the class's path to the same class ID and source Program. A same-named unrelated
class does not qualify. The walk stops at the first application even if an outer application imported the class.
That application's restricting domains must also be compatible with the target. The captured caller's domain
checks, the sandbox-manager rule and runtime restriction override remain enforced. Normal methods, copy and
destruction still check the object's Program separately.

This does not grant access to globals, static members, constants, functions, private module classes or builtin
constructors, nor to an unrelated application Program merely because it contains the same imported class.

#### Modules act for their application

A user module's Program inherits the parse options of the Program that loads it first
(`QoreModuleManager::loadSeparatedModule()`, and for an AOT module `qore_aot_module_init`), including explicitly-allowed
options such as `PO_ALLOW_INJECTION`, so whether one module had full access to another depended on load order: Qorus
loads `DataProvider` before its core Program enables injection and a provider module afterwards, so
`AbstractDataProviderFactory::create()` was refused (#5556 follow-up). `qore_reflection_has_module_caller()` now also
grants access to a target user module when the first application Program on the module call chain restricts no domain
the target allows and allows every explicitly-allowed domain it allows: the modules act with that application's
privileges, which it could use itself. A non-module caller is compared with the target as before, so a Program without
injection cannot call into a module that has it. The application is the nearest one on the chain: a restricted
Program's closure called back by module code for a privileged Program is judged as the restricted Program. Module code
that runs for no application finds none and is judged strictly: a thread that module code started has no application
frame, and a module's init and del closures run behind a barrier (`QoreModuleLifecycleBarrierHelper`, for source
modules in `QoreModuleDefContext::init()` and `QoreUserModule::runDelCallback()`, for AOT modules around the compiled
`MODULE_INIT` function) that the walk does not cross.

#### Defaults and module compatibility

This implements option (a) in issue #5483: retain the trusted-module exception **only for member-default evaluation**.
`AbstractMember::getDefaultValue()` and `TypedHash::doCast*()` may evaluate arbitrary member-initializer expressions.
A non-module caller without the target's explicitly-allowed domains is refused before evaluation. A user module (`PO_IN_MODULE`, which user code cannot set)
can continue evaluating defaults handed to it by a Program using injection or debugging. Restricting domains and
the sandbox-manager rule are still checked. This preserves `DataProvider::HashDeclDataType` with both source and AOT
modules, whose module Programs need not inherit their loader's explicitly-allowed domains. The general module
exception for navigation and all value access has been removed; the default-specific exception is intentional.

A caller-restriction override alone is not used as permission to execute a foreign initializer: defaults can enter
user functions and nested Program contexts. Supporting that alternative would require proving the restrictions
survive every execution backend and nested call. `TypedHash::doCast*()` also evaluates missing-member initializers
and uses the same default-specific exception: DataProvider calls it when converting records. Other callers still
require full access before any initializer is evaluated.

#### Constants and other value APIs

Namespace and class constants retain full value-access checks. Source constants are initialized during parsing or
module initialization, but `ConstantEntry::getReferencedValue()` also resolves deferred AOT constant references and
walks containers. Constants may contain objects and call references, so returning them is not structural inspection,
even when no expression is evaluated on this particular read. `Constant::forName(pgm, ...)` must retain **pgm** in
its private reflection handle, rather than accidentally recording the lookup caller.

Global and static-member reads retrieve stored values; normal-member reads use `getReferencedMemberNoMethod()` and
do not invoke member gates. These remain value access: object/call-reference values can confer capabilities.
`AbstractVariant::getDefaultArgs()` returns stored default expressions/values, not just parameter types, and likewise
requires full access. Enum member values, in contrast, are already-fixed scalar declarations and use structural
access. `Type::getDefaultValue()` returns the type's intrinsic empty/zero default, not a hashdecl member initializer.
`Type::acceptsValue()` checks/converts the supplied value; it does not construct missing hashdecl members as
`TypedHash::doCast*()` does. `TypedHashMember::getValue(h)` reads the caller-supplied hash, not Program state.

Separately (**N1**), a class, typed hash or enum declared in one Program and inherited by or imported into another
keeps referring to the namespace where it was declared, which belongs to the declaring Program. `getNamespace()` on
such a symbol reached in Program P no longer returns the declaring Program's namespace: it returns the namespace with
the same path in **P's own** tree (`qore_program_find_namespace()`), or raises `UNKNOWN-NAMESPACE` if P has none. So
navigation never leaves the reflection object's own Program by following an inherited symbol's declaring namespace —
which was the namespace-navigation exposure.

### Sandbox managers

A `QoreSandboxManager` is not bound to code statically: the manager that governs filesystem and network access,
interrupts and thread limits is resolved for each operation by walking the call stack — the current Program, then the
calling Programs, then the sandbox inherited by the thread
([thread-sandbox-inheritance.md](thread-sandbox-inheritance.md)). The caller captures its governing manager in
`QoreReflectionCaller`, and `checkProgramAccess()` compares it with the target Program's own manager:

|!Target Program|!Caller without a governing manager|!Caller with a governing manager
|Has its own manager|Allowed (subject to the other checks)|Allowed only if it is the same manager
|User module Program (`PO_IN_MODULE`) without a manager|Allowed|Allowed: the module's code runs under the caller's manager
|Any other Program without a manager|Allowed|Refused

A user module Program never has a manager of its own: whichever Program loaded it, sandbox resolution walks out of
it to the code that calls it, so module code is governed by its caller's sandbox — reached through reflection exactly
as when the caller calls the module directly. A sandboxed application can therefore use a factory in one module
(`DataProvider`) that constructs a provider class of another (`CsvUtil`) through reflection; the provider's
filesystem and network access in its constructor and methods is still checked against the application's sandbox.
User code cannot set `PO_IN_MODULE`, so an application Program cannot claim this. An application Program without a
manager is not governed by a sandboxed caller's manager for its own state, so a sandboxed caller is refused.

### Sibling caveat

The "at least as privileged" rule lets **two sibling Programs with identical parse options and no sandbox manager**
reach each other through a reflection object one hands the other: each is as privileged as the other, neither has a
manager. This is deliberate and documented: such Programs can already call each other's code through handed-over call
references, so reflection grants nothing new. It matters only when one Program deliberately gives another a reflection
handle.

**Qorus tenants are not affected.** A sandboxed Qorus interface Program forbids reflection outright (`PO_NO_REFLECTION`
is part of `PO_LOCKDOWN`) and has its own `QoreSandboxManager`, so it is never "equally privileged and unmanaged"
with respect to another tenant or to qorus-core, and it cannot use reflection at all. Module code that an interface
calls does use reflection on its behalf — a `DataProvider` factory constructing a provider class of another module —
under the [sandbox-manager rule](#sandbox-managers): the provider module's code runs under the interface's sandbox.

## Entry points

The following inventory covers every Qore-visible method declared in `modules/reflection/src/QC_*.qpp`; each name
includes all overloads. Inherited methods follow the row of the class that declares them. Constructors here create
reflection **handles**, not target instances; abstract constructors simply reject construction. Serialization hooks
encode declaration paths and resolve them in the deserializing Program, without executing target constructors.

- **Structural:** declaration metadata, type relations and handles. Existing metadata-only accessors on a supplied
  handle retain their behavior; namespace navigation and explicit foreign-Program lookup use `Structural` checks.
- **Value:** full access; class values also check class domains. An object member additionally checks its object's
  Program. No general user-module exception.
- **Default:** `MemberDefault` access, with the narrowly retained trusted-module exception described above.
- **Call:** Program access plus caller-and-target domain checks, including builtin functions, constructors and
  pseudo-methods. Public user-module classes additionally permit the exact imported-class check above.
  Normal/copy/destructor calls also check the object's Program.
- **Cast:** member-default access before constructing a typed hash or evaluating any missing-member initializer;
  the same trusted-module exception preserves DataProvider record conversion.
- **Input value:** operates on a supplied value, not foreign Program state; does not evaluate member defaults.

|!Class (source file `QC_<Class>.qpp`)|!Entry points (all overloads)|!Class of operation
|AbstractClass|`constructor`, `getName`, `getLanguage`, `isEqual`, `getModifiers`, `getModifierList`, `isBuiltin`, `isUser`, `isAbstract`, `isInjected`, `isModulePublic`, `isPseudoClass`, `getId`, `getHash`, `hasMemberGate`, `hasMethodGate`, `hasMemberNotification`, `getDomain`, `getDomainStringList`, `getVariant`, `getVariantArgs`, `getMethod`, `getMethods`, `getVariants`, `getParentClasses`|Structural
|AbstractClassMember|`constructor`, `getModifiers`, `getModifierList`, `getAccessModifierString`, `isBuiltin`, `isUser`, `isStatic`, `isTransient`, `getClass`|Structural
|AbstractConstant|`constructor`, `getName`, `getModuleName`, `isEqual`, `getModifiers`, `getModifierList`, `isClassConstant`, `isBuiltin`, `isUser`, `getType`, `getSourceLocation`|Structural
|AbstractConstant|`getValue`|Value
|AbstractMember|`constructor`, `getName`, `isEqual`, `getType`, `getSourceLocation`|Structural
|AbstractMember|`getDefaultValue`|Default
|AbstractMethod|`constructor`, `isStatic`, `getMethodTypeName`, `getVariant`, `getVariantArgs`, `getVariants`, `getClass`|Structural
|AbstractMethodVariant|`constructor`, `isAbstract`, `isFinal`, `isConstMethod`, `getMethod`, `getClass`, `getAccessModifierString`|Structural
|AbstractReflectionFunction|`constructor`, `isEqual`, `getName`, `getModuleName`, `isBuiltin`, `isUser`, `isInjected`, `isMethod`, `isPseudoMethod`, `numVariants`, `getCodeFlags`, `getCodeFlagList`, `getDomain`, `getDomainStringList`|Structural
|AbstractVariant|`constructor`, `isEqual`, `getCodeFlags`, `getCodeFlagList`, `getModifiers`, `getModifierList`, `getFunction`, `getDomain`, `getDomainStringList`, `isBuiltin`, `isUser`, `isNamedCallable`, `isInjected`, `hasBody`, `empty`, `isMethodVariant`, `isPseudoMethodVariant`, `numParams`, `getParamString`, `getReturnTypeString`, `toString`, `getReturnType`, `getParamTypes`, `getParamNames`, `getNamedParameterNames`, `getSourceLocation`|Structural
|AbstractVariant|`getDefaultArgs`|Value
|Class|`constructor`, `getPathName`, `getModuleName`, `getTypeParameters`, `findMethod`, `findNormalMethod`, `findStaticMethod`, `hasConstructorMethod`, `hasDestructorMethod`, `hasCopyMethod`, `getNormalVariant`, `getNormalVariantArgs`, `getStaticVariant`, `getStaticVariantArgs`, `getConstructorVariant`, `getConstructorVariantArgs`, `getDestructorVariant`, `getCopyVariant`, `getNormalMethod`, `getStaticMethod`, `getConstructorMethod`, `getDestructorMethod`, `getCopyMethod`, `getNormalMethods`, `getStaticMethods`, `getAllNormalMethods`, `getAllStaticMethods`, `getAllMethods`, `getNormalVariants`, `getStaticVariants`, `getConstructorVariants`, `getIfDestructorVariant`, `getIfCopyVariant`, `getType`, `getOrNothingType`, `getClassHierarchy`, `getMember`, `getNormalMember`, `getStaticMember`, `getMembers`, `getConstant`, `tryGetConstant`, `getConstants`, `getNamespace`, `instanceOf`, `getInheritanceAccess`, `forName`, `getClass`, `findAllRegex`, `getTypeParameterBounds`, `getTypeParameterDefaults`|Structural
|Class|`newObject`, `newObjectArgs`, `callConstructor`|Call
|ClassConstant|`constructor`, `getClass`|Structural
|Constant|`constructor`, `getNamespace`, `forName`, `findAllRegex`|Structural
|ConstructorMethod|`constructor`|Structural
|ConstructorMethod|`call`, `callArgs`|Call
|ConstructorMethodVariant|`constructor`|Structural
|ConstructorMethodVariant|`call`, `callArgs`|Call
|CopyMethod|`constructor`|Structural
|CopyMethod|`call`|Call
|CopyMethodVariant|`constructor`|Structural
|CopyMethodVariant|`call`|Call
|DestructorMethod|`constructor`|Structural
|DestructorMethod|`call`|Call
|DestructorMethodVariant|`constructor`|Structural
|DestructorMethodVariant|`call`|Call
|Enum|`constructor`, `isEqual`, `getName`, `getPathName`, `getModuleName`, `isBuiltin`, `isUser`, `getSourceLocation`, `getType`, `getOrNothingType`, `getBaseType`, `getNamespace`, `getMember`, `getMembers`, `isValidValue`, `forName`|Structural
|EnumMember|`constructor`, `getName`, `getValue`, `getType`, `getEnum`, `isBuiltin`, `isUser`|Structural
|Function|`constructor`, `getVariants`, `getVariant`, `getVariantArgs`, `forName`, `findAllRegex`|Structural
|Function|`call`, `callArgs`|Call
|FunctionVariant|`constructor`|Structural
|FunctionVariant|`call`, `callArgs`|Call
|GlobalVar|`constructor`, `isEqual`, `getName`, `isBuiltin`, `isUser`, `isModulePublic`, `getSourceLocation`, `getType`, `getNamespace`, `forName`, `findAllRegex`|Structural
|GlobalVar|`getValue`, `setValue`|Value
|Namespace|`constructor`, `getName`, `getModuleName`, `getPathName`, `isEqual`, `getModifiers`, `getModifierList`, `isBuiltin`, `isUser`, `isModulePublic`, `isImported`, `isRoot`, `getParentNamespace`, `getConstant`, `getConstants`, `getNamespace`, `getNamespaces`, `getClass`, `getClasses`, `getFunction`, `getFunctions`, `getVariant`, `getVariantArgs`, `getVariants`, `getGlobalVar`, `getGlobalVars`, `getTypedHash`, `getTypedHashes`, `getEnum`, `getEnums`, `forName`, `findAllRegex`|Structural
|NormalMember|`constructor`|Structural
|NormalMember|`getValue`, `setValue`|Value
|NormalMethod|`constructor`|Structural
|NormalMethod|`call`, `callArgs`|Call
|NormalMethodVariant|`constructor`|Structural
|NormalMethodVariant|`call`, `callArgs`|Call
|PluginRegistry|`get`, `getProcessRegistry`, `getProcessModules`, `getActiveModules`, `getTypes`, `getOperations`, `resolveOperation`|Structural
|PluginRegistry|`getRecentFallbackSites`, `clearFallbackSites`|Value
|PseudoClass|`constructor`, `forValue`, `forType`|Structural
|PseudoMethod|`constructor`|Structural
|PseudoMethod|`call`, `callArgs`|Call
|PseudoMethodVariant|`constructor`|Structural
|PseudoMethodVariant|`call`, `callArgs`|Call
|StaticMember|`constructor`|Structural
|StaticMember|`getValue`, `setValue`|Value
|StaticMethod|`constructor`|Structural
|StaticMethod|`call`, `callArgs`|Call
|StaticMethodVariant|`constructor`|Structural
|StaticMethodVariant|`call`, `callArgs`|Call
|Type|`constructor`, `getName`, `getPathName`, `isEqual`, `isOutputCompatible`, `isCompatible`, `isAssignableFrom`, `getBaseTypeCode`, `canConvertToScalar`, `hasDefaultValue`, `getDefaultValue`, `isOrNothingType`, `getBaseType`, `getOrNothingType`, `getMandatoryType`, `getElementType`, `isParameterized`, `getTypeArguments`, `getTypedHash`, `isTypedHash`, `isTypedCode`, `getCodeReturnType`, `getCodeParamTypes`, `codeHasVarargs`, `isUnion`, `getUnionMemberTypes`, `hasType`, `getType`, `getAcceptTypeHash`, `getReturnTypeHash`|Structural
|Type|`acceptsValue`|Input value
|TypedHash|`constructor`, `isEqual`, `getName`, `getPathName`, `getModuleName`, `isBuiltin`, `isUser`, `getSourceLocation`, `getType`, `getTypeParameters`, `getOrNothingType`, `getNamespace`, `getMember`, `getMembers`, `getParentTypedHash`, `inheritsFrom`, `forName`, `findAllRegex`, `getTypeParameterBounds`, `getTypeParameterDefaults`|Structural
|TypedHash|`doCast`, `doCastWithOverrides`, `doCastValues`, `doCastValuesWithOverrides`|Cast
|TypedHashMember|`constructor`, `isBuiltin`, `isUser`|Structural
|TypedHashMember|`getValue`|Input value

## libqore support

|!Symbol|!Header|!Purpose
| `QoreReflectionCaller` | `qore/QoreReflection.h` | capture the caller (Program, runtime parse options, sandbox manager) at an entry point
| `QoreReflectionCallCheckHelper` | `qore/QoreReflection.h` | set the runtime options for a run-time-selected call into a target Program
| `QoreExternalFunction::evalFunction(…, const QoreReflectionCaller&, …)` | `qore/QoreReflection.h` | call a function checked against the caller and the function's Program
| `qore_parse_options_allow_domain()` | `qore/QoreReflection.h` | the parser's domain rule, shared by all checks
| `qore_program_find_namespace()` | `qore/QoreReflection.h` | map a namespace to the one with the same path in a Program's tree (N1)
| `QoreClass::runtimeCheckDomain()` | `qore/QoreClass.h` | the class-domain rule (also used by `create_object()`)
| `qore_program_private_base::creator_ids` / `programCreatedBy()` | internal | the creator chain for `checkProgramAccess()`

The reflection module wraps these in `reflection_check_class_call()`, `reflection_check_class_value_access()`,
`reflection_check_variant_call()`, `reflection_check_program_access()`, `reflection_check_structural_access()` and
`reflection_program_namespace()`
(`modules/reflection/src/AbstractReflectionObject.{h,cpp}`).

## Compatibility

New exceptions are raised only where the caller could not have reached the code or data directly:
`INVALID-FUNCTION-ACCESS` / `CREATE-OBJECT-ERROR` for a call or construction the caller's or target Program's
restrictions forbid (previously the variant path and cross-Program calls bypassed this), and
`REFLECTION-ACCESS-ERROR` / `UNKNOWN-NAMESPACE` for navigation or value access into a Program the caller is not
entitled to. Trusted, equally- or more-privileged callers — including Qorus building interface objects by class name
in an interface Program whose restrictions allow the class — are unaffected.

## Regression coverage

`reflection-operation-access.qtest` checks each operation class with independent sibling Programs using injection,
debugging and both capabilities. It asserts that structural inspection has no initializer side effects, full access
is required at execution/value boundaries, denied writes and calls leave state unchanged, and restricting domains
and a caller governed by a different sandbox manager than an application target still deny structural access. It
covers fixed and selected call variants, all four typed-hash casts, builtin Program construction and object
factories, object members/copy/destruction, enum scalar values and the constant-handle Program identity.
`HashDeclDataTypeCallerPrivilege.qtest` uses an expression initializer to exercise the retained module-default
exception and record conversion; the ordinary non-module reader remains denied. The `ReflectionAccessProbe` test
module proves the exception does not grant module callers global reads, writes or arbitrary function calls. Existing
caller-domain and namespace-navigation tests cover builtin calls, creators, transitive creators and equal siblings. `QoreRepl.qtest` covers tooling-created Programs;
`AOTDataProviderExecutable.qtest` covers compiled executable use of DataProvider.

`FactoryCallerPrivilege.qtest` exercises a provider module with injection and debugger capabilities that the
DataProvider caller lacks: both factory creation paths, the single-constructor fallback, all seven reflection
constructor entry points and a static method variant. It rejects an intervening application without the import,
a same-named unrelated class, restricting-domain mismatches, private classes, builtin constructors, global reads and
a handle bound to an unrelated Program that imported the same class. A sandboxed importer constructs the class
through every entry point, and the provider's file and loopback-network access in its constructor is allowed or
denied by the importer's own sandbox. A caller governed by another manager is refused for a Program with a manager
of its own and for an application Program without one, and allowed with the target's own manager. The fixtures are tested
from source and compiled modules across AST, IR, JIT and tiered execution.
