# `iter` and `|>` — Lazy Iteration Pipe Operator

**Status:** Design. **Recommendation: defer.** This document captures the
proposal and the analysis behind the recommendation; the conclusion at the
end is that the cheaper alternative (fusion of existing nested-operator
syntax + a small set of new keyword operators) covers most of the benefit
without introducing parallel syntax.

**Target if accepted:** Qore 2.3 alongside [`char`](char-type.md), or a
later release as a pure additive feature.

**Companion:** [`char-type.md`](char-type.md) — independent, recommended
for 2.3.

---

## 1. The problem

Qore today composes lazy/functional operators by nesting:

```qore
foldl $1 + ", " + $2, (map process($1), source)
```

The composition is *partly* lazy: `source` (typically an iterator object
inheriting `AbstractIterator`) feeds into `map` element-by-element via
`AbstractIteratorHelper`, but **`map` materializes a list** as its return
value. `foldl` then iterates that list. So a chain
`foldl … (select … (map … source))` allocates two intermediate lists
even when the source is a true streaming iterator.

The native iterator fast-path landed on `bugfix/text_processing` cuts the
per-stage *dispatch* cost ~2× but does nothing about the intermediate
allocations. Per-stage list materialization remains the dominant cost in
multi-stage chains.

Two complementary fixes are possible:

- **Fusion of the existing nested-operator AST.** A compiler pass walks
  `OperatorNode(foldl, [..., OperatorNode(map, [..., src])])` patterns and
  emits a single fused C++ loop, eliminating the intermediate list. **Zero
  source-level migration needed; every existing program speeds up.** This
  is the high-value pass — useful regardless of whether we add pipe syntax.
- **Pipe-style chain syntax** (`iter source |> map ... |> reduce ...`) as
  an alternative way to *write* what fusion lowers to. The chain reads
  left-to-right (source-first) instead of right-to-left (innermost-first).
  Purely syntactic; the optimizer treats both forms the same.

This document focuses on the second piece — the pipe syntax — and the
honest analysis of when it earns its keep.

---

## 2. The proposal

### 2.1 Syntax

```qore
iter <iterable-expression>
    |> filter <pred>            # boolean test on each element
    |> map    <expr>            # transform each element
    |> take   <n>               # bounded prefix
    |> drop   <n>               # skip prefix
    |> while  <pred>            # take while pred holds
    |> until  <pred>            # take while !pred holds
    |> reduce <init>, <expr>    # foldl with explicit init
    |> count                    # count elements
    |> count  <pred>            # count elements matching pred (shortcut for filter |> count)
    |> first                    # first element (or NOTHING)
    |> first  <pred>            # first element matching pred (shortcut)
    |> any                      # True if at least one element
    |> any    <pred>            # True if any element matches pred (shortcut)
    |> all    <pred>            # True if every element matches pred
    |> collect                  # materialise as list (only at end of chain)
    |> foreach <body>           # consume with a statement body
```

`iter` is a unary prefix operator. `|>` is left-associative infix; the
right operand is one of the named stages. The whole chain is parsed as a
single AST node (`IterChain` with `source` and a list of `Stage` records).

### 2.2 Element-type protocol

`iter X` yields elements per `X`'s type:

| `X` is | element type |
|---|---|
| `string` | `char` (a Unicode codepoint — see [char-type.md](char-type.md)) |
| `binary` | `int` (raw byte, 0..255) |
| `list<T>` | `T` |
| `hash<K, V>` | `hash<{key: K, value: V}>` (key-value pair) |
| any object exposing `<class>::iterator()` | element type from that iterator |
| `int N` (positive) | `int` (range `0..N-1`) |
| `range<int>` | `int` |

Hash iteration yields a pair (matching today's `pairIterator()` precedent).
Pair-style is more useful inside stages because both key and value are
typically needed; `keyIterator()` and `valueIterator()` (= today's
`<hash>::iterator()`) stay as factory methods for the niche cases.

### 2.3 Element-type protocol notes

- For strings: yielded `char` values are pure codepoints with no encoding
  attached (see [char-type.md §3.5](char-type.md)). `iter "héllo" |> ...`
  produces codepoints regardless of the source string's encoding. To
  re-encode at the end, `|> map c.toString("UTF-16LE") |> ...`.
- For binary: bytes (`int`, 0..255), not characters. Use
  `iter bin |> map (b -> ...)` for byte-level processing.
- For objects: the iterator factory call (`<class>::iterator()`) is part of
  `iter X`; programmer doesn't write it.

---

## 3. When pipe syntax helps and when it doesn't

This is the central question. The honest answer based on real qlib code:

### 3.1 1-stage chains — pipe is worse

```qore
# Today
list<auto> evens = select source, $1 % 2 == 0;
int total       = foldl $1 + $2, source;

# Pipe
list<auto> evens = iter source |> filter $1 % 2 == 0 |> collect;
int total       = iter source |> reduce 0, $1 + $2;
```

More verbose, no gain. **Lose.**

### 3.2 2-stage chains — wash

Real qlib examples:

```qore
# qlib/MysqlSqlUtil.qm
sql += foldl $1 + ",\n" + $2, (map "  " + $1.getCreateSql(self), columns.iterator());

# Pipe equivalent
sql += iter columns
            |> map "  " + $1.getCreateSql(self)
            |> reduce "", $1 + ",\n" + $2;
```

Roughly equal. The existing one-line form fits the screen; pipe needs a
multi-line layout to read better. **Wash.**

The qlib codebase has ~20 occurrences of `foldl(map(...))` patterns at
2 stages — the dominant existing chain shape. Pipe doesn't help these.

### 3.3 3-stage chains — pipe slightly ahead

Hypothetical (qlib has very few of these, partly because they're awkward):

```qore
# Count distinct lowercase 4+ char words
# Today (right-to-left mental order)
int n = elements (foldl $1 + 1, 0,
                    (select
                        (map $1.lwr(),
                            (select text.splitLinesRegex("\\s+"), $1.size() > 3))
                    ));

# Pipe (left-to-right matches execution order)
int n = iter text
          |> splitLinesRegex "\\s+"
          |> filter $1.size() > 3
          |> map    $1.lwr()
          |> count;
```

Pipe is ~30 % clearer; the reader doesn't have to mentally invert order.
**Slight win for pipe.**

### 3.4 Short-circuit terminals — pipe wins (but new operators win independently)

The case where pipe is genuinely cleaner:

```qore
# Find first line containing ERROR
# Today (no nested-operator equivalent; must use foreach + break)
*string first_error;
foreach string line in (log.splitLines()) {
    if (line =~ /ERROR/) {
        first_error = line;
        break;
    }
}

# Pipe
*string first_error = iter log |> splitLines |> first $1 =~ /ERROR/;
```

```qore
# Take first 10 errors
# Today: counter + foreach + break
list<string> errs = ();
int n = 0;
foreach string line in (log.splitLines()) {
    if (line =~ /ERROR/) {
        errs += line;
        if (++n == 10) break;
    }
}

# Pipe
list<string> errs = iter log |> splitLines
                             |> filter $1 =~ /ERROR/
                             |> take 10
                             |> collect;
```

Five+ lines collapse to one expression. **Pipe is materially better here.**

But — and this matters — **the same gain comes from adding `take` /
`first` / `any` / `all` as ordinary keyword operators** in the existing
nested-form syntax:

```qore
*string first_error = first $1 =~ /ERROR/, log.splitLines();
list<string> errs   = take 10, (select log.splitLines(), $1 =~ /ERROR/);
bool any_critical   = any $1 =~ /CRITICAL/, log.splitLines();
```

So **80 % of the short-circuit win is the new operators, not the pipe
syntax.** Add `first`/`any`/`all`/`take`/`drop`/`while`/`until`/`count` as
ordinary keyword operators in the existing nested form and the
`foreach`-plus-`break` pattern goes away whether or not we ship pipe.

### 3.5 4+ stage chains — pipe meaningfully better

Five+ nested `(...)` levels in nested-operator form is genuinely hard to
read. Speculative example:

```qore
# Process CSV: skip comments, parse, dedupe by first column, take first 100
# Today: requires intermediate variables for sanity
list<auto> rows = csv.splitLines();
list<auto> data_rows = select rows, $1 !~ /^#/;
list<auto> parsed = map $1.split(","), data_rows;
hash<auto> seen = {};
foreach list<auto> row in (parsed) {
    seen{row[0]} = row;
}
list<auto> sample = take 100, seen.values();

# Pipe
list<auto> sample = iter csv
                       |> splitLines
                       |> filter $1 !~ /^#/
                       |> map    $1.split(",")
                       |> dedupe-by $1[0]
                       |> take 100
                       |> collect;
```

The pipe form is genuinely clearer. But a survey of qlib finds **zero**
existing chains at this depth. The argument is "if you build it they will
come" — programmers might write more elaborate chains *because* pipe makes
them readable. Real but speculative.

### 3.6 Distribution in real code

From a qlib grep:

| Chain depth | Count | Pipe verdict |
|---|---|---|
| 1 stage (`foldl X, src` / `select src, P` / `map E, src`) | ~hundreds | pipe loses |
| 2 stages (`foldl X, (map Y, src)`) | ~20 | pipe is a wash |
| 3 stages | ~3 | pipe slightly ahead |
| 4+ stages | 0 | pipe wins (but no existing code) |

For *current* qlib, pipe helps essentially no one. The case for pipe rests
on hypothetical future code.

---

## 4. The alternative — fusion + new operators in the existing syntax

Both performance and ergonomic wins are reachable without introducing pipe
syntax:

### 4.1 Fusion of nested operators

A new optimizer pass walks the AST after type checking, looking for the
pattern:

```
OperatorNode(consumer, [..., OperatorNode(producer, [..., source])])
```

where `consumer` ∈ `{foldl, foreach, map, select, ...}` and `producer` ∈
the same set or any iterator-yielding expression. The pass rewrites the
nested operators into a single `IterFusedLoopNode` with stages inlined.

The non-fused fallback is the current implementation (consumer iterates
producer's materialized list). Fusion is an *optimization*, not a semantic
requirement.

Wins:

- **Every existing nested chain in qlib speeds up.** Zero source migration.
- The intermediate-list allocation goes away.
- Fusion engineering effort is the same as it would be for pipe — the
  fused-loop IR doesn't care which surface syntax produced it.

### 4.2 New keyword operators

Add the streaming operators that don't exist in Qore today as ordinary
keyword forms:

```qore
take N, src                # first N elements
drop N, src                # skip first N elements
while EXPR, src            # take while EXPR true (per-element via $1)
until EXPR, src            # take while EXPR false
count src                  # count elements
count EXPR, src            # count elements matching EXPR
first src                  # first element (or NOTHING)
first EXPR, src            # first element matching EXPR
any   EXPR, src            # True if any element matches
all   EXPR, src            # True if every element matches
```

These compose with existing operators via nesting and participate in
fusion automatically. Effort: ~2 weeks for all of them combined (each is
~50 LOC of operator implementation).

### 4.3 Combined — what we get without pipe syntax

```qore
# Take first 10 ERROR lines
list<string> errs = take 10, (select log.splitLines(), $1 =~ /ERROR/);

# Find first match
*string first_error = first $1 =~ /ERROR/, log.splitLines();

# Count letters in a string
int n = count ($1.isLetter()), str.codePointIterator();

# Process and dedupe
list<auto> sample = take 100,
                       (select_unique
                           (map $1.split(","),
                               (select csv.splitLines(), $1 !~ /^#/)),
                           $1[0]);
```

The 4-stage example is uglier than the pipe form, but qlib doesn't have
chains at that depth today. **The 1-, 2-, and 3-stage cases — which are
all we observe — are equivalent or better in nested form.**

---

## 5. Recommendation

**Skip pipe syntax in 2.3. Ship:**

1. The fusion optimizer for existing nested operators. (~4-6 weeks.)
2. The new keyword operators (`take`, `drop`, `while`, `until`, `count`,
   `first`, `any`, `all`). (~2 weeks.)

These two together capture the perf wins and the short-circuit ergonomic
wins without introducing parallel syntax. Every existing program benefits.

**Revisit pipe in a later release** if usage patterns evolve toward 4+
stage chains. The fusion infrastructure ships first; pipe sugar can be
added on top later as a pure parser extension that lowers to the same
fused IR — additive, no compatibility cost, no semantic change.

This is the conservative path. The user-visible delta is smaller (no new
operator to learn) and the implementation risk is lower (no
double-syntax maintenance burden).

If we revisit and decide to add pipe later:

- The fusion optimizer doesn't change — pipe lowers at parse time to the
  same nested AST that already fuses.
- The keyword operators don't change — pipe stages refer to the same
  named operators.
- The grammar change is small (new `|>` operator, `iter` keyword).

Cost of deferring: programmers writing 4+ stage chains today (zero today,
maybe some tomorrow) lack the cleanest syntax for them. They can use
intermediate variables, or wait.

---

## 6. If pipe is accepted anyway — design notes

For completeness, in case the recommendation is overridden:

### 6.1 Method-style alternative (rejected)

`str.iter().filter(...).map(...).count()` — familiar to JS/Rust users.
Rejected because:

- Each stage is a method call; the optimizer must special-case "if I see
  `.filter(...).map(...).count()` on a known iterator type, fuse them".
  Virtual dispatch on the iterator interface means user-overridden methods
  bypass the fast path silently.
- Adding 6+ iterator-stage classes (`FilterIterator`, `MapIterator`, …)
  per element type combination is significant library footprint.
- Closure-as-arg syntax is awkward — `.filter({|c| c.isLetter()})`,
  `.filter(\foo)`, or a new lambda form.
- Conflict risk: anyone with a `.filter()` method on a custom iterator
  class who wasn't expecting fusion semantics gets surprising behaviour.

### 6.2 Pipe-style design (described in §2)

Pros:

- Single AST node, parse-time fusion.
- No virtual dispatch ambiguity.
- Tiny runtime footprint (stages are AST kinds, not classes).
- Natural inline closure form (`filter c -> body`, `map c -> body`, etc.).

Cons:

- Less familiar than method chains.
- No autocomplete after `|>`.
- No user extension of stages.

### 6.3 Closure inline form

Stages that take a per-element expression (`filter`, `map`, `while`,
`until`, `reduce`, `count` (with predicate), `first` (with predicate),
`any`, `all`) accept either:

- A bare expression using `$1` (matching the existing `map`/`select`
  convention), or
- An arrow-style closure: `c -> c.isLetter()`. Names the parameter for
  readability in long chains.

Both forms parse to the same internal closure node. The arrow form is
optional sugar.

### 6.4 Stage list

Initial set:

| Stage | Type | Notes |
|---|---|---|
| `filter` | non-terminal, 1 expr arg | drops elements where expr is false |
| `map` | non-terminal, 1 expr arg | transforms each element |
| `take` | non-terminal, 1 int arg | yields first N |
| `drop` | non-terminal, 1 int arg | skips first N |
| `while` | non-terminal, 1 expr arg | takes while pred holds |
| `until` | non-terminal, 1 expr arg | takes while pred is false |
| `reduce` | terminal, 1 init + 1 expr | foldl with explicit init |
| `count` | terminal, 0 or 1 expr arg | count (filtered) elements |
| `first` | terminal, 0 or 1 expr arg | first element (matching pred if given) |
| `any` | terminal, 0 or 1 expr arg | True if any matches |
| `all` | terminal, 1 expr arg | True if all match |
| `collect` | terminal, 0 args | materialise as list |
| `foreach` | terminal, 1 statement-body | consume with side effects |

Stages can be added later — adding a stage is a parser change plus an
optimizer-pass entry, no runtime class needed.

### 6.5 Compiler implementation

The fusion optimizer (`lib/IterChainOptimizer.cpp`) recognizes
`IterChain` AST nodes and lowers them to `IterFusedLoopNode`. Same node
the nested-operator fusion produces. Single shared lowering path for
both surface syntaxes.

### 6.6 Effort if accepted

| Component | Estimate |
|---|---|
| Lexer changes (`iter`/`\|>` tokens) | 1 day |
| Parser changes (iter_chain grammar with stage productions) | 5-7 days |
| Closure inline form (`c -> body`) — if not in language already | 3-4 days |
| Documentation + migration guide | 4 days |
| Test coverage | 1-2 weeks |
| **Total (in addition to fusion + new operators which are needed regardless)** | **~3-4 weeks** |

---

## 7. References

- [`char-type.md`](char-type.md) — companion proposal (independent, recommended for 2.3)
- `bugfix/text_processing` commits `8165b217b`–`bb1ea2d96` — the qlib
  byte-indexed migrations
- `lib/parser.ypp`, `lib/scanner.lpp` — parser/lexer touch points
- `include/qore/intern/AbstractIteratorHelper.h` — existing native fast-path
  the fusion optimizer builds on
