# Streaming Operators

**Status:** Implemented.

**Target:** Qore 2.3 implementation baseline.

## Summary

Qore now has streaming keyword operators and an `iterate` expression for
uniform iteration:

- `iterate <value>`
- `first [predicate,] source`
- `any [predicate,] source`
- `all predicate, source`
- `count [predicate,] source`
- `take count, source`
- `drop count, source`
- `takewhile predicate, source`
- `takeuntil predicate, source`

The existing `find` expression also supports explicit cardinality modifiers:

- `find first expr in data where (predicate)`
- `find last expr in data where (predicate)`
- `find one expr in data where (predicate)`

These forms close the old "find first" composition gap without changing the
legacy `find` return contract.

## Implemented Behavior

The implemented operator model is source-compatible by construction. Streaming
operator words are contextual in the parser and remain valid identifiers in
ordinary declaration and member positions. Compatibility is controlled by
parse options rather than by requiring a source-renaming migration tool:

- `%no-iterate`
- `%no-first`
- `%no-any-operator`
- `%no-all-operator`
- `%no-count`
- `%no-take`
- `%no-drop`
- `%no-takewhile`
- `%no-takeuntil`
- `%no-find-modifiers`
- `%no-stream-fusion`
- `%streaming-any`
- `%no-streaming-operators`

`%no-streaming-operators` is the broad compatibility escape hatch. It disables
all streaming keyword operators and the `find first` / `find last` / `find one`
modifiers while leaving legacy forms available. A source migration tool is not
part of the current plan; backwards compatibility should be preserved through
contextual parsing and parse-option opt-outs.

`take`, `drop`, `takewhile`, and `takeuntil` materialize lists when evaluated as
root expressions. When nested under another functional or streaming consumer,
they remain lazy and can short-circuit the source.

Lazy functional operators preserve their chain semantics in IR/JIT/AOT lowering.
Intermediate `map`, `select`, `map-select`, and streaming stages pass values to
downstream consumers as soon as each value is available; they must not
materialize an intermediate result before the downstream stage runs. Root
`map`, `select`, and `map-select` materialize only their final root result when
that result is needed, and preserve scalar-source behavior by returning a
scalar or `NOTHING` when the functional source is scalar. `foldl` consumes lazy
chains in source order.
`foldr` evaluates upstream lazy stages in source order and materializes only the
minimal final input required for reverse reduction. Non-reference `foreach`
over supported lazy chains executes the loop body per produced value without an
intermediate list.

Assignment to an explicit hard-list target, `list<T>` or `*list<T>`, from an
`AbstractIterator` drains the iterator into a new list and folds each yielded
value through `T`. Assignment to `auto`, `any`, `object`, `AbstractIterator`,
and compatible `softlist<T>` targets preserves the iterator object.

`iterate` creates an iterator over the natural element type of its input. The
current important cases are:

- `string` -> `char`
- `binary` -> byte integers
- `list<T>` -> `T`
- `hash<K, V>` -> key/value pair hashes
- bounded integer/range inputs -> integers
- `AbstractIterator` values -> delegated iterator values

## Implemented Coverage

Core implementation exists in the parser, AST, IR, JIT, and AOT serialization
paths:

- parser productions and parse-option scanning in `lib/parser.ypp` and
  `lib/scanner.lpp`
- `QoreIterateOperatorNode`
- `QoreStreamingOperatorNode`
- native streaming IR lowering and mixed lazy pipeline fusion in
  `QoreIRLowering`
- JIT runtime helpers for `iterate`
- AOT expression serialization for `ITERATE` and `STREAMING`
- tree-sitter grammar support in `modules/astparser/grammars/tree-sitter-qore`
- `QoreCodeFormat` support for streaming operators and parse directives

User-facing behavior is documented in:

- `doxygen/lang/175_expressions.dox.tmpl`
- `doxygen/lang/180_operators.dox.tmpl`
- `doxygen/lang/245_parse_directives.dox.tmpl`
- `doxygen/lang/900_release_notes.dox.tmpl`

Focused tests exist in:

- `examples/test/qore/misc/streaming-operators.qtest`
- `examples/test/ir/AOTStreamingOperators.qtest`
- `examples/test/qore/misc/context.qtest`
- `examples/test/qlib/QoreCodeFormat/QoreCodeFormat.qtest`
- `modules/astparser/test/astparser.qtest`

## Completed Follow-Up Work

The AOT, parse-option compatibility, generic hard-list materialization, and
mixed lazy pipeline items from the original follow-up plan are implemented.

### Completed: Freeze Current Semantics

Completed items:

- Treat root `take` / `drop` / `takewhile` / `takeuntil` as list-producing
  expressions and nested occurrences as lazy streaming stages.
- Document that generic iterator-to-list assignment is a planned follow-up, not
  current behavior.
- Document `%no-stream-fusion` as disabling fusion optimization only. Native
  non-fused lowering remains the semantic baseline, including source-stripped
  AOT execution.
- Document that source-renaming migration tooling is intentionally not part of
  the plan.

Completed acceptance criteria:

- No remaining references to the retired pending proposal.
- Stable design docs and Doxygen agree on root materialization, nested laziness,
  and parse-option compatibility.

### Completed: Add AOT Streaming Regression Tests

Goal: cover the AOT implementation before broader changes.

Completed items:

- Add `examples/test/ir/AOTStreamingOperators.qtest`.
- Generate a small module that exercises:
  - `iterate` over string, binary, list, hash, integer range, and an iterator
  - `first`, `any`, `all`, and `count`
  - root `take`, `drop`, `takewhile`, and `takeuntil`
  - nested streaming chains such as `first (drop 2, source)` and
    `count (take 2, source)`
  - `find first`, `find last`, and `find one`
- Compile the module to a source-stripped qmod and execute the same assertions
  through source and loaded artifact paths.
- Include one `%no-stream-fusion` variant to ensure native non-fused lowering
  remains valid when fusion is disabled.

Completed acceptance criteria:

- The new qtest passes in the normal IR/AOT test suite.
- A source-stripped qmod preserves streaming operator behavior.
- Any future AOT serialization break for `ITERATE` or `STREAMING` fails a
  focused test.

### Completed: Harden Contextual Keyword Compatibility

Goal: make parse options the compatibility mechanism for legacy sources.

Completed items:

- Add parser tests proving streaming words remain valid as:
  - local variables
  - global or `our` variables where supported
  - function names
  - method names
  - class members
  - hashdecl field names
  - namespace members where grammar permits
- Add tests for expression-position operator parsing:
  - source-only forms: `first source`, `any source`, `count source`
  - predicate forms: `any $1 > 0, source`, `count f($1), source`
  - direct-call ambiguity requiring parentheses, especially for `any (source)`
- Add tests for every per-keyword opt-out and for `%no-streaming-operators`.
- Keep `%streaming-any` and modern-mode `any` behavior explicit: `any` remains a
  type name in declarations and becomes an operator only in expression contexts.

Completed acceptance criteria:

- Legacy identifier positions work without opt-outs where contextual parsing can
  disambiguate safely.
- Each `%no-*` directive disables only the intended operator surface.
- `%no-streaming-operators` disables all streaming operators and find modifiers
  without breaking legacy `find`.

### Completed: Implement Generic Hard-List Materialization

Goal: make explicit hard-list targets collect iterator values without changing
soft or unrestricted assignment behavior.

Completed items:

- Add parse-time compatibility for assigning an `AbstractIterator` expression to
  `list<T>` or `*list<T>`.
- Add a runtime helper that drains the iterator from its current position into a
  new list, with cancellation checks every 100 yielded elements.
- Fold each yielded value through the target element type and report type errors
  at the failing element.
- Keep assignment to `auto`, `any`, `object`, `AbstractIterator`, and
  `softlist<T>` unchanged.
- Delegate runtime materialization to the shared typed-list assignment path used
  by AST, IR, JIT, and AOT execution modes.

Completed tests:

- `list<int> l = xrange(3);`
- `list<int> l = iterate 3;`
- `list<char> l = iterate "abc";`
- `list<hash<auto>> l = iterate {"a": 1};`
- custom `AbstractIterator<T>` materialization
- nullable hard-list assignment from `NOTHING`
- type-folding failure after earlier elements have been consumed
- negative test proving `softlist<T>` stores the iterator as one value
- negative test proving `auto` preserves the iterator object

Completed acceptance criteria:

- Hard-list targets collect iterator values consistently in AST, IR, JIT, and
  AOT execution modes.
- Existing unrestricted and soft-list assignment behavior is unchanged.
- Long or user-defined iterators remain cancellable while materializing.

### Completed: Preserve Mixed Lazy Pipeline Semantics in IR

Goal: make IR lowering preserve the lazy functional-operator contract for mixed
streaming and functional chains.

Completed items:

- Add a deterministic lazy-chain collector for:
  - `map`
  - `select`
  - `map expr, source, predicate`
  - `take`, `drop`, `takewhile`, and `takeuntil`
- Lower streaming terminal consumers (`first`, `any`, `all`, `count`) over mixed
  lazy chains without materializing intermediate lists.
- Lower root `map`, `select`, and `map-select` over lazy chains by passing each
  produced element through the ordered stages and materializing only the root
  result when the value is needed.
- Preserve scalar functional-source behavior by unwrapping the final root result
  to a scalar or `NOTHING` when the innermost functional source is scalar.
- Lower `foldl` over lazy chains in source order without intermediate
  materialization.
- Lower `foldr` by evaluating upstream lazy stages in source order and
  materializing only the final reducer input needed for reverse iteration.
- Lower non-reference `foreach` over supported lazy chains as one iterator loop,
  preserving loop-variable assignment, `$#`, `break`, `continue`, and loop local
  cleanup.
- Preserve implicit argument semantics:
  - `$1` for the current stage element
  - `$2` for reducer element values
  - `$#` for the current stage or foreach index
- Keep lowering-time cancellation checks for large generated chains and runtime
  loop checkpointing on fused loops.
- Treat `%no-stream-fusion` as an optimization directive only; it must not
  reintroduce eager intermediate materialization or change lazy-chain semantics.

Completed tests:

- streaming terminals over `map`, `select`, `map-select`, and streaming stages
  stop reading the source as soon as the terminal result is known
- root `map` over `select` preserves interleaved side effects
- `foldl` and `foldr` over `map(select(...))` preserve source-stage side-effect
  order
- non-reference `foreach` over `map(select(...))` runs the body per produced
  value and short-circuits correctly on `break`
- scalar functional chains such as `map $1, (select 3, True)` return scalar
  results, while streaming chains such as `map $1, (take 2, 3)` return lists
- `%no-stream-fusion` preserves lazy-chain semantics

## Explicit Non-Goals

- No source-renaming tooling in this plan. Compatibility is handled with
  contextual keyword parsing and parse-option opt-outs.
- No pipe operator work here. `|>` remains a separate deferred syntax proposal.
- No bounded `count limit N` form. Qore has no builtin infinite source today;
  user-defined infinite iterators remain the caller's responsibility.
- No `collect` operator in this plan. Root list-producing streaming operators
  and hard-list assignment materialization cover the current collection needs.
