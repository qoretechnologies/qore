# `char` Value Type and Lazy Iteration Operator

**Status:** Design. Two related language-feature proposals that together close
the remaining performance + ergonomics gap left after the byte-level
optimizations on `bugfix/text_processing` (commits `8165b217b`..`bb1ea2d96`).

**Target:** Qore 2.3 (the in-development release this branch already targets).
API and ABI compatibility are already broken in 2.3; both proposals can land
inside the same release without staged deprecations. Both are language-level
changes touching the lexer, parser, type checker, runtime, and qpp; they are
not library-level work.

**Author:** drafted alongside the text-processing performance branch.

---

## 1. Motivation

The current text-processing toolkit is everything we can reach **as a
library**:

- `<string>::getByte(i)` — O(1) byte access (160× faster than `str[i]`)
- `<string>::codePointIterator()` — O(1) codepoint iteration via fast-path dispatch
- `Qore::Scanner` — stateful peek/advance with byte and codepoint variants
- Native iterator fast-path (commits `b16051764`, `b0e3c348c`, `35573a101`)
- Lazy split iterators (`StringSplitIterator`, `StringRegexSplitIterator`)
- Six qlib tokenizer/scanner migrations (Qdx, DPQL, stripAnsi, scan_exp, …)

Every honest attempt to migrate a real qlib tokenizer produces the same shape:

```qore
# Before (idiomatic but O(N²)):
*string c = input[pos];
if (c == "#") { ... }
value += c;
advance();

# After (byte-indexed, requires UTF-8 normalization at entry, regex/concat
# sites need string fallback, ord() noise everywhere):
input = text.encoding() == "UTF-8" ? text : convert_encoding(text, "UTF-8");
int b = input.getByte(pos);
if (b == ord("#")) { ... }
value += chr(b);
++pos;
```

The migrated form is **4–11× faster** in production and silently broken on
UTF-16 / UTF-32 sources unless the author knows to call `convert_encoding()`.
We chose performance over readability and pushed the encoding contract into
every caller. The library can't fix this — `str[i]` returning `string` is the
bottleneck *because* the result has to support both `c == "#"` *and*
`value += c`, and the only such type today is `string` (which allocates).

There is also a chain-fusion gap. Every functional pipeline like
`foldl … (select … (map …, str.codePointIterator()))` pays four stages of
Qore-level dispatch even with the native fast path on each individual
iterator. Rust's `.iter().filter().map().sum()` fuses to a single C++ loop;
Qore's chain doesn't and can't, because the iterator stages are library
classes the compiler never sees together.

The two proposals below are designed to remove **both** ceilings:

| Proposal | Removes |
|---|---|
| `char` value type | the `str[i]` allocation + the encoding contract leak |
| Lazy iteration operator | per-stage dispatch in functional chains |

They are independent — either can ship without the other — but they multiply
each other's value because the natural per-element type of a lazy string
iterator is `char`.

---

## 2. What's already built (and where the gap remains)

### 2.1 Byte access primitives

`<string>::getByte(i)` and `Qore::Scanner::peekByte()` give O(1) byte
access. **Limitation:** byte-only — caller must compare against `ord("...")`
literals, can't concatenate raw bytes into a string without `chr(b)`, and
breaks silently on non-ASCII-compatible source encodings unless
`convert_encoding()` is called at entry.

The qlib migrations (`Qdx::QoreTokenizer`, `DpqlTokenizer`, `Util::stripAnsi`,
`Util::scan_exp`, `DpqlCompletionProvider`) all show the same shape:
ASCII-anchored tokenization with manual `ord()` and `chr()` plumbing plus an
explicit UTF-8 normalization at the public entry. Real perf wins (4×–11×) but
**a code review burden every author has to carry**.

### 2.2 Codepoint primitives

`<string>::codePointIterator()` and `Qore::Scanner::peek()` give correct
codepoints for any encoding. **Limitation:** the per-element type is
`int` — same `chr()` plumbing required for any concat, no compile-time
distinction between "this int is a codepoint" and "this int is a count".

`<string>::splitChars()` returns 1-character strings — type-correct for concat
but **allocates per yield**. Acceptable for short strings; not for tokenizers.

### 2.3 Iterator fast-path

`AbstractIteratorHelper` probes the iterator's priv class for a
`supportsNativeIteration()` override and bypasses Qore method dispatch on
`next()`/`getValue()` when present. **Limitation:** each *stage* of a
`map → select → foldl` chain still goes through the helper. We've reduced
per-stage cost ~2× (commit `b0e3c348c`), but per-element work in a 4-stage
chain is still 4× the per-element work of a fused C++ loop.

### 2.4 What we genuinely cannot reach without language changes

1. **`str[i]` that's both O(1) and allocation-free *and* concat-compatible.**
   The library has no return type that satisfies all three; only adding one
   does.

2. **Compiler-fused functional chains.** `map+select+foldl` on a
   string is conceptually a single loop; the runtime can only see four loops
   because each stage is a library object.

3. **Type-system-enforced "this scans codepoints, not bytes".** Today every
   migration is a manual review for "is this byte path safe?". A distinct
   `char` type makes the question a compile error.

---

## 3. Design A — the `char` value type

### 3.1 Summary

`char` is a new **value type** holding a single Unicode codepoint
(0–0x10FFFF). It is:

- **Inline-encoded** in `QoreValue` (no allocation, like `int`/`bool`)
- **Subtype-compatible with `string`** for concatenation and comparison
- **Distinct from `int`** at the type system level (no implicit narrowing)
- **Auto-convertible from `int`** (explicit cast) and `string` of length 1
  (implicit; runtime check)

### 3.2 Literal syntax

A new lexer token kind `QCHAR` handled by the existing string-literal
machinery extended with a single-codepoint constraint:

```
'a'         // U+0061
'\n'        // U+000A
'\t'        // U+0009
'\\'        // U+005C
'\''        // U+0027
'é'    // U+00E9 (é)
'\u{1F600}' // U+1F600 (😀; brace form for codepoints > 0xFFFF)
'\x7F'      // U+007F (byte-style, restricted to <= 0x7F)
```

Single-quoted *strings* in Qore today are `'...'`. The literal is **a single
character** — multi-char single-quoted literals become a parse error (so
`'ab'` is rejected; today it's a 2-character string). This is a
**backwards-compatible breaking change for single-quoted multi-char strings**;
double-quoted multi-char strings (`"ab"`) are the canonical form, and the
single-quoted form has historically been used for "raw" strings without
escape interpretation. We address compatibility in §7.

Alternative considered: introduce `c'...'` prefix for char literals to
avoid the ambiguity. Rejected — `'a'` is by far the most natural form, and
the existing single-quoted-string usage is migratable.

### 3.3 Type system semantics

```qore
char c = 'a';                  // direct
char c = "a";                  // implicit narrowing from 1-char string; runtime check
char c = 0x61;                 // implicit widening from int (compile-time const); else error
char c = (char)97;             // explicit cast from int
int  i = c;                    // implicit widening to int (codepoint value)
string s = c;                  // implicit widening to 1-char string
```

The type lattice gains one new node:

```
              auto
           ╱   │   ╲
         …   string  int
              │    │
              char─┘     // char is a subtype of BOTH string (length-1) and int (codepoint)
```

Concretely:

- **Subtype of `string`** for assignment compatibility, concatenation, and
  pattern matching (`c == "a"` still works; `value += c` still works).
- **Subtype of `int`** for codepoint arithmetic (`c + 1`, `c >= 'a' && c <= 'z'`).
- **Distinct nominal type** so function signatures can require `char`
  specifically. `string s = "abc"; char c = s;` is a runtime error
  (`s.size() != 1`), not a silent truncation.

### 3.4 Operations

```qore
char c = 'a';

# Comparisons (both string-style and int-style work)
c == 'a'               # char == char
c == "a"               # char == string (lifts char to length-1 string)
c == 0x61              # char == int (lifts char to codepoint int)
c >= 'a' && c <= 'z'   # char range

# Arithmetic (treats char as int, returns int)
int i = c + 1;         # 0x62
char d = (char)(c + 1); # explicit re-cast

# Concatenation (treats char as length-1 string)
string s = "x" + c + "y";   # "xay"
s += c;                     # mutating concat

# Codepoint access (no allocation)
string str = "héllo";
char c0 = str[0];           # 'h' (U+0068)
char c1 = str[1];           # 'é' (U+00E9), for any input encoding
```

The single new pseudo-class `<char>` exposes:

```qore
int    <char>::ord()                    # codepoint as int
string <char>::toString()               # length-1 string
bool   <char>::isAscii()                # 0..0x7F
bool   <char>::isLetter() / isDigit() / isAlpha() / isWhitespace()
bool   <char>::isUpper() / isLower()
char   <char>::upr() / lwr()            # case mapping (returns char)
int    <char>::utf8ByteLen()            # 1..4
```

### 3.5 Indexing change: `string[i]` returns `char`

Today `string[i]` returns `*string` (1-character string or NOTHING). The
proposal changes it to `*char` (codepoint or NOTHING).

This is the highest-impact change. Every existing tokenizer / scanner /
single-char compare site benefits without explicit migration:

```qore
# Today (slow + allocates)
*string c = input[pos];
if (c == "#") { ... }

# Under this proposal (no migration needed):
*char c = input[pos];   # type changes from *string to *char
if (c == "#") { ... }   # still compiles (char compares with length-1 string)
                        # but no QoreStringNode allocated
```

Backward compatibility:

- `c == "#"` continues to work (char ↔ length-1 string compare lifts to either side).
- `value += c` continues to work (char ↔ string concat).
- `*string c = input[pos];` declarations need either a runtime type widen
  (allowed: char → string is implicit) or a compile-time deprecation warning
  asking the author to write `*char c`.

No parse-directive gate. 2.3 already breaks API/ABI; the indexing return
type changes as part of the release. Existing code with `*string c =
input[pos]` continues to compile because `char → string` is implicit, and
the runtime cost of that lift is one inline branch (`isChar()` check) plus
the existing 1-char string materialization on demand — strictly faster than
today's per-call allocation.

### 3.6 Internal representation

`QoreValue` already uses NaN-boxed inline encoding with 16-bit tags
(`include/qore/QoreValue.h:286-297`). One more tag slot:

```cpp
static constexpr uint64_t TAG_CHAR = 0xFFFE000000000000ULL;
```

Payload layout: low 21 bits hold the codepoint (Unicode max U+10FFFF fits in
21 bits), upper bits zero. No allocation; `QoreValue` carrying a `char`
behaves exactly like one carrying an `int48` from the user's perspective.

`qore_type_t NT_CHAR = 51` is added to `node_types.h` for the rare paths
that switch on type tag (`AbstractQoreNode::getType()` etc.).

### 3.7 Conversion rules summary

| From | To `char` | To `int` | To `string` |
|---|---|---|---|
| `char` | identity | implicit | implicit |
| `int`  (compile-time const) | implicit if 0..0x10FFFF | identity | via `chr()` |
| `int`  (runtime) | explicit `(char)` cast | identity | via `chr()` |
| 1-char `string` | implicit (runtime check) | via `getUnicode(0)` | identity |
| ≥2-char `string` | runtime error | error | identity |
| `*char` | identity | NOTHING-aware | NOTHING-aware |

### 3.8 What the `char` type does NOT change

- `str.size()` still returns byte count.
- `str.length()` still returns codepoint count.
- `str.getByte(i)` and `str.getUnicode(i)` keep their existing semantics for
  authors who specifically want byte- or codepoint-position access.
- `<binary>` indexing stays as `*int` (raw bytes).

The `char` proposal is **strictly additive at the runtime level** and
**default-on at the parser level** with a one-release transition for
`'multi-char'` literals.

---

## 4. Design B — the lazy iteration operator

### 4.1 Summary

A new operator that produces a *lazy iterator* over any iterable expression.
The compiler recognises chains of these iterators and fuses them into a
single C++ loop at compile time, eliminating per-stage Qore method dispatch
that even the native fast path can't fully avoid.

The motivating shape:

```qore
# Today (4 stages of dispatch, each with O(1) but Qore-level call):
int letters = foldl $1 + $2,
    (map 1, (select str.codePointIterator(),
             ($1 >= ord("A") && $1 <= ord("Z")) || ($1 >= ord("a") && $1 <= ord("z"))));

# Under this proposal (single fused pass):
int letters = (iter str
                 |> filter c -> c.isLetter()
                 |> count);
```

### 4.2 Syntax

After comparing pipe-style (`|>`), method-chain (`.iter().filter(...)`), and
operator-keyword forms, the recommended syntax is:

```qore
iter <iterable-expression>             # produces an iterator over the natural element type
    |> filter <pred>                   # boolean test on each element
    |> map    <expr>                   # transform each element
    |> take   <n>                      # bounded prefix
    |> drop   <n>                      # skip prefix
    |> until  <pred>                   # take while !pred
    |> while  <pred>                   # take while pred
    |> reduce <init>, <fold-expr>      # foldl
    |> count                           # count of elements
    |> collect                         # materialise as list (only at the end)
    |> foreach <body>                  # consume with a statement body
```

`|>` is a new infix operator that left-associates; the right operand must be
a recognised pipe stage. The whole `iter … |> stage |> stage` form is itself
an expression that yields either an iterator (when the chain ends in a non-
terminal stage like `take` or `map`) or a value (terminal stages like
`reduce`, `count`, `collect`, `foreach`).

Stages whose operand uses `$1`/`$2` (closures/expressions) are interpreted
exactly like the existing `map`/`select`/`foldl` operators — no new
expression language is needed.

### 4.3 Element-type protocol

`iter X` yields elements per X's type:

| `X` is | element type |
|---|---|
| `string` | `char` |
| `binary` | `int` (raw byte) |
| `list<T>` | `T` |
| `hash<K, V>` | `hash<{key: string, value: V}>` (key-value pair) |
| any object exposing `<class>::iterator()` | element type from that iterator |
| `int` (positive) | `int` (range `0..N-1`, like `xrange(0, N)`) |
| `range<int>` | `int` |

Discoverability: a single `iter` operator replaces the per-type factories
(`codePointIterator`, `splitChars`, `keyIterator`, `pairIterator`, `iterator`,
`xrange`). The factory methods stay for backward compatibility.

### 4.4 Chain fusion

The operator is "lazy-capable", meaning the compiler is *allowed* to fuse
the chain when it can prove no observable side effect is reordered.
Concretely:

- The whole chain is parsed as a single AST node `IterChain { source, stages[] }`.
- The optimizer walks the chain. For each stage it knows (filter, map, take,
  drop, while, until, reduce, count), it emits inline element-handling code.
- The fused loop reads one element from `source`, runs the stages in order,
  and either advances or terminates.

A naive non-fused implementation that just chains `map`/`select`/`foldl`
internally is the fallback; fusion is an optimization, not a semantic
requirement.

What the optimizer needs to be conservative about:

- **Stage closures with side effects.** If the closure body has visible side
  effects (function calls, mutations, throws), the fused loop must preserve
  ordering — this is automatic because fusion is sequential per element, not
  parallel.
- **Short-circuit semantics.** `take`, `until`, `while` must terminate the
  source iterator early. The loop control is the same as today's
  `foreach … break;` pattern.
- **Reference holding.** The `iter X` source expression is evaluated once and
  held by the chain; the chain itself is RAII (deref on scope exit).

### 4.5 Integration with existing operators

The existing `map`/`select`/`foldl`/`foreach` operators stay. Internally
they all become syntactic sugar for `iter`-chain forms:

| Existing | Equivalent `iter` form |
|---|---|
| `map EXPR, ITER` | `iter ITER \|> map EXPR \|> collect` |
| `select ITER, PRED` | `iter ITER \|> filter PRED \|> collect` |
| `foldl EXPR, ITER` | `iter ITER \|> reduce 0, EXPR` (with semantic shim) |
| `foreach v in (ITER) BODY` | `iter ITER \|> foreach BODY` |

The parser rewrites these to internal `IterChain` nodes; existing user code
keeps working unchanged. New code written with `|>` gets fusion for free.

### 4.6 Why this works in Qore where library iterators don't

The key compiler property: **the chain's stages are visible at parse time as
a single AST**. With library iterators, `foldl(..., select(..., map(..., x)))`
parses to nested function-call nodes whose internal element-handling is
opaque to the optimizer. With `|>`, the compiler sees the entire pipeline,
including the closure bodies, before lowering — so it can rewrite the AST
into a single `for` loop with the closure bodies inlined.

Estimated speedup over current native-fast-path chains: **2–3× per-element**
on tokenizer-shaped workloads (4 stages → 1 stage), more on shorter chains
that allocate intermediate lists today.

---

## 5. Compiler implementation

### 5.1 Lexer (`lib/scanner.lpp`)

- Add a new lexer state for char literals. Single-quoted source is currently
  the QSTRING_SQ state; reuse the same machinery but emit `QCHAR` (with the
  decoded codepoint as `yylval->charcode`) when the literal is exactly one
  codepoint, and a deprecation warning + `QSTRING` when it is two or more
  (during the transition release).
- Add `\u{HHHHHH}` brace escape for codepoints above U+FFFF.
- Add `iter` and `|>` as reserved tokens.

### 5.2 Parser (`lib/parser.ypp`)

- New grammar production `char_literal` returning a `QoreCharNode`
  (parse-time node that lowers to an inline `QoreValue` carrying TAG_CHAR).
- New grammar production `iter_chain` with stages parsed as a left-associative
  `|>` chain. Each stage's operand is a regular expression (closures
  inclusive).
- Strict precedence: `iter` is unary prefix at the same level as
  `map`/`select`; `|>` is binary infix lower than `||` but higher than `?:`,
  so `cond ? iter x |> map e : default` parses as expected.

### 5.3 Type checker (`lib/QoreType.cpp` and friends)

- Add `charTypeInfo` and `charOrNothingTypeInfo` (matching the existing
  `intTypeInfo`/`intOrNothingTypeInfo` pattern).
- Make `charTypeInfo` a subtype of `stringTypeInfo` (assignment compat).
- Make `charTypeInfo` a subtype of `intTypeInfo` for arithmetic / comparison
  but **not** for return-type matching of methods declared `int` (so
  `int sub foo()` returning a `char` is a compile error — the author must be
  explicit).
- For the indexing change, `string::operator[]` is rewritten in
  `lib/Pseudo_QC_String.qpp` to declare return type `*char` instead of
  `*string`.

### 5.4 Optimizer / fusion pass (new file `lib/IterChainOptimizer.cpp`)

- Walks the parsed AST after type checking, identifies `IterChain` nodes,
  and lowers them to a single `IterFusedLoopNode` if all stages are
  fuseable. Falls back to nested-iterator emit when a stage uses a feature
  the optimizer doesn't recognise (custom user iterator, reflection, etc.).
- The fused-loop node knows its source's `nativeNext`/`nativeGetValue`
  primitives (from the existing fast-path infrastructure) and inlines the
  element type, avoiding `QoreValue` boxing for primitive element types
  (`char`, `int`, `bool`).

The optimizer's hardest job is **closures with reference capture**: the
inlined closure body must run in the captured scope, not in the loop's. We
solve this by emitting the closure call exactly as today's `map` does, just
inlined into the fused loop body. No new closure-handling code is needed.

---

## 6. Runtime implementation

### 6.1 `QoreValue` char encoding

`QoreValue::tag()` returns 16 high bits; `TAG_CHAR = 0xFFFE000000000000ULL`
is the next free slot. Codepoint stored in low 21 bits.

```cpp
// QoreValue.h additions
inline bool isChar() const { return tag() == TAG_CHAR; }
inline int  asCodepoint() const {
    assert(isChar());
    return static_cast<int>(bits & 0x001FFFFFULL);
}
inline static QoreValue makeChar(int cp) {
    assert(cp >= 0 && cp <= 0x10FFFF);
    QoreValue v;
    v.bits = TAG_CHAR | static_cast<uint64_t>(cp);
    return v;
}
// String-compat: implicit conversion to a 1-char QoreStringNodeView when
// asked for a string. This hooks into QoreValue::getStringNode() to
// allocate-on-demand only when actually consumed by a string-typed sink.
```

The "lift to length-1 string" path goes through a small fast accessor that
returns a stack-allocated UTF-8 buffer for the codepoint (1–4 bytes). String
*pseudo-method* calls on a `char` (e.g., `c.size()` or `c.upr()`) materialise
the stack buffer once per call; no permanent QoreStringNode is allocated
unless the string escapes (assignment to a `string` lvalue).

### 6.2 Encoding-aware indexing

`string::operator[]` (returning `*char`) needs to be O(1) for any encoding.
Today `qore_string_private::operator[]` walks UTF-8 from byte 0 — that's the
O(N²) bug we keep paying.

Fix: a per-string **cached codepoint offset table** in
`qore_string_private`. For UTF-8 source it's an array of byte offsets, one
per codepoint, built lazily on first indexed access. Memory: 4 bytes per
codepoint (or 8 for huge strings). For pure-ASCII strings, byte offset ==
char offset, no table needed (detected by a flag on `qore_string_private`).

For non-UTF-8 encodings (`UTF-16LE`, `Latin-1`, etc.) the same table caches
per-codepoint byte offsets. The conversion to UTF-8 some of our migrated
qlib code does at entry would no longer be necessary — the runtime handles
encoding transparently.

Cache invalidation: any mutation that changes the byte representation
(splice, concat-mutate, replace) bumps a version counter; `operator[]`
checks the counter and rebuilds the table on next access.

### 6.3 Lazy iterator IR (for fusion)

`IterFusedLoopNode` (in `include/qore/intern/IterFusedLoopNode.h`) holds:

- Source iterator's native primitives (`nativeNext` / `nativeGetValue`)
- A vector of stage descriptors (`Filter`/`Map`/`Take`/`Drop`/`Reduce`/etc.)
- Local variable slots for stage state (counter, accumulator, etc.)

`evalImpl()` is a single C++ loop that walks stages per element, breaks on
`Take`/`Drop`/`While`/`Until` boundaries, and reduces / collects to the
final value.

For element types that are inline `QoreValue` carriers (`int`, `char`,
`bool`, short string), the loop avoids the `QoreValue::deref()` /
`refSelf()` cycle entirely — the value passes through stages by copy.

---

## 7. Migration & backward compatibility

### 7.1 Delivery in 2.3

Both features ship together in 2.3. 2.3 is already breaking API/ABI for
unrelated runtime changes (text-processing perf branch, async I/O work);
piggybacking these language changes carries no incremental compatibility
cost beyond what the release already incurs.

The implementation can land **incrementally on the development branch**
(value type → indexing → pseudo-class → iter parse → iter fusion) so a
working subset is always testable, but every component lands before the 2.3
release tag — there is no "2.4 phase". Suggested commit order on the
language branch:

| Step | Commit theme | Test gate |
|---|---|---|
| 1 | `TAG_CHAR` in QoreValue + `<char>` pseudo-class + `'x'` lexer | char value tests |
| 2 | Codepoint-offset cache in `qore_string_private` | string indexing tests, valgrind |
| 3 | `string[i]` return type → `*char`; type checker rules | full qtest suite green |
| 4 | `iter` keyword + `\|>` operator + non-fused stage emit | iter chain tests |
| 5 | Fusion optimizer + property tests | bench harness shows fused win |
| 6 | qlib follow-up: collapse `getByte()` migrations to `char` | qlib regression tests |

Step 6 is optional cleanup of the text-processing migrations (Qdx, DPQL,
etc.) — they keep working with `getByte()` but are noticeably cleaner using
`char`.

### 7.2 Single-quoted multi-char literals

Hard error in 2.3. The release notes call it out; the parser emits an
unambiguous diagnostic ("multi-character single-quoted literal; did you
mean a string?  use double quotes"). A trivially-mechanical migration tool
(`qore-migrate-singlequote`) is shipped that rewrites every occurrence
across a tree.

We surveyed the qore tree for `'..'` literals before writing this design;
qlib has zero multi-char single-quoted literals. The existing usage is
overwhelmingly `'a'`, `'\n'`, etc. — single-character — which become `char`
and continue to behave correctly via the implicit `char → string`
conversion.

### 7.3 Backward compatibility rules

- `string s = c;` always compiles (char → string is implicit).
- `c == "x"` always compiles (char ↔ length-1 string compare).
- `value += c` always compiles (char → string in concat context).
- Single-character `'a'` literals start parsing as `char`. The implicit
  `char → string` conversion means existing assignments to `*string`
  variables continue to compile and behave correctly; the user's program
  observes no change unless they were depending on the literal's type
  being `string` for a method-overload selection (very rare).
- Multi-character single-quoted literals are a parse error. Migration tool
  shipped.

### 7.4 Library deprecations rolled in

- `<string>::splitChars()` is kept but documented as "less efficient than
  `iter str |> map …`" once the iter operator ships in the same release.
- `<string>::codePointIterator()` likewise — the natural form becomes
  `iter str` with element type `char`.

Both stay in the source for compatibility; we don't remove them in 2.3.

### 7.5 Effects on existing code

The qlib migrations done on `bugfix/text_processing` (Qdx, DPQL, stripAnsi,
scan_exp, DpqlCompletionProvider) become *simplifications*:

```qore
# Today (after byte-indexed migration):
int b = input.getByte(pos);
if (b == ord("#")) { comments++; ... }
value += chr(b);

# Under char + iter:
char c = input[pos];
if (c == '#') { comments++; ... }
value += c;
```

Same speed (the runtime indexing is now O(1) via the codepoint cache),
materially better readability, and no manual UTF-8 normalization at entry.

---

## 8. Performance projections

Numbers anchored against measurements from the
`bugfix/text_processing` branch (commits `8165b217b`..`bb1ea2d96`):

| Workload                                   | 2.2 (pre-branch) | 2.3 today | 2.3 + char + iter |
|---|---|---|---|
| Qdx tokenize 88 KB                         | 5041 ms          | 1054 ms   | ~700 ms |
| stripAnsi 84 KB                            | 1834 ms          |  172 ms   | ~130 ms |
| scan_exp 125 KB                            | 4714 ms          | 1166 ms   | ~800 ms |
| char-counting fold over 100 KB ASCII       |   50 ms          |   50 ms   | ~12 ms  |
| Lazy regex split, 50 K lines, early break  | 9095 ms          |  191 ms   | ~45 ms  |

The "2.3 today" column is the byte-indexed migration with manual
`getByte`/`ord`/`chr` plumbing and `convert_encoding` at entry. The third
column projects the same workloads after rewriting them to use `char` and
`iter` — the marginal speedup over today is modest on the simple
tokenizers (the hot loop is already O(N) and per-element overhead is small)
but substantial on multi-stage functional chains where fusion eliminates
intermediate iterator stages.

The bigger win of these features is **migrations we declined to do** today
because the byte path was too risky:

- `qlib/QoreCodeCompletion/CompletionEngine.qc` — char/byte/substr mix,
  reverted from this branch
- Any tokenizer expected to handle UTF-16/UTF-32 source without per-call
  conversion cost (today: forced UTF-8 normalize at ctor)
- Regex-heavy tokenizers where the byte path collides with PCRE2's
  codepoint matching

These all become straightforward with `char` indexing returning correct
codepoints in O(1) regardless of source encoding.

---

## 9. Risks and tradeoffs

### 9.1 Single-quoted-string syntax change

The most disruptive change. Some Qore code uses `'literal'` for short ASCII
strings, often unintentionally (the form is rare but exists). Mitigations:

- The implicit char → string conversion means that single-character
  `'a'` literals continue to behave correctly in nearly all expression
  contexts (assignment, concat, comparison).
- Multi-character `'ab'` becomes a hard error in 2.3 — the
  `qore-migrate-singlequote` tool walks a source tree and rewrites every
  occurrence to the double-quoted form. The transformation is unambiguous
  (any multi-char `'..'` was always semantically a string), so the tool
  can run unsupervised on a codebase.
- Single-quoted *string* literals in qlib were surveyed before this design;
  the cleanup surface is small.

### 9.2 Codepoint cache memory cost

The per-string codepoint offset table (§6.2) costs 4 bytes per codepoint.
For a 1 MB UTF-8 string with mixed scripts (~500K codepoints), that's 2 MB
of cache. Mitigations:

- Built lazily on first `[]` access; strings that are never indexed pay
  nothing.
- ASCII-only strings (detected by encoding flag, no high bits set during
  hash/compare) skip the cache entirely.
- Cache is per-string-instance; copies share the underlying buffer + cache
  via the existing shared-string mechanism.

### 9.3 Compiler complexity

The fusion optimizer is a non-trivial pass (~500–1000 LOC). Risk:
optimizer bugs that produce wrong results for unusual closures.
Mitigations:

- Fusion is an *optimization*, not a semantic requirement. The non-fused
  fallback (nested iterator implementation) is implemented first; fusion is
  a separate commit that can be reverted without affecting program
  behaviour. If fusion isn't ready by 2.3 release, ship the non-fused
  version — `iter` still wins via uniform syntax and the existing iterator
  fast-path infrastructure even without fusion.
- Each stage's fused emit is unit-tested with a parallel "naive emit" that
  generates the obvious nested-iterator form, and a property test that
  asserts both produce the same output for randomized inputs.

### 9.4 Type checker complexity

`char` being a subtype of *both* `string` and `int` is unusual — most
languages would require an explicit cast in one direction. The motivation
is ergonomics: tokenizer code reads naturally if `c == '#'` and `c + 1`
both work without ceremony. Risk: subtle type-inference edge cases. Mitigation:
prototype the type rules on the existing test suite first (it's a substantial
benchmark of the type system).

### 9.5 Standard library churn

`<char>` pseudo-class is new (~10 methods); `<string>::operator[]` return
type changes; introspection tools (qore-doc, QLS, astparser) need updates.
Estimated 1-2 weeks of follow-up after the language changes settle.

---

## 10. Effort estimate

| Component | Estimate |
|---|---|
| Lexer changes (char literals, brace escapes, `iter`/`\|>` tokens) | 3-4 days |
| Parser changes (char_literal, iter_chain grammar) | 5-7 days |
| Type checker (charTypeInfo, subtype rules, indexing return-type change) | 2 weeks |
| QoreValue encoding (TAG_CHAR + accessors + lift-to-string path) | 1 week |
| Codepoint offset cache for `string::operator[]` | 2 weeks |
| `<char>` pseudo-class | 4 days |
| Iter operator non-fused implementation | 2 weeks |
| Iter operator fusion optimizer | 4-6 weeks |
| Stage operators (filter/map/take/drop/while/until/reduce/count/collect) | 2 weeks |
| `qore-migrate-singlequote` tree-walker tool | 3 days |
| Test coverage (qtest + property tests for fusion) | 3-4 weeks |
| Doc updates (Doxygen, language guide, migration guide) | 1 week |
| **Total** | **~15-19 weeks** for both, all in 2.3 |

`char` alone (excluding iter): ~6-8 weeks. `iter` alone (excluding char):
~10-12 weeks. The two are independent and can be developed in parallel by
different people.

If 2.3's release window is shorter than this estimate, the natural split is
**`char` first, `iter` second**: `char` gives the larger user-visible win
on existing code (the indexing change benefits every tokenizer in qlib and
every external module), while `iter` is more prospective (new code written
to use it). `iter` can slip past 2.3 without delaying `char`; both ship in
the same release if both are ready.

---

## 11. Open questions

1. **Should `'a'` always be a `char`, or only in `char`-typed contexts?**
   Current proposal: always `char`. Alternative: context-sensitive — `'a'`
   is `char` when the target type is `char`, otherwise length-1 string. The
   alternative avoids the deprecation but makes literal type opaque.

2. **`hash<K, V>` element type under `iter`?** Pair, key, or value? Current
   proposal: pair (matching the `pairIterator()` precedent). Alternative:
   key (matching `foreach k in (h)` today). Pair is more useful inside
   stages, but breaks symmetry with `foreach`.

3. **Should `iter str` honour string encoding, or always promote to UTF-8?**
   Current proposal: honour encoding (codepoint cache handles it). If we
   always promote, we duplicate the conversion the migrated tokenizers do
   today. Honouring is cleaner.

4. **Stage ordering: pipe-style (`|>`) vs method-style (`.filter()`)?**
   Pipe-style chosen for readability and grammar simplicity. Method-style
   would integrate with existing object-method dispatch but conflicts with
   the iterator's element-type polymorphism.

5. **Should `count` accept a predicate?** `iter str |> count c -> c.isLetter()`
   reads naturally and avoids the `filter |> count` two-stage form. Probably
   yes; trivial to add.

---

## 12. References

- `bugfix/text_processing` commits `8165b217b`–`bb1ea2d96` (the qlib
  migrations whose ergonomic cost motivates this design)
- `include/qore/QoreValue.h` (NaN-boxed value layout)
- `include/qore/intern/AbstractIteratorHelper.h` (existing native fast-path
  the iter operator builds on)
- `lib/parser.ypp`, `lib/scanner.lpp` (parser/lexer changes land here)
- `design/dgc.md` (memory-correctness invariants the lift-to-string path must
  honour)
