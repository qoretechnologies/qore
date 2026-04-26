# `char` Value Type

**Status:** Design.

**Target:** Qore 2.3. API and ABI are already breaking in this release for the
text-processing performance branch (`bugfix/text_processing`); the language
changes proposed here piggyback on that break with no incremental
compatibility cost.

**Companion documents:**
- [`streaming-operators.md`](streaming-operators.md) — independent
  proposal for `first`/`any`/`all`/`take`/`drop`/`takewhile`/`takeuntil`/
  `count` keyword operators + the `iterate` verb-form iterator factory +
  fusion of nested operator chains. Recommended for 2.3 alongside this
  proposal.
- [`pipe-operator.md`](pipe-operator.md) — separate, more
  speculative proposal for a `|>`-style functional chain syntax.
  Recommended deferred.

The three proposals are independent: any subset can ship in 2.3.
`char` and the streaming operators are the strong proposals; pipe sugar
is optional.

---

## 1. Motivation

The text-processing branch added byte-level primitives (`<string>::getByte`,
`Qore::Scanner`, native iterator fast-path, codepoint iterator, lazy split
iterators) and migrated six qlib tokenizers to byte-indexed scans. The
migrated tokenizers are 4–11× faster, but the cost paid for that speed is
visible in every migrated function:

```qore
# What we'd like to write (idiomatic, but O(N²) today)
*string c = input[pos];
if (c == "#") { ... }
value += c;
advance();

# What we had to write to get the speedup
input = text.encoding() == "UTF-8" ? text : convert_encoding(text, "UTF-8");
int b = input.getByte(pos);
if (b == ord("#")) { ... }
value += chr(b);
++pos;
```

The migrated form leaks an encoding contract into every caller (`UTF-8`-only;
authors must remember to normalize), buries comparisons under `ord()` noise,
and forces `chr()` wrappers on concatenation. **The library cannot fix
this** — `str[i]` returning `string` is the bottleneck *because* the result
has to be usable for both `c == "#"` *and* `value += c`, and the only such
type Qore has today is `string`, which allocates.

A new value type — `char`, holding a Unicode codepoint — solves all four
problems at once:

- **No allocation per indexed access.** Inline-encoded in `QoreValue`, like
  `int`/`bool`.
- **Concat-compatible.** `value += c` works because `char` lifts to a
  length-1 string in concatenation context.
- **Compare-compatible.** `c == "#"` works because `char` lifts to a
  length-1 string in equality context.
- **Codepoint, not byte.** Encoding-correct for any source string. The
  manual `convert_encoding()` step at tokenizer entries goes away.

This document proposes the `char` type, the `string[i]` indexing change that
returns it, and the runtime support needed to avoid rescanning from byte 0
on repeated indexed access in any encoding.

---

## 2. What this design replaces

| Today | Under this design |
|---|---|
| `*string c = input[pos]; if (c == "#") {...}` (slow + alloc) | `*char c = input[pos]; if (c == '#') {...}` (fast indexed access, no alloc) |
| `int b = input.getByte(pos); if (b == ord("#")) {...}` (fast, byte-only) | `*char c = input[pos]; if (c == '#') {...}` (fast AND encoding-correct) |
| `value += chr(b);` (allocation, byte-only) | `value += c;` (lifts to a 1-char string in the target/default encoding) |
| `convert_encoding(text, "UTF-8")` at every tokenizer entry | (not needed; runtime handles encoding) |

`<string>::getByte()`, `<string>::codePointIterator()`, `<string>::splitChars()`,
and `Qore::Scanner` all stay — they remain useful for byte-oriented work
(HTTP, MIME) and explicit codepoint streams. `char` is added alongside; nothing
is removed.

---

## 3. The `char` type

### 3.1 Summary

`char` is a new **value type** holding a single Unicode codepoint
(0–0x10FFFF). It has:

- **No encoding.** A `char` is a pure codepoint — an integer in the Unicode
  range. When converted to a standalone `string`, the default encoding for
  the resulting string is **UTF-8**. When appended to an existing string,
  the char is encoded according to the target string's encoding. (See
  §3.6.)
- **Inline encoding** in `QoreValue` — no allocation, like `int`/`bool`.
- **String-compatible** for concatenation and comparison.
- **Codepoint-compatible** for arithmetic and numeric comparison through
  explicit or operator-specific lifts.
- **Distinct nominal type** so function signatures can require `char`
  specifically.

### 3.2 Literal syntax

A new lexer prefix `c'..'` / `c"..."` introduces a char literal:

```qore
c'a'           # U+0061
c'\n'          # U+000A
c'\t'          # U+0009
c'\\'          # U+005C
c'\''          # U+0027
c"'"           # U+0027 — apostrophe without escape
c'é'           # U+00E9
c'\u{1F600}'   # U+1F600 (😀; brace form for codepoints > 0xFFFF)
c'\x7F'        # U+007F (byte-style escape, restricted to <= 0x7F)
```

The prefix can be lowercase `c` only (uppercase `C` reserved for potential
future literal kinds). Inside the quotes the existing string-literal lexer
machinery applies — same escape sequences, same encoding rules — but the
lexer emits an error if the literal contains anything other than exactly one
codepoint.

**Why a prefix instead of overloading `'..'`:** Qore's existing single-quoted
literals (`'foo'`) are strings; redefining them to be chars would break every
program that uses `'..'` for short strings. The `c'..'` form is unambiguous
and adds no migration cost.

The choice between `c'..'` and `c"..."` is purely stylistic. `c"'"` lets you
write an apostrophe as a char without escaping; `c'"'` does the same for a
double-quote.

### 3.3 Type system semantics

The type lattice gains one node under `string`, with explicit and
operator-specific codepoint conversion to `int`:

```
              auto
           ╱   │   ╲
         …   string  int
              │      ▲
              char ──┘  (explicit / operator lift)
```

Conversion rules:

| Source | To `char` | Notes |
|---|---|---|
| `char` | identity | — |
| `int` (compile-time const, 0..0x10FFFF) | implicit | char literal value |
| `int` (runtime) | explicit `(char)` cast | runtime check for valid codepoint |
| `string` | **parse-time error** | `char c = some_string_func()` does not compile |
| `string` literal of length 1 | **parse-time error** | use `c'a'` form for char literals |
| `string` (any length) via `(char)` cast | runtime check | length != 1 throws `INVALID-CHAR-CONVERSION` |
| `*char` | identity (NOTHING-aware) | — |

| Source | To other types |
|---|---|
| `char` → `int` | explicit in assignment/return contexts with `(int)` or `.ord()`; arithmetic and numeric comparison operators lift to the codepoint value |
| `char` → `string` | implicit in string contexts; yields a length-1 string (UTF-8 for standalone materialization, target encoding for append/concat) |
| `char` → `bool` | implicit; True for every valid char, including U+0000, because the string view has length 1 |

**`char` is not assignable from `string`.** `char c = some_string_func()`
fails at **parse time**, not at runtime — there is no implicit
length-checking conversion that would silently throw on a string of
length != 1. Code that wants a string-to-char conversion must use the
explicit `(char)` cast, which carries the runtime length check
visibly at the call site:

```qore
string s = some_func();
char c1 = s;          # PARSE ERROR — string is not assignable to char
char c2 = (char)s;    # OK — explicit cast; throws INVALID-CHAR-CONVERSION
                      # at runtime if s.length() != 1
char c3 = s[0];       # OK — string[i] returns *char (see §4); needs the
                      # NOTHING check
char c4 = c'a';       # OK — char literal form
```

This rule eliminates an entire class of "the function happened to return
two characters today" runtime surprises.

`char` is **distinct from `int`** at the type system level. `int sub foo()`
returning a `char` is a compile error — the author must say `char sub foo()`
or explicitly cast to `int`. This prevents accidental codepoint-as-count
confusions.

### 3.4 Operations

Operations use the conversion/lift rules above:

```qore
char c = c'a';

# Comparisons
c == c'a'              # char == char
c == "a"               # char == string (lifts char → length-1 string)
c == 0x61              # char == int (lifts char → codepoint int)
c >= c'a' && c <= c'z' # char range

# Arithmetic (operator-specific lift to codepoint int, returns int)
int i = c + 1;             # 0x62
char d = (char)(c + 1);    # explicit re-cast back to char

# Concatenation (treats char as length-1 string)
string s = "x" + c + "y";  # "xay"
s += c;                    # mutating concat

# Codepoint indexing (the key change — see §4)
string str = "héllo";
char c0 = str[0];          # c'h' (U+0068)
char c1 = str[1];          # c'é' (U+00E9), regardless of str's encoding
```

A new pseudo-class `<char>` exposes the useful string-view and
codepoint-view pseudo-methods, plus a small set of char-specific additions.

**Method availability.** Because a `char` can be viewed as both a
length-1 string and a Unicode codepoint, methods from both views should be
available where they make sense. The `<char>` pseudo-class implements them
explicitly so that:

```qore
char c = c'a';

# <string> methods — char lifts to length-1 string
int n   = c.size();      # 1 — UTF-8 byte count, same as string semantics
int l   = c.length();    # 1 — codepoint count, same as string semantics
char d  = c.upr();       # c'A' — case mapping
bool b1 = c.startsWith("a");
bool b2 = c.contains("a");

# codepoint-view methods
bool cp = c.charp();     # True
int  hc = c.hash();      # codepoint hash, like int hash
string h = c.toHex();    # "61" — hex of codepoint

# Critically: this works under the indexing change (§4)
auto x = some_string[0];
int n = x.size();        # works — lifts via *char → *string → string method
```

This closes the type-inference compatibility risk: existing code with
`auto x = s[0]; ... x.size() ...` continues to work after the indexing
change, because `<char>::size()` is defined (delegating to the string
lift).

**Method-name conflicts.** The string and codepoint views have several methods with
the same name but different semantics. `<char>` resolves each
explicitly:

| Method | `<string>` semantics | `<int>` semantics | `<char>` resolution |
|---|---|---|---|
| `toString()` | identity | decimal text of int | **string-like**: returns the 1-char string |
| `toString(encoding)` | re-encode | (n/a) | **string-like**: returns 1-char string in given encoding |
| `toInt()` | parse decimal | identity | **int-like**: returns the codepoint |
| `toBool()` | non-empty? | non-zero? | **string-like**: True for every valid char, including U+0000 |
| `toFloat()` | parse decimal | int as float | **int-like**: codepoint as float |
| `typeCode()` | NT_STRING | NT_INT | returns NT_CHAR |
| `format(...)` | string format | int format | **string-like** (matches concat behaviour) |

The general rule: methods that *describe* the value as a string (length,
content, case) lift to the string side; methods that *convert* to a
numeric type (toInt, toFloat) lift to the int side. Conflicts are
documented in the `<char>` pseudo-class reference, not silently
resolved.

**Char-specific methods** (not on `<string>` or `<int>`):

```qore
int    <char>::ord()                # codepoint as int (alias for int lift)
bool   <char>::charp()              # True
bool   <char>::isAscii()            # codepoint in 0..0x7F
bool   <char>::isLetter()           # Unicode letter category
bool   <char>::isDigit()            # Unicode digit category
bool   <char>::isAlpha()            # alphabetic
bool   <char>::isWhitespace()       # Unicode whitespace
bool   <char>::isUpper() / isLower()
int    <char>::utf8ByteLen()        # 1..4 — bytes if encoded as UTF-8
int    <char>::utf16CodeUnits()     # 1 or 2 — UTF-16 code units (surrogates)
```

`upr()` / `lwr()` deserve a separate note — see §3.5.

### 3.5 Case mapping and locale awareness

Unicode case mapping isn't a clean per-codepoint operation in the
general case:

- **One-to-many**: German `ß` (U+00DF) uppercases to two characters
  `SS`. A single `<char>::upr()` returning `char` cannot represent this.
- **Locale-dependent**: Turkish `i` (U+0069) uppercases to `İ` (U+0130)
  in `tr_TR` locale, to plain `I` (U+0049) in most others. A pure
  function over codepoints cannot capture this.

Two alternatives:

**A. `<char>::upr()` returning `char` only handles the simple cases.**
For codepoints with one-to-one case mapping (the vast majority — basic
Latin, Greek, Cyrillic, etc.), it returns the mapped char. For
codepoints whose case mapping is one-to-many (`ß`, `ﬁ` ligature, etc.),
it returns the char unchanged. Programmers needing full case mapping
use `<string>::upr()` (which already handles the multi-char case via
string output).

**B. `<char>::upr()` returns `string`** (always, even for the simple
cases). Type-inconvenient but always correct. Forces the caller to
materialize a string for what's usually a single char.

Recommendation: **A** with documentation. The simple-case-only behaviour
matches what tokenizer code overwhelmingly needs (`c.upr() == c'A'` for
ASCII-anchored work), and the rare correctness-sensitive usage already
goes through string operations.

Locale awareness: case-mapping methods take an optional `*string locale`
parameter; absent it, default to the C/POSIX locale (Turkish-I problem
visible only when explicitly opted in).

### 3.6 char has no encoding

A `char` is a Unicode codepoint, full stop. It carries no encoding tag.
This is the right model for several reasons:

- A codepoint is just a number (e.g., U+00E9 = 233). The bytes it would
  occupy in any particular encoding are an output formatting choice, not a
  property of the value itself.
- It eliminates an entire class of "what encoding is this char in?" bugs.
  `char c0 = utf8_string[0]; char c1 = utf16_string[0]; c0 == c1` compares
  codepoints, which is encoding-independent.
- When a char is materialized as a standalone string (via assignment to a
  new `string`, return-from-function, list/hash insertion, or
  `c.toString()`), the default encoding is **UTF-8**. To produce a
  different encoding, use `c.toString(encoding)` explicitly.
- When a char is appended to or concatenated with an existing string, it is
  encoded into the target string's encoding before the existing string
  concatenation rules run. If the target encoding cannot represent the
  codepoint, the operation raises the same encoding-conversion exception as
  an explicit string conversion would.

The codepoint-offset cache (§5) makes repeated `string[i]` access avoid
rescanning from byte 0 for any source encoding *internally*, but the `char`
it returns is just a codepoint — the encoding awareness is handled by the
runtime, not exposed in the value.

For tokenizer migration, this means:

```qore
# Before (the qlib migrations on bugfix/text_processing all do this)
input = text.encoding() == "UTF-8" ? text : convert_encoding(text, "UTF-8");
int b = input.getByte(pos);

# After — convert_encoding becomes unnecessary
char c = text[pos];   # codepoint regardless of text's encoding
```

---

## 4. The `string[i]` indexing change

Today `string::operator[]` returns `*string` (a 1-character string or
NOTHING). Under this design it returns `*char`.

Backward compatibility:

- `c == "#"`, `value += c`, `string s = c;` all continue to compile because
  the implicit `char → string` conversion makes a `*char` usable wherever
  a `*string` was. The runtime cost of the conversion is one inline branch
  (`isChar()` check) plus the existing 1-char string materialization on
  demand — **strictly faster** than today's per-call allocation.
- `*string c = input[pos];` declarations continue to compile. The `*char`
  RHS lifts to `*string`. A compile-time hint can suggest changing the
  declaration to `*char` for performance, but it isn't required.
- Method overload resolution in user code that has `foo(*string)` and
  `foo(*char)` overloads picks the `*char` form for `foo(input[pos])`. This
  is the only case where existing code might observe a behavioral change,
  and it's almost always what the user wants.

Without this change, `char` is half-useful — programmers would have to type
`char c = (char)input[pos];` everywhere.

---

## 5. Runtime implementation

### 5.1 `QoreValue` encoding

`QoreValue` already uses NaN-boxed inline encoding with 16-bit tags
(see `include/qore/QoreValue.h:286-297`). A new tag slot:

```cpp
static constexpr uint64_t TAG_CHAR = 0xFFFE000000000000ULL;
```

Payload layout: low 21 bits hold the codepoint (Unicode max U+10FFFF fits in
21 bits), upper bits zero. No allocation; `QoreValue` carrying a `char`
behaves the same as one carrying an `int48` from the user's perspective.

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
```

`qore_type_t NT_CHAR = 51` is added to `node_types.h` for the rare paths
that switch on type tag.

### 5.2 char → string lift

When a `char` is consumed by a string-typed sink (assignment, concat,
parameter passing), the runtime materializes a 1-character buffer for the
codepoint. Standalone materialization uses UTF-8 by default; append/concat
uses the target string's encoding as described in §3.6. The materialization
happens on-demand in `QoreValue::getStringNode()` (or a new `getString()`
accessor for non-allocating callers).

For most consumers (concat, comparison, sprintf-style formatting) the
1-character string can live on the stack — only escapes (assignment to a
`string` lvalue, return-from-function, list/hash insertion) require a
heap-allocated `QoreStringNode`.

### 5.3 Encoding-aware indexing — the codepoint-offset cache

`string::operator[]` returning `*char` must not walk from byte 0 on every
access. Today `qore_string_private::operator[]` does that for multibyte
encodings, which is the O(N²) cost the byte-indexed migrations work around.

Fix: a per-string **cached codepoint-offset index** in
`qore_string_private`:

- For pure-ASCII strings (detected by an existing flag during hash/compare
  operations), byte offset == codepoint offset, no table needed.
- For small non-ASCII strings, a dense byte-offset table can be built lazily
  on first indexed access. Access is O(1) after the table is built; the
  first build is O(N).
- For large non-ASCII strings, use a chunked index (for example one
  checkpoint every 64 codepoints, with optional dense pages for hot
  regions). Access is bounded by the checkpoint stride instead of the full
  string length, so repeated scans stay linear without requiring a
  multi-gigabyte offset table.
- For other encodings (UTF-16LE/BE, Latin-1, etc.), the same index stores
  source-byte offsets. The runtime no longer needs the
  `convert_encoding(text, "UTF-8")` step that the migrated tokenizers
  perform today.

Cache invalidation: any mutation that changes the byte representation
(splice, replace, in-place concat) bumps a version counter on
`qore_string_private`. The affected dense table or chunked pages rebuild
lazily on the next indexed access.

Memory cost is proportional to the chosen index representation. A dense
table costs 4 bytes per codepoint while byte offsets fit in 32 bits (8 for
larger strings), so it must be capped or avoided for very large strings.
Mitigations:

- Built lazily; strings that are never indexed pay nothing.
- ASCII-only strings skip the cache entirely.
- Large strings use the chunked representation instead of a full dense
  table.
- The cache is per-string-instance; copies share the underlying buffer +
  cache via the existing shared-string mechanism.

---

## 6. Compiler implementation

### 6.1 Lexer (`lib/scanner.lpp`)

- New rule recognizing `c'..'` / `c"..."` — reuses the existing string
  literal machinery, then validates that the decoded content is exactly
  one codepoint. If it is, emit `QCHAR` with `yylval->charcode = cp`. If
  not, emit a parse error.
- Add `\u{HHHHHH}` brace escape for codepoints above U+FFFF (the existing
  `\uHHHH` form covers BMP only).

### 6.2 Parser (`lib/parser.ypp`)

- New grammar production `char_literal` returning a `QoreCharNode`
  (parse-time node that lowers to an inline `QoreValue` carrying TAG_CHAR).
- The `char` keyword as a type name in declarations and casts. Existing
  `string`/`int`/etc. machinery covers the rest.

### 6.3 Type checker (`lib/QoreType.cpp` and friends)

- Add `charTypeInfo` and `charOrNothingTypeInfo` (matching the
  `intTypeInfo`/`intOrNothingTypeInfo` pattern).
- `charTypeInfo` is string-compatible for assignment/comparison contexts
  and has explicit / operator-specific codepoint conversion for numeric
  operations. It is **not** a general subtype of `intTypeInfo` for
  assignment or return-type compatibility.
- For the indexing change, `<string>::operator[]` (in
  `lib/Pseudo_QC_String.qpp`) declares return type `*char`.

---

## 7. Migration & backward compatibility

### 7.1 Existing code

The `c'..'` literal prefix means existing single-quoted literals (`'a'`,
`'foo'`, etc.) keep their meaning as strings — no source migration needed.

Existing patterns that use `*string c = input[pos]` continue to compile
because of the implicit `char → string` lift. A `qore --warn` flag suggests
changing the declaration to `*char` for the perf gain, but does not error.

The qlib byte-indexed migrations (`Qdx::QoreTokenizer`, `DpqlTokenizer`,
`Util::stripAnsi`, `Util::scan_exp`, `DpqlCompletionProvider`) become
*simplifications* once `char` ships:

```qore
# Before (after the byte-indexed migration on this branch)
input = text.encoding() == "UTF-8" ? text : convert_encoding(text, "UTF-8");
int b = input.getByte(pos);
if (b == ord("#")) { comments++; ... }
value += chr(b);

# After (using char)
char c = input[pos];
if (c == c'#') { comments++; ... }
value += c;
```

Same asymptotic scanning behavior as the byte-indexed version (the runtime
index avoids rescanning from byte 0), materially better readability, no
manual UTF-8 normalization at entry.

### 7.2 Parse options — per-keyword opt-out for backward compatibility

Every new keyword and lexer construct introduced by this proposal gets
a matching parse option that disables it for older sources:

| Parse directive | Parse option flag (C++) | Disables |
|---|---|---|
| `%no-char-type` | `PO_NO_CHAR_TYPE` | `char` type keyword in declarations / casts; `c'..'`/`c"..."` literal prefix |
| `%no-string-index-char` | `PO_NO_STRING_INDEX_CHAR` | `string[i]` returning `*char`; parser/type/runtime keep string indexing returning `*string` for compatibility with older code |

The first opt-out covers programs that have an identifier named `char`
(rare; `char` is currently not a Qore keyword, so it's possible). The
second covers programs that depended on `string[i]` returning a
`*string` for type-introspection reasons (code calling
`(string[i]).typeCode()` that expected `NT_STRING`).

In the common case neither opt-out is needed — the implicit
`char → string` lift makes existing code work unchanged.

The flag values share the parse-option bit space documented in
`include/qore/qore_program_options.h`.

### 7.3 Library deprecations

Nothing is removed in 2.3. The byte primitives stay useful for byte-oriented
parsers (HTTP, MIME). `<string>::splitChars()` and
`<string>::codePointIterator()` stay; they're documented as "for explicit
streaming" alongside the simpler `string[i]` form.

---

## 8. Performance projections

Numbers anchored against measurements from `bugfix/text_processing`:

| Workload                                | 2.2 (pre-branch) | 2.3 today (byte-indexed) | 2.3 + char |
|---|---|---|---|
| Qdx tokenize 88 KB                      | 5041 ms          | 1054 ms                  | ~850 ms    |
| stripAnsi 84 KB                         | 1834 ms          |  172 ms                  | ~150 ms    |
| scan_exp 125 KB                         | 4714 ms          | 1166 ms                  | ~1000 ms   |
| QoreCodeCompletion (deferred from this branch) | (unmigrated) | (unmigrated)        | migratable |

The "+ char" column is modest because the byte-indexed migrations on the
current branch already paid the per-element cost; `char` removes the `chr()`
/ `ord()` overhead but the dominant cost in those loops is the loop body
itself.

The bigger win of `char` is **migrations we declined to do** because the
byte path was too risky:

- `qlib/QoreCodeCompletion/CompletionEngine.qc` — char/byte/substr mix,
  reverted from this branch
- Tokenizers expected to handle UTF-16/UTF-32 source without the per-call
  conversion cost
- Regex-heavy tokenizers where the byte path collides with PCRE2's
  codepoint matching

These all become straightforward with `char` indexing: codepoint access
without rescanning from byte 0, encoding-correct by construction, no
`convert_encoding` at entry.

---

## 9. Risks and tradeoffs

### 9.1 Type checker complexity

`char` having string-compatible behavior plus numeric operator lifts is
unusual — most languages would require an explicit cast in one direction.
The motivation is ergonomics: tokenizer code reads naturally if
`c == c'#'` and `c + 1` both work without ceremony, while assignment and
return types still distinguish `char` from `int`.

Risk: subtle type-inference edge cases. Mitigation: prototype the type
rules against the existing test suite before committing to public API.
The existing test suite is a substantial benchmark of the type system.

### 9.2 Codepoint cache memory cost

Documented in §5.3. Lazy + ASCII-fast-path + shared-string-aware. Dense
offset tables can be large, so the implementation must cap dense caches
and use the chunked representation for large strings.

### 9.3 Standard library churn

`<char>` pseudo-class is new (~10 methods); `<string>::operator[]` return
type changes. Introspection tools (qore-doc, QLS, astparser) need updates
for the new type and the indexing change. Estimated 1–2 weeks of follow-up
after the language changes settle.

### 9.4 Tooling / IDEs

Syntax highlighters need to recognize `c'..'` / `c"..."`. The `c` prefix
collides with no existing Qore syntax (Qore has no other `<letter>'..'`
forms today), so the lexer rule is unambiguous.

### 9.5 Regex matching on char

`c =~ /pattern/` requires lifting char to a length-1 string for the
PCRE2 engine to consume it. The materialization is a small stack
buffer (1–4 bytes for UTF-8), one allocation if the result escapes —
strictly cheaper than today's `*string c = str[i]; c =~ /.../`. Code
reviewers should be aware that regex on a char is *slightly* slower
than the codepoint-comparison alternatives (`c == c'a'`, `c.isLetter()`)
even though it's faster than today.

### 9.6 Auto-typed indexing change

`auto x = s[0];` continues to compile (x is `*char` instead of `*string`)
and `x.size()`, `x.toString()`, etc. continue to work because of the
pseudo-method availability from §3.4. The one observable change is in
explicit type queries: `x.typeCode() == NT_STRING` evaluates True today,
False after the change (it returns NT_CHAR). Code that introspects
type codes from indexed string access has to update — minor, but
worth a release-note entry.

---

## 10. Effort estimate

| Component | Estimate (sequential) |
|---|---|
| Lexer changes (`c'..'` literals, `\u{...}` brace escape) | 3-4 days |
| Parser changes (char_literal production, `char` type keyword) | 4-5 days |
| Type checker (charTypeInfo, string-compatible + numeric-lift rules, parse-time string→char rejection, indexing return-type change) | 2 weeks |
| QoreValue encoding (TAG_CHAR + accessors + lift-to-string path) | 1 week |
| Codepoint-offset cache for `string::operator[]` | 2 weeks |
| `<char>` pseudo-class with explicit string-view and codepoint-view methods | 1 week |
| Char-specific methods (`isLetter` etc., case mapping, encoding-targeted toString) | 4 days |
| Test coverage (qtest suite for char + indexing change + method availability) | 2 weeks |
| Doc updates (Doxygen, language guide, release notes) | 4 days |
| **Total (sequential)** | **~10-12 weeks** |
| **Total (with parallelism, 2 developers)** | **~6-8 weeks** |

The numbers above assume serial work; the QoreValue/lexer/runtime
pieces parallelize well with the type-checker/pseudo-class work, so
two developers reach the lower bound.

Independent of the iterate/pipe proposal — `char` can ship without it.

---

## 11. Open questions

1. **Method-name conflict resolution for the rare ambiguous cases.**
   §3.4 picks string-like or int-like for each conflicting method
   explicitly. Two cases that warrant a second look:
   - `<char>::format()` — string-style or int-style? Recommend
     string-style for consistency with concatenation behaviour, but
     int-style (printf-format with `%c` etc.) is plausible. Decide
     before public release.
   - Type predicates such as `intp()` / `stringp()` should continue to
     describe the concrete value type. Recommendation: add `charp()` and
     make `intp()` / `stringp()` return False for `char`; callers that want
     the converted views can use `c.ord().intp()` or `c.toString().stringp()`.

2. **`<char>` Unicode tables — built-in or via the `unicode` module?**
   The `isLetter` / `isDigit` / `isUpper` / etc. methods need Unicode
   character class data (~30-50 KB compressed). Two options: bundle into
   libqore (always available, larger binary) or provide via an optional
   `unicode` module (smaller core, requires `%requires unicode` for the
   methods). Recommendation: bundle the basic ASCII-aware versions in
   libqore (handles 99 % of tokenizer needs); full Unicode tables in the
   module.

3. **Char arithmetic safety — wrap or saturate?**
   `(char)(c'\u{10FFFF}' + 1)` — does it produce U+110000 (invalid),
   throw, or saturate at U+10FFFF? Recommendation: throw
   `INVALID-CODEPOINT` from the explicit cast; `c + 1` (which yields
   `int`, not `char`) doesn't need the check.

4. **Should `<char>::toString()` cache per-codepoint?**
   For tokenizers that build large output strings byte-by-byte via
   `value += c`, the per-iteration string materialization could be
   amortized if the runtime caches the most recent N codepoint→bytes
   conversions. Probably not worth it; the materialization is a tiny
   memcpy and the concat path can be optimized to write codepoint bytes
   directly into the target string buffer. Mark as "implement straight,
   profile, optimize if needed."

---

## 12. References

- `bugfix/text_processing` commits `8165b217b`–`bb1ea2d96` — the qlib
  byte-indexed migrations whose ergonomic cost motivates this design
- `include/qore/QoreValue.h` — NaN-boxed value layout
- `include/qore/intern/qore_string_private.h` — string private state where
  the codepoint-offset cache lives
- `lib/parser.ypp`, `lib/scanner.lpp` — parser/lexer touch points
- [`streaming-operators.md`](streaming-operators.md) — independent
  proposal for new keyword operators + chain fusion (recommended for 2.3
  alongside this proposal)
- [`pipe-operator.md`](pipe-operator.md) — separate proposal for
  pipe-style sugar (recommended deferred)
- `design/dgc.md` — memory-correctness invariants the lift-to-string path
  must honour
