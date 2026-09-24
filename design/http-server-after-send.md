# HttpServer `after_send`: running handler code after the response has been sent

**Status:** implemented (`qlib/HttpServer.qm`, `qlib/HttpServerUtil.qm`,
`qlib/HttpServerAsyncIo/HttpAsyncSocketIoController.qc`, `qlib/HttpServerAsyncIo/Http2PollOperation.qc`,
`lib/Http2Session.cpp`, `lib/QuicSession.cpp`, `lib/QuicPollOperations.cpp`,
`lib/QC_Http2PollOperationBase.qpp`, `lib/QC_Http3ServerPollOperation.qpp`, `lib/AsyncIoControllerPriv.cpp`)

A handler returns its response from `handleRequest()`; HttpServer sends it later, on another thread, through the
async I/O controller.  `HttpResponseInfo::after_send` (also in `HttpHandlerResponseInfo`) lets the handler run code
once the response has been sent, which a handler cannot otherwise order deterministically.  The motivating case is
WS-Addressing with a non-anonymous `wsa:ReplyTo`: the server answers with an empty `202` and sends the reply as a
separate request to the reply address, and Apache CXF drops a reply that arrives before it has processed the `202`.

## Contract

`code<nothing(hash<auto> cx, bool sent)> after_send` is called **exactly once** per response returned with it:

| Transport | `sent` is `True` once |
|---|---|
| HTTP/1.x | the last byte of the response (the terminating chunk of a chunked body) has been written to the socket |
| HTTP/2 | the frame carrying the response's `END_STREAM` flag has been written to the socket |
| HTTP/3 | the stream has been closed without an error, i.e. the peer acknowledged the whole response including the FIN |

`sent` is `False` when writing fails, the connection or stream is closed or reset first, the response is replaced
by an error response (invalid response, a failure in `sendReply()`), or the response never ends the exchange (SSE,
`101`, extended `CONNECT`).  With `reply_sent`, HTTP/1.x reports `True` at once (the handler wrote it on its thread)
and HTTP/2 / HTTP/3 report `False` (the server cannot observe the transmission).

The callback never runs on the async I/O thread or a controller call-dispatcher worker: it runs on the request's
thread (persistent sessions, `sendResponseSync()`) or on a handler-pool thread, under the handler's
`saveThreadLocalData()` / `restoreThreadLocalData()`.  Exceptions are logged in the error log and do not affect the
connection.

## Exactly once: `ResponseAfterSendNotifier`

`sendReply()` wraps the callback in a `Priv::ResponseAfterSendNotifier` (a once-guard, the handler, the context and
the listener) and stores it in `HttpAsyncResponseInfo::after_send`.  Every path that finally sends or drops the
response calls `notify(sent)`; only the first call has any effect.  The notifier's destructor reports `False` in a
new thread if nothing reported, so a response dropped on a path that does not report (for example a connection
cancelled during controller shutdown) still gets its callback.

## HTTP/1.x

`translateToRequestResult()` / `processInlineRequest()` put a closure in `HttpRequestResultInfo::after_send`.
`HttpAsyncSocketIoController::handleRequestResult()` then:

- never uses a fused send-and-read-next-header operation (`startPollSend*AndReadHeader()`), whose completion is only
  observed when the next request header has been read; the response is sent with `startPollSendHttpResponse()` or
  `startPollSendHttp{,Chunked}ResponseWithStream()`, with the `SendStreamResponse` state carrying `after_send` and
  `return_to_idle_after_send` in `HttpActiveConnectionInfo`
- on completion (`"sent"` / `"stream-complete"`) reports `True` and returns the connection to the idle pool or
  closes it; an error, an unexpected state or a cancelled operation reports `False`
- for the responses sent synchronously on the handler thread (non-I/O-thread-safe or close-delimited streams,
  `send_callback`), reports the result after `http_send_streamed_response()` returns or throws

Results are dispatched with `dispatchResponseSendResult()` onto the handler pool, never run on the dispatcher.

## HTTP/2 and HTTP/3: stream watches

A multiplexed response is submitted to the session by a handler thread and written later by the I/O thread, so
completion is reported by the session itself.  The controller puts a `watch_response_send` closure in the
stream's `header_info`; HttpServer calls it **before** the response is submitted (in `translateToRequestResult()`,
`sendMultiplexedResponse()` and `sendMultiplexedStreamingResponseInline()`), so no result can precede the watch.

The watch is registered through a socket enqueue operation (`Action::WatchResponseSend`), which runs on the async
I/O execution path serialized with the stream's frames; a stream that is already closed or reset is reported as
not sent at once.

**HTTP/2** (`Http2Session`): `send_buffer_base` makes positions in the outgoing byte stream absolute across buffer
compaction.  `onFrameSendCallback()` marks a watched stream when its `END_STREAM` frame is serialized; once
`sendPendingData()` has collected the frames into the send buffer, the stream's end position is recorded, and
`updateResponseSendWatches()` (run on every exit of `sendPendingData()`) reports `True` when the written position
passes it.  `onStreamCloseCallback()` reports `False` for a stream closed with an error, or before its `END_STREAM`
frame was serialized.  `Http2PollOperationPriv::continuePoll()` takes the results on every cycle.

**HTTP/3** (`QuicSession`): `streamCloseCallback()` reports a watched stream: ngtcp2 closes a stream without an
application error only after the peer acknowledged all of its data including the FIN.  The results travel with the
session lifecycle events (`SocketQuicServerPollOperation::takeSessionLifecycleEvents()`), always before the close of
their session; `Http3ServerPollOperationPriv::closeSessionStreams()` reports the session's remaining watches as not
sent.  A watch registered on a session that is already closed is reported as not sent at once: ngtcp2 does not close
the streams of a closed connection, and the session's close is reported only once (`reportCloseIfNew()`), possibly
before the watch existed — for example when the client closes its connection while the handler is still running.
Both the check and `reportCloseIfNew()` run under the session's `mtx_` on the I/O execution path, so every watch is
covered by exactly one of them.

Both poll operations hand the results to the controller, which dispatches `onStreamSendResult(stream_key, sent)`
(`DT_STREAM_SEND_RESULT_NOTIFY`) to a call-dispatcher worker.  The callbacks are kept by the poll operation objects
(`Http2PollOperation::response_send_callbacks`, the `response_send_callbacks` member of `Http3ServerPollOperation`);
the one called forwards the result to the handler pool.  When the connection closes
(`Http2PollOperation::failResponseSendWatches()` from `notifyStreamListenersClosed()` and
`HttpAsyncSocketIoController::handleConnectionError()`; `Http3ServerPollOperation::notifyStreamListenersClosed()`),
the remaining callbacks are called with `False` and no further watch is accepted.

## Tests

`examples/test/qlib/HttpServer/HttpServerAfterSend.qtest` covers every message body form over HTTP/1.1 (keep-alive
and close), HTTP/1.0, HTTP/2 and HTTP/3, persistent sessions, inline handlers, the client going away on each
transport, a client that closes its HTTP/2 or HTTP/3 connection before the response is watched, callback exceptions,
responses that are not sent as returned (and an invalid handler response being answered with a single error
response), and — with a raw client socket that does
not read until the callback has run — that the complete response can be read from the client socket when the
callback runs.
