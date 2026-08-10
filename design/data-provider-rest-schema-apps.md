# REST-Schema-Driven Application Modules

There are two ways to build a data provider application in this repository.

The **hand-written** path writes every action's options and output type in %Qore by hand; it is covered
by [data-provider-development-guide.md](data-provider-development-guide.md) and
[data-provider-checklist.md](data-provider-checklist.md), and it is what most of the `qlib/*DataProvider`
modules do.

The **schema-driven** path generates actions from a vendor's own OpenAPI 3 description using the
`RestSchemaActions` module. `StripeDataProvider` and `PaddleDataProvider` are built this way.

The machinery itself — the division of ownership between schema, manifest and overlay, request
flattening, wire codecs, reference data, drift classification — is documented on the
`RestSchemaActions` module main page (`qlib/RestSchemaActions/RestSchemaActions.qm`), and that
documentation is the reference. **This document does not repeat it.** It records the repository-level
conventions and the decisions that are not visible from inside any single module: which path to choose,
how an application is laid out, how a vendored schema is produced and kept current, and what the two
existing ports taught that the next one should not have to rediscover.

---

## Choosing a path

| Use the schema-driven path when | Use the hand-written path when |
|---|---|
| The vendor publishes a machine-readable OpenAPI 3 description and maintains it | There is no published schema, or it is written by hand and lags the API |
| The API is large enough that reproducing request and response contracts by hand is real, recurring work | The application exposes a handful of operations |
| Options and output types matter more than a bespoke provider shape | The integration needs record-based CRUD, server-side search expressions, or another non-action provider shape |

Two constraints decide it outright:

- **OpenAPI 3 only.** A Swagger 2.0 schema is rejected with a clear error rather than run through
  untested code paths. A vendor who publishes only Swagger 2.0 needs the hand-written path.
- **A schema fetched at run time is not an option.** The pinning discipline below is not a
  convenience; it is the reason this path is safe to use at all.

The paths are not exclusive, and no application is purely schema-driven. What the schema generates is
the API-call action surface. Everything else in a schema-driven application is ordinary hand-written
code:

| Generated from the schema | Hand-written |
|---|---|
| Action options, output types, and the transport request each action sends | The REST client module and its connection (`qlib/<App>RestClient.qm`) |
| Allowed values, requiredness and enums, from the schema's own declarations | `registerApp()`, the app info, the logo, the app groups |
| The dropdown queries, once a manifest names the action and fields that back them | Every `DPAT_EVENT` provider, its webhook signature verification and its event data types |

## Application layout

The convention both existing applications follow:

```
qlib/<App>RestClient.qm              # connection + client; conventional, see the development guide
qlib/<App>DataProvider/
  <App>DataProvider.qm               # module definition, registerApp(), registerActions()
  <App>DataProviderDefs.qc           # module dir, logo, shared constants
  <App>DataProviderFactory.qc        # factory registration
  <App>DataProvider.qc               # root provider
  <App>Schema.qc                     # provenance record + lazily-parsed schema and action set
  <App>Manifest.qc                   # the exported operations, the UX overlay, the reference data
  <App>BaseEventDataProvider.qc      # webhook plumbing and signature verification
  <App>EventDataProviders.qc         # one provider per webhook event
  <App>EventDataType.qc
  <app>-logo.svg
  <app>-openapi.yaml                 # the pruned, vendored schema
  i18n/
examples/test/qlib/<App>DataProvider/<App>DataProvider.qtest
```

`<App>Manifest.qc` is by far the largest source file in such a module — around 55 KB in both
applications. That is expected and correct: it is where every product decision lives, and it is the
file a reviewer should read to know what the application actually offers.

`<App>Schema.qc` and `<App>Manifest.qc` are the two files with no counterpart in a hand-written
application, and the two whose class-level documentation carries the application's durable rationale.

## Vendoring the schema

The committed `<app>-openapi.*` file is **not** the upstream document. It is the output of
`RestSchemaPruner::prune()` for exactly the operations the manifest exports, plus the transitive
closure of everything they reference. Pruning happens once, at import time, and the pruned document is
what gets committed.

This is not primarily a repository-size decision. Parsing the full upstream document is the cost being
avoided:

| | Upstream | Committed | Parse time, full document |
|---|---|---|---|
| Paddle | 7.4 MB, 70 paths, 99 operations, 519 component schemas | 569 KB, 237 schemas | ~1 second |
| Stripe | 416 paths, 1440 component schemas | 2.5 MB, 21 paths, 1022 schemas | ~45 seconds |

Note how little pruning removes from the component schemas in both cases — around half. Entity types in
a real API reference each other heavily, so do not expect the closure to be small; the win comes from
dropping the operations the application does not export and everything reachable only from them.

Three properties of the pinned schema are load-bearing:

- **Provenance describes the unmodified upstream document, not the committed artifact.** The
  `<App>SchemaProvenanceInfo` hashdecl records the source URL, the SHA-256 and byte size of the file as
  downloaded, and its path/operation/schema counts. Recording the pruned file's checksum instead would
  be useless: the next import has to be diffed against the same baseline the last one started from.
  Stripe additionally records the `api_version` its schema describes, because `StripeRestClient` sends
  that version with every request and the two must move together.

- **The schema is parsed on first use, behind a `Mutex`, into a `static` member** — not when the module
  loads. A program that only needs the connection never pays for it, and the action set is built once
  per process regardless of how many providers are constructed.

- **The schema file must be listed in `CMakeLists.txt`.** `qore_user_module()` takes extra non-source
  files as trailing arguments and copies them next to the built `.qmod`:

  ```cmake
  qore_user_module("qlib/PaddleDataProvider" "paddle-logo.svg" "paddle-openapi.yaml")
  qore_user_module("qlib/StripeDataProvider" "stripe-logo.svg" "stripe-openapi3.json")
  ```

  This is what makes `get_script_dir()` — used by `<App>DataProviderDefs.qc` to locate both the logo and
  the schema — resolve correctly whether the module is loaded from `qlib/` sources or from the
  AOT-compiled `build/qlib-qmod/<App>DataProvider/`. Omitting a file here produces a module that builds,
  installs and loads, and then throws when the first action is opened. Add both files at the same time
  the module is registered.

### There is no committed importer

The pruning step for both applications was performed by an ad-hoc program calling
`RestSchemaPruner::prune()`. Nothing in the repository automates *download → prune → write → update
provenance*; the pruner is exercised only by
`examples/test/qlib/RestSchemaActions/RestSchemaPruner.qtest`. A third schema-driven application is the
point at which that program should be committed rather than written a third time.

## Keeping a pinned schema current

An update is a reviewed, reproducible step, not a runtime one — an upstream change must not be able to
alter the contract of an action that existing workflows already call:

1. Download a named upstream revision and record its checksum, size and counts.
2. Prune it against the manifest's operation list.
3. Run `RestSchemaDrift::compare(action_set, candidate_schema)`.
4. `RestSchemaDrift::getBreaking()` separates what can be adopted from what needs a decision;
   `getReport()` renders the review. Adopt, or stop and decide.
5. Update the provenance record in the same commit as the schema file.

The guard that keeps this honest is a test in every schema-driven application's `.qtest`:

```qore
list<hash<RestSchemaDriftInfo>> drift = RestSchemaDrift::compare(aset, PaddleSchema::getSchema());
assertEq((), drift, "the manifest and the vendored schema agree");
```

If the committed schema and the manifest ever disagree, that assertion fails rather than the
disagreement surfacing when a user opens an action.

## Per-application test conventions

Beyond the drift-agreement test, each application's `.qtest` should cover:

- **Provenance shape** — the recorded checksum is a SHA-256, the counts are present.
- **Manifest resolution** — every manifest entry resolves against the vendored schema and its recorded
  `operationId` still matches.
- **Reference data** — every dropdown names an action the manifest actually exports.
- **Execution against a fake client** — subclass the application's `*RestClientIo` (see
  `FakePaddleRestClient` in `examples/test/qlib/PaddleDataProvider/PaddleDataProvider.qtest`) so that
  request construction is asserted without network access.

Flattening and its inverse, pruning invariants, drift classification and codec round-trips are tested
once in `examples/test/qlib/RestSchemaActions/`, not per application.

## What the first two ports taught

**Measure a schema-support gap construct by construct, against the parser.** The initial Paddle
evaluation bulk-normalized the document and concluded that OpenAPI 3.1 support was far off. Probing each
construct in isolation showed most of 3.1 already worked and only two constructs failed to parse: a
scalar `type: "null"`, and the object form of `discriminator` (which is the 3.0 form as well — the module
had only accepted Swagger 2.0's string form). Two further defects appeared only while *generating* the
actions: `anyOf: [X, null]` widened the whole composition to `any`, and a single-member `allOf` wrapping
a `$ref` discarded the referenced type. Those two cost 13 of Paddle's action options their types. A bulk
normalizer conflates "the parser rejects this" with "the parser accepts it and produces a poor type",
and the second is invisible until the types are used.

**`servers[0]` is not production.** Paddle's description lists sandbox first, so a client that adopted
the schema's first server would quietly talk to sandbox in production. Derive the URL from a connection
option in both directions and do not pass a URL to the client at all, so a stored URL that disagrees
with the option can never decide where requests go.

**Action identity is a product commitment, never a schema artifact.** An `operationId` may be recorded
and checked, but an upstream rename must be reported as drift, not silently rename an action. When a
schema-driven module replaces an existing application, keep that application's action IDs even if they
break the new module's own naming style — Paddle's are `snake_case` because that is what existing
workflows call, while Stripe's were schema-derived from the start and had no such constraint.

**Export at parity first.** The Paddle port exports 24 actions out of the 99 operations its schema
declares, matching the application it replaced rather than the schema's reach; Stripe exports 38 over 21
of 416 paths. Growing the exported set is a separate, reviewable decision, and the manifest is what
makes it deliberate.

**Webhook events are not generated, and the pruner drops them.** `RestSchemaPruner` discards a
document's top-level `webhooks` section, because pruning selects *operations* and a webhook is not one.
Both applications keep their event list as a module constant, cross-checked against an enum the pruned
document happens to retain. An application that wants event types generated from `webhooks` will need
the pruner to preserve that section first.

**Add a codec only where the wire type lies.** Stripe's timestamps are unix seconds and need
`RestSchemaActionCodecs::UnixSeconds`; Paddle's are RFC 3339 strings and need nothing. A codec applied
where the schema is already honest is a conversion nobody asked for.

**A deliberate duplicate operation is declared, not permitted.** `RestSchemaActionSet` refuses to export
one method/path pair twice, which catches a copy-and-paste slip. When two actions genuinely share an
operation — Paddle's `archive_product` and `update_product` are both `PATCH /products/{product_id}` —
declare the second with `variant_of` rather than relaxing the guard.

## Where per-application detail belongs

In the module's own documentation, not here.

`<App>DataProvider.qm`'s main page, the `<App>Manifest` class documentation and the `<App>Schema` class
documentation are versioned alongside the code they describe and are published to users. Both existing
applications record their overlay decisions, their pagination contract, their environment handling and
their schema rationale there, and that is the right place for it — a `design/<app>.md` per application
would duplicate it, drift from it, and grow this directory by one file per port.

`design/` carries what spans applications. If a decision in a new port is genuinely general — a lesson
the next application would otherwise repeat — add it to the section above instead of starting a new
document.
