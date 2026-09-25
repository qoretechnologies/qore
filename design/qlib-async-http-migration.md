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
| 3 | Sync clients created inside async providers: SendCloud cross-host/v3 clients (also fixed dropping a non-default port), AWS STS `assumeRole()`, generic API call discovery through a connection member (CustomerIo and PdfCo moved to phase 5 with their providers) | done |
| 4 | Discord and ServiceNow providers (sync clients passed in are converted); `ServiceNowRestClientIo` and `CdsRestClientIo` brought to parity with the sync clients (API path, API key, auto OAuth2 URLs, error translation); CDS providers (absolute `@odata.nextLink`, request path encoding) | done |
| 5 | 27 provider families typed to a sync `FooRestClient` (WooCommerce in phase 7); missing Io behavior added to the clients (GoHighLevel location, Zoho organization, FreshBooks account/business, Tableau sign-in, Unleashed signing, BigCommerce auth header, 429 retries) | done |
| 6a | Discord gateway discovery, `get_file_from_http()` (HttpClientIo loaded at run time: Util cannot require it), JSON-LD loader, A2A and ONE Record push notifications | done |
| 6b | `HttpClientDataProvider`, `FileLocationHandler` (HttpClientIo / RestClientIo loaded at run time: static dependencies would be cycles); `HttpConnection::getDataProvider()` loaded a nonexistent `HttpDataProvider` module | done |
| 7 | New async clients: FHIR, EmpathicBuilding, WooCommerce; `ServerSentEventClientDataProvider` with `ServerSentEventClientIo` | done |
| 8 | Connection polling pings and requests (`HttpPingPollOperation`, `HttpRequestPollOperation`, SSE connect and ping, WebSocket ping) on HttpClientIo; the ServiceNow, Mews and Salesforce pings on `RestClientIoPingPollOperation` | done |

## Watch providers

`AbstractWatchDataProviderBase` cancels its polling thread with `cancel_thread()` when polling is stopped, which ends
an in-flight `RestClientIo` request at once; observer notifications and checkpoint persistence run under
`defer_thread_cancel()`.  A watch provider must not close its REST client: the client is shared with the other
providers of the connection, and `RestClientIo::close()` shuts down the connection manager for good.

## Response body character encoding

`HttpClientIo` and `Qore::HTTPClient` decode response bodies with the same rules (see `design/http-body-charset.md`):
text without a `charset` is ISO-8859-1 unless the media type defines its encoding (JSON, YAML and XML are UTF-8 by
default), and each client can change the assumed encoding (`assume_encoding`).  The modules that replaced `HTTPClient`
therefore return the same strings without re-labelling them.

## Known limitations

| Limitation | Modules |
| --- | --- |
| The client builds its URL from other options and ignores `url`; loopback tests point the client at the local server with a test subclass | CustomerIo, Chargebee, Jotform, Instantly, Mailgun, Zoho*, Tableau, ShipStation, Discord |
| `copyWithUrl()` rebuilds from options already processed by `getOptions()`, which is not idempotent, so the copy loses the URL or credentials | Mailgun, PdfCo, Zoho*, Instantly, BigCommerce (store hash) |
| A per-request value (organization / account / business ID) is set on the shared client before a request | ZohoInventory, FreshBooks |
| `RestClientIo` sends request paths verbatim, while the synchronous client percent-encodes invalid characters; modules must encode the paths they build (CDS does with `CdsRestDataProviderBase::encodeRequestPath()`) | all |
