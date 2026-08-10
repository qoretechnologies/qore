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
- `design-pending/generic-api-rollout.md` — the work that has not been done

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

When the parse context provides a concrete expected result type, Qore can infer
the generic receiver for constructor and static factory calls:

```qore
Box<int> explicit_target = new Box(1);
Box<int> returned() {
    return new Box(2);
}

Factory<int> inferred_factory = Factory::make(3);
```

Constructor calls also infer class type arguments from supplied constructor
arguments when the result is unique:

```qore
auto inferred = new Box(4);       # creates Box<int>
auto named = new Box(value: 5);   # named calls participate in inference
```

Defaults fill any type parameters that inference does not bind. Bounds are
checked after inference, and ambiguous overload-derived bindings are rejected
with a parse-time error that asks the caller to write explicit type arguments.
Static factory receiver inference is intentionally limited to target/return
contexts; a raw static call with no expected type is still rejected when method
type checking needs a class type-argument substitution context.

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

Generic class types are reified. There is one runtime class definition, and
each constructed object carries an instantiated type info such as `Box<int>` or
`Queue<string>`.

The object wrapper is the source of truth:

- Non-generic objects have no instantiated type info and use the class type.
- Generic objects always carry a concrete instantiated type info.
- Raw generic construction is never an observable runtime object type.

The type layer adds `QoreParameterizedClassTypeInfo`, interned by base class,
type argument vector, and `or_nothing`. Generic type-parameter placeholders are
substituted through `qore_substitute_type_params()` at method dispatch,
constructor dispatch, member access, local variable declaration, return checks,
IR interpretation, JIT runtime helpers, and AOT loading.

Concrete type arguments match invariantly:

```qore
Box<int> b(1);

b instanceof Box<int>;     # true
b instanceof Box<string>;  # false
b instanceof Box<auto>;    # false
```

`auto` is a concrete type argument, not a wildcard.

Wildcard annotations can accept families of parameterized types:

```qore
Box<?> any_box = b;
Box<? extends int> int_source = b;
Box<? super int> int_sink = b;
```

`? extends T` is read-oriented: methods returning the wildcarded type parameter
return `T`, while methods accepting that parameter reject concrete values.
`? super T` is write-oriented: methods accepting the wildcarded type parameter
accept `T`, while methods returning that parameter return `auto`.

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

New generic classes should normally avoid raw compatibility. If constructor
argument or target-type inference cannot determine all required type arguments,
the raw construction expression is rejected at construction time with
`MISSING-TYPE-ARGUMENTS`. Classes marked with `[legacy_raw]` or
`[legacy_raw_construct]` keep their compatibility behavior: raw construction
creates the all-`auto` instantiation instead of inferring from constructor
arguments.

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

When a source method or function executes with concrete generic receiver or
method type arguments, the IR/JIT dispatcher can lower and cache a specialized
IR body for that type-argument tuple. The specialized body stores the concrete
receiver type and method type-parameter instantiation, substitutes those types
while lowering and while emitting LLVM type metadata, and keeps the
substitution-based AST path as the correctness fallback.

AOT type resolution parses nested serialized type paths for parameterized
classes and hashdecls. Class raw-compatibility flags are protected by
`QORE_AOT_FEAT_CLASS_RAW_GENERIC`; generic type paths are interned through the
existing type-table mechanism. Source-stripped AOT does not emit separate
native bodies for every generic instantiation; it preserves generic type paths
and resolves them with the current runtime receiver or method type-argument
context. Hot source-stripped AOT hashdecl construction caches resolved
hashdecl paths per receiver type context, so repeated construction of
parameterized generic result records does not redo path parsing and type
substitution on every iteration.

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
| Path-prefix map | `TreeMap<V>` |
| Qlib record iterators | `Mapper::AbstractMapperIterator<RecT>`, `DataProvider::AbstractDataProviderRecordIterator<RecT>`, `DataProvider::DefaultRecordIterator<RecT>` |
| Poll operations | `AbstractPollOperation<T>`, `SocketPollOperationBase<T>` and fixed-output socket poll subclasses |
| Shipped module cursor | `mongodb::MongoCursor<DocT: hash<auto> = hash<auto>>` |

Some classes intentionally remain raw or use `<auto>` parents because their
output varies by call goal or because their records are heterogeneous. Examples
include variable-output poll wrappers and object/hash member pair iterators
whose per-entry value type cannot safely be represented as the whole record
type.

## Shipped Module Rollout

The bundled `modules/*` review applies the same rule: use generics only where
one logical type is carried through the object.

`mongodb::MongoCursor<DocT>` is generic because the document type is stable for
the cursor. `MongoCollection::find()` and `MongoCollection::aggregate()` return
`MongoCursor<hash<auto>>` today, and the type parameter lets future typed
wrappers preserve narrower document hashdecls.

The `dataframe` module was processed without making `DataFrame` itself generic.
DataFrame row shapes change with `select()`, `groupBy()`, `agg()`, `join()`,
`pivot()`, and `melt()`, so `DataFrame<RecT>` would promise more than the
runtime can guarantee. Instead, DataFrame-producing methods now return
`DataFrame`, grouping returns `GroupedDataFrame`, `describe()` returns
`list<hash<ColumnStats>>`, `shape()` returns `hash<DataFrameShape>`, and CSV
options use `hash<CsvOptions>`. `DataProviderDataFrame` and
`MongoDbDataProvider` record iterators use
`AbstractDataProviderRecordIterator<hash<auto>>` because the row/document shape
is runtime-defined.

Other bundled modules (`logger_bin`, `tokenizer`, `protobuf`, `i18n`,
`astparser`, `reflection`, `ml`, `ocr`, `krb5`, and `linenoise`) were reviewed
and do not currently have a safe class-level generic conversion. Where their
result records are stable, typed hashdecl cleanup remains appropriate follow-up
work; where schemas or selected fields are runtime-defined, `hash<auto>` is the
honest public type.

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

The following are intentionally not part of the current core implementation or
still require rollout work:

- Source-stripped AOT native body generation per type-argument tuple remains
  future work. Source-stripped AOT preserves generic metadata and resolves it at
  runtime, but it does not yet emit a separate native entry point for every
  concrete generic instantiation. Current benchmark data does not justify that
  extra code size and metadata complexity: after caching source-stripped AOT
  hashdecl path resolution per receiver type context, AOT is at or faster than
  IR/JIT/tiered execution on the focused generic dispatch and hashdecl-heavy
  kernels in `bench/generic-aot-specialization.qr`. The decision was taken
  against a stated threshold, and only new data crossing it reopens the
  question: implement native specialization only if source-stripped AOT is at
  least 15% slower than **both** JIT/tiered and source-included AOT on at least
  two generic call kernels, or if a real qlib or module workload shows at least
  15% generic-dispatch overhead attributable to missing source-stripped
  specialization once allocation and I/O costs are excluded. Reproduce with
  `bench/run_generic_aot_specialization.sh`.
- Static factory receiver inference remains conservative when there is no
  assignment target or return context. Calls such as `Factory<int>::make(...)`
  are fully typed, and target/return contexts can infer the receiver type, but a
  raw `Factory::make(...)` call still requires explicit class type arguments
  when method checking needs a substitution context.
- The core tree and shipped modules still need a systematic API rollout pass for
  additional builtin, QPP, and qlib classes where a stable logical value type is
  carried through an object.

Concrete class and hashdecl type arguments are invariant unless the annotation
uses an explicit wildcard. `auto` remains a concrete type argument, not a
wildcard. Raw compatibility attributes are migration aids for specific legacy
APIs, not a substitute for wildcard annotations.

Execution is reified and substitution-based in the AST and AOT metadata paths.
For source bodies, IR/JIT execution can now cache specialized IR bodies for
concrete receiver and method type arguments. These specializations are internal
execution artifacts; reflection, diagnostics, and stack traces continue to
describe the source method or function.

Class type argument inference is intentionally conservative. Method-level
generic calls can infer call-local type arguments from supplied arguments, and
generic classes can infer class type arguments from constructor arguments,
assignment targets, and return contexts where the expected type is reliable.

The remaining rollout work — external `module-*` repositories, and the deferred
AOT specialization design should its threshold ever be crossed — is tracked in
`design-pending/generic-api-rollout.md`.

## Test Coverage

Primary coverage lives in:

- `examples/test/qore/misc/generic-classes.qtest`
- `examples/test/qlib/DataProvider/DataProvider.qtest`
- threading, iterator, socket, FTP, HTTP async I/O, and AOT tests touched by
  the generic class conversion commits

The generic class test covers source generics, multiple type parameters,
defaults, bounds, namespaces, overloads, inheritance, static generic methods,
method-level generics, explicit generic call type arguments, generic hashdecls,
generic hashdecl inheritance, wildcard type arguments, iterators, poll
operations, raw compatibility attributes, specialized generic execution paths,
and expected parse or runtime failures.
