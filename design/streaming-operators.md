# Streaming Operators (`first` / `any` / `all` / `take` / `drop` / `while` / `until` / `count`) + Operator-Chain Fusion

**Status:** Design.

**Target:** Qore 2.3 alongside [`char-type.md`](char-type.md).

**Companion:** [`iter-pipe-operator.md`](iter-pipe-operator.md) — separate
proposal for `|>`-style pipe syntax (recommended deferred). The operators
proposed here are the building blocks pipe would chain; they're useful on
their own in the existing nested-operator syntax and earn their place
without pipe.

---

## 1. Motivation

The text-processing branch and the `char` proposal close the per-element
performance gap. Two complementary gaps remain at the **chain** level:

### 1.1 The "find first" gap

Today's `find` operator returns *all* matching elements:

```qore
list<auto> result = find $1 == "x" in some_list where ($1.value > 0);
# returns every match, even if the caller only wanted the first
```

There is no built-in operator for "first matching element only." The
existing workarounds:

```qore
# Workaround 1: foreach + break (no nested-operator form possible)
*hash<auto> first_match;
foreach hash<auto> row in (some_list) {
    if (row.value > 0) {
        first_match = row;
        break;
    }
}

# Workaround 2: index-zero of find result (still materializes the full match list)
*hash<auto> first_match = (find $1 == "x" in some_list where ($1.value > 0))[0];
```

Both leak — option 1 is verbose and breaks composability; option 2
allocates an entire result list to throw all but the first element away.

The same gap exists for "does any element match?" (`any`), "do all elements
match?" (`all`), "first N elements" (`take`), "skip first N" (`drop`),
"until / while a condition holds." Every reasonably mature collections
library has these; Qore doesn't, except via the hand-rolled
`foreach + break` pattern.

### 1.2 The intermediate-list-materialization gap

Today's nested-operator chains materialize a list at every stage. Even
when the source is a streaming iterator:

```qore
foldl $1 + $2, (map $1 * 2, (select source, $1 > 0))
```

This:

1. Iterates `source` through `select`, builds an intermediate list of
   filtered elements.
2. Iterates that list through `map`, builds another intermediate list of
   doubled elements.
3. Iterates *that* list through `foldl`, computes the sum.

Two intermediate list allocations + two extra passes. The native iterator
fast-path on `bugfix/text_processing` reduced per-stage *dispatch* cost
~2×, but the intermediate-list overhead remains.

### 1.3 Both fixed by one optimizer pass

A compiler pass that recognizes nested `OperatorNode(consumer, [...,
OperatorNode(producer, [..., source])])` patterns and emits a single fused
loop fixes both:

- Streaming operators with short-circuit semantics (`first`, `any`,
  `take 10`) terminate the source iteration as soon as the result is
  decided. No intermediate list. No needless work.
- Pure transformation chains (`select` + `map` + `foldl`) execute as a
  single per-element pass. No intermediate list.

This proposal is **two pieces, both useful independently**:

| Piece | Effort | Contribution |
|---|---|---|
| New keyword operators | ~2 weeks | Closes the find-first gap and the related any/all/take/drop/while/until gaps |
| Operator-chain fusion optimizer | ~4-6 weeks | Eliminates intermediate list allocation in existing chains |

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
any <iterable>                    # bool — True if iterable yields >= 1 element
all <pred-expr>, <iterable>       # bool — True if every element matches
```

```qore
bool any_critical = any $1 =~ /CRITICAL/, log.splitLines();
bool all_positive = all $1 > 0, scores;
bool not_empty    = any source;
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
list<string> first_10_errors = take 10,
                                  (select log.splitLines(), $1 =~ /ERROR/);

# Skip header, process body
foreach hash<auto> row in (drop 1, csv.splitLines()) {
    process(row);
}
```

`take` short-circuits the source after N elements. `drop` skips the first
N then yields the rest.

### 2.4 `while` and `until`

```qore
while <pred-expr>, <iterable>     # iterator yielding elements until pred returns False
until <pred-expr>, <iterable>     # iterator yielding elements until pred returns True
```

Non-terminal, like `take`/`drop`.

```qore
# Lines until the first blank line (e.g., HTTP header end)
list<string> headers = while $1 != "", source.splitLines();

# Read until we hit a sentinel
list<string> body = until $1 == "<<END>>", source.splitLines();
```

`while` reads while the predicate holds; `until` reads until it holds.
Both stop iterating immediately at the boundary — no peek-ahead needed by
the caller.

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

The existing `find <expr> in <list> where (<pred>)` operator returns a
list of all matching elements. It stays unchanged.

This proposal additionally extends it with `first` / `last` / `one`
modifiers:

```qore
# Existing
list<auto> matches = find $1 == "x" in some_list where ($1.value > 0);

# New: find first match (returns *T, not a list)
*hash<auto> match = find first $1 == "x" in some_list where ($1.value > 0);

# New: find last match
*hash<auto> match = find last $1 == "x" in some_list where ($1.value > 0);

# New: find exactly one match (error if multiple)
*hash<auto> match = find one $1 == "x" in some_list where ($1.value > 0);
```

`find first` and `first <pred>, <list>` are equivalent and lower to the
same fused implementation. Two surface forms because:

- `find first ... in ... where (...)` is consistent with the existing
  `find` family — the natural choice when the existing `find` shape
  already fits.
- `first <pred>, <list>` is consistent with `select`/`map`/`foldl` —
  the natural choice in iterator-style pipelines.

Both compile to the same AST node; pick whichever reads better at the
call site. `find last` and `find one` are also new and have no `select`-
style equivalent (they're specifically `find`-family operations).

### 2.7 Argument syntax summary

| Operator | Args | Returns |
|---|---|---|
| `first <pred>, <iterable>` | predicate + source | `*T` |
| `first <iterable>` | source only | `*T` |
| `any <pred>, <iterable>` | predicate + source | `bool` |
| `any <iterable>` | source only | `bool` |
| `all <pred>, <iterable>` | predicate + source | `bool` |
| `take <n>, <iterable>` | int + source | iterator |
| `drop <n>, <iterable>` | int + source | iterator |
| `while <pred>, <iterable>` | predicate + source | iterator |
| `until <pred>, <iterable>` | predicate + source | iterator |
| `count <iterable>` | source only | `int` |
| `count <pred>, <iterable>` | predicate + source | `int` |
| `find first <expr> in <list> where (<pred>)` | (extends existing find) | `*T` |
| `find last <expr> in <list> where (<pred>)` | (extends existing find) | `*T` |
| `find one <expr> in <list> where (<pred>)` | (extends existing find) | `*T` |

In all cases, `<pred>` and `<expr>` are expressions that may use `$1` to
refer to the current element (matching the `select`/`map`/`foldl`
convention).

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
take, drop, while, until}` and `producer` is in the same set or any
iterator-yielding expression. The pattern recurses — chains of arbitrary
depth fuse into a single loop.

### 3.2 What the fused loop looks like

For

```qore
int n = count $1 =~ /ERROR/, (map $1.lwr(), log.splitLines());
```

today's emit is roughly:

```cpp
QoreListNode* tmp1 = log.splitLines();   // intermediate list
QoreListNode* tmp2 = new QoreListNode;   // map output list
for (auto& el : *tmp1) tmp2->push(el.lwr());
int n = 0;
for (auto& el : *tmp2) if (el =~ /ERROR/) ++n;
```

Two list allocations, three passes over the data.

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
- **Short-circuit semantics** (`first`, `any`, `take N`, `while`, `until`)
  emit `break` from the fused loop at the boundary. Non-fused fallback
  uses iterator `.next()` returning false.
- **Reference holding.** The source expression is evaluated once; the
  fused loop holds a ref via existing iterator lifetime rules.
- **Mixed sources.** If a stage's source is not a fuseable form (e.g., a
  reflective access, a user-defined iterator class with custom
  semantics), that stage falls back to the non-fused emit. The rest of
  the chain still fuses around it.

### 3.4 Non-fused fallback

If fusion can't apply (custom iterator class, reflection, optimizer
disabled), the chain emits as today — nested operator nodes that build
intermediate lists. Programs run correctly either way; fusion is a
performance optimization, not a correctness requirement.

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

- New operator keywords (`first`, `any`, `all`, `take`, `drop`, `while`,
  `until`, `count`) become reserved words. Any existing variable named
  e.g. `count` becomes a parse error. Survey of qlib needed before
  release; common collisions (`count` is the obvious one) remediated with
  rename.
- Existing operators (`find`, `select`, `map`, `foldl`, `foreach`)
  unchanged.
- `find first` / `find last` / `find one` — extensions to existing `find`
  syntax. Doesn't break anything because `first`/`last`/`one` after
  `find` was a parse error before.

### 4.2 Behavior compatibility

Fusion is observationally equivalent to the non-fused form *for pure
expressions*. Programs with side effects in stage closures should observe
the same ordering — fusion executes stages in source order per element,
which matches the nested form's left-to-right execution per element.

### 4.3 Reserved word survey

A grep before release: any qlib symbol named `first`, `any`, `all`,
`take`, `drop`, `while` (already a keyword), `until`, or `count` that is
a function or identifier rather than a keyword usage. `while` is already
reserved as a control flow keyword — no conflict. `count` is the most
likely user-named function (we have one in `<list>::size()` aliases and a
few module-internal helpers). The parser uses context to disambiguate
— `count <expr>` at expression position is the operator; `count(...)` as a
function call is unambiguous.

### 4.4 Documentation

`doxygen/lang/operators.dox.h` (or whatever the canonical operators page
is) gets entries for the new operators. The migration guide for 2.3
mentions the new reserved words.

---

## 5. Performance projections

Numbers extrapolated from the `bugfix/text_processing` branch's measured
fusion-style wins (the lazy iterator + native fast-path commits already
gave us a baseline).

### 5.1 Multi-stage chains (fusion)

| Workload | 2.3 today (post-branch) | 2.3 + fusion |
|---|---|---|
| `foldl X, (map Y, src)` over 100K-element list | ~150 ms | ~50 ms |
| `count P, (select Q, (map R, src))` over 100K | ~200 ms | ~40 ms |
| Real qlib `MysqlSqlUtil` DDL emit (250 cols) | (negligible already) | (negligible) |
| Synthetic 4-stage chain over 1M elements | ~1.2 s | ~0.3 s |

The real qlib chain shapes (~2 stages, small input) gain less in absolute
terms because the chain is short and cheap. The win is most visible in
synthetic / future-code patterns with longer chains over larger inputs.

### 5.2 Short-circuit operators

| Workload | Today | With new operators |
|---|---|---|
| Find first ERROR in 50K-line log | 9095 ms (full `find` materializes) or `foreach + break` (manual) | ~5 ms (`first` short-circuits at line 1 if hit) |
| `any =~ /CRITICAL/` over 50K-line log | ~50 ms (using `select` + `size() > 0`) | ~5 ms (`any` short-circuits) |
| `take 10` matching lines from a stream | manual counter pattern | one-line operator |

The `find first` case in particular goes from full O(N) to O(position-of-
first-match) because the new operator stops iterating immediately on hit.

### 5.3 Existing chains (zero migration)

Every existing `foldl(map(...))` chain in qlib speeds up automatically
once fusion ships. Specific calls in the qlib survey from earlier
analysis:

- `qlib/MysqlSqlUtil.qm` DDL emit: ~10% faster (small absolute, many
  callers)
- `qlib/OpenApi3.qm` schema header concat: ~30% faster
- `qlib/RestHandler.qm` subclass enumeration: ~20% faster

These are micro-improvements in absolute terms but **zero risk and zero
migration**.

---

## 6. Effort estimate

| Component | Estimate |
|---|---|
| Lexer changes (new reserved words) | 2 days |
| Parser changes (productions for first/any/all/take/drop/while/until/count) | 1 week |
| Parser changes for `find first`/`last`/`one` | 3 days |
| AST nodes + non-fused emit for each operator | 2 weeks |
| Fusion optimizer pass | 4-6 weeks |
| Test coverage (qtest for each operator + property tests for fusion equivalence) | 2-3 weeks |
| Doc updates | 1 week |
| **Total** | **~10-13 weeks** |

Order:

1. Operators + non-fused emit (~3 weeks). Releasable here — the operators
   work, the fusion just isn't applied yet, programs are no slower than
   before.
2. Fusion optimizer (~5 weeks).
3. Find-first/last/one extensions (~3 days, can land any time).

The operators alone are independently shippable in case the fusion pass
slips. Pre-fusion, a multi-stage chain still allocates intermediate
lists, but the new operators close the find-first gap, which is the
ergonomic win.

---

## 7. Risks and tradeoffs

### 7.1 Reserved-word collisions

`count` is the most likely collision in user code. Mitigation: a survey
before release catches all uses; the few that need rename can be migrated
with `qore-migrate-rename` (a 50-line tool). Other words (`first`, `any`,
`all`, `take`, `drop`, `until`) are less common but still possible.

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

### 7.3 Two-form duplication: `first` vs `find first`

Both `first <pred>, <list>` and `find first ... in ... where (...)` exist
and do the same thing. This is "two ways to do it" — usually a smell.
Mitigations:

- The forms are surface syntax for the same fused IR. No runtime
  duplication.
- The two forms target different idioms (operator-chain style vs
  context-row style). Programmers writing in one style don't have to
  learn the other.
- `find first/last/one` extends an existing operator family the user
  already knows; not adding it would force users into the standalone
  `first`/`last`/(no `one`) for what they already think of as a `find`.

### 7.4 Operator complexity creep

Eight new operators is a non-trivial addition to the language surface.
Mitigations:

- Each operator is small (~100-200 LOC including AST node, type
  checking, and fusion stage descriptor).
- Each one has a clear, common idiom — none of them is speculative or
  niche.
- They share a common AST shape (predicate + source for most), so the
  parser productions are mostly templated.

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
   to compose. Recommendation: iterator — callers who need a list say
   `list<int> l = take 10, ...;` and the implicit materialization
   happens.

3. **`count` of an infinite source — error or hang?**
   No infinite sources in Qore today (`xrange` requires explicit bounds).
   If a user-defined iterator turns out to be infinite, `count` would
   hang. Could add a `count limit N` form to bound it. Probably YAGNI;
   defer.

4. **Should `find first` be the *only* "find first" form?**
   The alternative is `first $1 == "x", (select some_list, $1.value > 0)`
   — verbose but uses only the new operators. Recommendation: ship both;
   `find first ... in ... where (...)` is closer to existing `find` users
   and worth the small parser cost.

5. **Lazy iterator interaction with stages that need a `size`.**
   Example: `take size(src) - 1, src` to drop the last element. `size()`
   on an iterator forces full materialization. Probably leave the
   `init/last` element gymnastics to library helpers; the operator set
   here covers the common cases.

---

## 9. References

- [`char-type.md`](char-type.md) — the strong companion proposal (`char` value
  type)
- [`iter-pipe-operator.md`](iter-pipe-operator.md) — separate, deferred
  proposal for `|>` pipe sugar that would lower to the same fused IR as
  this proposal
- `bugfix/text_processing` commits `8165b217b`–`bb1ea2d96` — the qlib
  optimizations that motivated this work
- `lib/parser.ypp` — operator grammar
- `include/qore/intern/AbstractIteratorHelper.h` — existing iterator
  fast-path the fused emit builds on
