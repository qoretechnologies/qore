# Evaluation — porting the Paddle app to Qore

Evaluated 2026-08-09 against `module-v8/ts/src/apps/paddle`.

## Recommendation

**Port it, and port it schema-driven — but only after `OpenApi3.qm` learns OpenAPI 3.1.**

Paddle publishes an official OpenAPI description, so this is the same shape of work as Stripe:
a manifest plus a UX overlay over `RestSchemaActions`, not 6,000 lines of hand-written actions.
The infrastructure that made Stripe cheap already exists and applies unchanged.

The gate is real, though: Paddle's spec is **OpenAPI 3.1**, and `OpenApi3.qm` cannot parse 3.1
today. That work is a shared-infrastructure investment rather than a Paddle cost — 3.1 is
increasingly the norm for newly published specs — but it has to happen first, and it is the
dominant part of the estimate.

## What exists today

|!Property|!Value
|Source|`ts/src/apps/paddle`, registered in `ActionsCatalogue`
|Size|6,328 lines
|Actions|24, **hand-written**, over 6 resources (customers, prices, products, reports, subscriptions, transactions)
|Triggers|5, **polling** (`pollCreatedItemsForTrigger`, ordered by \c created_at)
|Implementation|built on the official \c @paddle/paddle-node-sdk (28 files import it), not raw REST
|Auth|API key as a bearer token, plus an \c instance_type option selecting production or sandbox
|Ping|\c GET /event-types

There is no Paddle client in `qlib` yet, so the port starts from nothing on the Qore side.

## The upstream schema

|!Property|!Value
|Source|https://github.com/PaddleHQ/paddle-openapi — `v1/openapi.yaml`
|Size / retrieved|7,403,758 bytes, updated 2026-07-09, retrieved 2026-08-09
|sha256|`e1ac347eb3c5ff208fdbd6af9c4b3f7e9e4e9212be881468ee4f97792058734b`
|OpenAPI version|**3.1.0**
|Paths / operations|70 / 99
|Component schemas|519
|Servers|`https://sandbox-api.paddle.com`, `https://api.paddle.com`
|Webhook events|**56**, in the 3.1 top-level `webhooks` section

It is roughly an order of magnitude smaller than Stripe's (416 paths / 1440 schemas), and parses
in **0.66 s** once normalized — so pruning may not even be necessary.

Coverage of what the TypeScript app exposes is complete, with room to grow:

|!Resource|!Operations in the schema
|customers|18
|subscriptions|12
|transactions|7
|prices|4
|products|4
|reports|4

That is 49 operations against the 24 actions shipped today, before counting 14 further resource
families the current app does not expose at all — notifications and notification settings,
simulations, metrics, discounts, adjustments, checkout domains, client tokens, events and more.

## The blocker: OpenAPI 3.1 support in OpenApi3.qm

Parsing the unmodified spec fails. Working through the failures by normalizing each construct
and re-parsing gives the full list, with counts from this spec:

|!Construct|!Count|!Note
|`type: "null"`|1,504|3.1 expresses nullability this way; 3.0 used `nullable: true`. **Hard parse failure.**
|`examples:` as an array|2,179|3.1 replaced the singular `example`
|`unevaluatedProperties`|78|JSON Schema 2020-12
|`discriminator` as an **object**|1|see below — this one is not a 3.1 issue at all
|top-level `webhooks`|56 events|3.1 feature; not needed for actions, valuable for triggers

**`discriminator` is a pre-existing bug, not a 3.1 gap.** `OpenApi3.qm` expects a string, which
is the *Swagger 2.0* form; in OpenAPI **3.0 and 3.1 alike** it is an object
(`{propertyName, mapping}`). Any 3.0 spec using polymorphism hits this too, so it is worth fixing
regardless of whether Paddle is ported.

After normalizing the above, the spec parses and the action layer works. Four representative
operations built through `RestSchemaActionSet` with no Paddle-specific code:

|!Action|!Options|!Response|!First options
|list-customers|8|200|`id`, `after`, `per_page`, `email`, `order_by`, `status`
|create-customer|7|201|`id`, `name`, `email`, `marketing_consent`, `custom_data`, `locale`
|get-customer|1|200|`customer_id` (path variable, flattened)
|list-transactions|15|200|`include`, `id`, `after`, `billed_at`, `collection_mode`, `created_at`

All four produced registerable action definitions with options and output types. Note that
`create-customer` answers **201**, which the single-success-response rule handles without a
manifest hint.

## How the port would go

1. **OpenAPI 3.1 in `OpenApi3.qm`** — accept `type: "null"` and union `type` arrays as
   nullability, accept `examples` arrays, ignore `unevaluatedProperties`, and fix `discriminator`
   to the 3.x object form. Tests belong with the existing OpenApi3 suite. This is the bulk of the
   work and it benefits every 3.1 API, not only Paddle.
2. **`qlib/PaddleRestClient.qm`** — `paddle://` scheme, bearer API key, and a sandbox/production
   selector. The schema declares both hosts and the sandbox is listed **first**, so the connection
   must choose deliberately rather than inherit the schema's target URL. `required_options` must
   name the API key; there is no OAuth2 (`oauth2_grant_type: 'none'` in the current app).
3. **`qlib/PaddleDataProvider/`** — vendored pinned schema, a manifest over the selected
   operations, and an overlay re-derived against the current contract. The current app's
   allowed-value helpers map onto declarative `ref_data`.
4. **Triggers: replace polling with webhooks.** The current 5 triggers poll ordered by creation
   time, which cannot see anything created and removed between polls. Paddle signs webhooks with
   `Paddle-Signature: ts=…;h1=…` — the same timestamp-plus-HMAC-SHA256 shape as Stripe, so
   `DataProvider::WebhookSignature` covers it. One small addition is needed:
   `parseKeyValueHeader()` splits on `,` and Paddle uses `;`, so the separator has to become a
   parameter.

## Why it is worth doing

- **It is the second schema-driven app**, so it proves the `RestSchemaActions` layer generalizes
  beyond the app it was written for — with the OpenAPI 3.1 work being the only new cost.
- **Every polling trigger becomes a real webhook**, with signature verification, which the
  current app has none of.
- **The exposed surface can grow** from 24 actions to whatever is reviewed as useful, without
  hand-writing request and response contracts.
- **Upstream drift becomes visible** through `RestSchemaDrift` instead of being discovered by a
  user.

## Not yet verified

- No live Paddle account was used; nothing here is confirmed against the running API.
- The normalization above was a throwaway script to size the work — it is **not** the proposed
  implementation. The fix belongs in `OpenApi3.qm`, per the standing rule that unsupported valid
  OpenAPI is a parser gap rather than something to pre-process around.
- The SDK-to-REST mapping for the 24 current actions was not worked through operation by
  operation; the resource-level coverage is complete, but individual behaviours the SDK smooths
  over (pagination cursors, error envelopes, idempotency) still need checking during the port.
