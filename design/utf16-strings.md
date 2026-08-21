# UTF-16 Strings

**Status:** Implemented.

**Target:** Qore 3.0.

## Summary

A `QoreString` is a byte buffer plus an encoding tag; the encoding's registered handler functions are
the only thing that says how those bytes decode. UTF-16 is the one encoding %Qore supports that is
neither ASCII-compatible nor self-synchronizing, so it is the only one where byte-oriented string
code silently produces wrong results rather than merely inefficient ones.

This document defines the two invariants that make UTF-16 work and lists the code that has to agree
on them.

## Invariant 1: `UTF-16` means big-endian with no BOM

`QCS_UTF16` — the byte-order-neutral `"UTF-16"` encoding — is registered in `lib/charset.cpp` with
the **big-endian** handler set (`UTF16BE_getLength()`, `UTF16BE_getUnicode()`, and so on):

```cpp
QCS_UTF16 = addUnlocked("UTF-16", "variable-width universal character set", 2, 4, UTF16BE_getLength,
    UTF16BE_getByteLen, UTF16BE_getCharPos, q_UTF16BE_get_char_len, UTF16BE_getUnicode, false);
```

That is correct per the Unicode standard: a UTF-16 stream with no byte order mark is big-endian.
The invariant it imposes is:

> **The bytes of a string tagged `QCS_UTF16` are big-endian code units and contain no byte order
> mark.**

Two places have to uphold it.

### `IconvHelper` must not ask iconv for `"UTF-16"`

iconv's `"UTF-16"` conversion produces a **BOM followed by native-endian code units** — the opposite
of the invariant on a little-endian host. `IconvHelper::getIconvCode()` therefore substitutes
`"UTF-16BE"` whenever the %Qore encoding is `QCS_UTF16`, in both directions. Every conversion in the
library funnels through `IconvHelper`, so this one substitution covers `convert_encoding()`,
`QoreString::convertEncoding()`, `qore_string_private::concatUnicode()`,
`EncodingConversionInputStream`, `StreamWriter`, and everything built on them.

Without it, `convert_encoding("hé", "UTF-16")` returned six bytes (`ff fe 68 00 e9 00`) that decoded
as U+FFFE followed by two byte-swapped CJK ideographs: `length()` was 3 and `"hé"[0].ord()` was
65534.

### Byte order marks in external data are resolved by retagging

Data that enters %Qore from outside may carry a BOM and may be little-endian.
`q_remove_bom_utf16()` (`lib/QoreLib.cpp`) strips the mark and, for `QCS_UTF16`, **retags the string
as `QCS_UTF16BE` or `QCS_UTF16LE`** according to the mark found. The byte order is carried on the
string by its encoding pointer; the shared `QoreEncoding` objects hold fixed handler pointers and
cannot switch endianness per string.

It is applied by `binary_to_string()`, all three of `q_read_string_all()`, `q_read_string()` and
`q_read_string_short()` (which back `StreamReader::readString()` and `File::readString()`),
`StreamReader::readLineEol()`, `InputStreamLineIterator`, and `TempEncodingHelper::removeBom()`.
Any new path that turns externally-supplied bytes into a UTF-16-tagged string must call it too — the
read-everything path used to be the one that did not, so `readString(-1)` kept the mark and decoded
little-endian data as big-endian while `readString(n)` on the same bytes did not.

## Invariant 2: byte-oriented code must consult the encoding

Two properties of UTF-16 break assumptions that hold for every other supported encoding.

| Property | Consequence |
|---|---|
| Not ASCII-compatible: a code unit is two bytes, either of which may be `0x00` | a byte with the high bit clear is **not** a whole ASCII character; `strlen()`, `strncmp()`, `strncpy()`, `strstr()` and `while (*p)` all stop at the null half of a code unit |
| Not self-synchronizing: the low byte of one code unit and the high byte of the next can form the byte sequence of a different character | a byte-oriented substring search can report a match that is not at a character boundary |

### ASCII fast paths

Code that special-cases `!(byte & 0x80)` must gate that path on
`QoreEncoding::isAsciiCompat()` and fall through to the decode path otherwise. `QCS_UTF16*` are all
registered with `ascii_compat = false`, so the predicate is already correct — it just has to be
consulted. `apply_case_map()`, `apply_case_map_measure()` and `apply_unicode_charmap()`
(`lib/unicode-charmaps.cpp`) do this; their replacement text is concatenated with
`QoreString::concatUnicode()` or via a `QCS_USASCII` temporary rather than as raw bytes.

Where an operation is defined in terms of ASCII bytes and has no meaningful UTF-16 form — entity
encoding and decoding, URI encoding, path matching — the correct behavior is to raise
`UNSUPPORTED-ENCODING`, not to produce corrupt output.

### Null-terminated APIs

Never derive a needle's length with `strlen()` from a `QoreString`'s buffer, and never compare or
copy with the `str*` functions. `QoreString::startsWith()` and `QoreString::endsWith()` have
`const QoreString&` overloads that carry the byte length; `QoreString::bindex(const QoreString&)`
serves the same purpose for substring searches. The `const char*` overloads remain for genuinely
NUL-terminated C strings and are documented as unusable with UTF-16.

The failure mode is not a wrong answer in one direction: `strlen()` of big-endian UTF-16 data that
starts with an ASCII character is **zero**, so a zero-length comparison succeeds and *every*
argument matches.

### Character-boundary matching

Substring searches only match at character boundaries.
`qore_string_private::get_char_alignment()` returns the encoding's minimum character width — 2 for
UTF-16, 1 for everything else — and `memmem_aligned()` / `memrmem_aligned()` skip matches whose byte
offset is not a multiple of it. Because UTF-16 characters are two or four bytes, a character always
starts at an even byte offset from the start of the string.

With `align == 1` both helpers delegate straight to `q_memmem()`/`q_memrmem()`, so UTF-8 and
single-byte encodings are unaffected in behavior and cost.

The search sites that apply it are `index_simple()` and `rindex_simple()` (which back `index()`,
`rindex()`, `bindex()`, `brindex()`, `find()`, `rfind()` and `<string>::contains()`), `memstr()` in
`lib/ql_string.qpp` (which backs `split_intern()` and `split_with_quote()`), the `replace()` loop,
`StringSplitIterator`, and `<string>::getLine()`.

Without the constraint, `"x\u{100}\u{A41}y"` in UTF-16BE contains the byte pair that encodes `"\n"`
straddling the boundary between U+0100 and U+0A41: `split()` split a string that has no separator in
it, and `replace()` turned U+0A41 into U+5A41.

## What is not decoded

Regular expression operators and functions convert UTF-16 strings to UTF-8 before matching, so the
strings they return are tagged `UTF-8` rather than with the source encoding.

`<string>::getLine()` and the other APIs documented as taking byte offsets keep byte semantics for
their offset and count arguments; only their matching is character-aware.

## Tests

`examples/test/qore/vars/string.qtest`:

- `testUtf16Transforms()` — byte layout, `length()`, `ord()`, `splitChars()`, case mapping,
  `unaccent()`, surrogate pairs, the `Final_Sigma` condition, `reverse()`, `chomp()`, `replace()`,
  the string predicates, BOM resolution, and character-boundary matching, across all three UTF-16
  spellings
- `testGetLine()` — `<string>::getLine()` with automatic and explicit separators, embedded nulls,
  and UTF-16
- `testUtf16()` — comparison, `substr()`, and trim operators

## See also

- `design/unicode-case-mapping.md` — the case mapping data these transforms apply
- `doxygen/lang/170_character_encoding.dox.tmpl` — the user-facing documentation
