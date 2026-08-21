# Unicode Case Mapping

**Status:** Implemented.

**Target:** Qore 3.0.

## Summary

`<string>::lwr()`, `<string>::upr()`, `tolower()`, `toupper()`,
`QoreString::tolwr()`, `QoreString::toupr()` and the regex substitution case
operators (`\U`, `\L`, `\u`, `\l`) apply the **Unicode default full case
mappings**. `<char>::lwr()` and `<char>::upr()` apply the **Unicode simple
(1-to-1) case mappings** and return a `char`.

The mapping data covers the entire Unicode range and is generated from the
Unicode Character Database (UCD); it is not hand-maintained.

## Simple vs full mappings

Unicode defines two case mappings per direction:

| Mapping | Source | Result | Used by |
|---|---|---|---|
| Simple | `UnicodeData.txt` fields 12 (uppercase) and 13 (lowercase) | always exactly one codepoint | `<char>::lwr()`, `<char>::upr()` |
| Full | `SpecialCasing.txt`, falling back to the simple mapping | one to three codepoints, may be context-sensitive | `<string>::lwr()`, `<string>::upr()`, `tolower()`, `toupper()`, `QoreString::tolwr()`, `QoreString::toupr()` |

A `char` holds exactly one codepoint, so `<char>` case methods must use the
simple mappings; this is why `c'\u{DF}'.upr()` returns `c'\u{DF}'` (U+00DF has
no simple uppercase mapping) while `"\u{DF}".upr()` returns `"SS"`, and why
`c'\u{130}'.lwr()` returns `c'i'` while `"\u{130}".lwr()` returns
`"i\u{307}"`.

Language-specific mappings (Turkish, Azeri, Lithuanian) are **not** applied;
Qore case conversion is locale-independent. The one context-sensitive mapping
that *is* applied is `Final_Sigma`, which is language-independent: a Greek
capital sigma preceded by a cased character and not followed by one lowercases
to U+03C2 instead of U+03C3.

## Generated data

`tools/gen-unicode-case-data.py` reads `UnicodeData.txt`, `SpecialCasing.txt`
and `DerivedCoreProperties.txt` from a UCD release and writes
`include/qore/intern/unicode-case-data.h`, which is committed to the
repository. The build never runs the generator and the UCD files are not a
build dependency.

The generated header defines sorted static arrays searched with binary search:

| Table | Contents |
|---|---|
| `q_simple_lower_map`, `q_simple_upper_map` | every simple 1-to-1 case mapping |
| `q_full_lower_map`, `q_full_upper_map` | every unconditional `SpecialCasing` mapping that differs from the simple mapping |
| `q_cased_ranges`, `q_case_ignorable_ranges` | the `Cased` and `Case_Ignorable` property ranges needed to evaluate `Final_Sigma` |

`QORE_UNICODE_VERSION` records the UCD version the header was generated from.

To move to a new Unicode version, install the new UCD files and run:

```
tools/gen-unicode-case-data.py --ucd <ucd-dir>
```

then rebuild and run `examples/test/qore/vars/string.qtest` and
`examples/test/qore/misc/char-type.qtest`.

## Transformation algorithm

`lib/unicode-charmaps.cpp` holds the engine:

- `apply_case_map()` walks the source string; bytes below 0x80 take an
  ASCII-only fast path with no decoding, and every other character is decoded
  with `QoreString::getUnicodePointFromBytePos()`
- a full mapping is applied if there is one, otherwise the `Final_Sigma`
  condition is checked when lowercasing, otherwise the simple mapping is
  applied; a character with no mapping is copied back verbatim as source bytes,
  so it survives even if it cannot be re-encoded
- the "before C" half of `Final_Sigma` is tracked incrementally as a single
  boolean while walking forward, so no backwards scanning is needed; the
  "after C" half scans forward over case-ignorable characters only
- output is produced with `QoreString::concatUnicode()`, so the result keeps
  the source encoding rather than being forced to UTF-8
- when the source encoding cannot represent a mapping (for example U+00FF maps
  to U+0178, which does not exist in ISO-8859-1), the source character is
  written back unchanged instead of raising `ENCODING-CONVERSION-ERROR`, so
  case conversion is a total function on any valid string.  The check is
  skipped for UTF-8, where it can never fail
- `apply_case_map_measure()` computes the size of the transformed string in
  bytes or characters without materializing it; this backs the JIT/AOT fused
  operations (`qore_rt_pseudo_string_case_measure_native_noguard()`). It is
  exact for UTF-8 and falls back to performing the transformation for other
  encodings. `Final_Sigma` does not need to be evaluated there because U+03C2
  and U+03C3 have the same encoded size

## Known limitation

Case conversion still takes its ASCII fast path on raw bytes, so it does not work
with non-ASCII-compatible encodings such as UTF-16: an ASCII byte of a UTF-16 code
unit is mapped in place and the mapped bytes are written back one byte at a time,
which can produce an odd-length (invalid) UTF-16 byte stream. This is pre-existing
and is shared with `<string>::unaccent()`, which has the same byte-oriented
structure — both carry the `FIXME` in `lib/QoreString.cpp`.

Note that `QoreString::getUnicodePointFromBytePos()` itself decodes `UTF-16BE` and
`UTF-16LE` correctly; the separate `UTF-16` (unsuffixed) encoding is the one that
mis-decodes, because `QCS_UTF16` is registered with the big-endian handler set while
iconv emits a BOM followed by native-endian code units. See
`/tmp/qore-utf16-getunicodepoint.md` for the full analysis.
