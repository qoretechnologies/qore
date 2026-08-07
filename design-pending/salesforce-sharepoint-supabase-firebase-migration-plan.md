# Implementation Plan — migrating Salesforce, SharePoint, Supabase and Firebase to native Qore

**Status:** The recommended scope is **implemented** — phases S, P0, SP and V are done on
branch `feature/5388_app_migration`, tracked by
[#5388](https://github.com/qoretechnologies/qore/issues/5388). Phases SB (Supabase) and FB
(Firebase) remain deferred proposals, which is why this document is still in
`design-pending/` rather than `design/`.

| Phase | Status |
|---|---|
| S — Salesforce | Done; TS app deleted, Pub/Sub `change_type` filter and CDC enablement in `module-grpc`, REST providers on `RestClientIo` |
| P0 — OneDrive drive-path hook | Done; `getDriveBasePath()` takes the request, so the drive can be chosen per call |
| SP — SharePoint | Done; `qlib/SharePointDataProvider`, `SharePointRestConnection`, TS app deleted. 28 actions vs the TS app's 7 |
| V — API version pins | Done; only Shopify needed a change (a release candidate was pinned in production). Each other pin was checked against upstream and left, with reasons recorded in the commit |
| SB — Supabase | Deferred, not started |
| FB — Firebase | Deferred, not started — superseded by a Google Cloud Storage provider if demand appears |

**Deployment note.** The `sharepoint` connection must be re-created against the native
`sharepoint://graph.microsoft.com` scheme: the old one was a TypeScript connection
(`tsrest-sharepoint`), and that scheme no longer exists now that the TS app is gone, so the
existing connection resolves as `(InvalidConnection)` until it is migrated.

**Recommended scope:** Salesforce (phase S) and SharePoint (phases P0 + SP), plus the
version pins (phase V). **Supabase and Firebase are documented in full but deferred** —
see [§Deferred](#deferred-why-supabase-and-firebase-are-below-the-line) for why, and for
the Google Cloud Storage provider that should replace Firebase if demand appears.

**Repos touched:** `qore`, `module-grpc`, `module-v8`.

**Analysis this plan implements:**
[`module-v8-migration-candidates.md`](module-v8-migration-candidates.md).

**Precedent:** [`file-app-improvements-implementation-plan.md`](file-app-improvements-implementation-plan.md)
(issue [#5386](https://github.com/qoretechnologies/qore/issues/5386)) — the
completed S3 / Google Drive / Dropbox migration. Its ground rules apply here
unchanged unless restated.

**Normative references.** Every phase conforms to these:
- `design/qore-module-structure.md`
- `design/data-provider-development-guide.md`
- `design/data-provider-checklist.md`
- `design/data-provider-semantic-string-types.md`
- `design/generic-api-call-action.md`

**Reference implementation.** `qlib/OneDriveDataProvider/` — async-first on
`RestClientIo`, flat `DefaultChildMap`, per-action leaf providers, an events
provider, and `registerAppFileApi()`.

---

## 0. Ground rules for the whole effort

### 0.1 The acceptance bar

A migration is done only when the Qore implementation is **better than the TS
app it replaces** — more functionality and better UX. Parity is not a
deliverable. Each phase below states its own "must beat the TS app at" list;
those are gates, not aspirations.

### 0.2 Identity is preserved

App names, logos and OAuth2 client registrations carry over unchanged so
existing connections keep resolving.

| TS app | `AppName` | Logo file | Connection scheme | Factory name |
|---|---|---|---|---|
| `apps/salesforce` | `Salesforce` (already registered by Qore) | existing `salesforce-logo.svg` | `sfrest` (existing) | existing |
| `apps/sharepoint` | `SharePoint` | `sharepoint-logo.svg` | `sharepoint` | `sharepoint` |
| `apps/supabase` | `Supabase` | `supabase-logo.svg` | `supabase` | `supabase` |
| `apps/firebase` | `Firebase` | `firebase-logo.svg` | `firebase` | `firebase` |

Logos come from base64-decoding each TS `constants.ts` `*_APP_LOGO` (SharePoint's
is inlined directly in `index.ts`). Per `design/data-provider-checklist.md` the
logo must be **square** and a separate file loaded at module level, never an
inlined string — follow `OneDriveDataProviderDefs.qc:31`. The SharePoint SVG is
`viewBox="0 0 1992.333 1946"` and must be normalized.

### 0.3 The coexistence constraint — and why Salesforce is exempt

`module-v8` `4e90a44e` established this the hard way: an app registered by a
native Qore module **cannot coexist** with the same app in the TS catalogue —
loading both fails outright with

```
APP-ERROR: Application "AmazonS3" is already registered
```

(`TypeScriptActionInterface.qc:828`). Removing the TS app is therefore a
**prerequisite for testing the native module at all**, not a closing cleanup.
For SharePoint, Supabase and Firebase this forces a flag day: the TS removal and
the native module must land together, and the native module cannot be smoke
tested in a live catalogue until the TS app is gone.

**Salesforce is the exception, and it is a significant one.** `apps/salesforce`
is `IQoreExistingAppWithActions` and goes through `registerExistingApp()`
(`TypeScriptActionInterface.qc:1012-1050`) rather than `registerApp()`. That
path is structurally different: its duplicate checks are against
`appcasemap`/`initmap` — TypeScriptActionInterface's own registry of
TS-registered apps — and it then calls `load_module(app."module")`, deliberately
loading `SalesforceRestDataProvider` so that the Qore module registers the
`Salesforce` app and the TS side attaches its actions to it. There is no
`registerApp()` collision by construction.

Action names do not collide either, verified on both sides:

| Side | Action names |
|---|---|
| TS triggers | `new_record_trigger`, `updated_record_trigger`, `new_contact_trigger`, `new_lead_trigger` |
| Qore (existing) | `pubsub-event` |
| Qore (proposed, §S.2) | `new-record`, `updated-record`, `new-contact`, `new-lead` |

So **both can be live at once**. Salesforce can be migrated incrementally,
verified against a real org with the TS triggers still running as a control, and
only then have its TS side deleted. Sequence it first for exactly this reason.

### 0.4 Registration is a four-place operation

Three places in `qore`, plus the `module-v8` removal. Omitting any one fails
silently and differently each time:

| Place | Entry | Symptom if missing |
|---|---|---|
| `qlib/ConnectionProvider/ConnectionSchemeCache.qc` → `SchemeMap` | `"sharepoint": "SharePointRestClient"` | every connection becomes `(InvalidConnection)`, `type: "invalid"`, no `app`; silently filtered out of the action picker |
| `qlib/DataProvider/DataProvider.qc` → `FactoryMap` | `"sharepoint": "SharePointDataProvider"` | module loads fine, app never appears in the apps list |
| `CMakeLists.txt` | `qore_user_module("qlib/SharePointDataProvider" "sharepoint-logo.svg")` | not installed, no docs target |
| `module-v8` removal | see §0.5 | `APP-ERROR: Application ... is already registered` |

Also add each module to `Makefile.am` (per the repo-wide rule that new modules
go in both `CMakeLists.txt` and `Makefile.am` so docs build and the module
installs). The `qore_user_module()` second argument is **extra resource files
for the doc build**, not dependencies — dependencies come from `%requires`.

### 0.5 What removing an app from module-v8 touches

Derived from `4e90a44e`:

1. Delete `ts/src/apps/<app>/` — actions, triggers, helpers, constants, index.
2. Remove the app's entry from `NEW_APP_DIRS` in `ts/src/ActionsCatalogue/index.ts`
   (the lazy-load list) — or, for an *existing* app, from the `EXISTING_APPS`
   object and its eager `import` at the top of the same file.
3. Delete `ts/src/tests/<app>.test.ts` (present for sharepoint, supabase and
   firebase; there is no salesforce test).
4. **Leave the i18n entries in place.** They become unused translations;
   removing them churns the generated locale types for no functional gain.
5. Check for cross-app helper imports before deleting. `4e90a44e` had to retain
   four `apps/google-drive/helpers/` files because google-docs, google-forms and
   google-sheets import them. Run the equivalent check for each app here
   (§0.6).
6. Verify with `npx tsc --noEmit -p tsconfig.build.json` — clean.

### 0.6 Cross-app dependency check (run before deleting anything)

```bash
cd ~/src/qore/git/module-v8/ts/src
grep -rl "apps/<app>\|\.\./<app>" --include='*.ts' . | grep -v "^./apps/<app>/"
```

Anything this prints must be resolved — either migrated too, or the shared
helper retained as `4e90a44e` did for Drive. Do not skip this: the first pass of
`4e90a44e` deleted `file-search.helpers.ts` and only the typecheck caught it.

While here, resolve the **existing** debt this analysis found: `apps/google-drive`
survives as helper-only leftovers imported by google-docs, google-sheets,
google-forms and google-meet — a dangling dependency on an app that no longer
exists. Either promote those helpers to `ts/src/global/helpers/google-drive/`
or fold them into each consumer. This is not blocking, but it is in the same
blast radius and should not be left for a third pass.

### 0.7 Per-phase definition of done

No phase is complete until all of:

- `cmake --build build --target <Module>-qmod` succeeds for every touched qlib
  module. **A stale `.qmod` silently shadows edited `.qm`/`.qc` sources — the
  edited code simply never runs, with no error.**
- `./run_tests.sh -d qlib/<Module>` passes with no warnings and no errors, under
  `qore --enable-debug`.
- The relevant boxes in `design/data-provider-checklist.md` are ticked —
  including §5 (options and output types), §6 (`AllowedValueInfo` with
  `display_name`, never bare values), §7/§7b (`ref_data` and cascading
  dropdowns), §12 (option preselection) and §16 (markdown in descriptions).
- `cmake --build build --target docs-<Module>` builds clean. Doc tables use the
  Qore pipe format (`|!Header|!Header` / `|cell|cell`), never Markdown —
  `QORE_DOX_TABLE_STRICT` is on and fails the build otherwise.
- `npx tsc --noEmit -p tsconfig.build.json` clean in `module-v8`.
- Valgrind only where C++ changed. Phases S, SB and FB are pure Qore; phase P0
  and SP are pure Qore. **No valgrind run is expected anywhere in this plan** —
  if a phase acquires C++ changes, that phase acquires a valgrind gate too.

### 0.8 Live testing is mandatory

`file-app-improvements-implementation-plan.md` §"What live testing found that
offline testing had not" is the precedent: every defect in that table passed a
full offline suite first. Each phase here needs a live tenant —  a Salesforce
developer org with CDC enabled, a SharePoint site with a document library, a
Supabase project, a Firebase project. Budget for it up front; a green offline
suite is not evidence.

### 0.9 All I/O runs on the async I/O controller — `RestClientIo`, never `RestClient`

**This is not a preference; it is the architecture.** Every new client and data
provider in this plan does its I/O on the async socket I/O controller. In
practice that means:

- the client module supplies **both** classes over a shared base — a sync
  `<X>RestClient` for scripting compatibility and an async `<X>RestClientIo` —
  exactly as `OneDriveRestClient.qm` does with `OneDriveRestClientBase`,
  `OneDriveRestClient inherits RestClient::RestClient, OneDriveRestClientBase`
  and `OneDriveRestClientIo inherits RestClientIo::RestClientIo,
  OneDriveRestClientBase`
- the **data provider `%requires RestClientIo` and holds the async client**:
  `*RestClientIo::RestClientIo rest`, constructed as
  `new <X>RestClient::<X>RestClientIo(opts)` — see
  `OneDriveDataProviderBase.qc:86,114`
- the connection's `ConstructorOptions` accept a `RestClientIo` object, not a
  `RestClient` one
- no raw socket operations anywhere; liveness comes from the poll operation's
  own state, never from probing the socket

`OneDriveDataProvider`, `DropboxDataProvider`, `GoogleDriveDataProvider` and
`AwsS3DataProvider` (through `AwsRestClient`, which `%requires(reexport)
RestClientIo`) are all already on this footing. A provider that blocks a thread
per request does not scale to the connection counts Qorus runs, and it cannot
participate in the multiplexed H2/H3 connection pooling the async controller
provides.

**Existing debt this plan must not inherit.**
`SalesforceRestClient.qm` already defines `SalesforceRestClientIo`
(`SalesforceRestClient.qm:1723`), but `SalesforceRestDataProvider` still holds
the **synchronous** client — `SalesforceRestDataProviderBase.qc:120` declares
`SalesforceRestClient::SalesforceRestClient rest`. So every Salesforce REST
action blocks a thread today, and the Pub/Sub module has to reach past the
provider to the *connection* to get an async client for its call metadata (see
`SalesforcePubSubEventDataProvider::getPubSubClient()`, which is why a provider
built from a REST client rather than a connection cannot subscribe at all).

Migrating `SalesforceRestDataProviderBase` to `RestClientIo` is therefore a
prerequisite for treating Salesforce as finished, and it removes the awkward
two-constructor split in the Pub/Sub provider. It is listed as §S.6.

---

## Phase S — Salesforce

Two repos: `module-grpc` (all the work) and `module-v8` (deletion only).
Nothing in phases P0/SP/SB/FB depends on this, and it is the only phase without
a flag day (§0.3) — start here.

### S.1 `change_type` filter — the blocker

`SalesforcePubSubEventDataProvider::eventReceived()` is currently:

```qore
private eventReceived(hash<auto> event) {
    notifyObservers(EVENT_SALESFORCE_PUBSUB_EVENT, event);
}
```

Every change on the channel is raised. Without a filter the four TS triggers
collapse into one undifferentiated "something changed", which is a **UX
regression** and fails §0.1.

Add a `change_type` option to
`SalesforcePubSubEventDataProvider::LocalOptions`, alongside `replay_preset`:

- type `*softlist<string>` (multi-select; empty/absent = all change types)
- `allowed_values` as `AllowedValueInfo` hashes with `display_name` and `desc`
  per `design/data-provider-checklist.md` §6 — `CREATE`, `UPDATE`, `DELETE`,
  `UNDELETE`, plus `GAP_CREATE`/`GAP_UPDATE`/`GAP_DELETE`/`GAP_UNDELETE` and
  `GAP_OVERFLOW`, which Salesforce emits when it cannot deliver the full change
  and which must not be silently swallowed
- no `default_value` — defaulting to `CREATE` would silently drop events for
  anyone subscribing to a raw channel

Filter in `eventReceived()` on `event.payload.ChangeEventHeader.changeType`
(the envelope is `channel` / `event` / `payload`, per
`SalesforcePubSubEventDataType::Fields`).

Three correctness requirements:

1. **Platform events have no `ChangeEventHeader`.** When `change_type` is set
   and the channel is not a CDC channel, reject at option-check time with a
   clear error rather than filtering everything out at runtime. `is_pubsub_channel()`
   and the `/ChangeEvent$/` test in `SalesforcePubSubDefs.qc` already
   distinguish the two.
2. **Gap events must survive.** A `GAP_*` event means Salesforce could not
   deliver the change body. If the user asked for `CREATE` and a `GAP_CREATE`
   arrives, it must be delivered — dropping it converts a visible "we missed
   something" into silent data loss.
3. **Filtering happens after replay-position tracking, never before.** The
   replay ID must advance on every event received, filtered or not, or a
   restart replays everything the filter dropped.

### S.2 Named event actions

Register four actions against the `Salesforce` app, mirroring the TS trigger
names so the catalogue does not regress, each a thin preset over the same
provider:

| Action | Channel | `change_type` |
|---|---|---|
| `new-record` | `{object}ChangeEvent` (object picker) | `CREATE` |
| `updated-record` | `{object}ChangeEvent` (object picker) | `UPDATE` |
| `new-contact` | `ContactChangeEvent` | `CREATE` |
| `new-lead` | `LeadChangeEvent` | `CREATE` |

The existing generic `pubsub-event` action stays — it is the superset and the
only way to reach platform events and custom channels.

The `{object}` picker must be a real dropdown, not free text: the TS trigger
used `getSalesforceObjectAllowedValues`, and
`SalesforcePubSubEventsDataProvider::getChildProviderNamesImpl()` already
enumerates every subscribable channel in the org via `getChannelObjects()` +
`is_pubsub_channel()`. Wire that as `ref_data`/`get_allowed_values` per
checklist §6/§7 so the Qore picker is *live* where the TS one was a static
fetch — one of the "better UX" deliverables.

**Must beat the TS app at:** real-time delivery instead of polling; replay-ID
resumption across reconnects (72h retention) instead of high-water-mark
reconciliation; `DELETE`/`UNDELETE` visibility, which polling cannot see at all;
platform events and custom channels, which have no polling equivalent; live
channel discovery; and no hardcoded API version — `SalesforceRestClient.qm`
negotiates via `GET /services/data` while `apps/salesforce/constants.ts` pins
`v62.0`.

### S.3 CDC enablement — detection is done; **enablement is automatable**

Change Data Capture must be switched on per entity before a subscription
delivers anything, and a subscription to a non-enabled entity succeeds and then
delivers nothing — the worst possible failure mode. Detection is implemented (a
`TopicInfo.can_subscribe` pre-flight check that throws naming the entity and the
Setup path).

**The onboarding concern is resolved: enabling CDC does not require the customer
to do anything in Setup.** Verified against a live org: CDC entity selection is
exposed as `PlatformEventChannelMember` in the **Tooling API**, and is both
readable and writable.

Read the current selection:

```
GET /services/data/v67.0/tooling/query/?q=SELECT Id, DeveloperName, EventChannel, SelectedEntity FROM PlatformEventChannelMember
```

Enable an entity — verified working, `LeadChangeEvent` was added to the live dev
org this way and appeared immediately in the query above:

```
POST /services/data/v67.0/tooling/sobjects/PlatformEventChannelMember
{
  "FullName": "CM_ChangeEvents_Lead",
  "Metadata": {"eventChannel": "ChangeEvents", "selectedEntity": "LeadChangeEvent"}
}
```

`DeveloperName` follows the `CM_ChangeEvents_<Object>` convention Setup itself
uses. The standard channel is `ChangeEvents`; a query of `PlatformEventChannel`
returns the *custom* channels only, and was empty on the test org.

**Design consequence.** Offer enablement rather than only diagnosing the lack of
it, but **do not enable silently** — subscribing to events should not quietly
mutate org configuration. Provide one of:

- an `enable_cdc` option (default `False`) on the event actions, which on a
  failed `can_subscribe` check creates the `PlatformEventChannelMember` and
  retries; or
- a separate `enable-change-data-capture` API action in the catalogue, so
  enabling is an explicit, auditable step a flow can perform once

The error raised when the check fails should name the option or action, so the
remedy is one click rather than a Setup walkthrough.

**Caveats to handle:**

- the write needs a user with metadata-modify rights; a user without them must
  get a clear permissions error rather than a generic failure
- Salesforce caps how many entities may be selected on the standard channel;
  the cap depends on org edition and add-ons. Hitting it must be reported as
  such, not as a generic create failure. Confirm the current limit against the
  org's own error rather than assuming a number
- enabling CDC starts a 72-hour retention stream for that entity; it is not free
  of side effects, which is the other reason not to do it implicitly

### S.4 Packaging — **decided: `module-grpc` is always present**

`SalesforcePubSubDataProvider` lives in `module-grpc` and `%requires` `grpc`,
`protobuf`, `AvroUtil`, `AsyncSocketIo` and `GrpcUtil`, while
`SalesforceRestDataProvider` is in core `qore`. The question was whether an
installation could have the Salesforce app without the module supplying its
events.

**It cannot: `module-grpc`, `qore` and all other modules ship together.** So the
Pub/Sub event actions are as available as the REST actions are, and §S.5 is
unblocked — the TS triggers can simply be deleted.

Consequences for the rest of this phase:

- no degraded mode, no placeholder action, and no "events require `module-grpc`"
  caveat in any `desc`; the events are simply part of the Salesforce app
- the `registerChild()` graft in `SalesforcePubSubDataProvider`'s init block is
  the whole integration story — nothing needs to detect whether it ran
- a missing `module-grpc` is a broken installation rather than a supported
  configuration, so it needs no runtime handling beyond the error the module
  loader already raises

The two alternatives considered and now closed: registering a placeholder action
in core that throws naming the module (unnecessary), and moving Pub/Sub into core
`qore` (rejected regardless — it would drag gRPC, protobuf and Avro into the core
dependency set for one app).

### S.5 module-v8 removal

Per §0.5, but the *existing-app* variant:

- delete `ts/src/apps/salesforce/` (867 lines: 4 triggers, `helpers/`,
  `constants.ts`, `index.ts`)
- remove `salesforce` from the `EXISTING_APPS` object and its `import` at the
  top of `ts/src/ActionsCatalogue/index.ts`
- no test file to delete
- leave i18n in place
- run the §0.6 cross-app check first

Because of §0.3 this deletion can happen **after** the Qore actions are verified
live, not simultaneously. Take that option.

### S.6 Move `SalesforceRestDataProvider` onto `RestClientIo`

Per §0.9. `SalesforceRestDataProviderBase.qc:120` holds a synchronous
`SalesforceRestClient`, so every Salesforce REST action blocks a thread, while
`SalesforceRestClientIo` already exists and goes unused by the provider.

- change the base's member to `*RestClientIo::RestClientIo rest` and construct
  `SalesforceRestClient::SalesforceRestClientIo`, following
  `OneDriveDataProviderBase.qc:86,114`
- update `ConstructorOptions` — the `salesforcerestclient` option currently
  takes `new Type("SalesforceRestClient")` and must accept the `Io` class
- audit every `doRestCommand()`/request site in the provider tree for
  synchronous assumptions
- once done, `SalesforcePubSubEventDataProvider` no longer needs its
  REST-client-versus-connection split: the async client is available from the
  provider itself, so the "created from a REST client rather than a connection"
  failure path in `getPubSubClient()` can go away

This is independent of §S.1-S.3 and can land before or after them, but it must
land before Salesforce is called done.

### S.7 Tests

- `module-grpc` unit tests for the `change_type` filter: each type in isolation,
  multi-select, absent-option (all delivered), gap events delivered when their
  base type is selected, and the platform-event rejection path.
- Replay-advance test: filtered-out events still advance the replay position —
  assert by reconnecting with `CUSTOM` and checking nothing re-delivers.
- Live test against a developer org with CDC enabled on Account, Contact and
  Lead: create/update/delete a record and assert the right actions fire with the
  right payloads. Assert **field values**, not just shapes (checklist §14).
- Negative: subscribe to a CDC-disabled entity, assert the S.3 error.

---

## Phase P0 — OneDrive drive-path refactor (prerequisite for SharePoint)

Small, self-contained, and shippable on its own.

OneDrive's leaf providers hardcode the personal-drive prefix. From
`OneDriveGetItemDataProvider.qc:82-88`:

```qore
path = "me/drive/items/" + req.id;
...
path = "me/drive/root:" + item_path;
```

SharePoint document libraries are the *same* Graph drive-item API rooted at
`sites/{site-id}/drives/{drive-id}` or `drives/{drive-id}`. Reusing OneDrive's
providers — which is the entire argument for doing SharePoint next — requires
that prefix to be a hook.

**Change:** add a `getDriveBasePath()` method to `OneDriveDataProviderBase`
returning `"me/drive"`, and replace every hardcoded `me/drive` in the leaf
providers with a call to it. Audit all of them — `GetItem`, `ListItems`,
`CreateFolder`, `UploadFile`, `CreateUploadSession`, `DownloadFile`,
`DeleteItem`, `UpdateItem`, `CopyItem`, `CreateSharingLink`, `GetDriveInfo`,
`WatchChanges`.

This is a pure refactor with no behavioural change; OneDrive's existing test
suite is the regression gate. Land and verify it before phase SP starts so a
SharePoint bug can never be confused with a refactor bug.

---

## Phase SP — SharePoint

> **Test tenant: available and verified.** The `sharepoint` connection now has a
> SharePoint Online licence and the whole surface Phase SP needs was exercised
> live against it:
>
> | Check | Result |
> |---|---|
> | `GET /v1.0/sites/root` | OK — `qoretechnologies0.sharepoint.com`, "Communication site" |
> | `GET /v1.0/me/drive` | OK — OneDrive reachable on the same tenant |
> | `POST /v1.0/sites/{site-id}/lists` with `list.template = documentLibrary` | **OK — a document library can be created** |
> | `GET /v1.0/sites/{site-id}/lists/{list-id}/drive` | OK — the backing drive resolves |
> | `PUT /v1.0/drives/{drive-id}/root:/{name}:/content` | OK |
> | `@microsoft.graph.downloadUrl` on the item | present — **confirms `DPFR_URL` is the correct `read_mode` (§SP.3)** |
> | download + byte compare | byte-exact, with the caveat below |
>
> The probe left a library named `Qore Migration Test Library` and two small
> `probe*.bin` files in the root site; delete with
> `DELETE /v1.0/sites/{site-id}/lists/{list-id}` if it is not wanted as a
> fixture.
>
> `GET /v1.0/sites?search=*` returns `generalException` — a Graph quirk with the
> bare wildcard, not a licensing or permission problem. §SP.2's `list-sites`
> should use a real search term or `/sites?search=` with an empty value and be
> tested against both.

The strongest functional-gap case in the repo: `apps/sharepoint` has 7 actions
and **not one moves a file** (`create-folder`, `create-list`, `create-list-item`,
`delete-list-item`, `search-list-item`, `update-list-item`, plus a `new-row`
trigger). The only Graph drive call in the whole app is `create-folder`
patching `/drives/${driveId}/root:/${folderPath}`. SharePoint's primary use is
its document libraries, and the app cannot upload, download, copy, move or
version a document.

### SP.1 Client layer — **decided: an `api_profile` on `OneDriveRestClientBase`**

The TS app already targets `https://graph.microsoft.com` with OAuth2
authorization-code against `login.microsoftonline.com/common`, scopes
`openid email profile offline_access Sites.Manage.All Files.ReadWrite
Sites.Read.All User.Read`. `OneDriveRestClient.qm` targets the same host with
the same OAuth2 endpoints (`OneDriveRestClientBase::DefaultUrl`,
`OAuth2BaseUrl`), so the Microsoft OAuth2 plumbing already exists and must not
be duplicated.

**SharePoint is served by an `api_profile` mechanism added to
`OneDriveRestClientBase`**, mirroring `GoogleRestClientBase::ApiProfiles`
(`GoogleRestClient.qm:149-185`). Concretely:

- add `ApiProfiles` and the profile-resolution logic to
  `OneDriveRestClientBase`, with profiles `onedrive` (the current scopes and
  ping, and the default, so existing connections are unaffected) and
  `sharepoint` (the scope set above, ping `/v1.0/sites/root`)
- resolution follows the Google precedent: profile values fill in only where the
  caller has not set the option itself, i.e.
  `map opts{$1.key} = $1.value, conf.pairIterator(), !opts.hasKey($1.key)`
- add a `SharePointRestConnection` pinning the `sharepoint` profile, scheme
  `sharepoint`, `DefaultUrl = "sharepoint://graph.microsoft.com"`
- an unknown profile name throws naming the known profiles, as
  `GoogleRestClientBase` does

Because `ApiProfiles` lives on the **base**, both `OneDriveRestClient` and
`OneDriveRestClientIo` acquire it at once — which §0.9 requires, since the data
provider uses the `Io` class. `SharePointDataProvider` `%requires RestClientIo`
and holds `*RestClientIo::RestClientIo`; it must never hold the synchronous
class.

**Alternative closed:** a standalone `SharePointRestClient.qm`. It would
duplicate the Graph auth layer, and every fix to Microsoft OAuth2 handling would
then have to be made twice.

**Regression risk to cover in SP.8:** adding a profile mechanism changes how
`OneDriveRestClientBase` assembles its options. Existing `onedrive` connections
must come out byte-identical — assert the resolved option hash for a default
connection before and after, not merely that OneDrive's suite still passes.

### SP.2 Site and drive navigation

New providers with no OneDrive equivalent:

- `list-sites` / `get-site` — `/v1.0/sites?search=` and `/v1.0/sites/{site-id}`
- `list-drives` — `/v1.0/sites/{site-id}/drives` (the document libraries)

`site_id` and `drive_id` become the cascading dropdowns for everything else,
implemented as `ref_data` per checklist §7b — the TS app already has
`get-site-id-allowed-values.ts` and `get-drive-id-allowed-values.ts` to port.

### SP.3 Document library file API — the point of the phase

`SharePointDataProvider` reuses the phase-P0 drive-item providers with
`getDriveBasePath()` overridden to `sites/{site-id}/drives/{drive-id}`. That
yields, for free: `get-item`, `items` (DPAT_FIND), `create-folder`,
`upload-file`, `create-upload-session`, `download-file`, `delete-item`,
`update-item`, `copy-item`, `create-sharing-link`, `get-drive-info`.

Then register the file API, so SharePoint joins `AwsS3`, `Dropbox`,
`GoogleDrive` and `OneDrive` as a `conn://` file-location target with the shared
file vocabulary. Follow the Dropbox registration
(`DropboxDataProvider.qm:72-88`) for shape:

Because SharePoint reuses OneDrive's drive-item providers, its registration
mirrors OneDrive's (`OneDriveDataProvider.qm:71-83`) with the drive added as a
container:

```qore
DataProviderActionCatalog::registerAppFileApi(SharePointDataProvider::AppName,
    <DataProviderFileApiInfo>{
        "read_action": "download-file",
        "read_path_option": "id",
        "read_container_option": "drive_id",
        "read_mode": DPFR_URL,
        "read_data_field": "download_url",
        "read_mime_type_field": "content_type",
        "write_action": "upload-file",
        "write_path_option": "parent_id",
        "write_container_option": "drive_id",
        "write_data_option": "file",
        "write_data_format": DPFW_FILE,
        "write_path_is_parent": True,
        "path_is_id": True,
    });
```

Note `read_mode` is `DPFR_URL`, **not** `DPFR_DATA`: Graph's download does not
return bytes, it returns a pre-authenticated `@microsoft.graph.downloadUrl` that
the framework then fetches. **Verified live** — the field is present on an
uploaded item.

**Binary uploads must not go through the JSON codec.** This was demonstrated on
the live tenant and is easy to get wrong: the app connection is configured
`data = "json"`, so a binary body handed to `put()` is JSON-serialized — a
52-byte payload arrived as 74 bytes, which is base64 (72 chars) plus the two
quote characters, and Graph stored the corruption without complaint. The upload
succeeds, the item appears, and only a byte comparison reveals it.

The fix is the `"bin"` serialization mode: `setSerialization("bin")` around the
content request (restore the previous value with `on_exit`), or a client
dedicated to content operations. With `data = "bin"` the same payload round-trips
byte-exactly. Whichever the implementation picks, the upload provider must not
inherit the connection's `json` serialization for the content call. Dropbox's `DPFR_DATA` + `"base64"` registration is
the wrong template to copy here. Likewise `write_path_is_parent` and
`path_is_id` are both `True` because Graph addresses items by opaque ID plus a
parent, not by a path that may be split or normalized.

**Must beat the TS app at:** uploading and downloading documents at all;
streaming and resumable upload via upload sessions instead of base64-in-JSON
through the proxy; `conn://` file-location integration; consistent file-API
naming; and copy/move/sharing-link operations the TS app never had.

### SP.4 Lists surface

Port the six existing list actions so nothing regresses:
`/v1.0/sites/{site-id}/lists`, `.../lists/{list-id}/items`. Port
`get-list-id-allowed-values.ts`, `get-item-id-allowed-values.ts` and
`get-list-dependent-options.ts` as `ref_data`/cascading dropdowns. Add
`get-list` and `delete-list`, which the TS app omitted (it can create a list but
never delete one).

### SP.5 Record provider

`apps/sharepoint` is `TQoreRecordBasedApp` — it supplies `get_table_list`,
`get_record_type`, `search_records`, `create_records`, `update_records`,
`delete_records`, `search_options` and `expressions` over list items. Do not
drop this: implement the `AbstractDataProvider` record API over
`/lists/{list-id}/items` with a `SharePointRecordIterator` that fetches each
page as it is reached (the pattern in `file-app-improvements-implementation-plan.md`
tranche 4). Map the TS `expressions` to DPQL-compatible search options.

### SP.6 Events

The TS `new-row` trigger polls. Graph supports **delta queries** on both drive
items (`/delta`) and list items, and `OneDriveWatchChangesDataProvider.qc` is
the template. Provide `new-file`, `file-modified`, `new-folder` on the document
library and `new-row` (name preserved) on lists. Delta tokens give the same
"resume where we left off" property as Salesforce replay IDs.

Graph webhooks (`/subscriptions`) would be better still but need a
publicly-reachable notification URL; treat as out of scope here and note it.

### SP.7 module-v8 removal

Flag day per §0.3 — the TS app registers the `SharePoint` app, so the native
module cannot be tested in a live catalogue until it is gone. Per §0.5:
delete `ts/src/apps/sharepoint/`, remove `'sharepoint'` from `NEW_APP_DIRS`,
delete `ts/src/tests/sharepoint.test.ts`, leave i18n, run the §0.6 check.

### SP.8 Tests

`examples/test/qlib/SharePointDataProvider/`, structured like the OneDrive
suite. Offline suite plus a live site: upload → download → verify bytes
round-trip, resumable upload above the single-shot threshold, folder create/
copy/delete, list CRUD, record iteration across a page boundary, delta events
on both drives and lists. Assert field values, not shapes.

Two checks specific to the decisions above:

- **`api_profile` regression (§SP.1).** Assert the resolved option hash for a
  default `onedrive` connection is unchanged by the profile mechanism, and that
  an explicitly-set option still wins over the profile's value. Add this to the
  **OneDrive** suite, not the SharePoint one — it is OneDrive that is at risk.
  Also assert an unknown profile name throws naming the known profiles.
- **Byte fidelity (§SP.3).** Two distinct ways to corrupt content silently, both
  of which return success:
  - the file API is `DPFR_URL`, so content arrives via a pre-authenticated
    `@microsoft.graph.downloadUrl` rather than inline; a wrong
    `read_mode`/`read_data_encoding` corrupts every byte read through `conn://`
  - a binary upload issued on the connection's default `data = "json"`
    serialization is base64'd into a JSON string and stored that way — observed
    live, 52 bytes in, 74 bytes stored
  Assert a **byte-exact round-trip of binary content containing NUL and high
  bytes**, in both directions. A successful call proves nothing here, and neither
  does a text payload.

---

## Phase SB — Supabase — **deferred**, see §Deferred below

`apps/supabase` exposes `create-bucket`, `get-bucket`, `list-buckets` — and **no
object operations at all**. You can create a bucket and never put anything in it
or get anything out.

### SB.0 Port strategy — note before starting

The TS actions are written against the `@supabase/supabase-js` SDK
(`client.storage.listBuckets()`), not raw REST. There is nothing to transliterate:
the Qore implementation targets the underlying REST APIs directly —
PostgREST at `/rest/v1/`, Storage at `/storage/v1/`, Auth at `/auth/v1/`. All
are plain, well-documented REST on a single host, so this is tractable, but
budget it as new implementation rather than a port.

### SB.1 Client layer

`SupabaseRestClient.qm`, scheme `supabase`. Single host
`https://{project_id}.supabase.co`. Auth is an API key header, not OAuth2 — the
TS app sets `token_api_key_header: 'apikey'` with required options
`projectId,token` and `url_template_options: ['projectId']`. Mirror that:
connection options `project_id` (URL template) and `token` (the service-role
key; mark it sensitive), with the TS `desc` strings carried over — they tell the
user exactly where in the dashboard to find each value, which is good UX worth
keeping.

Replace the TS `set_options_post_auth` hack — which fetches `/rest/v1/` and
picks `paths[0]` as a ping path — with a fixed ping on `/rest/v1/`. Depending on
an arbitrary first table is fragile and breaks on an empty schema.

Per §0.9, `SupabaseRestClient.qm` supplies `SupabaseRestClient` and
`SupabaseRestClientIo` over a shared `SupabaseRestClientBase`, and the data
provider holds the `Io` class.

### SB.2 Storage — the functional gap

Bucket management, keeping the existing three and adding what was missing:
`create-bucket`, `get-bucket`, `list-buckets`, `update-bucket`,
`delete-bucket`, `empty-bucket`.

Objects, none of which exist today: `upload-object` (`POST
/storage/v1/object/{bucket}/{path}`), `download-object`, `list-objects`,
`delete-object`, `move-object`, `copy-object`, `get-public-url`,
`create-signed-url`, `create-signed-upload-url`.

Then `registerAppFileApi()`. Unlike SharePoint, Supabase Storage returns bytes
directly from `GET /storage/v1/object/{bucket}/{path}`, so this is a
`DPFR_DATA` registration: `read_action: "download-object"`,
`read_path_option: "path"`, `read_container_option: "bucket"`,
`read_mode: DPFR_DATA`, `read_data_field: "content"`,
`read_data_encoding: "raw"`, `read_mime_type_field: "content_type"`,
`write_action: "upload-object"`, `write_path_option: "path"`,
`write_container_option: "bucket"`, `write_data_option: "file"`,
`write_data_format: DPFW_FILE`, and `options_add: {"upsert": True}` — the
Storage API rejects an existing key by default, whereas every other
file-location scheme replaces.

`read_data_field` is required by the hashdecl even in `DPFR_DATA` mode, so the
`download-object` action must return the bytes in a named field (`content`)
rather than as a bare value.

**Must beat the TS app at:** having object operations at all; streaming instead
of base64-through-proxy; signed URLs; `conn://` integration.

### SB.3 Tables (PostgREST)

Keep the record surface — the TS app is `TQoreRecordBasedApp` with
`create/upsert/update/delete/search_records`, `get_table_list`,
`get_record_type` and `expressions`. Implement the `AbstractDataProvider` record
API over `/rest/v1/{table}`, with a record iterator using PostgREST `Range`
headers for paging. Port `get-table-allowed-values.ts`,
`get-table-fields.ts` and `get-filter-operator-allowed-values.ts` as
`ref_data`/allowed values. Map `expressions` onto PostgREST operators
(`eq`, `neq`, `gt`, `like`, `in`, …) via DPQL.

Derive the table list and record types from the OpenAPI document PostgREST
serves at `/rest/v1/` — that is a live schema, so types stay correct as the
project's schema changes. This is strictly better than the TS app's static
handling and is the main "more functionality" claim for this surface.

### SB.4 Events

The TS app has a polling `new-row` trigger. Supabase Realtime is a WebSocket
protocol (Phoenix channels) carrying Postgres change events. A native
`new-row` / `row-updated` / `row-deleted` event provider over Realtime would be
a genuine upgrade and Qore has the WebSocket client for it — but it is a real
protocol implementation, not a REST call.

**Gate:** timebox a Realtime spike. If it does not land inside the box,
implement the polling `new-row` trigger at parity and file Realtime as
follow-up. Do not let it block the storage work, which is the actual point of
the phase.

### SB.5 module-v8 removal and tests

Flag day per §0.3/§0.5: delete `ts/src/apps/supabase/`, remove `'supabase'` from
`NEW_APP_DIRS`, delete `ts/src/tests/supabase.test.ts`.

`examples/test/qlib/SupabaseDataProvider/`, plus live tests against a real
project: object round-trip, signed URL fetch, bucket lifecycle, record CRUD,
paging across a `Range` boundary, and type derivation against a schema change.

---

## Phase FB — Firebase — **deferred** in favour of Google Cloud Storage, see §Deferred below

`apps/firebase` has a real object surface — `upload-file`, `delete-file`,
`list-files-in-bucket`, `get-file-metadata`, `list-buckets` — plus Auth
(`get-user`, `list-users`) and FCM (`send-push-notification`).

### FB.0 Multi-host gate — **do this first, it may change the design**

Firebase is not one API. The TS app reaches **eight hosts**:

```
www.googleapis.com          cloudresourcemanager.googleapis.com
storage.googleapis.com      identitytoolkit.googleapis.com
fcm.googleapis.com          iid.googleapis.com
oauth2.googleapis.com       accounts.google.com
```

with a base URL of `cloudresourcemanager.googleapis.com` and per-action
overrides. This is the same shape as the Dropbox two-host problem that earned a
gated transport spike in the previous plan (Phase B), and it is worse.

**Gate:** a timeboxed spike, not committed, establishing how a single Qore
connection addresses multiple hosts under one OAuth2 token. Likely answers: a
per-provider host override on the request path, or several `RestClientIo`
instances sharing one token store. Resolve this before designing the provider
tree — it determines whether Firebase is one module or several.

### FB.1 Client layer

Firebase authenticates with Google OAuth2 — scopes
`.../auth/firebase`, `.../auth/identitytoolkit`, `.../auth/firebase.messaging`,
`.../auth/devstorage.read_write`, `.../auth/devstorage.full_control`,
`.../auth/cloud-platform`, with `access_type: offline` and `prompt: consent`.

`GoogleRestClient.qm` already has the `api_profile` mechanism
(`ApiProfiles` at :149, with `none`/`calendar`/`gmail`/`drive`/`drive-file`).
Add a `firebase` profile and a `FirebaseRestConnection` pinning it, scheme
`firebase` — the same mechanical extension that `drive` was for phase C of the
previous plan. This reuses Google's OAuth2 refresh handling wholesale.

Narrow the scopes while porting: `devstorage.full_control` *and*
`read_write` *and* `cloud-platform` is broader than any action needs.
Requesting less is a UX and security improvement and is exactly the kind of
thing the previous migration found (Dropbox had been requesting
`account_info.read` for nothing).

Per §0.9 the provider holds `RestClientIo`. This interacts directly with FB.0:
whatever multi-host mechanism is chosen must work on the **async** client, so
resolve FB.0 against `RestClientIo`, not against `RestClient`.

### FB.2 Cloud Storage

`list-buckets`, `list-objects`, `upload-object` (with resumable upload —
`uploadType=resumable`), `download-object`, `get-object-metadata`,
`update-object-metadata`, `delete-object`, `copy-object`, `compose`,
`create-signed-url`.

Then `registerAppFileApi()`, same `DPFR_DATA` shape as Supabase (GCS returns
bytes from `?alt=media`): `read_action: "download-object"`,
`read_path_option: "object"`, `read_container_option: "bucket"`,
`read_data_field: "content"`, `read_data_encoding: "raw"`,
`write_action: "upload-object"`, `write_data_format: DPFW_FILE`.

Note that GCS object names are **not** a path hierarchy — `/` is an ordinary
character in a flat key space, so the key must pass through verbatim with no
splitting or normalization. This is the same rule `AwsRestClient` already
follows for S3 (S3 paths are not normalized), and getting it wrong silently
addresses the wrong object. `path_is_id = True` is the flag that tells consumers
not to interpret the value; confirm during implementation that it does not also
suppress behaviour the file API needs, since its documented intent is opaque
immutable identifiers rather than flat name spaces.

**Must beat the TS app at:** streaming and resumable upload instead of
base64-in-a-JSON-field through the proxy; signed URLs; object copy/compose;
`conn://` integration.

### FB.3 Auth and messaging

Port `get-user`, `list-users` (Identity Toolkit) and `send-push-notification`
(FCM v1 — note the TS app also touches the legacy `iid.googleapis.com`, which
should not be carried forward). Add `create-user`, `update-user`, `delete-user`
so the Auth surface is CRUD-complete rather than read-only.

### FB.4 module-v8 removal and tests

Flag day per §0.3/§0.5: delete `ts/src/apps/firebase/`, remove `'firebase'` from
`NEW_APP_DIRS`, delete `ts/src/tests/firebase.test.ts`.

`examples/test/qlib/FirebaseDataProvider/` plus live tests: object round-trip,
resumable upload above the threshold, signed URL, user CRUD, and a push
notification to a test device token.

---

## Phase V — stale API version pins

Independent of everything above; can land at any time. All in `module-v8`
except where noted.

| Pin | Location | Action |
|---|---|---|
| `SALESFORCE_API_VERSION = 'v62.0'` | `ts/src/apps/salesforce/constants.ts` | **removed by phase S** — `SalesforceRestClient.qm` negotiates dynamically via `GET /services/data`. No separate work. |
| `api_version: '2024-12-18.acacia'` | `ts/src/apps/stripe/triggers/helpers.ts` | bump to current Stripe API version |
| `LINKED_IN_ORGANIZATIONS_API_VERSION = '202502'` | `ts/src/apps/linkedin-organizations/constants.ts` | bump; LinkedIn versioned APIs expire on a rolling window, so a stale pin eventually hard-fails |
| `NOTION_API_VERSION = '2025-09-03'` | `ts/src/apps/notion/helpers/constants.ts` | bump to current `Notion-Version` |
| `ZOHO_CRM_API_VERSION = 'v8'` | `ts/src/apps/zohocrm/constants.ts` | verify still current |
| `HELPSCOUT_API_VERSION = 'v8'` | `ts/src/apps/helpscout/constants.ts` | verify still current |
| `SHOPIFY_API_VERSION = '2026-10'` | `ts/src/apps/shopify/constants.ts` | verify; Shopify deprecates quarterly |
| `apiVersion: 'v2.1'` | `ts/src/apps/esignature/triggers/*.ts` | verify (DocuSign eSignature v2.1 is long-lived) |

**Not stale, do not touch:** `TWILIO_API_VERSION = '2010-04-01'` and
`SENDGRID_API_VERSION = 'v3'` are permanent upstream version strings.

**Method — do not take the values above on faith.** Each must be checked against
upstream at implementation time; several will have moved again by then. For each:
fetch the vendor's current version from its own versions endpoint or changelog,
bump the constant, run that app's tests, and check the diff of any response
shapes the actions destructure. A version bump that silently changes a payload
shape is worse than a stale pin.

Add a follow-up task: these pins have no expiry mechanism. Consider a CI check
that flags version constants older than N months so this does not recur.

---

## Deferred: why Supabase and Firebase are below the line

Phases SB and FB are written out in full above and remain accurate, but on
review **neither is recommended for this round**. The reasoning, so the decision
can be revisited rather than re-derived:

**The arguments for them are weaker than they first appear.**
- *Dropping the ts-proxy* is real but marginal per app. The proxy process stays
  as long as any of the ~90 remaining TS apps is in use, so migrating one app
  saves a subscription's worth of overhead, not the process.
- *File API / `conn://` participation* is a genuine gap for both, but it only
  pays off if customers actually keep integration payloads there.

**And the arguments against are specific.**
- Neither is a common enterprise integration endpoint next to SharePoint or
  Salesforce. Both are developer-facing platforms aimed at application builders.
- **Supabase's most valuable half is already reachable, and better.** Supabase
  exposes a direct PostgreSQL connection, and Qore has a first-class `pgsql`
  driver, `DbDataProvider`, `DatasourceProvider` and
  `design/datasource-mutation-observer.md`. A direct DB connection gives full
  SQL, transactions and real schema — strictly more than PostgREST. Realtime
  (§SB.4) would buy push-versus-poll on a source that is already better served
  another way.
- **Firebase Storage *is* Google Cloud Storage.** The TS app calls
  `storage.googleapis.com` directly; the Firebase SDK is a wrapper over GCS
  buckets. A `GoogleCloudStorageDataProvider` would cover Firebase Storage *and*
  every other GCP customer, for one module and without the eight-host problem in
  §FB.0 — a far better-targeted investment than a Firebase-shaped module
  carrying Auth and FCM.

**Recommendation.** Do Salesforce and SharePoint. If demand appears, build
**Google Cloud Storage** rather than Firebase — reusing the `GoogleRestClient`
`api_profile` mechanism (§FB.1) and the file-API shape in §FB.2, both of which
transfer unchanged. Leave Supabase until a customer asks, and then do Storage
only (§SB.2), since the database half is already covered by `pgsql`.

## Sequencing

```
[prereq]  Pub/Sub TLS trust     ── external; blocks live verification of S.1-S.3
Phase S  (Salesforce)          ── independent, no flag day, start here
   ├─ S.3 enablement offer     ── new: offer CDC enablement, never silent
   └─ S.6 RestClientIo move    ── independent of S.1-S.3, before "done"
Phase P0 (OneDrive refactor)   ── independent, small; gates SP
   └─ Phase SP (SharePoint)    ── flag day
Phase V  (version pins)        ── independent, any time

deferred:
Phase SB (Supabase)            ── only on customer demand, Storage only
Phase FB (Firebase)            ── superseded by a GCS provider
```

**The prerequisite is not part of this plan but gates part of it.** A
`qdp <conn>/pubsub/<channel> listen` currently fails with
`SOCKET-SSL-ERROR: ... unable to get local issuer certificate` against
`api.pubsub.salesforce.com:7443` — the gRPC channel is not finding a CA bundle.
Until that is fixed the §S.1-S.3 work cannot be demonstrated end to end, and
§S.5 (deleting the TS triggers) should not land on offline evidence alone.

S, P0 and V can run in parallel. SP must follow P0.

Recommended order if run serially: **S → P0 → SP**, with V slotted into any gap.
This front-loads the cheapest win (S), then the highest-value functional gap
(SP), and drops the two phases whose cost is least justified by customer value.

## Out of scope

- The 19 schema-driven apps. They are blocked on generalizing
  `TypeScriptActionInterface::getRestSchema()` into core qlib —
  `openapi_type_overrides` has **zero consumers in the qore repo** today. That
  is its own piece of infrastructure work; see
  `module-v8-migration-candidates.md` §3.
- Canva, Contentful and google-docs (analysis §2.4/§2.5).
- Graph webhooks for SharePoint (SP.6) — needs a public notification URL.
- Any attempt to remove the ts-proxy. Even with all four apps migrated, 90-plus
  TS apps remain.

## Decisions needed before coding starts

1. ~~**§S.4 — does `module-grpc` ship wherever the Salesforce app ships?**~~
   **Decided: yes** — `module-grpc`, `qore` and all other modules ship together.
   §S.5 is unblocked.
2. ~~**§SP.1 — `api_profile` on `OneDriveRestClientBase`, or a standalone
   `SharePointRestClient`?**~~ **Decided: the `api_profile` on the base**, so
   both the sync and `Io` clients acquire it at once.
3. **Live tenants** (§0.8). Status as of the last check:
   - **Salesforce — available.** The `salesforce` connection points at a dev org
     (`d2o000000aoygea2-dev-ed`) negotiating API v67.0, with CDC enabled on
     `Order`, `Account`, `Contact` and now `Lead`. Ready for §S.7 testing.
   - **SharePoint — available.** The `sharepoint` connection is licensed and
     verified end to end: a document library was created, a file uploaded and
     downloaded byte-exactly, and `@microsoft.graph.downloadUrl` confirmed. See
     the note at the head of Phase SP.

Deferred with their phases, and only live if Supabase or Firebase is revived:
§FB.0 (how one connection addresses eight hosts — and note §0.9 requires the
answer to work on `RestClientIo`) and §SB.4 (Realtime versus polling-at-parity).
