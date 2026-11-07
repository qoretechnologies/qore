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

## When the first write fails

The handover above needs a *partial* write.  A peer that answers from the request headers alone and closes at
once (an early `413`, say) can reset the connection while the client is still inside its first write of the
buffered request, before the socket has ever reported that it would block.  The next write of the body then fails
with `EPIPE` or `ECONNRESET`, and that happens before any handover.

`handleSending()` therefore decides, with `sendFailureReadsResponse()`, whether a failed first send of a request
means "the peer may have answered".  It reads the response when:

- the header block verifiably reached the wire (`SocketSendPollOperation::getBytesSent()` keeps the count of bytes
  written before a failure, and it is at least `request_header_len`), or
- the write failed because the peer closed or reset the connection (`sendFailedOnPeerClose()`: the exception's
  errno is `ECONNRESET`, `EPIPE` or `ECONNABORTED`, see `qore_socket_errno_is_peer_close()`) on the first request of
  the connection.

It does not read the response, and reports the send error as it is, when:

- the operation was closed or published an error (`closedLocally()`).  A cancel (`cancelStream()`) publishes its
  error, and an abort marks the operation closed, before the socket is shut down; the shutdown makes a write in
  progress fail with `EPIPE`, which must not be taken for a peer reset.  The error that the cancel or abort
  published stays the one reported (the first published error wins).
- the header block was not verifiably written on a connection that served a request before (`next_stream_id > 1`).
  A server that closes an idle keep-alive connection may send a response of its own first (a `408`); arriving
  before the request was on the wire, it is not this request's answer.  The send error there is reported as for
  any failed write on a reused connection: it is the connection failure that the qlib `HttpClientConnectionManager`
  repeats the request for on a new connection (`isConnectionErrorStatic()`); the `HTTPClient` class raises it (its
  manager, `QoreHttpClientConnectionManagerBase`, repeats an idempotent request only for the errors of
  `isConnectionClosedError()`).  On macOS the byte count can be short (below), so the error is also reported for a
  request whose header block did reach the server; a repeated request gets the server's answer on the new
  connection, where the first-request rule delivers it.  Reporting the connection failure is preferred to returning
  a response that may belong to no request.

`recoverResponseAfterSendFailure()` abandons the request (`SendState::ABANDONED`, see below) and reads the
response: if one has arrived it is delivered and the connection is closed; if not, the error reported is the
original send error (`send_failure_pending`), not the receive error that follows it.  If the response read cannot
even be started (the socket is closed), the send error is reported with `setError()`, so the pending stream is
notified and the connection marked closed.

The byte count alone is not enough, because it can be short after a reset.  A BSD-derived stack (macOS) fails a
`send()` that a reset interrupts and discards the count of the bytes that call had already transmitted (XNU
`sendit()` keeps a partial count only for `EINTR` and `EWOULDBLOCK`); Linux returns the partial count and reports
the error on the next call.  So on macOS the first write of a request can put the whole header block and part of
the body on the wire, the peer can answer and reset, and the write fails with `ECONNRESET` having counted nothing
(issue #5630).  A write that starts after the reset arrived fails at once, typically with `EPIPE` (seen on both
Linux and macOS in the tests), and with `ECONNRESET` when the reset is still pending on the socket.
After any of these the connection is gone, so the read returns at once with whatever the peer sent and then EOF; it
can never wait for a response that is not coming.  A peer that answers before it has read the whole header block
(a `431`, say) is handled by the same rule.

The same holds after the handover.  `handleFullDuplex()` drives the receive side before the send side, but the
response and the reset can arrive between the two: the receive side finds nothing, and the write of the rest of the
body fails.  `driveSendSide()` then gives the send up with `abandonFailedSend()` instead of failing the request
(again unless `closedLocally()`), and the read on the next cycle returns the response, or EOF and the send error.
In `SendState::ABANDONED` nothing more is sent and, since the request is incomplete, the connection is closed after
the response, as with `abandonBufferedSendAndDeliver()`.

### Streamed request bodies

A streamed (chunked) request - `sendWithSendCallback()`, `sendChunked()` with an `InputStream`, the
`pushSendData()` API - follows the same rules: its first write is its header block, so `handleSending()` applies
the rule above to it, and `driveSendSide()` applies it to the write of a chunk or of the final chunk.  Once the send
is abandoned, the chunks the application still pushes are queued and discarded with the queue when the exchange
ends (`resetSendState()`); the application's wait for the response then returns the response.

### TLS

Over TLS, a failed write raised `SOCKET-SSL-ERROR` without an errno, and `doNonBlockingIo()` closed the socket,
discarding the response in the kernel's receive buffer.  OpenSSL 3 queues the errno of a failed transport call as
an `ERR_LIB_SYS` entry; `handleErrorIntern()` now raises it as the exception argument (also for the empty-queue case
with a socket errno), and `doNonBlockingIo()` leaves the socket open after a write that failed with a peer-close
errno (`SslCall::Write`).  OpenSSL raises no alert for a failed transport write, so the read side stays usable:
`SSL_read()` returns the response records, then the peer's `close_notify` (end of data) or the reset (an error,
reported as a truncated response if a response had started, else as the send error).  The socket is closed by its
owner afterwards, as a plain socket's is.

### A response that has started

`send_failure_pending` is cleared as soon as a final response header arrives (`handleRecvHeader()`; a
`100 Continue` is only a promise of a response).  From then on a failure is the response's: a body cut short by the
reset is reported as the truncated response (`SOCKET-CLOSED` and the like), not as the send error, whether the
response is buffered or streamed to the caller.

### Thread confinement

`send_failure_pending`, `send_failure_err` and `send_failure_desc` belong to the I/O thread (the thread driving
`continuePoll()`).  `setError()`, which substitutes the send error, runs only there; `cancelStream()` runs in the
caller's thread and publishes its error with `publishError()`, which never touches those members.

### Tests

How early the peer's close arrives depends on how fast the peer's own socket close completes, which is why a
change to the cost of `Socket` close (see `async-socket-io.md`) moves this race.  `repeatedEarlyFaultTest` in
`HttpClientBufferedDuplex.qtest` repeats the race; the `EarlyAnswerPeer` cases force the failed write
deterministically: the request header block is larger than the socket buffers and the peer answers after reading
1 KiB of it, then resets the connection (with or without a FIN first), so the write fails before the header block
is out on every platform.  `HttpClientEarlyResponse.qtest` covers TLS, streamed bodies, truncated responses,
cancels and reused connections, using debug hooks (`ql_debug.cpp`, DEBUG builds only) to force each interleaving
without timers:

- `dbg_fail_http1_send_side()` stops the I/O thread between the receive side and the send side
  (`QoreDbgHttp1SendEvent::BeforeSendSide`), lets the peer answer and reset, and waits for `POLLHUP`/`POLLERR`
  (`poll()` with no events requested, no timeout), so the write that follows fails;
- `dbg_hold_http1_send_failure_recovery()` holds the I/O thread where it starts to read the response after a failed
  send until the operation publishes an error, as a cancel from the caller's thread does;
- `dbg_hold_http1_socket_send()` holds a write in `SocketSendPollOperation::continuePoll()`, with the socket's lock
  held, until the connection is shut down or reset, so a cancel's shutdown or a peer's reset lands before the write;
- `dbg_http1_send_failure_result()` returns the number of recoveries and the first published error.

Only a response that is on the wire before the peer's reset can be delivered: a peer that closes with request data
unread resets the connection, and the reset discards whatever the peer has not yet transmitted.  A client that is
not reading while it writes advertises a receive window of about 64 KiB, so a fault response larger than that can
be cut short whatever the client does; the early-fault tests therefore use a 2 KiB body, like a real `413`.

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
framing, an early fault from a peer that stops reading, a fault answered and followed by a reset while
the request header block is still being written (and a reset with no answer, which reports the send error),
`100 Continue` mid-upload, peer EOF,
request timeout, caller cancellation, same-client recovery after each of those, and a small
request that must still go out in one write and leave the connection poolable.

The chunked full-duplex path keeps its own acceptance gate in
`examples/test/qlib/HttpClientIo/HttpClientIo.qtest` (`FullDuplexEchoRawServer`).
