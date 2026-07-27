# TOON Support in the json Module

## Scope

`make_toon()` / `parse_toon()` add TOON (Token-Oriented Object Notation)
serialization to the json module as a regular supported feature, alongside
`make_json()`/`parse_json()` and `make_cbor()`/`parse_cbor()`.

Implementation target: **TOON specification version 3.0** (Working Draft,
2025-11-24, https://github.com/toon-format/spec), TOON Core Profile (§19) plus
tabular arrays. Source: `src/ql_toon.qpp`, `src/ql_toon.h`.

## Resolved design decisions

1. **Grammar revision**: TOON v3.0. The hand-written recursive-descent parser
   implements §§5–14 (root form, header syntax, strings/keys, objects, arrays,
   objects-as-list-items, delimiters, indentation, strict mode).

2. **Duplicate object keys**: last-value-wins, consistent with `parse_json()`
   (the spec does not reject duplicate keys for plain objects; only the
   optional path-expansion feature defines conflicts, and path expansion is not
   enabled here).

3. **Non-JSON Qore types**: normalized exactly as `make_json()` does so that
   `parse_toon(make_toon(x))` equals `parse_json(make_json(x))`:
   - `date` (absolute) → ISO-8601 datetime string; `date` (relative) →
     ISO-8601 duration string
   - `binary` → base64 string
   - `number` → canonical decimal; non-ordinary (`@nan@`, `@inf@`) → `null`
   - `float` `@nan@`/`@inf@` → `null`
   - object and other non-data types → `TOON-SERIALIZATION-ERROR`
   A consequence of TOON canonical numbers (§2: no exponent, zero fractional
   part emitted as an integer) is that e.g. `1e6` round-trips to integer
   `1000000` — identical to existing `make_json()`/`parse_json()` behavior.

4. **`sort_keys`**: exposed (default `False`). Off preserves hash insertion
   order; on sorts object keys and tabular field names lexically for
   deterministic output.

5. **Benchmarks**: out of scope for this module. Serializer/parser correctness
   lives here (QUnit, valgrind); LLM-effectiveness benchmarking belongs to
   downstream consumers.

6. **`max_inline_string_length` option**: omitted. TOON quoting is fully
   deterministic (§7.2); there are no equivalent alternative string forms to
   control.

## DataFrame / SQL / DataProvider integration

TOON is useful for row-oriented interchange with Qore data APIs because its
tabular-array form compactly represents `list<hash<auto>>` values. This covers
the common shape returned by SQL row APIs, generic DataProvider row blocks, and
`DataFrame::toRecords()`.

The integration boundary is deliberately the row-list data model:

- `make_toon()` accepts row lists such as `df.toRecords()` or SQL/DataProvider
  `list<hash<auto>>` results.
- `parse_toon()` returns plain Qore data; parsed tabular arrays can be passed to
  APIs that accept row lists, including the `DataFrame` constructor.
- `make_toon()` does not serialize `DataFrame` objects directly. This preserves
  parity with `make_json()`, keeps the json module independent of dataframe,
  SQL, and DataProvider modules, and keeps object serialization errors
  predictable.
- TOON is a readable text interchange format, not a dense or zero-copy
  transport. Dense DataFrame buffers, Arrow, Parquet, and DBI/BulkSqlUtil packed
  paths remain the right mechanisms for high-volume in-process or binary data
  movement.

`test/json.qtest` includes an optional dataframe interop case guarded by
`%try-module dataframe`; it verifies the supported conversion path without
making dataframe a required dependency of this module.

## Options

`make_toon(data, *options)`:
- `indent` (int, default 2, range 1–32)
- `tabular_arrays` (bool, default True)
- `sort_keys` (bool, default False)

`parse_toon(toon_str, *options)`:
- `strict` (bool, default True) — enforces TOON §14 strict-mode validation
- `indent` (int, default 2, range 1–32) — indentation width for depth

## Exceptions

- `TOON-SERIALIZATION-ERROR`
- `TOON-PARSE-ERROR` (includes line, and column where available)

These are kept distinct from `JSON-*` and `CBOR-*`.

## Not implemented (spec defaults to "off")

- Key folding (`keyFolding="safe"`) and path expansion (`expandPaths="safe"`),
  §13.4. Dotted keys are single literal keys (the spec default). This may be
  added later without breaking existing output.

## Encoding

TOON is always UTF-8 (spec §1.2, §18.2), which is stricter than JSON (RFC 8259
permits UTF-8/16/32). `make_toon()` always emits a UTF-8 `QoreStringNode`;
`parse_toon()` transcodes the input to UTF-8 via `TempEncodingHelper` before
parsing so a non-UTF-8 source string (e.g. ISO-8859-1) yields correctly
UTF-8-encoded results rather than raw bytes mis-tagged as UTF-8. (This differs
from `parse_json()`, which preserves the source string's encoding; converting
is the spec-correct choice for TOON because the format mandates UTF-8.)

## Sandboxing / cooperative cancellation

No filesystem, network, or blocking operations — `make_toon()`/`parse_toon()`
are pure in-memory transforms, so there is no filesystem/network sandbox
surface to check.

Cooperative cancellation follows `design/cooperative-cancellation.md`:

- a pre-operation `qore_check_cancel()` at the top of `make_toon()` and
  `parse_toon()` (Pattern 1) so a pre-requested interrupt/cancel is honored
  even for tiny inputs;
- a periodic `qore_check_cancel()` every `JSON_INTERRUPT_CHECK_INTERVAL` (100)
  iterations in every container/row/field/item loop and in the O(n)
  `buildLines()` pre-pass (Pattern 5);
- the periodic check is **not** gated on sandbox-manager presence, so
  `cancel_thread()` (`THREAD-CANCELLED`) is honored in non-sandboxed programs
  too; `qore_check_cancel()` provides its own zero-overhead fast path when
  nothing is active.

The `JSON_MAX_NESTING_DEPTH` (256) recursion limit is sandbox-scoped (active
only when a sandbox manager is present), matching `make_json()`/`parse_json()`
and the "sandbox only" semantics documented in `qore-json-module.h`; under a
sandbox this bounds native recursion (DoS protection). Cancellation propagates
as `PROGRAM-INTERRUPTED` (program interrupt) or `THREAD-CANCELLED` (per-thread
cancel).

## Tests

`test/json.qtest` (QUnit): scalar/hash/list round trips, tabular arrays,
optional DataFrame row-list interchange, determinism, options, data-model
normalization, error handling, upstream-spec example fixtures, full round-trip
fixtures, and sandbox depth/interrupt parity with the JSON functions.
