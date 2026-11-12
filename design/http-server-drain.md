# HttpServer graceful drain on stop (qore#5899)

Status: implemented (branch feat/5899-stop-drain); David chose option A (see "Decision" below). The sections before
"Refined mechanism" are the original proposal; "Refined mechanism" and "Implementation notes" describe the code.

## Goal

When a server is stopped with drain, it:
- stops accepting;
- tells every client the connection is closing (HTTP/2 and HTTP/3: GOAWAY; HTTP/1.1: `Connection: close`);
- lets accepted requests finish, bounded by `drain_timeout`;
- then runs the current teardown, which aborts anything still running.

The semantics are the same for HTTP/1.1, HTTP/2 and HTTP/3. The wait is event-driven, with no polling or periodic
timeouts. A stop without drain keeps today's immediate teardown (GOAWAY followed by abort; see 82bfa48d4a).

## Why count connections, not requests

The controller does not track requests in flight, and the places where a request ends cannot all be observed from
Qore:
- **HTTP/1.1:** the fused send+idle+read operations, and fast routes answered on the I/O thread, never report that a
  send finished.
- **HTTP/2 and HTTP/3:** send completion is visible only through the opt-in response-send watch.

Adding per-request tracking to every path would touch every hot path, which is not acceptable for a stop-only
feature.

Instead, during a drain each protocol closes a connection as soon as it has no work left. The drain is complete
when every connection that existed at drain start has closed. Connection close is already reported to the controller
on every path (`handleConnectionClosed`, `handleHttp2ConnectionClosed`, lingering close, and the H3 session
lifecycle events). No hot path gains a per-request cost; only code that already runs at a close or state change
checks a drain flag.

## Mechanism

### Controller (HttpAsyncSocketIoController)

`startDrain(timeout drain_timeout)`:
1. Stop accepting: cancel the accept operations and close the listen sockets, using the existing `removeListener`
   logic without cancelling connections.
2. Mark `draining = True`. New requests on existing HTTP/1.1 connections are still served, but answered with close
   (see below).
3. Snapshot the connections and set a `Counter drain_pending` to their number. Each H3 session counts as one.
4. Per protocol:
   - **HTTP/1.1 idle** (KeepAlive state): close now. `addIdleConnection` already closes when `stopping`; this
     extends it to `draining`.
   - **HTTP/1.1 busy:** the response is sent with `Connection: close`, then the connection lingering-closes. Set
     `info.close` in `handleHeaderComplete`/`handleBodyComplete` while draining; that already disables the fused
     path. For a connection already inside a fused operation or a fast route, a C++ per-socket "close after this
     response" flag makes the operation finish with close instead of going idle (new; see C++ below).
   - **HTTP/2:** GOAWAY(NO_ERROR, last processed stream). The 82bfa48d4a path is reused, sent at drain start rather
     than at abort. The operation is put in drain mode, in which it closes the connection when the session's active
     stream count reaches 0. This is checked after each frame is processed or sent, on the I/O thread. A connection
     with no active streams closes at once.
   - **HTTP/3:** `Http3ServerPollOperation::shutdown()` is reworked:
     - Every session gets a GOAWAY notice.
     - Each session closes (final GOAWAY + CONNECTION_CLOSE) when its last request stream ends, and reports a session
       close event.
     - The listener operation ends when its last session closes.
     - The time-based `drain_start` check in `handleDraining` is removed: the controller's deadline replaces it.
     - A request arriving while draining no longer cancels the drain.
   - **Long-lived streams** (SSE, WebSocket, CONNECT tunnels, persistent queues): these have no end a drain could wait
     for. They are closed at drain start with their protocol's closing handshake, as `closeDedicatedConnections()` /
     `signalStreamingHandlersToStop()` do today, and are not counted.
   - **Delayed responses:** stay in flight. Their connection is counted and closes after the delayed response, which
     today's stop would drop.
5. Each close callback for a counted connection calls `drain_pending.dec()`, once per connection; closes are recorded
   in a set keyed by the connection hash.

`waitDrain(timeout)` returns `drain_pending.waitForZero(timeout)`. The Counter wait is woken by the last close, so
there is no polling.

### C++

1. **Http2PollOperationBase drain mode:** a `startDrain()` method sets a flag under op_lock. `continuePoll` closes the
   connection (state CLOSED, normal close path) once the flag is set, the GOAWAY is flushed, and
   `Http2Session::getActiveStreamCount() == 0`. The check is cheap, and only runs when the flag is set.
2. **Http3ServerPollOperation drain rework:** described above; needs per-session tracking of open request streams,
   which QuicSession already has for stream cleanup.
3. **HTTP/1.1 close after response:** a flag on the fused send/idle operations and the fast-route path. When set,
   the operation completes with close after the response instead of going idle. Set through the socket so that an
   operation already in progress sees it.

### HttpServer

- New option `drain_timeout` (default 5s). `quic_drain_timeout` stays as a deprecated alias, as prof confirmed.
- Drain entry points per the API decision. In every variant, the teardown after the drain is the current `stop()`
  sequence.

## Tests (fail first)

For each of HTTP/1.1 keep-alive, HTTP/2 and HTTP/3:
- A request in flight at stop completes with its full response.
- A new request after the drain starts is refused (HTTP/1.1: connection closed; HTTP/2: stream above last_sid
  refused; HTTP/3: GOAWAY).
- Idle connections close at once.
- A handler that never returns is cut off at `drain_timeout`.
- Stop without drain stays immediate.

All tests synchronize on events (PING/ACK, handler-entered queues), with no sleeps.

Tests whose expectations change:
- `HttpServerH3Streaming` `testH3ShutdownDuringStreaming`: stop() < 5s mid-upload. It now takes the drain timeout, or
  uses stop without drain.
- `HttpServerAsyncIo` `stopWithInFlightHandlersTest`.
- `HttpServerAsyncHttp2Multiplexed` shutdown tests.
- `HttpServerBoundedShutdown`.

## Open questions (David)

1. API: stop() drains and stopNoWait() is immediate, plus a non-blocking startDrain() (prof's recommendation); or
   option A (stopNoWait() starts the drain in the background, and immediate close is stop(False)).
2. The HTTP/1.1 fast-route and fused-operation flag is the largest C++ piece. Alternative: during a drain, route all
   HTTP/1.1 requests through the non-fused path, which the drain flag already does for new requests, and accept
   that a request already inside a fused operation goes idle and is then closed by the idle close. This is simpler
   and probably enough: that request's response is already being sent when the drain starts.

## Review requirements (prof, 2026-11-12)

1. The HTTP/1.1 close-after-response flag must cost nothing measurable on the hot path. It is read only where the
   response is finalized, or from connection state the path already loads. Measure with callgrind on the HttpServer
   request benchmarks (design/benchmarks/5587, as in #5759) before and after; no regression is allowed.
2. Long-lived streams at drain start:
   - WebSocket: a close frame with status 1001 (going away), and the peer gets the close handshake within the drain
     timeout, not an abrupt socket close.
   - SSE: the stream ends cleanly.
   - CONNECT tunnels: closed.
3. The PR notes list each changed stop-test expectation, with the reason.

## Decision (David, 2026-11-12): option A

- `stop()` drains: it stops accepting, sends GOAWAY on HTTP/2 and HTTP/3, sends `Connection: close` on HTTP/1.1 and
  closes idle connections, waits event-driven for at most `drain_timeout` (default 5s), then tears down.
- `stopNoWait()` starts the same drain in the background; `waitStop(t)` bounds it. The Qorus shutdown
  (`stopNoWait()` + `waitStop(t)`) keeps working unchanged, verified with a Qorus-style shutdown test.
- `stopImmediate()` is today's immediate stop (GOAWAY, then abort). A separate method is used rather than a boolean
  argument, to match the existing stop()/stopNoWait()/waitStop() naming.
- `drain_timeout` replaces `quic_drain_timeout`; the old name stays as a deprecated alias.
- Docs and a release note describe the behaviour change.

## Refined mechanism (implementation)

Removing a connection from `connections` does not mean it has finished:
- an HTTP/1.1 connection is removed while its handler runs and re-added for the send;
- some state transitions remove an entry and re-add it.

So the drain counts with deterministic objects instead:

- **Connection tokens.** At drain start, every entry in `connections` gets a `drain_token`: an object whose
  constructor increments `drain_conns` (a Counter) and whose destructor decrements it. The token is carried in
  `HttpActiveConnectionInfo`. Every transition copies the entry, so the token lives exactly as long as some copy of
  the connection's entry, and it is released when the connection is gone. This is refcount-deterministic, and no
  removal site needs editing; only the few places that build a fresh entry for a connection (h2c upgrade) carry the
  token over.
- **Dispatch counter.** `dispatch_count` (a Counter) is incremented where an HTTP/1.1 request is dispatched (its
  entry is removed from `connections`, under the same lock) and decremented when the dispatch ends: after
  `handleRequestResult()` has re-added or closed the connection, or on a dispatch error. When the decrement happens
  while draining, the re-added entry gets a token first. This covers requests whose handler is running at drain
  start, which are in no entry. Its per-request cost is one Counter inc/dec pair; measure it.
- **Wait.** `dispatch_count.waitForZero(t)` first, then `drain_conns.waitForZero(remaining)`. Both are woken by the
  last decrement. After the first wait no untracked dispatch can exist, because every later dispatch belongs to a
  connection that has a token.
- **Long-lived connections need no exclusion.** WebSocket, SSE and CONNECT connections are closed at drain start
  with their protocol's close (WebSocket 1001), and their tokens are released when they close.
- **HTTP/1.1 close:**
  - while draining, `handleHeaderComplete`/`handleBodyComplete` set `info.close`, giving `Connection: close`;
  - `handleRequestResult` forces `result.close` for requests already past the header;
  - idle connections in `KeepAlive` are closed at drain start;
  - fused send+idle operations and fast routes check a per-socket drain flag (C++) when they reach the idle wait, and
    end with close instead; operations already waiting idle are woken so that they see it.
- **HTTP/2:** `Http2PollOperationBase::startDrain()` (implemented): a GOAWAY, then the operation closes the
  connection when the session has no open stream and nothing left to write. The check reads an atomic flag on each
  read cycle and touches the session only during a drain.
- **HTTP/3:** one entry per listener, so its token is released when the listener operation ends, after its last
  session drains (the shutdown rework).

## Implementation notes (after the reviews)

- Every write of a connection's entry is `connections{uh} @= (draining ? withDrainToken(uh, ci) : ci)`:
  withDrainToken() keeps the token of the entry it replaces, or creates one, and sets the socket's drain flag on a
  connection that carries requests. A server that does not drain pays only the test of the flag.
- HTTP/1.1 close: while draining, the header of a request is marked close (`info.close`), so a handler's own
  response says `Connection: close` too; handleRequestResult() makes every result that would return the connection
  to idle close it, and sets the header of a response that it sends. Fast routes do not answer a draining connection,
  so its requests are answered on this path.
- Idle connections: the socket's drain flag has two levels (Socket::drainHttpConnection(bool idle)):
  - 1, set on every connection written while draining and on one with a request in progress: a keep-alive
    operation closes the connection after its response unless a request has arrived; the first request of a new
    connection (a fast-route listener's fused operation, or after a TLS handshake that ended during the drain) is
    served;
  - 2, set by startDrain() on connections that wait for a request when the drain starts: in addition, an operation
    that waits for a request ends at once if none has arrived (a read-header operation that has received no byte
    ends, and the controller closes the connection).
  Trade-off: a connection written during the drain (accepted just before the listening socket closed, or whose TLS
  handshake ended during the drain) that never sends a request holds the drain until its read times out or the drain
  timeout ends it; the price of serving its first request.
- The end of a dispatch is called on each exit of the dispatch functions (an on_exit block cost every request
  ~146k instructions in the AOT module, qore#5927).
- HttpServer refuses, with 503 and `Connection: close`, a protocol upgrade, a CONNECT (tunnel or extended CONNECT)
  or an event stream that a handler starts while the server stops.
- Measured cost (design/benchmarks/5587, release): +0.8% per keep-alive request, +0.7% per new connection; accepted
  by David.
