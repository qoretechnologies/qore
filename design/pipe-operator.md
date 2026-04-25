# `|>` Pipe Operator — Lazy Chain Sugar

**Status:** Design. **Recommendation: defer to a release after 2.3.**
This document captures the proposal so we have it on file; the
substantive wins are delivered by [`streaming-operators.md`](streaming-operators.md),
and pipe syntax is pure surface sugar that can be added later additively
without breaking compatibility.

**Target if accepted:** post-2.3. Pipe lowers at parse time to the same
fused IR that streaming-operators.md already targets.

**Companions:**
- [`char-type.md`](char-type.md) — recommended for 2.3 (`char` value type)
- [`streaming-operators.md`](streaming-operators.md) — recommended for 2.3
  (the `iterate` keyword + the operators + fusion this sugar would chain)

---

## 1. What this proposal adds

Once [`streaming-operators.md`](streaming-operators.md) ships, every chain
of new keyword operators (`first`, `any`, `all`, `take`, `drop`,
`takewhile`, `takeuntil`, `count`) plus the existing ones (`select`,
`map`, `foldl`, `foreach`) auto-fuses into a single loop. The
functionality is fully present; the `iterate` keyword is the uniform
iterator entry point.

What pipe syntax adds is a **left-to-right, source-first chain syntax**
that desugars at parse time into the same nested AST:

```qore
# Without pipe (existing keyword-operator syntax, with new operators)
int letters = count $1.isLetter(), iterate str;

list<string> errs = take 10, (select log.splitLines(), $1 =~ /ERROR/);

# With pipe (proposed sugar)
int letters = iterate str
                |> filter $1.isLetter()
                |> count;

list<string> errs = iterate log
                       |> splitLines
                       |> filter $1 =~ /ERROR/
                       |> take 10
                       |> collect;
```

Pipe is **purely a surface choice** — the runtime sees the same fused
loop either way. The question this document addresses is: is the new
syntax worth the cognitive load of having two ways to write the same
chain?

---

## 2. The honest answer

### 2.1 1-stage chains — pipe is worse

```qore
# Existing
list<auto> evens = select source, $1 % 2 == 0;
int total       = foldl $1 + $2, source;

# Pipe
list<auto> evens = iterate source |> filter $1 % 2 == 0 |> collect;
int total       = iterate source |> reduce 0, $1 + $2;
```

More verbose, no gain. **Lose.**

### 2.2 2-stage chains — wash

Real qlib examples:

```qore
# qlib/MysqlSqlUtil.qm
sql += foldl $1 + ",\n" + $2, (map "  " + $1.getCreateSql(self), columns.iterator());

# Pipe equivalent
sql += iterate columns
            |> map "  " + $1.getCreateSql(self)
            |> reduce "", $1 + ",\n" + $2;
```

Roughly equal. The existing one-line form fits the screen; pipe needs a
multi-line layout to read better. **Wash.**

The qlib codebase has ~20 occurrences of `foldl(map(...))` at 2 stages —
the dominant existing chain shape. Pipe doesn't help these.

### 2.3 3-stage chains — pipe slightly ahead

```qore
# Today (right-to-left mental order)
int n = elements (foldl $1 + 1, 0,
                    (select
                        (map $1.lwr(),
                            (select text.splitLinesRegex("\\s+"), $1.size() > 3))
                    ));

# Pipe (left-to-right matches execution order)
int n = iterate text
          |> splitLinesRegex "\\s+"
          |> filter $1.size() > 3
          |> map    $1.lwr()
          |> count;
```

Pipe is ~30 % clearer because the reader doesn't have to mentally
invert order. **Slight win for pipe.**

The equivalent using new keyword operators from
[`streaming-operators.md`](streaming-operators.md) is comparable in
character count if a multi-line layout is allowed:

```qore
int n = count
            $1.size() > 3 && $1 =~ /^[a-z]+$/,
            (map $1.lwr(), text.splitLinesRegex("\\s+"));
```

Roughly the same effort to write either; pipe still slightly clearer.

### 2.4 4+ stage chains — pipe meaningfully better

```qore
# Process CSV: skip comments, parse, dedupe by first column, take first 100
# Today: requires intermediate variables for sanity (no nested form is readable)
list<auto> rows = csv.splitLines();
list<auto> data_rows = select rows, $1 !~ /^#/;
list<auto> parsed = map $1.split(","), data_rows;
hash<auto> seen = {};
foreach list<auto> row in (parsed) {
    seen{row[0]} = row;
}
list<auto> sample = take 100, seen.values();

# Pipe
list<auto> sample = iterate csv
                       |> splitLines
                       |> filter $1 !~ /^#/
                       |> map    $1.split(",")
                       |> dedupe-by $1[0]
                       |> take 100
                       |> collect;
```

The pipe form is genuinely clearer. But a survey of qlib finds **zero**
existing chains at this depth. The argument is "if you build it they
will come" — programmers might write more elaborate chains *because*
pipe makes them readable. Real but speculative.

### 2.5 Distribution in real code

| Chain depth | qlib count | Pipe verdict |
|---|---|---|
| 1 stage (`foldl X, src` / `select src, P` / `map E, src`) | ~hundreds | pipe loses |
| 2 stages (`foldl X, (map Y, src)`) | ~20 | wash |
| 3 stages | ~3 | pipe slightly ahead |
| 4+ stages | 0 | pipe wins (but no existing code) |

For *current* qlib, pipe helps essentially no one. The case for pipe
rests on hypothetical future code.

---

## 3. Recommendation: defer

The substantive wins (perf, find-first ergonomics, no-foreach-break
patterns) are delivered by
[`streaming-operators.md`](streaming-operators.md):

- Fusion makes existing chains as fast as fused chains, **without
  source migration**.
- New keyword operators close the foreach-break gap in nested-form
  syntax.
- `find first`/`last`/`one` extend the existing `find` family.
- The `iterate` keyword unifies iterator entry points (replacing the
  per-type factory methods).

Pipe sugar adds:

- Left-to-right reading for chains the user happens to write at 4+
  stages (zero today).
- A second way to write what already works in nested form.

The maintenance cost of two parallel syntaxes is permanent; the user
benefit is speculative. **Don't ship pipe in 2.3.**

If usage patterns evolve toward longer chains in a future release —
for example, if data-pipeline modules become common in Qore code —
revisit. The fusion infrastructure already exists at that point; pipe
becomes a pure parser change that lowers to the same fused IR.

---

## 4. If pipe is accepted anyway — design notes

For completeness.

### 4.1 Syntax

```qore
iterate <iterable-expression>
    |> filter <pred>
    |> map    <expr>
    |> take   <n>
    |> drop   <n>
    |> takewhile <pred>
    |> takeuntil <pred>
    |> reduce <init>, <expr>
    |> count                    # or: count <pred>
    |> first                    # or: first <pred>
    |> any                      # or: any <pred>
    |> all    <pred>
    |> collect
    |> foreach <body>
```

`|>` is left-associative infix; the right operand is one of the named
stages. The whole chain parses as a single AST node (`IterChain` with
`source` and a list of `Stage` records) and lowers to the same
fused-loop IR that nested-form chains lower to.

The `iterate` keyword and the per-type element protocol come from
[`streaming-operators.md`](streaming-operators.md) — pipe just consumes
what `iterate` produces. (`iterate` is part of the recommended-2.3 proposal,
not unique to pipe.)

### 4.2 Method-style alternative (rejected)

`str.iterate().filter(...).map(...).count()` — familiar to JS/Rust users.
Rejected on optimization grounds:

- The optimizer must special-case "if I see `.filter(...).map(...).count()`
  on a known iterator type, fuse them". Virtual dispatch on the
  iterator interface means user-overridden methods bypass the fast path
  silently.
- Conflict risk: anyone with a `.filter()` method on a custom iterator
  class who wasn't expecting fusion semantics gets surprising
  behaviour.

A library-footprint argument is sometimes made (one stage class per
combination of element type and stage), but generic stage classes
parameterized by element type are how every method-style language
handles this — it's not the dominant concern.

Pipe-style avoids both issues — single AST node visible to the
optimizer, no virtual dispatch, no user-extension surface to clash
with built-in stages.

### 4.3 Closure inline form

Stages that take a per-element expression accept either:

- A bare expression using `$1` (matching the existing `map`/`select`
  convention), or
- An arrow-style closure: `c -> c.isLetter()`. Names the parameter for
  readability in long chains.

Both forms parse to the same internal closure node. The arrow form is
optional sugar.

### 4.4 Parse option for backward compat

If pipe ships, it gets a parse option matching the convention of the
streaming-operator opt-outs:

| Parse directive | Parse option flag (C++) | Disables |
|---|---|---|
| `%no-pipe` | `PO_NO_PIPE` | the `\|>` operator (parser falls back to treating `\|` as bitwise-or) |

Programs that don't use pipe pay nothing whether or not the opt-out is
set; programs that use the pipe operator break under the opt-out (by
design — that's the back-compat case).

### 4.5 Effort if accepted (in addition to streaming-operators.md)

| Component | Estimate |
|---|---|
| Lexer changes (`\|>` token) | 1 day |
| Parser changes (iter_chain grammar with stage productions) | 5-7 days |
| Closure inline form (`c -> body`) — if not in language already | 3-4 days |
| Documentation + migration guide | 4 days |
| Test coverage | 1-2 weeks |
| **Total (sequential)** | **~4-6 weeks** |

The fusion optimizer and the new keyword operators land first
(streaming-operators.md) and stand alone; pipe sugar lowers at parse
time to the same AST those proposals already define.

---

## 5. References

- [`char-type.md`](char-type.md) — `char` value type (recommended for
  2.3)
- [`streaming-operators.md`](streaming-operators.md) — the operators,
  fusion, and `iterate` keyword this sugar would chain (recommended for
  2.3)
- `bugfix/text_processing` commits `8165b217b`–`bb1ea2d96` — the
  text-processing optimizations that motivated all three proposals
- `lib/parser.ypp`, `lib/scanner.lpp` — parser/lexer touch points
