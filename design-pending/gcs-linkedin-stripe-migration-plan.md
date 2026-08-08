# Implementation Plan — Google Cloud Storage, LinkedIn and Stripe

**Status:** Design proposal, nothing implemented. Successor to
[`salesforce-sharepoint-supabase-firebase-migration-plan.md`](salesforce-sharepoint-supabase-firebase-migration-plan.md),
whose recommended scope (Salesforce, SharePoint, the version pins) is complete.

**Repos touched:** `qore`, `module-v8`.

**Normative references.** Every phase conforms to these, unchanged:
- `design/qore-module-structure.md`
- `design/data-provider-development-guide.md`
- `design/data-provider-checklist.md`
- `design/data-provider-semantic-string-types.md`
- `design/generic-api-call-action.md`

**Ground rules carried over** from the previous plan and not restated in full:
§0.1 the migration must *beat* the app it replaces, §0.4 registration is a
four-place operation, §0.7 per-phase definition of done, §0.8 live testing is
mandatory, §0.9 all I/O runs on `RestClientIo`, never `RestClient`.

**Precedent worth reading first.** Phase SP of the previous plan (SharePoint)
established the pattern reused twice here: one client module serving two products
that share a host and an OAuth2 flow, differentiated by an `api_profile`, with the
data provider reusing another module's leaf providers.

---

## 0. What is actually being proposed

These three are **not** the same kind of work, and conflating them is the main risk
in scheduling them together:

| Phase | Kind of work | Blocked on |
|---|---|---|
| **G** — Google Cloud Storage | A *new* app; no TypeScript predecessor | Nothing; the logo is in place |
| **L** — LinkedIn | A migration of two TypeScript apps | Nothing; needs a live LinkedIn app for each scope family |
| **T** — Stripe | A port plus one infrastructure decision | Schema-driven action registration (§T.1) |

Phase G and phase L are ordinary work. Phase T is *mostly* ordinary too, but it
carries one genuine infrastructure decision (§T.1) whose answer changes its size by
an order of magnitude, and one security improvement that is worth generalising
(§T.2).

---

## Phase G — Google Cloud Storage

A new app, not a migration. The argument for building it instead of Firebase is in
the previous plan's §Deferred; the short form is that Firebase Storage *is* GCS
(the TypeScript app calls `storage.googleapis.com` directly), so GCS covers
Firebase Storage as a subset while also serving every other GCP customer.

### G.0 Why this is not a flag day

The app name is `GoogleCloudStorage`, which does not collide with the TypeScript
`Firebase` app, so the coexistence constraint (previous plan §0.3) never fires:
there is no `APP-ERROR: Application ... is already registered`, no coordinated
removal, and no window in which neither app works. **GCS ships independently and
`apps/firebase` stays in `module-v8` untouched.** That is a real advantage over
building a Firebase-shaped module, which would force a flag day.

Nothing in this phase touches `module-v8`.

### G.1 It closes a gap that already exists inside the product

This is the strongest argument for the phase and it is not hypothetical:
`VertexAiDataProvider` and `GoogleDocumentAiDataProvider` already take GCS
locations as inputs and outputs — 9 `gcs_uri` options between them, plus
`gcsUri`/`gs://bucket` in their examples — and there is **no provider that can put
a file in a bucket or read one back**. Today a flow must stage GCS content by some
other means before it can call either module.

Phase G closes that, and separately makes GCS the **sixth** `conn://` file-location
target alongside `AwsS3`, `GoogleDrive`, `Dropbox`, `OneDrive` and `SharePoint`. A
file-API app is a storage backend for the whole platform, not merely an entry in
the action picker.

### G.2 Client layer — **`GcpAuth`, not `GoogleRestClient`**

`GoogleRestClient` supports only the `authorization_code` grant: interactive user
consent. That is right for consumer-facing Google products and wrong for GCS, which
in real GCP deployments is reached with a **service account**.

That path already exists. `qlib/GcpAuth.qm` provides service-account JWT-bearer
(`urn:ietf:params:oauth:grant-type:jwt-bearer`) and Application Default
Credentials, with token caching, and has four existing consumers:
`VertexAiRestClient`, `GoogleDocumentAiRestClient`, `VertexAiDataProvider` and
`GoogleDocumentAiDataProvider`.

- add `GoogleCloudStorageRestClient.qm` modelled on `VertexAiRestClient.qm`:
  `GoogleCloudStorageRestClientBase` with a sync `…RestClient` and an async
  `…RestClientIo` over it, per §0.9
- authenticate through `GcpAuth`; single host `https://storage.googleapis.com`
- connection scheme `gcs`, `GoogleCloudStorageRestConnection`
- **narrow the scopes.** The TypeScript Firebase app requests
  `devstorage.full_control` *and* `devstorage.read_write` *and* `cloud-platform`,
  which is far more than any of its actions needs; `devstorage.read_write` covers
  the object surface below, with `read_only` offered as a connection option
- interactive OAuth2 may be added later as a secondary grant for users who have no
  service account; do not design for it first

**This corrects §FB.1 of the previous plan**, which proposed the
`GoogleRestClient` `api_profile` route. That remains correct for Firebase and is
wrong for GCS.

### G.3 Object and bucket surface

`list-buckets`, `get-bucket`, `list-objects`, `get-object-metadata`,
`update-object-metadata`, `upload-object` (with resumable upload —
`uploadType=resumable`), `download-object`, `delete-object`, `copy-object`,
`compose`, `create-signed-url`.

Then `registerAppFileApi()`. GCS returns bytes directly from `?alt=media`, so this
is a `DPFR_DATA` registration, **not** the `DPFR_URL` shape SharePoint needed:

```qore
DataProviderActionCatalog::registerAppFileApi(GoogleCloudStorageDataProvider::AppName,
    <DataProviderFileApiInfo>{
        "read_action": "download-object",
        "read_path_option": "object",
        "read_container_option": "bucket",
        "read_mode": DPFR_DATA,
        "read_data_field": "content",
        "read_data_encoding": "raw",
        "read_mime_type_field": "content_type",
        "write_action": "upload-object",
        "write_path_option": "object",
        "write_container_option": "bucket",
        "write_data_option": "file",
        "write_data_format": DPFW_FILE,
        "path_is_id": True,
    });
```

`read_data_field` is required by the hashdecl even in `DPFR_DATA` mode, so
`download-object` must return the bytes in a named field rather than as a bare
value.

### G.4 Two hazards, both already paid for once

1. **GCS object names are a flat key space.** `/` is an ordinary character, not a
   path separator. The key must pass through **verbatim** — no splitting, no
   normalization, no collapsing of `//`. `AwsRestClient` already follows this rule
   for S3. Getting it wrong does not error; it silently addresses the wrong object.
   `path_is_id: True` is what tells consumers not to interpret the value; confirm
   during implementation that it does not also suppress behaviour the file API
   needs, since its documented intent is opaque identifiers rather than flat name
   spaces.
2. **Binary uploads must not go through the JSON codec.** A connection with
   `data = "json"` base64-encodes a `binary` body into a JSON string and the server
   stores the corruption with a success response — 256 bytes became 346 in the
   OneDrive case (fixed in `8c11c406b`). Use the client's raw request path
   (`restDoRawRequest()`), never `setSerialization("bin")`, which mutates
   client-wide state shared between provider objects and threads.

### G.5 Logo — **supplied**

`qlib/GoogleCloudStorageDataProvider/google-cloud-storage-logo.svg` is in place: the
four-colour Google Cloud mark, `viewBox="0 -25 256 256"`.

It needed no normalization — it is already square, and the `-25` y-offset is
already the centering pad that `google-drive-logo.svg` (`0 -4.65 87.3 87.3`) and the
SharePoint logo (`0 -23.1665 1992.333 1992.333`) use. It is well-formed XML with no
entities, scripts, external references or embedded rasters.

Load it at module level per `design/data-provider-checklist.md` — a separate file
read with `File::readTextFile(get_script_dir() + "/google-cloud-storage-logo.svg")`,
never an inlined string — and pass `"logo_mime_type": MimeTypeSvg`. Add the file as
the second argument to `qore_user_module()` in `CMakeLists.txt` so the doc build
picks it up.

Note it is the *generic* Google Cloud mark rather than the distinct Cloud Storage
bucket icon. That is a deliberate, reversible choice: the generic mark is more
widely recognised, and swapping it later is a one-file change.

### G.6 Tests

`examples/test/qlib/GoogleCloudStorageDataProvider/`, offline plus a live GCP
project with a service account. Byte-exact round trip of content containing NUL and
high bytes in **both** directions; resumable upload above the single-shot
threshold; an object whose name contains `/`, `+`, spaces and a percent sign,
asserted to be addressed verbatim; signed URL fetch; bucket and object lifecycle.
Assert field values, not shapes.

---

## Phase L — LinkedIn

Two TypeScript apps, both to be replaced:

| TS app | App name | LOC | Surface |
|---|---|---|---|
| `apps/linkedin` | `LinkedIn` | 807 | 5 actions: create text/image/video post, delete post, get current user |
| `apps/linkedin-organizations` | `LinkedInOrganizations` | 1757 | 4 actions + 1 `new-post` polling trigger |

### L.1 One client, two profiles — the SharePoint pattern again

Both apps target **the same host** (`https://api.linkedin.com`) with **the same
OAuth2 endpoints** (`https://www.linkedin.com/oauth/v2/authorization` and
`/accessToken`). They differ in exactly two things:

| | `LinkedIn` | `LinkedInOrganizations` |
|---|---|---|
| scopes | `w_member_social`, `email`, `profile`, `openid` | `r_dma_admin_pages_content` |
| API family | `/v2/userinfo`, `ugcPosts`, `assets?action=registerUpload` | `/rest/dma*` with a `LinkedIn-Version` header |
| ping | `/v2/userinfo` | `/rest/dmaMe` |

That is precisely the shape the `api_profile` mechanism exists for. So: **one
`LinkedInRestClient.qm`** with a shared base and sync + `Io` classes, carrying
profiles `member` and `dma`, and two connections pinning them
(`LinkedInRestConnection`, `LinkedInDmaRestConnection`). Duplicating the LinkedIn
OAuth2 layer across two client modules would mean fixing every LinkedIn auth bug
twice.

**Heed the constant-shadowing trap.** When a connection class subclasses another
and shadows `DefaultUrl`, three separate readers get the *base* class's value
because a Qore constant reference resolves lexically, not virtually: the inherited
`setUpdateOptionsCode()` (which **persists** the wrong URL back to the
configuration store), the static config normalizer, and Qorus's
`DataProviderAppRestClass::getAutoConnectionUrl()`, which reads the constant by
reflection and prefers `DefaultConnectionUrl` over `DefaultUrl` precisely to let a
shadowing subclass disambiguate. This cost hours in phase SP; see commit
`7c4fef515`. If the two LinkedIn connections are siblings rather than
parent/child, the trap does not arise at all — prefer that.

### L.2 Two apps, two flag days — but they can be sequenced

Both app names collide with the TypeScript catalogue, so each needs the §0.3 flag
day: the TS app must be deleted before the native one can be tested in a live
catalogue. They are independent of each other, so do them **one at a time** —
`LinkedIn` first, since it is half the size and its API is the simpler one.

Run the §0.6 cross-app dependency check before deleting either.

### L.3 Must beat the TS apps at

- **`LinkedInOrganizations` is on a version that is past its support window.** The
  DMA family pins `LinkedIn-Version: 202502`; the latest is `202605`, and LinkedIn
  guarantees only one year. This is the Phase V item the previous plan could not
  close without an approved DMA developer application — a migration is the natural
  place to close it, because the response shapes have to be re-verified anyway.
- The `new-post` trigger polls. The DMA feed APIs and `Feed Content Finder` support
  cursor-based paging (`paginationCursor`/`nextPaginationCursor`), which gives the
  same resume-where-we-left-off property that delta queries gave SharePoint;
  reconciling against seen IDs is strictly worse.
- The member app's image and video posts use the two-step
  `assets?action=registerUpload` flow and then upload the binary to a returned URL.
  **That is the binary-codec hazard again** — the upload must use the raw request
  path.
- Neither TS app exposes organization *management* or post analytics beyond
  follower statistics; the DMA surface offers considerably more
  (`dmaOrganizationalPageFollows`, `dmaSocialMetadata`, `dmaComments`,
  `dmaReactions`), which is where the "more functionality" gate is met.

### L.4 Tests

`examples/test/qlib/LinkedInDataProvider/` and
`examples/test/qlib/LinkedInOrganizationsDataProvider/`, offline plus live. A live
LinkedIn developer application is needed for **each** scope family, and the DMA
product requires a separate approval that takes days — start that request before
the phase, not during it.

---

## Phase T — Stripe

`apps/stripe` is 3413 lines and contains **zero hand-written actions**. Its surface
comes from two places: actions generated from an OpenAPI document (§T.1), and 13
webhook triggers (§T.2). Only the first raises a real question.

### T.1 The actions are generated from an OpenAPI document

`apps/stripe/index.ts` supplies `schema: stripe.swagger.json` with
`swagger_paths: createSwaggerPaths(STRIPE_ALLOWED_PATHS)` — 20 allowed paths out of
the 387 in Stripe's schema. The actions the user sees are generated at load time.

Qore has **most** of this already:

- `qlib/RestSchemaDataProvider/` builds a **provider tree** from a REST schema
  (`RestSchemaDataProvider`, `RestSchemaRequestDataProvider`, a factory), and
  `Swagger.qm`, `OpenApi3.qm` and `RestSchemaValidator.qm` parse the documents.

What is missing is narrow and specific:

- **nothing registers app *actions* from a schema.** That is
  `TypeScriptActionInterface::getRestSchema()`, which lives in Qorus's qlib, and
  the `openapi_type_overrides` mechanism it consumes has **zero consumers in the
  qore repo**.

So the real deliverable is: *generalize schema-driven app-action registration into
core qlib* — a path filter, type overrides, and `registerAction()` calls derived
from schema operations with proper `display_name`/`short_desc`/`desc`/`options`/
`output_type` per the checklist.

**That unblocks Stripe and the other ~18 schema-driven apps at once**, which is why
it should be scoped and scheduled as infrastructure, not as "migrate Stripe".

> **Alternative if Stripe alone is wanted sooner:** hand-write the 20 allowed paths
> as ordinary actions. That is genuinely feasible at this size, and it buys better
> descriptions and typed responses than generation gives. It costs the property
> that keeps the app current with upstream automatically, and it does nothing for
> the other 18 apps. Choose deliberately; do not drift into it.

### T.2 The webhook triggers are an ordinary port — the pattern is well established

The 13 hand-written triggers are all webhook-delivered: `webhook_register` creates
a `/v1/webhook_endpoints` subscription pointing at a public URL, and Stripe pushes
events to it.

This is **not** a blocker and needs no spike. `qlib/DataProvider/WebhookSupport.qc`
defines `DataProviderWebhook` — registered create/delete callbacks,
`hasWebhookSupport()`, `createWebhook()` returning a public URL, `deleteWebhook()`,
with the embedder supplying the infrastructure — and roughly **eleven modules
already use it**: Chargebee (4 event providers), Jotform (2), Linear (2),
WooCommerce, Lemlist, Instantly, Wave, CustomerIo, ShipStation, Mailgun and
EasyPost. `ChargebeeWatchSubscriptionsEventDataProvider.qc` is a good template to
copy: `createWebhook()` in `observersReady()`, `stopEventsIntern()` in
`stopEvents()`, `getEventTypesImpl()`, and `getExampleEventDataImpl()` fetching a
recent real event with a static fallback.

**Signature verification is the real gap, and it is mostly — but not entirely —
missing.** Of the 16 webhook event providers in `qlib`, exactly **two verify**:

- `EasyPostDataProvider/EasyPostBaseEventDataProvider.qc` generates a shared secret
  when it registers the webhook and checks the `x-hmac-signature` header with
  `SHA256_hmac(webhook_secret, body)`, enforced whenever a secret is configured.
- `MailgunDataProvider/MailgunBaseEventDataProvider.qc` verifies HMAC-SHA256 over
  `timestamp + token` with the signing key and **rejects** the event when it does
  not match.

The other 14 do not — Chargebee (4), Jotform (2), Linear (2), WooCommerce, Lemlist,
Instantly, Wave, CustomerIo and ShipStation. Neither do any of the **24**
`module-v8` apps that register webhooks (`stripe`, `github`, `paypal`, `jira`,
`calendly`, `typeform`, … ).

For Stripe this is not optional: an unverified endpoint accepts forged
`charge.succeeded` and `invoice.payment_failed` events, so a caller could fabricate
payment outcomes. Stripe signs every delivery with `Stripe-Signature` — HMAC-SHA256
over `timestamp.payload`, with a tolerance window against replay.

The good news is that there is nothing to invent: `SHA256_hmac()` is available, and
EasyPost and Mailgun are two working in-repo templates with slightly different
shapes (header-based versus payload-block, self-issued secret versus provider
signing key). Phase T should follow them, and should lift the common part onto the
shared webhook layer so the other 14 providers can adopt it — a platform
improvement this phase is the right occasion for, not Stripe-specific work.

### T.3 The API version pin resolves itself here

The previous plan's Phase V left `api_version: '2024-12-18.acacia'` alone, because
the current version is `2026-07-29.dahlia` — three major releases later — and the
triggers declare fields those releases changed (`currency_conversion` was removed
from `checkout.session` in Clover; `collected_information.tax_ids` was renamed to
`tax_id` in Dahlia). Bumping without updating the declared shapes would silently
change payloads.

A Qore port declares its event types from scratch against the **current** version,
so it closes that item rather than inheriting it. Pin the version explicitly at
webhook-endpoint creation and state it in the module documentation.

### T.4 Recommendation

Decide §T.1 before scheduling anything, because it is the only thing that makes
phase T large:

- **If schema-driven app-action registration is funded as infrastructure**, size it
  on its own merits — it is justified by ~19 apps, not by Stripe — and take Stripe
  as its first consumer and proof.
- **If not**, hand-write the 20 allowed paths and phase T becomes a normal port of
  comparable size to phase L.

Either way, do the webhook signature verification of §T.2 as part of this phase and
generalise it, since eleven existing providers would benefit.

---

## Sequencing

```
Phase G  (Google Cloud Storage)  ── independent, no flag day, closes an internal gap
                                    needs: a GCP service account (logo already in place)
Phase L  (LinkedIn)              ── independent of G; two flag days, do them one at a time
   ├─ L: LinkedIn (member)       ── smaller, simpler API, do first
   └─ L: LinkedInOrganizations   ── also closes the stale 202502 DMA version pin
                                    needs: an approved DMA developer application (lead time)

Phase T  (Stripe)                ── size depends entirely on the T.1 decision
   ├─ T.1 schema-driven actions  ── EITHER infrastructure unblocking ~19 apps,
   │                                OR hand-write 20 actions and stay small
   └─ T.2 webhook triggers       ── ordinary port (16 precedents), plus generic
                                    signature verification that only 2 of them have
```

**Recommended order: G → L → T.** G is self-contained and has no flag day, so it
carries the least schedule risk and delivers a capability two shipped modules
already need. L is a direct application of a pattern just proven in phase SP. T
should not start until the §T.1 question is answered, since that decision changes
its size by an order of magnitude.

## Out of scope

- Firebase Auth and FCM. They are separate products bundled into the TypeScript
  Firebase app; if wanted they are separate modules with their own hosts, and the
  legacy `iid.googleapis.com` endpoint should not be carried forward.
- The remaining ~18 schema-driven apps. They are unblocked by §T.1 but each is its
  own port afterwards.
- Any attempt to remove the ts-proxy. Even with these three migrated, ~85
  TypeScript apps remain.

## Decisions needed before coding starts

1. **Live tenants**: a GCP project with a service account (phase G); a LinkedIn
   developer application for the member scopes and a second, separately approved,
   DMA application (phase L); a Stripe account with webhook access (phase T).
2. **§T.1 scope** — is schema-driven app-action registration funded as
   infrastructure for ~19 apps, or is Stripe hand-written as 20 actions? This
   changes phase T from a large infrastructure item to a small port, and changes
   what the other 18 apps cost later.
3. **Should webhook signature verification be generalised** (§T.2)? Only 2 of the
   16 webhook providers verify (EasyPost, Mailgun), and none of the 24 webhook
   apps in `module-v8` do. Stripe forces the issue because forged payment events
   are a real risk, but the fix belongs on the shared webhook layer rather than in
   one app.
