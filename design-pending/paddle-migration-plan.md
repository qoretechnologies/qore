# Plan — OpenAPI 3.1 support in OpenApi3.qm, and the Paddle port

Planned 2026-08-09, to be executed on `feature/5388_app_migration`.
Supersedes the sizing in [paddle-migration-evaluation.md](paddle-migration-evaluation.md), whose
OpenAPI 3.1 gap list was measured by bulk normalization and **overstated the work** — see phase 0.

## Scope

Replace the TypeScript Paddle app (`module-v8/ts/src/apps/paddle`, 6,328 lines, 24 hand-written
actions, 5 polling triggers) with a native Qore app whose actions are generated from Paddle's
official OpenAPI description through `RestSchemaActions`, exactly as Stripe (phase T) was.

Two things gate it, and only one is real work: `OpenApi3.qm` must accept two constructs it
currently rejects.

## Hard constraint — connection option compatibility

There is a live, working `paddle-sandbox` connection. The native connection **must accept the same
option names and values**, so it can be recreated with identical `opts` on the flag day.

|!Option|!Contract|!Source
|`token`|The API key, sent as a bearer token. Display name \c "API Key".|`PADDLE_CONN_OPTIONS.token`
|`instance_type`|\c "api" (Production, **default**) or \c "sandbox-api" (Sandbox)|`PADDLE_CONN_OPTIONS.instance_type`
|`token_type`|\c "Bearer"|live connection
|`data`|\c "json"|live connection
|`ping_method` / `ping_path`|\c GET \c /event-types|live connection

`token` and `token_type` are already standard `RestConnection` options, so **`instance_type` is the
only app-specific one to define**. The target URL is derived as `https://{instance_type}.paddle.com`
— which reproduces the live connection's `sandbox-api.paddle.com` exactly.

There is **no OAuth2** (`oauth2_grant_type: "none"`), so unlike Stripe, `required_options` names the
API key only and no API-server client is involved.

## Phase 0 — OpenAPI 3.1 in OpenApi3.qm (the gate)

**This is two small fixes, not an infrastructure project.** Probing each construct in isolation
against the current module showed most of 3.1 already works:

|!Construct|!Status today
|`type: ["string", "null"]` (array form)|**already supported** — sets nullable, filters `null`
|`examples:` as an array|already accepted
|`unevaluatedProperties`, `prefixItems`, `const`|already accepted
|top-level `webhooks`|**already parsed** — `OpenApi3Schema::webhooks`, all 56 Paddle events
|`openapi: 3.1`|already accepted (`OpenApi3.qm:724`)
|numeric `exclusiveMinimum`/`exclusiveMaximum`|already handled (3.0 bool / 3.1 numeric)
|`contentEncoding` / `contentMediaType`|already handled

Only two constructs fail:

**0.1 — scalar `type: "null"`** (`OpenApi3.qm:~4479`). The list branch handles `null`; the string
branch does not, so a standalone `{"type": "null"}` — overwhelmingly as an `anyOf` member, which is
how 3.1 expresses nullability in a composition — throws
`INVALID-FIELD-VALUE ... invalid schema type value passed: "null"`. Fix symmetrically with the list
branch: set `nullable`, and treat the type as unconstrained.

*Decision to make:* the list branch maps a null-only type to `"any"`. Mapping a standalone null
schema the same way is consistent but loses the "only null is valid" constraint. Consistency with
the existing branch is the recommendation; note it rather than inventing a null type.

**0.2 — `discriminator` as an object** (`OpenApi3.qm:4572`). The module requires `NT_STRING`, which
is the **Swagger 2.0** form. **OpenAPI 3.0 and 3.1 both define an object** `{propertyName, mapping}`,
so this is a pre-existing bug affecting 3.0 specs too, not a 3.1 gap. Accept the object form
(reading `propertyName`) and keep the string form for back-compat.

*Second defect in the same code:* it **throws** when the discriminator property is not in `required`.
OpenAPI 3.x says the property *SHOULD* be required, not MUST, so that throw rejects valid 3.x
documents. Relax it for the object form.

**Tests** — extend `examples/test/qlib/OpenApi3/OpenApi3.qtest`:
- both new forms, positive and negative;
- **regression cases for the six constructs above that already work** — they are currently
  untested, so lock them in before touching the type parser;
- a small checked-in 3.1 fixture alongside `PetStore.openapi3.yaml`.

**Verification:** the unmodified 7.4 MB Paddle spec parses. Already confirmed with a throwaway
patched copy — both fixes applied, parse succeeded, 56 webhooks parsed, paths resolve including
path-variable matching. That copy is **not** the implementation; make the fixes properly in
`qlib/OpenApi3.qm` and rebuild `OpenApi3-qmod` (a stale `.qmod` silently shadows the source).

Phase 0 stands on its own and is worth having regardless of Paddle.

## Phase 1 — vendored schema

The full document parses in **11.3 s**, far too slow to do per provider construction, so prune as
Stripe did with `RestSchemaActions::RestSchemaPruner` (already committed).

- vendor as `qlib/PaddleDataProvider/paddle-openapi.yaml`, pruned to the manifest's operations and
  formatted, so schema updates review as a diff;
- record provenance in `PaddleSchema.qc` — upstream URL, sha256 of the **upstream** file
  (`e1ac347eb3c5ff208fdbd6af9c4b3f7e9e4e9212be881468ee4f97792058734b`), upstream date 2026-07-09,
  import date;
- pruning is driven by the manifest, so phase 2's operation list settles first.

## Phase 2 — `qlib/PaddleRestClient.qm`

`paddle://` scheme, mirroring `StripeRestClient.qm`, with the compatibility contract above.
The lessons that cost live debugging on Stripe apply directly:

- **`getAppName()` must be overridden** — the base returns nothing and `getInfo()` omits `app`.
- **`required_options`** must name `token`.
- **Never inherit the schema's target URL** — Paddle lists **sandbox first**, so a connection that
  takes `servers[0]` silently talks to sandbox. Derive from `instance_type`.
- **`getDataProvider()` passes only declared options** (the GCS defect), not the whole option hash.

## Phase 3 — `qlib/PaddleDataProvider/`

Mirror the Stripe layout: `PaddleDataProvider.qm`, `PaddleDataProviderFactory.qc`,
`PaddleManifest.qc`, `PaddleSchema.qc`, `paddle-logo.svg`.

- **Manifest** — map the 24 current TS actions to operations first, so the port is at least at
  parity. The mapping must be worked through **per action**: the TS app calls
  `@paddle/paddle-node-sdk`, not raw REST, so pagination cursors, error envelopes and idempotency
  need checking rather than assuming. Expanding beyond 24 (the schema has 49 operations across the
  same six resources, plus 14 unexposed families) is a separate, reviewable decision.
- **Overlay re-derived, never copied** from the TS source — re-deriving is what caught two real
  defects in the Stripe app. ID dropdowns become declarative `ref_data` and must set
  `supports_custom_values`.
- **Codecs** — Paddle uses RFC3339 timestamp strings, not Stripe's `unix-time` integers, so the
  existing codecs are likely unnecessary. Confirm before adding any.
- **Registration in four places**, and `doxygen/lang/120_modules.dox.tmpl` plus
  `900_release_notes.dox.tmpl` lists must stay **alphabetical**.
- `x-permissions` on each operation names the API-key permission it needs. Surfacing it in action
  descriptions is a cheap usability win — optional, decide during review.

## Phase 4 — triggers: polling becomes webhooks

The 5 TS triggers poll ordered by creation time and therefore cannot see anything created and
removed between polls, and verify nothing. All five map to real webhook events, confirmed present
in the spec's 56: `customer.created`, `product.created`, `subscription.created`,
`transaction.created`, `report.created`.

- Paddle signs with `Paddle-Signature: ts=…;h1=…` — the same timestamp-plus-HMAC-SHA256 shape as
  Stripe, so `DataProvider::WebhookSignature` covers it after **one change**:
  `parseKeyValueHeader()` splits on `,` and Paddle uses `;`, so the separator becomes a parameter.
- Webhook lifecycle is real API, not manual setup: `/notification-settings` supports POST and
  DELETE (plus GET/PATCH), so destinations can be created and torn down like Stripe's endpoints.
- Event selection: at minimum the 5 that replace polling.

## Phase 5 — tests

- **Offline** `examples/test/qlib/PaddleDataProvider/PaddleDataProvider.qtest` against the vendored
  schema: action registration, request flattening and the reverse map, option and output types,
  `instance_type` URL derivation, connection options, drift via `RestSchemaDrift`, and signature
  verification positive/negative/tampered/wrong-secret/stale-timestamp.
- **Live**, gated on `-c paddle-sandbox` for reads and `-W` for writes, per the Stripe pattern.
  Run the suite **with `-c`** before calling the phase done.
- Negative and corner cases are required, not optional.

## Phase 6 — removal and flag day

Remove `ts/src/apps/paddle` from module-v8 with its i18n entries and `ActionsCatalogue`
registration, as was done for Stripe (`2ff01833`). Release notes in both doxygen templates.

Nothing in either repo is pushed; `qore` and `module-v8` ship together, and **not without approval**.

## Sequencing and risk

Phase 0 is the only true blocker and is now small. Phases 1–3 are the bulk. Phase 4 is small given
`WebhookSupport.qc`. The largest remaining unknown is the **SDK-to-REST mapping** in phase 3 — that
is where hidden behaviour differences live, and where the live `paddle-sandbox` connection earns its
keep.

Open decisions to confirm during execution, so they are not silently made:
1. null-schema mapping (phase 0.1);
2. whether to relax the discriminator `required` check for the string form too, or only the object
   form (phase 0.2);
3. staying at 24 actions versus expanding (phase 3);
4. the webhook event list (phase 4).
