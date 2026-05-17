# Generic Class Types - Design

**Status:** Implementation in progress.

**Target:** Pending. This document captures the language and implementation
shape for generic Qore classes and generic builtin/QPP classes. It is written
against the `feature/5164_jit` branch.

**Reference commit:** `1a3781762`. File/line references below are anchored to
that commit and will drift as the branch evolves. Treat them as starting
points.

## Summary

Qore already has parameterized type syntax and runtime type information for a
fixed set of hard-coded complex types:

```qore
list<int>
hash<string, auto>
reference<hash<auto>>
code<int(string, bool)>
object<MyClass>
```

The missing feature is not angle brackets in general. The missing feature is
allowing classes to declare and use type parameters:

```qore
class C<T> {
    private T value;

    constructor(T value) {
        self.value = value;
    }

    T get() {
        return value;
    }

    nothing set(T value) {
        self.value = value;
    }
}

C<int> c(1);
int i = c.get();
```

This design proposes **reified, runtime-checked generic class types**. There is
one runtime class `C`, not one compiled class per `C<int>`, `C<string>`, etc.
Each instantiated object carries its type arguments, and type checks,
assignment filtering, method signatures, member declarations, and AOT/IR
metadata use those arguments when needed.

The same model covers existing builtin/QPP-backed classes where generics would
immediately improve APIs:

```qore
Queue<int> q();
q.push(1);
int value = q.pop();

Promise<hash<ResponseInfo>> p();
Future<hash<ResponseInfo>> f = p.getFuture();
p.set({"code": 200});
hash<ResponseInfo> response = f.get();
```

The recommended implementation path is phased:

1. Add the type-system and object-runtime support for parameterized class
   types.
2. Prove it with builtin/QPP classes such as `Queue<T>`,
   `Channel<T>`, `Promise<T>`, and `Future<T>`.
3. Enable source-defined generic Qore classes such as `class C<T> { ... }`.

This avoids requiring parser, qpp, member storage, method dispatch, IR, AOT,
and JIT changes to all land at once.

## Goals

- Support generic Qore class declarations:

  ```qore
  class C<T> {
      T get() {
          ...
      }
  }
  ```

- Support generic builtin/QPP class declarations:

  ```qore
  qclass Queue<T> [dom=THREAD_CLASS; arg=Queue* q; ns=Qore::Thread];
  nothing Queue::push(T arg, timeout timeout_ms = 0);
  T Queue::pop(timeout timeout_ms = 0);
  ```

- Preserve Qore's existing runtime model: values are still `QoreValue`, methods
  still execute one implementation body, and generic class instantiations are
  not monomorphized into separate compiled code bodies.
- Make generic type information visible to parse-time type checks, runtime
  checks, reflection, QPP metadata, AOT deserialization, and IR guards.
- Keep raw existing APIs such as `Queue` and `Future` source-compatible
  via two class-gated compatibility flags described in Implementation
  Sketch: `raw_accepts_parameterized` for parameter / variable / cast
  acceptance, and `raw_construction_defaults_to_auto` for
  no-type-arguments construction. Migrated builtins set both; new
  source-defined generic classes set neither in v1.
- Make `Queue<T>`, `Channel<T>`, and `Future<T>` enforce their value type at
  the producer boundary (`push`, `send`, `Promise<T>::set`), not only
  document the return type. Consumer-side return-type spelling is per-class
  and is still open — see Open Questions.

## Non-goals For The First Implementation

- Do not add C++-style `template <T> class C` syntax. Qore should use
  `class C<T>`.
- Do not add method-level generic functions in v1:

  ```qore
  T identity<T>(T value);  # later, not v1
  ```

- Do not add type inference for class type arguments at arbitrary expression
  sites. Require the type arguments in a type position:

  ```qore
  C<int> c(1);
  C<int> c = new C<int>(1);
  ```

- Do not add constraints or bounds in v1:

  ```qore
  class C<T is SomeBase> { ... }  # later
  ```

- Do not add variance in v1. Generic class types should be invariant except
  for explicit raw compatibility on selected legacy builtin classes migrated
  to generics.
- `<auto!>` is allowed but is the same as `<auto>` for type-argument
  matching. The no-narrowing distinction only matters at folding sites
  (assignment, return, etc.); type-arg slots are not folding sites
  because they are positional. Implementations must accept `Queue<auto!>`
  syntactically and treat it identically to `Queue<auto>` for cache
  interning, acceptance, and reflection.
- Do not add generic hashdecls in v1. `hash<T>` already exists as a complex
  hash value type, but a declaration like `hash<Result<T>>` where the hashdecl
  itself has formal type parameters is a separate feature.
- Do not require JIT/AOT monomorphization per type argument.

## Syntax

### Generic Class Declarations

The source syntax should follow the existing class declaration shape with type
parameters appended to the class name:

```qore
class Box<T> {
    private T value;

    constructor(T value) {
        self.value = value;
    }

    T get() {
        return value;
    }
}
```

Multiple type parameters are allowed:

```qore
class Pair<K, V> {
    private K key;
    private V value;
}
```

v1 type parameters are names only. No bounds, defaults, variance markers, or
specialization syntax are included in the first version.

### Generic Type Uses

Generic class types are valid wherever ordinary class types are valid:

```qore
Box<int> b(1);
Box<string> s("x");

sub f(Box<int> b) {
    int value = b.get();
}

Box<hash<string, int>> nested();
```

The user-facing spelling should be the direct class spelling:

```qore
Box<int>
Qore::Thread::Queue<string>
```

The internal canonical path may continue to use the existing class-type
`object<...>` convention, but it must round-trip type arguments:

```qore
object<::Ns::Box<int>>
object<::Qore::Thread::Queue<string>>
```

The exact internal path spelling is less important than stability. AOT,
variant signatures, reflection, duplicate-signature checks, and serialized IR
must all use the same canonical spelling.

### Constructor Calls

Both constructor styles should be valid:

```qore
Box<int> b(1);
Box<int> b = new Box<int>(1);
```

The type arguments are not ordinary constructor arguments. They are constructor
metadata used to instantiate the object type, substitute member/method types,
and configure private data for generic-aware builtin classes.

### Raw Or Missing Type Arguments

For new user-defined generic classes, missing type arguments should be a parse
error:

```qore
class Box<T> {}

Box b();        # parse error in v1
Box<> b();      # parse error in v1 (empty type-arg list)
Box<auto> b();  # explicit concrete value-erased instance
```

`Box<>` is reserved as a parse error in v1; it may later carry a "use
defaults for every parameter" meaning once Phase 4 adds type-parameter
defaults. Treating it as an error today keeps the door open without
locking in semantics.

Existing non-generic builtin classes that become generic need two compatibility
rules, both explicitly opted in per class:

1. **Raw construction defaults to `<auto>`.** Existing code that constructs a
   migrated builtin without type arguments creates the concrete value-erased
   instantiation.

```qore
Queue q();   # constructed object is Queue<auto>
```

2. **Raw type annotations are legacy acceptance types.** A raw annotation can
   accept any instantiated value of the same generic class, but the raw type is
   not the same type as `C<auto>`.

```qore
Queue<int> qi();

Queue legacy = qi;        # allowed through raw compatibility
Queue<auto> erased = qi;  # rejected: Queue<auto> is not a wildcard
```

Source syntax for defaulted type parameters can be added later if it proves
useful. It is not required to make legacy builtin classes compatible, and it
does not imply raw construction defaults for new source-defined generic
classes.

## Semantics

### Reified Runtime Type Arguments

A generic object has:

- a base `QoreClass*`, for example `Box`
- an ordered vector of resolved type arguments, for example `[int]`
- the ordinary object member data and private data

The type arguments are part of the object's runtime type identity. They must be
available to:

- `runtimeAcceptsValue()`
- method parameter filtering
- method return filtering
- member assignment filtering
- `instanceof`
- casts
- reflection
- AOT and IR guards

### Runtime Object Type Invariant

Every object whose class has type parameters carries a concrete parameterized
runtime type. Raw is a source/static compatibility form only; it is not an
observable runtime object type for generic objects.

Program lifetime is handled by the existing class/object machinery: an object
holds `pgm`, classes hold `spgm`, and `Program`-scoped type-arg classes pin
their owning `Program` through the same chain that already keeps `list<MyClass>`
or `hash<string, MyClass>` working today. No new lifetime invariant is
introduced for parameterized class types.

Examples:

- `Queue q()` constructs an object whose `instantiated_type` is
  `Queue<auto>`.
- `Queue<int> q()` constructs an object whose `instantiated_type` is
  `Queue<int>`.
- `Box<auto> b()` constructs an object whose `instantiated_type` is
  `Box<auto>`.

There is no valid generic object whose runtime type is raw `Queue` or raw
`Box`. Raw type infos can exist for metadata, reflection, and selected legacy
acceptance checks, but mutation and return filtering must always read the
receiver object's concrete parameterized type.

#### Cross-Program Type Identity

Parameterized class type-info follows the same identity rule as ordinary class
type-info: identity is by `QoreClass*` pointer, not by canonical path.

When a `Program A` object whose type is `Box<int>` is observed from
`Program B`, the object carries a type-info pointer derived from A's
`Box` class. If B has its own independently-declared `Box`, B resolves a
separately-interned `Box<int>` against B's class. The two type-infos are
not identity-equal even when their canonical paths match — exactly the
same rule that governs `list<MyClass>` and `hash<string, MyClass>` today,
and exactly the same rule that governs raw `QoreClass*` identity.

The exception is the existing one for shared classes: when the loader
installs the same `QoreClass*` into both programs (a static-system or
binary class shared by reference), both programs naturally observe the
same parameterized cache entry because the cache is keyed by the shared
`QoreClass*`. No new exception, no new mechanism.

Raw acceptance follows the same identity rule cross-program. Even for
migrated builtins where `raw_accepts_parameterized = true`, `Program A`'s
raw `Queue` type-info accepts only objects whose `theclass` is `Program
A`'s `Queue` class. Independently-declared `Queue` classes in two programs
do not accept each other's values; the shared-class exception only applies
when `Queue` is the same `QoreClass*` in both programs.

### Type Equality And Matching

Generic class matching should be invariant in v1:

```qore
Box<int>    != Box<number>
Queue<int>  != Queue<auto>   # as an exact instantiated type
```

However, raw compatibility is needed for migration. This compatibility is
class-gated, not global: it is enabled for selected legacy builtin classes
migrated to generics, and disabled for new source-defined generic classes
unless a future explicit default/compatibility policy opts in.

```qore
Queue       accepts Queue<int>      # legacy generic-aware builtins only
Queue       accepts Queue<auto>
Queue<auto> does not accept Queue<int>
```

For mutable types such as `Queue<T>`, covariance would be unsound:

```qore
Queue<int> qi();
Queue qa = qi;          # allowed through raw compatibility
qa.push("not an int");  # runtime check still sees the object is Queue<int>
```

Therefore, treat raw compatibility as an acceptance rule, not as a general
subtype relation that changes the object's actual type arguments. Method
mutation paths must enforce the receiver object's stored type arguments even
when the static reference is raw.

For source-defined generic classes, raw syntax remains a parse error in v1.
The implementation may still expose a raw definition type through metadata,
but that raw type does not accept `Box<int>` values unless the class explicitly
has the legacy raw-acceptance flag.

#### Acceptance And `instanceof` Polarity

`runtimeAcceptsValue()` polarity follows directly from the rule above. Pin
it explicitly so implementers do not invert the comparison:

```text
runtimeAcceptsValue(rawQueueType,    queueIntValue)  -> true   # legacy raw acceptance
runtimeAcceptsValue(rawQueueType,    queueAutoValue) -> true   # legacy raw accepts <auto>
runtimeAcceptsValue(queueIntType,    queueIntValue)  -> true
runtimeAcceptsValue(queueIntType,    queueAutoValue) -> false  # invariant
runtimeAcceptsValue(queueAutoType,   queueIntValue)  -> false  # invariant
runtimeAcceptsValue(queueAutoType,   queueAutoValue) -> true

runtimeAcceptsValue(rawBoxType,      boxIntValue)    -> false  # source generic, no raw flag
runtimeAcceptsValue(boxIntType,      boxIntValue)    -> true
```

`instanceof` uses the same matching rules:

```qore
Queue<int> qi();
Queue<auto> qa();

qi instanceof Queue          # true:  legacy raw acceptance
qi instanceof Queue<int>     # true:  exact match
qi instanceof Queue<auto>    # false: invariant
qa instanceof Queue          # true:  legacy raw acceptance
qa instanceof Queue<int>     # false: invariant
qa instanceof Queue<auto>    # true:  exact match
```

Casts (`cast<Queue<int>>(x)`) follow the same rule as `instanceof`: exact
match for parameterized targets, wildcard acceptance for raw targets. A
cast that fails the exact-match rule throws `RUNTIME-CAST-ERROR`, even when
the source value's runtime type would convert at the value level — generic
class types are invariant, including under cast.

### Type Parameters In Class Bodies

Type-parameter names live in their own lookup phase that runs **before**
class, typedef, and hashdecl resolution within the class body. A name `T`
declared on `class C<T>` shadows any same-named class or typedef visible
from the class body, but only inside that class's type positions
(members, method signatures, constructors, locals declared as `T`,
expression-position type-parameter uses such as `C<T>::method` once
static methods are supported).

This matches every other generic language (Java, C#, TypeScript, Rust)
and avoids surprising shadowing: a future user-defined typedef called
`T` cannot accidentally shadow a class-level type parameter.

This is a semantic shadowing rule, not a reason to reject ordinary type
names. The parser rejects duplicate type-parameter names and syntactically
invalid parameter names. It may also reject reserved builtin type keywords
if the scanner does not allow them as identifiers in `class C<...>` syntax.
It must not reject `class C<T>` merely because a visible class, typedef, or
hashdecl named `T` exists.

Inside a generic class declaration, a type parameter name resolves to a
symbolic type parameter, not to an ordinary class or builtin type:

```qore
class C<T> {
    private T value;

    T get() {
        return value;
    }

    nothing set(T value) {
        self.value = value;
    }
}
```

The symbolic `T` is substituted with the instance's type argument when an
object of type `C<int>` or `C<string>` is constructed or used.

The method body is still compiled once. The runtime execution context must
carry enough generic context to answer "what is `T` for this `self`?" when the
body performs a type-sensitive operation.

For the example above:

- `C<int>::set()` filters its argument as `int`.
- `self.value = value` filters member assignment as `int`.
- `C<int>::get()` returns a value checked as `int`.
- `C<string>` uses the same method body but substitutes `string`.

### Constructors

Constructors can use type parameters:

```qore
class Box<T> {
    private T value;

    constructor(T value) {
        self.value = value;
    }
}

Box<int> b(1);
Box<int> bad("x");  # parse-time or runtime type error
```

Constructor variant selection must see the substituted signature for the
instantiated class type. For `new Box<int>(1)`, the constructor parameter type
is effectively `int`, not symbolic `T`.

### Static Members And Static Methods

v1 should be conservative. Static members should not use class type parameters:

```qore
class C<T> {
    static T value;  # reject in v1
}
```

Static methods using class type parameters require a parameterized static class
context:

```qore
class C<T> {
    static C<T> make(T value) {
        return new C<T>(value);
    }
}

C<int> c = C<int>::make(1);
```

This is useful, but it requires method dispatch to carry generic type
arguments even without `self`. It can be included after instance methods work,
or rejected in v1 with a clear parse error.

### Inheritance

Generic inheritance is useful but should not be the first milestone.

Eventually:

```qore
class Base<T> {
    T get();
}

class Derived<T> inherits Base<T> {
}

Base<int> b = new Derived<int>();
```

This requires inherited member and method signatures to be substituted through
the base-class type-argument mapping. For v1 source-defined generic classes,
either reject generic base classes or allow only non-generic bases from generic
classes:

```qore
class Derived<T> inherits NonGenericBase {
}
```

`Queue<T>` and `Channel<T>` do not require generic inheritance to prove the
core type machinery. `Future<T>` is the exception among the proposed pilots:
because `Promise<T>::getFuture()` returns a concrete `FutureImpl<T>` that must
be accepted as `Future<T>`, Phase 2 must include a narrow QPP-only
parameterized vparent mapping, for example `qclass FutureImpl<T>
[vparent=Future<T>]`. This is not full source-level generic inheritance; it
only maps the concrete implementation's type arguments to its generic abstract
base in generated builtin metadata.

Because `Future`, `Promise`, and `FutureImpl` are loaded from the static
system namespace into every program (see "Cross-Program Type Identity"),
the parameterized vparent mapping preserves identity across `Program`
boundaries — a `Program A` `FutureImpl<int>` value remains `instanceof
Future<int>` when observed from `Program B`. The Phase 2 pilots have no
cross-program identity hazard for this reason; the hazard only appears
later when generic source-defined classes start crossing program
boundaries.

## Current Source Observations

Qore already has a generic-shaped parse type layer:

- `QoreParseTypeInfo` stores `subtypes` and renders names like
  `Type<A, B>`.
  - `include/qore/intern/QoreParseTypeInfo.h:50-90`
- Duplicate-signature comparison already has a parse-type fallback through
  `paramTypesIdentical()`.
  - `include/qore/intern/QoreParseTypeInfo.h:47-50`
  - `include/qore/intern/QoreParseTypeInfo.h:131`
- The parser has a custom nested angle-bracket splitter in
  `ParserTypeStruct::getSubTypes()`.
  - `lib/parser.ypp:1295`
- Type declarations already accept `ANGLE_IDENTIFIER`.
  - `lib/parser.ypp:3324-3355`
- The scanner recognizes complex type identifiers and casts with angle
  brackets.
  - `lib/scanner.lpp:2475`
  - `lib/scanner.lpp:2604`

The resolved type layer is hard-coded to known parameterized base names:

- `QoreTypeSpec` has kinds for simple types, classes, hashdecls, and hard-coded
  complex hash/list/reference types, but no parameterized class type.
  - `include/qore/intern/QoreTypeSpec.h:57-65`
- `QoreClassTypeInfo` represents an ordinary class type.
  - `include/qore/intern/QoreTypeInfo.h:895`
- `QoreParseTypeInfo::resolveSubtype()` handles `hash`, `list`, `softlist`,
  `reference`, `date`, `object`, `enum`, `union`, and `code`, then rejects
  unknown subtype declarations.
  - `lib/QoreTypeInfo.cpp:2431`
- Runtime subtype resolution mirrors the same hard-coded logic.
  - `lib/QoreTypeInfo.cpp:2126`
- Existing complex list/hash/reference type infos are cached by value type.
  - `lib/QoreTypeInfo.cpp:564`
  - `lib/QoreTypeInfo.cpp:602`
  - `lib/QoreTypeInfo.cpp:690`

Class objects currently expose one class type per class:

- `qore_class_private` owns `QoreClassTypeInfo* typeInfo`.
  - `include/qore/intern/QoreClassIntern.h:2004`
- `QoreClass::getTypeInfo()` returns that one type.
  - `lib/QoreClass.cpp:5249`
- `QoreObject` is pimpl-backed, so adding per-object generic metadata is
  possible without exposing public object layout directly.
  - `include/qore/QoreObject.h:61`
  - `include/qore/QoreObject.h:703`
  - `include/qore/intern/QoreObjectIntern.h:192-197`

`Queue` and `Future` are natural pilot classes:

- `Queue::push()` and `Queue::insert()` accept `auto`.
  - `lib/QC_Queue.qpp:137`
  - `lib/QC_Queue.qpp:159`
- `Queue::get()` and `Queue::pop()` return `auto`.
  - `lib/QC_Queue.qpp:190`
  - `lib/QC_Queue.qpp:269`
- `QoreQueue` stores and returns `QoreValue`.
  - `include/qore/QoreQueue.h:65`
  - `include/qore/QoreQueue.h:86`
- `Future::get()` is abstract and returns `auto`.
  - `lib/QC_Future.qpp:73`
  - `lib/QC_Future.qpp:90`
- `FutureImpl::get()` returns `auto`. `FutureImpl` inherits via
  `vparent=Future`; the typed instantiation must propagate through that
  parent linkage so `Promise<T>::getFuture()` returns a `Future<T>` whose
  concrete impl is `FutureImpl<T>`.
  - `lib/QC_FutureImpl.qpp:78` (vparent declaration)
  - `lib/QC_FutureImpl.qpp:107`
- `QoreFuture::get()` and `QorePromise::set()` work with `QoreValue`.
  - `include/qore/QoreFuture.h:56`
  - `include/qore/QoreFuture.h:116`
- `Promise::setError(string err, string desc, auto arg)` carries a dynamic
  error payload, not the typed result. It should remain `auto` even under
  `Promise<T>`.
  - `lib/QC_Promise.qpp:164`

Function dispatch and signature matching are type-info based:

- `UserSignature::resolve()` turns parse types into resolved `QoreTypeInfo`.
  - `lib/Function.cpp:955`
- Runtime overload selection uses `QoreTypeInfo::runtimeAcceptsValue()`.
  - `lib/Function.cpp:1220`
  - `lib/Function.cpp:1327`
- Parse-time overload selection compares `QoreTypeInfo` and parse type
  information.
  - `lib/Function.cpp:1737`
- Duplicate-signature checking depends on resolved and parse-time type
  identity.
  - `lib/Function.cpp:3740`

The branch's IR/AOT work must preserve generic type metadata:

- `QoreIRNewObjectInstruction` currently carries `QoreClass*`, constructor
  variant, and source expression metadata.
  - `include/qore/intern/QoreIR.h:1353`
- `QoreIRBuilder::createNewObject()` only accepts the class and variant.
  - `lib/QoreIRBuilder.cpp:478`
- The IR interpreter's `NewObject` path resolves class/variant and calls the
  constructor without type arguments.
  - `lib/QoreIRInterpreter.cpp:1646`
  - `lib/QoreIRInterpreter.cpp:1695`
- The native/JIT helper also constructs from `QoreClass*` and variant.
  - `lib/JITRuntime.cpp:8121`
- AOT guard serialization already writes `QoreTypeInfo::getPath()`.
  - `lib/QoreAOTInstRegistry.cpp:1192`
- AOT `NewObject` serialization writes class path and constructor variant
  signature, but no generic type arguments.
  - `lib/QoreAOTInstRegistry.cpp:1325`
- `QoreAOTTypeResolver` resolves builtin, class, hashdecl, and complex types,
  but not parameterized class type paths. The `object<...>` branch uses
  `strrchr(start, '>')` to locate the closing bracket and treats the interior
  as a flat class path; for nested generic class types like
  `object<Box<int>>` or `object<Pair<int, string>>` the interior is no longer
  a class path and the lookup will silently miss.
  - `include/qore/intern/QoreAOTBinary.h:614`
  - `lib/QoreAOTBinary.cpp:1532`
  - `lib/QoreAOTBinary.cpp:1746`
  - `lib/QoreAOTBinary.cpp:1763` (`object<...>` strrchr branch)
- `QoreAOTCallTarget` is the per-call slot that bridges serialized
  `class_path + variant_sig` (written by `writeNewObject`) to the JIT helper
  at runtime. It carries `func / variant / pgm / uvb / method / qc /
  method_name / is_pseudo`, but no instantiated type info. Generic
  `NewObject` needs an additional field here so the JIT helper can construct
  with the correct parameterized object type.
  - `include/qore/intern/QoreAOT.h:69`
- AOT feature flags. The runtime feature-bit set already runs through
  `1ULL << 17` (`QORE_AOT_FEAT_FUNC_CALL_VARIANT`); every prior wire-format
  extension on this branch (CONTEXT_IR, LVPATH_SLICE, MODULE_PATH_LISTS,
  LVPATH_DELETE_EXPR, LVPATH_PATTERN, FUNC_CALL_VARIANT) added a flag and
  updated all three serialisation paths together. Generic class types will
  need a new `QORE_AOT_FEAT_*` bit, and writers/readers in
  `QoreAOTInstRegistry.cpp`, the slot-map in `QoreAOTBinary.cpp`, and the
  slot-map / handler-IR readers in `QoreAOTRuntime.cpp` must all be updated
  in lockstep.
  - `include/qore/intern/QoreAOTBinary.h:90-107`
- AOT TYPE_TABLE (`QORE_AOT_FEAT_TYPE_TABLE`, bit 9) interns pre-resolved
  type paths per blob. Generic class type infos must either be addressable
  in TYPE_TABLE (so guard/coerce sites can index them by id) or be
  re-resolved per use; that decision affects load-time cost and the
  canonical-spelling decision in Phase 0.
  - `include/qore/intern/QoreAOTBinary.h:99`

## Implementation Sketch

### Type Metadata

Add a resolved type-info representation for parameterized class types. The
exact class names are illustrative:

```c++
class QoreParameterizedClassTypeInfo : public QoreTypeInfo {
    const QoreClass* base_class;
    std::vector<const QoreTypeInfo*> type_args;
    bool or_nothing;
public:
    DLLLOCAL const QoreClass* getBaseClass() const;
    DLLLOCAL const std::vector<const QoreTypeInfo*>& getTypeArgs() const;
    DLLLOCAL size_t getArgCount() const;
    DLLLOCAL bool isOrNothing() const;
};
```

The accessors are referenced throughout the rest of this document. In
particular, the substitution context mechanism reads
`instantiated_type->getTypeArgs()[typeparam_index]` at every type-filter
site, so `getTypeArgs()` must be a cheap inline accessor returning a
`const&` to the stored vector — no copy, no allocation.

Add a matching `QoreTypeSpec` representation, either:

- a new kind such as `QTS_PARAMCLASS`, or
- an extension of `QTS_CLASS` that optionally carries type arguments.

A separate kind is probably clearer because current code often assumes
`QTS_CLASS` means one `QoreClass*` with no extra data.

Add a parameterized type-info interning cache, modelled on the existing
complex-type caches in `lib/QoreTypeInfo.cpp:564-712`:

```c++
const QoreTypeInfo* qore_get_parameterized_class_type(
    const QoreClass* qc,
    const std::vector<const QoreTypeInfo*>& args);
```

Do not allocate unbounded per-use type-info objects. Generic class type infos
should be interned/cached by `(base class, type arg pointer list, or_nothing)`.

Cache key contract:

- The key is `(base_class: const QoreClass*, type_args: vector<const
  QoreTypeInfo*>, or_nothing: bool)`.
- `type_args` elements are themselves interned `QoreTypeInfo*` pointers
  (every type used as an arg already lives in some cache: builtin
  singleton, complex-type cache, hashdecl, or another parameterized
  class entry). **Pointer-equality on the vector is sufficient** — no
  recursive structural compare is needed for cache lookup.
- Hash combines the base-class pointer, the pointer-list (each element
  hashed by pointer value), and the `or_nothing` flag.
- Constant-time hash and equality per lookup; callers do not need to
  canonicalise the path string before lookup.

Implementations must not fall back to canonical-path equality on cache
miss — that would create distinct entries whose pointer-equality and
path-equality disagree, and the substitution machinery downstream
assumes pointer-equality is the source of truth.

`Program` lifetime is handled identically to existing complex types like
`list<MyClass>`: the cache holds raw pointers, the `QoreClass*` for the
base class and any type-arg classes already pin their owning `Program`
through `spgm`, and class destruction at `Program` shutdown deletes the
class type-info pointers — leaving any cache entries that referenced them
as unreachable garbage that is cleared at process shutdown. This is the
existing pattern for `cl_map`, `ch_map`, etc.; no new ownership concept
is needed for parameterized class types.

#### Cache Lifetime And Invalidation

Follows the existing complex-type cache pattern in
`lib/QoreTypeInfo.cpp:564-712`: a single global cache per kind, pointer-keyed,
inserted on first lookup, never actively reaped during runtime, and cleared
at process shutdown via the global destructor at
`lib/QoreTypeInfo.cpp:412-429`.

`Program`-scoped type-arg classes are handled by the existing class /
`Program` ref machinery:

- The `QoreClass*` for any `Program`-scoped type-arg class holds `spgm` and
  refs its owning `Program` (`include/qore/intern/QoreClassIntern.h:2025`,
  derefed at `lib/QoreClass.cpp:848-853`).
- Objects that reference a parameterized type-info hold `pgm` on
  `qore_object_private`, which transitively pins the type-arg classes
  through their members and through `theclass`.
- When a `Program`-scoped class is destroyed, its `QoreClassTypeInfo*` is
  deleted (`QoreClass.cpp:879-880`). Cache entries in the parameterized cache
  keyed on or referencing that pointer become unreachable garbage — the same
  bounded-leak pattern that already governs `list<MyClass>` and
  `hash<string, MyClass>` today. Garbage is cleared at process shutdown via
  the same global destructor that clears `cl_map` / `ch_map` / etc.

This deliberately matches existing behaviour rather than introducing a
parameterized-class-specific lifetime invariant. If `list<MyClass>` works
today without per-`Program` cache reaping, `Queue<MyClass>` works the same
way.

### Parse Type Resolution

Extend `QoreParseTypeInfo::resolveSubtype()`:

1. If `scope` is a known hard-coded complex type, preserve current behavior.
2. Otherwise, try to resolve `scope` as a class.
3. If the class is generic, verify type-argument count and resolve each
   subtype.
4. Return the parameterized class type info.
5. If the class is not generic and subtypes were supplied, raise the existing
   "type does not take subtype declarations" error.

Also extend `resolveRuntimeSubtype()` and `QoreAOTTypeResolver` with the same
structured logic.

The current string splitters in `parser.ypp`, `QoreTypeInfo.cpp`, `qpp.cpp`,
and AOT resolution are already duplicated. Generic classes will make that
duplication more fragile. A shared structured type-argument parser would reduce
risk.

### Type Parameters

Add a symbolic type-info placeholder for type parameters:

```c++
class QoreTypeParameterInfo : public QoreTypeInfo {
    const QoreClass* owner;
    unsigned index;
    std::string name;
};
```

During parsing of:

```qore
class C<T> {
    T get();
}
```

the name `T` resolves to `QoreTypeParameterInfo(C, 0, "T")` in class member,
method, constructor, and local declaration type positions.

At use sites, substitute placeholders through an instantiation context:

```text
C<T> with args [int] => T resolves to int
```

This substitution helper should operate on arbitrary `QoreTypeInfo` trees:

```text
T                         => int
list<T>                   => list<int>
hash<string, T>           => hash<string, int>
code<T(T)>                => code<int(int)>
Queue<T>                  => Queue<int>
```

#### Substitution Rules

These rules are normative; every substitution path (parse-time, runtime,
AOT load, reflection) must agree on them.

- **Or-nothing folding (`*T`).** `*T` substitution sets the `or_nothing`
  flag on the resolved type. If `T = int`, `*T = *int`. If `T = X` where
  `X` is itself or-nothing (e.g. `T = *Y`), the result is idempotent:
  `*T = *Y`, not `**Y`. Substituting into a non-prefixed `T` never
  introduces or-nothing.
- **`auto` and `auto!`.** `*auto = *auto`. `auto!` (no-narrowing variant)
  substitutes through unchanged: `*auto! = *auto!`, `list<auto!> = list<auto!>`.
  Type parameters never observe the auto-folding behaviour because they
  are positional placeholders, not narrowing sites.
- **Containers and complex types.** Substitution recurses through
  `list<...>`, `hash<...>`, `softlist<...>`, `reference<...>`, and
  parameterized class types. The result is interned through the same
  cache as the literal form, so `list<T>` substituted to `list<int>` is
  pointer-equal to a literal `list<int>` declaration.
- **Hashdecls.** Hashdecls are leaf types under substitution.
  `Box<hash<MyDecl>>` substitutes `T → hash<MyDecl>` and the resulting
  hashdecl reference is used as-is — no descent into hashdecl members.
  Generic hashdecls (declared with their own type parameters such as
  `hashdecl Result<T> { ... }`) are out of scope for v1 per Non-goals;
  when added later, hashdecl substitution will follow the same recursive
  shape as parameterized class types.
- **`code<T(T)>` and other callable signatures.** Callable type infos
  are currently represented by signature strings; substitution must
  re-render the substituted signature and re-resolve via the existing
  code-type cache. This shares the structured type-argument parser
  proposed in `### Parse Type Resolution` — keep one parser for the whole
  family rather than open-coding callable substitution separately. After
  substitution, callable parameter and return positions use the existing
  structural-acceptance rule for `code<...>` types: a passed callable
  matches if its signature is structurally compatible with the substituted
  callable type, the same as if `code<int(int)>` had been written
  literally at the declaration site. No new acceptance logic is needed
  for substituted callables.
- **Identity.** Substituting an empty type-arg map, or substituting a
  type that contains no type parameters, must return the input pointer
  unchanged so callers can cheaply detect "nothing to do".

### Generic Class Metadata

Extend `qore_class_private` with formal type parameter metadata:

```c++
struct QoreClassTypeParam {
    std::string name;
    // Later: default type, constraint, variance.
};

std::vector<QoreClassTypeParam> type_params;
bool raw_accepts_parameterized = false;
bool raw_construction_defaults_to_auto = false;
```

`QoreTypeParameterInfo::index` (declared in the next section) refers to a
position in this `type_params` vector on the owning class. The two
structures form a single registry: the class holds the formal-name list,
the type-info placeholder holds `(owner_class, index)`, and substitution
sites look up `instantiated_type->getTypeArgs()[index]` to resolve.

The parser should reject duplicate parameter names and syntactically invalid
parameter names. Reserved builtin type keywords may be rejected if the scanner
cannot treat them as identifiers in the generic-parameter grammar. Ordinary
class, typedef, and hashdecl name conflicts are allowed and resolved by the
type-parameter lookup phase described in "Type Parameters In Class Bodies".

`raw_accepts_parameterized` is a compatibility flag for migrated builtin
classes such as `Queue`, `Channel`, `Promise`, and `Future`. It is false for
new source-defined generic classes in v1. This keeps raw metadata available
without making raw `Box` a source-level wildcard for every generic class.

`raw_construction_defaults_to_auto` is the separate construction-side
compatibility flag. It allows legacy construction spellings such as `Queue q()`
or reflection construction without explicit type arguments to create the
concrete `Queue<auto>` object. It is also false for new source-defined generic
classes in v1; those classes require explicit type arguments at every
construction entry point.

These flags live on the class, not on the raw `QoreClassTypeInfo`. The
acceptance check inside `runtimeAcceptsValue()` reaches the flag via
`type_info->getUniqueReturnClass()->priv->raw_accepts_parameterized` — a
single dereference per check, but a single source of truth. Construction
defaults read `raw_construction_defaults_to_auto` from the same class metadata.
Implementers must not duplicate either flag onto the type-info or it will
drift from the class state.

#### `QoreClass::getTypeInfo()` For Generic Classes

`qore_class_private::typeInfo` (`include/qore/intern/QoreClassIntern.h:2004`)
is an unconditional pointer today. For a generic class declaration, decide
once and document:

- `getTypeInfo()` returns the **raw, un-instantiated** class type-info
  for a generic class. It is never the same object as any `Box<int>` /
  `Box<auto>` cached parameterized type-info. Its acceptance behavior is
  controlled by `raw_accepts_parameterized`: migrated legacy builtins can use
  it as a wildcard, while new source-defined generic classes keep it as
  metadata only.
- A new accessor `getTypeInfo(const std::vector<const QoreTypeInfo*>& args)`
  returns the interned parameterized type-info for the given args. Callers
  that mean "the class type for `Box<int>`" must use this form.
- A non-generic class behaves as today: `getTypeInfo()` returns the single
  ordinary class type-info; the parameterized accessor is rejected.

This split avoids a silent regression where callers that need `Box<int>` get
`Box<auto>` merely because they call the historical zero-argument accessor.
Existing callers that read `qc->getTypeInfo()` continue to receive the raw
compatibility form. New parameter-aware callers must use the parameterized
accessor, and exact-match sites must reject raw type infos explicitly.

### Object Metadata

Store instantiated type arguments on `QoreObject` or its private implementation
so generic type acceptance can inspect any object uniformly:

```c++
class qore_object_private {
    const QoreClass* theclass;
    const QoreTypeInfo* instantiated_type;  // non-null iff theclass has type parameters
};
```

`instantiated_type` is non-null if and only if `theclass` has type parameters.
For a `Box<int>` object, it is the interned `Box<int>` type info. For a `Queue`
constructed through legacy raw syntax, it is `Queue<auto>` per the Runtime
Object Type Invariant. For a non-generic class, `instantiated_type` is always
null and callers must fall back to `theclass->getTypeInfo()`. The field is
single-meaning — never used to cache the ordinary class type for non-generic
objects.

`Program` lifetime for `Program`-scoped type-arg classes is governed by the
existing `pgm` field on `qore_object_private` and `spgm` on the type-arg
classes — the same chain that already keeps `list<MyClass>` working. No
new pinning invariant is needed.

This is preferable to storing type arguments only in builtin private data,
because `runtimeAcceptsValue(Box<int>, value)` must work for user-defined
generic classes and for builtin classes through the same path.

Builtin generic-aware private data may still cache frequently needed type args
locally:

```c++
class qore_queue_private {
    const QoreTypeInfo* value_type;
};
```

The authoritative object type should remain on the object wrapper.

#### Construction Order

`instantiated_type` must be set on `qore_object_private` **before**
constructor entry — before any private-data init, before any user
constructor body runs, before any base-class constructor delegation.
Constructor overload selection and parameter filtering use
`NewObject.object_type_info` as their substitution source, because they can
happen before a usable `self` exists. The same concrete type info is copied to
`self->instantiated_type` immediately after object allocation, before any
constructor body, base delegation, private-data init, or member write can
observe the object.

Concretely, the construction sequence is:

1. Resolve the construction expression to a concrete parameterized
   `object_type_info`. If the expression omits type arguments, this is allowed
   only when `raw_construction_defaults_to_auto` is true for the class; then
   it resolves to the all-`<auto>` instantiation.
2. Run constructor overload selection and parameter filtering using
   `object_type_info`.
3. Allocate `qore_object_private`.
4. Set `theclass` and `instantiated_type` from `object_type_info`. The
   existing `pgm` field is set per the unchanged construction-program rule.
5. Run base-class constructor delegation chain.
6. Run private-data init / user constructor body.

Construction paths that do not carry an explicit `object_type_info`
(reflection's `Class::callConstructor`, runtime-internal C++ construction
sites, deserialization of legacy-form blobs) may default to the
all-`<auto>` form only when `raw_construction_defaults_to_auto` is true
for the class. Otherwise they must raise a missing-type-arguments error. A
generic construction path must never fall back to an unsubstituted symbolic
type; missing substitution context is a hard failure, not a permissive
accept-all mode.

Reflection callers that need a specific instantiation must use the new
`Class::callConstructor(args, type_args)` overload added in Phase 2.
Without `type_args`, reflection construction only preserves legacy behavior for
classes with `raw_construction_defaults_to_auto = true`. For source-defined
generic classes, the old reflection overload raises the same missing-type-args
error as source `Box b()`.

#### Object Copy Preservation

Every object-copy path must propagate `instantiated_type` to the destination
object. Specifically:

- `QoreObject::copy()` and `qore_object_private::copyData()` must copy
  `instantiated_type` before returning the new object.
- The user-defined `copy(self)` constructor inherits the receiver's
  `instantiated_type`; it is not allowed to change the type-args.
- `clone()` private-data hooks on builtin classes must not overwrite the
  destination's `instantiated_type` from a stale source private-data
  mirror.
- For builtin classes that maintain a private-data mirror of the type-args
  (e.g. `qore_queue_private::value_type`), the mirror on the new object is
  re-derived from the new wrapper's `instantiated_type`, not copied from
  the source private data. The wrapper is the single source of truth;
  private-data mirrors are pure caches.

A regression here would let `Queue<string>` be silently produced from a
copy of `Queue<int>` when the wrapper-side type was not propagated, and
the next `push("x")` would succeed where it should have been rejected.

`raw_construction_defaults_to_auto` is irrelevant on copy paths. The
source object's `instantiated_type` is always present (Object Metadata
invariant), so the destination has a concrete type to copy and never
needs to consult the construction-defaults flag. Copy is a fully
type-determined operation regardless of the flag's value.

### Member And Local Type Substitution

`QoreMemberInfo` currently stores a resolved `const QoreTypeInfo*`. For generic
classes, member declarations can contain type parameters:

```qore
class C<T> {
    private T value;
}
```

There are two viable approaches:

1. Store symbolic member type info and substitute it at each member access or
   assignment using the object's instantiated type.
2. Store both symbolic and substituted type infos where possible.

The first approach is simpler and avoids per-instantiation class copies. It
requires assignment paths such as `acceptInputMember()` and lvalue creation to
ask for the effective type in the current object context.

The same issue exists for local declarations inside generic methods:

```qore
class C<T> {
    T copy(T value) {
        T local = value;
        return local;
    }
}
```

The runtime method context must expose the current generic substitution map so
local variable initialization and return filtering can resolve `T`.

The lvalue path family (`LValuePathAssign`, `LValuePathCompound`,
`LValuePathUnary`, `LValuePathBinaryMut`, `LValuePathTernary`) is the hot
path for member writes through `obj.value = x` on this branch. Each handler
resolves the effective member type via `LValueHelper`; the substitution
context must be reachable from those sites the same way `acceptInputMember`
gets it. A regression here would silently bypass type-arg checking on
member-write paths even when constructor and direct-assign paths are correct.

#### Substitution Context Mechanism

The substitution map must be visible to type-sensitive ops at runtime
(member-assign filter, return filter, local-init filter, lvalue path,
`acceptInputMember`).

**Reject:** thread-local generic-args pointer pushed by the dispatch
wrapper. It would work, but it hides the substitution context from IR / JIT
/ AOT analysis, fights the branch's goal of complete IR lowering, prevents
LLVM from specialising on known type arguments, and matches the kind of
implicit-context plumbing this branch has been removing elsewhere. TLS for
generic context is a hack.

**Take:** make the substitution context explicit and derive it from the
operation that is being checked:

| Site | Substitution source |
| --- | --- |
| Constructor overload selection and parameter filtering | the instantiated type carried by `NewObject.object_type_info`, for example `Box<int>` |
| Instance method overload selection and parameter filtering | the receiver object's concrete `instantiated_type` |
| Instance method body, return filtering, local init, member writes, lvalue paths | `self->instantiated_type` |
| Raw static type annotation | no substitution source; raw annotations are compatibility acceptance types, not `T -> auto` maps |
| Static generic methods in Phase 4 | explicit IR operand carrying the type-args id |
| AOT load for generic class definitions | no substitution; keep symbolic placeholders bound to the class type-parameter table |
| Reflection | raw class metadata exposes symbolic types; instantiated type reflection exposes substituted types |

Inside an instance method body, no extra operand is needed because `self`
already carries `instantiated_type`; the JIT can fold the dereference when the
object type is known after a guard. Constructor and method dispatch are
different: they happen before body execution, so they must receive the
instantiated type from the construction expression or receiver object.

Concretely: parse-time declarations store symbolic `QoreTypeInfo` trees that
can contain `QoreTypeParameterInfo` nodes. A direct `T` may be represented or
cached as `(typeparam_index, owner_class)` as an optimization, but the
normative representation must preserve nested forms such as `list<T>`,
`hash<string, T>`, `code<T(T)>`, and `Queue<T>`. Runtime filtering substitutes
the symbolic tree through the concrete substitution source above and then
applies normal acceptance against the substituted type.

### Method Signatures And Dispatch

Keep one method body and one variant definition, but allow the variant
signature to contain symbolic type parameters.

At parse time:

- Duplicate-signature checks inside the generic class compare symbolic
  signatures.
- The v1 ambiguity rule: reject overload sets where some instantiation of
  the type parameters makes the two signatures pointwise-identical under
  exact-type matching. This is decidable per overload pair by unifying the
  two symbolic type trees. The rejection test is whether all type-parameter
  equality constraints are jointly satisfiable.

Worked examples for `class C<T>` and `class Pair<T, U>`:

```qore
set(T)              vs set(int)              # collision at T=int            -> reject
set(T)              vs set(string)           # collision at T=string         -> reject
set(T)              vs set(int, int)         # different arity, no overlap   -> allow
set(T)              vs set(int, T)           # different arity, no overlap   -> allow
set(T)              vs set(U)                # collision at T=U=any value    -> reject
set(list<T>)        vs set(list<U>)          # collision at T=U=any value    -> reject
pair(T, U)          vs pair(int, string)     # collision at T=int, U=string  -> reject
pair(T, T)          vs pair(int, string)     # T=T forces both slots equal,
                                             # cannot satisfy int!=string    -> allow
pair(T, T)          vs pair(int, int)        # collision at T=int            -> reject
pair(T, U)          vs pair(U, T)            # collision at T=U              -> reject
set(T)              vs set(list<T>)          # T = list<T> requires the
                                             # occurs-check; recursive type
                                             # equation, unsatisfiable        -> allow
set(T)              vs set(*T)               # collision when T is already
                                             # or-nothing; *T folds           -> reject
set(T)              vs set(*int)             # collision at T=*int           -> reject
set(*T)             vs set(*U)               # collision at T=U=any value    -> reject
set(*T)             vs set(T)                # symmetric of set(T) vs set(*T) -> reject
get() : T           vs get() : int           # return-type-only, same arity
                                             # and param shape — rejected by
                                             # the existing duplicate-signature
                                             # rule, no new logic needed     -> reject
```

Implementations should share one helper between parse-time duplicate
detection and AOT load-time signature reconciliation, since they apply the
same rule.

The decision procedure is small first-order unification over v1 type trees,
not the intuitive "any type-param-only slot collides with any concrete sibling
slot" rule — that simple rule would over-reject `pair(T, T)` vs
`pair(int, string)` and under-specify `set(T)` vs `set(U)`. Concretely:

1. Walk both signatures pointwise. Different arity → no collision, allow.
2. For each slot pair, unify the left and right type trees structurally.
   Concrete type names must match exactly. Parameterized class/list/hash/code
   nodes unify only if their base type and arity match, then recurse into
   children.
3. `or_nothing` (`*`) is a wrapper in the type tree, but substitution is
   idempotent: if `T = *int`, then `*T = *int`, not `**int`. The unifier must
   model this folding. Therefore `T` and `*T` collide: choose any or-nothing
   type for `T`, and both signatures become identical.
4. Type parameters are variables. Binding a variable to a concrete type, to
   another variable, or to a structured type is allowed if it does not
   contradict an existing binding.
5. If a type parameter is bound to two incompatible concrete or structured
   types across slots (the `pair(T, T)` vs `pair(int, string)` case),
   unification fails — allow.
6. Reject recursive type equations explicitly in v1. A type parameter must not
   be bound to a type tree that contains itself, for example `T = list<T>`.
   This acts as the occurs-check and keeps the algorithm finite.
7. If all bindings are jointly satisfiable, the signatures collide under that
   instantiation — reject.

Linear in signature size for v1's no-bounds, no-defaults rules. The
helper returns `(collides: bool, witness_substitution: ?map)`; the
witness is useful for the parse-error message ("rejected because
instantiating `T = int, U = string` makes these signatures identical").

Reuse the existing `PARSE-DUPLICATE-SIGNATURE-EXCEPTION` error code and
embed the witness instantiation in the message:

```text
PARSE-DUPLICATE-SIGNATURE-EXCEPTION:
  "method 'set' overloads collide under generic instantiation:
   set(T) and set(int) become identical when T=int"
```

Witness-bearing messages are required for any rejection that the
non-generic duplicate-signature rule would not have caught — otherwise
the user has no way to see which instantiation triggered the conflict.

The `set(T)` vs `set(*T)` rejection in particular may surprise users who
expect those to overload like non-generic `set(int)` and `set(*int)` do.
The rejection follows from the `*T` idempotency rule in the Substitution
Rules subsection: any or-nothing instantiation collapses `*T` to `T` and
the two signatures become pointwise identical. The witness in that case
binds `T` to any or-nothing type, e.g. `T = *int`, and the error message
should make this explicit:

```text
PARSE-DUPLICATE-SIGNATURE-EXCEPTION:
  "method 'set' overloads collide under generic instantiation:
   set(T) and set(*T) become identical when T = *int (and any other
   or-nothing type), because *T folds when T is already or-nothing"
```

To differentiate at the value boundary, route through a non-generic
helper that takes the substituted concrete type, or wait for v2
method-level generic functions which can specialise on or-nothing.
Migrating non-generic code that relied on `set(int)` / `set(*int)`
overloads to the generic form requires consolidating both into a single
`set(*T)` and branching on `exists value` inside the body.

At call time:

- Constructor calls such as `new C<int>(...)` substitute `T -> int` from
  the `NewObject.object_type_info` before constructor overload selection,
  parameter filtering, default argument filtering, and direct-call guard
  selection.
- Instance method calls substitute from the receiver object's concrete
  `instantiated_type` before parameter acceptance, default argument filtering,
  return filtering, and direct-call guard selection.
- If `self` is `C<auto>`, substitute `T -> auto`.
- If the static target type is raw `C`, do not substitute `T -> auto`.
  Instance-method filtering must use the receiver object's actual
  instantiated type arguments. Raw type annotations are compatibility
  acceptance types, not substitution maps.
- Substitution flows through method-parameter type declarations the same
  way it flows through return and member types. If `class Container<T>`
  declares `sub useBox(Box<T> b)`, then a call on `Container<int>`
  resolves the parameter as `Box<int>` by substituting `T → int` from the
  receiver's `instantiated_type` before the parameter is filtered. Nested
  parameterized types in parameter declarations (`map<T, list<U>>`,
  `code<T(T)>`) follow the same substitution rules from the `Substitution
  Rules` subsection.

The existing dispatch code in `Function.cpp` can remain structurally similar,
but it needs a generic substitution context before comparing or filtering
types.

### QPP Support

Extend qpp class headers and type parsing:

```qore
qclass Queue<T> [dom=THREAD_CLASS; arg=Queue* q; ns=Qore::Thread];

nothing Queue::push(T arg, timeout timeout_ms = 0) {
    ...
}

T Queue::pop(timeout timeout_ms = 0) {
    ...
}
```

qpp should not treat `Queue<int>` as a class literally named `Queue<int>`.
It must parse the base class name and type arguments structurally.

Generated metadata should preserve symbolic `T` in the class declaration and
method signatures. Runtime wrappers should receive the same `QoreValue` values
as today; generic enforcement happens in normal parameter and return filtering,
plus class-specific checks where needed.

#### Legacy-Compatibility Class Header Attributes

The two class-gated compatibility flags must be settable from qpp source so
the policy is visible at the qclass declaration. New attributes parsed by
the qpp class-header bracket list:

```text
qclass Queue<T> [
    dom=THREAD_CLASS;
    arg=Queue* q;
    ns=Qore::Thread;
    legacy_raw                   # sets both compat flags
];
```

The three accepted spellings mirror the Phase 4 source modifiers:

- `legacy_raw` — sets both `raw_accepts_parameterized = true` and
  `raw_construction_defaults_to_auto = true`. Use this for full migration
  compat (the default for `Queue`, `Channel`, `Promise`, `Future`).
- `legacy_raw_accepts` — sets only `raw_accepts_parameterized = true`.
- `legacy_raw_construct` — sets only
  `raw_construction_defaults_to_auto = true`.

These attributes must be opt-in. New generic builtins introduced after
the Phase 2 pilots get strict (no-flag) semantics by default, so a future
`qclass TaskGroup<T>` does not silently inherit migration compat. The
qpp class-header parser raises a hard error if any of these attributes
appears on a non-generic `qclass`.

#### Parameterized vparent

The narrow Phase 2 generic-inheritance carve-out (see "Inheritance") needs
qpp to accept a parameterized type as the `vparent` value of a class
header. Concrete shape, used for `FutureImpl<T>`:

```text
qclass FutureImpl<T> [
    dom=THREAD_CLASS;
    arg=QoreFuture* f;
    ns=Qore::Thread;
    vparent=Future<T>            # parameterized vparent — passes T through
];
```

Constraints on the v1 parameterized-vparent parser:

- The vparent base name must resolve to another generic class declared
  earlier in the qpp processing order.
- Every type parameter used in the vparent must be a formal parameter of
  the enclosing class. The parser rejects fresh names introduced only in
  the vparent (e.g. `vparent=Future<U>` where `U` is not declared on the
  derived class).
- Concrete type arguments (`vparent=Future<int>`) are also accepted and
  pass through unchanged.

Without this carve-out the `Promise<T>::getFuture() -> Future<T>` /
concrete `FutureImpl<T>` linkage cannot be expressed in builtin metadata.

### Queue<T>

`Queue<T>` can be implemented without changing the queue storage value type.

Private data additions:

```c++
const QoreTypeInfo* value_type;  // auto for concrete Queue<auto>
```

Constructor behavior:

- `Queue()` in legacy code constructs `Queue<auto>`.
- `Queue<int>()` records `int` as the value type on the object/private data.

Method behavior:

- `push(T arg)` and `insert(T arg)` filter or reject before enqueueing.
- `get()` and `pop()` return `T`.
- Stored values remain `QoreValue`.

`tryGet()` and `tryPop()` currently return a wrapper hash with keys such as
`"value"`. A fully typed result wants either a generic result class or generic
hashdecls:

```qore
QueueResult<T>
hash<QueueTryResult<T>>
```

That should not block `Queue<T>`. v1 can leave these methods as `auto` or
existing wrapper hash return types.

### Promise<T> And Future<T>

`Future<T>` should be paired with `Promise<T>`; otherwise type safety is only
documented at the consumer side.

Shared private data additions:

```c++
const QoreTypeInfo* result_type;  // auto for concrete Promise<auto>/Future<auto>
```

Behavior:

- `Promise<T>::set(T value)` filters before fulfilling.
- `Promise<T>::getFuture()` returns `Future<T>`.
- `Future<T>::get()` returns `T`.
- `FutureImpl<T>` preserves the same type argument as the backing private
  future.
- `FutureImpl<T>` maps to its abstract parent as `Future<T>` through the
  narrow QPP parameterized vparent support included in Phase 2.
- Existing raw `Promise` construction and untyped future producers create
  concrete `Promise<auto>` / `Future<auto>` objects. Raw `Promise` and
  `Future` annotations remain compatibility acceptance types as described in
  "Raw Or Missing Type Arguments".

`call_async()` can later infer `Future<ReturnType>` from the callable's return
type. That is useful but does not have to be part of the first generic class
patch.

### Channel<T>

`Channel<T>` is the strongest additional Qore API candidate from the scan. It
has the same implementation shape as `Queue<T>` but represents a concurrent
typed handoff instead of a buffered container.

Current surface:

- `lib/QC_Channel.qpp` declares `Channel` with `send(auto)`, `recv() -> auto`,
  `trySend(auto)`, `tryRecv() -> auto`, and `iterator() -> ChannelIterator`.
- `lib/QC_ChannelIterator.qpp` declares `ChannelIterator` with
  `getValue() -> auto`.
- `include/qore/intern/QoreChannel.h` stores `QoreValue`, so typed channels do
  not require typed C++ storage.

Suggested generic shape:

```qore
qclass Channel<T> [dom=THREAD_CLASS; arg=QoreChannel* ch; ns=Qore::Thread];

nothing Channel::send(T value, timeout timeout_ms = 0);
bool Channel::trySend(T value);
*T Channel::recv(timeout timeout_ms = 0);
auto Channel::tryRecv();
```

`recv()` returns `*T` for all `T` because closed or drained channels produce
`NOTHING`. Note that this is a different signature from raw
`Channel::recv() -> auto`; that asymmetry is intentional under the committed
raw / `<auto>` split — raw and parameterized are distinct types and need not
share signatures. Concretely:

- `Channel::recv() -> auto` (raw, unchanged from legacy).
- `Channel<int>::recv() -> *int`.
- `Channel<auto>::recv() -> *auto`.

`tryRecv()` has the same result-wrapper issue as `Queue<T>::tryGet()` and can
remain `auto` until generic result records or generic hashdecls exist.

Under `Channel<*T>` (where `T` itself is or-nothing, e.g. `Channel<*int>`),
the or-nothing-folding idempotency rule from "Substitution Rules" means
`recv()` returns `*int`, not `**int`. Drained-channel NOTHING is therefore
indistinguishable from a sender-supplied NOTHING in the return value. Use
`tryRecv()` (which returns its result-wrapper hash) to distinguish the two
cases when the channel's value type is itself or-nothing.

This should be in the builtin/QPP pilot phase with `Queue<T>`. It exercises
the same runtime type-argument storage and producer-boundary filtering.
`Channel::iterator()` can remain raw `ChannelIterator` in Phase 2. The typed
iterator surface:

```qore
ChannelIterator<T> Channel::iterator();

qclass ChannelIterator<T> [
    dom=THREAD_CLASS;
    arg=QoreChannelIterator* i;
    ns=Qore::Thread;
    vparent=AbstractIterator<T>
];

T ChannelIterator::getValue();
```

belongs to Phase 2b with the rest of `AbstractIterator<T>`. That keeps the
initial channel pilot focused on producer-boundary filtering and return-type
substitution, and avoids coupling it to the broader iterator hierarchy
migration.

### AbstractIterator<T> Family

The iterator hierarchy is one of the highest-leverage applications of
parameterized class types. Today every iterator subclass returns `auto` from
`getValue()` or inherits from a raw iterator base even when the actual element
type is statically known. The runtime already computes element types for the
lazy `map` / `select` family via
`FunctionalOperatorInterface::getValueTypeImpl()`
(`include/qore/intern/QoreSelectOperatorNode.h:102, 123, 147, 172` and the
matching map operator definitions); generics expose that existing internal
type-derivation through the user-facing iterator surface.

Iterator typing should be separated into **Phase 2b**, after the core builtin
pilots (`Queue<T>`, `Channel<T>`, `Promise<T>`, `Future<T>`) have proven the
parameterized class type machinery. The iterator hierarchy is broad enough to
deserve its own implementation slice: it touches many QPP classes, existing
intermediate iterator bases, the functional-operator return paths, and several
legacy iterator classes whose `getValue()` shape is not a simple "element
type" mapping.

The first Phase 2b step is a hierarchy audit. Do not bypass existing iterator
base classes just to attach `AbstractIterator<T>` directly. The typed hierarchy
should preserve the current parent structure where the parent provides real
behaviour or API:

```text
qclass AbstractIterator<T> [...];
abstract T AbstractIterator::getValue();

qclass AbstractBidirectionalIterator<T> [
    vparent=AbstractIterator<T>;
    ...
];

qclass AbstractQuantifiedIterator<T> [
    vparent=AbstractIterator<T>;
    ...
];

qclass AbstractQuantifiedBidirectionalIterator<T> [
    vparent=AbstractBidirectionalIterator<T>,AbstractQuantifiedIterator<T>;
    ...
];
```

Multi-vparent qpp syntax already exists today —
`lib/QC_AbstractQuantifiedBidirectionalIterator.qpp:36` declares
`vparent=AbstractBidirectionalIterator,AbstractQuantifiedIterator`. Phase 2b
extends each vparent slot to accept a parameterized type, the same extension
introduced for the single-vparent case (`vparent=Future<T>`) in Phase 2.
There is no fundamental qpp limitation to work around.

The diamond through `AbstractIterator<T>` must resolve to the same
`AbstractIterator<T>` instantiation on both paths. Concrete qpp parse-time
rule: when a class inherits multiple parameterized vparents that share a
common ancestor, the type-arg expressions on each vparent must be
syntactically identical. `vparent=AbstractBidirectionalIterator<T>,
AbstractQuantifiedIterator<T>` is valid (both paths reach
`AbstractIterator<T>` with the same `T`);
`vparent=AbstractBidirectionalIterator<int>,AbstractQuantifiedIterator<string>`
is rejected because the two paths to `AbstractIterator` would resolve to
incompatible instantiations. The check is local to a single qclass header —
no cross-class inference needed.

The work splits into four Phase 2b groups plus the later Phase 4
source-defined iterator group.

#### Group A: Concrete-Arg Vparent Iterators (Phase 2b)

These iterators have a fixed element type that does not depend on a
caller-supplied type argument. They use the same parameterized-vparent qpp
machinery introduced for `FutureImpl<T> [vparent=Future<T>]`, but with a
concrete arg instead of a passed-through formal:

```text
qclass StringCharIterator [vparent=AbstractIterator<int>; ...];

qclass AbstractLineIterator [
    vparent=AbstractIterator<string>;
    ...
];
qclass DataLineIterator [vparent=AbstractLineIterator; ...];
qclass FileLineIterator [vparent=AbstractLineIterator; ...];
qclass InputStreamLineIterator [vparent=AbstractLineIterator; ...];

qclass StringSplitIterator [vparent=AbstractIterator<string>; ...];
qclass StringRegexSplitIterator [vparent=AbstractIterator<string>; ...];
```

`StringCharIterator` yields integer codepoints, not strings. Note that
`StringCharIterator::getValue()` already declares its return type as `int`
today via a covariant override (`lib/QC_StringCharIterator.qpp:164`); direct
calls on `StringCharIterator` already get statically-typed `int`. The Phase
2b change replaces the raw `vparent=AbstractIterator` with
`vparent=AbstractIterator<int>`, which aligns the inherited contract and
narrows upcast / dynamic-base call sites — the actual user-visible win is
for callers that use the parent type, not direct `StringCharIterator`
references.

`StringSplitIterator` and `StringRegexSplitIterator` inherit from
`AbstractIterator` directly today (`lib/QC_StringSplitIterator.qpp:189`,
`lib/QC_StringRegexSplitIterator.qpp:212`), not from `AbstractLineIterator`.
The Phase 2b shape preserves that — they become direct
`vparent=AbstractIterator<string>`, not children of the typed
`AbstractLineIterator`. Early readers might expect them under
`AbstractLineIterator`; they are not, and Phase 2b should not change that.

`RangeIterator` is also not a fixed `int` iterator today: its optional
`auto val` constructor argument changes `getValue()` from the range index
to that supplied value. Phase 2b should either leave `RangeIterator`
raw/`auto`, or give it an explicit generic shape such as `RangeIterator<T>`
only after deciding how the constructor-selected element type is represented
without general constructor type inference.

#### Group B: Shape-Preserving Builtin Iterators (Phase 2b)

Shape-preserving builtin iterators carry a type argument and pass it through
the existing iterator base hierarchy:

```text
qclass ListIterator<T> [vparent=AbstractQuantifiedBidirectionalIterator<T>; ...];
qclass ListReverseIterator<T> [vparent=ListIterator<T>; ...];
qclass SingleValueIterator<T> [vparent=AbstractIterator<T>; ...];
qclass ChannelIterator<T> [vparent=AbstractIterator<T>; ...];
qclass HashIterator<V> [vparent=AbstractQuantifiedBidirectionalIterator<V>; ...];
qclass HashReverseIterator<V> [vparent=HashIterator<V>; ...];
```

These vparent shapes preserve the existing parent chains.
`ChannelIterator` inherits `AbstractIterator` directly today
(`lib/QC_ChannelIterator.qpp:67`), and Phase 2b keeps that single-vparent
shape — the simplest case in Group B. `ListIterator` and `HashIterator`
already inherit `AbstractQuantifiedBidirectionalIterator` (where the
diamond-resolution rule from the preamble applies), and the reverse
iterators preserve their existing `vparent=ListIterator` /
`vparent=HashIterator` chains, just with the type argument passed through.

`HashIterator<V>` is over `hash<string, V>`: `getValue() -> V`,
`getKeyValue() -> V`, `getKey() -> string` (always), and `getValuePair()`
returns a typed wrapper hashdecl such as `hash<HashIterPair<V>>` once Phase 4
generic hashdecls exist.

`SingleValueIterator<T>` faces the same legacy-`auto` constructor
compatibility issue as `RangeIterator` in Group A: today's
`SingleValueIterator::constructor(auto v)` (`lib/QC_SingleValueIterator.qpp:66`)
accepts arbitrary values, so a typed `SingleValueIterator<T>::constructor(T v)`
breaks every legacy `SingleValueIterator(unknown_value)` call site. Phase 2b
must either set the `legacy_raw_construct` qpp attribute so
`SingleValueIterator(x)` continues to construct `SingleValueIterator<auto>`,
or defer typing until an explicit migration plan is chosen for the optional
constructor argument. Group B should not silently narrow this constructor.

Phase 2b for Group B applies only to iterators whose constructor signatures
already specify the element type (`ListIterator`'s `list<auto> l`-style
constructors substitute through to `T` cleanly). Iterators with `auto`
constructor arguments need the same legacy-flag treatment, not silent
narrowing.

**Untyped methods on otherwise-typed iterators stay raw in Phase 2b.**
`HashIterator<V>::getValuePair()` keeps `hash<auto>` as its return type
because there is no Phase 2b way to spell `hash<HashIterPair<V>>` —
generic hashdecls are deferred to Phase 4. The same rule applies to
`Object*Iterator::getValuePair()` once those are typed, and to any other
wrapper-shape methods on the typed iterators. Element-shaped methods
(`getValue()`, `getKeyValue()`, `getKey() -> string` for keys) narrow in
Phase 2b; wrapper-shaped methods narrow only when Phase 4 generic
hashdecls / result records exist to express their payloads. Phase 2b
tests must assert that the wrapper-shaped methods stay `hash<auto>` and
are not accidentally typed as `hash<V>`.

#### Group C: Key, Pair, Hash-List, And Object Iterators (Phase 2b/Phase 4)

Some existing iterator subclasses intentionally change what `getValue()`
means. They should not simply inherit `HashIterator<V>` or
`ObjectIterator<V>` once those parents mean `AbstractIterator<V>`:

- `HashKeyIterator` / `ObjectKeyIterator` yield keys, so at the value
  level they would look like `AbstractIterator<string>` candidates, but
  they currently inherit `HashIterator` / `ObjectIterator`
  (`lib/QC_HashKeyIterator.qpp:65`, `lib/QC_ObjectIterator.qpp:70`).
  **Recommended Phase 2b shape: `vparent=HashIterator<auto>` with the
  existing covariant `string getValue()` override** (and the analogous
  form for ObjectKeyIterator). This:
  - preserves the existing parent chain unchanged;
  - exposes the typed `HashIterator<auto>` base — downstream callers
    that reference the parent type get the typed surface;
  - keeps the covariant `string` return on `getValue()` so direct calls
    on `HashKeyIterator` are still statically `string`;
  - does not invent a `HashKeyIterator<V>` form whose `V` parameter is
    not actually used by `getValue()`.

  Phase 4 with generic hashdecls / result records can revisit if a
  cleaner shape (e.g. a typed key-iterator base) emerges.
- `HashPairIterator` / `ObjectPairIterator` yield a `{key, value}` hash, so
  they should remain raw/`hash<auto>` until generic hashdecls or generic
  result records can express `HashIterPair<V>`.
- `HashListIterator`, `ListHashIterator`, and object/hash row iterators yield
  row-shaped hashes or object-derived values and need a separate row-shape
  decision.

Phase 2b may type the key-only iterators only after that compatibility decision
is made. Pair and row-shape iterators should either stay raw for compatibility
or move with Phase 4 generic hashdecl/result-record work. This avoids
promising `HashPairIterator<V> : AbstractIterator<V>`, which would be false.

#### Group D: Functional Operator Result Typing (Phase 2b)

The `map` and `select` family already compute element types internally via
`FunctionalOperatorInterface::getValueTypeImpl()`. The user-facing result
spelling is currently `auto` or raw `AbstractIterator`. Once
`AbstractIterator<T>` exists, the operator return types should narrow:

```text
map(coll, code<R(T)>)        -> AbstractIterator<R>
                                  # R is the callable's return type
select(coll, code)           -> AbstractIterator<T>
                                  # T is coll's element type
foldr(coll, ...) / foldl(coll, ...)
                              # already typed through callable signature
```

Implementation of this group is mostly **wrapping existing internal type info
in a parameterized `AbstractIterator<T>`** at the operator's return-type site;
it should not invent new derivation logic. The result-typed operator chains
then flow through to consumers automatically.

#### Group E: Source-Defined Generic Iterators (Phase 4)

User-defined and qlib generic iterators that inherit from
`AbstractIterator<T>` need full Phase 4 source-level generic inheritance
(`class Derived<T> inherits Base<T>`). The clearest example today is
`Mapper::AbstractMapperIterator<RecT>` (qlib Qore source). Once Phase 4
generic inheritance lands, Qore source code can declare:

```qore
public class TypedMapperIterator<RecT> inherits AbstractIterator<RecT> {
    RecT getValue() {
        ...
    }
}
```

The substitution context machinery proposed in `### Member And Local Type
Substitution` carries through the inherited base-class type arguments per
the Phase 4 generic-inheritance work; no iterator-specific machinery
needed beyond what generic inheritance already provides.

#### Substitution Through Concrete-Vparent Iterators

Group A is straightforward because the concrete arg is fixed at the qpp
parser level — no substitution map needed at runtime. The qpp class-header
parser sees `vparent=AbstractIterator<string>`, resolves `AbstractIterator`
+ args at qpp time, and writes the resolved parameterized type-info into
the generated registration. There is no formal `T` to substitute on concrete
iterators such as `StringCharIterator` or `AbstractLineIterator`. This is a
meaningful simplification compared to Group B, but it still belongs in Phase
2b because it depends on the typed iterator base hierarchy.

### Parser And Scanner

Class declaration scanning currently returns a `CLASS_STRING` token for
`class WORD`. Generic declarations need one of:

- a widened scanner token that includes the generic parameter list, or
- a cleaner grammar-level parse of `TOK_CLASS IDENTIFIER '<' ... '>'`.

The second approach is more maintainable, but it may require more churn because
the current parser relies on class-name tokens that already include the word
after `class`.

Type-use positions already accept `ANGLE_IDENTIFIER`, but `new Box<int>()`
currently follows the complex-type construction path, not ordinary object
construction. Constructor parsing must distinguish parameterized class
construction from existing complex value construction.

The cleanest dispatch keeps the existing scanner shape: `Box<int>` lexes
as `ANGLE_IDENTIFIER` (single token) regardless of whether `Box` names a
class or a complex value type. The grammar production for `new
ANGLE_IDENTIFIER ( ... )` is shared; the semantic action resolves the
base scope at parse-commit time:

- If the resolved scope names a class (generic or otherwise), emit a
  parameterized-class constructor call (`new` → `NewObjectCallNode` /
  `ScopedObjectCallNode` with the parameterized object type info).
- If the resolved scope names a hard-coded complex type (`hash`, `list`,
  `softlist`, `reference`, `code`), emit the existing complex-value
  construction path.
- If neither resolves, raise the existing parse error.

This keeps the grammar single-production, defers the dispatch to the
point where the type is actually known, and avoids splitting `new` into
two grammar branches that would both need to backtrack on `<`.

Any parser/scanner change must be mirrored in `modules/astparser/src/` (the
flex/bison front-end at `ast_scanner.lpp` / `ast_parser.ypp`) and in the
tree-sitter grammar.

#### Tree-sitter Grammar

The tree-sitter Qore grammar at
`modules/astparser/grammars/tree-sitter-qore/grammar.js:267` currently
declares:

```text
class_declaration: $ => seq(
  optional($.modifiers),
  'class',
  field('name', choice($.identifier, $.scoped_identifier)),
  optional($.superclass_list),
  ...
)
```

There is no type-parameter slot. Generic class support requires:

- a new `type_parameter_list` rule (`<` commaSep1(IDENTIFIER) `>`),
- threading it into `class_declaration` after the name and into the type
  rule so `Box<int>`, `Pair<int, string>`, `code<T(T)>`, and nested forms
  parse uniformly,
- regenerating `parser.c` and reviewing the generated CST node names that
  `AstTreeHolder` / `CSTSearcher` walk.

This is more than a one-line update because tree-sitter ships generated
sources in-tree.

#### qpp

`lib/qpp.cpp:6462` parses `qclass <name> [...]` by reading bytes between the
keyword and the first whitespace as the class name. With `qclass Queue<T>
[dom=...]` the entire string `Queue<T>` lands in `cn` and is passed
downstream as if it were the bare class name. Generic qpp support needs:

- an explicit base-name + type-arg-list split at the qclass header parser,
- propagation of the type-parameter list into the generated `QoreClass`
  registration so the class's resolved type-info exposes formal type params,
- and a structured method-signature parser so `nothing Queue::push(T arg,
  ...)` resolves `T` symbolically against the enclosing class's type-param
  table rather than searching for a class literally named `T`.

### IR, JIT, And AOT

Generic class types should not require new compiled method bodies for each
instantiation. They do require richer metadata.

IR `NewObject` should carry either:

- `const QoreTypeInfo* object_type_info`, or
- `(QoreClass*, vector<const QoreTypeInfo*>)`

The first option is simpler and matches type guard serialization.

The interpreter and JIT runtime helpers then construct the object with the
instantiated type info:

```text
new Box<int>(1)
  -> class: Box
  -> object type: Box<int>
  -> variant: constructor(T) substituted as constructor(int)
```

AOT changes:

- `QORE_AOT_FEAT_CLASS_TYPE_PARAMS` (`1ULL << 44`) marks class records that
  include the ordered source generic type-parameter table.
- AOT type-path writers must use the AOT canonical type spelling, not raw
  `QoreTypeInfo::getPath()`, so symbolic type parameters survive in guard,
  slot-map, expression, and generated LLVM helper metadata.
- `NewObject` must serialize the instantiated object type path in addition to
  the base class path and constructor variant signature, or replace class path
  with a canonical object type path.
- Variant signatures must include substituted generic paths where the variant
  is serialized for an instantiated call.
- **Symbolic type parameters in generic-class qmods.** A generic class is
  compiled once with symbolic signatures (`T`, `K`, `V`); substitution
  happens at the consumer's call site. The AOT writer for a generic class
  must:
  - Emit a per-class type-parameter table (ordered list of formal names).
  - Emit method signatures with placeholder tokens instead of attempting
    to resolve to a class. The implemented token form is
    `typeparam<owner-path, index, name>` with the usual leading `*` for
    or-nothing type parameters, for example `typeparam<::Ns::Box, 0, T>`.
  - Emit member, local, parameter, and return declarations as symbolic
    type-info trees. A direct `T` is encoded as a placeholder token; nested
    forms such as `list<T>` and `code<T(T)>` are encoded by the existing
    AOT type-string format with placeholder tokens occupying leaf
    positions:

    ```text
    T            -> typeparam<::Ns::Box, 0, T>
    list<T>      -> list<typeparam<::Ns::Box, 0, T>>
    *list<T>     -> *list<typeparam<::Ns::Box, 0, T>>
    hash<string,T>
                 -> hash<string, typeparam<::Ns::Box, 0, T>>
    code<T(T)>   -> code<typeparam<::Ns::Box, 0, T>(typeparam<::Ns::Box, 0, T>)>
    Box<T>       -> object<::Ns::Box<typeparam<::Ns::Box, 0, T>>>
    ```

    The reader reuses the existing structured AOT type-string parser; the
    only new capability is recognising the `tparam:` leaf and not
    attempting class lookup for it.

  The AOT reader must:
  - Recognise the placeholder token and not attempt class lookup.
  - Bind the placeholder to the per-class type-parameter table at qmod
    load time.
  - Defer substitution until the relevant context is known: constructor
    dispatch uses `NewObject.object_type_info`, instance method dispatch and
    bodies use the receiver object's `instantiated_type`, and static generic
    methods use the Phase 4 IR-operand type-args id.

  This is a separate wire-format concern from the parameterized-class
  type-info wire format and gates Phase 3 (source-defined generic
  classes) under AOT.
- `QoreAOTTypeResolver` must parse and resolve parameterized class paths. The
  current `object<...>` branch (`lib/QoreAOTBinary.cpp:1763`) uses
  `strrchr(start, '>')` and treats the interior as a flat class path, which
  silently misses on `object<Box<int>>` / `object<Pair<int, string>>`. This
  must be replaced with the same structured type-argument parser proposed
  above for `QoreParseTypeInfo::resolveSubtype()`.
- `QoreAOTCallTarget` (`include/qore/intern/QoreAOT.h:69`) is the slot the
  JIT helper at `JITRuntime.cpp:8121` reads from at constructor-call time.
  Generic `NewObject` needs an additional field on this struct (e.g.
  `const QoreTypeInfo* instantiated_type`) populated by the slot-map reader
  from the new wire data, so the helper can pass the parameterized type info
  to `execConstructor` / object init.
- Both serialisation paths must be updated together: the per-instruction
  `writeNewObject` / `readNewObject` in `QoreAOTInstRegistry.cpp`, the
  slot-map writer in `QoreAOTBinary.cpp`, and the slot-map / handler-IR
  readers in `QoreAOTRuntime.cpp`. A wire-format change that lands in only
  one of those paths is the most common shape of AOT regression on this
  branch (see the LVPATH_SLICE landing notes for the same gotcha).
- Decide whether parameterized class type infos can live in TYPE_TABLE
  (`QORE_AOT_FEAT_TYPE_TABLE`, bit 9). If yes, guard/coerce sites can index
  by id; if no, they re-resolve per use. Affects load-time cost.

### Reflection And Documentation

Reflection should expose:

- formal type parameters on generic classes
- instantiated type arguments on parameterized class type infos
- method signatures containing symbolic type params for class definitions
- substituted signatures where reflecting a specific instantiated type, if the
  reflection API has such a concept

The reflection module lives at `modules/reflection/src/`. New accessors
needed (Phase 2 for read-only inspection of builtin generics, Phase 3 for
source-defined generic classes):

- `Class::getTypeParameters() -> *list<string>` — returns the formal
  type-parameter names for a generic class, NOTHING for non-generic
  classes.
- `Type::getTypeArguments() -> *list<Type>` — returns the resolved type
  arguments on a parameterized class type info, NOTHING for
  non-parameterized types. The result is recursive: each `Type` element
  may itself be parameterized and return its own type-arguments via the
  same accessor. `Type` for `Box<Pair<int, string>>` returns
  `[Pair<int, string>]`; calling `getTypeArguments()` on that element
  returns `[int, string]`. This lets reflection consumers walk arbitrary
  nested generic structure with one accessor.
- `Type::isParameterized() -> bool` — true when the type info is a
  parameterized class type info.
- `AbstractMethodVariant::getSymbolicSignature() -> *list<string>` —
  returns the unresolved symbolic spelling of the variant signature
  (e.g. `["T", "*int"]`) for variants belonging to generic classes; the
  existing signature accessors return the resolved form, which for a
  generic class means the substituted form when called via a
  parameterized type info.

Reflection's own dynamic-call APIs (`Class::callStaticMethod`,
`Method::call`, etc.) intentionally take and return `auto` and are not
parameterized. Reflection exposes generic metadata; it does not become a
generic API itself.

Doxygen/Qdx support needs the same parser support as the runtime parser. The
standard library docs for `Queue`, `Channel`, `Promise`, and `Future` should
show both raw legacy and typed forms.

## Additional API Candidate Scan

The scan looked for Qore APIs where `auto`, `list<auto>`, `hash<auto>`, raw
`Future`, raw `Queue`, or raw iterator/poll classes are used to carry a value
whose type is logically stable across a class instance.

| Candidate | Current surface | Possible generic form | Assessment |
| --- | --- | --- | --- |
| `Channel<T>` / `ChannelIterator<T>` | `Channel::send(auto)`, `recv() -> auto`, `ChannelIterator::getValue() -> auto` | Phase 2: `send(T)`, `recv() -> *T`; Phase 2b: `iterator() -> ChannelIterator<T>` | `Channel<T>` is a strong Phase 2 candidate, but its iterator should move with the typed iterator hierarchy in Phase 2b. |
| `AbstractIterator<T>` family | `AbstractIterator::getValue() -> auto`; many concrete iterators have a fixed return type (line iterators, `StringCharIterator`, `HashKeyIterator`) but still surface raw iterator bases; lazy `map`/`select` already compute element types internally via `FunctionalOperatorInterface::getValueTypeImpl()` | `AbstractIterator<T>::getValue() -> T`; generic intermediate bases (`AbstractBidirectionalIterator<T>`, `AbstractQuantifiedIterator<T>`, `AbstractQuantifiedBidirectionalIterator<T>`); concrete-arg vparent for fixed-type iterators; generic-T vparent for shape-preserving builtin iterators; `map` / `select` return `AbstractIterator<R>` / `AbstractIterator<T>`; source-defined generic iterators inherit `AbstractIterator<T>` from Qore source | **Phase 2b (builtin iterator hierarchy + functional-operator return typing) + Phase 4 (source-defined / qlib generic iterators).** See "AbstractIterator<T> Family". This is high-value but broad enough to keep separate from the initial builtin pilots. |
| `AbstractPollOperation<T>` | `AbstractPollOperation::getOutput() -> auto`; HTTP/Ftp/Socket/WebSocket poll operations return known shapes through `getOutput()` | `AbstractPollOperation<T>::getOutput() -> T` | Strong async API candidate, especially with `Future<T>`. Needs typed result records/hashdecls for best HTTP/WebSocket surfaces. |
| Event and warning queues | HTTPClient, Socket, FtpClient, Datasource, and DatasourcePool accept raw `Queue` plus `auto arg` and push event hashes | `Queue<hash<EventInfo>>`, possibly `Queue<hash<EventInfo<Arg>>>` | Useful once `Queue<T>` exists, but event payloads are heterogeneous and include user `arg`, so generic hashdecls would improve the design. |
| DataProvider records and pipelines | `AbstractDataProviderRecordIterator::getValue() -> hash<auto>`, `AbstractDataProcessor::processRecord(auto)`, `DataProviderPipeline::submit(auto)`, async methods return raw `Future` | `AbstractDataProviderRecordIterator<R>`, `AbstractDataProcessor<In, Out>`, `DataProviderPipeline<In, Out>`, `Future<Result>` | High-value source-defined generic class use case. Larger effort because DataProvider also has dynamic provider type metadata and record hash shapes. |
| SQL statement and datasource rows | `AbstractSQLStatement::fetchRow()/getValue() -> *hash<auto>`, `fetchRows() -> list<auto>`, datasource select methods mostly return `auto` or `hash<auto>` | `SQLStatement<RowT>`, `fetchRow() -> *RowT`, `fetchRows() -> list<RowT>` | Valuable for application code, but row shape is query-dependent. Likely needs explicit row type arguments and typed hashdecls. |
| MongoDB module | `MongoCollection::find() -> MongoCursor`, `findOne() -> *hash<auto>`, `MongoCursor::next() -> *hash<auto>` | `MongoCollection<DocumentT>`, `MongoCursor<DocumentT>`, `findOne() -> *DocumentT`, `next() -> *DocumentT` | Very clean module-level candidate. Best when `DocumentT` can be a hashdecl or structured record type. |
| DataFrame module | Constructors and transforms use `list<auto>`, `hash<auto>`, and return `auto` DataFrame values; column APIs return `list<auto>` | `DataFrame<RowT>` or `DataFrame<SchemaT>` | High-level value, but schema changes across transforms make this more complex than containers. Better after records/hashdecls and perhaps method-level generics. |
| ML modules | Datasets use `list<auto>` / `hash<auto>`; encoders and predictors transform stable input/output domains | `Estimator<InputT, TargetT, ResultT>`, `Transformer<In, Out>`, `LabelEncoder<T>` | Ecosystem value, not a core proof point. Needs API design across multiple modules and likely benefits from generic functions/inference. |
| Logger appenders | `pushEvent(int, auto params)`, `processEvent(int, auto params)` | `LoggerAppender<ParamsT>` | Possible but lower value because logger event params are intentionally extensible. |
| Reflection, `Program`, serialization | Dynamic invocation and serialization APIs intentionally return or accept `auto` | Mostly keep raw/dynamic | Not strong generic candidates. Reflection should expose generic metadata, but its dynamic call APIs should remain type-erased. |

Scan conclusion:

- Promote `Channel<T>` into the initial builtin/QPP pilot set.
- Land the `AbstractIterator<T>` work as Phase 2b, after the container/async
  builtin pilots. Phase 2b covers the builtin iterator hierarchy and
  functional-operator return typing; source-defined generic iterators follow
  in Phase 4 with full source-level generic inheritance.
- Treat `AbstractPollOperation<T>` as the next core async abstraction after
  result-record typing is viable.
- Use DataProvider, SQL, MongoDB, and DataFrame as validation cases for
  source-defined generic classes plus typed record/hashdecl support.
- Do not force generics into intentionally dynamic APIs such as reflection,
  `Program`, and serialization; those APIs mostly need generic metadata
  visibility, not generic type parameters.

## Phasing

### Phase 0: Foundation Checks

- Audit `QoreParseTypeInfo::copy()` callers before fixing. The implementation
  at `include/qore/intern/QoreParseTypeInfo.h:163` constructs from only
  `cscope`, while the copy constructor on the same class preserves
  `subtypes` and `or_nothing`. Generic classes will turn this latent bug
  into a guaranteed regression. Audit step:
  `git grep -nE 'QoreParseTypeInfo[^;]*->copy\(\)|\.copy\(\)' lib/ include/`
  on parse-type instances, classify each caller, then fix the constructor
  and add a round-trip test that survives a `copy()` cycle on a
  subtype-bearing instance.
- Add round-trip tests for nested type paths (`Box<int>`,
  `Pair<int, string>`, `Queue<list<hash<string, int>>>`,
  `*Box<*int>`).
- Decide canonical path spelling for parameterized class types. The
  candidates are `object<::Ns::Box<int>>` (extends the existing
  `QoreClassTypeInfo::pname` `"object<...>"` convention) or the direct
  user-facing spelling `::Ns::Box<int>`. Whichever is chosen must be the
  string returned by `QoreTypeInfo::getPath()` for parameterized class
  types, since AOT guard, slot-map, and TYPE_TABLE serialisation all hinge
  on that single accessor.
- Verify all parser, resolver, dispatch, reflection, and AOT paths follow the
  committed raw / `<auto>` split via the two class-gated flags
  `raw_accepts_parameterized` (acceptance side) and
  `raw_construction_defaults_to_auto` (construction side). Both default
  false; both are true for migrated legacy builtins; both stay false for
  source-defined generic classes in v1. `<auto>` is a concrete value-erased
  instantiation, and raw migrated builtins are the legacy raw-acceptance and
  raw-construction-default form. The Phase 0 audit fails if any path conflates
  the two flags or applies one without consulting the other.
- Add a cache/lifetime parity test against existing complex-type behavior:
  `list<MyClass>` and `Queue<MyClass>` must behave the same when `MyClass` is
  declared in one `QoreProgram` and the value is observed from another. This
  validates that parameterized class type infos reuse the existing `pgm` /
  `spgm` ref-chain contract instead of introducing a new owner model.

### Phase 1: Parameterized Class TypeInfo

- Add cached parameterized class type infos.
- Extend type parsing/resolution for class subtypes.
- Extend `QoreTypeSpec::match()` and `runtimeAcceptsValue()`.
- Store instantiated type info on `QoreObject`.
- Extend casts, `instanceof`, duplicate signatures, and reflection enough to
  recognize parameterized class types.

### Phase 2: Builtin/QPP Pilots

- [x] Extend qpp to declare generic builtin classes and symbolic type
  parameters.
- [x] Implement `Queue<T>`.
- [x] Implement `Channel<T>` producer/consumer typing; keep
  `ChannelIterator` raw until Phase 2b.
- [x] Implement `Promise<T>` and `Future<T>`.
- [x] Add narrow QPP parameterized vparent support for builtin metadata needed by
  `FutureImpl<T> -> Future<T>`.
- [x] Preserve raw construction defaults and raw annotation compatibility for
  `Queue`, `Channel`, `Promise`, and `Future`.
- [x] Set both compatibility flags for migrated builtin pilots via the
  `legacy_raw` qpp class-header attribute (see "Legacy-Compatibility Class
  Header Attributes" in QPP Support). This sets
  `raw_accepts_parameterized = true` and
  `raw_construction_defaults_to_auto = true` on each pilot. New
  source-defined generic classes and any future generic builtins
  introduced without `legacy_raw` keep both flags false.
- [x] Decide and test a serialization policy per pilot class before changing
  its wire form. Phase 2 keeps `Queue<T>`, `Channel<T>`, `Promise<T>`, and
  `Future<T>` explicitly unserializable, with tests asserting the stable
  `SERIALIZATION-ERROR` behavior.
- [x] Extend `Class::getTypeParameters()` / `Type::getTypeArguments()` /
  `Type::isParameterized()` accessors in `modules/reflection/src/` for the
  builtin generic pilots.
- [x] Add a `Class::callConstructor(list<auto> args, *list<Type> type_args)`
  overload so reflection callers can construct a specific parameterized
  instantiation. Without `type_args`, the existing single-argument
  overload constructs the all-`<auto>` form only for classes with
  `raw_construction_defaults_to_auto = true`; otherwise it raises a
  missing-type-arguments error.
- [x] Add parser, runtime, reflection, and serialization-policy tests around
  these classes.
- [x] Extend explicit IR/JIT/AOT-mode coverage for the Phase 2 builtin pilots
  beyond the source-defined generic class AOT/IR coverage.

### Phase 2b: Typed Iterator Family

Phase 2b and Phase 3 are independent and can be sequenced based on team
capacity. Phase 2b depends only on the QPP parameterized-vparent carve-out
introduced in Phase 2 and the typed-class machinery from Phase 1. Phase 3
depends on Phase 1 + Phase 2 type-info machinery but no iterator work. Either
can land first.

- [x] Genericize the builtin iterator base hierarchy without flattening it:
  `AbstractIterator<T>`, `AbstractBidirectionalIterator<T>`,
  `AbstractQuantifiedIterator<T>`, and
  `AbstractQuantifiedBidirectionalIterator<T>`.
- [x] Land concrete-arg iterator vparents where the current contract is stable:
  `StringCharIterator -> AbstractIterator<int>` and line/split iterators to
  `AbstractIterator<string>`. Keep `RangeIterator` raw or decide an explicit
  generic shape before typing it, because its optional `auto val` constructor
  argument changes the value returned by `getValue()`. Type key iterators only
  after preserving their existing `HashIterator` / `ObjectIterator` API
  compatibility.
- [x] Land shape-preserving builtin iterators:
  `ListIterator<T>`, `SingleValueIterator<T>`, `ChannelIterator<T>`,
  `HashIterator<V>`, and reverse variants, preserving existing intermediate
  iterator bases.
- [x] Keep pair and row-shape iterators raw until generic hashdecls/result records
  can express their `{key, value}` or row payloads.
- [x] Preserve typed functional-operator inference for `map` / `select`.
  Map/select expressions remain eager `list<T>` values for compatibility, and
  their lazy `FunctionalOperatorInterface` paths now derive element types from
  `AbstractIterator<T>` metadata instead of degrading to `auto`.
- [x] Add qpp, parser, runtime, foreach, and functional-operator tests for
  typed iterators.
- [x] Extend explicit IR/JIT/AOT-mode coverage for Phase 2b typed iterators.

### Phase 3: Source-Defined Generic Classes

- [x] Parse `class C<T>` in the core parser, mirror scanner support in
  `modules/astparser/src/`, and update the checked-in tree-sitter grammar
  artifacts with focused CST coverage.
- [x] Resolve type parameter names in class member/method/constructor/local
  type contexts before class, typedef, and hashdecl lookup.
- [x] Add generic substitution context for instance method execution.
- [x] Support constructor argument filtering, member assignment filtering,
  local assignment filtering, and return filtering with substituted types in
  AST, IR, and JIT execution.
- [x] Reject duplicate type-parameter names and generic overload signatures
  that collide under a concrete type-parameter instantiation.
- [x] Add source tests for source-defined generic classes, including
  namespaced use sites, `T` members/methods/locals, nested generic overload
  checks, and raw source-generic construction rejection.
- [x] Extend focused coverage to AOT cold-load / serialized-IR paths for
  source-defined generic class construction and method calls before declaring
  Phase 3 complete for production AOT.

### Phase 4: Optional Extensions

- [x] Generic inheritance: source classes can declare
  `class Derived<T> inherits Base<T>`, including fixed-argument bases such as
  `class Derived<T> inherits Base<string>`. The runtime records the
  parameterized parent type so inherited method return/argument types,
  `instanceof`, and subtype matching substitute through the derived class's
  actual type arguments. AOT metadata also preserves the parameterized base
  type path so source-stripped cold-load uses the same substitution metadata.
- [x] Source-defined generic iterators: source-defined generic
  iterators that inherit from `AbstractIterator<T>` from Qore source code.
  Includes `Mapper::AbstractMapperIterator<RecT>` (qlib) and any user-defined
  generic iterator that wants to participate in the typed-iterator surface.
  Requires the full `class Derived<T> inherits Base<T>` generic-inheritance
  machinery from Phase 4's first bullet. Builtin generic-T iterators land in
  Phase 2b via the QPP parameterized-vparent carve-out.
- Static generic methods.
- Type parameter defaults in source.
- Constraints/bounds.
- Method-level generic functions.
- Generic hashdecls/result records.
- Variance annotations for immutable/read-only abstractions.
- Opt-in raw-compatibility modifiers for source-defined generic classes,
  needed when migrating an existing user-defined non-generic class to a
  generic form without breaking either raw-`Box`-typed callers or
  `Box b()` construction sites. Three modifiers cover the two flags
  with both fine and combined granularity:
  - `class Box<T> [legacy_raw_accepts]` — sets only
    `raw_accepts_parameterized = true`. Existing raw-`Box` parameter
    sites keep working; existing `Box b()` constructors must be migrated
    to `Box<auto>` or fail.
  - `class Box<T> [legacy_raw_construct]` — sets only
    `raw_construction_defaults_to_auto = true`. Existing `Box b()`
    constructors keep working as `Box<auto>`; existing raw-`Box`
    parameter sites must be migrated to `Box<auto>` or fail.
  - `class Box<T> [legacy_raw]` — sets both flags. Full source-compat
    for migrated user-defined non-generic classes.
  v1 keeps both flags false for all source-defined generic classes by
  default to avoid silently widening acceptance or default construction
  in new code.

## Estimated Effort

Approximate effort for one experienced Qore runtime engineer. The
"prototype only" row was dropped: a parameterized type-info that does not
survive AOT round-trip is not deliverable on `feature/5164_jit`, where
AOT is the production load path. Every estimate below assumes the IR /
AOT / JIT paths are integrated.

| Scope | Estimate |
| --- | --- |
| `Queue<T>` / `Channel<T>` / `Future<T>` with qpp support and tests (Phase 1 + Phase 2 integrated through IR/AOT/JIT) | 6-10 weeks |
| Typed builtin iterator hierarchy and functional-operator return typing (Phase 2b) | 4-8 weeks |
| Source-defined generic classes, instance methods, members, constructors (Phase 3) | 8-14 weeks |
| Full generic language including inheritance, statics, constraints, inference (Phase 4) | 3-6+ months |

The riskiest implementation areas are not the syntax. They are:

- substituting symbolic type parameters through existing type-info trees
- preserving generic object type info through construction and copying
- avoiding accidental raw-compatibility unsoundness for mutable classes
- keeping AOT type paths stable
- preventing qpp and parser string parsing from diverging
- making method/member/local type filtering use the correct generic context

## Testing Plan

Add focused tests under `examples/test/`:

- Parse and type resolution:
  - `class C<T>`
  - `C<int>`
  - nested `C<list<int>>`
  - wrong type-argument count
  - non-generic `Class<int>` rejection
- Parse-error coverage:
  - `class Box<>` — empty type-parameter list rejected
  - `class Box<T, T>` — duplicate type-parameter name rejected
  - `class Box<int>` — rejected if `int` is a reserved builtin type keyword
    rather than an identifier token; this is a lexical/grammar rule, not the
    ordinary shadowing rule
  - `class Box<T>` when a visible class or typedef named `T` exists — accepted;
    uses of `T` in the class body resolve to the type parameter
  - method-level generic syntax `T identity<T>(T)` — rejected in v1
    (Non-goals)
  - `new Box<int, string>(1)` for single-parameter `Box<T>` — wrong
    arity rejected
  - `Box b()` for source-defined `class Box<T>` — missing type-args
    rejected (legacy default only applies to migrated builtins)
  - `Box<>` use site — empty type-arg list at use rejected
  - reflection `Class::callConstructor(args)` on source-defined `Box<T>`
    without `type_args` rejects; the same call on migrated `Queue<T>`
    constructs `Queue<auto>`
- Runtime type acceptance:
  - `C<int>` accepts only matching instantiated objects
  - raw compatibility for legacy migrated builtins
  - raw source-defined `C` metadata does not accept `C<int>` unless the class
    has the explicit legacy raw-acceptance flag
  - `Queue<auto>` does not accept `Queue<int>`
  - `instanceof C<int>`
  - casts to `C<int>`
- Generic members and methods:
  - `private T value`
  - `constructor(T)`
  - `T get()`
  - `nothing set(T)`
  - assignment rejection for wrong type
- Source-defined generic classes:
  - `examples/test/qore/misc/generic-classes.qtest` covers `class C<T>`,
    `Pair<K, V>`, namespaced generic type use, constructor/method/member/local
    substitution, generic overload collision checks, and raw construction
    rejection. Run it in default, AST, IR, JIT, and compiled AOT modes.
- `Queue<T>`:
  - `push()` accepts `T`
  - `push()` rejects wrong type
  - `pop()` / `get()` return type narrows to `T`
  - raw `Queue` remains compatible
- `Channel<T>`:
  - `send()` / `trySend()` accept `T`
  - `send()` / `trySend()` reject wrong type
  - `recv()` return type narrows to `*T`
  - `iterator()` remains raw in Phase 2; Phase 2b narrows it to
    `ChannelIterator<T>`
  - raw `Channel` remains compatible
- `Promise<T>` / `Future<T>`:
  - `Promise<T>::set(T)` accepts/rejects correctly
  - `getFuture()` returns `Future<T>`
  - `Future<T>::get()` returns `T`
  - `FutureImpl<T>` is accepted as `Future<T>` through QPP parameterized
    vparent metadata
  - raw behavior remains compatible
- `AbstractIterator<T>` family (Phase 2b):
  - Generic intermediate bases preserve the existing hierarchy:
    `AbstractBidirectionalIterator<T>`,
    `AbstractQuantifiedIterator<T>`, and
    `AbstractQuantifiedBidirectionalIterator<T>` all resolve to a consistent
    `AbstractIterator<T>` base.
  - Concrete-arg vparent: `*LineIterator::getValue() -> string`,
    `StringSplitIterator::getValue() -> string`,
    `StringRegexSplitIterator::getValue() -> string`,
    and `StringCharIterator::getValue() -> int` all narrow at the static-type
    level. Key iterators narrow to `string` only after the Phase 2b
    compatibility decision for their existing `HashIterator` /
    `ObjectIterator` parent APIs. `RangeIterator` stays raw/`auto` unless
    Phase 2b chooses an explicit generic shape for the optional-value
    constructor.
  - Shape-preserving builtin: `ListIterator<int>` typed loops yield `int`;
    `HashIterator<MyClass>::getValue()` returns `MyClass`;
    `ChannelIterator<int>` narrows `getValue()`; raw `ListIterator` /
    `HashIterator` / `ChannelIterator` remain compatible via the class-gated
    raw-acceptance flag on the migrated iterators.
  - Pair and row-shape iterators remain raw until generic hashdecls/result
    records can express their payloads; tests should assert they are not
    accidentally typed as `AbstractIterator<V>`.
  - Functional operator return typing: `map(list<int> li, int sub(int x) {
    return x*2; })` has static type `AbstractIterator<int>`;
    `select(list<MyClass> lc, *bool sub(MyClass m) { ... })` has static type
    `AbstractIterator<MyClass>`. Chain through `for (MyClass m : select(...))`
    and assert the loop variable is statically `MyClass`, not `auto`.
  - Source-defined generic iterator (Phase 4): a
    `class TypedMapperIterator<RecT> inherits AbstractIterator<RecT>`
    declaration in Qore source compiles, the inherited `getValue()` is
    typed as `RecT`, and `RecT` substitutes through to consumers.
- IR/AOT:
  - generic object construction
  - generic method call
  - generic type guard
  - AOT deserialization of generic type paths
- qpp:
  - generated metadata for generic class params
  - generated method signatures using symbolic type params
- astparser/tree-sitter:
  - `class C<T>` is covered by `modules/astparser/test/astparser.qtest`
    (`Generic class CST`), including the `type_parameter_list` node.
  - `new C<int>()` is parsed in the same CST coverage.
  - generic class types in declarations and signatures
- Cross-`Program` type identity:
  - `class C<T>` declared independently in two `Program` instances — assert
    that `Program A`'s `C<int>` does not accept `Program B`'s `C<int>`
    value, identity-based on the underlying `QoreClass*` exactly as for
    today's plain class types
  - static-system or binary generic class installed as the same shared
    `QoreClass*` in two consumer programs — same parameterized type-info
    pointer must be observed from both, exactly like a shared
    `QoreClassTypeInfo*` today
  - `Queue<MyClass>` where `MyClass` is `Program A`-scoped — assert
    `Program A` teardown is held off by the live object reference through
    the existing `pgm` / `spgm` chain, the same way `list<MyClass>` is held
    today
  - repeated create/destroy cycles for `list<MyClass>` and `Queue<MyClass>`
    preserve the existing complex-type cache behaviour and do not introduce a
    parameterized-class-specific lifetime rule
- Serialization round-trip:
  - for generic objects that are serializable, `serialize(c)` where
    `c : C<int>` and `deserialize` recovers a value with the same
    parameterized type (type-arg path survives the round-trip)
  - if `Queue<T>` is serializable, `Queue<int>` containing values whose type is
    exactly `int` — verify rejection on deserialize when the wire form encodes
    `Queue<string>` pointing at `int` data
  - if `Promise<T>` / `Future<T>` remain unserializable, verify the explicit
    failure rather than silently degrading to `Promise<auto>` / `Future<auto>`
- Reflection round-trip:
  - `getTypeInfo()` on a generic class returns the raw form
  - `getTypeInfo(args)` returns the same interned parameterized type-info
    pointer for equal `args`
  - enumerate type-args from a parameterized type-info, reconstruct the
    canonical path string, re-resolve to the same pointer
- GC scan:
  - `Queue<Box<int>>` where `Box` holds a back-ref to the queue —
    object-graph (`RObject`) scan must traverse member values, not the
    type-arg metadata; assert that adding type-arg awareness does not
    introduce a phantom cycle
- AOT cold-load:
  - qmod containing only `class Box<T>` with no instantiation site —
    loads cleanly, `Box<int>` resolves correctly the first time it is
    constructed by a consumer
  - mirrors the prior `ProviderInfo AOT-PENDING-CONSTANT` pattern: the
    first instantiation must run any deferred per-class init the same
    way class constants do

## Open Questions

- If source-defined generic classes later support default type parameters,
  should omitted type arguments be allowed only when every parameter has an
  explicit default?
- Should static methods using class type parameters be rejected in v1 or
  supported through explicit `C<int>::method()` dispatch context?
- How much generic inheritance is needed before source-defined classes feel
  complete?
- Should `Queue<T>::tryGet()` remain `auto`, return `*T`, return
  `union<nothing, T>`, or wait for a generic result class/hashdecl feature?
- Should `Channel<T>::tryRecv()` follow the same answer as
  `Queue<T>::tryGet()` / `tryPop()`, or should channel receive status get its
  own generic result class?
- Should qpp support default type parameters in source syntax immediately, or
  should legacy defaults be attached only in generated C++ metadata?

## Recommendation

Proceed with reified generic class types, not monomorphized templates.

Use `Queue<T>`, `Channel<T>`, and `Promise<T>` / `Future<T>` as the first
production targets because they deliver immediate API value and exercise the
runtime type machinery without requiring every source-defined generic class
feature on day one.

The full design should explicitly allow source-defined Qore classes like:

```qore
class C<T> {
    T get() {
        ...
    }
}
```

but it should land after the object type metadata, type substitution, qpp,
IR/AOT, and runtime acceptance paths have been proven with builtin generic
classes.

### Key Decisions

These are load-bearing semantic commitments made elsewhere in the
document. A reader who skims to the bottom should not miss them.

- **Reified, not monomorphized.** One runtime class per generic
  declaration; every generic object carries a concrete parameterized type on
  `qore_object_private`. Raw is never an observable runtime object type.
- **Parameterized type-info lifetime matches existing complex types.**
  Single global cache per kind, pointer-keyed, never actively reaped during
  runtime, cleared at process shutdown. `Program` lifetime for type-arg
  classes is handled by the existing `pgm` / `spgm` ref chain that already
  governs `list<MyClass>` and `hash<string, MyClass>` today. No new
  parameterized-class-specific lifetime invariant.
- **Substitution context is explicit and site-specific.** Constructor dispatch
  uses `NewObject.object_type_info`; instance method dispatch and bodies use
  the receiver object's `instantiated_type`; static generic methods (Phase 4)
  get an explicit IR operand. No thread-local context, no implicit-state
  plumbing.
- **`<auto>` is a concrete value-erased instantiation, not a wildcard.**
  Matches the existing `list<auto>` / `hash<auto>` semantics.
- **Raw compatibility is class-gated** via
  `qore_class_private::raw_accepts_parameterized` (default false; true for
  migrated legacy builtins; opt-in for source-defined classes per Phase 4).
  Migrated builtins such as `Queue` can accept `Queue<int>` through their
  raw annotation form; `Queue<auto>` does not. New source-defined generic
  classes do not get raw wildcard acceptance in v1. Raw acceptance is also
  identity-gated cross-program — it does not reach across `Program`
  boundaries to accept independently-declared classes.
- **Raw construction defaults are separately gated** via
  `qore_class_private::raw_construction_defaults_to_auto` (default false; true
  for migrated legacy builtins). Source-defined generic classes require
  explicit type arguments even through reflection/internal construction APIs.
- **`getTypeInfo()` returns the raw form** for a generic class. A
  separate `getTypeInfo(args)` accessor returns the interned parameterized
  form. Avoids silent wildcard regressions at existing callers.
- **`<auto!>` is accepted but folded to `<auto>`** for matching;
  no-narrowing only applies at folding sites.
- **Type-parameter names have their own lookup phase** that runs before
  class/typedef resolution within the class body.
- **AOT serialisation requires a new `QORE_AOT_FEAT_*` bit** (next free:
  `1ULL << 18`), and writers/readers in `QoreAOTInstRegistry.cpp`,
  `QoreAOTBinary.cpp`, and `QoreAOTRuntime.cpp` must be updated in
  lockstep. Symbolic type parameters in generic-class qmods serialise as
  placeholder tokens bound to a per-class type-parameter table at load.
- **`AbstractIterator<T>` is Phase 2b, not part of the first builtin pilot.**
  Phase 2b genericizes the builtin iterator base hierarchy, adds concrete-arg
  and shape-preserving builtin iterator vparents where the current contracts
  support them, and narrows `map`/`select` return types using existing
  `FunctionalOperatorInterface::getValueTypeImpl()` data. Pair and row-shape
  iterators stay raw until generic hashdecls/result records can express their
  payloads. Source-defined generic iterators
  (`class MyIter<T> inherits AbstractIterator<T>`, including
  `Mapper::AbstractMapperIterator<RecT>`) ride on Phase 4's full generic
  inheritance.
