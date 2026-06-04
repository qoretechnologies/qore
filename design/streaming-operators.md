# Streaming Operators

**Status:** Implemented with one optional optimization follow-up remaining.

**Target:** Qore 2.3 implementation baseline; follow-up work is tracked here
instead of in `design-pending`.

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
- native streaming IR lowering and nested streaming-stage fusion in
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

## Remaining Gap

### Broader Lazy-Operator Fusion

The current native fusion path is implemented for nested streaming stages. The
original design also called for a broader optimizer that recognizes mixed
chains of existing lazy functional operators such as `map`, `select`, `foldl`,
`foldr`, and `foreach`.

Current IR lowering already has native implementations and some pattern-specific
fused opcodes for common `map` / `select` / `foldl` shapes, but it is not a
general mixed-stage pipeline optimizer.

## Follow-Up Implementation Plan

The AOT, parse-option compatibility, and generic hard-list materialization
items from the original follow-up plan are implemented. The remaining work is a
broader mixed lazy pipeline optimizer.

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

### Phase 2: Introduce a General Lazy Pipeline Descriptor

Goal: create a compiler representation that can fuse mixed streaming and
functional operator chains without duplicating lowering logic per operator.

Tasks:

- Add a chain collector that recognizes a source plus ordered stages:
  - `select`
  - non-hash `map`
  - streaming `take`, `drop`, `takewhile`, `takeuntil`
  - streaming terminals `first`, `any`, `all`, `count`
  - `foldl` / `foldr` terminal reducers
  - root `map` / `select` materialization terminals
- Keep source evaluation exactly once.
- Preserve implicit argument semantics:
  - `$1` for current element
  - `$2` for reducer accumulator/next value where applicable
  - `$#` for current element index
- Preserve side-effect order, exception order, and cancellation behavior.
- Define fallback boundaries for unsupported stages, hash-map operators,
  reflective calls, and AST-delegated expressions.

Acceptance criteria:

- The collector produces a deterministic stage list for supported chains.
- Unsupported chains fall back to existing native or AST lowering.
- `%no-stream-fusion` disables this generalized fusion path while leaving
  operator semantics available.

### Phase 3: Lower Mixed Pipelines to Native IR

Goal: replace wrapper-chain execution for supported mixed chains with one
native loop.

Tasks:

- Reuse the existing streaming fused-loop structure as the first backend.
- Support direct-index list loops for typed list sources where profitable.
- Support iterator loops for arbitrary iterable sources.
- Add list-building terminals for root `map`, `select`, `take`, `drop`,
  `takewhile`, and `takeuntil`.
- Add scalar terminals for `first`, `any`, `all`, `count`, `foldl`, and `foldr`.
- Ensure AST-delegated stage bodies push/pop implicit arguments correctly.
- Ensure exception targets are wired through invoke and throw paths.
- Add loop cancellation checkpoints and lowering-time cancellation checks for
  very large generated chains.

Tests:

- `count P, (map E, source)`
- `count P2, (map E, (select source, P1))`
- `first P2, (map E, (drop N, source))`
- `foldl R, (map E, (select source, P))`
- `take N, (select source, P)` root materialization
- side-effect ordering in each stage
- exceptions thrown by stage predicates and map bodies
- `%no-stream-fusion` equivalence against the same source

Acceptance criteria:

- Supported chains produce the same values and side effects with and without
  fusion.
- Fused IR does not materialize intermediate lists unless the root expression
  requires a list.
- Existing pattern-specific fast opcodes remain valid or are replaced only when
  the generalized path is demonstrably equivalent.

### Phase 4: Extend Fusion to `foreach` Conservatively

Goal: close the statement-level fusion gap without risking control-flow bugs.

Tasks:

- Recognize `foreach` over a supported lazy chain.
- Fuse only when the loop body can be lowered safely with existing statement
  lowering and control-flow targets.
- Preserve `break`, `continue`, `return`, exception handling, and closure
  capture behavior.
- Fall back to existing foreach lowering whenever control-flow targets are not
  representable in the fused loop.

Acceptance criteria:

- `foreach` over `map` / `select` / streaming-stage chains avoids intermediate
  list materialization in supported cases.
- All control-flow tests pass with and without `%no-stream-fusion`.
- Unsupported bodies fall back safely.

### Phase 5: Verification and Benchmarking

Goal: prove correctness first, then quantify performance.

Tasks:

- Add a semantic-equivalence qtest matrix that runs representative chains with
  fusion enabled and with `%no-stream-fusion`.
- Add AOT and JIT smoke coverage for the same representative chains.
- Benchmark:
  - streaming short-circuit operators over large inputs
  - mixed `select` / `map` / `count` chains
  - `foldl(map(...))` qlib-style chains
  - root materialization chains
- Compare against current lazy functional wrapper execution, not against an
  eager materializing baseline.

Acceptance criteria:

- Full test suite passes.
- Benchmarks show no meaningful regression for one-stage chains.
- Mixed chains avoid per-stage wrapper overhead where the generalized fusion
  path applies.

## Explicit Non-Goals

- No source-renaming tooling in this plan. Compatibility is handled with
  contextual keyword parsing and parse-option opt-outs.
- No pipe operator work here. `|>` remains a separate deferred syntax proposal.
- No bounded `count limit N` form. Qore has no builtin infinite source today;
  user-defined infinite iterators remain the caller's responsibility.
- No `collect` operator in this plan. Root list-producing streaming operators
  and hard-list assignment materialization cover the current collection needs.
