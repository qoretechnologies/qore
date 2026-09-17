# LLM Chat Streaming

Copyright (C) 2026 Qore Technologies, s.r.o.

`LlmBackend::chatStream()` streams a chat completion through the provider registered for the backend mode.
This document describes the event contract every provider implements and how streamed responses are assembled.

## Event contract

A provider calls the stream callback with `ChatStreamEvent` hashes:

| Rule | Detail |
| --- | --- |
| One event per native event | Each provider-native stream event is delivered at most once, with the parsed event in `raw`. Delivering a native event twice makes the stream consumers count its tool calls twice. |
| Text | `delta` holds the text of the native event. Events without text (tool calls, usage, reasoning) are delivered without `delta`. |
| Final event | Exactly one event has `done` set. It can carry the last text and the last native event; a stream that ends without a native final event gets a final event without `raw`. |
| Failures | A rejected request, a response that is not an event stream, an error event in the stream, an event that cannot be parsed, and a read timeout raise `LLM-ERROR`. A failure is never reported as a completed response. |

`SseStreamReader::readEvent()` returns nothing both at the end of the stream and when the wait for an event times
out; providers tell them apart with `isDone()`.

## Opening the stream

Providers open their streams with `RestClientIo::restSseReader()`, which applies the client's default headers and
connection path and validates the response before any event is read:

- An error status raises `REST-RESPONSE-ERROR` with the status and the raw body. An error body that cannot be
  deserialized (Vertex AI sends JSON errors with the event-stream `Content-Type`) keeps the raw body instead of
  raising `DESERIALIZATION-ERROR`, which would hide the status.
- A successful response whose `Content-Type` is not `text/event-stream` raises `REST-RESPONSE-ERROR`.
- Events delimited with `\n\n` or `\r\n\r\n` are both separated (Gemini and Vertex AI use CRLF).

The providers add the provider's error message to the `LLM-ERROR` description.

## Assembling responses

The provider hooks `AbstractLlmProvider::updateOpenAiStreamStateFromRaw()` and
`AbstractLlmProvider::getOpenAiToolCallDeltas()` read the native events into an OpenAI-compatible stream state:
the response ID, model, usage, finish reason, tool calls, and, where replay requires them, reasoning blocks in
`state.reasoning_blocks`. Three consumers use the hooks, so every provider's events are handled the same way:

| Consumer | Result |
| --- | --- |
| `LlmBackend::chatCompletionsStream()` | OpenAI `chat.completion.chunk` hashes and the final OpenAI response |
| `LlmBackend::chatStreamCollect()` | a `ChatResponse`, as returned by `chat()`, including tool-call reasoning signatures and reasoning blocks |
| `LlmBackend::chatStream()` telemetry | the stop reason, usage, response ID, and model of `stream_delta` and `end` events |

`AbstractAgentStrategy::callChatStream()` uses `chatStreamCollect()`. `SimpleAgentStrategy::executeStream()`
suppresses the final event of each model response and emits one final event at the end; the text of a
suppressed final event is still forwarded, and the reasoning blocks of a response are replayed with its
assistant turn.

## Providers

| Mode | Streaming |
| --- | --- |
| `gemini`, `vertex-ai` | `AbstractGeminiModelProvider::chatStream()` streams `:streamGenerateContent?alt=sse`; the path is derived from the provider's `:generateContent` path. A chunk with a `finishReason`, or with a `promptFeedback.blockReason` for a blocked prompt, is final. A blocked prompt has the OpenAI finish reason `content_filter` and the stop reason `stop_sequence`. |
| `anthropic` | Every Messages API event except `ping` is delivered; `message_stop` is final, and an `error` event raises. The hooks assemble `tool_use` blocks from `input_json_delta` events (a tool call without input gets `{}`) and `thinking` / `redacted_thinking` blocks from their deltas. |
| `http` and the OpenAI-compatible modes | Chunks with text or tool-call deltas are delivered; the chunk with a `finish_reason` is final. Subclasses supply their credentials through `buildAuthHeader()` (Azure OpenAI sends `api-key` or an Entra bearer token). |
| `bedrock` | One Converse request; the complete response is delivered as one final event, and the hooks read its `toolUse` blocks, stop reason, and usage. |
| `callback` | With `api_stream_callback`, the gateway's OpenAI chunks are forwarded; otherwise one `chat()` call is delivered as a text event and a final OpenAI chunk that includes the tool calls. |
| `cohere` | Streaming is not supported. |
