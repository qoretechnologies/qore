# REST-Schema-Driven Application Modules

There are two ways to build a data provider application in this repository.

The **hand-written** path writes every action's options and output type in %Qore by hand; it is covered
by [data-provider-development-guide.md](data-provider-development-guide.md) and
[data-provider-checklist.md](data-provider-checklist.md), and it is what most of the `qlib/*DataProvider`
modules do.

The **schema-driven** path generates actions from a vendor's own OpenAPI 3 description using the
`RestSchemaActions` module. `StripeDataProvider` and `PaddleDataProvider` are built this way.

The machinery itself - the division of ownership between schema, manifest and overlay, request
flattening, wire codecs, reference data, drift classification - is documented on the
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
| Options and output types matter more than a bespoke provider shape | The integration needs a bespoke provider shape that is neither an action nor a record table |

Record-based CRUD and server-side search expressions are **not** a reason to choose the hand-written path.
They are built once in `RestSchemaActions` and declared per resource; see *Record Tables* below.

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

`<App>Manifest.qc` is by far the largest source file in such a module - around 55 KB in both
applications. That is expected and correct: it is where every product decision lives, and it is the
file a reviewer should read to know what the application actually offers.

`<App>Schema.qc` and `<App>Manifest.qc` are the two files with no counterpart in a hand-written
application, and the two whose class-level documentation carries the application's durable rationale.

### The five registrations that live outside the module

A schema-driven application is still an ordinary Qore module, so it needs the same five entries
outside its own directory that any other one does. They are listed here because everything else on
this page is inside the module, and because **all five applications in the first series shipped
without all five of them** - the module loaded, every test passed, and the live verification against
each vendor's API worked, because none of these is on the path a `%requires` takes:

| Registration | Where | What breaks without it |
|---|---|---|
| Connection scheme | `qlib/ConnectionProvider/ConnectionSchemeCache.qc` → `SchemeMap` | the scheme is unknown to the index, so the app is dropped from it |
| Provider factory | `qlib/DataProvider/DataProvider.qc` → `FactoryMap` | the module loads but the app never appears in the apps list |
| Presentation catalog | `qlib/<Module>/i18n/data-provider.<base64 app name>/root.json` | `Presentation.qtest` fails; the app has no translatable strings |
| Module list | `doxygen/lang/120_modules.dox.tmpl` | the module is missing from the published module index |
| Release note | `doxygen/lang/900_release_notes.dox.tmpl` | the new module is unannounced |

Both `doxygen/lang/` lists are maintained in **alphabetical order**; insert in place rather than
appending. The presentation catalog is generated, never hand-written - see the checklist for the
command, and regenerate it whenever a display name or description changes.

The two failures compound in a way worth knowing about: the presentation check reaches an app only
once its factory is in `FactoryMap`, so a module missing both is reported as missing *neither*.
Adding the factory is what made the catalogs' absence visible.

[data-provider-checklist.md](data-provider-checklist.md) is authoritative for all five and for
everything else a module owes the platform - its sections 1, 2 and 9-11 apply to a schema-driven
application unchanged; only sections 3-8 and 12 are taken over by the manifest and its overlay.
**Run it before calling a port finished.**

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

Note how little pruning removes from the component schemas in both cases - around half. Entity types in
a real API reference each other heavily, so do not expect the closure to be small; the win comes from
dropping the operations the application does not export and everything reachable only from them.

Three properties of the pinned schema are load-bearing:

- **Provenance describes the unmodified upstream document, not the committed artifact.** The
  `<App>SchemaProvenanceInfo` hashdecl records the source URL, the SHA-256 and byte size of the file as
  downloaded, and its path/operation/schema counts. Recording the pruned file's checksum instead would
  be useless: the next import has to be diffed against the same baseline the last one started from.
  Stripe additionally records the `api_version` its schema describes, because `StripeRestClient` sends
  that version with every request and the two must move together.

- **A vendored schema is never edited by hand, and house style does not apply to it.** It is generated
  output: the next import rewrites the file, so an edit is silently temporary and shows up as drift in
  the meantime. This is not hypothetical - a repository-wide em-dash sweep reached
  `stripe-openapi3.json` and turned Stripe's "no longer recommended—use the Payment Intents API" into
  "recommended-use", which reads as a typo in every rendered description and would have been reverted by
  the next import. Vendor text is quoted, not written here; a formatting rule that applies to our own
  prose stops at the vendored artifacts, the generated i18n catalogs and anything else a tool produces.

- **The schema is parsed on first use, behind a `Mutex`, into a `static` member** - not when the module
  loads. A program that only needs the connection never pays for it, and the action set is built once
  per process regardless of how many providers are constructed.

- **The schema file must be listed in `CMakeLists.txt`.** `qore_user_module()` takes extra non-source
  files as trailing arguments and copies them next to the built `.qmod`:

  ```cmake
  qore_user_module("qlib/PaddleDataProvider" "paddle-logo.svg" "paddle-openapi.yaml")
  qore_user_module("qlib/StripeDataProvider" "stripe-logo.svg" "stripe-openapi3.json")
  ```

  This is what makes `get_script_dir()` - used by `<App>DataProviderDefs.qc` to locate both the logo and
  the schema - resolve correctly whether the module is loaded from `qlib/` sources or from the
  AOT-compiled `build/qlib-qmod/<App>DataProvider/`. Omitting a file here produces a module that builds,
  installs and loads, and then throws when the first action is opened. Add both files at the same time
  the module is registered.

### The importer

`RestSchemaActions::RestSchemaImporter` performs *download → prune → write → update provenance*, and
`tools/rest-schema-import.qr` is its command line. A fresh import selects operations from a file; a
re-import takes them from the manifest itself, so the two cannot drift:

```sh
QORE_MODULE_DIR=qlib tools/rest-schema-import.qr \
    --source=https://gitlab.com/gitlab-org/gitlab/-/raw/master/doc/api/openapi/openapi_v3.yaml \
    --output=qlib/GitLabDataProvider/gitlab-openapi.yaml \
    --manifest=GitLabDataProvider:GitLabManifest::Manifest \
    --provenance=qlib/GitLabDataProvider/GitLabSchema.qc \
    --drift=GitLabDataProvider:GitLabSchema::getActionSet()
```

It rewrites only the provenance members it owns, so an application-specific one - Stripe's `api_version`,
GitLab's `gitlab_version` - survives untouched and stays the importer's caller's responsibility. It
re-parses what it wrote, so an artifact that cannot be loaded is caught during the import rather than at
the next build. A multi-document application imports each document from one `--spec` file.

**Match manifest paths to the document with normalisation, not string equality.** A trailing slash and a
path-parameter *name* are not differences - a parameter name is scoped to its own operation - but comparing
paths literally reports the operation as withdrawn. Evaluating candidates this way produced false
rejections: Trello scored 19/21 until `/boards/` was normalised, Intercom 10/13 until `{contact_id}` was
matched against `{id}`; both are in fact 100%. The importer normalises both, and **fails** on a match that
needed it, because the manifest still has to name the path as the document spells it or the action set will
not resolve it at run time.

## Keeping a pinned schema current

An update is a reviewed, reproducible step, not a runtime one - an upstream change must not be able to
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

- **Provenance shape** - the recorded checksum is a SHA-256, the counts are present.
- **Manifest resolution** - every manifest entry resolves against the vendored schema and its recorded
  `operationId` still matches.
- **Reference data** - every dropdown names an action the manifest actually exports.
- **Execution against a fake client** - subclass the application's `*RestClientIo` (see
  `FakePaddleRestClient` in `examples/test/qlib/PaddleDataProvider/PaddleDataProvider.qtest`) so that
  request construction is asserted without network access.

Flattening and its inverse, pruning invariants, drift classification and codec round-trips are tested
once in `examples/test/qlib/RestSchemaActions/`, not per application.

## What the first two ports taught

**Measure a schema-support gap construct by construct, against the parser.** The initial Paddle
evaluation bulk-normalized the document and concluded that OpenAPI 3.1 support was far off. Probing each
construct in isolation showed most of 3.1 already worked and only two constructs failed to parse: a
scalar `type: "null"`, and the object form of `discriminator` (which is the 3.0 form as well - the module
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
break the new module's own naming style - Paddle's are `snake_case` because that is what existing
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

**Check that the document declares paging before choosing this path.** An action can only offer what the
schema declares, and an overlay cannot add a parameter - that is structural, not presentational. Bitbucket
documents `page` and `pagelen` on a shared *Pagination* page rather than per endpoint, and its published
OpenAPI 3 description declares them on **6 of 331 operations**: every collection accepts them and almost
none says so. A generated listing action therefore cannot ask for page 2 or for more than Bitbucket's
default page of 10, which is a regression against any application that paged by hand - and it is invisible
until somebody opens a dropdown against a workspace with more than ten repositories in it.

Measure this over the **exported** operations, not the whole document, and ignore single-entity reads -
a `GET /pages/{id}` has nothing to page. Across the five applications evaluated for #5394 the measurement
separates cleanly:

| app | exported collection GETs that can be paged |
|---|---|
| GitLab | all of them (`page`, `per_page`) |
| Confluence | all of them (`limit`, `cursor`) |
| GitHub | all of them (`page`, `per_page`) |
| Xero | all of them (`page`, `pageSize`), except the four settings listings that have no paging at all |
| Bitbucket | **3 of 16** - only the three code-search endpoints |

So this is not a general property of published descriptions; it is one vendor's description being wrong in
one specific way.

Where the count is low, repair the document at **import time** rather than working around it in the
application. `RestSchemaNormalizationInfo` declares the repair in the import spec, with a reason:

```yaml
normalize:
  - reason: >-
      Bitbucket documents "page" and "pagelen" on its shared pagination page rather than per endpoint,
      and declares them on 3 of the 331 operations in this document.
    operations: [GET /workspaces, GET /repositories/{workspace}, ...]
    add_parameters:
      - {name: pagelen, in: query, schema: {type: integer, maximum: 100}}
```

Three properties make this safe to do:

- naming an operation the document does not declare, or a parameter it **already** declares, fails the
  import - so a repair that upstream has since made unnecessary is removed rather than left in place
  forever as a silent local edit;
- the repair is recorded in the committed artifact under `x-qore-normalized`, so a reviewer diffing it
  against the upstream document is told why they differ;
- the provenance checksum still describes the *unmodified* upstream document, because that is the
  baseline the next import has to be diffed against.

`relax_oneof` covers the other repair the ports have needed: a `oneOf` whose branches overlap. `oneOf`
means *exactly* one branch matches, so a composition whose branches share every value describes something
that can never be valid - Confluence declares a page body as `oneOf: [PageBodyWrite, PageNestedBodyWrite]`
where neither branch requires any property, and every create and update was therefore rejected before the
request was sent. Naming the pointer turns it into the `anyOf` the API actually accepts.

Repair is for what a vendor documents and omits, or encodes in a way that cannot be satisfied. It is not
for guessing at an API: a construct the vendor does not document belongs nowhere, and an operation the
schema describes wrongly rather than incompletely is the hand-written case above.

**Almost no identifier is global, so a dropdown usually needs scope.** A branch belongs to a repository, a
page to a space, a member to a workspace. `RestSchemaReferenceDataInfo::options_from` forwards named options
of the action being filled in into the listing action, and leaves the dropdown empty until they have values.
Without it a scoped dropdown either lists the wrong scope or cannot exist, which is a regression against any
TypeScript application being replaced - every one of them passed the enclosing option to its
`get_allowed_values` function.

**A dropdown can only name an action the manifest exports, so exporting one is sometimes the point.** The
GitLab application offered milestone, topic and user dropdowns whose endpoints it never exported, so they
returned nothing or the authenticated user alone. Three read-only listings were added to the manifest for no
other reason than to fill them. Check every `get_allowed_values` helper against the exported set before
deciding a port is at parity.

**When the schema cannot describe an operation, hand-write that one action - do not repair the schema.**
GitLab's own description declares `POST /projects/{id}/repository/commits` with a `multipart/form-data` body
whose only property is `file`; the real request carries `branch`, `commit_message` and an `actions` array.
15 of that document's 656 request bodies carry the same generated placeholder. An overlay must not invent a
request contract - that hides an upstream defect behind something that looks like presentation - so the
action is written by hand, keeps its action ID, and is registered alongside the generated ones. The two
paths are not exclusive, and this is the case that proves it.

**Measure such a defect before reacting to it.** One placeholder body in 24 exported operations is a
hand-written action; a document where most bodies are placeholders is not a candidate for this path at all.

**A watch provider must inherit the watch base first.** `AbstractDataProvider` and
`AbstractWatchDataProviderBase` both implement `observersReady()`, %Qore resolves a method to the first
`inherits` branch that declares it, and `AbstractDataProvider`'s implementation reports the operation as
unsupported - so listing the data provider base first leaves every subscription failing with `UNIMPLEMENTED`
at run time, not at build time.

**Publish the event type from the schema.** A polling event provider that declares `supports_observable`
must implement `getEventTypesImpl()`, and the type is already available: it is the record type of the
listing action the provider polls, taken from the action set the application ships. A hand-written second
copy of that shape would drift from the schema the actions use.

**A deliberate duplicate operation is declared, not permitted.** `RestSchemaActionSet` refuses to export
one method/path pair twice, which catches a copy-and-paste slip. When two actions genuinely share an
operation - Paddle's `archive_product` and `update_product` are both `PATCH /products/{product_id}` -
declare the second with `variant_of` rather than relaxing the guard.

## What the GitHub and Xero ports added

**Read the superseded application's action IDs out of its i18n catalog, not out of the schema.** The
TypeScript loader derives an action ID from the vendored `operationId`, but the action catalog accepts only
`A-Za-z0-9_-`, so a slash-bearing `operationId` was registered with the slash replaced. GitHub's IDs are
`pulls-list` and `repos-get`, **not** `pulls/list` - and the only place that is written down is
`ts/src/i18n/en/apps/<App>/index.ts`, which is keyed by the registered name. Getting this wrong renames
every action in the application while looking like it preserved them.

**A connection-owned value still has to be sent.** `RestSchemaFieldOverlayInfo::ignore` exists for exactly
the value a connection owns - an API key, Xero's `Xero-Tenant-Id` - but dropping it from the action left a
request the schema rejected as incomplete, because the client only merges its own default headers when the
request is finally sent. An ignored **header** is now filled in from the client's default headers when the
request is built, which is what makes `ignore` mean what it documents. A header the client does not supply
is still reported missing, because that is the truth.

**A multi-document application needs peer reference data.** One action set per document routes itself, but
dropdowns do not split along the same lines: a Xero project names a *contact*, and contacts are declared in
the accounting document. `RestSchemaActionSet` therefore takes optional **peer sets** whose reference data
it may use, and the listing runs against the set that declares it - the only way its URI prefix can be
right. Peers are given at construction, so the sets are built in dependency order and a cycle cannot be
expressed.

**Deduplicate a dropdown's values.** GitHub's owner dropdown is the `owner` of each repository the
credential can see, and the same owner appears on every repository it owns. Two entries with the same value
are indistinguishable to whoever is choosing one, so `RestSchemaActionDataProvider` now keeps the first and
drops the rest.

**A watch provider needs the record's ID field, not just its timestamp.** `AbstractTimestampWatchDataProvider`
breaks a tie between records sharing a timestamp with `getIdField()`, whose default is `"id"`. Xero names
every identity after its type - `ContactID`, `InvoiceID` - so without the override **nothing was ever
delivered**, and the symptom was an empty feed rather than an error.

**When three schemas describe one action differently, hand-write it.** This is the `variant_of` problem one
level up. Xero publishes a payroll description per region, and Australia's is a different API version from
New Zealand's and the United Kingdom's - the create takes an array in one and a single object in the other
two. A registered action has exactly one option shape, so generating from any one region would publish that
region's contract to everybody. The two employee actions are written by hand, take the region from the
connection, and normalise the response envelope; they keep their action IDs. The rule from GitLab's
create-commit generalises: *the schema must be able to describe the action the application means*, and three
schemas that disagree cannot.

**Measure a webhook trigger against what it actually subscribes to.** GitHub's `create` event covers
branches *and tags*, and its `issues` event covers `opened`, `edited`, `closed` and a dozen more - so a
trigger called "New Branch" or "New Issue" that forwards every delivery is reporting something other than
what it says. Each event now raises only the change its name describes, with an `all_actions` option that
restores the old firehose for anyone who depended on it. The same measurement found that the superseded
GitHub application's `new_review_request` trigger subscribes to reviews being *given*, not requested; the
payload is the commitment, so the behaviour was kept and the presentation corrected.

**A webhook with no secret is not a webhook worth having.** The superseded GitHub application registered its
hooks without `config.secret`, so GitHub sent no signature and none could be checked: every trigger would
fire on anything posted to its endpoint by whoever learned the URL. A generated per-subscription secret and
a `X-Hub-Signature-256` check are the minimum, and are cheap.

**A vendor can withdraw an endpoint the port depends on, and only live traffic will tell you.** Bitbucket
removed every cross-workspace listing - `GET /2.0/workspaces`, `GET /2.0/repositories` and the
`/2.0/user/permissions/…` family all answer **410 Gone, "CHANGE-2770"** - while still *declaring* them in
its published OpenAPI description. So the schema resolved, the manifest agreed, drift was zero, and 12 test
cases passed against a document describing endpoints that no longer exist. The application's workspace
dropdown, which scopes every other dropdown it offers, could not be filled at all.

Nothing in the schema-driven path can catch that: a description is what the vendor says, and this is a case
where they no longer agree with themselves. Only a live call finds it, which is the argument for running
one against each ported application before it ships.

The repair splits by whether a replacement exists:

| | |
|---|---|
| A replacement exists | **repoint the action, keep its ID.** `workspaces_get` moved from `GET /workspaces` to `GET /user/workspaces`. That is what an action ID being a product commitment means: the operation behind it changed, the name workflows call did not. |
| Nothing replaces it | **withdraw the action.** `repositories_get` listed public repositories across all of Bitbucket; Atlassian's guidance is to list the workspaces and ask each one, which another exported action already did. Keeping a name that cannot work is worse than removing it. |

Repointing an action to a path the committed document does not yet contain is the bootstrap problem again:
the module cannot load, so the importer cannot read the manifest. Import once from an `ops` file **with the
spec's `normalize` block**, then re-import from the manifest. Bootstrapping with a bare `--ops` list drops
the repairs, and the next import fails on an overlay naming a parameter the repair was supposed to add.

**An option is a value, not a URL fragment - the transport escapes it.** `RestSchemaDataProvider` used to
place query values and path variables into the URI verbatim. That works only while no value contains a
reserved character, and the first live GitHub search broke it outright: an unescaped space ends the HTTP
request line, and the response came back without headers at all. A Xero filter (`Type=="BANK"`) and a file
called `a file with spaces.md` are the same problem in a query argument and a path segment. Both are now
percent-encoded, with a list's separating commas left literal because they are the separator rather than
part of any element.

This settled an open question in the GitLab port, which had documented the opposite contract - that the
user supplies an *already*-encoded project path (`group%2Fproject`). That cannot be right: a value the
caller pre-escapes is a value the framework cannot escape, so the moment one contains a space there is no
correct answer. The option now takes the path as it reads and the transport escapes it, which is what
GitLab's single `{id}` path segment needs and what every other option already did.

Confirmed against live GitLab, which is worth stating as measurement rather than argument: the same
project answers **200 escaped** and **404 unescaped**. The unescaped form is what the port sent before this
change, so every action naming a project by its path was broken, and no test caught it because a fake
client records whatever path it is handed.

**An OAuth2 scope set is a commitment, like an action ID.** The platform publishes an OAuth2 client per
application (`qrest dataprovider/apps/<App>/oauth2_clients`), and for a replacement it is the client
registered for the application being replaced. Where the provider validates a request against the scopes the
**registered application** holds, asking for anything outside that set fails the authorization outright,
before the user is shown a consent screen at all. The port changed GitLab's from the superseded app's
`api, profile, email` to `api, read_user, email` - `read_user` is a perfectly valid GitLab scope, just not
one that application was registered for - and every authorization died with *"The requested scope is invalid,
unknown, or malformed."*

So diff the scopes against the superseded app the same way the action surface is diffed, and remember that
providers differ in whether they check:

| | |
|---|---|
| GitLab, Atlassian, Xero | validate against the application's registered scopes; a new scope needs the application re-registered, not a code edit |
| GitHub | OAuth Apps carry no registered scope list, so a request may ask for more - which is why adding `delete_repo` was safe |

Asking for **fewer** scopes is always safe, which is why Confluence's reduced set is not a problem.

**Before the flag day, diff the two action surfaces mechanically.** A port is not finished when its tests
pass; it is finished when it exports everything the TypeScript application it replaces exported. Both
sides can be enumerated exactly, so this is a diff and not a reading exercise:

- the TypeScript surface is in the app's i18n catalog under
  `module-v8/qlib/TypeScriptActionInterface/i18n/data-provider.<base64 app name>/root.json` - every key
  matching `app.<app>.action.<base64 action id>.display_name` names one exported action;
- the native surface is `DataProviderActionCatalog::getActions("<app>")`, after `%requires`-ing the
  module.

Every ID present on the left and absent on the right is a workflow that breaks on the migration, and each
one needs a decision on the record: ported, deliberately withdrawn, or a gap to close. Run on the five
schema-driven ports, this found three defects that every other check had passed:

- **GitHub's record tables were unreachable from the action surface.** The module builds `issues`, `pulls`
  and `releases` correctly, but registered no action against them, so the `search`, `search-single` and
  `create` actions the TypeScript app offered simply vanished. Tables are reached by *actions*; declaring
  the providers is only half of it. `SalesforceRestDataProvider` is the pattern - one action per record
  operation against `"/tables/{table}"`, with `data_dependent_options` resolving the per-table options.
- **GitLab lost `get_project_id_by_url`**, an action no schema can generate because no GitLab endpoint
  takes a URL.
- **The GitLab commit action had not kept up with the escaping change above.** It builds its own URI
  instead of going through `RestSchemaDataProvider`, so it was still substituting the project verbatim
  while every generated action had moved to escaping it. The same value therefore behaved differently in
  two actions of one application. Nothing caught it because the project dropdown supplies a numeric ID,
  which is unaffected - the divergence is only reachable by typing a path, which the option explicitly
  supports and its own documentation recommends. **A change to how the framework builds requests has to be
  applied to the hand-written actions too**; they are precisely the ones that opted out of it.

A deliberate withdrawal is a real outcome and belongs in the commit message, not silence: Bitbucket's
`repositories_get` is gone because Bitbucket withdrew the endpoint, and that is the answer for it.

**Compose an event's payload type from the pinned schema's components.** The pruner drops a document's
webhook section, and GitHub's payloads are under an `x-webhooks` extension the pruner would have to learn.
But the *objects* those payloads carry - an issue, a repository, a user - are component schemas the exported
operations already reference, so an event type can be composed from them. Only the few shapes a vendor does
not publish as reusable components stay open hashes. That keeps the event and the actions describing the
same record the same way.

## What the Intercom port added

**A union request body cannot be published as one option shape.** This is the third distinct reason to
hand-write an action, and it is not the same as GitLab's placeholder body or Xero's three disagreeing
regional schemas. Intercom declares its conversation reply as
`oneOf: [contact_reply_conversation_request, admin_reply_conversation_request]`, and the first of those is
itself a `oneOf` of three more. The flattener lifts an object body's *properties*, and a union has none of
its own, so the generated action came out with the path variable and **no way to supply the reply at all** -
not a bad option shape, an absent one.

The test that decides what to do is the same one as always: *can the schema describe the action the
application means?* Here it can - one branch describes a teammate's reply exactly - and only the union
around it cannot be flattened. So that branch is written out by hand, the action keeps its ID, and it is
registered alongside the generated ones. `relax_oneof` is the wrong tool: it exists for a composition whose
branches overlap so completely that nothing can satisfy it, and these branches are genuinely alternative.

Two consequences worth knowing. The operation is dropped from the manifest, so no operation selects that
path and the pruner drops it from the committed artifact - which is correct, because a hand-written action
does not resolve through the schema. And the hand-written action must not diverge from the generated ones
on the conventions they share: it escapes its path variables the same way and applies the same timestamp
codec, both of which are asserted in the module's tests.

**Diff the vendor's own versions for endpoints that were removed, not only for endpoints that changed.**
A port usually moves an application from an old pinned API version to the current one, and the superseded
application's `get_*_allowed_values` helpers are written against the *old* one. Intercom removed the Help
Center **sections** endpoints at version 2.10 - its 2.9 description declares `/help_center/sections`, and no
description from 2.10 onward declares either it or its single-item sibling - so the section dropdown the
TypeScript application shipped has been calling a removed endpoint against every workspace for as long as
they have been on 2.10 or later.

Nothing in the port would have caught that: the dropdown is helper code in the application being replaced,
not an operation in the new schema, so it is absent from the drift report and from the action-surface diff
alike. Downloading both versions of the description and diffing the path sets takes a minute and is the only
thing that finds it. The repair is a decision, not a mechanism - here the dropdown is **withdrawn** and the
option narrowed to the one value that still resolves, because Intercom now returns what used to be a section
as a nested collection.

**A description can contradict its own example, on the same operation.** Intercom declares the `contact_id`
path variable of `POST /contacts/{contact_id}/notes` as `type: integer`, types the same value as a `string`
on every other operation that takes it - including the two tag operations on the very next path - and prints
`"contact_id": "6762f0ad1bb69f9f2193bb62"` in that operation's own example block. A contact ID is a
24-character hex string, so the generated option would have rejected every real one.

This is a **type presentation override** on the field overlay, not a normalization: `normalize` adds
parameters a vendor documents and omits, and cannot retype one. The request contract is unchanged - the value
still travels in the same path segment - so the overlay is expressing what the operation's own example says.
Worth a pass over the scalar types of path variables that appear on more than one operation: a value the
document types two ways is types one of them wrongly.

**A header every operation declares and none requires is still the connection's.** Intercom's dated API
revision rides an `Intercom-Version` header declared, optional, on all 226 operations. Optional is the
dangerous case rather than the safe one: a request that omits it does not fail, it silently rides whichever
revision the workspace's own app package names - so the same integration behaves differently in two
workspaces, and changes behaviour when somebody edits that package. The client sends the revision the pinned
description describes, every action drops the header with `ignore`, and the two are asserted equal in the
module's tests, because an action generated from one revision and sent against another is making a request
the description does not describe.

## What the Klaviyo evaluation added

**An application whose TypeScript predecessor was hand-written does not port at parity for free.** Every
port before Klaviyo replaced an application built with `buildActionsFromSwaggerSchema` over an
`allowed-paths` map, so its option names were already the *vendor's* - Xero's catalog even lists an option
called `xero-tenant-id`, which is the header name. There was nothing to preserve that the schema did not
already say, and the ports looked mechanical because of it.

Klaviyo's application is 21 hand-written `createLocalizedAction` calls over the vendor's SDK, with a
designed camelCase surface - `phoneNumber`, `pageSize`, `additionalFields` - and **not one of its ~70
option names matches the wire**. Workflows supply option values by name, so generating from the schema
renames all of them. Check how the superseded application builds its actions before estimating a port:
that, not the size of the API, is what decides whether the option surface is free.

**A body wrapped in a fixed envelope is lifted from inside it.** `flatten_body` lifts an object body's
top-level properties, which is the right depth when the body *is* the object being sent and the wrong depth
for a vendor that wraps everything. Klaviyo is JSON:API:

```json
{"data": {"type": "profile", "attributes": {"email": "...", "first_name": "..."}}}
```

Lifting one level publishes a single `data` option with everything meaningful two levels inside it, which
is unusable as a form and unrecognisable to an existing workflow.
`RestSchemaActionInfo::request_unwrap` names the path to lift instead - `("data", "attributes")` - so the
action offers `email` and `first_name`, and the envelope is rebuilt when the request is sent. It is the
mirror of `response_unwrap`, and it is opt-in: an application that does not set it is unaffected.

Two properties make it safe rather than a way of inventing a contract:

- a property the schema declares **required with exactly one permitted value** is filled in automatically
  wherever it sits in the envelope, because there is nothing to choose - JSON:API's `type` discriminator is
  always this, and publishing it as a required option would make every existing call fail;
- **every other property of the envelope is published as an option** at the level it belongs to, so that
  lifting the body cannot quietly withdraw part of the request contract. JSON:API carries the identity of
  the object being changed as `data.id` beside the attributes, and an update that could not send it would
  not work at all.

Such an option is addressed in the overlay by its path within the body - `"body.data.id"` - which is also
how it is renamed when it collides with a path variable of the same name. A required one that must simply
repeat a path variable is best given `value_from` naming it, which is the Confluence case one level up:
the caller supplies the identity once and it travels in both places.

### `lift`: a grouping object is not a domain object

Flattening deliberately stops at the top level of the body, because a property that is itself an object is
usually a domain object worth supplying whole. Some are not. Klaviyo carries a postal address as a nested
`location` object, and the application being replaced published `city`, `region`, `country` and `zip` as
ordinary options; publishing `location` instead would have asked a user to build a hash by hand and broken
every workflow that set one of the four.

The `lift` field overlay publishes an object-valued field's properties as options in place of the field,
and assembles the object back when the request is built. It is applied **after** `codec` and `type`, so a
compound value the API spells as a single string can be presented as a structured type by a codec and then
lifted into individual options - which is how Klaviyo's `filter` query argument becomes the options the
superseded application offered rather than a DSL string nobody would type. A lifted property that collides
with an option the action already has is reported, as any other collision is.

## A connection has to configure itself

**A connection option whose value only exists after authorization must be discovered, never asked for.**
Two of these applications need a value that no user has before the OAuth2 flow runs and that nobody should
have to go and look up afterwards: Xero's `tenant_id` - a credential reaches one or more organisations and
every call has to name one - and Confluence's `cloud_id` - an Atlassian credential is issued against the
gateway rather than against a site. A connection that has been authorized and still does not work is a
defect in the module, not a step for the user, and both of these failed in exactly that way: every Xero
call was rejected for a missing header, and every Confluence call went to the gateway and 404'd, including
the ping.

The mechanism is already there and has to be used on **every** path that can complete an authorization,
because different callers complete it in different places:

|Path|Who calls it|
|---|---|
| `RestConnection::processOAuth2TokenResponseImpl()` | the platform, after it exchanges an authorization code - the flow a user actually goes through |
| `RestClient::getUpdateOptionsAfterLogin()` | a synchronous client performing its own login or refresh |
| `RestClientIo::getUpdateOptionsAfterLogin()` | the async client, and the connection's ping poller through it |

Whatever they return is persisted onto the connection, so the discovery belongs in one shared method per
application and each override is three lines. Four rules make it safe:

- **discover with the token just issued**, from the response hash, not with whatever the client still holds;
- **discover once**: skip when the option is already set, which leaves an explicitly chosen value alone and
  keeps a refresh from paying for the call. The precise predicate is "is this connection already usable" -
  the client already sends the header, the client already targets a site - not a flag about the flow;
- **a failed discovery must not break the authorization**: log it and continue, or an unrelated outage at
  the vendor turns a good credential into no connection at all;
- **log the alternatives**: taking the first organisation or site a vendor lists is right, but a user with
  several has to be able to see what was chosen and set the option to another one.

**The requirement can reach back into what a connection may be created with.** Confluence's
`required_options` demanded `cloud_id`, so a connection could not exist until it had a value that only its
own authorization produces - which is what pushed a *site* of `api.atlassian.com`, the gateway, into a real
connection. A connection whose site is discovered has to be creatable without one and target the gateway
until the flow fills it in. The same edit has to make the option's absence unambiguous in both directions:
the gateway is not a site, so it must never be read back out of a URL as one.

**A URL path is not always an API namespace, and the ping is where that bites.** An absolute `ping_path`
normally addresses the **host**, so `RestConnection` drops the connection's own URL path for it - which is
what a connection on `http://host/v1` needs, since a health endpoint outside the namespace would otherwise
be looked for at `/v1/health`. Atlassian is the other case: `https://api.atlassian.com/ex/confluence/<cloud_id>`
puts *which site is being reached* in the path, so dropping it asks the gateway for a site it never named.
The symptom is specific and misleading - **the ping 404s while every action and table on the same connection
succeeds**, because only the ping clears the path. Only the connection knows which of the two its path is,
so it says: `RestConnection::pingInheritsConnectionPath()`, `False` by default, `True` for Confluence. It is
honored on both ping paths - the blocking one and the polling one - because each implements the rule
separately and it was the polling one that failed.

**Name the connection option in the error a missing one produces.** A value the connection owns is dropped
from every action, which is right, and it therefore appears nowhere a user can see when it is missing: the
Xero failure surfaced as a schema type error about `xero-tenant-id` fifteen frames down. `RestSchemaActions`
now reports a **required** connection-owned header the connection does not send as what it is, naming the
option to set - `connection_option` on the field overlay - while an optional one is still simply left out.

## Record tables

A schema-driven application exposes its record-shaped collections as tables under a `tables` child, declared
in a `<App>RecordTables.qc` and reached by record-based actions registered against `"tables/{table}"`. The
machinery is documented on the `RestSchemaActions` module main page; what belongs here is what the fleet-wide
roll-out taught.

**A table is a declaration, not an implementation.** Every one names actions the manifest already exports, so
a table adds no second copy of a path, a request contract or an escaping rule. Sixty-one tables across nine
applications are around 3,000 lines of declaration in total, and none of them makes an HTTP request of its
own.

**A table usually needs the manifest to grow first.** A vendored schema is pruned to what its port exported,
and a port exports the listings and writes its predecessor had - which is never the single reads, updates and
deletes a table needs. Klaviyo needed sixteen new actions and Intercom thirteen; every one was a new action
ID, so none of them touched a preserved ID. Expect a manifest expansion and a re-import, not just a new
`.qc`.

Two mechanical notes on that re-import. The importer loads the application's module to read its manifest, and
the module validates itself against the *pruned* document that does not yet contain the new paths - so the
first import of an expanded manifest has to be bootstrapped, either by staging the upstream document in place
of the vendored one (Klaviyo) or by giving the importer the operation list directly with `ops:` (Intercom,
whose full upstream document does not validate at all: it has defects in operations nobody exports). Once the
new document is written, the ordinary manifest-driven import reproduces it exactly and the drift check passes,
which is the thing to verify before committing.

**Declare `supports_native_search` only where the API takes a filter expression.** Four vendors in this
family publish one - Xero's `where`, Bitbucket's BBQL, Klaviyo's `filter` and Intercom's search tree - and
their tables really do have the server do the searching. The rest narrow a listing with query arguments and
evaluate the rest client-side, which is a different claim, and `supports_pushdown` per operator is where that
detail goes. The three GitHub tables that existed before this work declared native search and did not have
it: `where_cond` never reached GitHub.

**A filter language is almost always smaller than a renderer assumes, and every way it is smaller is a way
a search goes wrong rather than merely slow.** The renderer was written for one shape - infix predicates
joined by AND, OR and NOT into a parenthesised string - and neither of the two applications added after it
fits:

| | |
|---|---|
| Klaviyo has **no OR and no NOT at all**, writes `equals(field,"v")` and joins with a bare comma | a connective a language does not declare fails `canRender()`, so the expression stays whole and is answered client-side; joining an OR's arguments with commas answers with their intersection and looks like it worked |
| Intercom's query is a **tree of hashes in the request body** | `render_query` renders the whole expression and may return any type, while the operator tables still decide what is pushed down |
| Klaviyo indexes each field for particular comparisons - no `>=` on a profile's `created`, one on a campaign's `created_at` | `field_ops` states the matrix per field; a per-field-only `fields` set turns `created >= x` into a **failed** search rather than a wrong one |
| Intercom's `>=` and `<=` do not exist at all | leave them unmapped; mapping `>=` onto `>` silently drops the boundary records |

**Measure the matrix; do not read it.** Klaviyo's is in each argument's own description and was still
verified live. Intercom's description says only "the accepted field that you want to search on" and
enumerates nothing, so all of it was measured - one request per field and operator - which is the only way
anyone would learn that a contact's timestamps refuse `!=` while a conversation's accept it.

**A rendered query is already in wire form.** The argument a table renders into is often the same argument an
action publishes in a *curated* form: Klaviyo's `filter` takes a whole expression, and the listing actions put
a wire codec on it publishing the four named fields the superseded application offered. Passing a rendered
expression through that codec converts it to `nothing`, drops the filter, and answers the search with the
**whole collection** and no error. `RestSchemaRawValue` marks the value so the codec and the request-type
check both leave it alone - which is what lets a table reuse the listing action the manifest already exports
instead of declaring a second copy of the path with the codec left off.

**A column whose write option is spelled differently is dropped, and the write reports success.** Manifests
rename options for a reason - Klaviyo returns a list's `opt_in_process` and its update action publishes
`optInProcess`, because that is the name the application it replaces used - and a write sends the columns the
write action *declares*. `write_field_map` declares the pairs.

**A limit counts matching records.** This is the defect that motivated the whole design and it is worth
stating as a rule: fetch lazily and let `DefaultRecordIterator` apply the limit *after* the residual
predicate. Truncating the fetch first and filtering afterwards under-returns silently, and no test that
passes `NOTHING` for `where_cond` will ever notice.

**Measure what a vendor's description actually says before deriving anything from it.** Three separate
descriptions in this fleet describe their own collections wrongly or not at all, and each needed a different
answer:

| | |
|---|---|
| GitLab declares the 200 response of **every** listing as a single entity rather than an array of them | derive the record type from the response's own fields; the body really is an array at run time |
| Paddle describes a report as `oneOf` seven models, and Stripe an external account as `anyOf` two | no element type to derive: the table names its columns, which are the ones every branch declares |
| Bitbucket declares `q` on its repository and commit-comment listings but not on its pull request one, though the API accepts it | an overlay cannot add a parameter, so that table filters on `state` alone; repairing it belongs in the import spec's `normalize` block |

The layer refuses a table whose record type can neither be derived nor is declared, because a table with
`has_record` and no columns is worse than one that failed while the application was being built.

**An identity is not always one value, and a scope is not the same thing.** GitHub addresses a repository
by its `owner` **and** its `repo`, both path variables, and carries the first as a nested `owner.login` on
the record. `id_parts` declares such an identity as option -> record field path. The distinction that
decides which mechanism to use: a **scope option** identifies the *collection* and is the same for every
record in a search, so the caller supplies it and the listing requires it; an **identity part** identifies
*one record* and varies from row to row within a single listing, so it is read back off each record.
GitHub's repository listing spans owners, which is exactly why the owner cannot be a scope there.

Naming a record is only a shortcut when the predicate is the identity and **nothing else**. A search for
`id == 5 AND state == "open"` names a record *and* asks a question about it; reading that record without
asking the question would answer a search with a record it excluded - and for `updateRecords()` it would
write to a record the caller never matched.

**An identity is what the API's paths take, not what looks like an ID.** GitHub addresses an issue by its
`number` within the repository and never by its global `id`; GitLab uses `iid` for the same reason, `key` for
a CI variable and `slug` for a wiki page; Bitbucket takes a repository's `slug` in every path and describes
that field in no schema, so the identity is a column only `extra_columns` can supply. Getting this wrong
produces a table that reads correctly and cannot write at all.

**A record type must reflect what the API returns, not what its listing projects.** A listing returns a
projection and a single read returns the record, and the two rarely agree: GitLab describes a project with
**24** fields in its listing and **161** in its single read, GitHub a repository with 98 and 105, Confluence
a page with 15 and 20. The record type is therefore derived from both reading actions, the richer one wins,
and a column only the other declares is added rather than lost. Guard the case where the single read
describes *less*: Stripe reads a customer as `anyOf [customer, deleted_customer]`, a union with no fields of
its own, and the listing has to win there.

This is not cosmetic. **A column the table does not declare cannot be searched at all** - the framework
rejects a predicate on an undeclared field before the table is reached - so under-declaring silently removes
functionality. The acceptance test is a live one: read every table and check that no key of a real record is
missing from the record type. That is what found Bitbucket's five.

**A field a vendor returns and describes nowhere is declared with `extra_columns`.** Bitbucket returns a
repository's `slug` - the value that addresses it in *every* path - plus `workspace`, `website`,
`override_settings` and `enforced_signed_commits`, and declares none of them; it returns a pull request's
`description` and declares that nowhere either. Merging a short list over the derived type keeps the schema
primary and does not go stale when the vendor adds a field, which a hand-written record type does. Without
the slug column that table could read and never write: a repository record carries a UUID and a
`full_name`, Bitbucket documents `{repo_slug}` as accepting a brace-wrapped UUID, and that does not survive
this transport - a path variable is percent-encoded and `%7B...%7D` answers 404.

**A listing sometimes returns a membership rather than the thing it names, and the answer is still to
publish the API's shape.** Bitbucket's `/user/workspaces` and `/workspaces/{workspace}/members` answer with
`{type, administrator, workspace}` and `{type, links, user, workspace}` - the slug, the display name and the
account UUID a caller is actually after are one level down. Flattening that with a `to_record` projection
looks tempting and costs more than it looks: the layer requires `to_record` and `to_wire` **together**, so a
table that never writes would declare a conversion that can never run, and a projection also replaces the
derived record type with a hand-written one that goes stale when the vendor adds a field. Publish the shape
the vendor returns, say in the table's description where the useful values are, and let a mapper read
`workspace.slug` as a nested path. A projection is for preserving a predecessor's column names, which is a
product commitment; it is not a way to improve on an API's shape.

**A collection with no identity of its own is still a table.** Neither membership above has an identifier,
and neither vendor publishes a path that reads one, so those tables declare no identity and no single-record
read, and a search is answered by reading the listing. That is the same shape as Xero's `invoice-history`,
an append-only log. The layer only needs an identity when something addresses a record by it.

**A record path that does not resolve fails silently.** Paddle's manifest already unwraps `data` on a single
read, so the tables unwrapping it again read one record as *nothing*, and no test that only searches ever
noticed. `checkTables()` now resolves every `records_path`, `record_path` and `write_record_path` against
the response its action publishes - the same class of check that caught four field errors - and only skips
what it cannot verify, a response describing no fields.

**Check what the vendor cannot do before offering an action for it.** GitHub deletes no issue, pull request or
release; Paddle and Xero delete nothing at all, archiving or voiding instead. Those applications register no
delete action, because an action that could only fail is worse than no action.

**A default value on a search option must not block a predicate on the same field.** GitHub's tables default
`state` to `all` so that a plain search returns open and closed issues alike, as the application they replace
did. If that default were treated as a value the caller chose, no `state` predicate could ever be pushed
down. A defaulted option yields to a predicate; one the caller set explicitly does not, and the two together
mean the intersection.

## Where per-application detail belongs

In the module's own documentation, not here.

`<App>DataProvider.qm`'s main page, the `<App>Manifest` class documentation and the `<App>Schema` class
documentation are versioned alongside the code they describe and are published to users. Both existing
applications record their overlay decisions, their pagination contract, their environment handling and
their schema rationale there, and that is the right place for it - a `design/<app>.md` per application
would duplicate it, drift from it, and grow this directory by one file per port.

`design/` carries what spans applications. If a decision in a new port is genuinely general - a lesson
the next application would otherwise repeat - add it to the section above instead of starting a new
document.
