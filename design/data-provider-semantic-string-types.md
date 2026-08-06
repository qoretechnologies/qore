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
`validation_regex`/`sensitive`) into the derived `ActionOptionInfo`.

A semantic-typed field surfaces its identity on the derived option through the option's **type**, not through
`ActionOptionInfo::ui_type`: `ui_type` is populated only from an explicit field attribute, and a semantic type
does not set one. A UI reads `option.type.getTag("qore.external_name")` (and `qore.validation_regex`) to pick a
control. Covered by `semanticTypesThroughProviderTest()` in
`examples/test/qlib/DataProvider/DataProvider.qtest`, which asserts both the tag path and that `ui_type` stays
unset — see [Open design questions](#open-design-questions) for whether it should be derived.

## Adoption

195 fields across 37 `qlib/` modules are typed with a semantic format: 103 `uri`, 67 `email`, 15 `phone`,
9 `hostname`, 1 `uuid`. Adoption was driven by review, not by a name-matching sweep — a name-only scan
produces false positives (a Firecrawl field named `link` holds a hex colour; Firecrawl/Instantly fields named
`mobile`/`is_domain` are booleans), so every field was checked against what the API actually carries.

### The rule applied

**Response/record fields** — converted freely. A record returned by a provider is never run through
`acceptsValue()`, so typing one is pure metadata: it gives a UI the format identity with no chance of a new
runtime throw. Fields were still checked for truthfulness, not just converted on a name match.

**Input fields** (request types, and record types reachable from `getCreateRecordTypeImpl()`/
`getUpdateRecordTypeImpl()`) — converted only where the regex cannot reject a value the API would have
accepted. In practice that means:

- `uri` — only where the remote service must **dereference** the URL itself (fetch it, call it back, or
  render it): webhook/callback targets, PDF.co source documents, Firecrawl scrape targets, Bitly `long_url`,
  Dropbox `save_url`, MCP resource URIs. A scheme is required for those calls to work at all.
- `email` — singular address fields on create/update/enrich operations, where the value is definitionally one
  address.
- `hostname` — bare-domain fields (`apollo.io`, `bit.ly`) whose APIs take a bare domain.

### What was deliberately left as `string`

Leaving a field as `string` is the correct outcome when the value is legitimately looser than the regex:

| Left as `string` | Count | Why |
|------------------|-------|-----|
| Human-entered URL fields (CRM "website", social-profile URLs, a BigCommerce blog `url` **slug**) | 17 | `uri` requires a scheme; `www.example.com` is a legitimate thing for a user to type |
| Phone input fields | 31 | the regex rejects extensions (`555-1234 x89`) and free-text placeholders these APIs accept |
| Bitly `guid`/`group_guid`/`organization_guid` | 13 | Bitly GUIDs are opaque base62 ids (`Bg7ay8wPvXW`), not RFC-4122 |
| Elasticsearch/OpenSearch `index_uuid` | 2 | base64url ids; the in-repo `example_value` `"aXNkZXhfMQ"` fails the RFC-4122 regex |
| A `link` authored into a generated PDF | 1 | PDF.co never dereferences it, so it may hold anything |
| Fields with no documented format (ClickFunnels/EmpathicBuilding `uuid`) | 2 | no evidence either way |
| Gorgias `uri` fields | 4 | hold a relative path (`/api/...`), not a URI |
| Firecrawl `link` | 1 | holds a hex colour value |
| `SoftString*`-declared fields | 73 | see below |

**`SoftString*` fields are never converted.** The semantic types are backed by `StringType`, and there is no
soft variant, so replacing `SoftStringOrNothingType` with `UriOrNothingType` would silently drop the field's
coercion of non-string input — and `Mapper::mapFieldType()` *does* call `acceptsValue()` when writing a field,
so that would be a live behavior change, not just metadata.

### Verification

- Every conversion was checked at runtime by instantiating the type and asserting the field's
  `qore.external_name` tag, not by reading the diff.
- `example_value`, `default_value`, and `allowed_values` are validated by `acceptsValue()` at
  **field-construction** time (`QoreDataField.qc`, `AbstractDataField::setDefaultValue()`/
  `setAllowedValues()`), so a conversion inconsistent with the module's own documented example fails loudly
  when the type is built. All 55 in-repo example values on candidate fields were run against the real types;
  the single failure (`index_uuid`) became a rejection above.
- `examples/test/qlib/DataProvider/DataProvider.qtest` `semanticTypesThroughProviderTest()` covers the types
  through a real provider; `PdfCoDataProvider`, `BitlyDataProvider`, and `KitDataProvider` carry
  `semanticStringTypeTests()` asserting both the new identity and that each new rejection is intentional.

### Open design questions

Raised by the adoption pass; none is changed unilaterally, because every schema-imported type shares these
regexes.

1. **Scheme-less URLs.** 17 human-entered URL fields were left as `string` solely because `UriType` requires a
   scheme. If that pattern keeps recurring, the better fix is a `url` format that accepts a scheme-less
   authority (or relaxing `UriType`), not skipping the fields. `hostname` already covers the bare-domain case.
2. **Phone extensions.** 31 phone input fields were left as `string` because `PhoneType` rejects `x89`/`ext. 3`.
   A format that tolerates a trailing extension would unlock all of them.
3. **`ui_type` derivation.** `ActionOptionInfo::ui_type` is populated only from an explicit field attribute, so
   a semantic-typed field leaves it unset and a UI must read `option.type`'s `qore.external_name` tag instead.
   Deriving `ui_type` from the format would be more discoverable, but the format names (`uri`) do not match the
   `ui_type` vocabulary already in use (`url`), so this needs a decision rather than a patch.
4. **Coverage not yet reviewed.** 174 candidate fields sit in types this pass could not attribute to a request
   or a response root, and 17 are reachable from both; neither group was swept.

## Adding a format

1. Add a `public const <Name>Type`/`<Name>OrNothingType` pair in `QoreStringFormatDataType.qc`.
2. Register `"<name>"`/`"*<name>"` in `AbstractDataProviderTypeMap`.
3. Add `case "<name>":` to `getTypeIntern()` in `Swagger.qm` and `OpenApi3.qm`, and an entry to
   `OpenApi3TypeMapper::TypeMap`.
4. Add `QoreStringFormatDataType.qc.dox.h` is already in `doxygen/qlib/Doxyfile.DataProvider` (one entry
   covers the class); extend `examples/test/qlib/DataProvider/DataProvider.qtest` coverage.
