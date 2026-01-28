# Action-Based Session Services for Creator WebSocket

This document defines action-first session APIs in Qore so Qorus can expose a unified
Creator WebSocket interface while preserving the existing "action" pattern.

## Goals

- Keep Creator WS client contract based on `action`.
- Make REPL, DPQL, and OpenAI response streaming transport-agnostic in Qore.
- Share behavior between Qore WS modules and Qorus without protocol duplication.
- Provide consistent request/response payloads for the frontend.

## Non-Goals

- Changing existing action names in Qorus Creator WS.
- Breaking the existing Qore WebSocket modules.

## Architecture Overview

Introduce Qore session classes that consume `action` + args and emit structured responses
and events. Existing WebSocketConnection classes delegate to these sessions.

### Session API Shape

All sessions should expose a minimal common surface:

- `handleAction(string action, hash<auto> args)` -> `hash<auto>` response
- Optional `setEmitter(code cb)` or constructor option to receive events
- Optional `close()` for cleanup

`handleAction()` returns a response hash suitable for Qorus `sendResponse()` payloads.

## DPQL Session

### New Session Class (Qore)

Suggested name: `DpqlActionSession`.

Responsibilities:
- Track context (provider, record type, expressions).
- Provide parse, tokenize, completion, validation, serialization.
- Keep payload size minimal via context caching.

### Actions

- `dpql-set-context`
  - args: `provider`, `subtype`, `options`, `recordOptions`, `recordType`
  - response: context summary (fields, expressions)
  - note: Required for schema-aware key completions. The provider resolves full field
    type information including nested hash/list structures, which enables suggesting
    hash keys when the cursor is inside `{...}` accessors.
- `dpql-parse`
  - args: `text`
  - response: diagnostics + tokens
- `dpql-get-completions`
  - args: `text`, `position`, optional `fields` override
  - response: completions list
  - note: Uses cached context from `dpql-set-context` if available. Schema-aware key
    completions (e.g., `@record{` suggesting hash keys) require provider-backed context.
- `dpql-get-tokens`
  - args: `text`
  - response: tokens list
- `dpql-format`
  - args: `text`
  - response: formatted text
- `dpql-validate`
  - args: `text`
  - response: diagnostics list
- `dpql-serialize`
  - args: `expression`
  - response: serialized string

### Compatibility

`DpqlWebSocketConnection` should instantiate `DpqlActionSession` and translate `type`
messages to action calls. Qorus should call actions directly.

## REPL Session

### New Session Class (Qore)

Suggested name: `QoreReplActionSession`.

Responsibilities:
- Provide REPL execution and command handling.
- Emit streaming output and prompt updates for UX.

### Actions

- `repl-eval` (args: `code`)
- `repl-command` (args: `command`)
- `repl-interrupt` (args: `id`)
- `repl-close`

### Events

Emitted via session emitter:
- `repl-output` (stdout/stderr stream + text)
- `repl-result` (value + metadata)
- `repl-prompt` (prompt state)
- `repl-exit` (session termination)

### Compatibility

`QoreReplWebSocketConnection` should delegate to this session. Qorus uses it directly.

## OpenAI Response Streaming Session

### New Session Class (Qore)

Suggested name: `OpenAiResponseActionSession`.

Responsibilities:
- Provide non-streaming and streaming response actions.
- Manage `OpenAiResponseStreamDataProvider` lifecycle.
- Persist conversation state via `OpenAiConversationContextStore`.

### Actions

- `openai-response-create` (non-streaming)
  - args: `model`, `input`, `metadata`, `conversation_key`, `conversation_context`, `previous_response_id`
  - response: normalized output with rich parts

- `openai-response-start` (streaming)
  - args: same as create + `stream_options`
  - response: `status: streaming`

- `openai-response-stop`
  - response: `status: stopped`

### Events

Emitted via session emitter:
- `openai-response-event`
  - `event_type` (simplified category)
  - `openai_type` (raw OpenAI `type` string)
  - `data` (normalized payload)
  - `content` (*list<ContentPart>) when output is available

## Qorus Creator WS Integration

Qorus maintains action-based handlers in `QorusCreatorWebSocketHandler`.
Integration plan:
- Add per-connection session instances for DPQL, REPL, OpenAI.
- Route action to session `handleAction()` and map response to `sendResponse()`.
- Use session emitter to publish stream events via `send()` with action payload.

## AsyncAPI Schema Requirements

All Creator WS actions and events must be documented in the `/creator` `@WEBSOCKET`
block using `@subscribe` / `@publish` message schemas per
`design/asyncapi-schema-generation.md`.

## Testing Requirements

- Unit tests for new session classes.
- Integration tests for existing WS modules (ensuring behavior unchanged).
- Qorus Creator WS tests for new actions and event streams.
- Corner cases and negative tests for malformed payloads and missing context.
