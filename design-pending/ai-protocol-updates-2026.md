# AI Protocol Updates — Implementation Plan (2026)

Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

> **STATUS: proposal.** Nothing here is implemented. Move each phase into `design/` only
> once it has landed and is tested.

Covers four workstreams, ordered by deadline pressure:

| # | Workstream | Driver | Repo |
|---|------------|--------|------|
| 0 | OpenAI Assistants → Responses/Conversations | **Hard deadline 2026-08-26** | qore |
| 1 | A2A 1.0.1 conformance | Spec drift | qore |
| 2 | A2A extensions + signed agent cards | Missing v1.0 features | qore |
| 3 | MCP Apps extension (`io.modelcontextprotocol/ui`) | Ecosystem parity | qore (+ qorus) |
| 4 | AG-UI binding for Qonsole | Client-facing streaming | qore + qorus |
| — | Skills over MCP | **Draft SEP — watch only** | — |

---

## Phase 0 — OpenAI Assistants API sunset (deadline: 2026-08-26)

### Problem

OpenAI deprecated the Assistants API on 2025-08-26 with a sunset of **2026-08-26**.
`qlib/OpenAiDataProvider/` exposes ~15 providers built on `assistants`/`threads`/`runs`
that stop working on that date:

```
OpenAiAssistantsDataProvider.qc        OpenAiThreadsDataProvider.qc      OpenAiRunsDataProvider.qc
OpenAiAssistantDataProvider.qc         OpenAiThreadCreateDataProvider.qc OpenAiRunDataProvider.qc
OpenAiAssistantCreateDataProvider.qc   OpenAiThreadGetDataProvider.qc    OpenAiRunCreateDataProvider.qc
OpenAiAssistantUpdateDataProvider.qc   OpenAiThreadUpdateDataProvider.qc OpenAiRunUpdateDataProvider.qc
OpenAiAssistantDeleteDataProvider.qc   OpenAiThreadDeleteDataProvider.qc OpenAiRunDeleteDataProvider.qc
                                       OpenAiThreadCreateRunDataProvider.qc  OpenAiRunSubmitToolOutputs.qc
                                       OpenAiThreadMessagesGetDataProvider.qc
```

Blast radius is **confined to `qlib/OpenAiDataProvider/`** — verified: no `.qc`/`.qm` outside
that directory references `AssistantApiHdr`, `threads/runs`, or `assistant_id`. `QoreLlmUtils`,
`QoreAgentStrategies`, `LlmBackend`, and `AzureOpenAiDataProvider` use `chat/completions` only,
which is **not** deprecated.

### What already exists

The Responses API is substantially implemented:

- `OpenAiModelCreateResponseDataProvider.qc` (2336 lines) — `POST responses`, with
  `previous_response_id`, `store`, and conversation references
- `OpenAiResponseGetDataProvider.qc` / `OpenAiResponseDeleteDataProvider.qc` — `GET`/`DELETE responses/{id}`
- `OpenAiResponseStreamDataProvider.qc` — streaming, tracks `conversation_id`
- `OpenAiResponseActionSession.qc` — local `conversation_context_store`
- registered at `OpenAiDataProvider.qc:52` as the `responses` child

### Migration mapping (per OpenAI's guide)

| Assistants API | Responses API replacement |
|---|---|
| Assistants | **Prompts** (versioned config: model, tools, instructions) |
| Threads | **Conversations** (`/v1/conversations`) |
| Messages | **Items** |
| Runs | **Responses** |
| Run steps | **Items** |
| Submit tool outputs | Response items |

### Work

**0.1 — Add the missing Responses-side surface** (new files under `qlib/OpenAiDataProvider/`)

| New file | Endpoint | Replaces |
|---|---|---|
| `OpenAiConversationsDataProvider.qc` | container | `threads` |
| `OpenAiConversationCreateDataProvider.qc` | `POST conversations` | `POST threads` |
| `OpenAiConversationGetDataProvider.qc` | `GET conversations/{id}` | `GET threads/{id}` |
| `OpenAiConversationUpdateDataProvider.qc` | `POST conversations/{id}` | `POST threads/{id}` |
| `OpenAiConversationDeleteDataProvider.qc` | `DELETE conversations/{id}` | `DELETE threads/{id}` |
| `OpenAiConversationItemsDataProvider.qc` | `GET/POST conversations/{id}/items` | `threads/{id}/messages` |
| `OpenAiConversationItemDeleteDataProvider.qc` | `DELETE conversations/{id}/items/{item_id}` | — |
| `OpenAiResponseInputItemsDataProvider.qc` | `GET responses/{id}/input_items` | run steps |
| `OpenAiResponseCancelDataProvider.qc` | `POST responses/{id}/cancel` | `runs/{id}/cancel` |
| `OpenAiPromptsDataProvider.qc` + CRUD children | `/v1/prompts` | `assistants` |

Register `conversations` and `prompts` in `DefaultChildMap` (`OpenAiDataProvider.qc:45`).

**0.2 — Reference-value plumbing.** `OpenAiDataProviderCommon.qc:276` `getReferenceAssistants()`
enumerates `GET assistants` to build `AllowedValueInfo` lists. Add `getReferencePrompts()` and
`getReferenceConversations()` alongside it; leave the assistant variant in place until 0.4.

**0.3 — Tool-output loop.** `OpenAiRunSubmitToolOutputs.qc` implements the
`requires_action` → `submit_tool_outputs` round trip. Under Responses, tool outputs are just
input items on the next `POST responses` with `previous_response_id`. Verify
`OpenAiModelCreateResponseDataProvider.qc` accepts `function_call_output` items; if not, add
them to the request type. `OpenAiResponseActionSession.qc` is the natural home for the loop —
it already holds conversation context.

**0.4 — Deprecate, do not delete.** Per the no-breaking-changes rule, keep every
`OpenAiAssistant*`/`OpenAiThread*`/`OpenAiRun*` provider registered but:

- add a `@deprecated` doxygen tag naming the replacement provider on each class
- emit a one-shot deprecation warning from `OpenAiDataProviderCommon` when `AssistantApiHdr`
  (`OpenAiDataProviderCommon.qc:165`) is used
- document the 2026-08-26 sunset in the module docs and release notes

They can be removed in a later major version once the sunset has passed.

**0.5 — Tests.** `examples/test/qlib/OpenAiDataProvider/OpenAiDataProvider.qtest` (968 lines).
Add coverage for the new conversation/prompt/item providers with a mock REST backend; add a
negative test asserting the deprecation warning fires for the assistant path. Existing
assistant tests stay green until removal.

### Risk

The Prompts object is the weakest part of the mapping — it is a config object, not a
stateful agent, so anything relying on assistant-side state (file attachments, per-assistant
vector stores) needs the caller to carry that state. Flag this in the migration docs rather
than trying to emulate it.

---

## Phase 1 — A2A 1.0.1 conformance (small, do first in the A2A stream)

Upstream is **v1.0.1 (2026-05-26)**; `A2aClient`/`A2aServerHandler` target v1.0 + v0.3 with
bidirectional translation in `A2aVersionTranslation.qc`.

**1.1 — `application/a2a+json` content type.** Spec fix #1753 makes it the preferred HTTP
binding media type. Eight hardcoded `MimeTypeJson` sites:

- `A2aClient/A2aClient.qc:528` (Accept, card fetch), `:811`, `:812`, `:906`
- `A2aServerHandler/A2aServerHandler.qc:246`, `:257`, `:288`, `:1187`

Add `public const MimeTypeA2aJson = "application/a2a+json";` to `A2aVersionTranslation.qc`
next to the version constants (`:29`). Client sends
`Accept: application/a2a+json, application/json` on v1.0; server content-negotiates and
echoes `application/a2a+json` when the client accepts it, falling back to `application/json`.
Do **not** send it on v0.3 — the v0.3 wire format is unchanged.

**1.2 — TaskStatus values.** Spec fix #1801 corrected `TaskState` values in examples and
replaced partial terminal-state lists with the complete set of four. Diff
`A2aStateMapV03toV10` (`A2aVersionTranslation.qc:38`) and `A2aStateMapV10toV03` (`:50`)
against the v1.0.1 schema; confirm all four terminal states (`COMPLETED`, `FAILED`,
`CANCELED`, `REJECTED`) round-trip and that any code branching on "is terminal" enumerates
all four.

**1.3 — Transcoding errors.** Spec fix #1627 changed transcoding-related error mappings.
Review the JSON-RPC error paths in `A2aServerHandler.qc` against the updated table.

**1.4 — Tests.** `examples/test/qlib/A2aClient/A2aVersionTranslation.qtest` (468 lines) is the
right home for 1.2. Add content-negotiation cases to
`examples/test/qlib/A2aServerHandler/A2aServerHandler.qtest` (1497 lines): client accepting
only `application/json`, only `application/a2a+json`, both, and neither.

---

## Phase 2 — A2A extensions + signed agent cards

Two v1.0 features that are entirely absent.

### 2.1 Protocol extensions

**Current state:** the agent-card hashdecls carry `*list<string> extensions` (`A2aClient.qc:307`,
`:328`), but the translation layer **deletes** them — `A2aVersionTranslation.qc:302` and `:364`
both do `delete result.extensions;`. The `X-A2A-Extensions` activation header appears nowhere
in the tree.

This is the mechanism AP2/x402 and every other A2A add-on rides on, so it gates any future
payments work.

**Work:**

1. New file `A2aClient/A2aExtensions.qc`:
   - `public const A2aExtensionsHeader = "X-A2A-Extensions";`
   - `hashdecl A2aAgentExtension { string uri; *string description; bool required = False; *hash<auto> params; }`
   - parse/serialise helpers for the comma-separated header value
2. `A2aClient.qc`: add `requestExtensions(list<string> uris)`; send `X-A2A-Extensions` on
   every JSON-RPC request; parse the echoed header from the response and expose
   `getActivatedExtensions()`. Fail the call if a `required` extension the card declares is
   not activated by the server.
3. `A2aVersionTranslation.qc`: **stop deleting `extensions`.** v1.0 → v0.3 keeps the field
   (v0.2.2+ has it too); only strip for a peer that declared `protocolVersion` < 0.2.2.
4. `A2aServerHandler.qc`: register handlers by extension URI. Add a
   `registerExtension(string uri, code handler)` hook and an overridable
   `getSupportedExtensions()` that feeds the agent card. Echo `X-A2A-Extensions` with the
   subset actually activated. Reject with a JSON-RPC error when the client requests a
   `required` extension the server does not implement.

### 2.2 Signed agent cards

**Current state:** no `signature`/`jws` handling anywhere in `A2aClient/` or `A2aServerHandler/`.

A2A v1.0 signs agent cards with JWS (detached payload, `signatures` array on the card).

**Work:**

1. New file `A2aClient/A2aCardSignature.qc`:
   - `signAgentCard(hash<auto> card, hash<auto> key_info)` → card with `signatures`
   - `verifyAgentCard(hash<auto> card, *hash<auto> opts)` → `hash<A2aCardVerification>`
     (`verified`, `key_id`, `issuer`, `*error`)
   - JWKS resolution: `jwks_uri` from the card, `.well-known/jwks.json` fallback, plus an
     explicit trusted-keys option for pinned deployments
2. Reuse the existing JWT/JWS primitives already in the tree rather than adding a crypto
   dependency — audit what `qlib/` provides before writing anything new.
3. `A2aClient.qc`: add a `verify_card_signature` option (default *warn*, not *fail*, so
   existing unsigned deployments keep working); expose the verification result on the client.
4. `A2aServerHandler.qc`: optional `card_signing_key` option; sign the card served at both
   `/.well-known/agent-card.json` (`:239`) and `/.well-known/a2a-agent-card` (`:250`).

### 2.3 REST and gRPC bindings — scope decision

v1.0 defines three protocol bindings; Qore implements **JSONRPC only**.
`A2aVersionTranslation.qc:405` explicitly picks the `JSONRPC` interface out of
`supportedInterfaces` and `A2aClient.qc:221` documents `"JSONRPC"`, `"REST"`, `"GRPC"` as
values it never produces.

**Recommendation: implement the REST (HTTP+JSON) binding, skip gRPC.** REST is a mechanical
mapping over machinery that already exists (`RestClientIo` on the client, the HTTP handler on
the server) and is what most non-Google implementations expose. gRPC would pull in a protobuf
service definition and a new server transport for very little reach — revisit only if a
customer asks. Document the omission in the module docs so the gap is explicit rather than
implied.

If REST is deferred too, at minimum make `A2aVersionTranslation.qc:405` **fail loudly** with a
clear error when a card advertises only non-JSONRPC interfaces, instead of silently falling
through to `supportedInterfaces[0]` (`:410`) and then talking JSON-RPC to a REST endpoint.

### 2.4 Tests

- `A2aVersionTranslation.qtest`: extensions survive round-trip translation in both directions
- `A2aServerHandler.qtest`: header echo, required-extension rejection, unknown-extension
  ignore, signed-card serve + client verify, tampered-card rejection, unsigned-card warn path
- new `examples/test/qlib/A2aClient/A2aCardSignature.qtest`: sign/verify unit + negative cases
  (wrong key, expired key, malformed JWS, missing JWKS)

---

## Phase 3 — MCP Apps extension

### Why this one and not Skills

**Skills over MCP is not implementable yet.** It is SEP-2640, *In Review*, on the Extensions
Track, produced by a WG that only converted from Interest Group in April 2026. The design
direction (Resources-based) is settled but the wire format is not. Building against it now
means rebuilding it. **Track the SEP; do not implement.** Revisit when it merges.

MCP Apps, by contrast, is a published official extension (`modelcontextprotocol/ext-apps`,
spec revision 2026-01-26) already supported by Claude, Claude Desktop, VS Code Copilot,
M365 Copilot, Goose, and Postman.

### Extension shape

- identifier: **`io.modelcontextprotocol/ui`**
- client settings: `{"mimeTypes": ["text/html;profile=mcp-app"]}`
- server declares it in `server/discover` → `capabilities.extensions`
- a tool opts in via `_meta.ui.resourceUri` pointing at a `ui://` resource; `_meta.ui` may
  also carry `csp` (allowed external origins) and `permissions`
- the host fetches the `ui://` resource (HTML, usually with inlined JS/CSS), renders it in a
  sandboxed iframe, and bridges `ui/`-prefixed JSON-RPC over `postMessage`

### Qore already has the rail

The extension framework is in place from the Tasks work:

- `McpClient/McpProtocol.qc:58` — `MCP_ExtensionTasks = "io.modelcontextprotocol/tasks"`
- `McpServerHandler/McpServerHandler.qc:386` — `extensions` block in `ModernCapabilities`
- `McpServerHandler.qc:1708` — `getDiscoverCapabilities()` overridable hook, documented for
  exactly this
- `McpServerHandler.qc:3659` — per-request `clientSupportsTasks()` pattern to mirror
- `McpClient/McpClient.qc:595` — client-side extension advertisement

### Scoping: server full, client pass-through

The **server** side is fully in scope for Qore and is not large. The **host** side (iframe
sandbox, `postMessage` bridge, CSP enforcement) is a browser concern — a Qore `McpClient` is
not a rendering host. `McpClient` should therefore *negotiate* the extension and *surface*
`_meta.ui` plus the fetched `ui://` resource to its caller, and stop there. Attempting a
"host" in Qore would be building a browser.

### Work

**3.1 — `McpProtocol.qc`:** add `MCP_ExtensionUi = "io.modelcontextprotocol/ui"`, the
`text/html;profile=mcp-app` MIME constant, and a `McpUiMeta` hashdecl (`resourceUri`,
`*csp`, `*permissions`).

**3.2 — `McpServerHandler.qc`:**
- add `MCP_ExtensionUi` to `ModernCapabilities.extensions` (`:386`), gated so it only
  advertises when the subclass has registered at least one UI resource
- `registerUiResource(string uri, string html, *hash<auto> ui_meta)` — validates the `ui://`
  scheme and serves it through the existing resource machinery
- tool registration gains an optional `ui` argument that populates `_meta.ui` on the
  `tools/list` entry
- `clientSupportsUi(hash<auto> cx)` mirroring `:3659`; when the client does **not** support
  it, the tool must still return meaningful text content (the spec's graceful-degradation
  requirement) — enforce this by requiring a text fallback at registration time

**3.3 — `McpClient.qc`:** advertise `MCP_ExtensionUi` in `client_capabilities.extensions`
under an opt-in option (mirroring `enable_tasks` at `:595`); expose `_meta.ui` on tool
descriptors and add `getUiResource(string uri)`.

**3.4 — Tests:** extend `examples/test/qlib/McpServerHandler/McpModernProtocol.qtest`
(891 lines) — capability advertised only when a UI resource exists, `_meta.ui` present on
`tools/list`, `ui://` resource fetch, text fallback returned to a non-UI client, registration
rejected when a UI tool has no text fallback. Client side in `McpClient.qtest`.

### The Qorus payoff

Qorus already has a `ui_component` ContentPart type (`design/rich-content-schema.md`) with a
real component registry — `interfaces`, `oauth2-authorization`, `connection-management`,
`design-artifact-summary`, `design-review-panel` — but it renders **only in the Qorus
front end**. `QorusOpenApiGateway` already registers every terminal REST operation as an MCP
tool (`design/openapi-mcp-gateway.md`).

MCP Apps closes that loop: attaching `_meta.ui` to those gateway tools makes Qorus panels
render inside Claude Desktop, VS Code Copilot, and M365 Copilot. That is a genuinely new
distribution channel for the design-mode review panel, not a reimplementation.

Sequencing note: do this **before** Phase 4. It is the correct home for renderable UI, and it
determines how much of `ui_component` the AG-UI binding has to carry.

---

## Phase 4 — AG-UI: assessment and binding

### Does AG-UI benefit Qonsole? — Yes, additively. Not as a replacement.

**What Qonsole has today** (`design/qonsole.md`, `design/rich-content-schema.md`):

- `POST /api/v9/qonsole/exec` → SSE with `exec-start` / `exec-progress` / `exec-complete`
- `ContentPart` envelope shared with the Creator WebSocket: `text`, `code`, `markdown`,
  `json`, `yaml`, `image`/`audio`/`video`/`document`/`file`, `table`, `ui_component`
- session token in the `Qonsole-Session` header; no persistent connection
- wizards with `step` / `confirmAck` / `rollback` (`design/qonsole-wizards.md`)
- design mode: clients refresh `ui_content_parts` from
  `GET /api/latest/qonsole/design/{artifact_id}/transition-options` rather than replaying chat
- three client classes: ncurses CLI, IDE front end, AI agents

**AG-UI event vocabulary:** `RunStarted`/`RunFinished`/`RunError`, `StepStarted`/`StepFinished`,
`TextMessageStart`/`Content`/`End`, `ToolCallStart`/`Args`/`End`/`Result`,
`StateSnapshot`/`StateDelta` (RFC 6902 JSON Patch)/`MessagesSnapshot`,
`ActivitySnapshot`/`ActivityDelta`, `ReasoningStart`/`MessageContent`/`End`, `Raw`, `Custom`.

**Where it wins:**

1. **`StateSnapshot`/`StateDelta` for design mode.** Today the FE re-GETs the whole
   `transition-options` projection to refresh. JSON Patch deltas push only what changed on
   the stream the client is already reading. Fewer round trips, smaller payloads, and no
   "did I miss an update" ambiguity.
2. **Typed tool-call events.** `exec-progress` is a single untyped event.
   `ToolCallStart`/`Args`/`End`/`Result` lets the FE render per-tool progress for the
   MCP-gateway tools without Qorus inventing its own vocabulary.
3. **Reasoning events.** The NL pipeline is a 3-phase LLM flow with no standard way to
   surface intermediate reasoning; AG-UI has one.
4. **Third-party front ends.** `@ag-ui/client` (TS) and CopilotKit React components mean an
   embedded chat surface in a customer product — or a third-party IDE — works without anyone
   writing a bespoke Qonsole client. This is the strongest commercial argument, and it speaks
   directly to the "IDE integration" client class already in the Qonsole architecture.
5. **HITL.** Wizard `step`/`confirmAck`/`rollback` maps onto AG-UI's tool-call approval
   pattern, making wizards drivable from a generic front end.

**Where it does not win — state these plainly:**

1. **It does not fix the design-loop scalability ceiling.**
   `design/qonsole-design-loop-io-scalability.md` establishes that the ~3 drives/s ceiling is
   **CPU-bound in qorus-core**, and that persistence batching measured 3-6% *slower* and was
   reverted. Delta streaming reduces client-refresh round trips and payload size; it does
   **not** touch the per-turn CPU that is the actual lever. Do not sell it as a performance fix.
2. **`ui_component` has no AG-UI equivalent.** It would ride as `Custom` events — a wrapper,
   not a gain. MCP Apps (Phase 3) is the right home for renderable UI, which is why Phase 3
   comes first.
3. **The ncurses CLI gains nothing.** It is a native client on a protocol that works.
4. **No versioned spec.** AG-UI publishes SDK releases, not numbered protocol revisions
   (confirmed against the events reference). Pin to a specific `@ag-ui/*` release and treat
   the event set as a moving target — this is a real maintenance cost, not a footnote.

**Verdict:** worth doing as an **additive second binding** over the existing Qonsole core
event stream. A rewrite of a deployed, working protocol with three live client classes is not
justified and is not proposed.

### Work — qore side: new `AgUi` module

New `qlib/AgUi/` (server-side emitter + client), mirroring the `A2aServerHandler` layout:

| File | Contents |
|---|---|
| `AgUi.qm` | module def; `%requires` DataProvider, RestClientIo, json |
| `AgUiEvents.qc` | event-type constants + one hashdecl per event, using hashdecl inheritance from a base carrying `type`/`timestamp`/`rawEvent` |
| `AgUiEmitter.qc` | typed emit methods; SSE framing; monotonic sequencing; `StateDelta` generation from a before/after hash pair (RFC 6902) |
| `AgUiClient.qc` | consumes an AG-UI SSE stream into typed events; applies `StateDelta` patches |
| `AgUiConnection.qc` | ConnectionProvider integration |

`AgUiEmitter` must not own a socket — it emits into whatever SSE response stream the caller
already has, exactly as `A2aServerConnection.qc:76` does. That keeps it usable from both the
Qonsole HTTP/SSE handler and the Creator WebSocket.

The RFC 6902 diff generator is the only non-trivial piece. Check whether `qlib/` already has
JSON Patch support before writing one; if not, it belongs in a general-purpose module, not
buried in `AgUi`.

### Work — qorus side: `/api/v9/qonsole/agui`

A **projection**, not a fork. `QorusQonsoleCore` keeps emitting its existing internal events;
a new adapter renders them as AG-UI instead of `exec-*`. Content-type or path selects the
binding; `QorusQonsoleRestClass.qc` gains one endpoint.

ContentPart → AG-UI mapping:

| ContentPart | AG-UI |
|---|---|
| `text`, `markdown` | `TextMessageStart`/`Content`/`End` |
| `code` | `TextMessageContent` with a fenced block |
| `json`, `yaml`, `table` | `ToolCallResult.content` |
| `image`/`audio`/`video`/`document`/`file` | `ToolCallResult.content` with the URL |
| `ui_component` | `Custom` (`name` = component id, `value` = `{data, title, context}`) — or an MCP App once Phase 3 lands |
| design artifact state | `StateSnapshot` on run start, `StateDelta` per turn |
| wizard `step`/`confirmAck` | `ToolCallStart`/`Args`/`End` + approval round trip |
| `exec-start` / `exec-complete` / errors | `RunStarted` / `RunFinished` / `RunError` |

Update `design/rich-content-schema.md` and `design/qonsole.md` with the second binding once
it lands.

### Tests

- `examples/test/qlib/AgUi/AgUiEvents.qtest` — construction and serialisation of every event type
- `examples/test/qlib/AgUi/AgUiEmitter.qtest` — SSE framing, ordering, `StateDelta` correctness
  including the empty-diff and whole-replacement corner cases
- `examples/test/qlib/AgUi/AgUiClient.qtest` — loopback emitter → client, patch application,
  malformed-event and truncated-stream negative cases
- Qorus side: a Qonsole test asserting the same command produces equivalent content over both
  bindings

---

## Sequencing

```
Phase 0 (OpenAI)  ████████░░░░░░░░░░  deadline 2026-08-26 — start now, blocks nothing
Phase 1 (A2A 1.0.1)   ████░░░░░░░░░░  small, independent
Phase 2 (A2A ext/sig)     ████████░░  depends on 1; gates any future AP2/x402
Phase 3 (MCP Apps)            ██████  independent; gates Phase 4's ui_component decision
Phase 4 (AG-UI)                 ████  qore module, then qorus binding
Skills over MCP                    ✗  draft SEP — watch only
```

Phase 0 is the only item with a clock on it and shares no code with the others, so it can run
concurrently with Phase 1.

## Cross-cutting requirements

- `%modern` throughout; hashdecl inheritance for the event/extension type hierarchies
- new modules added to both `CMakeLists.txt` and `Makefile.am` so docs build and they install
- functional domains marked on anything touching network or filesystem
- no version bumps until public release; no release notes for unreleased functionality
- tests use `%prepend-module-path` for local modules and `TmpFile`/`TmpDir` from `FsUtil`
- every phase's tests pass without warnings before the next phase starts
- no C++ changes are anticipated in any phase, so valgrind is not required unless that changes
