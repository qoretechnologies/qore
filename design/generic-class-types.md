# Generic Class Types - Implementation

## Status: Implemented

Generic class types, generic hashdecls, and the first core generic builtin
class conversions are implemented on `feature/5164_jit`.

This document replaces the exploratory pending design note. It records the
implemented model, the compatibility rules that module authors must preserve,
and the remaining limits that are intentional in this phase.

Related operational checklists:

- `design/generic-builtin-class-conversion-checklist.md`
- `design/generic-phase4-static-and-hashdecl-checklist.md`
- `design/generic-future-work-checklist.md`

User-facing documentation lives in:

- `doxygen/lang/155_data_type_declarations.dox.tmpl`
- `doxygen/lang/215_classes.dox.tmpl`
- `doxygen/lang/217_hashdecl.dox.tmpl`
- `doxygen/lang/220_threading.dox.tmpl`
- `doxygen/lang/900_release_notes.dox.tmpl`

API metadata documentation lives in:

- `design/api-metadata-for-modules.md`

## User-Facing Surface

Source classes can declare type parameters:

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

Box<int> box(1);
int value = box.get();
```

Multiple parameters and namespaces are supported:

```qore
class Pair<K, V> {
    private K key;
    private V value;
}

Pair<string, int> p("id", 1);
```

Generic source inheritance is supported:

```qore
class Base<T> {
    T get(T value) {
        return value;
    }
}

class Child<T> inherits Base<T> {
}

Base<int> b = new Child<int>();
```

Static methods in generic classes may use class type parameters when the call
site supplies a parameterized receiver:

```qore
class Factory<T> {
    static Factory<T> make(T value) {
        return new Factory<T>(value);
    }
}

Factory<int> f = Factory<int>::make(1);
```

Calling a type-parameter-dependent static method through a raw generic class
is rejected because there is no substitution context.

Generic hashdecls are supported:

```qore
hashdecl Result<T> {
    T value;
    list<T> values;
}

hash<Result<int>> r(("value": 1, "values": (2, 3)));
```

Generic hashdecl inheritance is supported:

```qore
hashdecl Parent<T> {
    T value;
}

hashdecl Child<T> inherits Parent<T> {
    string name;
}

hash<Child<int>> h(("value": 1, "name": "one"));
```

Type parameters may declare defaults and concrete upper bounds:

```qore
class Box<T: int = int> {
    private T value;

    constructor(T value = 1) {
        self.value = value;
    }
}

Box<> b();
```

Functions and methods may declare their own type parameters. Explicit generic
call type arguments are optional when the argument types infer a unique
instantiation:

```qore
T sub identity<T>(T value) {
    return value;
}

int a = identity(1);
int b = identity<int>(1);
string c = identity<string>(value: "x");
```

## Runtime Model

Generic class types are reified and invariant. There is one runtime class
definition, and each constructed object carries an instantiated type info such
as `Box<int>` or `Queue<string>`.

The object wrapper is the source of truth:

- Non-generic objects have no instantiated type info and use the class type.
- Generic objects always carry a concrete instantiated type info.
- Raw generic construction is never an observable runtime object type.

The type layer adds `QoreParameterizedClassTypeInfo`, interned by base class,
type argument vector, and `or_nothing`. Generic type-parameter placeholders are
substituted through `qore_substitute_type_params()` at method dispatch,
constructor dispatch, member access, local variable declaration, return checks,
IR interpretation, JIT runtime helpers, and AOT loading.

Matching is invariant:

```qore
Box<int> b(1);

b instanceof Box<int>;     # true
b instanceof Box<string>;  # false
b instanceof Box<auto>;    # false
```

`auto` is a concrete type argument, not a wildcard.

## Raw Compatibility

Raw compatibility is explicit and class-local. It exists to migrate existing
public APIs without breaking old source.

Three attributes are supported on generic source classes and QPP classes:

- `[legacy_raw]`: raw annotations accept parameterized instances, and raw
  construction creates the all-`auto` instantiation.
- `[legacy_raw_accepts]`: raw annotations accept parameterized instances, but
  raw construction still requires explicit type arguments.
- `[legacy_raw_construct]`: raw construction creates the all-`auto`
  instantiation, but raw annotations are not a wildcard.

For example:

```qore
class CompatBox<T> [legacy_raw] {
    constructor(T value) {
    }
}

CompatBox raw(1);          # constructs CompatBox<auto>
CompatBox<int> typed(1);
CompatBox any = typed;     # allowed by raw annotation compatibility
```

New generic classes should normally avoid raw compatibility and require
explicit type arguments. A raw construction expression for such a class is
rejected at construction time with `MISSING-TYPE-ARGUMENTS`.

The same rules are serialized into AOT class metadata. API metadata exposes
`type_parameters`, `raw_accepts_parameterized`, and
`raw_construction_defaults_to_auto` so downstream tools and binary modules can
preserve the same compatibility story.

## QPP Support

QPP class declarations can declare formal type parameters and use them in
method signatures and virtual parents:

```qore
qclass Queue<T> [dom=THREAD_CLASS; arg=Queue* q; ns=Qore::Thread; legacy_raw];
T Queue::pop(timeout timeout_ms = 0);
```

```qore
qclass FutureImpl<T> [
    dom=THREAD_CLASS;
    arg=QoreFuture* f;
    ns=Qore::Thread;
    vparent=Future<T>;
    legacy_raw
];
```

QPP also supports generic hashdecl declarations. The generated metadata records
formal type parameters for classes and hashdecls so reflection, API metadata,
and source-stripped AOT can reconstruct the generic shape.

## AOT, IR, And JIT

Parameterized type information is preserved through all construction paths:

- parsed `new Class<T>()` and declaration construction
- scoped static calls such as `Class<T>::method()`
- IR `NewObject` instructions
- JIT runtime construction helpers
- source-stripped AOT class and hashdecl metadata
- AOT type-table resolution for nested generic paths

`QoreIRNewObjectInstruction`, scoped object call nodes, and AOT call targets
carry `object_type_info` so constructors and static calls have a concrete
substitution context before a usable `self` exists.

AOT type resolution parses nested serialized type paths for parameterized
classes and hashdecls. Class raw-compatibility flags are protected by
`QORE_AOT_FEAT_CLASS_RAW_GENERIC`; generic type paths are interned through the
existing type-table mechanism.

## Implemented Core Conversions

The core tree now uses generic types in the APIs where a stable logical value
type is carried through the object.

| Area | Generic APIs |
|------|--------------|
| Thread values | `Queue<T>`, `Channel<T>`, `Promise<T>`, `Future<T>`, `FutureImpl<T>` |
| Thread result records | `QueueTryResult<T>`, `ChannelTryResult<T>` |
| Iterator hierarchy | `AbstractIterator<T>`, `AbstractBidirectionalIterator<T>`, `AbstractQuantifiedIterator<T>`, `AbstractQuantifiedBidirectionalIterator<T>` |
| Concrete iterators | `ListIterator<T>`, `ListReverseIterator<T>`, `SingleValueIterator<T>`, `ChannelIterator<T>` |
| Hash iterators | `HashIterator<V>`, `HashPairIterator<V>`, `HashReverseIterator<V>`, `HashPairReverseIterator<V>`, `KeyValueInfo<V>` |
| Qlib record iterators | `Mapper::AbstractMapperIterator<RecT>`, `DataProvider::AbstractDataProviderRecordIterator<RecT>`, `DataProvider::DefaultRecordIterator<RecT>` |
| Poll operations | `AbstractPollOperation<T>`, `SocketPollOperationBase<T>` and fixed-output socket poll subclasses |

Some classes intentionally remain raw or use `<auto>` parents because their
output varies by call goal or because their records are heterogeneous. Examples
include variable-output poll wrappers and object/hash member pair iterators
whose per-entry value type cannot safely be represented as the whole record
type.

## Poll Operation Output Policy

`AbstractPollOperation<T>` and `SocketPollOperationBase<T>` are generic, but
only fixed-output subclasses are specialized. Examples:

- FTP control poll: `SocketPollOperationBase<*hash<auto>>`
- FTP data poll: `SocketPollOperationBase<*binary>`
- FTP port accept poll: `SocketPollOperationBase<*Socket>`
- HTTP idle / keep-alive / accept / websocket poll bases:
  `SocketPollOperationBase<hash<auto>>`
- HTTP/2, HTTP/3, and HTTP client ping poll bases:
  `SocketPollOperationBase<*hash<auto>>`

Classes whose `getOutput()` result depends on the requested operation remain
raw so the type system does not advertise a misleading stable result type.

## Binary Module Rollout Guidance

For `~/src/qore/git/module-*`, migrate generic APIs conservatively:

1. Identify classes where one stable logical type flows through producer and
   consumer methods.
2. Record the meaning of each type parameter before editing.
3. Convert one class family per commit.
4. Use `[legacy_raw]` only for public classes that already existed as raw APIs.
5. Prefer generic hashdecl result records for envelopes such as
   `Result<T>`, `TryResult<T>`, or iterator pair records.
6. Avoid generics for heterogeneous payloads where `auto` is more honest.
7. Update examples, generated API metadata, and release notes with concrete
   generic spellings.
8. Run the module's focused tests and apply `audit-changes` before committing.

## Known Limits

The following are intentionally not part of this implementation:

- variance
- monomorphized code generation per type argument
- inference of class type arguments at arbitrary expression sites

Generic class and hashdecl type arguments are currently invariant. `auto`
remains a concrete type argument, not a wildcard. Raw compatibility attributes
are migration aids for specific legacy APIs, not a substitute for general
wildcard or variance semantics.

Execution is currently reified and substitution-based. The runtime keeps one
class or function body and substitutes type parameters for parsing, runtime
checks, IR, JIT fallback, and AOT metadata. It does not yet create specialized
compiled bodies for each type-argument tuple.

Class type argument inference is intentionally conservative. Method-level
generic calls can infer call-local type arguments from supplied arguments, and
generic classes may use defaults for omitted type arguments. General inference
from constructor arguments, assignment target types, or broader expression
context remains future work.

The future-work checklist is tracked in
`design/generic-future-work-checklist.md`.

## Test Coverage

Primary coverage lives in:

- `examples/test/qore/misc/generic-classes.qtest`
- `examples/test/qlib/DataProvider/DataProvider.qtest`
- threading, iterator, socket, FTP, HTTP async I/O, and AOT tests touched by
  the generic class conversion commits

The generic class test covers source generics, multiple type parameters,
defaults, bounds, namespaces, overloads, inheritance, static generic methods,
method-level generics, explicit generic call type arguments, generic hashdecls,
generic hashdecl inheritance, iterators, poll operations, raw compatibility
attributes, and expected parse or runtime failures.
