# Generic Class Future Work Checklist

This checklist covers generic class type work after the core source/builtin
generic class, generic hashdecl, static generic method, method-level generic,
default type argument, bounded type argument, and explicit generic call type
argument implementation.

Phases 1-5 and 7 are complete in the current branch. Phase 6 now has a
benchmark-driven go/no-go gate and does not justify implementation with the
current results. The remaining open rollout work is Phase 8, which touches broad
public surfaces in core qlib and shipped binary modules.

## Phase 1: Variance And Wildcard Type Arguments

Goal: allow APIs to express "some instantiation compatible with this bound"
without relying on raw legacy annotations.

Design checklist:

- [x] Choose final wildcard syntax for class and hashdecl type arguments.
- [x] Support unbounded wildcards.
- [x] Support covariant upper-bound wildcards.
- [x] Support contravariant lower-bound wildcards if there is a clear,
  user-facing assignment/call use case.
- [x] Define how wildcards interact with `*T` optional types.
- [x] Define how wildcards interact with `auto`, which remains a concrete type
  argument.
- [x] Define compatibility between wildcard arguments and raw generic
  compatibility attributes.
- [x] Define diagnostic wording for rejected wildcard construction attempts.

Implementation checklist:

- [x] Extend parser and scanner type grammar.
- [x] Mirror parser/scanner changes in `modules/astparser/src/`.
- [x] Extend `QoreTypeInfo` representation for wildcard generic arguments.
- [x] Update class type compatibility checks.
- [x] Update hashdecl type compatibility checks.
- [x] Update `instanceof`, assignment, argument filtering, return checking,
  casts, and overload selection.
- [x] Preserve wildcard type paths through AOT type metadata.
- [x] Expose structured wildcard argument metadata for API metadata consumers.
- [x] Add positive and negative tests for source classes, builtin classes,
  source hashdecls, inherited hashdecls, raw compatibility, AOT, AST mode, and
  error messages.

## Phase 2: Expression-Site Class Type Argument Inference

Goal: infer class type arguments where the result is unique and explain
ambiguity when it is not.

Design checklist:

- [x] Infer constructor type arguments from explicit constructor arguments.
- [x] Infer constructor type arguments from assignment target type.
- [x] Infer static factory receiver type arguments from assignment target type
  only when the receiver is generic and otherwise unbound.
- [x] Infer from return context only where Qore already has a reliable expected
  type.
- [x] Reject ambiguous inference when overloads, defaults, or unrelated
  argument positions produce competing bindings.
- [x] Prefer explicit type arguments over inference.
- [x] Preserve default type argument behavior when inference does not bind an
  optional parameter.
- [x] Define diagnostics that tell users which explicit spelling to write.
- [x] Preserve `[legacy_raw]` and `[legacy_raw_construct]` raw construction
  semantics by skipping constructor argument inference for those classes.

Implementation checklist:

- [x] Reuse or extend method-level type-parameter inference helpers for class
  constructors.
- [x] Thread expected target type through declaration construction, assignment,
  and return-expression parse initialization where available.
- [x] Add inference bindings for constructor formal types containing class type
  parameters.
- [x] Apply bounds/default checks after inference.
- [x] Preserve inferred object type through IR, JIT helpers, and AOT.
- [x] Add tests for constructor inference, target-type inference, defaults,
  bounds, overload ambiguity, named arguments, AST mode, and AOT.

## Phase 3: Specialized Generic Code Generation

Goal: allow IR/JIT to emit specialized bodies for concrete generic
instantiations without changing source-visible semantics, while preserving AOT
compatibility through runtime-resolved generic metadata.

Design checklist:

- [x] Decide which execution tier owns specialization: specialized IR is
  lowered at dispatch time from the source body, JIT compiles that specialized
  IR when JIT or tiered execution is active, and AOT keeps using serialized
  generic metadata resolved at runtime.
- [x] Define specialization cache keys for class receiver type arguments,
  method/function type arguments, static generic calls, and parameterized
  hashdecl-derived records.
- [x] Keep the substitution-based interpreter path as the correctness baseline.
- [x] Define invalidation and reuse rules for source-stripped AOT and module
  loading: specialized IR is owned by the source variant and reused for the
  lifetime of that variant; source-stripped AOT bodies remain generic and
  resolve type metadata through the AOT type table at runtime.
- [x] Define when specialization is skipped, such as calls without concrete
  generic context, source-stripped AOT bodies, and failed specialized lowering.
  These paths use the substitution-based AST or runtime-metadata execution path.
- [x] Define observability rules so reflection and stack traces still describe
  the source method/function.

Implementation checklist:

- [x] Introduce an internal specialization descriptor.
- [x] Intern or cache descriptors per variant.
- [x] Clone or lower IR with concrete substituted types.
- [x] Make JIT lookup specialization-aware while keeping fallback execution.
- [x] Preserve AOT compatibility without adding new metadata by serializing
  generic hashdecl type paths and resolving them through the runtime
  receiver/type-argument context.
- [x] Add correctness tests that compare specialized and non-specialized paths.
- [x] Add stress tests for cache reuse and mixed instantiations.
- [x] Add performance-focused smoke tests only after correctness is stable.

## Phase 4: Doxygen And User Documentation

Documentation checklist:

- [x] Update `doxygen/lang/155_data_type_declarations.dox.tmpl`.
- [x] Update `doxygen/lang/215_classes.dox.tmpl`.
- [x] Update `doxygen/lang/217_hashdecl.dox.tmpl`.
- [x] Update `doxygen/lang/220_threading.dox.tmpl` if builtin generic examples
  change.
- [x] Update `doxygen/lang/900_release_notes.dox.tmpl`.
- [x] Include examples for defaults, bounds, method-level generics, explicit
  generic call type arguments, generic hashdecl inheritance, variance/wildcards,
  expression-site inference, and any visible specialization behavior.
- [x] Keep examples concrete and executable where practical.

## Phase 5: Local Verification

Run focused checks after each implementation phase, then run a complete local
verification before the final commit:

- [x] `cmake --build build --target qore -j$(nproc)`
- [x] `cmake --build build --target qpp astparser -j$(nproc)`
- [x] `./run_tests.sh -d qore/misc/generic-classes.qtest`
- [x] `build/qore --exec-mode=ast examples/test/qore/misc/generic-classes.qtest`
- [x] `build/qore --exec-mode=ir examples/test/qore/misc/generic-classes.qtest`
- [x] `build/qore --exec-mode=jit examples/test/qore/misc/generic-classes.qtest`
- [x] `build/qore --exec-mode=tiered examples/test/qore/misc/generic-classes.qtest`
- [x] `build/qcc -o build/generic-classes-qtest-aot examples/test/qore/misc/generic-classes.qtest`
- [x] `build/generic-classes-qtest-aot`
- [x] `build/qore modules/astparser/test/astparser.qtest`
- [x] Relevant docs fast targets, such as `make docs-lang-final-fast`.
- [ ] Re-run full `./run_tests.sh` after installing Tesseract English data. The
  previous full run passed 653/654 tests with only
  `examples/test/qlib/QoreOcrUtils/QoreOcrUtils.qtest` failing due missing
  `eng.traineddata`; the focused OCR qtest now passes after installing
  `tesseract-ocr-eng`.
- [x] Final post-cleanup `git diff --check`.
- [x] Apply `audit-changes` before each commit.

## Phase 6: Source-Stripped AOT Native Generic Specialization

Goal: decide with benchmark data whether source-stripped AOT should emit
separate native bodies for concrete generic type-argument tuples, then implement
it only if the speedup justifies the code size and metadata complexity.

Design checklist:

- [x] Add benchmarks that compare AST, IR, JIT, current AOT, and source-stripped
  AOT for hot generic class methods, method-level generic functions, static
  generic methods, and generic hashdecl-heavy result records.
- [x] Define a minimum improvement threshold before implementing native AOT
  specialization, for example a material speedup on hot generic dispatch or type
  substitution benchmarks without unacceptable binary growth.
- [ ] Define specialization keys for AOT native entry points: source body id,
  receiver class type arguments, method/function type arguments, and any
  parameterized hashdecl paths needed by the lowered body.
- [ ] Decide whether specialized AOT entry points are emitted eagerly from known
  source call sites, lazily from a load-time/runtime cache, or both.
- [ ] Define code-size controls: per-body specialization limits, fallback to the
  generic metadata path, and diagnostics or counters for skipped
  specializations.
- [ ] Preserve source observability: stack traces, reflection, profiling names,
  and error locations must continue to identify the source method/function.

Implementation checklist:

- [ ] Extend AOT metadata to record specialization descriptors without changing
  source-visible type identity.
- [ ] Teach qcc/AOT lowering to substitute concrete type arguments into native
  bodies using the same rules as IR/JIT specialization.
- [ ] Add a dispatch path that selects a matching specialized native entry point
  and falls back to the generic runtime metadata path when no match exists.
- [ ] Preserve compatibility for old AOT artifacts and modules built without
  generic support.
- [ ] Add correctness tests for source-stripped modules with mixed
  instantiations, nested generic hashdecls, static generic methods, and fallback
  paths.
- [x] Add performance tests that report both runtime and generated artifact size.

Benchmark and decision:

- Benchmark harness: `bench/generic-aot-specialization.qr`
- Mode comparison runner: `bench/run_generic_aot_specialization.sh`
- Reproduction command used on 2026-05-19:
  `SAMPLES=3 WARMUP=1 INNER=100000 HASH_INNER=100000 bench/run_generic_aot_specialization.sh --no-header`
- Generated executable sizes in this run: source-included AOT was 173,520
  bytes; source-stripped AOT was 128,464 bytes.
- Go/no-go threshold: implement native source-stripped AOT specialization only
  if source-stripped AOT is at least 15% slower than both JIT/tiered and
  source-included AOT on at least two generic call kernels, or if a real qlib or
  module workload shows at least 15% generic-dispatch overhead attributable to
  missing source-stripped specialization after allocation and I/O costs are
  excluded.
- Current local result: source-stripped AOT is faster than source-included AOT
  and faster than AST/IR/JIT/tiered for the generic dispatch kernels and the
  generic hashdecl-heavy kernels. The earlier source-stripped AOT regression in
  `generic_hashdecl_records` and `hashdecl_construct_only` was caused by
  repeated runtime hashdecl path/type resolution, not by missing native generic
  body specialization; caching the resolved hashdecl path per receiver type
  context removed that bottleneck.
- Decision: do not implement native AOT generic specialization now. Keep the
  implementation checklist above as deferred design work if future benchmarks
  cross the threshold.

Local benchmark summary:

| Kernel | AST ms | IR ms | JIT ms | Tiered ms | AOT incl src ms | AOT stripped ms |
|--------|--------|-------|--------|-----------|-----------------|-----------------|
| `generic_class_method` | 262.611 | 360.501 | 267.444 | 266.100 | 250.058 | 247.946 |
| `method_generic_function` | 140.949 | 184.225 | 141.035 | 141.190 | 129.654 | 126.564 |
| `static_generic_method` | 169.982 | 201.376 | 165.400 | 161.370 | 143.847 | 141.285 |
| `generic_hashdecl_records` | 269.318 | 208.901 | 215.763 | 205.598 | 189.837 | 185.545 |
| `typed_list_literal` | 119.842 | 92.851 | 92.714 | 92.688 | 95.984 | 92.411 |
| `plain_hash_record` | 224.366 | 185.240 | 183.156 | 181.801 | 166.164 | 160.850 |
| `hashdecl_construct_only` | 235.484 | 165.982 | 178.659 | 167.580 | 165.078 | 159.793 |
| `hashdecl_access_only` | 77.009 | 58.946 | 60.098 | 59.226 | 61.654 | 57.528 |

## Phase 7: Broader Static Factory Receiver Inference

Goal: allow raw generic static factory calls to infer the receiver type from
static method arguments when the binding is unique and all bounds/defaults can
be checked, while preserving precise ambiguity diagnostics.

Design checklist:

- [x] Support inference for calls such as `Factory::make(1)` when a static method
  signature mentions class type parameters in argument positions and produces a
  unique `Factory<int>` receiver.
- [x] Keep explicit receiver types authoritative:
  `Factory<string>::make(...)` must not be rewritten by argument inference.
- [x] Keep target/return-context inference as the first choice when present, then
  use argument-based receiver inference only for otherwise unbound raw generic
  static calls.
- [x] Integrate method-level type-parameter inference and class receiver
  inference so method type arguments and class type arguments do not bind each
  other inconsistently.
- [x] Reject calls with ambiguous overload-derived bindings, unbound required
  class type parameters, conflicting defaulted parameters, or failed bounds.
- [x] Define diagnostics that suggest the explicit spelling, for example
  `Factory<int>::make(...)`.

Implementation checklist:

- [x] Reuse constructor/method generic inference helpers for static method
  variant selection before final receiver substitution.
- [x] Thread named-call argument mapping into static receiver inference.
- [x] Preserve inferred receiver types through IR, JIT specialization keys, and
  AOT expression metadata.
- [x] Add tests for argument inference, defaults, bounds, named arguments,
  overload ambiguity, method-level generic interaction, AST/IR/JIT/tiered modes,
  and source-stripped AOT.

Verification performed for Phase 7:

- `./run_tests.sh -d qore/misc/generic-classes.qtest`
- `build/qore --exec-mode=ast examples/test/qore/misc/generic-classes.qtest`
- `build/qore --exec-mode=ir examples/test/qore/misc/generic-classes.qtest`
- `build/qore --exec-mode=jit examples/test/qore/misc/generic-classes.qtest`
- `build/qore --exec-mode=tiered examples/test/qore/misc/generic-classes.qtest`
- `build/qcc -o build/generic-classes-qtest-aot examples/test/qore/misc/generic-classes.qtest`
- `build/generic-classes-qtest-aot`

## Phase 8: Core, Qlib, And Shipped Binary Module Generic API Rollout

Goal: finish applying generics where the API has a stable logical value type,
and avoid generics where they would hide heterogeneous or operation-dependent
data behind a misleading type parameter.

Core candidate checklist:

- [x] Evaluate `RangeIterator`: deferred. Its constructors can produce either
  integer sequence values or an arbitrary caller-supplied value. A single class
  type parameter would hide that overload-dependent behavior unless a separate
  design splits the integer iterator from the value-repeating iterator.
- [x] Evaluate `TreeMap`: implemented as `TreeMap<V>`. Keys remain path strings;
  a separate key type parameter would be misleading because the key API is not
  a generic map key contract.
- [x] Evaluate `HashListIterator` / `ListHashIterator` and reverse variants:
  deferred. These iterators flatten runtime hash/list shapes and often produce
  heterogeneous `hash<auto>` records, so `hash<RecT>` would overstate row-shape
  stability.
- [x] Evaluate `MongoCursor` in the shipped mongodb module: implemented as
  `MongoCursor<DocT: hash<auto> = hash<auto>>`, with `next()` returning
  `*DocT` and `toList()` returning `list<DocT>`.
- [x] Evaluate logger queue internals for `Queue<LoggerEvent>` and typed event
  hashes: deferred for public APIs. Logger event payloads are intentionally
  dynamic hashes; queue internals can be tightened later without exposing a
  public generic class contract.
- [x] Evaluate connection/pool abstractions for a resource type parameter:
  deferred. Pool/resource subclasses do not currently expose one stable
  `AbstractPoolableResource` subtype across the public API.

Qlib candidate checklist:

- [x] Convert remaining qlib record iterators that already return one stable
  record shape to `AbstractDataProviderRecordIterator<RecT>` or
  `DefaultRecordIterator<RecT>`.
- [x] Review provider-specific iterators in MongoDB and DataFrame for this
  branch. `MongoDbRecordIterator` and `DataFrameRecordIterator` now inherit
  `AbstractDataProviderRecordIterator<hash<auto>>`; the related
  `searchRecordsImpl()` methods return the same typed parent.
- [x] Review provider-specific iterators in ServiceNow, Salesforce,
  ElasticSearch, SmartSheet, Wave, Jotform, CdsRest/OData, Generator, Qdrant,
  DbDataProvider, FixedLength, Edifact, CsvUtil, TableMapper, and the broader
  qlib DataProvider set. Stable record iterators now use
  `AbstractDataProviderRecordIterator<hash<auto>>` and
  `DefaultRecordIterator<hash<auto>>`. The abstract `AbstractDataProvider`
  implementation hooks intentionally remain raw because they are type-erased
  override points, and the local `QoreModelRegistry::DefaultRecordIterator`
  class is already a typed, module-local iterator despite its legacy class name.
- [x] Keep provider iterators raw or `hash<auto>` when the record shape depends
  on runtime metadata, selected columns, or remote schema discovery.
- [x] Normalize Swagger, OpenAPI 3, and RestSchema record-search response
  bodies before wrapping them in typed iterators: `NOTHING` becomes an empty
  iterator, a hash becomes one record, and list responses must contain only hash
  records or a provider-specific search error is raised.
- [x] Review non-DataProvider qlib iterator APIs where the element type is
  stable. SQL schema-name iterators now return `ListIterator<string>`,
  selected SQL utility iterators expose typed `AbstractIterator` results, and
  local `ListIterator` uses in `SchemaReverse`, `WebSocketHandler`, and `QLS`
  carry explicit element types.
- [x] Defer qlib queues and dynamic walkers that intentionally carry mixed
  payloads, sentinels, runtime schemas, or arbitrary user data. Examples include
  HTTP/WebSocket/SSE queues, generic `HashIterator`/`ListIterator` walkers in
  serialization/debug/formatting helpers, and mapper input iterators whose
  contract is "hash-like input" rather than one concrete public value type.
- [x] Update examples to show concrete generic classes where the value type is
  known and keep raw examples only for intentional type erasure.

Shipped binary module candidate checklist:

- [x] Review bundled `modules/*` QPP classes first: dataframe, logger_bin,
  mongodb, tokenizer, protobuf, i18n, astparser, reflection, ml, and ocr.
- [ ] Review external `~/src/qore/git/module-*` repos that are delivered with
  Qore, prioritizing iterator/cursor/message APIs: json/ndjson, yaml, xml/sax,
  geos, openldap, mongodb, zip/tar streams, grpc streams, nats/amqp/zmq
  messaging, and database driver cursors or result records.
- [ ] Convert one module family at a time, guarded by `QORE_HAVE_GENERIC_CLASSES`
  or equivalent configure checks where the module must also build against plain
  develop.
- [ ] Update module Doxygen examples, release notes, generated `.meta.json`, and
  focused module tests for each conversion.
- [ ] Apply `audit-changes` before each module commit.

Bundled module review results:

| Module | Decision |
|--------|----------|
| `dataframe` | Processed. The class is not generic because row shape changes by operation. DataFrame-returning APIs now return `DataFrame`, grouped APIs return `GroupedDataFrame` / `DataFrame`, and stable hashes use `DataFrameShape`, `ColumnStats`, and `CsvOptions`. |
| `mongodb` | Processed. `MongoCursor<DocT: hash<auto> = hash<auto>>` preserves the document type through `next()` and `toList()`. MongoDbDataProvider uses `MongoCursor<hash<auto>>` and typed record iterators. |
| `logger_bin` | No public generic class conversion in this pass. Event hashes are intentionally dynamic; queue internals can be revisited without changing the public API. |
| `tokenizer` | No class-level generic candidate. Tokenizer result/option hashes are runtime-schema records; consider typed hashdecl cleanup separately. |
| `protobuf` | No safe class-level generic candidate. Message hashes are schema-selected by the runtime protobuf type name. |
| `i18n` | No safe class-level generic candidate. Catalogs, locale contexts, options, and stats are metadata hashes rather than carried value types. |
| `astparser` | No data-container generic candidate. Generic syntax support is parser/tooling behavior, not a public generic object family. |
| `reflection` | Already exposes generic class/hashdecl metadata and type arguments; reflection objects are handles to runtime metadata, not generic containers. |
| `ml` | No class-level generic candidate. Models, matrices, and transform outputs are numeric or fitted-schema driven; existing typed result hashdecls remain the right surface. |
| `ocr`, `krb5`, `linenoise` | No stable carried value type identified for class-level generics. |
