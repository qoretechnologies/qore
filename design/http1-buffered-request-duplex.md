# HTTP/1.1 buffered request bodies are sent full-duplex

**Status:** implemented (`lib/QC_Http1ClientPollOperationBase.qpp`,
`include/qore/intern/QC_Http1ClientPollOperationBase.h`)

HTTP/1.1 is a serial protocol per exchange, but the two *directions* of one exchange are not
serial.  A client that finishes writing its request before it starts reading the response
deadlocks against any peer that commits to a response before it drains the request body: neither
side's write can drain, and both block until something times out.

This document records when the HTTP/1.1 client poll operation runs the two directions
concurrently, and what it does when one of them finishes first.

## Why it is required

SOAP 1.2 Part 2 §§6.2.3 and 7.5.1 require a requesting SOAP node to accept and process response
information while it is still transmitting, precisely to avoid this deadlock (W3C test assertions
`x2-bindformdesc-dlock`, `x2-http-reqsoapnode-dlock`).  The same shape occurs outside SOAP
wherever a peer answers a large upload before reading it: streaming transformation services,
early `413`/`401` faults, and any service that replies from the request headers alone.

The deadlock needs no exotic peer.  It appears as soon as the response exceeds the peer's send
buffer plus the client's receive buffer *and* the request exceeds the client's send buffer plus
the peer's receive buffer — a few megabytes each on a stock Linux loopback.

## The two request-body shapes

| Shape | Framing | Body source | Send side |
|---|---|---|---|
| streaming send | `Transfer-Encoding: chunked` | the application pushes it incrementally (`pushSendData()`) | active from the moment the request headers are written |
| buffered send | `Content-Length` | a string or binary payload held in full by the client | active only once a partial write shows the headers are out |

Both drive the same `SendState` machine on `send_op` (`NB_SEND`) concurrently with the `ReqState`
receive machine on `current_op` (`NB_RECV`); the socket's non-blocking flags are per direction, so
the two coexist.  `send_buffered_body` distinguishes them, and it matters: the buffered send owns
its whole remaining body, while a streaming send owns only what the application has pushed so far.

## Handover for a buffered body

The request header block and the body are built into **one** buffer
(`Http1ClientPollOperationPriv::submitRequest()`), so a request that fits in the socket's send
buffer still reaches the wire in a single write.  Splitting every request in two would cost an
extra syscall and expose every small `POST` to the write-write-read delay that Nagle plus delayed
ACK produces.

`handleSending()` therefore watches for a *partial* write instead:

1. `SocketSendPollOperation::continuePoll()` returns poll info — the socket would not take the
   whole request.
2. `SocketSendPollOperation::getBytesSent()` is at least the header block's length
   (`request_header_len`, recorded with the request), so the peer can already be answering.
3. `startBufferedFullDuplex()` moves the in-flight send operation from `current_op` to `send_op`,
   installs a `SocketReadHttpHeaderPollOperation` as the new `current_op`, and enters
   `ReqState::RECV_HEADER` with `SendState::SENDING`.

A request that completes in one write never takes this path, so nothing changes for it — not the
syscall count, not the stale-keep-alive check that follows a completed send.

## Receive before send

`handleFullDuplex()` drives the receive side first and the send side second.  A peer that answers
early — a fault raised before it has drained the request — very often stops reading or closes
right after writing its response, which makes the next request-body write fail with `EPIPE` or
`ECONNRESET`.  Draining the response first means the caller gets the peer's actual answer instead
of the write error that answer caused.

The poll interests of both directions are merged into the returned poll info, so the controller
wakes the operation for either one.

## When the response finishes first

A buffered request declared a `Content-Length`; stopping short of it leaves the peer holding a
request it cannot process.  Since the whole body is already in hand, finishing it costs only the
writes the request already promised.  `dispatchResponse()` and `dispatchStreamingEnd()` therefore
park the finished response in `ReqState::AWAIT_SEND` and let `awaitBufferedSend()` run the send
side to its goal before the dispatch resumes:

| Outcome | Response | Connection |
|---|---|---|
| request body completes | delivered once the body is out | reusable — both directions completed |
| peer stops reading (send fails) | delivered anyway; it is complete and authoritative | closed — it carries a half-written request |
| receive side fails first | the transport error reaches the caller | closed |

Nothing bounds `AWAIT_SEND` beyond the caller's own request timeout; that is deliberate, since a
timer here would be an arbitrary deadline layered on the one the caller already chose.

A **streaming** send is not deferred: its remaining body only the application can produce, so the
pre-existing behavior stands — the response is delivered and the connection is force-closed
because unsent request bytes remain on it.

## `100 Continue`

RFC 9110 §15.2.1 makes `100 Continue` the server's promise of a final response to come, so it is
never the answer itself.  Reading the response concurrently with the body is exactly what makes it
arrive: a peer answers `100 Continue` first whenever the request carried `Expect: 100-continue`,
which `qlib/HttpServer.qm` does.  `handleRecvHeader()` discards it and reads the next header
block, bounded by `MAX_INTERIM_RESPONSES` so a peer cannot stream them forever.

Only `100` is consumed.  `101` ends HTTP framing and is handled separately, and the remaining
`1xx` codes keep the behavior a handler returning one relies on (see the `102` case in
`HttpServer.qtest`'s `issue3116`), since nothing forces a final response to follow those.

## Tests

`examples/test/qore/classes/HTTPClient/HttpClientBufferedDuplex.qtest` drives a raw peer that
writes its whole response before it reads a single body byte, so `PeerObserved::response_written`
can only become true if the client read while its own upload was in flight.  It covers plain and
TLS transports, string and binary bodies, `Content-Length` / chunked / close-delimited response
framing, an early fault from a peer that stops reading, `100 Continue` mid-upload, peer EOF,
request timeout, caller cancellation, same-client recovery after each of those, and a small
request that must still go out in one write and leave the connection poolable.

The chunked full-duplex path keeps its own acceptance gate in
`examples/test/qlib/HttpClientIo/HttpClientIo.qtest` (`FullDuplexEchoRawServer`).
