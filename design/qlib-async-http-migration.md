# Migrating qlib modules to asynchronous HTTP I/O

Copyright (C) 2026 Qore Technologies, s.r.o.

Issue: <https://github.com/qoretechnologies/qore/issues/5469>

`HttpClientIo` and `RestClientIo` are the primary HTTP client APIs.  Modules in `qlib` do their own HTTP I/O with
them; the synchronous `Qore::HTTPClient` and `RestClient::RestClient` APIs remain supported for user code.

## Rules

| Rule | Detail |
| --- | --- |
| Providers use the async client | A data provider gets its client from the connection's `getAsync()`, never from `get()`. |
| Streams use the SSE reader | A stream is opened with `RestClientIo::restSseReader()`; `readEvent()` returns nothing both on a timeout and at the end of the stream, so a consumer checks `isDone()` before ending its loop. |
| One implementation | A synchronous class kept for compatibility is a thin wrapper over the asynchronous implementation. |
| Keep documented errors | A wrapper translates the errors of the asynchronous layer to the codes the synchronous class documents (see `sync-httpclient-as-async-wrapper.md` §4a). |
| Loopback tests | A migrated module keeps its offline tests and gets a test against a local server where it only had fake-client coverage. |

## Synchronous SSE streams

`RestClientIo::RestClientSyncSseStream` implements the synchronous streams of the AI REST clients
(`OpenAiResponseSseStream`, `AnthropicSseStream`, `GeminiResponseSseStream`, `GrokResponseSseStream`,
`PerplexityChatSseStream`).  Each stream creates the client's `*RestClientIo` class from
`RestClient::getConstructorOptions()` and closes it with the stream.

| Synchronous behavior | Implementation |
| --- | --- |
| Default headers and connection path of the client, including changes after creation | copied from the synchronous client when the stream is opened |
| Error response raises `HTTP-CLIENT-RECEIVE-ERROR` with the response headers and `status_code` in the argument | translated from `REST-RESPONSE-ERROR`; the argument also has the `body` |
| Non-event-stream success response raises `SSE-ERROR` | translated from `REST-RESPONSE-ERROR` |
| `readEvent()` raises `SOCKET-TIMEOUT` on a timeout, and the stream remains usable | raised when the reader returns nothing and has not ended |
| An event is limited to the client's `max_response_body_size`, else 128 MiB | `restSseReader()` passes the stream handle's maximum response body size to the reader |

## Status

| Phase | Scope | Status |
| --- | --- | --- |
| 1 | Streaming: sync SSE streams as wrappers, `restSseReader()` event limit, `OpenAiResponseStreamDataProvider` ending on a wait timeout, Perplexity dead sync branch | done |
| 2 | Connections passing `get()` to providers: `RestConnection` (failed with `RUNTIME-OVERLOAD-ERROR` without a schema), AWS (same), Mews, ElasticSearch, OpenSearch | done |
| 3 | Sync clients created inside async providers (SendCloud, CustomerIo, PdfCo, AWS STS, generic API discovery) | pending |
| 4 | CDS, Discord, ServiceNow providers | pending |
| 5 | 28 provider families typed to a sync `FooRestClient` | pending |
| 6 | Direct `HTTPClient` users (Discord gateway discovery, `get_file_from_http()`, JSON-LD loader, A2A and ONE Record push notifications, `HttpClientDataProvider`, `FileLocationHandler`); `HttpConnection::getDataProvider()` loads a nonexistent `HttpDataProvider` module | pending |
| 7 | New async clients: FHIR, EmpathicBuilding, WooCommerce; `ServerSentEventClientDataProvider` | pending |
