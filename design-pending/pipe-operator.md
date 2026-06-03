# `|>` Pipe Operator — Lazy Chain Sugar

**Status:** Design. **Recommendation: defer to a release after 2.3.**
This document captures the proposal so we have it on file; the
substantive wins are delivered by the streaming operators now documented
in `doxygen/lang/180_operators.dox.tmpl`, and pipe syntax is pure surface
sugar that can be added later additively without breaking compatibility.

**Target if accepted:** post-2.3. Pipe lowers at parse time to the same
streaming-operator AST and IR lowering used by the keyword forms.

**Companions:**
- [`char-type.md`](../design/char-type.md) — implemented `char` value type
- streaming operators — implemented for 2.3; see
  `doxygen/lang/180_operators.dox.tmpl` for the `iterate` keyword,
  keyword operators, and fusion behavior this sugar would chain

---

## 1. What this proposal adds

With the streaming operators in Qore 2.3, chains of streaming keyword
operators (`first`, `any`, `all`, `take`, `drop`, `takewhile`,
`takeuntil`, `count`) can already execute through native streaming IR.
The functionality is present in nested keyword form; the `iterate`
keyword is the uniform iterator entry point for non-string sources.

What pipe syntax adds is a **left-to-right, source-first chain syntax**
that desugars at parse time into the same nested AST:

```qore
# Without pipe (existing keyword-operator syntax, with new operators)
int letters = count $1.isLetter(), iterate str;

list<string> errs = map $1, (take 10, (select log.splitLines(), $1 =~ /ERROR/));

# With pipe (proposed sugar)
int letters = iterate str
                |> filter $1.isLetter()
                |> count;

list<string> errs = iterate log.splitLines()
                       |> filter $1 =~ /ERROR/
                       |> take 10
                       |> collect;
```

Pipe is **purely a surface choice** — the runtime sees the same fused
loop either way. The question this document addresses is: is the new
syntax worth the cognitive load of having two ways to write the same
chain?

The examples below use pipe-stage names, not new standalone keyword
operators. Normative lowering is in §4.1. In particular, `filter` is the
pipe spelling of `select`, `reduce` is the pipe spelling of `foldl`, and
`collect` materializes the current lazy stream with an identity `map`.

---

## 2. The honest answer

### 2.1 1-stage chains — pipe is worse

```qore
# Existing
list<auto> evens = select source, $1 % 2 == 0;
int total       = foldl $1 + $2, source;

# Pipe
list<auto> evens = iterate source |> filter $1 % 2 == 0 |> collect;
int total       = iterate source |> reduce $1 + $2;
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
            |> reduce $1 + ",\n" + $2;
```

Roughly equal. The existing one-line form fits the screen; pipe needs a
multi-line layout to read better. **Wash.**

The qlib codebase has ~20 occurrences of `foldl(map(...))` at 2 stages —
the dominant existing chain shape. Pipe doesn't help these.

### 2.3 3-stage chains — pipe slightly ahead

```qore
# Today (right-to-left mental order)
int n = count $1 =~ /^[a-z]+$/,
              (map $1.lwr(),
                  (select text.splitLinesRegex("\\s+"), $1.size() > 3));

# Pipe (left-to-right matches execution order)
int n = iterate text.splitLinesRegex("\\s+")
          |> filter $1.size() > 3
          |> map    $1.lwr()
          |> count $1 =~ /^[a-z]+$/;
```

Pipe is ~30 % clearer because the reader doesn't have to mentally
invert order. **Slight win for pipe.**

The equivalent using the new keyword operators is comparable in character
count if a multi-line layout is allowed:

```qore
int n = count
            $1.size() > 3 && $1 =~ /^[a-z]+$/,
            (map $1.lwr(), text.splitLinesRegex("\\s+"));
```

Roughly the same effort to write either; pipe still slightly clearer.

### 2.4 4+ stage chains — pipe meaningfully better

```qore
# Process CSV: skip blanks/comments, parse, take first 100
# Today: requires intermediate variables for sanity (no nested form is readable)
list<auto> rows = csv.splitLines();
list<auto> non_empty = select rows, $1 != "";
list<auto> data_rows = select non_empty, $1 !~ /^#/;
list<auto> parsed = map $1.split(","), data_rows;
list<auto> sample = map $1, (take 100, parsed);

# Pipe
list<auto> sample = iterate csv.splitLines()
                       |> filter $1 != ""
                       |> filter $1 !~ /^#/
                       |> map    $1.split(",")
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
patterns) are delivered by the streaming operators:

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
    |> reduce <expr>
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

Stage lowering:

| Pipe stage | Nested-form lowering |
|---|---|
| `filter <pred>` | `select <source>, <pred>` |
| `map <expr>` | `map <expr>, <source>` |
| `take <n>` | `take <n>, <source>` |
| `drop <n>` | `drop <n>, <source>` |
| `takewhile <pred>` | `takewhile <pred>, <source>` |
| `takeuntil <pred>` | `takeuntil <pred>, <source>` |
| `reduce <expr>` | `foldl <expr>, <source>` |
| `count` / `count <pred>` | `count <source>` / `count <pred>, <source>` |
| `first` / `first <pred>` | `first <source>` / `first <pred>, <source>` |
| `any` / `any <pred>` | `any <source>` / `any <pred>, <source>` |
| `all <pred>` | `all <pred>, <source>` |
| `collect` | `map $1, <source>` (identity materialization) |
| `foreach <body>` | `foreach` statement over `<source>` with `<body>` |

This proposal does **not** add method-call stages such as `|> splitLines`
or new domain stages such as `|> dedupe-by`. Callers write those operations
in the source expression (`iterate text.splitLinesRegex("\\s+")`) or add a
separate future proposal.

The `iterate` keyword and the per-type element protocol are part of the
streaming-operator implementation — pipe just consumes what `iterate`
produces.

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

### 4.5 Effort if accepted (in addition to streaming operators)

| Component | Estimate |
|---|---|
| Lexer changes (`\|>` token) | 1 day |
| Parser changes (iter_chain grammar with stage productions) | 5-7 days |
| Closure inline form (`c -> body`) — if not in language already | 3-4 days |
| Documentation + migration guide | 4 days |
| Test coverage | 1-2 weeks |
| **Total (sequential)** | **~4-6 weeks** |

The fusion optimizer and the new keyword operators stand alone; pipe
sugar would lower at parse time to the same AST those operators define.

---

## 5. References

- [`char-type.md`](../design/char-type.md) — implemented `char` value type
- `doxygen/lang/180_operators.dox.tmpl` — implemented streaming
  operators, fusion, and `iterate` keyword this sugar would chain
- `bugfix/text_processing` commits `8165b217b`–`bb1ea2d96` — the
  text-processing optimizations that motivated all three proposals
- `lib/parser.ypp`, `lib/scanner.lpp` — parser/lexer touch points
