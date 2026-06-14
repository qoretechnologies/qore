# GraphQL Client and Server Modules — Implementation Plan

This document is the implementation plan for two related deliverables:

1. **`GraphQLClient`** — a reusable GraphQL client base/mixin consumed by `LinearRestClient`,
   `WaveRestClient`, and future GraphQL API wrappers (client side).
2. **`GraphQLHandler`** — a full-spec GraphQL execution engine exposed through Qore's
   `HttpServer`, with a **DataProvider bridge** that auto-generates a GraphQL schema and
   resolvers from registered Qore data providers (server side).

These are independent efforts and are sequenced accordingly: deliverable 1 is a contained,
test-validated refactor; deliverable 2 is a multi-phase subproject built on top of a new
GraphQL execution engine.

## Status

- **Implemented** on branch `feature/graphql`.
  - **Deliverable 1 (`GraphQLClient`)**: complete — mixin + Linear/Wave refactor + tests; committed.
  - **Deliverable 2 (`GraphQLHandler`)**: complete — pure-Qore engine (lexer, parser, SDL/schema,
    validation, execution with non-null error propagation, `@skip`/`@include`, introspection),
    HTTP handler (POST/GET, document cache), and DataProvider bridge; full `.qtest` (15 cases),
    example program, docs.

### Implementation notes / decisions made during the build

- **Single module, not two.** The engine, HTTP handler, and DataProvider bridge ship in one
  `GraphQLHandler` module rather than a separate `GraphQL` engine module. The engine classes
  remain HTTP-free (transport-agnostic per D3), so a future split is still possible; it was not
  worth the extra registration/doc/test surface now. Consequence: the module depends on `json`
  and `DataProvider` even though the bare engine does not.
- **AST = base hashdecl + derived nodes** (`GraphQLNode` with a `kind` discriminator; derived
  node hashdecls). Heterogeneous collections are `list<hash<GraphQLNode>>`; consumers `cast<>`
  to the derived type after switching on `kind`. (Qore reserved words forced renames: no
  `context`/`in`/`sub` identifiers.)
- **OQ1 resolved**: the client raises `GRAPHQL-ERROR` by default with a `graphql_raise_errors`
  opt-out (matches the previously-unimplemented doc contract).
- **OQ4 (bridge update/delete) — resolved: use DPQL; Phase A implemented**: where-conditions are
  exposed as a DPQL string argument forwarded to the DataProvider's native DPQL overloads. The
  bridge now generates a `where`-filtered `search` plus `update`/`delete`/`upsert` mutations
  (gated on `supports_search_expressions` + the per-op capability flags), with DPQL validation
  against the record schema, template-reference rejection by default, and GraphQL→provider
  field-name round-tripping (which also fixed a latent `create` bug). See
  [§ DPQL integration & closing update/delete](#dpql-integration--closing-updatedelete-resolves-oq4)
  below; Phase B/C remain optional follow-ups.
- **Build-system fix** (`cmake/QoreMacros.cmake`): the qmod dependency finalizer wired a
  build-order edge to *any* CMake target whose name matched a `%requires` dependency, including
  third-party FetchContent targets. The external `json` qore module collided with nghttp2's
  bundled `json` target (whose doc build is broken), breaking the `GraphQLHandler` qmod build.
  Fixed by only wiring edges to targets in the source tree (excluding `/_deps/`), which is the
  first time a qlib module has `%requires json`.

## Background and motivation

### Client side (real, mechanical duplication)

`LinearRestClient.qm` and `WaveRestClient.qm` both wrap a GraphQL-over-HTTP API on top of
`RestClient`/`RestClientIo`. Their GraphQL methods are byte-for-byte identical and are
copied **four times** total (sync + `Io` variant, in each of the two modules):

```qore
hash<auto> graphql(string query, *hash<auto> variables, *reference<hash<auto>> info) {
    hash<auto> body = {"query": query, "variables": variables};
    return post("", body, \info);
}
hash<auto> query(string q, *hash<auto> v, *reference<hash<auto>> i)  { return graphql(q, v, \i); }
hash<auto> mutate(string m, *hash<auto> v, *reference<hash<auto>> i) { return graphql(m, v, \i); }
```

Reference sites:
- `qlib/LinearRestClient.qm:212` (`graphql`), `:231` (`query`), `:245` (`mutate`), `:452` (`Io::graphql`)
- `qlib/WaveRestClient.qm:223` (`graphql`), `:242` (`query`), `:256` (`mutate`), `:465` (`Io::graphql`)

The DataProvider layers funnel everything through a single per-module `doGraphQL()` wrapper,
so the consumer surface is small and well-defined:
- `qlib/LinearDataProvider/LinearDataProviderBase.qc:140` — `rest.graphql(query, variables)`
- `qlib/WaveDataProvider/WaveDataProviderBase.qc:140` — `rest.graphql(query, variables)`
- `qlib/WaveDataProvider/WaveCustomersDataProvider.qc:141` — direct `rest.graphql(...)` in a
  pagination loop

**Correctness gap that argues for consolidation:** both modules' doc comments promise
`@throw GRAPHQL-ERROR if the GraphQL response contains errors`
(`qlib/LinearRestClient.qm:210`, `qlib/WaveRestClient.qm:221`), but **neither inspects the
`errors` array**. GraphQL returns HTTP 200 with a top-level `errors` array on failure, so both
clients currently hand error responses back to callers as if they succeeded. This logic must
live in exactly one place.

### Server side (no prior art; new capability)

Qore has no GraphQL server capability today. A `GraphQLHandler` slots into the existing
`AbstractHttpRequestHandler` framework (`qlib/HttpServerUtil.qm:2448`), but the HTTP plumbing
is ~10% of the work — the other 90% is a GraphQL execution engine (parser, type system,
validation, execution, introspection). The DataProvider bridge then sits on top of that engine
as one resolver *source*, mirroring (in reverse) the existing schema↔DataProvider mapping done
by `RestSchemaDataProvider`/`Swagger`/`OpenApi3`.

## Architectural decisions (settled)

### D1 — Parser: pure-Qore recursive descent

The GraphQL grammar (executable documents + SDL) is small and stable. A pure-Qore
recursive-descent parser keeps the entire engine in `qlib/` with **no build/packaging changes**
and avoids coupling a server module to a binary dependency.

AOT compilation (`qcc`, AST→IR→LLVM→native) removes interpreter dispatch overhead, so parser
control flow runs native. AOT does **not** eliminate refcounting, per-node heap allocation, or
encoding-aware string access — but for realistic GraphQL workloads this does not matter:
documents are small (hundreds of bytes to a few KB), resolution dominates parse time, and
parsed documents are cached (see D4). tree-sitter remains an escape hatch only if profiling
ever shows a genuine parse-bound workload.

Mitigations baked in from the start (cheap insurance):
- strict-typed `hashdecl` AST/token nodes (static layout, no `auto` boxing), **not** `hash<auto>`
- offset-based tokenizer: track `(start, len)` into the source, slice once — no per-char substrings
- byte-oriented scanning of the (UTF-8) source in the tokenizer hot loop

### D2 — Client shape: mixin base class, not a new client hierarchy

The existing clients already inherit `RestClient`/`RestClientIo`. Qore multiple inheritance
lets a shared `GraphQLClientBase` be mixed in alongside the transport class
(`class LinearRestClient inherits RestClient, GraphQLClientBase`). The base owns the GraphQL
*protocol* (body construction, `errors`→exception handling); the transport class owns HTTP.
This avoids forking the client class tree and works for both sync and `Io` variants.

### D3 — Engine is transport-agnostic

The execution engine takes a parsed/validated document + variables + a resolver interface and
returns the `{data, errors}` envelope. It does not know about HTTP. `GraphQLHandler` is a thin
adapter from `handleRequest()` to the engine; the engine is independently unit-testable and
could later back a non-HTTP transport.

### D4 — Document caching keyed by query hash

The handler caches parsed+validated documents keyed by a hash of the query text (the standard
"automatic persisted queries" pattern). This removes repeat parsing/validation from the hot
path and makes parser performance a non-issue for steady-state operation.

### D5 — DataProvider bridge maps capability flags → operations

The bridge reads `DataProviderInfo` capability flags and record types to generate schema:
- record type fields → GraphQL object type fields
- `supports_read`/search → `Query` fields (search options → GraphQL arguments)
- `supports_create`/`supports_update`/`supports_delete` → `Mutation` fields
- `supports_request` (`doRequest`) → `Query` or `Mutation` field depending on declared side effects
- child providers (`getChildProviderNames()`) → nested types / namespaced fields

This mirrors the inverse mapping already implemented in `Swagger.qm:getTypeIntern()`
(`qlib/Swagger.qm:1182`) and `RestSchemaValidator.qm:getDataTypeIntern()`
(`qlib/RestSchemaValidator.qm:1236`).

## Open questions (resolve during the relevant phase)

- **OQ1 (Phase 1):** Should `errors`→`GRAPHQL-ERROR` throwing be the default, or opt-in via a
  client option? Default-throw is the least-surprising behavior and matches the existing
  (unimplemented) doc contract, but it changes behavior for any current caller relying on the
  silent pass-through. Proposal: default-throw, with a `graphql_raise_errors` option (default
  `True`) to restore the old behavior. Decide after auditing the Linear/Wave DataProvider call
  sites for code that already inspects `errors` itself.
- **OQ2 (Phase 5):** GraphQL `subscription` support (long-lived, over WebSocket/SSE). Out of
  scope for the initial engine; the type model should reserve the operation type so it can be
  added without a schema-model rewrite.
- **OQ3 (Phase 6):** Custom scalar mapping policy for DataProvider types with no GraphQL
  primitive (e.g. Qore `date` → custom `DateTime` scalar vs ISO-8601 `String`).
- **OQ4 (Phase 6) — RESOLVED:** represent where-conditions as a **DPQL string** argument
  forwarded to the DataProvider's native DPQL overloads, rather than generating a bespoke GraphQL
  filter-input grammar. Typed introspectable filters (compiling to DPQL) remain an optional later
  enhancement. See [§ DPQL integration & closing update/delete](#dpql-integration--closing-updatedelete-resolves-oq4).

---

# Deliverable 1 — `GraphQLClient` module

A small module providing the GraphQL protocol layer, plus a refactor of the two existing
consumers onto it.

### Phase 1.1 — Create `qlib/GraphQLClient.qm`

New module with these public members:

- `class GraphQLClientBase` — the mixin. Provides:
  - `hash<auto> graphqlExecute(code poster, string query, *hash<auto> variables, *reference<hash<auto>> info)`
    — builds the `{query, variables}` body, calls the supplied `poster` closure (so the base is
    transport-agnostic: sync `post()` vs `Io::restPost()`), inspects the response for a
    top-level `errors` array, and throws `GRAPHQL-ERROR` (with the `errors` payload as exception
    arg) when present and `graphql_raise_errors` is set.
  - thin `query()`/`mutate()` semantics documented as aliases of the execute path.
- `const GRAPHQL_ERROR = "GRAPHQL-ERROR";` and documented exception shape.
- `hashdecl GraphQLResponseInfo { *hash<auto> data; *list<hash<auto>> errors; *hash<auto> extensions; }`
  for typed access to the envelope.

Module metadata mirrors the existing clients (`version = "1.0"`, MIT, Qore Technologies),
`%requires Mime`, `RestClient`, `RestClientIo` only if needed by helper signatures — keep
`requires` minimal; the base should not force a transport dependency on consumers.

### Phase 1.2 — Refactor `LinearRestClient` / `WaveRestClient`

- Add `GraphQLClientBase` to the inheritance list of `LinearRestClient`, `LinearRestClientIo`,
  `WaveRestClient`, `WaveRestClientIo`.
- Replace the four duplicated method bodies with delegations to `graphqlExecute()`, passing the
  appropriate `post`/`restPost` closure.
- Add `%requires GraphQLClient` to both modules.
- Remove the now-shared boilerplate; keep module-specific URL/auth/ping config untouched
  (`DefaultUrl`, `getOptions()`, connection classes).
- Wire the `errors`→exception behavior (OQ1) and confirm the Linear/Wave DataProvider
  `doGraphQL()` wrappers still behave correctly (they currently assume success-shaped
  responses; centralized throwing actually *improves* them).

### Phase 1.3 — Build registration

- Add `qore_user_module("qlib/GraphQLClient.qm")` to `CMakeLists.txt` near the other client
  modules (cf. `CMakeLists.txt:3187`).
- Add the equivalent entry to `Makefile.am` (required for docs + install per repo conventions).

### Phase 1.4 — Tests

New `examples/test/qlib/GraphQLClient/GraphQLClient.qtest`:
- Unit: body construction (`{query, variables}` shape), `query()`/`mutate()` delegation.
- Negative: response with `errors` array → `GRAPHQL-ERROR` thrown; `graphql_raise_errors=False`
  → errors returned in the envelope, no throw.
- Corner: `errors` present *and* partial `data` present (GraphQL allows both) — verify the
  exception carries both.
- Use a local in-process `HttpServer` with a stub handler returning canned GraphQL envelopes
  (no external network); follow the `RestHandler.qtest:413` server-setup pattern.
- Re-run existing `LinearDataProvider.qtest` and `WaveDataProvider.qtest` (their non-integration
  test groups run without `LINEAR_TOKEN`/`WAVE_TOKEN`) to confirm the refactor is behavior-
  preserving.

### Phase 1.5 — Validation

- `qore --enable-debug` for all new/changed tests.
- No C++ changes → no valgrind required.
- Docs build (`cmake --build build --target docs`) must be warning-free for the new module.

**Deliverable 1 is complete and shippable on its own.** Deliverable 2 does not depend on it
beyond shared `GRAPHQL-ERROR` naming conventions.

---

# Deliverable 2 — `GraphQLHandler` (engine + DataProvider bridge)

Built bottom-up: language → engine → HTTP → bridge. Each phase has its own conformance tests
and must pass before the next begins.

### Phase 2.1 — Lexer + AST model

- `hashdecl` token and AST node types (per D1: strict-typed, offset-based).
- Tokenizer for GraphQL executable documents and SDL: names, ints/floats, strings (incl. block
  strings `"""`), punctuators, comments, commas-as-whitespace.
- Define the AST node `hashdecl`s for: `Document`, `OperationDefinition`
  (query/mutation/subscription), `FragmentDefinition`, `SelectionSet`, `Field`, `Argument`,
  `Directive`, `VariableDefinition`, `FragmentSpread`, `InlineFragment`, value nodes
  (scalars, lists, objects, enums, variables, null).
- Tests: tokenizer corpus (valid + malformed), exact span/offset assertions.

### Phase 2.2 — Parser (executable documents)

- Recursive-descent parser producing the Phase 2.1 AST from a query/mutation document.
- Variables, aliases, fragments (named + inline), directives, default values.
- Precise error reporting with line/column derived from token offsets.
- Tests: parse the GraphQL spec example documents; negative tests with positional error
  assertions.

### Phase 2.3 — SDL parser + schema model

- Parser for the Schema Definition Language: `type`, `interface`, `union`, `enum`, `input`,
  `scalar`, `schema`, directives, field args, non-null (`!`) and list (`[]`) wrappers,
  descriptions.
- In-memory schema model (`hashdecl`s) with name resolution over the type graph and the
  built-in scalars (`Int`, `Float`, `String`, `Boolean`, `ID`).
- Tests: round-trip SDL → model → introspection consistency (see 2.6).

### Phase 2.4 — Validation

- Implement the spec validation rules needed for a correct server: operation/field existence,
  argument types, variable usage/coercion, fragment type conditions, no unused
  variables/fragments, leaf/selection-set correctness.
- Tests: spec validation examples, one negative test per rule.

### Phase 2.5 — Execution engine

- Resolver interface (Qore `code`/abstract class): `resolve(field, parent, args, context)`.
- Field execution: selection-set traversal, argument coercion against schema, value completion
  (non-null, list, object), alias handling, fragment inlining, error collection into the
  `errors` array with `path` (partial `data` + `errors` per spec), `extensions` pass-through.
- `@skip`/`@include` directive handling.
- Transport-agnostic (D3): returns `hash<GraphQLResponseInfo>` (reuse the Deliverable 1
  hashdecl if available, else define locally).
- Tests: end-to-end execution against a hand-written in-memory schema + resolver map covering
  nesting, lists, aliases, fragments, partial errors.

### Phase 2.6 — Introspection

- Implement `__schema`, `__type`, `__typename` and the `__Type`/`__Field`/`__InputValue`/
  `__EnumValue`/`__Directive` meta-types so GraphiQL/Apollo tooling and codegen work.
- Tests: introspection query returns a structure that re-derives the input SDL; verify against
  the canonical introspection query used by client tooling.

### Phase 2.7 — `qlib/GraphQLHandler.qm` (HTTP adapter)

- `class GraphQLHandler inherits HttpServer::AbstractHttpRequestHandler` implementing
  `handleRequest(cx, hdr, *body)` (`qlib/HttpServerUtil.qm:2921`), following the
  `RestHandler.handleRequest` pattern (`qlib/RestHandler.qm:1473`).
- Accept `POST` `application/json` (`{query, operationName, variables, extensions}`) and `GET`
  (query in query-string) per the GraphQL-over-HTTP spec.
- Document cache keyed by query hash (D4); on miss: parse → validate → cache.
- Build `HttpResponseInfo` (`qlib/HttpServerUtil.qm:386`) with `code` 200 and the JSON envelope;
  use `AbstractHttpRequestHandler::makeResponse()` for serialization/encoding.
- Map engine/validation failures to the correct HTTP + GraphQL error shapes (validation errors
  are HTTP 200 with `errors`; malformed request bodies are HTTP 400).
- Authentication via the standard `AbstractAuthenticator` constructor arg.
- Tests: register on an in-process `HttpServer` (`HttpServer.setHandler`,
  `qlib/HttpServer.qm:1804`; setup pattern at `RestHandler.qtest:413`); drive with `RestClient`;
  cover query/mutation/introspection/error/auth.

### Phase 2.8 — DataProvider bridge

- `class DataProviderGraphQLSchema` (or similar) that, given one or more registered
  `AbstractDataProvider`s, generates a GraphQL schema model + resolver map (D5):
  - `getRecordType()` → object type; walk fields via `getFields()` →
    `AbstractDataField::getName()/getType()/isMandatory()` (`qlib/DataProvider/AbstractDataField.qc:514`).
  - Qore-type → GraphQL-type mapping function modeled on `Swagger.qm:getTypeIntern()`
    (`qlib/Swagger.qm:1182`) and the `TypeCodeMap`/`DataTypeMap` in
    `qlib/DataProvider/AbstractDataProviderType.qc:49`. Custom scalar policy per OQ3.
  - capability flags from `getInfo(): hash<DataProviderInfo>`
    (`qlib/DataProvider/AbstractDataProvider.qc:2130`) → `Query`/`Mutation` fields per D5.
  - search options (`getSearchOptions()`) / create options → GraphQL field arguments (OQ4).
  - child providers (`getChildProviderNames()`) → nested fields/types.
  - resolvers translate GraphQL field invocations into `searchRecords`/`doRequest`/
    `createRecord`/`updateRecords`/`deleteRecords` calls.
- `GraphQLHandler` gains a constructor/factory that takes a `DataProviderGraphQLSchema` so a
  data provider tree is exposed over GraphQL with zero hand-written schema.
- Tests: build a synthetic in-memory DataProvider with a known record type + capability flags;
  assert generated SDL/introspection; execute query + mutation through the bridge and verify the
  underlying DataProvider methods are invoked with coerced arguments.

### Phase 2.9 — Build registration, docs, examples

- `qore_user_module("qlib/GraphQLHandler.qm")` in `CMakeLists.txt` (cf. `RestHandler` at
  `CMakeLists.txt:3147`) and the matching `Makefile.am` entry.
- Doxygen module docs with a worked example (define schema + resolvers; serve via `HttpServer`;
  and the DataProvider-bridge one-liner).
- `examples/` sample program exposing a DataProvider tree over GraphQL.

### Phase 2.10 — Full validation

- All engine + handler + bridge tests under `qore --enable-debug`.
- `./run_tests.sh -d <dir>` for the new test directories green with no warnings.
- Docs build warning-free.
- No C++ changes expected (pure-Qore engine) → no valgrind; if any C++ is touched, run valgrind
  on affected tests at the end.

## Testing strategy (both deliverables)

- Use `FsUtil` `TmpFile`/`TmpDir` for any on-disk fixtures; never hand-roll `/tmp` paths.
- Use `%prepend-module-path` to load the local development modules under test.
- In-process `HttpServer` for all client↔server tests; no external network dependency.
- Conformance: drive the engine with the GraphQL specification's own example documents and
  introspection query as the correctness oracle.
- Meaningful assertions only (no identity tests); include negative + corner cases per phase.

## Sequencing summary

| Order | Work | Depends on | Shippable alone |
|------:|------|-----------|-----------------|
| 1 | Deliverable 1 (`GraphQLClient` + Linear/Wave refactor) | — | Yes |
| 2 | Phases 2.1–2.6 (language + engine + introspection) | — | Engine library only |
| 3 | Phase 2.7 (`GraphQLHandler` HTTP adapter) | 2.1–2.6 | Yes (hand-written schemas) |
| 4 | Phase 2.8 (DataProvider bridge) | 2.7 | Yes |
| 5 | Phases 2.9–2.10 (docs, examples, validation) | 2.8 | — |

Deliverable 1 should land first as an immediately useful, low-risk cleanup. Deliverable 2 is a
distinct subproject and should not be folded into the same change set.

## Key repository references

| Concern | Location |
|---------|----------|
| Duplicated client GraphQL methods | `qlib/LinearRestClient.qm:212`, `qlib/WaveRestClient.qm:223` |
| `*Io` client variants | `qlib/LinearRestClient.qm:452`, `qlib/WaveRestClient.qm:465` |
| DataProvider consumer wrappers | `qlib/LinearDataProvider/LinearDataProviderBase.qc:140`, `qlib/WaveDataProvider/WaveDataProviderBase.qc:140` |
| HTTP handler base / request entrypoint | `qlib/HttpServerUtil.qm:2448`, `:2921` |
| `HttpResponseInfo` hashdecl | `qlib/HttpServerUtil.qm:386` |
| Example handler (parse body, dispatch, respond) | `qlib/RestHandler.qm:1473` |
| Handler registration with HttpServer | `qlib/HttpServer.qm:1804`; setup at `examples/test/qlib/RestHandler/RestHandler.qtest:413` |
| DataProvider info / capabilities | `qlib/DataProvider/AbstractDataProvider.qc:2130` |
| Record type / field introspection | `qlib/DataProvider/AbstractDataProvider.qc:6264`, `qlib/DataProvider/AbstractDataField.qc:514` |
| Type → external-schema prior art (inverse mapping) | `qlib/Swagger.qm:1182`, `qlib/RestSchemaValidator.qm:1236` |
| Qore type maps | `qlib/DataProvider/AbstractDataProviderType.qc:49` |
| Module build registration | `CMakeLists.txt:3147` (`RestHandler`), `:3187` (REST clients); `Makefile.am` |

---

# DPQL integration & closing update/delete (resolves OQ4)

This section is the detailed plan to add `update`, `delete`, and `upsert` to the DataProvider
bridge and to add filtering to `search`, using DPQL (Data Provider Query Language) as the
where-condition mechanism. It supersedes the original Phase 2.8 "future work" note for OQ4.

## Rationale

The DataProvider layer already speaks DPQL natively for every operation we need, so DPQL
collapses the hard part of OQ4 (a bespoke GraphQL filter grammar) into "accept and forward a
string". Verified API basis:

- `AbstractDataProvider::searchRecords(string dpql_where, *hash search_options)` — `AbstractDataProvider.qc:2827`
- `AbstractDataProvider::updateRecords(hash set, string dpql_where, *hash search_options): int` — `:2978`
- `AbstractDataProvider::deleteRecords(string dpql_where, *hash search_options): int` — `:3063`
- `AbstractDataProvider::upsertRecord(hash rec, *hash upsert_options): string` (returns id) — `:2437`
- `DataProvider::parseDpqlExpression(string): hash<DataProviderExpression>` — `DataProvider.qc:872`
- `DataProvider::validateDpqlExpression(string text, hash<string, AbstractDataField> fields, *hash expressions, *hash server_expressions): list<hash<DpqlDiagnostic>>` — `DataProvider.qc:1241`
  (defaults the operator set to `DataProviderGenericExpressions` when `expressions` is omitted)
- capability flags on `hash<DataProviderInfo>`: `supports_update` (`:201`), `supports_upsert`
  (`:204`), `supports_delete` (`:207`), `supports_native_search` (`:210`),
  `supports_search_expressions` (`:273`), and the supported-operator map `expressions` (`:1191`).

DPQL syntax/semantics: `design/dpql-syntax.md`; integration/security: `design/dpql-integration.md`.

## Generated schema (after this change)

For a provider whose record type generates object type `<R>` (e.g. `peopleRecord`):

```graphql
type Query {
  # `where` is an optional DPQL filter string, e.g. '@active == true && @age >= 18'
  search(where: String, limit: Int, offset: Int): [<R>!]!
}

type Mutation {
  create(<scalar field args...>): <R>          # if supports_create (existing)
  update(set: <R>SetInput!, where: String!): Int!   # if supports_update — returns affected count
  delete(where: String!): Int!                  # if supports_delete — returns affected count
  upsert(<scalar field args...>): String!       # if supports_upsert — returns the record id
}

# all record fields, all OPTIONAL (no "!"): the columns to assign in an update
input <R>SetInput { <field>: <Type> ... }
```

Design points:
- **`where` is `String!` (non-null) on `update`/`delete`** — the schema itself forbids an
  unconditional mass mutation. An intentional "all rows" operation uses a tautology filter
  (e.g. `@id != null`); a future `allRecords: Boolean` flag could make that explicit.
- **`where` is optional `String` on `search`** (no filter = return all, subject to `limit`).
- **Return shapes mirror the API exactly**: `update`/`delete` → `Int!` (affected count),
  `upsert` → `String!` (id). Returning the affected *records* instead would require a
  non-atomic follow-up `searchRecords` — deferred (see Phase C).
- **`<R>SetInput` fields are all optional** so a partial update assigns only the supplied columns.

## Resolver behavior

All resolvers close over the provider `p`, the record-type field map `fields`
(`getRecordType()`), and `info.expressions`.

- `search(where, limit, offset)`:
  `it = where.val() ? p.searchRecords(validateWhere(where), {}) : p.searchRecords();`
  then apply `offset`/`limit` paging as today (`doSearch`).
- `update(set, where)`: `validateWhere(where); return p.updateRecords(toProviderRec(set), where, {});`
- `delete(where)`: `validateWhere(where); return p.deleteRecords(where, {});`
- `upsert(args)`: `return p.upsertRecord(toProviderRec(args), {});`

`validateWhere(where)` returns the validated DPQL string (or throws — see Validation). Resolver
exceptions are already turned into GraphQL `errors` entries by the executor, so a bad filter
surfaces as a field error, not a crash.

## Validation & security

`validateWhere(string where)`:
1. `list<hash<DpqlDiagnostic>> diags = DataProvider::validateDpqlExpression(where, fields, info.expressions);`
   — checks field references against the record schema and operators against what the provider
   supports. If any diagnostic has error severity, throw `GRAPHQL-EXECUTION-ERROR` with the
   concatenated messages (and positions).
2. **Template-reference defense (default-deny):** parse with `parseDpqlExpression` and walk the
   resulting `hash<DataProviderExpression>` tree for template-reference nodes (`$context:value`);
   reject by default. This is defense-in-depth on top of the runtime default (template references
   throw `TEMPLATE-RESOLUTION-ERROR` unless an `expand` callback is registered via
   `setTemplateCallbacks` — `dpql-integration.md:588`). A bridge option `allow_templates`
   (default `False`) can lift the restriction for trusted deployments.
   - *Investigation item:* confirm the exact node shape used for template references in
     `hash<DataProviderExpression>` so the walk is precise (small task against `DataProviderExpressions.qc`).
3. **Do not register permissive template callbacks** for the client-facing path; document that
   operators exposing the bridge on untrusted input must keep template resolution disabled.
4. *Optional:* bound expression complexity (node count / nesting depth) to limit DoS via
   pathological filters.

## Field-name round-tripping (also fixes a latent create bug)

GraphQL field/arg names are sanitized to valid GraphQL identifiers (`sanitizeName()`), but the
DataProvider expects its **original** field names. The current `create` resolver passes the
GraphQL arg hash straight to `createRecord()`, which is wrong whenever a name was sanitized
(e.g. `first-name` → `first_name`). Fix as part of this work:

- During generation build a `gql_name -> provider_field_name` map.
- Add `toProviderRec(hash<auto> gqlArgs)` that rekeys to provider field names; use it in
  `create`, `update` (`set`), and `upsert` resolvers.
- The DPQL `where` string references the provider's **native** field names directly (the client
  writes raw DPQL); document this boundary. For records whose field names are already valid
  GraphQL identifiers (the common case) `gql_name == provider_field_name` and this is a no-op.

## Capability gating

| GraphQL operation | Generated when |
|---|---|
| `Query.search` (+ `where` arg) | `supports_read` |
| `Mutation.create` | `supports_create` |
| `Mutation.update` + `<R>SetInput` | `supports_update` |
| `Mutation.delete` | `supports_delete` |
| `Mutation.upsert` | `supports_upsert` |

If no mutation capabilities are present, no `Mutation` type is emitted (as today). If a provider
advertises neither read nor any mutation, keep the existing `_empty: Boolean` placeholder so the
schema still has a query root.

## Implementation steps (in `qlib/GraphQLHandler/GraphQLDataProviderBridge.qc`)

1. In `generate()`: add the `where` arg to the `search` field; conditionally emit `update`,
   `delete`, `upsert` mutation fields and the `<R>SetInput` input type per the gating table.
2. Build and store the `gql_name -> provider_field_name` map; add `toProviderRec()`.
3. Add `validateWhere()` (validation + template-reference defense).
4. Add the `update`/`delete`/`upsert` resolver closures; route `search` through the DPQL overload
   when `where` is supplied. Convert `create`/`upsert`/`update.set` args via `toProviderRec()`.
5. Add the `allow_templates` constructor option (default `False`).
6. Reuse the existing `mapScalarType()`/`mapFieldType()`; for `<R>SetInput` strip the non-null
   marker (all set fields optional).

## Test matrix (extend `examples/test/qlib/GraphQLHandler/GraphQLHandler.qtest`, `bridgeTest`)

- `search(where: "@active == true")` filters to the matching records.
- `search(where: "@age >= 18 && @name like \"A%\"")` exercises compound/operator filters.
- `update(set: {...}, where: "@id == 1")` → affected count; a follow-up `search` confirms the change.
- `delete(where: "@id == 2")` → affected count; follow-up `search` confirms removal.
- `upsert(...)` → returns an id; record present afterward.
- invalid filter: `search(where: "@nosuchfield == 1")` → field error from `validateDpqlExpression`.
- template-reference rejected by default: `where: "@x == $qore-expr:{1}"` → error.
- safety: `delete` with no `where` is a **schema-level** error (non-null arg) — assert the
  request is rejected without reaching the resolver.
- a read-only provider (no update/delete/upsert flags) emits none of those fields and no
  `Mutation` type.
- field-name round-trip: a provider with a field needing sanitization (e.g. `full-name`)
  round-trips correctly through `create`/`update`.

## Phasing

- **Phase A (this plan) — DONE:** DPQL-backed `search` filter + `update`/`delete`/`upsert` +
  `SetInput` + validation + template defense + field-name round-trip + capability gating + tests.
  Closes the functional gap with full filter power for a contained amount of code. Note: DPQL
  filtering and update/delete are gated on the provider advertising `supports_search_expressions`
  (and populating `info.expressions`); the generated resolvers delegate to the provider's
  `searchRecords`/`updateRecords`/`deleteRecords` DPQL overloads, so the provider's
  `updateRecordsImpl`/`deleteRecordsImpl` are responsible for applying the where-expression.
- **Phase B — DONE:** typed, introspectable GraphQL filter input types (`<R>WhereInput`,
  per-scalar `IntFilter`/`FloatFilter`/`StringFilter`/`BooleanFilter` with
  `eq`/`ne`/`gt`/`gte`/`lt`/`lte`/`in`/`nin`/`like`, and recursive `_and`/`_or`/`_not`) that
  **compile to DPQL** — DPQL stays the canonical IR. A typed `filter` argument is offered on
  `search`/`update`/`delete` alongside the raw `where` DPQL string (both optional, AND-combined).
  Because GraphQL cannot express "exactly one of filter/where is required", the Phase A
  schema-level non-null-`where` mass-mutation guard became a **runtime guard** (update/delete with
  no selector → error). Operator gating relies on `validateDpqlExpression` rejecting operators the
  provider does not support at runtime.
- **Phase C — DONE:** a `return_records` constructor option switches `update`/`delete` to return
  the affected records (read via a non-atomic follow-up search; for `delete` the matching records
  are captured before deletion) instead of the affected-row count; and `createMany` /
  `upsertMany` bulk mutations (with a shared `<R>Input` type) are generated when the provider
  advertises `supports_bulk_create` / `supports_bulk_upsert` (each also requires the
  corresponding single-record capability). Bulk impls receive the DataProvider columnar block.

## Effort

Phase A is comparable to the existing `create`+`search` work plus the validation/security helper
and the field-name map — a single contained change to `GraphQLDataProviderBridge.qc` and the
bridge test. Phase B is a separate subproject (filter-type generation + DPQL compilation + a
per-operator test matrix). Phase C is incremental.

---

# Gap-closure plan (Phases D–H)

This section is the detailed plan to close every documented limitation. Each gap (numbered as in
the review) maps to a phase below; phases are ordered by value and dependency and are independent
unless noted.

| Gap | Limitation | Phase |
|----:|------------|-------|
| 4 | non-scalar Qore types flattened to `String` | D |
| 5 | bridge maps one flat record, no child-provider traversal | D |
| 8 | typed-filter operators gated at runtime, not in the schema | D |
| 2 | execution-time validation, not a full pre-execution pass | E |
| 3 | depth bounded but no complexity/cost budget | E |
| 6 | client-side search paging (no limit/offset pushdown) | F |
| 9 | `doc_cache` is clear-on-full, not LRU | F |
| 10 | no persisted queries (APQ) / request batching | F |
| 7 | `return_records` is non-atomic | F |
| 11 | one module; `json` + `DataProvider` hard deps | G |
| 1 | no streaming subscriptions | H |

Verified API basis: `AbstractDataProviderType::getFields()` (`AbstractDataProviderType.qc:815`,
object fields), `getElementType()` (`:812`, list element), `isList()` (`:546`),
`getBaseTypeCode()` (`NT_DATE` etc.); `AbstractDataProvider::getChildProviderNames()`
(`AbstractDataProvider.qc:4269`) + `getChildProvider()` (`:4294`); `getSearchOptions()` (`:6399`);
`info.expressions` (`:1191`); transaction support `requiresTransactionManagement()` (`:4445`) +
`beginTransaction()`/`commit()`/`rollback()` (`:4458`+); `WebSocketHandler.qm` for subscriptions.

## Phase D — bridge type-system depth (gaps 4, 5, 8) — DONE

Implemented: D1 recursive type mapping (DateTime/Base64/JSON custom scalars, list types, nested
object types; soft-type list-detection fix; `max_type_depth` JSON fallback); D2 direct
child-provider traversal as read-only Query fields (`max_provider_depth`, default 1); D3
operator-gated filter input types from `info.expressions`. Also fixed a latent output bug:
record/nested fields whose GraphQL name was sanitized now resolve via the provider's native field
name (output field resolvers). Original plan retained below.



Turns the bridge from flat single-record CRUD into a relational graph. This is the largest
value-add and is one cohesive change to `GraphQLDataProviderBridge.qc`.

### D1 — recurse the record type into a GraphQL type graph (gap 4)
- Replace the flat `mapScalarType()` with a recursive `mapType(AbstractDataProviderType): typeRef`:
  - scalar base types → `Int`/`Float`/`Boolean`/`String`/`ID` as today
  - `NT_DATE` → a custom `DateTime` scalar serialized/parsed as ISO-8601 via the
    @ref GraphQLExecutor scalar-coercer hook (already implemented): register
    `{"DateTime": {"input": parse ISO-8601, "output": render ISO-8601}}` on the generated executor
  - `isList()` types → GraphQL list `[Inner]`, recursing on `getElementType()`
  - object/hash types with `getFields()` → a generated nested GraphQL object type
    `<Record>_<fieldPath>` (deduplicated by a structural key), recursing into its fields
  - binary → a `Base64` custom scalar (base64 in/out coercer)
- Emit the generated nested object types and custom scalar definitions into the SDL.
- The default field resolver already reads nested values from the parent hash, so nested objects
  resolve without extra resolver code; only the custom scalars need coercers.
- **Cycle/decision:** bound nested-type generation depth (records can be deeply/recursively
  typed); past a limit, fall back to a `JSON` custom scalar. Add an option `max_type_depth`.

### D2 — child-provider traversal (gap 5)
- For each name in `provider.getChildProviderNames()`, fetch `getChildProvider(name)`, generate its
  record object type (reusing D1), and add a field on the parent record type (or on `Query`) that
  resolves to the child provider's records.
- Resolver: navigate to the child provider and run a (filterable) search, mirroring the top-level
  `search`. Recurse to a bounded depth (`max_provider_depth`) to avoid unbounded expansion on
  providers with deep/cyclic child graphs; log what was truncated.
- **Decision:** child fields are read-only initially (search/read); child mutations are a later
  increment.

### D3 — schema-level operator gating (gap 8)
- When generating each scalar field's `*Filter` input type, emit only the operators the provider
  advertises in `info.expressions` (map DPQL operator names → filter fields: `==`→`eq`, `>`→`gt`,
  `like`→`like`, `in`→`in`, …). A provider that doesn't support `like` won't expose a `like` field.
- Keep the runtime `validateDpqlExpression` gate as defense-in-depth.

### Tests / effort
- New `bridgeTypeGraphTest`: a provider whose record has a date field, a nested record field, and a
  list field → assert the generated `DateTime`/nested-object/list SDL, round-trip an ISO-8601 date
  filter, query nested fields, and traverse a child provider. Assert operator gating (a provider
  advertising a reduced `expressions` set omits the ungated filter fields).
- Effort: large — the recursive type mapping + nested-type deduplication + child traversal is the
  bulk. Self-contained to the bridge + its test.

## Phase E — full validation pass + complexity budget (gaps 2, 3) — DONE

Implemented as `GraphQLValidator` (run by `executeDocument` before execution; returns all errors
together; toggleable via `GraphQLExecutor::setValidation()`): operation-name uniqueness +
lone-anonymous rule; fragment-name uniqueness, spread-target existence, global cycle detection,
unused-fragment detection, valid type conditions; variable uniqueness + input-type check +
defined-and-used + used-are-defined; field existence, leaf/composite selection correctness, valid
+ required + non-duplicate arguments, valid directive locations; and a configurable
query-complexity (field-count) budget plus a recursion-depth guard. Field-merge-conflict detection
is the one spec rule left out (most complex, lowest impact). Original plan retained below.



A dedicated pre-execution validation phase in a new `GraphQLValidator` class, run by
`GraphQLExecutor.executeDocument` before execution; on any error it returns the `errors` envelope
with all diagnostics and does not execute.

### E1 — spec validation rules (gap 2)
Implement the executable-document validation rules not already enforced inline:
- operation name uniqueness; lone-anonymous-operation; subscription single-root
- variable uniqueness, all variables defined-and-used, variable usages are type-compatible
- fragment name uniqueness, fragment spread targets exist, **no fragment cycles** (global, not the
  current per-path guard), fragment type conditions are valid object/interface/union types
- field existence on the type in scope, leaf vs selection-set correctness, argument names valid +
  no duplicate arguments, required arguments present
- directives valid at their location; `@skip`/`@include` argument types
- field-merging conflict detection (same response key must have compatible shapes)
Reuse the AST and schema model; emit `GraphQLError` entries with positions.

### E2 — complexity/cost budget (gap 3)
- During validation, compute a query cost (node count, with list-field multipliers) and reject past
  a configurable `max_complexity`; also a `max_aliases`/`max_fields` cap to stop fragment-diamond
  amplification. Configurable on the executor/handler.

### Tests / effort
- One negative test per rule; a fragment-cycle test; an amplification test that trips the budget.
- Effort: medium-large (the rule set is broad but mechanical); independent of Phase D.

## Phase F — performance, scale, and atomicity (gaps 6, 9, 10, 7) — DONE

Implemented: F1 search limit/offset pushdown into the provider's search options when it advertises
them (else client-side paging); F2 LRU `doc_cache` (and an LRU APQ cache); F3 Automatic Persisted
Queries (`extensions.persistedQuery.sha256Hash`, with NotFound/HashMismatch handling) plus request
batching (a JSON array body returns an array of envelopes, bounded by `MaxBatchSize`); F4
transactional `return_records` (update/delete wrap mutate + re-read in
`beginTransaction`/`commit`/`rollback` when `requiresTransactionManagement()`). Original plan below.



### F1 — search limit/offset pushdown (gap 6)
- Add the provider's `keys getSearchOptions()` to the resolver spec; in `execSearch`, if the
  provider advertises `limit`/`offset`, pass them in `search_options` and skip in-memory paging;
  otherwise keep the current client-side paging. Cap `collectMatching` (return-records) with a
  configurable maximum and `log()` truncation.

### F2 — LRU document cache (gap 9)
- Replace the clear-on-full `doc_cache` with a bounded LRU (track access order; evict the
  least-recently-used entry at the cap) in `GraphQLHandlerImpl.qc`.

### F3 — persisted queries (APQ) + batching (gap 10)
- Implement the Automatic Persisted Queries protocol: accept `extensions.persistedQuery.sha256Hash`;
  on a hash-only request, look up the cached document (return `PersistedQueryNotFound` if absent);
  on a hash+query request, validate the hash and cache. Reuses the existing cache keyed by hash.
- Accept a JSON array body as a batch of operations, executing each and returning an array of
  envelopes (bounded by a `max_batch` limit).

### F4 — atomic return_records (gap 7)
- When `return_records` is set **and** `provider.requiresTransactionManagement()` is true, wrap the
  mutate + re-read in `beginTransaction()` / `commit()` (rollback on error) so the returned records
  reflect this operation atomically; otherwise document the best-effort non-atomic behavior (as now).

### Tests / effort
- Pushdown test with a provider advertising limit/offset; LRU eviction test; APQ hit/miss/mismatch
  + batch test; transactional return-records test with a transaction-capable fixture.
- Effort: medium; each item is independent.

## Phase G — module split (gap 11)

- Extract a `GraphQL` module (lexer, AST, parser, schema, executor, introspection, value,
  validator) with no `json`/`HttpServer`/`DataProvider` dependency. `GraphQLHandler` then `%requires`
  `GraphQL` + `json` + `HttpServerUtil`; the DataProvider bridge moves to a `GraphQLDataProvider`
  module requiring `GraphQL` + `DataProvider`.
- Register all three in `CMakeLists.txt`; move tests; update docs/module-list/release-notes.
- **Decision:** do this only once the engine API is stable (after D/E), since the split freezes the
  public engine surface. Effort: medium (mechanical, but touches registration/docs/tests broadly).

## Phase H — streaming subscriptions (gap 1) — largest

- Add a subscription transport on top of `WebSocketHandler.qm` implementing the
  `graphql-transport-ws` protocol (connection_init/ack, subscribe, next, complete, error).
- Subscription resolvers return an event source (an `AbstractIterator`/async stream); the engine
  executes the selection set per emitted event and pushes a `next` message per result.
- Reuse the engine unchanged for per-event execution; the new work is the WS protocol handler and
  the subscription resolver contract.
- **Dependency:** WebSocket server support (see [[project_ws_server_async_io]] in the broader
  effort). Effort: large; sequence last.

## Sequencing summary

D (bridge graph) and E (validation) are independent and the highest value — do them first, in
either order. F is incremental refinement. G (module split) should follow D/E once the engine API
is stable. H (subscriptions) is the largest and depends on WS transport — do it last. Gap 7 is
closed within F4 only where the provider supports transactions; otherwise it remains a documented
best-effort behavior bounded by the DataProvider API.
