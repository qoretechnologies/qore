# AI Protocol Support

Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

How %Qore implements the four AI-agent interoperability protocols it speaks, why each is
scoped the way it is, and what is deliberately left out.

| Protocol | Modules | Role |
|---|---|---|
| OpenAI Responses / Conversations | `OpenAiDataProvider` | client of a model provider |
| A2A (Agent-to-Agent) | `A2aClient`, `A2aServerHandler` | both peers |
| MCP (Model Context Protocol) | `McpClient`, `McpServerHandler`, `McpClientDataProvider` | both peers |
| AG-UI (Agent User Interaction) | `AgUi` | both peers |

All four are moving targets maintained by other organizations, so the standing rule for this
area is: **check the published specification or the reference implementation's own test data
before writing code, never a summary of it.** Every interop defect recorded below was found
that way and none of them were predicted in advance.

## OpenAI: Responses and Conversations

The Assistants API was deprecated on 2025-08-26 and is removed on **2026-08-26**. The
`assistants` / `threads` / `runs` providers are still registered and still work, but the
Responses API surface is what new work should use.

| Assistants API | Responses API replacement |
|---|---|
| Assistants | Prompts (versioned config: model, tools, instructions) |
| Threads | Conversations (`/v1/conversations`) |
| Messages | Items |
| Runs | Responses |
| Run steps | Items |
| Submit tool outputs | Response items on the next request |

Implemented under `qlib/OpenAiDataProvider/`: `OpenAiConversations*`,
`OpenAiConversationItem*`, `OpenAiResponseInputItemsDataProvider`,
`OpenAiResponseCancelDataProvider`, alongside the pre-existing
`OpenAiModelCreateResponseDataProvider` and `OpenAiResponseStreamDataProvider`.

**Deprecated, not deleted.** Every `OpenAiAssistant*` / `OpenAiThread*` / `OpenAiRun*` provider
carries a `@deprecated` tag naming its replacement and emits a one-shot warning when the
Assistants API header is used. They can be removed once the sunset has passed; removing them
earlier would be a breaking change for deployments that have not migrated.

**Known limit of the mapping.** A Prompt is a config object, not a stateful agent. Anything
that relied on assistant-side state — file attachments, per-assistant vector stores — has to
be carried by the caller. This is documented rather than emulated.

**Interop defects found by checking the spec.** The repo's own vendored `openai.yaml` predates
Conversations and cannot be trusted; the upstream `openai-openapi` schema is the reference.
Server-side conversation IDs were never propagated, and there was no way to submit tool
outputs at all.

## A2A

`A2aClient` and `A2aServerHandler` target **v1.0.1** and translate bidirectionally to v0.3 in
`A2aVersionTranslation.qc`.

- **`application/a2a+json`** is the preferred media type on v1.0; the client offers it
  alongside `application/json` and the server content-negotiates. It is never sent on v0.3,
  whose wire format is unchanged.
- **Protocol extensions** (`A2aExtensions.qc`) ride the `X-A2A-Extensions` header. The
  translation layer used to *delete* the `extensions` field outright, which silently disabled
  the mechanism every A2A add-on — AP2, x402 — depends on. A client fails the call when a
  `required` extension the card declares is not activated by the server.
- **Signed agent cards** (`A2aCardSignature.qc`) sign and verify with JWS over the existing
  JWT primitives; no new crypto dependency. Verification defaults to *warn*, not *fail*, so
  existing unsigned deployments keep working; JWKS resolves from the card's `jwks_uri`, the
  `.well-known` fallback, or explicitly pinned trusted keys.

**Bindings: JSONRPC only.** v1.0 defines JSONRPC, REST and gRPC. REST would be a mechanical
mapping over machinery that already exists and is what most non-Google implementations expose;
gRPC would pull in a protobuf service definition and a new server transport for very little
reach. Neither is implemented yet. A card advertising only non-JSONRPC interfaces fails
loudly rather than falling through to the first interface and then talking JSON-RPC at a REST
endpoint.

**Interop defects found by checking the spec.** Task states `pending` and `active` were used
where no peer accepts them; several error codes each collided with a *different* assigned
meaning; and push webhooks were wrapped in a JSON-RPC envelope, which is wrong for both
protocol versions.

## MCP

`McpServerHandler` serves protocol revision **2026-07-28** and every earlier revision back to
2024-11-05 from the same endpoint, selecting per request rather than per connection. See the
module documentation for the era split.

### MCP Apps (`io.modelcontextprotocol/ui`)

Server support is complete; the client negotiates and surfaces, and does not render.

- `registerUiResource()` declares a `ui://` HTML interface with its Content Security Policy
  and permission requests; `registerUiTool()` attaches one to a tool. The extension is
  advertised in `server/discover` only when the server actually ships an interface.
- The extension's graceful-degradation rule — a user interface tool must still return a
  meaningful `content` array — is enforced **per call**, not at registration: a closure cannot
  be statically proven to return text. A tool that returns only what its interface renders is
  reported as an error, because the alternative is a caller silently receiving nothing useful.
- An app-only tool (`visibility` without `"model"`) is omitted from `tools/list` for a client
  that cannot reach it.
- Client side: the `enable_ui` option, `getToolUiMeta()` (accepting the deprecated flat
  `_meta."ui/resourceUri"` form) and `getUiResource()`, which returns the document together
  with the sandbox policy that must be applied to it.

**Rendering is deliberately out of scope.** A host must run untrusted HTML in a sandboxed
iframe and bridge `ui/`-prefixed JSON-RPC over `postMessage`. That is a browser. An
`McpClient` hands its caller the document and the policy, and stops.

**Where the specification differed from expectation.** The tool's `_meta.ui` carries only
`resourceUri` and `visibility`; `csp`, `permissions`, `domain` and `prefersBorder` ride on the
**resource's** content entry — which is what the host renders, so that is where the sandbox
policy belongs. `csp` is structured (`connectDomains` / `resourceDomains` / `frameDomains` /
`baseUriDomains`), not a flat origin list. An *empty hash* is the presence marker for a
requested browser capability, so pruning unset members must not drop empty hashes. And client
capabilities are declared at `initialize` time by a legacy client — the way the specification's
own example negotiates — so a per-request capability check alone cannot see them.

### Skills over MCP — not implemented

SEP-2640 is *In Review* on the Extensions Track. The direction (Resources-based) is settled
but the wire format is not, so building against it now means rebuilding it. Track the SEP.

## AG-UI

`qlib/AgUi/` provides an emitter and a client for the streamed event vocabulary that lets an
agent back end drive a front end it did not write — `@ag-ui/client`, CopilotKit components, or
a third-party IDE integration.

Neither half owns a socket. `AgUiEmitter` formats events and hands each to a caller-supplied
sink, exactly as `A2aServerConnection` does, so the same emitter works over an HTTP/SSE
response stream, a WebSocket, or a test harness. `AgUiClient` parses such a stream back into
typed events and reconstructs the run's state.

The emitter guarantees the run lifecycle (one `RUN_STARTED`, exactly one terminal event, with
`runFinished()` and `runError()` mutually idempotent so an `on_error` beside a normal
completion is safe) and computes each `STATE_DELTA` against the state the client actually last
saw, falling back to a snapshot when it has none.

**No versioned specification.** AG-UI publishes SDK releases, not numbered protocol revisions,
so there is nothing to negotiate and no way for a peer to state which vocabulary it speaks.
Both halves therefore pass unknown events and unknown members through rather than rejecting
them. This is a standing maintenance cost, not a footnote.

**The wire format came from the reference SDK's cross-implementation compatibility fixtures,
not the prose documentation**, and that mattered: the wire `type` values are
`SCREAMING_SNAKE_CASE` (`RUN_STARTED`, `STATE_DELTA`) while the documentation names the event
*classes* in PascalCase — a peer sent `"RunStarted"` rejects it as unknown. Framing is a bare
`data: <json>` line with unset members omitted, matching the reference encoder.

`AgUiConnection` returns an `AgUiHttpAgent` that posts a run input and consumes the response
stream. The protocol's `context` member is spelled `ctx` in `AgUiRunAgentInput` and renamed on
the way out by `agui_run_agent_input_to_wire()`, because `context` is a reserved word in %Qore
and cannot name a hashdecl member.

### JsonPatch

`STATE_DELTA` carries an RFC 6902 JSON Patch, and `qlib/` had no generator, so
`qlib/JsonPatch.qm` is a general-purpose module rather than something buried in `AgUi`.

RFC 6901 *lookup* is not reimplemented: `json_pointer_get()`, `json_pointer_exists()` and
`json_pointer_escape()` already exist in the `json` module and are reexported. Only splitting,
joining and unescaping were missing.

The diff compares arrays positionally and emits removals highest-index-first so the indices in
earlier operations stay valid as the patch is applied in order. It does not search for moved
elements: that costs O(n·m) to find, and the common case in a streamed state delta is appends
and in-place edits, where positional comparison is already minimal.

## Qorus-side bindings — outstanding

Two integrations live in the Qorus repository and are not done:

- **MCP Apps on the OpenAPI gateway.** `QorusOpenApiGateway` already registers every terminal
  REST operation as an MCP tool, and Qorus already has a `ui_component` ContentPart with a real
  component registry that renders only in the Qorus front end. Attaching `_meta.ui` to those
  gateway tools makes Qorus panels render inside Claude Desktop, VS Code Copilot and M365
  Copilot — a new distribution channel, not a reimplementation.
- **`/api/v9/qonsole/agui`.** A *projection*, not a fork: `QorusQonsoleCore` keeps emitting its
  existing internal events and a new adapter renders them as AG-UI instead of `exec-*`.

  | ContentPart | AG-UI |
  |---|---|
  | `text`, `markdown` | `TEXT_MESSAGE_START` / `_CONTENT` / `_END` |
  | `code` | `TEXT_MESSAGE_CONTENT` with a fenced block |
  | `json`, `yaml`, `table` | `TOOL_CALL_RESULT.content` |
  | `image`/`audio`/`video`/`document`/`file` | `TOOL_CALL_RESULT.content` with the URL |
  | `ui_component` | `CUSTOM`, or an MCP App |
  | design artifact state | `STATE_SNAPSHOT` on run start, `STATE_DELTA` per turn |
  | wizard `step`/`confirmAck` | `TOOL_CALL_START`/`_ARGS`/`_END` + approval round trip |
  | `exec-start` / `exec-complete` / errors | `RUN_STARTED` / `RUN_FINISHED` / `RUN_ERROR` |

  **This is not a performance fix.** `design/qonsole-design-loop-io-scalability.md` establishes
  that the design-loop ceiling is CPU-bound in qorus-core, and that persistence batching
  measured 3-6% *slower* and was reverted. Delta streaming cuts client-refresh round trips and
  payload size; it does not touch the per-turn CPU that is the actual lever.

  The ncurses CLI gains nothing from this and should stay on the existing binding.
