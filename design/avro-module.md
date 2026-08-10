# The builtin `avro` module

Tracking issue: [#5367](https://github.com/qoretechnologies/qore/issues/5367)
Depends on: [#5366](https://github.com/qoretechnologies/qore/issues/5366) (builtin `json` with a
public C++ API — see `design/json-module-migration.md`) and
[#5371](https://github.com/qoretechnologies/qore/issues/5371) (the module C++ API mechanism that
both modules publish through — see `design/module-cpp-api.md`)
First consumer: [#5365](https://github.com/qoretechnologies/qore/issues/5365) (Salesforce Pub/Sub
event source in `module-grpc`)

## Goal

A builtin binary module `modules/avro/` implementing
[Apache Avro](https://avro.apache.org/docs/current/specification/) — schema parsing, the binary
datum codec in both directions, object container files, and reader/writer schema resolution.

Avro is schema-driven: the encoded bytes carry no field names, no tags and no type markers, just
values packed in schema order. Nothing can be decoded without the schema, and the schema is itself
a JSON document. So the module is two things — a JSON-driven schema parser and a binary codec —
and the `json` module publishing its parser as a C++ API (#5366, then #5371) is what makes the
first half cheap.

**Both framings are first-class.** A *container file* is self-describing: schema in the header,
then blocks of records. A *bare datum* has no framing at all — the schema arrives out of band and
is resolved at runtime by ID. Salesforce Pub/Sub sends only bare data, and `module-jni`'s
`qlib/AvroDataProvider` (the only Avro support in the ecosystem today) is container-file oriented,
which is why it cannot serve #5365.

## Decisions

### 1. The schema representation stays internal; both the Qore handle and the C++ API are opaque

The reusable resolved-schema handle is the Qore-level `AvroSchema` object: it is fully resolved at
construction, immutable afterwards, and therefore safe to share between threads and cache
indefinitely. A consumer keyed by schema ID caches `hash<string, AvroSchema>` and pays the JSON
parse once per distinct schema.

For content-addressed caching the module also exposes the spec's own identity primitives —
`AvroSchema::getCanonicalForm()` (Parsing Canonical Form) and `AvroSchema::getFingerprint()`
(CRC-64-AVRO over it) — so two schemas that differ only in documentation, aliases, field order of
the JSON text, or default values compare equal and share a cache entry.

**As originally written this decision also said "no `include/qore/QoreAvro.h`, no exported C++
schema type", because the consumer that needs parse-once/decode-many from C++ (#5365) lives in
`module-grpc`, a separate repository, and resolving C++ symbols across separately `dlopen()`ed
modules had been rejected as fragile when the JSON codec was placed (see
`design/json-module-migration.md`, decision 1).** #5371 removed that obstacle: the module now
publishes `QoreAvroApi` through the module C++ API mechanism
(`design/module-cpp-api.md`, `include/qore/QoreAvroApi.h`), so a consumer in another repository
gets versioned, on-demand access with a clean exception on mismatch instead of a lazy-binding
abort.

What has *not* changed is that the schema representation stays internal. `QoreAvroSchemaRef` is an
opaque handle with explicit `schema_ref` / `schema_deref` in the API struct; `AvroNode`,
`AvroSchemaData` and `QoreAvroSchema` remain module-private and can be changed freely. The API
struct also bridges both directions between a handle and the Qore `AvroSchema` object
(`schema_object` / `schema_from_object`), so a schema parsed on one side can be used on the other
without reparsing — which is what a Pub/Sub event source needs when it decodes in C++ but maps
types with `AvroUtil`'s `AvroTypeHelper` in Qore.

### 2. The codec is hand-written; Apache `avro-cpp` is not vendored

The binary encoding is small and completely specified: zigzag varint for `int`/`long`,
little-endian IEEE for `float`/`double`, length-prefixed `bytes`/`string`, counted blocks for
`array`/`map`, a varint branch index for `union` and `enum`, raw N bytes for `fixed`. The whole
codec is well under a thousand lines.

Against that, `avro-cpp` costs a Boost dependency and another vendored tree. The last vendoring
decision in this tree — jsoncons for `json` — cost 3.9 MB and shipped a behavioural regression
that only surfaced under test (`design/json-module-migration.md`, decision 2). There is no reason
to repeat that for an encoding this size.

Consequence: the module links **libqore only** (plus zlib, already a hard libqore dependency, for
the container-file `deflate` codec). The JSON codec it needs for schema parsing is resolved at run
time through the module C++ API mechanism rather than linked, so there is no build-time dependency
on the `json` module either — see `design/module-cpp-api.md`.

### 3. Parser invariants are enforced with real checks, never `assert()`

Every value the decoder reads is attacker-controlled in the Pub/Sub use case: union branch
indexes, block counts, block byte sizes, string and `bytes` lengths, `fixed` sizes, enum indexes.
All of them are range-checked against the schema and the remaining buffer on every read, in
release builds.

This is a direct lesson from the `json` migration: a jsoncons behaviour change turned an
`assert`-only invariant in the CBOR visitor into a `stack.back()` on an empty vector — a
heap-corrupting read of a garbage pointer in release builds, where `assert` compiles away.
`assert()` may document an invariant here; it may never be the only thing enforcing one.

### 4. Nesting depth is capped unconditionally, not only under sandboxing

`AVRO_MAX_NESTING_DEPTH` is 256 and applies to both the schema parser and the datum codec, in all
builds, regardless of whether a `QoreSandboxManager` is installed.

This differs deliberately from `JSON_MAX_NESTING_DEPTH`, which the `json` module enforces in
sandbox mode only. In JSON the nesting of the *document* bounds recursion, and a trusted caller
parsing its own trusted document cannot hurt itself.
In Avro a **recursive schema** — a record with a field of its
own type, which is legal and common (linked lists, trees) — means the *datum* drives recursion
depth without limit. Sixteen bytes of crafted input can request a hundred thousand levels of
descent. That is a stack overflow, i.e. a crash, not a policy decision, so it is not gated on
policy.

256 is far above any real schema; container files and bare data alike are rejected with
`AVRO-DECODE-ERROR` beyond it.

### 5. `decimal` maps to `number`, converted through an exact decimal string

Avro `decimal` is a `bytes` or `fixed` holding a two's-complement big-endian **unscaled integer**,
with `precision` and `scale` in the schema. Precision commonly exceeds 18 digits (Salesforce and
Debezium both emit `decimal(38, …)`), so the unscaled value does not fit in `int64` and
fixed-width arithmetic is not an option.

The module therefore carries a small big-integer helper that converts base-256 two's complement to
and from a base-10 string, and routes every conversion through that string:

- decode: unscaled bytes → decimal string → `QoreNumberNode(str)`
- encode: `number` → decimal string → scaled → unscaled bytes

`QoreNumberNode(const char*)` allocates `max(128, strlen*5)` bits of MPFR precision, which
round-trips a 38-digit decimal exactly; the round trip is asserted in the test suite up to the
`fixed`-size limit.

Nothing is silently rounded: encoding a `number` with more fractional digits than `scale`, or more
significant digits than `precision`, raises `AVRO-ENCODE-ERROR`.

### 6. Date/time logical types

Qore's `date` covers both absolute timestamps and relative durations, which is enough to represent
every Avro temporal type faithfully:

| Avro logical type | Base | Qore value |
|---|---|---|
| `date` | `int` (days since 1970-01-01) | absolute date, UTC midnight |
| `time-millis` | `int` (ms since midnight) | **relative** date (duration) |
| `time-micros` | `long` (µs since midnight) | **relative** date (duration) |
| `timestamp-millis` | `long` (ms since epoch) | absolute date, UTC |
| `timestamp-micros` | `long` (µs since epoch) | absolute date, UTC |
| `local-timestamp-millis` | `long` | absolute date, **local** zone |
| `local-timestamp-micros` | `long` | absolute date, **local** zone |
| `duration` | `fixed[12]` (3 × LE uint32: months, days, ms) | relative date |
| `uuid` | `string` | `string` |

`time-*` is a time of day with no date attached, and Qore has no time-only type; a relative date
(`PT13H45M30S`) says exactly that and nothing more, where an absolute date on 1970-01-01 would
invent a date the data does not contain.

`local-timestamp-*` is defined by the spec as a wall-clock timestamp with no zone. Qore dates
always carry a zone, so it is materialised in the local zone — the only reading under which
formatting it reproduces the written wall-clock time.

Avro `duration` has three independent components; Qore relative dates hold months and days
separately from the time part, so the round trip is lossless.

### 7. Union branch selection when encoding

Decoding a union is unambiguous — the branch index is on the wire. Encoding is not: a Qore string
offered to `["string", "bytes"]` could go either way. The module resolves this with a fixed,
documented preference order rather than requiring callers to wrap values:

| Qore value | Branch preference |
|---|---|
| `NOTHING` / `NULL` | `null` |
| `bool` | `boolean`, `int`, `long`, `float`, `double`, `string` |
| `int` | `int` (if it fits in 32 bits), `long`, `float`, `double`, `string` |
| `float` | `double`, `float`, `long`, `int`, `string` |
| `number` | `decimal`, `double`, `float`, `string` |
| `string` | `enum` (if the value is a symbol), `uuid`, `string`, `bytes`, `fixed` (exact size) |
| `binary` | `bytes`, `fixed` (exact size) |
| `date` (absolute) | `timestamp-micros`, `timestamp-millis`, `local-timestamp-*`, `date`, `long` |
| `date` (relative) | `duration`, `time-micros`, `time-millis`, `long` |
| `list` | `array` |
| `hash` | `record` with the best field-name match, then `map` |

Within a branch kind, the first matching branch in schema order wins. Where several `record`
branches are present the one whose required-field set the hash satisfies with the fewest unmatched
keys is chosen; ties go to schema order. No match raises `AVRO-ENCODE-ERROR` naming the value type
and the branch list.

The alternative — an explicit `{"<branch>": value}` wrapper, as Avro's *JSON* encoding uses — was
rejected for the binary API: it collides with any single-field record whose field name happens to
match a branch name, and it would force every caller of the common `["null", T]` shape to wrap
values that are never ambiguous.

### 8. Schema resolution is performed inline, with no plan cache

Reader/writer resolution follows the spec's reconciliation rules: record fields matched by name or
reader alias, missing reader fields filled from their declared defaults, extra writer fields
skipped without materialising a value, enum symbols unknown to the reader falling back to the
reader's `default`, the numeric promotions `int`→`long`/`float`/`double`, `long`→`float`/`double`,
`float`→`double`, and `string`↔`bytes` interchange, plus the four union/non-union combinations.

An explicit pre-computed resolution tree, cached per (reader, writer) pair, was considered and
dropped. The only part of resolution with non-trivial per-value cost is the record field lookup,
and each record node already carries a `name → field index` map built once at parse time, so
inline resolution costs one hash lookup per field — the same as the plan would. A cache would add
a lock and a lifetime problem for no measurable gain.

Writer fields absent from the reader are handled by a dedicated `skipValue()` walk that advances
the cursor without allocating.

### 9. Container-file codecs: `null` and `deflate`

`null` and `deflate` are implemented. Avro's `deflate` is raw DEFLATE (RFC 1951) with no zlib
wrapper — `windowBits = -15` — not the zlib-framed output of Qore's `compress()`.

`snappy`, `bzip2`, `xz` and `zstandard` are not implemented; a container file using one is
rejected at header-parse time with `AVRO-CODEC-ERROR` naming the codec, rather than failing
obscurely part-way through the first block. They can be added later without an API change.

### 10. `module-jni`'s `qlib/AvroDataProvider` is not retired by this work

It is registered in `qlib/DataProvider/DataProvider.qc` as the provider behind the `avroread` and
`avrowrite` data provider factories. This module ships a **codec**, not those factories, so
removing it now would delete working functionality with no replacement.

It is nevertheless superseded in substance: it is JVM-backed, container-file only, and cannot do
the bare-datum decode #5365 needs. The right follow-up is a separate issue that reimplements
`avroread`/`avrowrite` on this codec and then retires the JNI-backed provider — a data-provider
change, in a different repository, with its own test surface. Out of scope here.

What this work does deliver toward that is task 7 of #5367: `AvroTypeHelper`, the Avro →
`AbstractDataProviderType` mapping, which is the piece any future provider needs.

## Module layout

```
modules/avro/
    CMakeLists.txt              # modelled on modules/protobuf; no VERSION (builtin modules
                                # version with Qore)
    docs/
        mainpage.dox.tmpl       # index; @section avrointro
        getting-started.dox.tmpl
        cookbook.dox.tmpl
        schema-guide.dox.tmpl   # type/logical-type mapping tables, resolution rules,
                                # union branch selection
        release-notes.dox.tmpl
    src/
        avro-module.h / .cpp    # QoreNamespace("Qore::Avro"), registration
        AvroSchema.h / .cpp     # node graph, JSON schema parser, named-type registry,
                                # canonical form, fingerprint
        AvroDecimal.h / .cpp    # base-256 two's complement <-> base-10 string
        AvroDecoder.h / .cpp    # bare datum decode + skipValue + resolved decode
        AvroEncoder.h / .cpp    # bare datum encode
        AvroContainer.h / .cpp  # object container file framing, codecs
        QC_AvroSchema.qpp
        QC_AvroFileReader.qpp
        QC_AvroFileWriter.qpp
qlib/AvroUtil.qm                # AvroTypeHelper (Avro -> AbstractDataProviderType)
examples/test/modules/avro/     # binary module suites
examples/test/qlib/AvroUtil/    # user module suite
```

`run_tests.sh` only scans `examples/test`, so suites under `modules/<name>/test/` are silently
excluded from the standard run. The binary suites therefore go to `examples/test/modules/avro/`,
following `sshutil` and `json`.

### Schema representation

Named types may be recursive, so ownership cannot be a `shared_ptr` graph — it would leak on every
cycle. Instead:

- `AvroSchemaData` — refcounted, owns `std::vector<std::unique_ptr<AvroNode>>` (an arena), the
  `fullname → AvroNode*` registry, and the original schema JSON text.
- `AvroNode` — refers to other nodes by raw `AvroNode*` into the arena. Cycles are free; nothing
  is owned twice.
- `QoreAvroSchema` (the `AvroSchema` private data) — a reference to an `AvroSchemaData` plus the
  `AvroNode*` it is rooted at.

Rooting a `QoreAvroSchema` at an interior node is what makes `AvroSchema::getNamedType()` cheap
and safe: the returned handle shares the arena and holds it alive.

Everything in `AvroSchemaData` is written during construction and read-only afterwards, so a
parsed `AvroSchema` needs no lock and can be shared across threads.

### Cancellation

`qore_check_cancel(xsink, …)` is called every `AVRO_INTERRUPT_CHECK_INTERVAL` (100) iterations in
every unbounded loop: array and map block element loops, the container-file block loop, and the
record-field loop for very wide records. Per `design/cooperative-cancellation.md`, the check is one
atomic load when nothing is cancelled.

### Functional domains

`AvroFileReader::constructor(string path)` and `AvroFileWriter::constructor(string path, …)` are
tagged `dom=FILESYSTEM`; they are the only members that touch the filesystem. The domain is on the
individual constructors rather than the `qclass` so that the `binary`, `InputStream` and
`OutputStream` forms stay usable under `PO_NO_FILESYSTEM`.

## Qore API

All classes are in `Qore::Avro`; QPP `ns=Qore::Avro` matches the `QoreNamespace("Qore::Avro")`
constructor argument (see `design/qore-module-structure.md`).

### `AvroSchema`

```qore
AvroSchema(string schema_json)          # the canonical entry point
AvroSchema(hash<auto> schema)           # a schema already parsed into Qore data
AvroSchema(list<auto> union_schema)     # a top-level union

string   getType()                      # "record", "string", "union", ...
*string  getName()                      # fullname, for named types
*string  getNamespace()
string   getJson()
auto     getHash()                      # the schema as Qore data
string   getCanonicalForm()             # Parsing Canonical Form
int      getFingerprint()               # CRC-64-AVRO over the canonical form
list<string> getNamedTypeNames()
*AvroSchema  getNamedType(string fullname)

auto   decode(binary data, *AvroSchema writer_schema)
binary encode(auto data)
```

`decode()` with no `writer_schema` decodes a bare datum written with this same schema. With one,
`self` is the **reader** schema and the argument is the **writer** schema, and the spec's
resolution rules apply.

### `AvroFileReader` (an `AbstractIterator<auto>`)

```qore
AvroFileReader(string path)                     # dom=FILESYSTEM
AvroFileReader(binary data)
AvroFileReader(Qore::InputStream is)

AvroSchema getSchema()
hash<string, binary> getMetadata()
string getCodec()
binary getSyncMarker()

bool next()
auto getValue()
bool valid()
```

### `AvroFileWriter`

```qore
AvroFileWriter(string path, AvroSchema schema, *hash<auto> opts)   # dom=FILESYSTEM
AvroFileWriter(Qore::OutputStream os, AvroSchema schema, *hash<auto> opts)

nothing write(auto datum)
nothing writeAll(list<auto> data)
nothing flush()
nothing close()
```

`opts`: `codec` (`"null"` | `"deflate"`, default `"null"`), `block_size` (bytes before an automatic
block flush, default 64 KiB), `metadata` (a `hash<string, data>` of extra header metadata; keys
under the reserved `avro.` prefix are rejected), `sync` (a 16-byte `binary` sync marker, for
reproducible output).

### Exceptions

| Code | Raised when |
|---|---|
| `AVRO-SCHEMA-ERROR` | the schema is not valid Avro |
| `AVRO-DECODE-ERROR` | the data does not match the schema, or is truncated |
| `AVRO-ENCODE-ERROR` | the value cannot be represented by the schema |
| `AVRO-RESOLUTION-ERROR` | the reader and writer schemas cannot be reconciled |
| `AVRO-CODEC-ERROR` | an unsupported or corrupt container-file codec |
| `AVRO-FILE-ERROR` | container-file framing is invalid |

## Type mapping

| Avro | decodes to | encodes from |
|---|---|---|
| `null` | `NOTHING` | `NOTHING`, `NULL` |
| `boolean` | `bool` | `bool` |
| `int` | `int` | `int` in [-2³¹, 2³¹) |
| `long` | `int` | `int` |
| `float` | `float` | `float`, `int` |
| `double` | `float` | `float`, `int` |
| `bytes` | `binary` | `binary`, `string` (its bytes) |
| `string` | `string` (UTF-8) | `string` |
| `record` | `hash` | `hash` |
| `enum` | `string` (the symbol) | `string` symbol, or `int` index |
| `array` | `list` | `list` |
| `map` | `hash` | `hash` |
| `union` | the branch's value | see decision 7 |
| `fixed` | `binary` of exactly `size` | `binary` of exactly `size` |

Logical types are in decision 6, except `decimal` (decision 5) which decodes to `number` and
encodes from `number`, `int`, `float` or `string`.

## Test plan

`examples/test/modules/avro/`:

| Suite | Covers |
|---|---|
| `avro.qtest` | every primitive and complex type against fixed byte vectors from the spec; round trips; recursive schemas; the named-type registry; canonical form and fingerprint |
| `AvroLogicalTypes.qtest` | every logical type, `decimal` at precisions 1…38 on both `bytes` and `fixed`, negative and zero unscaled values, `duration` component independence |
| `AvroContainer.qtest` | container files with `null` and `deflate`, multi-block files, empty files, custom metadata and sync markers, iteration, round trips through path / `binary` / streams |
| `AvroResolution.qtest` | added and removed reader fields, defaults for every type, field reordering, aliases, every promotion, enum default, all four union/non-union combinations |
| `AvroNegative.qtest` | truncated input at every field boundary, out-of-range union branch index, unknown enum symbol, wrong `fixed` size, negative block counts, oversized block byte sizes, nesting-depth overflow, unsupported codec, reserved metadata keys, unresolvable schemas, cancellation during a long decode |

Byte vectors are hard-coded from the Avro specification rather than produced by the module, so the
tests validate the encoding and not merely self-consistency.

`examples/test/qlib/AvroUtil/AvroUtil.qtest` covers the `AbstractDataProviderType` mapping,
including cycle detection on a recursive schema.

One valgrind pass runs over all suites at the end.

## Downstream registration

Being builtin, `avro` needs no clone/CI/mirror setup, but it does need to be listed everywhere the
other builtin modules are:

**qore**
- `CMakeLists.txt`: `add_subdirectory(modules/avro)`, `QORE_QM_METADATA_MODULE_DIRS`, and the
  `QORE_AOT_BINARY_MODULE_TARGETS` loop.
- `doxygen/lang/120_modules.dox.tmpl`: a `|binary|` row in "Modules Provided With %Qore". The
  table is case-insensitively alphabetical, so `avro` goes after `AsyncSocketIo` and before
  `AwsEventStream`.
- `doxygen/lang/900_release_notes.dox.tmpl`: the 3.0.0 "new builtin modules" summary line and a
  detail block in `qore_3_0_0_new_builtin_modules`, alphabetically first (before `dataframe`).

There is no `module-avro` repository to sequence against, so unlike the `json` migration this can
land in qore independently. A downstream product that cross-references the Qore module docs or
generates Java bindings per module needs a matching additive entry of its own, but that change is
not sequenced against this one and can follow at any time.

## Checked before building

- **No CMake target-name collision on `avro`.** Verified against the configured build tree
  (`build/CMakeFiles/TargetDirectories.txt` has no `avro` target from any FetchContent
  dependency). `QORE_BINARY_MODULE_INTERN2` derives the `.qmod` filename from the target name, so
  a collision cannot be worked around by renaming — it cost the `json` module a fix to nghttp2's
  `ENABLE_DOC` handling.
- **`*.dox.h` is gitignored** and must not appear in `MODULE_DOX_INPUT`.

## Sandboxing audit

Performed against `design/module-sandboxing-audit-guide.md`.

| Subsystem | Finding |
|---|---|
| **Filesystem** | The only filesystem access in the module is `QoreFile::open2()`, in `AvroFileSource::open()` and `AvroFileSink::open()`. `open2()` resolves the sandbox manager itself and calls `checkFilesystemAccess()` with a mode derived from the open flags, so the allow/deny list and sandbox root are enforced without the module doing anything. The two constructors that reach it are additionally tagged `dom=FILESYSTEM`, which is the separate parse-option (`PO_NO_FILESYSTEM`) gate. No other path in the module touches `fopen`, `stat`, `unlink`, `mkdir`, `opendir` or `dlopen`. **No gaps.** |
| **Network** | The module performs no network I/O of any kind. **Not applicable.** |
| **Resource limits** | Every allocation the module makes on behalf of decoded data is bounded before it happens: array and map block counts against the bytes remaining (or `AVRO_MAX_ZERO_WIDTH_ELEMENTS` when the element type can encode to zero bytes), container-file blocks and their decompressed expansion against `AVRO_MAX_BLOCK_SIZE`, recursion against `AVRO_MAX_NESTING_DEPTH`, and decimal digit counts against `AVRO_MAX_PRECISION`. The module creates no threads and performs no native recursion that is not depth-capped. **No gaps.** |
| **Interrupts** | `qore_check_cancel()` runs every `AVRO_INTERRUPT_CHECK_INTERVAL` iterations in the record-field, array-element and map-entry loops of the decoder, the resolver and the encoder; once per object in the container-file reader; once per entry in the container header metadata loop; every iteration of the deflate expansion loop; and every 100 fields in the schema parser. There is no blocking operation to poll: the codec is pure computation over a buffer, and the only I/O is a `QoreFile` read or a user-supplied `InputStream`. No cancel callback applies. **No gaps.** |

**Compliance level: full.** Safe for use in sandboxed programs.

The check is `qore_check_cancel()` called unconditionally at the interval, not gated on a
non-null `QoreSandboxManager*` as the older `json` code does. That is deliberate: per
`design/cooperative-cancellation.md` the function's first act is a single atomic load of the
per-thread cancel flag, which is cheaper than constructing a `QoreSandboxManagerHelper`, and
gating on a sandbox manager would make `cancel_thread()` unable to interrupt a long decode in an
unsandboxed program.
