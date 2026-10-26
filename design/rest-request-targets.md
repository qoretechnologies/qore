# REST request targets

Copyright (C) 2026 Qore Technologies, s.r.o.

Issue [5485](https://github.com/qoretechnologies/qore/issues/5485) separates three
different HTTP addressing operations. `HTTPClient::send()` sends a request target,
with an empty argument selecting its connection/default path. `sendUrl()` resolves
a URI reference under RFC 3986. `HttpClientConnectionManager::request()` uses its
URL for the connection origin and accepts a complete target, defaulting to `/`.
Their transport contracts remain unchanged.

REST endpoints instead belong to an API namespace. `RestRequestTarget`, exported
by the existing `RestClient` module, makes that namespace explicit:

```qore
rest.restDoRequestTarget("GET", <RestRequestTarget>{"path": "orders"}, NOTHING, NOTHING);
rest.restDoRequestTarget("GET", <RestRequestTarget>{
    "mode": RestPathMode::OriginAbsolute, "path": "/health"}, NOTHING, NOTHING);
```

`ApiRelative` is the default. A leading slash does not change its meaning, and
matching an API-root prefix does not change its meaning. `OriginAbsolute` requires
a leading slash and never interprets a host or changes the connection origin.
Full URLs and fragments are rejected. Redirects continue to use the shared URI
resolver; API-root composition is never applied to a redirect target.

The `rest` command accepts full endpoint URLs at its CLI boundary. It removes
the fragment before parsing, separates the connection origin from the path and
query, and dispatches that target as `OriginAbsolute`. An absent path becomes
`/`; repeated slashes and empty queries are retained. Named-connection request
paths remain `ApiRelative`. Request exceptions produce a nonzero exit status;
`--ignore-errors` still allows HTTP error responses through the normal output
path.

## Composition and configuration

The pure `rest_request_path()` function normalizes only the joining boundary.
For either `/api` or `/api/` as the root:

|!Endpoint|!ApiRelative target
|`users`, `/users`|`/api/users`
|`/api/users`|`/api/api/users`
|`/apiary/users`|`/api/apiary/users`
|empty, `/`|`/api`
|`/users/`|`/api/users/`
|`?page=2`|`/api?page=2`
|`?`|`/api?`
|`/users//x`|`/api/users//x`

Internal slashes, percent escapes, dot segments, query order, repeated query keys,
and a defined empty query are preserved. The Io adapter encodes after
composition and before validation, signing, and dispatch. Synchronous target
methods retain the native HTTPClient encoding options.

For `RestClientIo`, an explicit connection-path override wins, including an empty
or cleared override. Otherwise the URL path wins, including `/`; `default_path`
is used only if the URL has no path. The selected root is retained independently
of pooled socket reconnections. Roots are path prefixes: constructor URL paths,
`default_path`, and explicit roots containing queries or fragments are rejected.
Callers must move the query into the request target. This prevents appending an
endpoint inside a root's query.

A validator whose `managesUriPath()` returns true owns the API namespace. Its
`getBasePath()` replaces the connection root rather than being concatenated with
it. A schema builder that has already included that base path must send an
explicit `OriginAbsolute` target. Validation receives the effective target.

The released synchronous `RestClient` string methods retain their legacy path
composition and raw-request behavior. Its additive target methods use the shared
composer. The target methods also distinguish explicit clearing from the native
HTTPClient default-path fallback; this does not change that fallback for legacy
string calls.

## Request lifetime

Each Io request retains a typed `RestRequestContext` containing its encoded target
and validator. Root selection and the validator snapshot happen under the
configuration read lock. Provider query parameters are added through
`prepareRequestPath()` before encoding; that hook runs once, under the same lock,
and must not acquire the configuration lock or mutate client configuration.

Preparation hooks receive this context, so credentials and signatures can be
refreshed while the path stays fixed. Blocking, async, caller-driven polling,
SSE-reader, and SSE-observable requests reuse it across authentication and
transport retries. The response uses the request's validator even if another
request changes the client's validator meanwhile. An SSE observable captures its
target when created, before `observersReady()` starts transport work.

An already serialized body is independent of path mode. The raw target API skips
request serialization but still resolves the target and validates the response.
The async and polling target APIs accept a `raw` body flag; SSE readers accept
serialized bodies, while observables serialize them. The internal prepared
context is distinct from the caller-facing target type.

## Caller migration

* OpenAI-compatible LLM providers retain their provider-specific `/v1` rules and
  send the resulting complete paths as `OriginAbsolute`. Azure OpenAI and
  Perplexity keep API-relative endpoint overrides.
* Anthropic messages and generic embedding/reranking clients compose their own
  final `/v1` segment before sending an absolute target. Jina's versioned content
  endpoints and DeepSeek's upload endpoint also select their complete targets.
* OpenAPI, Swagger, and REST-schema builders dispatch their fully composed paths
  as `OriginAbsolute`. Foundry actions explicitly bind the project prefix into
  their schema namespace without modifying a shared schema. Generic API calls
  use API-relative endpoints with either
  serialized or structured bodies.
* OData continuation URLs supply complete targets. Providers that deliberately
  strip their service root from pagination links continue to use relative
  endpoints. Direct connection-manager calls already supply complete targets.
* Connection pings use explicit targets instead of clearing a client's root.
  Absolute ping URLs select a sibling client for the URL's origin and retain the
  full path and query as the target.
* The synchronous SSE bridge resolves through its synchronous client's target
  rules, then dispatches that complete target explicitly through Io.
* AWS, Unleashed, provider query decorators, response-error adapters, rate-limit
  adapters, and test doubles carry the new request context or target API. Bedrock
  preserves its control/runtime endpoint routing and encoded SigV4 policy.
* Io subclasses overriding preparation or response hooks must forward the new
  optional request context. Direct implementations of `AbstractRestClient` must
  implement the two explicit target methods. Existing subclasses of `RestClient`
  inherit those methods; legacy string hooks remain available.
* DataStream's separately documented streaming-path contract remains explicit:
  a leading slash selects an origin-absolute streaming target. Its shared
  composer replaces the local slash concatenation.

## Regression coverage

`examples/test/qlib/RestClientIo/RequestTargets.qtest` checks actual wire targets
over HTTP/1.1, HTTP/2, and HTTP/3, both root spellings, default precedence,
queries, escapes, raw bodies, async futures, caller-driven polling, signing,
SSE readers and observables, redirects, concurrent path modes, and validator
snapshots across authentication retries. Existing HTTP, REST, provider, schema,
ping, and SSE suites remain the compatibility checks. In particular,
`OpenAiApiRoot.qtest` retains the regression coverage for issue 5484.
