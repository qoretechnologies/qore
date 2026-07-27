# Generic "Make an API call" Action

Every REST-capable data-provider app in Qore qlib and module-v8 automatically
exposes a `make-api-call` action that accepts arbitrary method/path/body/headers
and dispatches the call through the connection's authenticated REST client.
This document describes how the auto-injection is wired so adding a new REST
provider requires zero per-provider boilerplate.

## Goals

- Make the same generic-call action available on every REST app — Anthropic,
  OpenAI, Linear, YouTube, Stripe, every TypeScript app from module-v8 — without
  per-provider opt-in.
- Reuse the connection's existing authentication (OAuth2, API key, signed AWS,
  basic, bearer) with no extra wiring.
- Surface the action in the UI's action picker with a structured form: a body
  editor that reshapes when the user picks a body type (JSON / form / multipart
  / XML / YAML / text / binary / auto), path-variable templating, response-
  status assertions, error passthrough, and an optional debug payload.
- Stay framework-side: no class hierarchy refactors, no module circular deps.

## Non-Goals

- Replacing per-API typed actions. The hand-written `create-message`,
  `list-models`, etc. remain the primary actions; `make-api-call` is the
  escape hatch for endpoints that don't yet have a first-class wrapper.
- Wrapping non-REST connections (DB, FTP, MongoDB, WebSocket). These are
  automatically excluded by the positive REST predicate
  (`DataProviderActionCatalog::isRestScheme()`) — no per-app opt-out
  needed; see [Gating: positive REST detection](#gating-positive-rest-detection).
- Supporting pagination, retries, streaming responses, or per-call OAuth2 scope
  overrides. Each is a future extension; see [Open Issues](#open-issues).

## Architecture

Three coordinating layers:

| Layer | Module | Responsibility |
|-------|--------|----------------|
| Framework contract | `DataProvider` | Declares the `getRestClientForGenericCall()` virtual, the auto-injected `__call__` child, the `disable_generic_api_call` opt-out, and the catalog auto-registration that fires on `registerApp()`. |
| Generic-call implementation | `RestClientDataProvider` | Supplies the `GenericApiCallProvider` (a tiny data-provider class that wraps any `AbstractRestClient`) plus the request- and response-type schemas, then registers the framework's two builder hooks. |
| Per-provider wiring | none required for ~99% of providers | Reflective discovery finds the REST client on every provider that follows the universal `rest` convention (qore qlib) or the `conn` convention (module-v8). Per-provider overrides exist only as escape hatches for non-standard layouts. |

The `DataProvider` module purposely has no link-time dependency on
`RestClientDataProvider` or any REST transport — the implementation flows in
via runtime-registered builder hooks, keeping the dependency direction
acyclic.

## Framework contract (DataProvider module)

### `AbstractDataProvider::getRestClientForGenericCall()`

Public method returning the underlying REST client object (typed as `*object`
so the `DataProvider` module avoids a hard link to RestClient/RestClientIo).
Delegates to a protected virtual `getRestClientForGenericCallImpl()` whose
default implementation reflectively discovers a REST client:

1. Walks `Reflection::Class::getClass(self)` looking for a member named
   `"rest"` whose value is an object with a callable `restDoRequest()` method —
   the universal qore-qlib convention `private { RestClientIo rest; }`.
2. If not found, looks for a member named `"conn"` whose value is an object
   with a callable `get(bool)` method — the module-v8 TypeScript convention
   `private { AbstractConnection conn; }`. Calls `conn.get(False)` to fetch
   the underlying client without forcing a network connection.
3. Returns `NOTHING` if neither convention matches — opting the provider out
   automatically.

The duck-type checks (`hasCallableMethod("restDoRequest")` and
`hasCallableMethod("get")`) avoid forcing the `DataProvider` module to import
specific REST classes; both `RestClient::RestClient` (sync) and
`RestClientIo::RestClientIo` (async) extend `RestClient::AbstractRestClient`
which defines `restDoRequest()`.

Reference: `qlib/DataProvider/AbstractDataProvider.qc`, search for
`getRestClientForGenericCallImpl`.

### Reserved names

```qore
const GenericApiCallChildName  = "__call__";    // virtual child provider name
const GenericApiCallActionName = "make-api-call"; // catalog action name
const GenericApiCallRestMemberName       = "rest"; // reflection convention #1
const GenericApiCallConnectionMemberName = "conn"; // reflection convention #2
```

These names are reserved framework-wide. User-defined providers must not
publish a `__call__` child or a `make-api-call` action.

### Auto-injected `__call__` child

The public `getChildProvider()` and `getChildProviderEx()` overlays on
`AbstractDataProvider` intercept lookups of `__call__`: when
`hasGenericApiCallChild()` is true, the framework constructs the child by
calling `cb_build_generic_api_call_child(rest_client)` and returns the result;
otherwise the lookup fails as unknown. Either way the reserved name is **never**
passed to the provider's own `getChildProviderImpl()`. The name is reserved
framework-wide, so delegating it buys nothing and actively hurts: a dynamic
provider (one that resolves children with a remote lookup — Salesforce
SObjects, database tables, bucket listings) would issue a request for a child
that cannot exist, producing a spurious 404 and an error log entry on every
resolution, at any level of the tree.

The `getChildProviderNames()` overlay similarly appends `__call__` to the
provider's own list when `hasGenericApiCallChild()` is true.

### Injection point: once per provider tree, at the root

`__call__` is injected **only at the root of a provider tree**, matching the
`"/__call__"` path the `make-api-call` action is registered with. Four
conditions gate `hasGenericApiCallChild()`:

1. the provider is not inside a subtree that already exposes the child
2. a child builder has been registered
3. the provider has not opted out via `hasGenericApiCallChildImpl()`
4. `getRestClientForGenericCall()` returns a REST client

Condition 1 is what makes the injection root-scoped. The reflective discovery
in `getRestClientForGenericCallImpl()` matches *every* provider in an app's
tree, not just the root, because the `rest` member (qlib) / `conn` member
(module-v8) is declared on a shared provider base class that all descendants
inherit — 6 to 65 subclasses per app. Those per-node copies carry no value:
the child is built from the REST client alone and has no parent context, so
`aftership/trackings/__call__` dispatches exactly like `aftership/__call__`.
Worse, `GenericApiCallProvider` holds the client in a `rest` member itself, so
unconstrained discovery makes every `__call__` advertise a `__call__` of its
own — an infinitely deep tree that no recursive walk (UI browse, catalog
crawl, `getChildProviderSummaryInfo()` recursion) can terminate.

The framework therefore marks every child it hands out from
`getChildProvider()` / `getChildProviderEx()` — and every child it builds in
`buildGenericApiCallChild()` — as suppressed. A provider is a root by default:
providers obtained from a factory, from a connection, or by direct
construction all expose the child. Suppression propagates from the provider
that actually exposes the child, so a REST subtree hanging off a non-REST
parent still gets its own `__call__` at the subtree root. Testing the cheap
flag before the reflective discovery also means no reflection is performed
anywhere below a suppressed root.

### Runtime side of the app opt-out

An app that sets `disable_generic_api_call` suppresses the catalog action, but
the runtime provider tree is a separate code path: a provider has no way to
look up the app it belongs to, so the opt-out must be declared on both sides.
Providers of an opted-out app override `hasGenericApiCallChildImpl()` to
return `False` — `RestClientDataProviderBase` does this for the GenericRest
app, so its canonical `/call` child is not shadowed by a duplicate `__call__`.

Overriding `hasGenericApiCallChildImpl()` rather than
`getRestClientForGenericCallImpl()` keeps `getRestClientForGenericCall()`
truthful for any other caller: the opt-out is expressed as an opt-out, not by
hiding the provider's REST client.

### Auto-registered `make-api-call` action

`DataProviderActionCatalog::registerApp()` is modified to also register a
`make-api-call` action whenever
`isRestScheme(app.scheme) && !app.disable_generic_api_call` — see
[Gating: positive REST detection](#gating-positive-rest-detection). The
action's full `DataProviderActionInfo` is produced by a builder closure
registered via `setGenericApiCallActionBuilder()`.

A small backlog mechanism handles module load order: if an app is
registered before the builder is set, the app is queued in
`pending_generic_api_call_apps` and processed when the builder is eventually
registered. The REST predicate is re-evaluated at drain time so apps whose
scheme verdict has since changed (e.g. ConnectionProvider was loaded after
the app) are handled correctly.

Reference: `qlib/DataProvider/DataProviderActionCatalog.qc`, search for
`registerGenericApiCallActionForApp` and `setGenericApiCallActionBuilder`.

### Gating: positive REST detection

The framework injects `make-api-call` iff the app's connection scheme is
REST-derived — i.e. the scheme's `ConnectionSchemeInfo.cls` inherits
`RestClient::RestConnection`. This is **positive detection**, not an opt-out
denylist: a future non-REST app (Cassandra, gRPC transport, etc.) that
forgets to set `disable_generic_api_call` does **not** get a broken action —
the predicate rejects it on the basis of its connection class hierarchy
alone.

The predicate is implemented in
`DataProviderActionCatalog::isRestScheme()`. Because the DataProvider
module cannot `%requires ConnectionProvider` (which would form a cycle —
ConnectionProvider already `%requires(reexport) DataProvider`), the
`ConnectionSchemeCache::getSchemeEx()` lookup is late-bound via a callback
hook (`setSchemeLookupCallback()`) that ConnectionProvider registers at its
own init time. From there, the predicate walks the connection class
hierarchy by *name* (`Class::getClassHierarchy()` → looking for
`"RestClient::RestConnection"`), so no RestClient symbol import is
required either.

The one explicit opt-out today is **GenericRest** (`scheme: "rest"`),
which is itself `RestConnection`-derived but ships its own canonical
`/call` action; auto-injecting `make-api-call` would duplicate it.
Future apps may set `disable_generic_api_call: True` for similar
semantic reasons (e.g. compliance-sensitive integrations where a
free-form API surface is undesirable). **Non-REST apps do not need this
flag** — they are filtered by the predicate.

The catalog's app-equality check (used to dedupe duplicate registrations
from related modules — e.g. `FtpClientDataProvider` + `FtpPoller`)
excludes `disable_generic_api_call` from comparison so one caller setting
the opt-out and another omitting it does not break duplicate-registration.

## Generic-call implementation (RestClientDataProvider module)

### `GenericApiCallProvider` class

A small data-provider class (single file:
`qlib/RestClientDataProvider/GenericApiCallProvider.qc`) that:

- Accepts any `RestClient::AbstractRestClient` in its constructor — sync or
  async, qlib or module-v8 alike.
- Exposes `supports_request: True` (DPAT_API).
- Overrides `getRequestTypeWithOptionsImpl()` and
  `getRequestTypeWithDataImpl()` to re-type the `body` field based on the
  current `body_type` selection (see [Dynamic body typing](#dynamic-body-typing)).
- Implements `doRequestImpl()` which: substitutes `{name}` placeholders in
  `path` from `path_vars`; appends `query_args`; serializes `body` according
  to `body_type` and sets the matching `Content-Type` header (with a
  conditional fallback to the existing `hdr` if the caller supplied one);
  invokes `rest.restDoRawRequest()` for pre-serialized body types and
  `rest.restDoRequest()` for `body_type=auto`; catches the response, applies
  `error_passthru` and `expected_status` semantics; and builds the lean
  response hash.

### Request type — `GenericApiCallRequestDataType`

| Field | Purpose | Type |
|-------|---------|------|
| `method` | HTTP method, with allowed-values dropdown | `string` (req) |
| `path` | URI path relative to the connection's base URL; may contain `{name}` placeholders | `string` (req) |
| `path_vars` | Substitutions for `{name}` placeholders, URL-encoded | `*hash<auto>` |
| `query_args` | Query string parameters | `*hash<auto>` |
| `body_type` | Serialization selector (auto/json/form/multipart/xml/yaml/text/binary) — drives both the dynamic body type and the Content-Type header | `string`, default `"auto"` |
| `body` | Request body — type reshapes per `body_type` (see below) | dynamic |
| `hdr` | Custom request headers; overrides the body-type-derived Content-Type | `*hash<auto>` |
| `expected_status` | Whitelisted response status codes; mismatches throw `REST-STATUS-ERROR` | `*list<int>` |
| `error_passthru` | If True, non-2xx responses return as data instead of throwing | `*softbool` |
| `include_debug` | If True, the response includes a `debug` sub-hash with request/timing/size info (sensitive headers redacted) | `*softbool` |

### Dynamic body typing

`body_type` is the dependency controller and `body` is the dependent. The
action option for `body_type` carries `has_dependents: True`,
`structural_determinate: True`, and `on_change: ("refetch",)`. The action
option for `body` carries `depends_on: ("body_type",)`. The action itself
carries `data_dependent_options: True`.

When the UI changes `body_type`, it re-issues
`PUT /api/latest/dataprovider/apps/<app>/actions/make-api-call/getOptions?context=ui`
with the new value, and `GenericApiCallProvider::getRequestTypeWithOptionsImpl()`
returns a type with `body` re-typed:

| `body_type` | `body` field type |
|-------------|-------------------|
| `auto` | `auto` (template / context picker) |
| `json` / `form` / `yaml` | `*hash<auto>` |
| `xml` / `text` | `*softstring` |
| `binary` | `*softbinary` |
| `multipart` | `FileDataType` (name + base64 content + mime_type) |

For `body_type=multipart`, the provider builds a `multipart/form-data`
envelope at request time using the boundary helper described in the
Anthropic file-upload reference implementation.

### Response type — `GenericApiCallResponseDataType`

Lean three-field shape:

| Field | Purpose |
|-------|---------|
| `status` | HTTP response status code |
| `headers` | Response headers, lowercased keys (the synthetic `status_message` pseudo-header is stripped) |
| `body` | Deserialized response body (RestClient handles JSON/XML/YAML/etc. based on `Content-Type`) |
| `debug` | Optional. Populated only when the request specified `include_debug: True`. Contains `request.method`, `request.url`, `request.headers` (sensitive values redacted), `duration_ms`, `request_size`, `response_size`. |

Deliberately omits the noisy fields that `RestClientCallResponseDataType`
includes (`request-headers` echoing bearer tokens, `response-headers-raw`
duplicating `response-headers`, several `NOTHING` placeholders). The legacy
type is retained for the standalone `GenericRest` `/call` action.

### Header redaction

`GenericApiCallProvider::RedactedRequestHeaders` lists the lowercased header
names whose values are replaced with `"***"` in any `debug.request.headers`
echo: `authorization`, `proxy-authorization`, `cookie`, `set-cookie`,
`x-api-key`, `x-auth-token`, `x-amz-security-token`, `x-csrf-token`. The
keys are preserved so callers can see WHICH sensitive headers were sent;
only the values are masked. Extend the constant if your deployment uses
additional custom-named secrets.

### Builder registration

`RestClientDataProvider.qm`'s init block registers:

1. `AbstractDataProvider::setGenericApiCallChildBuilder()` — closure that
   takes the rest client and returns `new GenericApiCallProvider(rest)` if
   the value is an `AbstractRestClient`, NOTHING otherwise.
2. `DataProviderActionCatalog::setGenericApiCallActionBuilder()` — closure
   that takes an `app` and returns a `DataProviderActionInfo` for
   `make-api-call`, with the option set sorted into Request / Body /
   Response groups and the dynamic-options metadata on `body_type` and
   `body`.

## Per-provider integration

### Qore qlib providers

Every REST-based provider in this codebase already holds its REST client in
a member named `rest` (universal convention — see
`AftershipDataProviderBase`, `LinearDataProviderBase`,
`OpenAiDataProviderCommon`, every `*DataProviderBase.qc` and
`*DataProviderCommon.qc`). Reflective discovery finds it automatically —
**no per-provider override is needed**. Because the member is declared on the
shared base, discovery matches every provider in the tree; the framework's
suppression rule (see
[Injection point](#injection-point-once-per-provider-tree-at-the-root))
narrows that back down to the tree root.

Providers that store the REST client under a different field name override
`getRestClientForGenericCallImpl()` to return it. Providers that genuinely
have no REST client (e.g., DB, FTP) opt out at the app level via
`disable_generic_api_call: True`; providers of an app that opts out also
override `hasGenericApiCallChildImpl()` to return `False` so the runtime tree
matches the catalog.

### module-v8 TypeScript apps

`TypeScriptActionAppDataProvider` (the root provider for every
TypeScript-defined app) holds an `AbstractConnection conn` rather than a
direct REST client. The reflective default discovers `conn` and resolves
the underlying REST client via `conn.get(False)`. **No per-app and no
framework-side override is required.**

`TypeScriptActionDataProviderBase` — the base for the per-action child
providers — holds a `conn` member too, so discovery matches those as well;
the root-only suppression rule is framework-side, so module-v8 needs no
change for the child to appear exactly once per app there either.

See `module-v8/design/generic-api-call-action.md` for the module-v8 side.

## UI form layout

Option ordering in the form is bucket-sorted by Qorus by required-ness then
preselected-ness. To keep `body_type` and `body` adjacent, the action builder
registers them in the same preselected-optional bucket (neither marked
`required` at the action level — `body_type` carries `default_value: "auto"`
so the form is pre-filled). Both fields also carry the same
`groups: ("Body",)` tag, surfacing a `Body` section in UIs that render
grouped sections.

Order in the registration hash (the Qorus `sort` field follows this order
within each bucket): method, path, body_type, body, path_vars, query_args,
hdr, expected_status, error_passthru, include_debug. Qorus assigns
sort=1 to the synthetic `qorus_app_connection` field, then sort=2..N to
preselected options, then sort=999999 to non-preselected options.

## Qorus UI `{type, value}` encoding boundary

The `{type, value}` wrapping seen in Qorus UI request/response payloads is a
Qorus UI transport convention, not a DataProvider API shape. qorus-ide uses
typed editor values (see its `TTypedValue` representation) so it can preserve
editor type, template-vs-literal intent, and widget selection.

The Qore DataProvider contract remains plain Qore values: providers,
`getRequestTypeWithOptionsImpl()`, `getRequestTypeWithDataImpl()`, and
`doRequestImpl()` should receive normal hashes/lists/scalars. When
`context=ui` is used, Qorus owns both directions of the encoding boundary:
it UI-encodes option/field responses and must decode incoming `{type, value}`
payloads before invoking DataProvider APIs. This is documented in Qorus'
`design/ui-data-encoding.md`.

`GenericApiCallProvider::extractBodyType()` currently accepts both plain
`body_type` values and leaked `{value: ...}` values as a narrow compatibility
guard for existing callers. It should not become the pattern for new providers,
and Qore should not add a generic `unwrapTypedValue()` helper to the
DataProvider module; that would mix a Qorus UI transport concern into reusable
qlib provider code.

## Testing

Integration tests at
`examples/test/qlib/RestClientDataProvider/GenericApiCallAutoInjection.qtest`
cover:

- Framework hooks registered after module load
- Auto-registration on REST-capable apps and skip for opted-out / no-scheme apps
- `__call__` child resolves and dispatches through the underlying REST client
- `__call__` is injected only at the tree root: children and grandchildren of a
  REST-capable provider do not expose it, and neither does a path-resolved
  descendant
- `__call__` does not nest inside itself, whether framework-built or
  constructed directly
- resolving the reserved name never reaches `getChildProviderImpl()`, whether or
  not the provider exposes the child
- `disable_generic_api_call` suppresses the child as well as the action, while
  `getRestClientForGenericCall()` keeps reporting the client
- a REST subtree under a non-REST parent gets its own `__call__` at the subtree
  root
- Dynamic body retyping for all 8 body_type values
- Per-body-type Content-Type and serialization (json / form / text / multipart)
- `path_vars` substitution
- `expected_status` whitelist (accept + reject)
- `error_passthru` (4xx returned as data)
- Lean response shape (no legacy request-* fields, no duplicates)
- `include_debug` populates redacted debug info

Per-provider integration is covered by spot-checks in the qtest plus the
existing per-app test suites (each app's `__call__` child resolves through
the framework's reflective discovery — no per-app test code needed).

## Open issues

- **Qorus-side `context=ui` decode before DataProvider calls.** Qorus should
  decode incoming UI-encoded option and request payloads at the REST/API
  boundary, before invoking DataProvider dynamic-option resolution or action
  execution. Once that boundary is consistently enforced,
  `GenericApiCallProvider::extractBodyType()` can drop its compatibility
  handling for leaked `{value: ...}` wrappers.
- **Pagination wrapper.** Common pattern across most REST APIs; would
  warrant a `make-api-call-paginated` variant with strategy
  (page-number / cursor / next-link / Link-header).
- **Streaming responses.** Server-Sent Events, large file downloads, and
  LLM token streams need a different return shape (stream object instead
  of a body hash).
- **Per-call timeout override.** Currently inherits from the connection;
  some endpoints need their own.
- **Per-call OAuth2 scope override.** Some endpoints need additional
  scopes beyond what the connection was granted.
- **Multi-file multipart.** Today only single-file uploads are supported
  via `body_type=multipart`; a `body_type=multipart-form` accepting a list
  of FileDataType + a form-fields hash would generalize.

## Cross-references

- [DataProvider development guide](data-provider-development-guide.md)
  — for the bigger DataProvider framework context.
- [Dynamic options pattern](data-provider-development-guide.md#dynamic-options)
  — for the `data_dependent_options` / `getRequestTypeWithOptionsImpl()`
  pattern used to drive the body-type-dependent body field.
- `qlib/RestClientDataProvider/GenericApiCallProvider.qc` —
  implementation
- `qlib/RestClientDataProvider/GenericApiCallRequestDataType.qc` —
  request schema
- `qlib/RestClientDataProvider/GenericApiCallResponseDataType.qc` —
  response schema
- `qlib/DataProvider/AbstractDataProvider.qc` — framework virtual +
  reflective discovery
- `qlib/DataProvider/DataProviderActionCatalog.qc` — auto-registration
  + opt-out + backlog
