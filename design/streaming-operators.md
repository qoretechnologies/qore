# Streaming Operators

## Summary

Qore has streaming keyword operators and an `iterate` expression for uniform
iteration:

- `iterate <value>`
- `first [predicate,] source`
- `any [predicate,] source`
- `all predicate, source`
- `count [predicate,] source`
- `take count, source`
- `drop count, source`
- `takewhile predicate, source`
- `takeuntil predicate, source`

The `find` expression supports explicit cardinality modifiers:

- `find first expr in data where (predicate)`
- `find last expr in data where (predicate)`
- `find one expr in data where (predicate)`

These forms provide explicit cardinality without changing the legacy `find`
return contract.

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
modifiers while leaving legacy forms available. Backwards compatibility is
preserved through contextual parsing and parse-option opt-outs; no
source-renaming migration tool is required.

`%no-stream-fusion` disables optional fusion fast paths only. It must not
reintroduce eager intermediate materialization or otherwise change lazy-chain
semantics.

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
important cases are:

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

## Non-Goals

- Source-renaming migration tooling is not part of this feature. Compatibility
  is handled with contextual keyword parsing and parse-option opt-outs.
- The pipe operator `|>` is outside the streaming operator semantics.
- There is no bounded `count limit N` form. Qore has no builtin infinite source;
  user-defined infinite iterators remain the caller's responsibility.
- There is no `collect` operator. Root list-producing streaming operators and
  hard-list assignment materialization cover collection needs.
