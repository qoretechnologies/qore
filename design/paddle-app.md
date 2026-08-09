# The Paddle application

Phase P of issue #5388: the TypeScript Paddle app (`module-v8`, `ts/src/apps/paddle`) was replaced by
two native Qore modules, `PaddleRestClient` and `PaddleDataProvider`, with actions generated from
Paddle's own OpenAPI 3.1 description by `RestSchemaActions`.

This records the decisions that are not evident from the code. What each module does, and how the
schema is pinned, is documented in the modules themselves — `PaddleDataProvider.qm`'s main page and
the header of `PaddleManifest.qc`.

## Why it needed OpenApi3 work first

An early evaluation measured OpenAPI 3.1 support by bulk-normalizing Paddle's document and concluded
a large gap. Probing each construct in isolation showed that most of 3.1 already worked, and only two
constructs actually failed to parse: a scalar `type: "null"`, and the object form of `discriminator`
(which is the 3.0 form too — the module only accepted Swagger 2.0's string). Two further defects
surfaced while generating the actions rather than while parsing: `anyOf: [X, null]` widened the whole
composition to `any`, and a single-member `allOf` wrapping a `$ref` discarded the referenced type.
Both cost real fidelity — 13 of Paddle's action options came out untyped.

**Lesson:** measure a schema gap construct by construct against the parser. A bulk normalizer conflates
"the parser rejects this" with "the parser accepts it but produces a poor type", and the second is
invisible until the types are used.

## Decisions

| Decision | Rationale |
|---|---|
| Action IDs keep the TypeScript app's `snake_case` (`list_products`, not `list-products`) | Action identity is what existing workflows call. `qorus-api`'s `QorusSaasAdminRestHandler` resolves five of them by name as child providers. The Stripe app's IDs were schema-derived, so it had no such constraint; Paddle's were hand-written and in use. |
| 24 actions, at parity with the app replaced | The schema exposes 99 operations across 70 paths. Expanding the exported set is a separate, reviewable decision; the port's job was to replace, not to grow. |
| A listing returns Paddle's `{data, meta}` envelope; a single-entity action returns the entity | `meta.pagination` carries the cursor without which the next page cannot be fetched, and the app it replaced discarded it. A single-entity `meta` holds only a request ID, so there is nothing to keep. |
| `archive_product` is a declared variant of `update_product` | Both are `PATCH /products/{product_id}`. `RestSchemaActions` refuses to export an operation twice, which catches a copy-and-paste slip; the deliberate case is now declarable with `variant_of` rather than the guard being removed. |
| 16 webhook events, replacing 5 polling triggers | The five the triggers polled, plus the billing outcomes a Paddle integration cannot work without: payment failure, subscription activation, cancellation and past-due, catalog changes, and report readiness. |
| No wire codecs anywhere | Paddle's timestamps are RFC 3339 strings, not epoch counts. Stripe needed a unix-seconds codec; here a codec would be a conversion nobody asked for. |

## The environment is the connection's identity

Paddle's OpenAPI description lists **sandbox first** in `servers`, so a client that adopted
`servers[0]` would quietly talk to sandbox in production. The connection derives its URL from the
`instance_type` option in both directions, and does not pass a URL to the client at all, so a stored
URL that disagrees with the option can never decide where requests go.

## Signature verification

Paddle signs `"{ts}:{body}"` with HMAC-SHA256 and presents it as `Paddle-Signature: ts=…;h1=…`. It is
the same shape as Stripe's but differs in both separators — semicolon between pairs where Stripe uses
a comma, colon between timestamp and body where Stripe uses a full stop. The pair separator is now a
parameter of `WebhookSignature::parseKeyValueHeader()`; the payload separator is per-provider code.

## Not carried over

`Skip-Count` is exported as the `skip_count` option and sent as Paddle's header, but Paddle does not
always honour it: a small collection comes back counted either way, verified with a raw request. The
test asserts the header is sent and the page is usable, not what Paddle puts in the count.

The `RestSchemaPruner` drops a document's top-level `webhooks` section, because pruning selects
operations and a webhook is not one. That is not a problem here — the event list is a module constant
checked against the schema's `EventTypeName` enum, which the pruned document retains because the
`/notification-settings` operations reference it — but a future application that wants to generate
event types from `webhooks` will need the pruner to keep them.
