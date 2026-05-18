# Generic Builtin Class Conversion Checklist

This checklist captures the process for evaluating and converting builtin/QPP
classes to generic class types. It applies to the core Qore repository and to
binary module repositories such as `~/src/qore/git/module-*`.

## Current Core Conversions

The initial core conversions are:

| Class | Type Parameter | Notes |
|-------|----------------|-------|
| `Qore::Thread::Queue<T>` | `T` | `push()` and `insert()` accept `T`; `get()` and `pop()` return `T` |
| `Qore::Thread::Channel<T>` | `T` | `send()` and `trySend()` accept `T`; `recv()` returns `*T` |
| `Qore::Thread::Promise<T>` | `T` | `set()` accepts `T`; `getFuture()` returns `Future<T>` |
| `Qore::Thread::Future<T>` | `T` | `get()` returns `T` |
| `Qore::Thread::FutureImpl<T>` | `T` | QPP-only concrete implementation with `vparent=Future<T>` |

All initial conversions use `legacy_raw`, meaning raw construction creates the
explicit `<auto>` instantiation and raw annotations still accept parameterized
instances.

## Candidate Review

Use generics only when one or more method signatures carry the same logical
value type through the object:

- Containers: enqueue/dequeue, send/receive, read/write, cache get/set.
- Async handles: producer result type, future result type, operation output.
- Iterators: current value type, key/value pairs, record shape.
- Builders or processors: input/output type pairs, only when both sides are
  stable and not hidden behind dynamic provider metadata.

Avoid conversion when:

- The class stores unrelated heterogeneous values with no single useful type
  parameter.
- Existing APIs deliberately accept arbitrary payloads and return unrelated
  shapes.
- Method overloads would require variance, wildcard, or expression-site
  inference semantics that are not implemented yet.
- The class is source-defined Qore code but cannot state a stable type
  parameter contract without misleading callers.

## QPP Declaration Checklist

For each converted QPP class:

- Rename the declaration to `qclass Name<T>` or `qclass Name<K, V>`.
- Use `legacy_raw` only for migrated public classes where existing raw code must
  keep working.
- Use `legacy_raw_accepts` when raw annotations should accept parameterized
  instances but raw construction should stay invalid.
- Use `legacy_raw_construct` when raw construction should create `<auto>` but raw
  annotations should not accept parameterized values.
- Keep formal parameter names short and semantic (`T`, `K`, `V`, `R`, `In`,
  `Out`) and do not reuse names in the same class.
- For QPP-only generic inheritance, use parameterized vparents explicitly, for
  example `vparent=Future<T>`.
- Verify generated metadata contains `type_parameters`,
  `raw_accepts_parameterized`, and `raw_construction_defaults_to_auto`.

## Method Signature Checklist

For each method and constructor:

- Replace producer parameters with the type parameter where the object stores
  the value, for example `push(T value)`.
- Replace consumer return types with the type parameter where the object returns
  the stored value, for example `T pop()`.
- Use `*T` only when the method can return no value without throwing.
- Keep wrapper/status-returning APIs as `auto` or a concrete hash/list type until
  the wrapper type itself can represent `T` accurately.
- Preserve `NAMED_ARGS` flags on variants already reviewed for named calls.
- Do not expose type arguments as runtime constructor parameters; type arguments
  come from the declared object type.
- Confirm raw legacy subclasses still satisfy abstract methods when the parent
  was migrated to `T` return types.
- Prefer method-level generics for helpers where the type is call-local and does
  not describe object state.

## Runtime Construction Checklist

For C++ code that creates objects of a converted generic class directly:

- Set the instantiated type info on every object that bypasses normal parsed
  construction.
- Use a helper when a class is created from many C++ call sites. Core
  `FutureImpl` uses `qore_new_future_impl_object()` so all raw async producers
  create `FutureImpl<auto>` and `Promise<T>::getFuture()` creates
  `FutureImpl<T>`.
- When wrapping a parameterized parent/child relationship, verify runtime checks
  accept the mapped parent type, for example `FutureImpl<int>` as `Future<int>`.
- Review copy/clone/serialization paths. Copies must preserve the instantiated
  type. Serialization must either preserve the type arguments or fail explicitly.

## Test Checklist

Add focused tests for each conversion:

- Successful construction with concrete type arguments.
- Producer methods accept correct values and reject incorrect values.
- Consumer methods return the declared concrete type.
- Raw construction remains compatible when `legacy_raw` or
  `legacy_raw_construct` is used.
- Raw annotations accept parameterized instances only when
  `legacy_raw` or `legacy_raw_accepts` is used.
- Invariance and wildcards: `Name<auto>` must not accept `Name<int>` unless a raw
  annotation is used, while `Name<?>` and bounded wildcard annotations should
  accept compatible instantiations.
- Parameterized vparent checks, when relevant.
- Static generic methods and method-level generic calls, when relevant.
- AOT/IR round-trip for parameterized object types when the class appears in
  compiled code.

## Documentation And Metadata Checklist

Update all applicable documentation:

- Class/method Doxygen examples should show concrete generic use.
- Language docs should explain user-facing type behavior and compatibility.
- Release notes should identify the migrated classes and compatibility rules.
- `design/api-metadata-for-modules.md` should describe any new metadata fields.
- Binary module release notes should mention source compatibility and any places
  where raw examples were replaced with typed examples.

## Binary Module Rollout

When applying this to `~/src/qore/git/module-*`:

1. Search for QPP classes whose methods move one logical type through an object:
   `rg -n "qclass|auto .*::|\\bFuture\\b|Queue\\b|Iterator" ~/src/qore/git/module-*`.
2. For each candidate, write down the type parameter meaning before editing.
3. Convert one class family at a time and keep commits small.
4. Run the module's focused tests and any core tests that exercise the public API.
5. Run `audit-changes` before each commit.
6. Check generated `.meta.json` output for the generic spelling and raw
   compatibility flags.
7. Prefer explicit typed examples in docs and qlib/module code when the type is
   known; keep raw examples only where the value type is intentionally erased.
8. Use method-level generics for API helpers that merely transform or echo a
   call-local type and should not force a class-level type parameter.
