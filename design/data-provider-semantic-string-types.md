# DataProvider Semantic String Types

## Overview

The DataProvider module defines a family of **string-backed semantic types** that carry a JSON-Schema
`format`-style identity (`email`, `uri`, `uuid`, `hostname`, `ipv4`, `ipv6`, `phone`). They let a data
provider, an app-action option, or an auto-derived API schema declare *what a string means* so that user
interfaces can render an appropriate input control, while the wire/storage representation stays a plain
string.

## Design

- **One parameterized class** — `qlib/DataProvider/QoreStringFormatDataType.qc`
  (`inherits QoreStringDataTypeBase`). Each format is a registered instance (a `public const`), not a
  per-format subclass, so adding a format is a one-line registration.
- **String-backed identity.** `getValueType()` is `StringType`, so `getBaseTypeName()` returns `"string"` —
  serialization, mappers, and value handling are unchanged from a plain string. The semantic identity is
  carried by `getName()` (e.g. `"email"`) and the `qore.external_name` tag (also `"email"`), which is what a
  front end keys off to choose a widget. A permissive validation regex is additionally published in the
  `qore.validation_regex` tag.
- **Nullable variants for free.** `getOrNothingType()`/`getMandatoryType()` are inherited from
  `QoreDataType` (copy-mutate, `*`-prefix). Both `email` and `*email` are registered.
- **Server-side validation is a backstop.** `acceptsValue()` validates string input against the format regex
  and throws `RUNTIME-TYPE-ERROR` on failure. This fires **only on input/coercion paths** (request/message
  types, options, search criteria, expression returns); records returned by a provider are never run through
  `acceptsValue()`, so relayed third-party data is unaffected. Regexes are deliberately permissive (strict
  for `uuid`/`ipv4`/`ipv6`; lenient for `email`/`phone`/`hostname`/`uri`); a field that must accept anything
  should use plain `string`.

## Registration

`qlib/DataProvider/AbstractDataProviderType.qc` — `AbstractDataProviderTypeMap` registers each format and its
`*`-nullable sibling. Like `file`/`rgbcolor`, these class-backed types live only in
`AbstractDataProviderTypeMap` (not `DataTypeMap`), so they are referenced by object
(`AbstractDataProviderTypeMap."email"`), not resolved by name through `AbstractDataProviderType::get(string)`.
A `getInfo()`/`get(hash<DataTypeInfo>)` round-trip degrades the reconstructed type to its string base but
preserves the `qore.external_name` tag, so the UI identity survives serialization.

## Schema integration

- **Import (schema → type):** `Swagger.qm` and `OpenApi3.qm` `getTypeIntern()` map `type: string` +
  `format: <email|uri|uuid|hostname|ipv4|ipv6|phone>` to the corresponding semantic type. Previously every
  non-`byte`/`binary`/`date` format collapsed to plain `string`.
- **Generation (type → schema):** `OpenApi3.qm` `OpenApi3TypeMapper::TypeMap` emits
  `{type: string, format: <format>}` for a semantic type. `Swagger.qm` `get_qore_type()` is intentionally
  **not** changed — it returns a Qore *language* type name (used for code generation), where the semantic
  name is not a valid Qore type; the semantic mapping lives at the DataProvider-type layer.
- `date-time` deliberately continues to map to `SoftDateType` (a date, not a string subtype); `password` is a
  UI/masking concern handled by the `sensitive` option, not a data type.

## App-action options

`DataProviderActionCatalog::getActionOptionFromFields()` propagates a field's `ui_type` (and the existing
`validation_regex`/`sensitive`) into the derived `ActionOptionInfo`, so a request/response field typed as a
semantic format surfaces its identity on the app-action option that a UI renders.

## Adding a format

1. Add a `public const <Name>Type`/`<Name>OrNothingType` pair in `QoreStringFormatDataType.qc`.
2. Register `"<name>"`/`"*<name>"` in `AbstractDataProviderTypeMap`.
3. Add `case "<name>":` to `getTypeIntern()` in `Swagger.qm` and `OpenApi3.qm`, and an entry to
   `OpenApi3TypeMapper::TypeMap`.
4. Add `QoreStringFormatDataType.qc.dox.h` is already in `doxygen/qlib/Doxyfile.DataProvider` (one entry
   covers the class); extend `examples/test/qlib/DataProvider/DataProvider.qtest` coverage.
