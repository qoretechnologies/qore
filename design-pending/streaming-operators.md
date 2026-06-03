# Streaming Operators (`first` / `any` / `all` / `take` / `drop` / `takewhile` / `takeuntil` / `count`) + Operator-Chain Fusion + `iterate` keyword

**Status:** Design.

**Target:** Qore 2.3 alongside [`char-type.md`](../design/char-type.md).

**Companion:** [`pipe-operator.md`](pipe-operator.md) — separate
proposal for `|>`-style pipe syntax (recommended deferred). The operators
proposed here are the building blocks pipe would chain; they're useful on
their own in the existing nested-operator syntax and earn their place
without pipe.

---

## 1. Motivation

The text-processing branch and the `char` proposal close the per-element
performance gap. Two complementary gaps remain at the **chain** level:

### 1.1 The "find first" gap

Today's `find` operator has unusual three-way return semantics
(see `lib/FindNode.cpp:69-106`):

| match count | returned value |
|---|---|
| 0 | NOTHING |
| 1 | the matched element directly (not a list) |
| 2+ | list of matched elements |

The variable return type alone makes `find` awkward to compose: the
caller can't write a single `[0]` unwrap that's correct for both the
"single match" and "multi match" cases (in the single-match case, `[0]`
indexes *into* the matched element). The two existing workarounds for
"first match only" both have problems:

```qore
# Workaround 1: foreach + break (no nested-operator form possible;
# breaks composition with map/select/foldl)
*hash<auto> first_match;
foreach hash<auto> row in (some_list) {
    if (row.value > 0) {
        first_match = row;
        break;
    }
}

# Workaround 2: cast + index — only correct in the multi-match case
list<auto> all = find $1 == "x" in some_list where ($1.value > 0);
*hash<auto> first_match = all.size() ? all[0] : NOTHING;
# Still allocates the entire result list to throw everything but the first away.
```

A consistent-return-type `find first` (always returns `*T`) is needed.

The same gap exists for "does any element match?" (`any`), "do all elements
match?" (`all`), "first N elements" (`take`), "skip first N" (`drop`),
"take while / until a condition holds." Every reasonably mature collections
library has these; Qore doesn't, except via the hand-rolled
`foreach + break` pattern.

### 1.2 The remaining chain-execution gap

Qore already has lazy functional evaluation for the existing functional
operators. In particular, nested non-hash `map` / `select` chains under
`foldl`, `foldr`, or `foreach` do **not** materialize a list at every
stage:

```qore
foldl $1 + $2, (map $1 * 2, (select source, $1 > 0))
```

This shape is already evaluated through the `FunctionalOperatorInterface`
chain, so the `select` and `map` stages feed values lazily to `foldl`.

Materialization still exists when the expression asks for a materialized
result, for example:

```qore
list<int> l = map $1, source.iterator();
```

That is correct: the root `map` expression returns a list. The remaining
gap is different:

1. The lazy path is an interpreted chain of iterator wrapper objects,
   so every element still pays per-stage wrapper / virtual-dispatch cost.
2. The lazy path only covers the existing operator set and the existing
   lazy contexts.
3. There are no terminal short-circuit operators such as `first`, `any`,
   or `take 10` that can decide a result before walking the whole source.

### 1.3 Two pieces address the gaps

Together, the new operators and a compiler pass that recognizes nested
`OperatorNode(consumer, [..., OperatorNode(producer, [..., source])])`
patterns and emits a single fused loop close the remaining gaps:

- Streaming operators with short-circuit semantics (`first`, `any`,
  `take 10`) terminate the source iteration as soon as the result is
  decided. No needless work.
- Existing lazy transformation chains (`select` + `map` + `foldl`) execute
  through a single emit node instead of a stack of iterator wrappers.
- New operators participate in the same lazy/fused execution model from
  day one instead of each inventing its own partial fast path.

This proposal is **two pieces, both useful independently**:

| Piece | Effort | Contribution |
|---|---|---|
| New keyword operators + hard-list materialization | ~2-3 weeks | Closes the find-first gap and the related any/all/take/drop/takewhile/takeuntil gaps; lets iterator-returning stages materialize naturally at a `list<T>` target |
| Operator-chain fusion optimizer | ~4-6 weeks | Replaces existing lazy wrapper chains with a single fused loop and extends lazy execution to the new stages |

Both ship in 2.3.

---

## 2. The new operators

All follow the existing Qore operator convention — keyword prefix,
comma-separated arguments, source on the right (consistent with `select`,
`map`, `foldl`). Each is also a fully-fledged AST node that participates
in fusion §3.

### 2.1 `first` — first element matching a predicate

```qore
first <pred-expr>, <iterable>     # returns *T (the element type, or NOTHING)
first <iterable>                  # no predicate; returns first element
```

The predicate uses `$1` for the current element (matching `select` and
`foldl`).

```qore
# Find first error line
*string err = first $1 =~ /ERROR/, log.splitLines();

# First element of an iterator
*int n = first xrange(10, 100);   # 10
```

`first` short-circuits — it stops iterating as soon as a match is found.
For sources that are real iterators (not pre-built lists), this means
work proportional to the position of the match, not the size of the
source.

### 2.2 `any` and `all`

```qore
any <pred-expr>, <iterable>       # bool — True if any element matches
any (<iterable>)                  # bool — True if iterable yields >= 1 element
all <pred-expr>, <iterable>       # bool — True if every element matches
```

```qore
bool any_critical = any $1 =~ /CRITICAL/, log.splitLines();
bool all_positive = all $1 > 0, scores;
bool not_empty    = any (source);
```

Both short-circuit: `any` stops on the first match, `all` stops on the
first miss. `all` of an empty source is True (vacuous truth, matches
mathematical convention).

### 2.3 `take` and `drop`

```qore
take <int-expr>, <iterable>       # iterator yielding first N elements
drop <int-expr>, <iterable>       # iterator yielding all but first N
```

Unlike the other terminals, `take`/`drop` are **non-terminal** —
they yield iterators that compose into further stages.

```qore
auto first_10_error_iter = take 10,
                                (select log.splitLines(), $1 =~ /ERROR/);
list<string> first_10_errors = first_10_error_iter;

# Skip header, process body
foreach hash<auto> row in (drop 1, csv.splitLines()) {
    process(row);
}
```

`take` short-circuits the source after N elements. `drop` skips the first
N then yields the rest. If a list is needed, assign the iterator to a
hard-list target (§2.11).

### 2.4 `takewhile` and `takeuntil`

```qore
takewhile <pred-expr>, <iterable>     # iterator yielding elements until pred returns False
takeuntil <pred-expr>, <iterable>     # iterator yielding elements until pred returns True
```

Non-terminal, like `take`/`drop`.

```qore
# Lines until the first blank line (e.g., HTTP header end)
auto header_iter = takewhile $1 != "", source.splitLines();
list<string> headers = header_iter;

# Read until we hit a sentinel
auto body_iter = takeuntil $1 == "<<END>>", source.splitLines();
list<string> body = body_iter;
```

`takewhile` reads while the predicate holds; `takeuntil` reads until it
holds. Both stop iterating immediately at the boundary — no peek-ahead
needed by the caller.

**Why not `while` / `until`?** `while` is already a Qore reserved
keyword for control-flow statements (`while (cond) statement`). Adding
it as an expression operator (`while EXPR, ITER`) would force the
grammar to disambiguate `while (` between the statement and operator
forms via parser-level lookahead — workable, but a real grammar
maintenance burden. The renamed `takewhile` / `takeuntil` (matching
Haskell, F#, Rust's `take_while`) avoids the issue. `until` is not a
current keyword but is renamed for symmetry.

### 2.5 `count`

```qore
count <iterable>                  # int — number of elements
count <pred-expr>, <iterable>     # int — number of elements matching pred
```

```qore
int total      = count log.splitLines();
int err_count  = count $1 =~ /ERROR/, log.splitLines();
int letters    = count $1.isLetter(), str.codePointIterator();
```

`count` without a predicate is `<iterable>.size()` for lists; for
iterators it walks them all. With a predicate it's equivalent to
`count (select <iterable>, <pred>)` but compiles to a single pass.

### 2.6 Relationship to existing `find`

The existing `find` operator is a context expression, not a collection
operator. It iterates a data expression suitable for a Qore context
(typically a hash of lists such as a datasource result), exposes columns
with `%name` references, evaluates a result expression for each matching
row, and returns:

| match count | returned value |
|---|---|
| 0 | NOTHING |
| 1 | the result expression directly |
| 2+ | list of result-expression values |

That legacy form stays unchanged.

This proposal additionally extends it with `first` / `last` / `one`
modifiers:

```qore
# Existing
auto matches = find %name, %id in rows where (%status == "open");

# New: first matching row result (returns *T, not a list)
*string first_name = find first %name in rows where (%status == "open");

# New: last matching row result
*string last_name = find last %name in rows where (%status == "open");

# New: exactly one matching row result
# - 0 matches → NOTHING
# - 1 match  → the result expression
# - 2+ matches → throws MULTIPLE-MATCHES-ERROR with the offending count
*string only_name = find one %name in rows where (%status == "open");
```

`find first` and `first <pred>, <iterable>` are related but not identical:

- `find first ... in ... where (...)` preserves context semantics
  (`%column` references, result expression, hash-of-lists data shape).
- `first <pred>, <iterable>` is the collection / iterator operator
  (`$1` current element, natural iterable element type).

They can share lower-level short-circuit implementation helpers, but they
should remain distinct AST nodes because their binding and type-checking
rules are different. `find last` and `find one` are also new and remain
specifically `find`-family operations.

### 2.7 The `iterate` keyword — uniform iterator factory

The `iterate` keyword is a verb-form operator meaning "treat this as an
iterator." It returns an iterator over the expression's **natural
element type**, picked by the type:

| `X` is | `iterate X` element type | iterator class returned |
|---|---|---|
| `string` | `char` | `StringCharIterator` |
| `binary` | `int` (raw byte 0..255) | `BinaryByteIterator` (new — trivial) |
| `list<T>` | `T` | `QoreListIterator` |
| `hash<K, V>` | `hash<{key: K, value: V}>` (a pair) | `QoreHashIterator` (pair view) |
| `int N` (positive) | `int` (range `0..N-1`) | `RangeIterator` |
| `range<int>` | `int` | `RangeIterator` |
| any object exposing `<class>::iterator()` | element type from that iterator | (delegated) |

`iterate` is a uniform-naming win over the per-type factory methods. Both
forms work — `iterate str` and `str.codePointIterator()` produce the same
iterator value:

```qore
# Equivalent
foreach char c in (iterate str) { ... }
foreach char c in (str.codePointIterator()) { ... }

# Equivalent — both produce a StringCharIterator
*char first_letter = first $1.isLetter(), iterate str;
*char first_letter = first $1.isLetter(), str.codePointIterator();
```

`iterate` is independent of the [`pipe-operator.md`](pipe-operator.md)
proposal — it's useful in nested-form chains as a discoverability win
and shorter syntax. If pipe ships later, `iterate` is the natural way to
introduce a chain (`iterate str |> filter ... |> count`); without pipe,
`iterate` still earns its keep as a uniform replacement for the per-type
factory methods (`<string>::codePointIterator()`,
`<hash>::pairIterator()`, etc.).

The existing factory methods stay for backward compatibility.

### 2.8 Argument syntax summary

| Operator | Args | Returns |
|---|---|---|
| `first <pred>, <iterable>` | predicate + source | `*T` |
| `first <iterable>` | source only | `*T` |
| `any <pred>, <iterable>` | predicate + source | `bool` |
| `any (<iterable>)` | source only | `bool` |
| `all <pred>, <iterable>` | predicate + source | `bool` |
| `take <n>, <iterable>` | int + source | iterator |
| `drop <n>, <iterable>` | int + source | iterator |
| `takewhile <pred>, <iterable>` | predicate + source | iterator |
| `takeuntil <pred>, <iterable>` | predicate + source | iterator |
| `count <iterable>` | source only | `int` |
| `count <pred>, <iterable>` | predicate + source | `int` |
| `find first <expr> in <data> where (<where>)` | (extends existing find) | `*T` |
| `find last <expr> in <data> where (<where>)` | (extends existing find) | `*T` |
| `find one <expr> in <data> where (<where>)` | (extends existing find) | `*T` |

For collection-style operators, `<pred>` and `<expr>` may use `$1` to
refer to the current element (matching the `select`/`map`/`foldl`
convention). For `find` modifiers, `<expr>` and `<where>` use existing
find/context binding rules such as `%column`.

### 2.9 Behaviour on hashes

For all operators that take an `<iterable>`, a hash source iterates
over **pairs** by default — same as `iterate h` (§2.7) and the existing
`<hash>::pairIterator()`. So:

```qore
hash<auto> h = {"a": 1, "b": 2, "c": 3};

# first hash pair (per the iterate element type for hashes)
*hash<auto> p = first h;                    # {key: "a", value: 1}

# count pairs whose value is positive
int n = count $1.value > 0, h;              # 3
```

Callers that want keys-only or values-only use the existing factory
methods or `iterate` over a derived iterator:

```qore
*string first_key = first iterate h.keyIterator();
*int first_val    = first iterate h.valueIterator();
```

### 2.10 Side-effect ordering with short-circuit

The short-circuit operators (`first`, `any`, `all`, `take`, `takewhile`,
`takeuntil`) iterate the source only as far as needed to produce the
result. If the predicate has side effects, **only the elements visited
before the result is decided are processed**:

```qore
list<int> seen = ();
*string err = first ((seen += $1.size()) || $1 =~ /ERROR/),
                    log.splitLines();
# After this, `seen` contains an entry for each line up to and including
# the first ERROR — not for every line in the log.
```

This is the correct streaming behaviour but worth explicit
documentation: programmers porting from `select`/`map` (which iterate
the entire source) need to know that the new operators don't.

### 2.11 Materialization on hard-list assignment

Iterator-yielding expressions can be materialized by assigning them to a
hard-list target:

```qore
auto limited_iter = take 10, source;       # still lazy
list<int> limited = limited_iter;          # consumes the remaining iterator elements

list<string> headers = takewhile $1 != "", source.splitLines();
```

This is part of the initial proposal because `take` / `drop` /
`takewhile` / `takeuntil` intentionally return iterators for composition,
but callers often need the final result as a list. Without target-driven
materialization, the practical spelling would be an identity `map`:

```qore
list<int> limited = map $1, (take 10, source);
```

That is correct but too obscure to make the streaming operators feel
natural.

The rule is deliberately narrow:

| Target type | Assignment from `AbstractIterator` |
|---|---|
| `list<T>` / `*list<T>` | materializes the iterator's remaining elements into a list, folding each element through `T` |
| `auto`, `any`, `object`, `AbstractIterator` | stores the iterator object |
| `softlist<T>` | keeps existing soft-list semantics; an iterator object is one value, not a stream to collect |

Materialization consumes the iterator from its current position to
exhaustion. The new list owns referenced copies of the yielded values.
Element type errors are reported at the element that fails type folding,
after any earlier elements have already been pulled from the iterator.
This is the same unavoidable side-effect boundary as manual
materialization with `map $1, iter` or a `foreach` loop.

This feature is **not** general "iterators materialize on assignment."
The broad rule would be too surprising: assignment to `auto` would no
longer preserve iterator objects, and assigning a file, SQL, channel, or
user-defined iterator could unexpectedly block or drain an external
resource. Hard-list assignment is explicit enough at the target type and
matches the programmer's stated request for a list.

---

## 3. Operator-chain fusion

A new compiler pass walks the AST after type checking and fuses nested
operator chains into single-loop emit nodes.

### 3.1 What gets fused

The pass recognizes the pattern:

```
OperatorNode(consumer, [..., OperatorNode(producer, [..., source])])
```

where `consumer` ∈ `{foldl, foreach, map, select, first, any, all, count,
take, drop, takewhile, takeuntil}` and `producer` is in the same set or
any iterator-yielding expression. The pattern recurses — chains of
arbitrary depth fuse into a single loop.

### 3.2 What the fused loop looks like

For a chain like:

```qore
int n = count $1 =~ /ERROR/, (map $1.lwr(), log.splitLines());
```

the non-fused implementation should use the existing lazy functional
operator machinery, not build temporary lists:

```cpp
int n = 0;
std::unique_ptr<FunctionalOperatorInterface> f = map.getFunctionalIterator(...);
while (!f->getNext(iv, xsink)) {
    if (iv =~ /ERROR/) {
        ++n;
    }
}
```

That is already one pass and no intermediate result list for this shape,
but it still routes each element through the functional-operator wrapper
chain.

After fusion:

```cpp
int n = 0;
StringSplitIterator it(log, "\n");      // source iterator (already O(1) per next)
while (it.next()) {
    QoreStringNode* el = it.getValue().lwr();   // inlined map body
    if (el =~ /ERROR/) ++n;                     // inlined count predicate
}
```

One pass, no intermediate lists.

### 3.3 Fusion rules

The pass is conservative:

- **Stage closures with side effects** (function calls, mutations,
  exceptions) preserve their evaluation order — fusion is sequential per
  element, not parallel, so this is automatic.
- **Short-circuit semantics** (`first`, `any`, `take N`, `takewhile`,
  `takeuntil`) emit `break` from the fused loop at the boundary.
  Non-fused fallback uses iterator `.next()` returning false.
- **Reference holding.** The source expression is evaluated once; the
  fused loop holds a ref via existing iterator lifetime rules.
- **Mixed sources.** If a stage's source is not a fuseable form (e.g., a
  reflective access, a user-defined iterator class with custom
  semantics), that stage falls back to the non-fused emit. The rest of
  the chain still fuses around it.

### 3.4 Non-fused fallback

If fusion can't apply (custom iterator class, reflection, optimizer
disabled), the chain emits as today — existing lazy functional evaluation
where available, otherwise ordinary operator evaluation. Programs run
correctly either way; fusion is a performance optimization, not a
correctness requirement.

### 3.5 What this gives us at the user level

**Zero source migration needed.** Every `foldl/map/select` chain in qlib,
in every external module, in every user program, automatically runs
faster after this optimizer ships.

The new operators (§2) participate in fusion automatically — they're
compiled to AST nodes the optimizer recognizes.

---

## 4. Migration & compatibility

### 4.1 Source compatibility

100% additive at the language level:

- New operator keywords (`first`, `any`, `all`, `take`, `drop`,
  `takewhile`, `takeuntil`, `count`, `iterate`) become reserved words. Any
  existing identifier with these names becomes a parse error.  Survey
  needed before release.
- Existing operators (`find`, `select`, `map`, `foldl`, `foreach`)
  unchanged.
- `find first` / `find last` / `find one` — extensions to existing
  `find` syntax. Doesn't break anything because `first`/`last`/`one`
  after `find` was a parse error before.
- `while` is **not** added as an expression operator; renamed to
  `takewhile`/`takeuntil` to avoid the grammar conflict with the
  existing `while` statement keyword.

### 4.2 Behavior compatibility

Fusion is observationally equivalent to the accepted non-fused form.
For currently lazy `map` / `select` / `foldl` / `foreach` chains, programs
with side effects in stage closures should observe the same per-element
ordering as the existing `FunctionalOperatorInterface` path. Root
expressions that intentionally materialize, such as:

```qore
list<int> l = map $1, iter;
list<int> l2 = take 10, iter;
```

are not fused across the statement boundary; materialization remains the
observable result.

Hard-list assignment materialization is an additive behavior change for
assignments that are currently rejected at parse time, for example
`list<int> l = some_iterator;`. It does not change assignment to `auto`,
`any`, `object`, `AbstractIterator`, or `softlist<T>`, which continue to
preserve the iterator object under existing rules.

### 4.3 Reserved-word collisions and disambiguation

Each new operator keyword (`first`, `any`, `all`, `take`, `drop`,
`takewhile`, `takeuntil`, `count`, `iterate`) potentially conflicts
with existing user-code identifiers. The conflict surface differs per
keyword:

| Keyword | Conflict surface |
|---|---|
| `any` | **Already a Qore type name** (`any $x = ...`). Type names in Qore can also be used as variable/function/method/class names in non-type contexts, so `int any = 5;` is legal today. The new operator usage is yet another context; the parser must distinguish all three. |
| `count` | Very common as user variable name (counters, totals). Likely the highest-volume conflict in real code. |
| `first` | Moderately common as variable name. |
| `iterate` | Verb-form picked deliberately to avoid `iter` (extremely common as iterator-variable name). The verb form is rarely used as a noun-style variable name. |
| `all`, `take`, `drop`, `takewhile`, `takeuntil` | Less common but possible as identifiers. |

**Disambiguation by parse context.** The parser uses position to
distinguish operator from identifier usage:

| Position | `count` interpreted as |
|---|---|
| Expression-statement start, no immediately-following `(` | operator (e.g., `count source;`) |
| Expression with immediately-following `(` | function call (`count(arg);`) |
| Identifier position (after `int`, `string`, etc.) | identifier (`int count = 0;`) |
| Inside a `find ... in ... where` clause | not an operator (the find form has its own grammar) |

This is the same disambiguation Qore already does for keyword
operators that share names with possible identifiers. Adding the new
operators extends an existing parsing pattern rather than introducing
a new kind of ambiguity.

**`any` specifically** — the existing duck of "any is a type AND can
be used as an identifier" extends to "any is also an operator at
expression position." The source-only operator form uses parentheses
(`any (source)`) so `any name;` remains a declaration. Predicate forms
such as `any $1 > 0, source` remain unambiguous. If the remaining
conflict surface in qlib is too large, we fall back to the alternative
names below.

**Alternative names if context-sensitive parsing proves insufficient:**

| Primary | Fallback |
|---|---|
| `any <pred>, src` | `anymatch <pred>, src` |
| `all <pred>, src` | `allmatch <pred>, src` |
| `count <pred>, src` | `countof <pred>, src` |
| `first <pred>, src` | `firstof <pred>, src` |

The fallback names are ugly but unambiguously clash-free. We start
with the cleaner names; switch if the qlib survey shows widespread
breakage.

**Migration tooling.** `qore-migrate-rename` ships with the release.
Survey runs as part of the 2.3 release-readiness check; conflicts are
listed in the migration guide.

### 4.4 Parse options — per-keyword opt-out for backward compatibility

Every new keyword introduced by this proposal gets a matching parse
option that disables the keyword in the lexer for that program /
module. Older sources that use one of the keywords as an identifier
can opt out without rewriting:

| Parse directive | Parse option flag (C++) | Disables |
|---|---|---|
| `%no-iterate` | `PO_NO_ITERATE` | `iterate` keyword (still parses as identifier) |
| `%no-first` | `PO_NO_FIRST` | `first` keyword (still parses as identifier); also disables `find first` |
| `%no-any-operator` | `PO_NO_ANY_OPERATOR` | `any` operator usage (the type-name usage is unaffected) |
| `%no-all-operator` | `PO_NO_ALL_OPERATOR` | `all` keyword |
| `%no-count` | `PO_NO_COUNT` | `count` keyword |
| `%no-take` | `PO_NO_TAKE` | `take` keyword |
| `%no-drop` | `PO_NO_DROP` | `drop` keyword |
| `%no-takewhile` | `PO_NO_TAKEWHILE` | `takewhile` keyword |
| `%no-takeuntil` | `PO_NO_TAKEUNTIL` | `takeuntil` keyword |
| `%no-find-modifiers` | `PO_NO_FIND_MODIFIERS` | `find first`/`last`/`one` (the bare `find` form stays) |

Plus a bundled escape hatch:

| Parse directive | Parse option flag | Disables |
|---|---|---|
| `%no-streaming-operators` | `PO_NO_STREAMING_OPERATORS` | All of the above in one go (sets the union of bits) |

Programs using the old single-name `find` form continue to parse
unchanged whether or not these options are set. The opt-outs only
affect new keyword recognition.

**Why per-keyword granularity instead of a single bundle?** Modules
usually have only one or two collisions, not all of them. A
fine-grained opt-out lets a module preserve its single conflicting
identifier while still using the rest of the new keywords.

**Lifecycle.** The opt-outs ship in 2.3 and stay supported until the
release after most callers have migrated. Removal is opt-in via the
release notes; no automatic deprecation timer.

The flag values share the parse-option bit space documented in
`include/qore/qore_program_options.h`. We have ~14 free bits at the
time of writing; the new flags fit comfortably.

### 4.5 Documentation

`doxygen/lang/operators.dox.h` (or whatever the canonical operators page
is) gets entries for the new operators. The migration guide for 2.3
mentions the new reserved words.

---

## 5. Performance projections

**All numbers in this section are estimates** extrapolated from the
`bugfix/text_processing` branch's measured fusion-style wins (the lazy
iterator + native fast-path commits already gave us a baseline). They
need to be confirmed against a fusion-pass prototype before release.

### 5.1 Multi-stage chains (fusion) — estimated

Existing `foldl X, (map Y, src)` and similar lazy-supported shapes already
avoid intermediate list materialization. The fusion win for those cases is
therefore expected to come from:

- removing per-stage `FunctionalOperatorInterface` wrapper dispatch,
- enabling more static type propagation through the whole stage chain,
- making the new operators use the same optimized path immediately.

The exact win needs a prototype benchmark against the current lazy baseline.
It should not be estimated from an eager "one list per stage" model.

Real qlib chain shapes (~2 stages over small input) are likely to gain less
in absolute terms because the chain is short and cheap. The win is most
visible in longer chains, in new short-circuit operators, and in cases not
covered by the current lazy path.

### 5.2 Short-circuit operators

| Workload | Today | With new operators |
|---|---|---|
| First ERROR in 50K-line log | full `select`/`map`-style scan or `foreach + break` (manual) | ~5 ms (`first` short-circuits at line 1 if hit) |
| `any =~ /CRITICAL/` over 50K-line log | ~50 ms (using `select` + `size() > 0`) | ~5 ms (`any` short-circuits) |
| `take 10` matching lines from a stream | manual counter pattern | one-line operator |

The `first` / `find first` cases in particular go from full O(N) scans to
O(position-of-first-match) because the new operators stop iterating
immediately on hit.

### 5.3 Existing chains (zero migration) — estimated

Every existing lazy-supported `foldl(map(...))` chain in qlib is eligible
for the lower-overhead fused path once fusion ships. The specific qlib
chain shapes are short (2 stages) over small input (typically <1 KB column
lists, schema headers, etc.), so the **absolute** gains may be small. We
have not benched these directly; the release benchmark must compare
against the existing lazy implementation, not against a materializing
baseline.

---

## 6. Effort estimate

| Component | Estimate (sequential) |
|---|---|
| Lexer changes (new reserved words: `first`, `any`, `all`, `take`, `drop`, `takewhile`, `takeuntil`, `count`, `iterate`) | 2 days |
| Parser changes (productions for the 8 keyword operators) | 1 week |
| Parser changes for `find first`/`last`/`one` | 3 days |
| Parser + runtime support for `iterate` (uniform iterator factory) | 4 days |
| Hard-list assignment materialization for `AbstractIterator` | 3-4 days |
| AST nodes + non-fused emit for each operator | 2 weeks |
| Fusion optimizer pass | 4-6 weeks |
| Test coverage (qtest per operator, hard-list materialization tests, property tests for fusion equivalence) | 2-3 weeks |
| Reserved-word survey + `qore-migrate-rename` tool | 4 days |
| Doc updates | 1 week |
| **Total (sequential)** | **~15-18 weeks** |
| **Total (with parallelism, 2 developers)** | **~10-12 weeks** |

Order:

1. Operators + non-fused emit + `iterate` keyword + hard-list assignment
   materialization (~4-5 weeks). Releasable here — the operators work,
   fusion just isn't applied yet, programs are no slower than before.
   **The find-first ergonomic win and the `take`/`drop` materialization
   ergonomics are fully realized at this milestone**, even without fusion.
2. Fusion optimizer (~4-6 weeks). Adds the perf-on-existing-chains win.
3. `find first`/`last`/`one` extensions (~3 days, can land any time
   after step 1).

The operators alone are independently shippable in case the fusion pass
slips. Pre-fusion, currently lazy-supported multi-stage chains keep using
the existing lazy functional path, and the new operators still close the
find-first gap, which is the ergonomic win.

---

## 7. Risks and tradeoffs

### 7.1 Reserved-word collisions

`count` is the most likely collision in user code. Mitigation: a survey
before release catches all uses; the few that need rename can be migrated
with `qore-migrate-rename` (a 50-line tool). Other words (`first`, `any`,
`all`, `take`, `drop`, `takeuntil`) are less common but still possible.

### 7.2 Fusion correctness

The optimizer pass produces fused code that must be observably equivalent
to the non-fused emit. Risk: subtle bugs (closure capture, exception
order, side-effect ordering).

Mitigations:

- **Property tests**: each operator's fused emit is tested in parallel
  with the non-fused emit, asserting equal outputs for randomized
  inputs.
- **Conservative fallback**: any AST node the optimizer doesn't
  understand causes that branch to emit non-fused.
- **Optional flag**: `%no-fusion` parse directive disables the optimizer
  per-program for debugging. Removed once the optimizer is settled.

### 7.3 Two related forms: `first` vs `find first`

Both `first <pred>, <iterable>` and `find first ... in ... where (...)`
exist, but they target different binding models. This is still extra
surface area and should be justified clearly.
Mitigations:

- The two forms target different idioms (operator-chain style vs
  context-row style). Programmers writing in one style don't have to
  learn the other.
- `find first/last/one` extends an existing operator family the user
  already knows; not adding it would force users into the standalone
  collection operators for code that is already naturally a `find`.
- The implementations can share lower-level short-circuit helpers without
  pretending the AST or type-binding rules are identical.

### 7.4 Operator complexity creep

Eight new operators is a non-trivial addition to the language surface.
Mitigations:

- Each operator is small (~100-200 LOC including AST node, type
  checking, and fusion stage descriptor).
- Each one has a clear, common idiom — none of them is speculative or
  niche.
- They share a common AST shape (predicate + source for most), so the
  parser productions are mostly templated.

### 7.5 Assignment materialization side effects

Hard-list assignment materialization makes an assignment expression able
to consume an iterator. For bounded in-memory iterators this is expected;
for file, SQL, channel, or user-defined iterators it can perform I/O,
block, or consume an unbounded stream.

Mitigations:

- Limit the rule to hard-list targets only (`list<T>` / `*list<T>`).
  Assignment to `auto`, `any`, `object`, `AbstractIterator`, and
  `softlist<T>` preserves the iterator object.
- Document the operation as consuming the iterator's remaining elements,
  not as copying a reusable collection.
- Keep the new streaming operators' non-terminal forms lazy; only the
  hard-list assignment boundary materializes.

---

## 8. Open questions

1. **`first` without a predicate — should it use `[0]` semantics?**
   `first list` returns `*T` (the first element or NOTHING). `list[0]`
   returns the same thing today. Mostly redundant; the value of
   `first list` is the *uniform* form across iterators (where `[0]`
   doesn't apply). Probably keep both.

2. **`take`/`drop` element type — exact match of source, or always
   iterator?**
   `take 10, list<int>` could return `list<int>` (materialized) or an
   iterator. Materialized is friendlier for callers; iterator is faster
   to compose. Recommendation: iterator — callers who need a list assign
   to a hard-list target, for example `list<int> l = take 10, source;`.
   A dedicated `collect` operator can still be considered later for cases
   where the target type is not a hard-list assignment.

3. **`count` of an infinite source — error or hang?**
   No infinite sources in Qore today (`xrange` requires explicit bounds).
   If a user-defined iterator turns out to be infinite, `count` would
   hang. Could add a `count limit N` form to bound it. Probably YAGNI;
   defer.

4. **Should `find first` be the *only* "find first" form?**
   No. `find first` serves existing context/hash-of-lists code; `first`
   serves ordinary iterables. Recommendation: ship both, but document them
   as related operations with different binding rules, not as aliases.

5. **Lazy iterator interaction with stages that need a `size`.**
   Example: `take size(src) - 1, src` to drop the last element. `size()`
   on an iterator forces full materialization. Probably leave the
   `init/last` element gymnastics to library helpers; the operator set
   here covers the common cases.

---

## 9. References

- [`char-type.md`](../design/char-type.md) — implemented `char` value type
- [`pipe-operator.md`](pipe-operator.md) — separate, deferred
  proposal for `|>` pipe sugar that would lower to the same fused IR as
  this proposal
- `bugfix/text_processing` commits `8165b217b`–`bb1ea2d96` — the qlib
  optimizations that motivated this work
- `lib/parser.ypp` — operator grammar
- `include/qore/intern/AbstractIteratorHelper.h` — existing iterator
  fast-path the fused emit builds on
