# GraphQL Modules — Design

This document describes the architecture of Qore's GraphQL support: the `GraphQL` execution
engine, the `GraphQLClient` client mixin, the `GraphQLDataProvider` schema bridge, and the
`GraphQLHandler` server module (HTTP/WebSocket handler).

## Overview

| Module | Role | Dependencies |
|--------|------|--------------|
| `GraphQL` | transport-agnostic GraphQL execution engine | `qore` only |
| `GraphQLClient` | reusable GraphQL client protocol mixin for REST transports | `Mime` |
| `GraphQLDataProvider` | generates a schema and resolvers from a DataProvider | `GraphQL`, `DataProvider` |
| `GraphQLHandler` | exposes the engine over HTTP/WebSocket | `GraphQL`, `json`, `HttpServerUtil`, `WebSocketHandler` |

All public symbols live in the `GraphQLHandler` namespace. The `GraphQL` module contributes the
engine classes (and the `GRAPHQL-*` exception-code constants) to that namespace; both
`GraphQLDataProvider` and `GraphQLHandler` re-export `GraphQL`. The dependencies are deliberately
disjoint: `GraphQLHandler` does **not** depend on `DataProvider`, and `GraphQLDataProvider` does
**not** depend on the HTTP/WebSocket stack — so an HTTP GraphQL service with hand-written resolvers
pulls in no DataProvider machinery, and a DataProvider-backed executor can be consumed over any
transport (or none). A service that wants both `%requires` both modules and composes them:
`GraphQLHandler gh(DataProviderGraphQLSchema::createExecutor(provider))`.

## GraphQLClient (client side)

`GraphQLClientBase` is a mixin inherited *alongside* a transport class (`RestClient` /
`RestClientIo`) via Qore multiple inheritance — e.g.
`class LinearRestClient inherits RestClient, GraphQLClientBase`. The mixin owns the GraphQL
*protocol* (request-body construction and inspection of the response `errors` array); the transport
class owns HTTP. This avoids forking the client class hierarchy and works for both the synchronous
and async (`Io`) variants.

A response containing a non-empty top-level `errors` array raises `GRAPHQL-ERROR` by default
(matching the documented contract); `setRaiseGraphQLErrors(False)` returns such responses to the
caller unchanged. `LinearRestClient` and `WaveRestClient` consume this mixin.

## GraphQL engine

The engine is transport-agnostic: it takes a document string (or parsed AST), variables, a resolver
map, and a context value, and returns the standard `{data, errors}` envelope. It has no HTTP or
DataProvider dependency and is independently usable and testable.

### Pipeline

```
source ──lexer──▶ tokens ──parser──▶ AST ──┐
SDL    ──lexer──▶ tokens ──SDL parser──▶ schema model
                                            │
document ─────────────▶ validator ─────────┤  (pre-execution)
                                            ▼
                          executor (+ introspection) ──▶ {data, errors}
```

### AST

AST nodes are typed `hashdecl`s with a base `GraphQLNode { string kind; }` and one derived hashdecl
per node type (`GraphQLField`, `GraphQLOperationDefinition`, value nodes, type-reference nodes,
etc.). Heterogeneous collections are typed `list<hash<GraphQLNode>>`; consumers switch on `kind` and
`cast<hash<GraphQLDerived>>(node)` to access derived members. This gives construction-time
validation and self-documenting node shapes while still allowing polymorphic collections. (A null
value uses the base `GraphQLNode` with kind `NK_NULL`, since Qore does not allow an empty hashdecl
body.)

### Parser

Both the executable-document parser and the SDL parser are pure-Qore recursive-descent parsers
sharing a `GraphQLParserBase` (token cursor; value, type-reference, argument, and directive
productions). The GraphQL grammar is small and stable, so this keeps the engine entirely in `qlib/`
with no build/packaging dependency. Parsed documents are small and resolution dominates execution
time, so parser throughput is not a bottleneck; the HTTP handler additionally caches parsed
documents.

### Schema model

`GraphQLSchema` parses SDL into `GraphQLTypeDef` / `GraphQLFieldDef` / `GraphQLInputValueDef`
records (object/interface/union/enum/input-object/scalar, plus `schema` root mappings), registers
the five built-in scalars, and validates that every referenced type exists and that a query root is
defined. Type references reuse the AST type-reference nodes (`GraphQLNamedType` / `GraphQLListType`
/ `GraphQLNonNullType`).

### Validator

`GraphQLValidator` runs before execution (toggleable via `GraphQLExecutor::setValidation()`) and
returns all errors together. It enforces operation-name uniqueness and the lone-anonymous rule;
fragment-name uniqueness, spread-target existence, cycle detection, unused-fragment detection, and
valid type conditions; variable uniqueness, input-type checks, defined-and-used, and
used-are-defined; field existence, leaf/composite selection correctness, valid/required/
non-duplicate arguments, and valid directive locations. It also enforces a query-complexity
(field-count) budget and a recursion-depth bound. (Field-merge-conflict detection is not
implemented.)

### Executor

A resolver map is keyed by type name, then field name, where each value is a closure/call reference
`auto sub (auto parent, hash<auto> args, auto context)`. When no resolver is registered, a default
resolver reads the field (by name) from the parent hash/object. Execution:

- coerces variables against their declared types and arguments against field-argument types;
- completes values against the declared type with GraphQL's **non-null error propagation**: a field
  error is recorded and, in a non-null position, propagates (via an internal `GRAPHQL-NULL-BUBBLE`
  signal) to the nearest nullable parent, nulling it; otherwise the offending field is nulled and
  execution continues. Argument coercion, resolver invocation, and value completion are all guarded
  so any error becomes a field error rather than aborting the operation.
- applies `@skip`/`@include`, expands fragments and inline fragments, and resolves abstract
  (interface/union) types via a `__typename` key in the resolved value;
- supports custom scalars via an optional coercer map (`{input, output}` closures per scalar name),
  mirroring GraphQL's `parseValue`/`serialize`; built-in `Int` is enforced as a signed 32-bit value.

### Introspection

The introspection meta-types (`__Schema`, `__Type`, `__Field`, `__InputValue`, `__EnumValue`,
`__Directive`, `__TypeKind`, `__DirectiveLocation`) are defined in SDL and merged into the executor's
working type map; their resolvers read the schema model lazily (types looked up by name on demand),
so recursive and mutually-recursive schemas introspect without infinite expansion. `__typename`,
`__schema`, and `__type` are handled as meta-fields.

### Subscriptions

`GraphQLExecutor::executeSubscription()` validates a subscription operation (exactly one root field)
and returns an opaque per-subscription state without calling the resolver.
`getSubscriptionStream()` calls the subscription field's resolver to obtain an event source (an
`AbstractIterator`), and `resolveSubscriptionEvent()` executes the selection set against one event.
Because Qore iterators are thread-bound, `getSubscriptionStream()` must be called from the thread
that consumes the events.

A plain iterator cannot be interrupted while blocked, so the engine provides
`GraphQLEventQueue` — a thread-safe, cancellable event source (`post()` publishes an event,
`complete()`/`cancel()` ends the stream) implementing the `GraphQLCancellableSource` interface. A
transport that holds a reference to the source can call `cancel()` from another thread to unblock a
subscription waiting for an event and end it cleanly.

## GraphQLHandler (server side)

### HTTP handler

`GraphQLHandler` is an `AbstractHttpRequestHandler` implementing the GraphQL-over-HTTP conventions:
`POST application/json` and `GET` (query in the query string). Field/validation errors return HTTP
200 with an `errors` array; only a malformed HTTP request (unparsable body, missing query) returns
400. The HTTP call context (`cx`) is passed to resolvers as the context value. The handler:

- caches parsed documents in an LRU keyed by query text;
- supports Automatic Persisted Queries (`extensions.persistedQuery.sha256Hash`);
- executes a JSON-array request body as a batch (one envelope per operation, bounded);
- restricts `GET` to query operations (mutations rejected) to avoid CSRF;
- converts any unexpected exception into an error envelope rather than leaking an HTTP 500.

### WebSocket handler

`GraphQLWsHandler` / `GraphQLWsConnection` implement the `graphql-transport-ws` protocol on
`WebSocketHandler`: `connection_init`/`connection_ack`, `subscribe`→`next`*→`complete`, `complete`,
`ping`/`pong`, and `error`. Query and mutation operations sent over `subscribe` execute once (a
single `next` then `complete`); a subscription streams a `next` per event from a background thread
(the event-source iterator is created in that thread). The handler enforces the protocol lifecycle
(any message before `connection_init` is rejected and the connection closed; a duplicate
`connection_init` and a duplicate subscription `id` are likewise rejected) and a per-connection
concurrent-subscription cap (`max_subscriptions`, default 100). A subscription stops when its source
is exhausted, or when the client sends `complete` or disconnects — and if the source is a
`GraphQLCancellableSource` it is `cancel()`led so a stream blocked waiting for an event exits
promptly. Backpressure is inherent: `WebSocketConnection::send()` blocks when the send buffer fills,
throttling a fast producer.

## DataProvider bridge (GraphQLDataProvider)

`DataProviderGraphQLSchema` (in the `GraphQLDataProvider` module) generates a GraphQL schema and
resolvers from an `AbstractDataProvider`, mirroring (in reverse) the type↔schema mapping done by
`Swagger`/`OpenApi3`/`RestSchemaValidator`. It produces a transport-agnostic `GraphQLExecutor`
(`getExecutor()` / the `createExecutor()` static); exposing that over HTTP is the caller's job (pass
it to a `GraphQLHandler`), which is why the bridge has no HTTP/WebSocket dependency.

**Type mapping** recurses the DataProvider type tree: scalar base types map to GraphQL scalars;
dates to a `DateTime` custom scalar (ISO-8601 via the executor coercer hook), binary to `Base64`,
free-form hashes to `JSON`; list types to GraphQL lists; object/hash types with declared fields to
generated nested GraphQL object types. Recursion is bounded (`max_type_depth`) with a `JSON`
fallback for unbounded/recursive types. (Note: "soft" Qore types report `isList()` true but have no
element type, so a real list is detected by the presence of an element type.)

GraphQL forbids using an output object type in input position, so a non-scalar field appearing in a
mutation argument (`create`/`update`-`set`/`upsert`, the record `Input`) gets a *parallel* `input`
type generated alongside its output `type` (recursively, including lists of objects). On input these
sanitized nested field names round-trip back to the provider's native names structurally — driven by
the provider record type, not a precomputed map — so arbitrarily deep nested objects and
lists-of-objects map back correctly.

**Capabilities → operations**, gated on `DataProviderInfo` flags:
- `supports_read` → a `search` query field;
- `supports_create` → `create`; `supports_update` → `update`; `supports_delete` → `delete`;
  `supports_upsert` → `upsert`; `supports_bulk_create`/`supports_bulk_upsert` → `createMany`/
  `upsertMany`;
- direct child providers (`getChildProviderNames()`) → a read-only `<child>` Query field, plus
  prefixed `<child>Create`/`<child>Update`/`<child>Delete`/`<child>Upsert` mutations gated on the
  *child's* own capability flags. Child `update`/`delete` use the raw DPQL `where` selector (not a
  typed filter) so per-provider operator gating stays unambiguous; each child resolver re-fetches
  the child provider and builds a fresh spec at call time (the child may be request-scoped).

**Filtering** uses DPQL (Data Provider Query Language) as the where-condition mechanism, since the
DataProvider layer consumes DPQL natively. Two forms are offered and combined (AND-ed): a raw `where`
DPQL string, and a typed `filter` input (`<Record>WhereInput` with per-scalar `*Filter` types and
recursive `_and`/`_or`/`_not`) that is compiled to DPQL — so DPQL remains the single canonical
intermediate representation. Per-scalar filters exist for `Int`/`Float`/`String`/`Boolean` and for
dates via a `DateTimeFilter` (ordered comparisons over the `DateTime` custom scalar; the ISO-8601
input is coerced to a Qore date and rendered as a native unquoted DPQL date literal). Filter
operators are gated on the provider's advertised `info.expressions`. `update`/`delete` require a selector (the mass-mutation guard) and return the
affected-row count, or the affected records when `return_records` is set (wrapped in a transaction
when the provider requires transaction management). `search` pushes `limit`/`offset` into the
provider's search options when advertised, else pages client-side.

GraphQL field/argument names are sanitized to valid GraphQL identifiers; the bridge round-trips
them back to the provider's native field names on input (create/update/upsert) and via output field
resolvers on read. Distinct provider fields that sanitize to the same GraphQL name are rejected.

## Security model

- DPQL filters are validated against the record's field schema and the provider's supported
  operators; **template references are rejected by default** (an `allow_templates` opt-out exists),
  and the engine never registers permissive template callbacks for client input.
- HTTP `GET` executes query operations only.
- Query depth and complexity are bounded; input-object coercion rejects undefined fields; the `Int`
  scalar is range-checked.

## Testing

- `examples/test/qlib/GraphQL/GraphQL.qtest` — the engine alone (requires only `GraphQL`, proving no
  hidden HTTP/DataProvider dependency).
- `examples/test/qlib/GraphQLHandler/GraphQLHandler.qtest` — lexer, parser, schema, validation,
  execution, introspection, the HTTP handler (POST/GET/errors/APQ/batching), and WebSocket
  subscriptions (requires only `GraphQLHandler`, proving no hidden DataProvider dependency).
- `examples/test/qlib/GraphQLDataProvider/GraphQLDataProvider.qtest` — the DataProvider bridge (type
  graph, child providers, filters, CRUD, bulk, return-records, paging, transactions); requires only
  `GraphQLDataProvider`, proving no hidden HTTP dependency.
- `examples/test/qlib/GraphQLClient/GraphQLClient.qtest` — the client mixin.

## Build note

The `GraphQL` engine module has no third-party build dependency. `GraphQLHandler` requires the
external `json` module; it is the first qlib module to do so, which surfaced a latent issue in the
qmod dependency finalizer in `cmake/QoreMacros.cmake` (it wired a build-order edge to any CMake
target matching a `%requires` name, colliding with nghttp2's bundled `json` FetchContent target).
The finalizer now only wires edges to targets in the source tree (excluding `/_deps/`).
