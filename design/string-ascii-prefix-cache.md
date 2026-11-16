# ASCII Prefix Cache of Strings

Issue #6138. A character offset in a UTF-8 string is found by walking the characters before it, so `str[i]`,
`substr()`, `index()` and the other character-offset operations cost O(i), and a loop over the characters of a string
costs O(n^2). Every string therefore caches the byte length of a known ASCII prefix of its text; in UTF-8, every byte
of that prefix is a whole and valid character, so a character offset in the prefix is its byte offset.

## Invariant

`qore_string_private` (`include/qore/intern/qore_string_private.h`) holds a value `prefix` with:

- `prefix <= len`;
- every byte in `[0, prefix)` is `< 0x80`.

The value is a property of the bytes, not of the encoding: it is kept when the encoding of a string changes, but it is
only used when the encoding is UTF-8. In other multi-byte encodings a byte `< 0x80` can be the second byte of a
character (Shift_JIS, GBK, Big5) or half of one (UTF-16), and single-byte encodings do not need it (their code paths
do not read it). 0 is always valid.

## Storage

There is no field of its own: `sizeof(qore_string_private)` is unchanged (72 bytes, an 80-byte malloc chunk; one more
`size_t` would make it 96). The value is in the field that is not used in each state of the string
(`asciiPrefixStorage()`):

| State | Storage | Why the field is free |
|---|---|---|
| owns its storage (`view_parent == nullptr`) | `view_offset` | only a view has an offset in a parent |
| view (`view_parent != nullptr`) | `allocated` | a view has no buffer of its own |

So every reader of `allocated` must first make sure that the string is not a view: `check_char()`, `allocate()`,
`concat(char)`, `vsprintf()` (through `mutableBuffer()`), `swapText()` and `clear()` materialize or abandon the view
first, and `QoreString::capacity()` returns 0 for a view. The state transitions move the value:

- `QoreStringNode::makeView()`: the view starts with the part of the parent's prefix that it contains,
  `min(parent_prefix - offset, byte_len)` (0 if the view starts after it); the parent cannot change while the view
  references it (copy-on-write, `QORE_ASSERT_MUTABLE`);
- `materialize()`: the view's value (in `allocated`) becomes the owned string's value (in `view_offset`): the bytes are
  the same;
- `abandon_view()`: `allocated = 0` and `dec_view_parent()` sets `view_offset = 0`, so both are 0.

## Readers

Reads use `std::atomic_ref<size_t>` with relaxed loads and stores, as a string that is shared read-only is read by
several threads at once. Each thread computes the value from the same immutable bytes, and a reader never depends on
another reader's store: a value it loads is valid whatever thread stored it, and a lost update only costs a scan.
`getAsciiPrefix(b, need)` returns the value if it is at least `need` or `len`; otherwise `extendAsciiPrefix()` scans
from it, a word at a time, until the first byte `>= 0x80` or up to `max(need, 2 * prefix + 64)` bytes, and publishes
the longer prefix. The geometric step makes a loop over the characters scan each byte about once while publishing
O(log n) times, and a single offset in a long string costs no more than the walk that it replaces.

The UTF-8 entry points use three helpers that are equivalent to the encoding functions on the string's own bytes:
`getByteLenFrom(b, start, chars)` (`QoreEncoding::getByteLen(b + start, b + len, chars)`), `getLengthFrom(b, start)`
(`getLength(b + start, b + len)`) and `getCharPosTo(b, end)` (`getCharPos(b, b + end)`). Inside the prefix they
answer from it; after it they walk with the encoding function from the end of the prefix. The UTF-8 functions walk one
character at a time and a byte `< 0x80` is always a character of one byte, so the walk from the end of the prefix
reaches the same position with the same validation of the bytes after it: invalid text raises the same
`INVALID-ENCODING` exception (`raiseInvalidEncoding()`, the text of `QoreEncoding`) and returns the same values. In
another encoding the helpers call the encoding functions as before.

Entry points that use them: `compute_substr_range()` (`QoreStringNode::substr()`, so `str[i]`, `substr()` and
`QoreValue::makeCharFromStringAt()`), `substr_complex()` (`QoreString::substr()`), `QoreString::length()`,
`getCharLength()`, `findByteOffset()` (`index()`, `rindex()`, `concat()` from a character offset), the character
positions of `index()` and `rindex()`, `getByteOffset()`, `getUnicodePoint()`, `getUnicodePointFromUTF8()`,
`splice_complex()`, `trimTrailing()` and the source offsets of `QoreString::concat(const QoreString*, size_t, ...)`.
`replace()` in `ql_string.qpp` still walks with the encoding functions.

`effective_buf_view_path()` is never inlined (`QORE_INTERN_NOINLINE`): with the helpers, GCC inlined it into
`compute_substr_range()`, whose path for a string that is not a view then needed a register for `view_parent` and one
more instruction in single-byte encodings.

A debug build checks the invariant at every read (`checkAsciiPrefix()`): the whole prefix if it is at most 256 bytes
long, its first and last 16 bytes otherwise, so that character offsets stay O(1).

## Mutators

A mutation only happens on a string that no other thread reads. It must keep the invariant:

- appends (`concat*()`, `vsprintf()`, `addch()`, `concatUnicode()`, the encode and escape functions) leave the
  value as it is, as they do not change the bytes before `len`; they do no work for the cache;
- mutations that keep the bytes before an offset lower the value to it (`lowerOwnedAsciiPrefix()`): `splice_simple()`
  (and so `replace()` and `replaceAll()`), `splice_complex()`, `replaceChar()`, `insert()`, `insertch()` and
  `trimTrailing()`;
- every other mutation resets it (`resetOwnedAsciiPrefix()`): `terminate()` (its callers can write the bytes through
  the buffer pointer before they set the length, ex: `reserve()`, `getBuffer()` and `terminate()`, which also covers
  `chomp()` and the trailing trims of single bytes), `take*()`, `giveBuffer()` (and `reset()`), `set*()` (and
  `operator=`), `clear()`, `removeBytes()`, `prepend()`, the leading trims and `removeStatefulPrefix()`;
- `swapText()` swaps the values with the bytes; `concat_reverse()` and the encoding conversions write to an empty
  target, whose value is 0.

A new string has 0 (also a copy, so that the copy constructor does no more work), and so does a string made from
another one by an operation that creates a new string (`substr()` of fewer than 4096 bytes, case conversion, regular
expression substitution, `reverse()`, encoding conversion). Bytes written through the `const char*` of `getBuffer()`
or `c_str()` without a call of `terminate()` are outside the contract of the class ("this data should not be
changed"); the writes of that kind in the library only write ASCII bytes, which keep the invariant.

## Cancellation

The scan does not check for cancellation. It is bounded by the length of the string, like the walks of the encoding
functions it replaces (which do not check either), it covers at most about twice the bytes that the walk it replaces
would have covered, and it is faster than that walk. Cancellation stays at the statements and loops that call it
(`design/cooperative-cancellation.md`).

## Tests

`examples/test/qore/strings/ascii_string_index.qtest` runs:

- `ascii_string_index.cpp` (`qore-ascii-string-index-test`): every mutator of the C++ API (also a trim in a stateful
  encoding, which swaps the text of the string) after the cache is filled
  (not at all, to the middle, to the end), in owned strings and in views (at the end of the parent, in its middle,
  views of views, with the parent's cache filled or not), compared at every character offset, negative and out of
  range offsets included, with the same bytes in a reference encoding that has the UTF-8 functions of the library but
  not the cache; valid and invalid UTF-8; and threads that read new strings at once;
- `ascii-string-index.q` in every execution mode: the mutators of the language, compared with the same text in
  UTF-16LE, text that is not valid UTF-8, and threads;
- `ascii-string-index-scan.q` in every execution mode (debug builds): it counts the bytes that the character offset
  functions walk or scan (`dbg_get_string_scan_bytes()`, `qore_dbg_string_scan_bytes`).  Reading every character of
  a 50,000 and a 200,000 character ASCII string forwards, with `length()` in the loop condition, and backwards scans
  at most twice the length of the string (a quadratic loop walks about n * n / 2 bytes); a read at offset 1,000,
  10,000 or 100,000 of a string whose ASCII text is known, and its `length()`, scan nothing.  With the cache disabled
  (`dbg_set_ascii_prefix_cache()`), the code walks from the start of the string as before the change, and a read at a
  later offset walks more by the difference, which shows that the counter sees an O(i) walk.  The test is
  deterministic: it asserts on counts, not on time.
