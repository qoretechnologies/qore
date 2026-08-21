# `char` Value Type

**Status:** Implemented.

**Target:** Qore 3.0.

## Summary

Qore now has a native `char` value type for a single Unicode codepoint. The
type is stored inline in `QoreValue`, can be used without allocation in string
indexing and string iteration, and converts to a one-codepoint string when a
string context requires it.

This completes the string part of streaming iteration: `iterator <string>` and
`iterate <string>` now yield `char` values. `<string>::codePointIterator()` is
kept as the explicit integer-codepoint iterator.

## Runtime Representation

`char` is a first-class value type:

- `NT_CHAR = 53`
- `TAG16_CHAR = 0xFFFC`
- `TAG_CHAR` identifies inline char values in `QoreValue`
- valid payload range: `0..0x10FFFF`

The value stores a Unicode codepoint and no encoding. Encoding is applied only
when the value is materialized as a string or appended to a string.

The implementation adds `char`, `*char`, `softchar`, and `*softchar` type
information. `softchar` accepts `char`, an integer in the Unicode codepoint
range, or a string containing exactly one codepoint. Plain `char` assignment
remains narrow: string and integer inputs need an explicit cast or `softchar`.

## Syntax

Char literals use a `c` prefix:

```qore
char a = c'a';
char newline = c"\n";
char apostrophe = c"'";
char smile = c'\u{1F600}';
```

The decoded literal must contain exactly one codepoint. The `\u{...}` brace
escape is supported in normal strings and char literals for codepoints that are
awkward to express with fixed-width escapes.

For source compatibility, `%no-char-type` disables the builtin `char` type name
and `c'...'` / `c"..."` char literals.

## String Indexing

Single integer string indexing now returns `*char`:

```qore
string s = "cafe";
char c = s[0];       # c'c'
*char missing = s[9]; # NOTHING
```

Range and list indexing still return strings:

```qore
string first_two = s[0..1];
string selected = s[0, 2..3];
```

`%no-string-index-char` preserves the historical single-index behavior where
`string[i]` returns `*string`.

The current implementation uses the existing encoding-aware string traversal
paths. It does not add a persistent codepoint-offset cache to `QoreStringNode`;
therefore repeated random access on multibyte encodings follows the existing
indexing complexity profile. The main allocation win is that successful
single-character access returns an inline `char` instead of allocating a
one-character string.

## String Iteration

`<string>::iterator()` and `iterate <string>` yield `char` values:

```qore
list<char> chars = map $1, iterate "abc";
list<int> cps = map $1.ord(), iterate "abc"; # (97, 98, 99)
```

`<string>::codePointIterator()` remains available and yields integer
codepoints:

```qore
list<int> cps = map $1, "abc".codePointIterator();
```

The runtime checks cancellation during string iteration every 100 codepoints,
matching the cooperative-cancellation policy for tight loops.

## Operators and Conversion

`char` participates in the expected string, comparison, and arithmetic contexts:

```qore
char c = c'a';

assertEq(c'a', c);
assertTrue(c == "a");
assertTrue(c == 97);
assertTrue(c >= c'a' && c <= c'z');

string s = "x" + c + "y"; # "xay"
int next = c + 1;          # 98
char d = cast<char>(next); # c'b'
```

Arithmetic treats the char as its codepoint and returns integer results.
Soft comparison treats a `char` as compatible with an integer codepoint or a
one-codepoint string; exact comparisons still preserve the distinct `char`
runtime type. String concatenation materializes the char as a one-codepoint string.
Conversions validate that integer codepoints are in range and that string
inputs contain exactly one codepoint.

The `<char>` pseudo-class exposes codepoint and string-view methods, including:

- `ord()` / `typeCode()` for numeric and type metadata
- `toString()` for explicit string materialization
- `size()`, `length()`, and `utf8ByteLen()` for string-view size queries
- `lwr()` and `upr()`, returning chars; these apply the Unicode *simple* (1-to-1)
  case mappings, which are always single codepoints.  `<string>::lwr()` and
  `<string>::upr()` apply the Unicode *full* case mappings instead, which are
  context-sensitive and can expand one codepoint into up to three codepoints
  (for example `"\u{DF}".upr()` is `"SS"` while `c'\u{DF}'.upr()` is `c'\u{DF}'`,
  since `\u{DF}` has no simple upper-case mapping)
- `charp()`, `intp()`, `strp()`, and `val()`

## Parser and Tooling

The core bison/flex parser supports the new literal syntax, parse directives,
and type names. The astparser bison/flex parser mirrors the same syntax and
parse options.

The tree-sitter grammar in `modules/astparser/grammars/tree-sitter-qore` is
aligned with:

- `char` and `softchar` builtin type names
- `char_literal`
- brace Unicode escapes
- the `%no-char-type` and `%no-string-index-char` directives

`QoreCodeFormat` recognizes both parse options.

## Compiler, AOT, and JIT

The compiler and runtime paths were updated for `char` values:

- `QoreValue` type, equality, string conversion, and type-name handling
- parse-time and runtime type info for `char` and `softchar`
- string indexing in AST, IR, JIT runtime, and generated LLVM paths
- `iterate <string>` return typing and runtime iteration
- AOT serialization/deserialization with binary format version 6
- builtin type metadata exposed through `qc_qore`

Streaming string iteration and char-aware string indexing are part of the full
implementation; no intermediate feature target is kept.

## Documentation and Tests

User-facing behavior is documented in:

- `design/streaming-operators.md`
- `doxygen/lang/145_basic_data_types.dox.tmpl`
- `doxygen/lang/147_fast_text_processing.dox.tmpl`
- `doxygen/lang/155_data_type_declarations.dox.tmpl`
- `doxygen/lang/180_operators.dox.tmpl`
- `doxygen/lang/245_parse_directives.dox.tmpl`
- `doxygen/lang/900_release_notes.dox.tmpl`

Focused tests are in:

- `examples/test/qore/misc/char-type.qtest`
- `examples/test/qore/misc/streaming-operators.qtest`
