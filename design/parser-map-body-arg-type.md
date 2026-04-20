# Option B — propagate lvalue hint into map/foreach body `$1` type

## Goal

Extend the lvalue-hint channel we landed in `72b91b78c` so that
sub-expressions *inside* map/foreach bodies see a narrowed `$1` type
when the iterator source has a known element type, rather than always
falling through to `auto`.

## Current state (after `72b91b78c` + `8070e5a64`)

The hint chain today stops at the map's *return* type.  Narrowing
works when the body's inferred type would otherwise lose to auto AND
the lvalue supplies a hash<K, V> hint:

```
hash<string, softint> h = map {$1.name: $1.id}, src.iterator();
                          ^                               ^
                          $1 still auto                   iterator source
                          here; only the                  has element type
                          outer hash narrows              but it's not used
                          via the hint channel
```

Good enough for the `cast<>` Workflow.qc pattern because the outer
typing was what the developer wanted.  But cases where the body
itself has a typing problem (e.g., `$1.field` where `.field` is a
method call that needs to resolve, or nested map, or foreach
destructuring) still see `$1` as auto and fail the same way.

## The extension

Two distinct hint sources to propagate into the body:

**(B1) Iterator element type — pushes down through `$1`.**
When the iterator expression has a known element type (via
`getUniqueReturnComplexList` on `list<T>`, `getComplexHashValueType`
on `hash<K, V>::iterator()`, etc.), set the implicit-arg type for
`$1` inside the body to that T rather than `auto`.

Today `lib/QoreHashMapOperatorNode.cpp:88-91`:
```cpp
const QoreTypeInfo* implicitArgType =
    QoreTypeInfo::getImplicitArgTypeForIterator(e[2], iteratorTypeInfo);
ParseImplicitArgTypeHelper pia(implicitArgType);
```

`getImplicitArgTypeForIterator` already does some of this — but it
falls back to auto for `AbstractIterator`-typed sources (the Workflow
case).  The extension: when the iterator is an object whose class
has an `iterator()` return type declared `AbstractIterator` AND the
receiver has a complex-list/hash element type in scope, use the
receiver's element type directly.  This is option A from our earlier
discussion, but scoped to the map operator's immediate use.

**(B2) Lvalue value type — pushes down through return-value slot.**
Already partially in place: `QoreHashMapOperatorNode::setReturnTypeInfo`
consumes the expected hash-value type when the body lands on auto.
The extension is to also push the lvalue's *value* type INTO the body
value expression's parse-init so nested constructs inside the body
can narrow too (e.g., a nested map that produces the hash value).

## Scope

Code to change — roughly the same surface area as 72b91b78c:

1. **`QoreHashMapOperatorNode::parseInitImpl`** (`lib/QoreHashMapOperatorNode.cpp:75-108`):
   - Compute element type from iterator source using
     `QoreTypeInfo::getUniqueReturnComplexList(iteratorTypeInfo)` or
     `getComplexHashValueType` (for hash iterators).
   - Prefer that element type over the current fallback when building
     `implicitArgType`.
   - Before parsing body value (line 99), also set
     `parse_context.expected_type_info` to the outer hint's value
     type so nested constructs inside the body narrow.

2. **`QoreMapOperatorNode::parseInitImpl`** (the list-result variant —
   `lib/QoreMapOperatorNode.cpp`): mirror the same wiring so `map
   <expr>, iter` narrows too.

3. **`QoreForEachStatement::parseInitImpl`** (foreach): set `$1`'s
   implicit-arg type from the source's element type.  Already does
   this for typed-list sources; extend to object-iterator sources
   where the receiver type carries element info.

4. **`QoreParseHashNode`** in nested-narrowing position: no change
   needed — it already consumes `expected_type_info`.

## Test plan

Add cases to `examples/test/qore/operators/lvalue-type-hint.qtest`:

- **Body sees narrowed `$1`**:
  ```qore
  int sum;
  list<int> src = (1, 2, 3);
  sum += map $1 + 1, src.iterator();   // $1 is int, not auto
  ```
- **Iterator over hash<K, V> gets V-typed `$1`**:
  ```qore
  hash<string, int> data = {"a": 1, "b": 2};
  list<string> keys = map $1.key, data.pairIterator();
  ```
- **Nested map with narrowing**:
  ```qore
  hash<string, list<int>> result = map {$1: (1, 2, 3)}, ("a", "b");
  ```
- **Foreach body destructuring** with typed source:
  ```qore
  list<hash<MapperInfo>> items = (...);
  foreach hash<MapperInfo> m in (items) { ... }   // m typed, no cast
  ```

Regression: AOTSmoke 93, JITSmoke 156, all operator tests, the 7
existing lvalue-type-hint cases.

## Alignment check (AST/IR/AOT)

Same as 72b91b78c: it's a parse-time change that sets `typeInfo` /
implicit-arg-type on AST nodes.  All three modes read these through
the same `ParseImplicitArgTypeHelper` machinery.  No new divergence.

The AST/IR hash-literal alignment we just fixed in `8070e5a64`
already ensures that if `$1`'s narrower type surfaces through a hash
literal inside the body, the runtime will honour it correctly in
AST eval too.

## Risk

Low.  Same opt-in model: `parseAccepts` at the assignment site
remains the authoritative gate, every new narrowing can only tighten
inference, never loosen it.  The one wrinkle is when the iterator
source's declared element type is *wider* than what the runtime
actually produces — but that's already a latent bug (the declared
type is the contract).

## Payoff

Targeted sweep in Qorus after landing should show additional
`cast<hash<...>>` / `cast<list<...>>` workaround removals.  Earlier
grep showed ~15 hits on `cast<hash<` in Qorus (most are hashdecl
casts, which this doesn't touch; ~3 are complex-hash narrowings
likely fixable here).  More importantly, opens up cleaner Qore/Qorus
user code going forward — new patterns like `map $1.field, items`
will just work when `items` has a known element type.

## Effort

- 0.5 day: implementation (three parse-init sites + threading)
- 0.5 day: test coverage
- Total ~1 day.
