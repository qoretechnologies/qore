# Migration candidates: module-v8 TypeScript apps → native Qore providers

**Status:** Analysis / design proposal. Tracked by
[#5388](https://github.com/qoretechnologies/qore/issues/5388), branch
`feature/5388_app_migration`.

**Companions:**
- [`cloud-storage-qlib-modules.md`](cloud-storage-qlib-modules.md) — the completed
  S3 / Google Drive / Dropbox migration ([#5386](https://github.com/qoretechnologies/qore/issues/5386))
- [`file-app-improvements-implementation-plan.md`](file-app-improvements-implementation-plan.md)

**Question answered:** now that `AwsS3DataProvider`, `GoogleDriveDataProvider` and
`DropboxDataProvider` are native qlib modules and the corresponding TS apps have
been removed (`module-v8` `4e90a44e`), which app should be next?

**Acceptance bar.** A migration is only worth doing if the Qore implementation
delivers **better UX and more functionality** than the TS app it replaces —
parity is not a deliverable. Every candidate below is ranked on that basis, and
candidates that would only achieve parity are called out as such.

---

## 0. Method and inventory

`module-v8/ts/src/apps/` holds **98 directories, 97 with an `index.ts`**.
(`google-drive/` is the one exception: the app was deleted in `4e90a44e` but its
`helpers/` survive as a shared library for `google-docs`, `google-sheets`,
`google-forms` and `google-meet`, which import file-search and folder-id
allowed-value helpers from it. That coupling is a hidden dependency on a
nominally removed app and is worth untangling on its own.)

The apps fall into three architectural buckets, which determine migration cost:

| Bucket | How actions are produced | Count | Migration cost |
|---|---|---|---|
| **Qore-module-backed** (`IQoreExistingAppWithActions`) | a Qore data provider supplies every action; TS supplies triggers only | 3 — `salesforce`, `dynamics`, `business-central` | lowest |
| **Schema-driven** | `buildActionsFromSwaggerSchema()` over a bundled OpenAPI/Swagger JSON filtered by an `allowedPaths` whitelist | 19 | highest (see §3) |
| **Hand-written** | each action coded in TS | ~75 | proportional to action count |

Qore-side coverage was cross-checked against all 169 `*DataProvider` modules in
`qore/qlib` and the other `module-*` repos. Direct name overlap with a
module-v8 app is limited to `salesforce` → `SalesforceRestDataProvider`,
`gemini` → `GeminiDataProvider`, `hugging-face` → `HuggingFaceDataProvider`,
`claude` → `AnthropicDataProvider`, plus `dynamics`/`business-central` →
`CdsRestDataProvider`, and `google-drive` → `GoogleDriveDataProvider` (done).

### 0.1 The ts-proxy tax

Every app with *any* TypeScript in it keeps Qorus on the `ts-proxy` path: an
out-of-process Node/V8 child spoken to over a UNIX socket with a length-prefixed
YAML protocol (`module-v8/design/ts-proxy.md`). Actions pay a serialization
round-trip per call; **event actions are worse** — each one requires a dedicated
event socket plus a listener thread per proxy in `JavaScriptProgramProxy`, held
open for the lifetime of the subscription, with restart/teardown machinery
around it.

A Qore-native provider removes that process, its socket, its listener threads
and its restart state machine. This makes **trigger-only apps the most
cost-effective migrations per line of code moved** — they carry the full
event-socket overhead while contributing the fewest lines.

---

## 1. Salesforce — migrate; highest ratio of benefit to effort

### 1.1 What is actually left in TypeScript

`apps/salesforce` is **867 lines and contributes zero actions**. Its `index.ts`
is:

```ts
actions: [...mapTriggersToApp(SALESFORCE_APP_NAME, SALESFORCE_TRIGGERS, locale)],
```

with `module: 'SalesforceRestDataProvider'` — every Salesforce *action* in the
catalogue already comes from Qore. The whole TS surface is **four polling
triggers**: `NewSalesforceRecord`, `UpdatedSalesforceRecord`,
`NewSalesforceContact`, `NewSalesforceLead`, each built on
`pollCreatedItemsForTrigger()` over a SOQL `/query` call.

Salesforce is therefore the single app closest to leaving the ts-proxy entirely,
and it is a pure-event app — the worst case for proxy overhead per §0.1.

### 1.2 The replacement already exists and is strictly better

`module-grpc/qlib/SalesforcePubSubDataProvider/` implements the Salesforce
**Pub/Sub API** — gRPC over HTTP/2 with Avro payloads — and grafts itself onto
the existing REST provider rather than creating a second app:

```qore
SalesforceRestDataProvider::SalesforceRestDataProvider::registerChild(PubSubChildName, ...);
DataProviderActionCatalog::registerAction(<DataProviderActionInfo>{
    "app": SalesforceRestDataProvider::AppName,
    "path": "/" + PubSubChildName + "/{channel}",
    "action": "pubsub-event",
    "action_code": DPAT_EVENT,
    ...
});
```

Against the four polling triggers this is a genuine functional upgrade, not
parity:

- **Real-time instead of polled.** Change Data Capture and platform events
  arrive on a live subscription; the TS triggers poll on an interval and are
  bounded by `DEFAULT_TRIGGER_POLL_ITEM_LIMIT`.
- **No missed events.** The subscription tracks its replay position, so a
  dropped connection resumes after the last event delivered (Salesforce retains
  replay IDs for 72 hours). The polling triggers reconcile by high-water mark on
  a unique field and lose anything that churns between polls.
- **Deletes and undeletes become available.** CDC carries `DELETE` and
  `UNDELETE`; a create/update poll cannot see them at all.
- **Custom channels and platform events** — `Order_Shipped__e`, `*__chn` — have
  no polling equivalent whatsoever.
- **Channel discovery.** `SalesforcePubSubEventsDataProvider::getChildProviderNamesImpl()`
  enumerates every subscribable channel in the org, so the picker is live rather
  than a static list.
- **No API-version pin.** `apps/salesforce/constants.ts` hardcodes
  `SALESFORCE_API_VERSION = 'v62.0'` (Winter '25). `SalesforceRestClient.qm`
  negotiates the version dynamically from `GET /services/data`, so the Qore path
  does not silently rot.

### 1.3 Gaps that must be closed first

**Status:** gaps 1 and 2 are now implemented in `module-grpc` — the `change_type`
filter with gap-event semantics, the four named CDC actions as change type preset
children, and a `TopicInfo.can_subscribe` pre-flight check. Gap 3 is resolved (see below) and gap 4 remains,
as does moving `SalesforceRestDataProvider` onto `RestClientIo` (see §0.9 and §S.6
of the implementation plan).

These are real and the migration should not be declared done without them:

1. **No change-type filter.** This is the blocker. The Pub/Sub provider raises
   `EVENT_SALESFORCE_PUBSUB_EVENT` for *every* change on a channel;
   `changeType` appears in the module only inside example data. Reproducing
   "New Record" versus "Updated Record" as distinct catalogue entries requires a
   `change_type` option (`CREATE`/`UPDATE`/`DELETE`/`UNDELETE`, multi-select)
   filtered in `eventReceived()`. Without it the four named triggers collapse
   into one undifferentiated "something changed", which is a **UX regression**
   and fails the acceptance bar in the header.
2. **CDC must be enabled per entity** in Salesforce Setup → Change Data Capture.
   The polling triggers need no org configuration. This is an onboarding
   regression that must at minimum be documented on the action, and ideally
   detected — the REST provider can check the entity's CDC status and return an
   actionable error instead of a silent no-event subscription.
3. ~~**Deployment dependency.**~~ **Resolved:** `module-grpc`, `qore` and all
   other modules ship together, so the Pub/Sub provider is always present
   wherever the Salesforce app is. No degraded mode is needed.
4. **Convenience triggers.** "New Contact" and "New Lead" are just
   `ContactChangeEvent` / `LeadChangeEvent` plus a `CREATE` filter. Once (1)
   lands these are configuration, not code — but they should still be registered
   as named actions so the catalogue does not get *harder* to use.

### 1.4 Recommendation

**Migrate.** Sequence: add the `change_type` filter and CDC-status detection to
`SalesforcePubSubDataProvider`, register the four named event actions against
the `Salesforce` app, resolve the `module-grpc` packaging question, then delete
`apps/salesforce`. The TS deletion is ~867 lines and removes an always-on
ts-proxy child with a live event socket.

---

## 2. Apps that handle files

The marker for file capability on the Qore side is
`DataProviderActionCatalog::registerAppFileApi()`. Exactly four providers
register one: `AwsS3DataProvider`, `DropboxDataProvider`,
`GoogleDriveDataProvider`, `OneDriveDataProvider`.

Scanning module-v8 for real file-transfer semantics (`multipart/form-data`,
`FormData`, `base64`, `Buffer.from`, `arrayBuffer`, `application/octet-stream`)
rather than filename matches — "profile" matching "file" produces a lot of false
positives — gives these candidates:

### 2.1 SharePoint — the strongest candidate after Salesforce

`apps/sharepoint` has 7 actions and **none of them move a file**:
`create-folder`, `create-list`, `create-list-item`, `delete-list-item`,
`search-list-item`, `update-list-item`. The only Graph drive call in the whole
app is `create-folder.action.ts` patching `/drives/${driveId}/root:/${folderPath}`.

SharePoint's primary use is its **document libraries**, and the app cannot
upload, download, copy, move or version a document. Meanwhile
`qlib/OneDriveDataProvider/` already implements exactly that surface against the
same Microsoft Graph drive API — its data types already carry SharePoint
identifiers (`OneDriveSubDataTypes.qc` documents a `siteId` field as "The
identifier of the SharePoint site (for SharePoint items)") and it already
registers a file API.

This is the clearest "more functionality" case in the repo: a
`SharePointDataProvider` built on the OneDrive drive-item layer (or an extension
of it addressing `/sites/{site-id}/drives`) turns an app that cannot touch
documents into one that can, and inherits streaming, the `conn://` file-location
scheme and the shared file-API vocabulary for free.

### 2.2 Supabase Storage — bucket management with no objects

`apps/supabase` exposes `create-bucket`, `get-bucket`, `list-buckets` — and no
object operations at all. You can create a bucket and never put anything in it
or get anything out. Any migration should add the object surface
(upload/download/list/delete/signed URLs) and register a file API.

### 2.3 Firebase Cloud Storage — genuine object store, no Qore provider

`apps/firebase` has a real object surface — `upload-file`, `delete-file`,
`list-files-in-bucket`, `get-file-metadata`, `list-buckets` — plus Auth and FCM
actions. It is a legitimate `registerAppFileApi()` candidate. The known TS
weaknesses from §1.2 of `cloud-storage-qlib-modules.md` apply here too: content
crosses the proxy base64-inflated in a JSON field, and there is no streaming or
resumable upload.

### 2.4 Canva and Contentful — asset APIs

`apps/canva` (`upload-image`, `upload-image-by-url`, `get-image`,
`delete-image`) and `apps/contentful` (an `assets/` action group) are both
asset-management APIs that would benefit from the file API and streaming, but
neither is a general-purpose file store; rank them below the three above.

### 2.5 google-docs

`apps/google-docs` (`upload-document`, `get-document`,
`create-document-from-template`, `append-text-to-file`) already sits on
`GoogleDriveDataProvider`'s territory and imports the orphaned `google-drive`
helpers described in §0. Migrating it would also close that dangling
dependency — but note it is document *content* manipulation (the Docs API), not
file storage, so it is a Docs provider, not a file-API provider.

---

## 3. API and capability coverage across the schema-driven apps

### 3.1 The measurement

For each schema-driven app, the number of paths in the `allowedPaths` whitelist
versus the paths and operations present in the bundled schema:

| App | Allowed paths | Schema paths | Schema operations | Path coverage |
|---|---|---|---|---|
| `gitlab` | 32 | 867 | 1112 | 3.7% |
| `github` | 24 | 630 | 986 | 3.8% |
| `esignature` | 7 | 208 | 402 | 3.4% |
| `jira` | 13 | 368 | 553 | 3.5% |
| `magento` | 14 | 362 | 455 | 3.9% |
| `netsuite` | 18 | 378 | 998 | 4.8% |
| `stripe` | 20 | 387 | 559 | 5.2% |
| `zendesk` | 15 | 327 | 458 | 4.6% |
| `bitbucket` | 20 | 186 | 322 | 10.8% |
| `confluence` | 13 | 141 | 202 | 9.2% |
| `mailchimp` | 17 | 164 | 270 | 10.4% |
| `pipedrive` | 16 | 179 | 279 | 8.9% |
| `asana` | 25 | 137 | 189 | 18.2% |
| `trello` | 21 | 186 | 254 | 11.3% |
| `intercom` | 13 | 76 | 108 | 17.1% |
| `freshdesk` | 17 | 28 | 49 | 60.7% |

Stale version pins found alongside: `salesforce` `v62.0`, `stripe`
`2024-12-18.acacia`, `linkedin-organizations` `202502`, `notion` `2025-09-03`.
(`twilio` `2010-04-01` and `sendgrid` `v3` are permanent upstream version
strings, not staleness.)

### 3.2 What this does and does not mean

Low coverage here is **curation, not neglect** — the whitelist is what keeps the
catalogue usable, and Qore's `DataProviderActionCatalog` is designed to
auto-inject a generic `make-api-call` action for any REST-derived app
(`setGenericApiCallActionBuilder()`), so in principle the unlisted 96% is not
unreachable. Coverage percentage on its own is therefore **not** a migration
argument.

> **Caveat — that injection is currently failing for the TypeScript apps.**
> Loading connections in a live Qorus tree logs, for every TS-backed app:
>
> ```
> DataProviderActionCatalog: failed to auto-register generic API call action for
> app "Notion": RUNTIME-TYPE-ERROR: <lvalue path assign> expects type
> 'hash<string, bool>', but got no value instead
> ```
>
> observed for Notion, Attio, Craft, Hubspot, Paddle, ZohoCRM, Gitlab, Xero and
> SharePoint among others. So on a current tree the schema-driven apps have
> **neither** the unlisted 96% via `make-api-call` **nor** any way to reach it —
> which strengthens the case for migrating them, but the right first move is to
> fix the registration bug, not to migrate around it. Root cause not identified:
> three-level typed-hash auto-vivification works in isolation, and
> `modulePathsContain()` already takes `*hash<string, bool>`, so neither of the
> obvious candidates explains it. Needs its own investigation.

### 3.3 The real asset is the override layer, and it is expensive

The schemas were edited to work and to improve UX, and `TypeScriptActionInterface`
adds a layer letting TS/JS code and config override call behaviour. Its size:

| Mechanism | Occurrences |
|---|---|
| `get_allowed_values` call sites | 2050 |
| distinct `*allowed-values*.ts` helper files | 617 |
| `allowed_values_creatable` | 855 |
| `default_value` | 771 |
| `override_options` | 557 (across 19 apps) |
| `set_options_post_auth` | 23 |
| `url_template_options` | 24 |
| `swagger_type_overrides` | 3 |

A naive "point `SwaggerDataProvider`/`OpenApi3DataProvider` at the same JSON"
migration would **lose all of it** and ship a worse product — failing the
acceptance bar outright.

The encouraging part: much of the plumbing is already Qore, not TypeScript.
`TypeScriptActionInterface.qc` normalizes `swagger*` → `openapi*` and hands
schema, path filter and type overrides to `TypeScriptActionInterface::getRestSchema()`,
a Qore static method; and Qore's own `AbstractDataField`/`QoreDataField` already
support `allowed_values`, `allowed_values_creatable`, `element_allowed_values`
and defaults natively. What is genuinely TypeScript is the ~617 *dynamic*
allowed-value callbacks (they make live API calls to populate pickers) and the
hand-written actions.

**Prerequisite for any schema-driven migration:** lift the
schema + `openapi_paths` + `openapi_type_overrides` loading out of
`module-v8/qlib/TypeScriptActionInterface` into a core qlib module so a native
Qore app can declare a curated, override-corrected schema without any TS. Note
that `openapi_type_overrides` currently has **no consumer anywhere in the qore
repo** — it exists only in module-v8. That generalization is the unlock; without
it every schema-driven app migration re-implements the same machinery.

### 3.4 Recommendation

Do **not** migrate schema-driven apps individually yet. Do the
`getRestSchema()` generalization first, then migrate one mid-sized app
(`intercom` at 13 paths / 76, or `freshdesk` at 17 / 28) as a proof of the
pattern before touching `github`, `gitlab`, `jira` or `stripe`.

---

## 4. Ranked recommendation

| # | Candidate | Why | Effort |
|---|---|---|---|
| 1 | **Salesforce** | 867 LOC and 0 actions left; real-time gRPC events replace polling; removes an always-on event proxy | low — close §1.3 gaps |
| 2 | **SharePoint** | cannot move documents at all today; `OneDriveDataProvider` already implements the Graph drive layer | medium |
| 3 | **Generalize `getRestSchema()`** | unblocks all 19 schema-driven apps | medium — infrastructure |
| 4 | `google-docs` | closes the orphaned `google-drive` helper dependency | medium |
| 5 | Canva / Contentful assets | asset APIs, narrower benefit | medium |
| — | **Google Cloud Storage** *(instead of Firebase)* | Firebase Storage **is** GCS — the TS app calls `storage.googleapis.com` directly — so one GCS module covers Firebase Storage and every other GCP customer, without Firebase's eight-host problem | medium, on demand |
| — | ~~Firebase~~ | **deferred**, superseded by GCS above | — |
| — | ~~Supabase~~ | **deferred**; its database half is already better served by Qore's `pgsql` driver + `DbDataProvider`, since Supabase exposes a direct PostgreSQL connection | — |

Firebase and Supabase were ranked 3rd and 4th in the first version of this
analysis. They are demoted on the grounds that both are developer-facing
platforms rather than enterprise integration endpoints, that Supabase's valuable
half is already reachable — and more capably — over `pgsql`, and that Firebase's
valuable half is a GCS bucket with a much larger addressable audience under its
own name. Full implementation plans for both are retained in
[`salesforce-sharepoint-supabase-firebase-migration-plan.md`](salesforce-sharepoint-supabase-firebase-migration-plan.md)
should that judgement change.

**Architectural constraint on every candidate above:** all I/O runs on the async
socket I/O controller — data providers hold `RestClientIo`, never `RestClient`.
See §0.9 of the implementation plan, which also records that
`SalesforceRestDataProvider` does not yet comply.

## 5. What migration does not buy

- **It does not remove the ts-proxy from a deployment** unless *every* app in
  that deployment is native. Salesforce, S3, Drive and Dropbox together still
  leave 90-plus TS apps.
- **It does not automatically improve API coverage.** Coverage is a curation
  decision (§3.2), and Qore already has `make-api-call` for the long tail.
- **It does not preserve the UX overrides for free** (§3.3) — those must be
  ported deliberately, and the acceptance bar requires improving on them.
