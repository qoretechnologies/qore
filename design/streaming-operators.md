# Streaming Operators (`first` / `any` / `all` / `take` / `drop` / `takewhile` / `takeuntil` / `count`) + Operator-Chain Fusion + `iterate` keyword

**Status:** Design.

**Target:** Qore 2.3 alongside [`char-type.md`](char-type.md).

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

### 2.4 `takewhile` and `takeuntil`

```qore
takewhile <pred-expr>, <iterable>     # iterator yielding elements until pred returns False
takeuntil <pred-expr>, <iterable>     # iterator yielding elements until pred returns True
```

Non-terminal, like `take`/`drop`.

```qore
# Lines until the first blank line (e.g., HTTP header end)
list<string> headers = takewhile $1 != "", source.splitLines();

# Read until we hit a sentinel
list<string> body = takeuntil $1 == "<<END>>", source.splitLines();
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

# New: find exactly one match
# - 0 matches → NOTHING
# - 1 match  → the element
# - 2+ matches → throws MULTIPLE-MATCHES-ERROR with the offending count
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
| `any <iterable>` | source only | `bool` |
| `all <pred>, <iterable>` | predicate + source | `bool` |
| `take <n>, <iterable>` | int + source | iterator |
| `drop <n>, <iterable>` | int + source | iterator |
| `takewhile <pred>, <iterable>` | predicate + source | iterator |
| `takeuntil <pred>, <iterable>` | predicate + source | iterator |
| `count <iterable>` | source only | `int` |
| `count <pred>, <iterable>` | predicate + source | `int` |
| `find first <expr> in <list> where (<pred>)` | (extends existing find) | `*T` |
| `find last <expr> in <list> where (<pred>)` | (extends existing find) | `*T` |
| `find one <expr> in <list> where (<pred>)` | (extends existing find) | `*T` |

In all cases, `<pred>` and `<expr>` are expressions that may use `$1` to
refer to the current element (matching the `select`/`map`/`foldl`
convention).

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

Fusion is observationally equivalent to the non-fused form *for pure
expressions*. Programs with side effects in stage closures should observe
the same ordering — fusion executes stages in source order per element,
which matches the nested form's left-to-right execution per element.

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
expression position." Code that has `any` as a variable inside an
expression (e.g., the right-hand side of an assignment) needs care.
Mitigation: `qore-migrate-rename` can rewrite identifier uses to a
non-keyword name; the survey before release flags how many sites need
this. If the conflict surface in qlib is too large, we fall back to
the alternative names below.

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

### 4.4 Documentation

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

| Workload | 2.3 today (post-branch) | 2.3 + fusion (estimated) |
|---|---|---|
| `foldl X, (map Y, src)` over 100K-element list | ~150 ms | ~50 ms |
| `count P, (select Q, (map R, src))` over 100K | ~200 ms | ~40 ms |
| Synthetic 4-stage chain over 1M elements | ~1.2 s | ~0.3 s |

Real qlib chain shapes (~2 stages over small input) gain less in
absolute terms because the chain is short and cheap. The win is most
visible in synthetic / future-code patterns with longer chains over
larger inputs.

### 5.2 Short-circuit operators

| Workload | Today | With new operators |
|---|---|---|
| Find first ERROR in 50K-line log | 9095 ms (full `find` materializes) or `foreach + break` (manual) | ~5 ms (`first` short-circuits at line 1 if hit) |
| `any =~ /CRITICAL/` over 50K-line log | ~50 ms (using `select` + `size() > 0`) | ~5 ms (`any` short-circuits) |
| `take 10` matching lines from a stream | manual counter pattern | one-line operator |

The `find first` case in particular goes from full O(N) to O(position-of-
first-match) because the new operator stops iterating immediately on hit.

### 5.3 Existing chains (zero migration) — estimated

Every existing `foldl(map(...))` chain in qlib speeds up automatically
once fusion ships. The specific qlib chain shapes are short (2 stages)
over small input (typically <1 KB column lists, schema headers, etc.),
so the **absolute** gains are small even when the **relative** gain is
significant. We have not benched these directly; estimating ~10–30 %
faster on the 2-stage qlib chains, but the win is principally in the
**zero-source-migration** property — no code review, no migration
testing, every program improves.

---

## 6. Effort estimate

| Component | Estimate (sequential) |
|---|---|
| Lexer changes (new reserved words: `first`, `any`, `all`, `take`, `drop`, `takewhile`, `takeuntil`, `count`, `iterate`) | 2 days |
| Parser changes (productions for the 8 keyword operators) | 1 week |
| Parser changes for `find first`/`last`/`one` | 3 days |
| Parser + runtime support for `iterate` (uniform iterator factory) | 4 days |
| AST nodes + non-fused emit for each operator | 2 weeks |
| Fusion optimizer pass | 4-6 weeks |
| Test coverage (qtest per operator + property tests for fusion equivalence) | 2-3 weeks |
| Reserved-word survey + `qore-migrate-rename` tool | 4 days |
| Doc updates | 1 week |
| **Total (sequential)** | **~14-17 weeks** |
| **Total (with parallelism, 2 developers)** | **~10-12 weeks** |

Order:

1. Operators + non-fused emit + `iterate` keyword (~4 weeks). Releasable
   here — the operators work, fusion just isn't applied yet, programs
   are no slower than before. **The find-first ergonomic win is fully
   realized at this milestone**, even without fusion.
2. Fusion optimizer (~4-6 weeks). Adds the perf-on-existing-chains win.
3. `find first`/`last`/`one` extensions (~3 days, can land any time
   after step 1).

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
- [`pipe-operator.md`](pipe-operator.md) — separate, deferred
  proposal for `|>` pipe sugar that would lower to the same fused IR as
  this proposal
- `bugfix/text_processing` commits `8165b217b`–`bb1ea2d96` — the qlib
  optimizations that motivated this work
- `lib/parser.ypp` — operator grammar
- `include/qore/intern/AbstractIteratorHelper.h` — existing iterator
  fast-path the fused emit builds on
