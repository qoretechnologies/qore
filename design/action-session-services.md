# Action-Based Session Services

Several %Qore modules expose interactive, stateful services — DPQL editing, the REPL, LLM response
streaming — that a consuming platform reaches over a WebSocket. Rather than each module defining its own
wire protocol, the behaviour lives in a transport-agnostic **action session** class, and the WebSocket
handler is a thin adapter over it.

The point is that the same session object serves a %Qore WebSocket module and an embedding platform's
own handler without either duplicating protocol logic, and that the interesting behaviour is unit
testable without a socket.

## The session shape

A session class is not derived from a common base — the three implementations have genuinely different
lifecycles — but they share a convention:

| Member | Contract |
|---|---|
| `handleAction(string action, *hash<auto> args)` | dispatch one action by name and return its response |
| `setEmitter(*code cb)` | register a callback for asynchronous events, where the service has any |
| `close()` | release resources, where the service holds any |

`handleAction()`'s return type is per-service and is **not** uniform: `DpqlActionSession` and
`OpenAiResponseActionSession` return `hash<auto>`, while `QoreReplActionSession` returns
`list<hash<auto>>` because one evaluation can produce several ordered payloads. An adapter must not
assume a single response hash.

Actions are named `<service>-<verb>` so that one flat namespace can carry every service on a shared
connection.

## The implementations

| Class | Module | Actions |
|---|---|---|
| `DpqlActionSession` | `qlib/DataProvider/` | `dpql-set-context`, `dpql-parse`, `dpql-get-completions`, `dpql-get-tokens`, `dpql-format`, `dpql-validate`, `dpql-eval`, `dpql-serialize`, `dpql-get-signature-help` |
| `QoreReplActionSession` | `qlib/QoreRepl/` | `repl-eval`, `repl-command`, `repl-interrupt`, `repl-complete`, `repl-close`; emits `repl-output`, `repl-result`, `repl-prompt`, `repl-completion`, `repl-error`, `repl-exit` |
| `OpenAiResponseActionSession` | `qlib/OpenAiDataProvider/` | `openai-response-create`, `openai-response-start`, `openai-response-stop`; emits `openai-response-event`. Inherits `DataProvider::Observer` and manages the response stream provider's lifecycle |

`DpqlWebSocketConnection` (`qlib/DpqlWebSocket/`) is the reference adapter: it constructs a
`DpqlActionSession` and translates its message types into action calls.

`dpql-set-context` exists because schema-aware completion needs resolved field types, including nested
hash and list structure — that is what makes `@record{` able to suggest hash keys. The session caches
that context so later actions carry a small payload; see [dpql-integration.md](dpql-integration.md) for
callback setup and field-metadata construction.

## Token spans

`dpql-get-tokens` and `dpql-parse` return editor tokens in one of two formats, selected by the session's
`token_format` option: `"dpql"` returns raw `DpqlTokenInfo` records, and `"token-span"` (the default)
returns a **language-neutral** span designed to suit a tree-sitter capture stream as readily as the DPQL
tokenizer, so a client renders one shape regardless of source:

```
TokenSpan:
- type (string)          category, from the fixed set below
- start (hash)           row (int, 0-based), column (int, 0-based)
- end (hash)             row (int, 0-based), column (int, 0-based)
- text (*string)         raw token text
```

Two details are easy to get wrong. The rows and columns are **0-based**, while `DpqlTokenInfo` is
1-based — `toTokenSpan()` converts. And whitespace, newline and EOF tokens are dropped from the span
stream but present in the raw one, so span offsets are not positional indices into the token list.

The categories are deliberately few, and several DPQL token types collapse into one:

| Category | Source token types |
|---|---|
| `field` | `DPQL_TOK_FIELD` |
| `identifier` | `DPQL_TOK_IDENTIFIER`, and any unmapped type |
| `keyword` | `DPQL_TOK_BOOLEAN`, `DPQL_TOK_NULL` |
| `string` | `DPQL_TOK_STRING`, `DPQL_TOK_REGEX` |
| `number` | `DPQL_TOK_NUMBER` |
| `constant` | `DPQL_TOK_DATE`, `DPQL_TOK_BINARY`, `DPQL_TOK_LIST`, `DPQL_TOK_HASH` |
| `operator` | every `DPQL_TOK_OP_*` |
| `punctuation` | `DPQL_TOK_LPAREN`, `DPQL_TOK_RPAREN`, `DPQL_TOK_COMMA` |
| `variable` | `DPQL_TOK_TEMPLATE` |
| `error` | `DPQL_TOK_ERROR` |

Collapsing is the design, not a shortcut: a client themes a small fixed vocabulary, and a new DPQL token
type maps into an existing category rather than forcing every client to learn it. `mapTokenType()`
returns `identifier` for anything unmapped, so an unrecognised token renders plainly instead of
throwing.

Adding a DPQL token type therefore means adding a `mapTokenType()` case — otherwise it silently renders
as an identifier.

## Documenting the WebSocket surface

Every action and event carried over a WebSocket must appear in that handler's `@WEBSOCKET` block as
`@subscribe` / `@publish` message schemas — see
[asyncapi-schema-generation.md](asyncapi-schema-generation.md). The session class is the source of
truth for what those schemas describe.
