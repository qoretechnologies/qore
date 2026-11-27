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
    operation closes the connection after its response unless a request has arrived;
  - 2, set by startDrain() on connections that wait for a request when the drain starts, and on an HTTP/1.x
    connection registered after the drain took its snapshot (accepted just before the listening socket closed, or
    whose TLS handshake ended during the drain; drainNewH1Connection(), qore#7438): in addition, an operation that
    waits for a request ends at once if none has arrived (a read-header operation that has received no byte ends,
    and the controller closes the connection); a request whose bytes have arrived is read and served.
  A new connection is drained whether it was in the snapshot or not: one that never sends a request does not hold the
  drain until its read times out.
- The end of a dispatch is called on each exit of the dispatch functions (an on_exit block cost every request
  ~146k instructions in the AOT module, qore#5927).
- HttpServer refuses, with 503 and `Connection: close`, a protocol upgrade, a CONNECT (tunnel or extended CONNECT)
  or an event stream that a handler starts while the server stops.
- Measured cost (design/benchmarks/5587, release): +0.8% per keep-alive request, +0.7% per new connection; accepted
  by David.
- Measured cost of the per-listener drain (qore#7692; design/benchmarks/5587 server, callgrind Ir per request, release
  build, equal-length trees, N=1001, min over environment paddings 1/37/211/997/2003; accepted by David, 2026-11-26):

  | Benchmark | Base | qore#7692 | Delta |
  |---|---|---|---|
  | keep-alive | 5,047,400 | 5,050,237 | +2.8k (+0.06%) |
  | new connection | 6,449,720 | 6,474,192 | +24.5k (+0.38%) |
  | keep-alive, inline handler | 1,801,403 | 1,813,596 | +12.2k (+0.68%) |

  The remainder is the choice of the listener's dispatch Counter on each request (the read of
  `HttpListenerInterface::dispatch_counter` from the request's parameter, which takes the object's lock, and the
  reference-cycle scan of the Counter's reference that a request holds), the tests of `draining` and
  `have_removed_listeners` and their frame slots in `handleHeaderComplete()`, `handleBodyComplete()` and
  `handleRequestResult()`, and, for a new connection, the registration guards and drain checks in
  `handleAcceptComplete()`; the inline path is short, so the same fixed cost weighs more there. Removing it needs a
  restructured hot path (no per-listener count of the dispatches, or a Counter that costs nothing per request).
- qore#7509 (B', a request holds its server until its response has been handed to the I/O; HttpServer.qm,
  HttpServerAsyncIo): callgrind, Release build, instructions per request, (Ir(300) - Ir(100)) / 200 of one process
  that starts an HttpServer with a plain 200 handler and sends 100 or 300 sequential GET requests over one keep-alive
  HTTPClient connection; base: HttpServer.qm, HttpServerAsyncIo.qm and HttpAsyncSocketIoController.qc of
  1ffd8e79fc (before B') in the same build:
  - server side only (collected in HttpServerNativeAsyncTarget::onHttpRequest() / tryInlineRequest() and
    HttpAsyncSocketIoController::handleRequestResult(), --toggle-collect): base 4,877,667, head 4,926,247
    (+48.6K, +1.0%); two head runs differ by 1.5K;
  - whole process (client included): base 6,891,580 / 6,897,170, head +48.8K (+0.7%);
  - the controller and the new HttpRequestResultInfo::request_hold field alone cost 1K to 7K; the rest is in the
    HttpServer.qm request path, spread over many small runtime calls (local variable handling, member lookups by
    name), not attributed; accepted by David, the per-request cost is followed up in qoretechnologies/qore#7765.
- Measured cost of the HTTP/1.1 dispatch runner (qore#7769; same method, 2026-11-27): about +2.3k Ir per request
  (+0.04%), from `runDispatchBody()` (+211 exclusive over `runHandlerBody()`) and its frame (type resolution, cleanup);
  the submission is unchanged. That is below the spread between two trees of the same build (an A/A pair: 2-4k), and
  with the trees swapped the keep-alive minimum was 1.9k lower with qore#7769. Single minima differ by up to 25k with
  the layout (malloc and reference-cycle scan), and the same build by about 14k between sessions: a comparison is
  valid in one session only, with a swap or an A/A pair as its control. Follow-ups: qore#7791 (`remove` of a local is
  not lowered natively by AOT, which made a transfer of the work item cost about +15k Ir per request) and qore#7792
  (the submitter's references release a pool task's body on the submitting thread when the worker ends first).

## Per-listener drain (qore#7692)

The stop of one listener (`HttpServer::stopListenerID()`, `HttpListener::stop()`) drains the connections of that
listener, and of its paired QUIC listener, with the same mechanism as the server's drain, while the other listeners
keep serving.

### Controller state

- Each registered listener has an `HttpListenerDrainState` (`lds`) in `listener_states` (keyed like `listeners`):
  `listener_obj` (the `listener` member of its connections' entries), `dispatch_count` (the HTTP/1.1 requests of the
  listener in handlers), `drain_conns` (the connections that the listener's drain waits for), `draining`, `closed`,
  `deadline_us`, `exempt` and, for an HTTP/3 listener, `h3_token`.
- Neither the listeners' registrations nor the connections' entries carry anything for it: an entry is copied on every
  request, and each member costs every copy. A connection's state is found on the paths of a drain only, by its
  `listener` member (`stateOfListenerUnlocked()`), or for a WebSocket connection, whose member is the listener's view
  for its handlers, by `conn_states{uh}` (`stateOfEntryUnlocked()`).
- `draining` means "some drain runs" (`server_draining || listener_drains > 0`). Every write of an entry still tests
  only this flag (`draining ? withDrainToken(uh, ci) : ci`); `withDrainToken()` first returns the entry unchanged
  unless the server or the entry's listener drains. The places that decide the HTTP/1.1 close
  (`handleHeaderComplete()`, `handleBodyComplete()`, `handleRequestResult()` through `closesAfterResult()`,
  `addIdleConnection()`, the registrations that drain a new connection) test `draining` first and look the listener up
  only then.
- A drain token (`HttpDrainToken`) holds a list of Counters: that of the server's drain, that of the listener's drain,
  or both. A drain that starts while a connection has a token from the other drain adds its Counter to the token,
  which every copy of the entry shares.
- An HTTP/1.1 dispatch is counted by its listener's Counter, which the listener gives
  (the member `HttpListenerInterface::dispatch_counter`, read without a method call; `HttpListener` has its own, its
  handlers' view has none): `(cinfo.listener ? cinfo.listener.dispatch_counter : NOTHING) ?? dispatch_count`, read
  from the parameter rather than from the locals that the request's closure captures, whose access costs more. The
  server's
  `waitDrain()` waits for its own Counter and for the `dispatch_count` of every listener, then for `drain_conns`.
  HTTP/2 and HTTP/3 requests are not counted: their connection or session is open while a stream is, which the
  connection drain covers.

### Stop sequence

`HttpServer::stopListenerID()`, `stopListener()` and `stopListenerIDNoWait()` check and reserve the listener under
`lm` (`stop_reserved`: listener ID -> Counter, which the `protect_last_listener` check excludes) and run the stop
without it, as the stop waits for requests whose handlers can call the server; `removeHandlerFromListenerID()`
reserves and stops a listener left without handlers after releasing `lm`. A second blocking call for a reserved
listener waits for the reservation's Counter, which `listenerStopped()` decrements (in an `on_exit` block, also when
it raises an exception), unless it runs in a request of the listener or of its QUIC partner
(`HttpListener::isRequestOfListener()`: the server's request helper, or the controller's
`isCallerRequestOf()`, which checks `running_dispatch` and `running_stream_conn` and so covers inline requests,
request preparation and the code that runs after a handler returned), or the calling thread held `lm` already (code
that the server's `stop()` runs for a listener under `lm`). Every stop of a listener is reserved in
`stopListenerAccepting()`, however it started, so a later blocking stop waits for it. The server refuses upgrades on a
draining listener while `listeners_draining` is set (`draining_lids`, filled in `stopListenerAccepting()` and emptied in
`listenerStopped()`), so a reply pays one member read when no listener stops.

`HttpListener::stop()` calls the async I/O controller directly (it is given the controller, weakly, and the server's
`drain_timeout` when it is registered: `setDrainController()`; the TCP listener is given its paired QUIC listener:
`setQuicDrainPartner()`); its only calls to the server are `stopListenerAccepting()`, `removeAsyncListener()` and
`listenerStopped()`, through `serverCall()`. Each step is wrapped: an error is logged and the next step runs.

1. `stopAccepting()` (qore#7690): the accept operations are canceled and the listening socket closed; an HTTP/3
   listener sends its sessions a GOAWAY notice, and gets its `h3_token` before its operation can end.
2. `startListenerDrain(listener, drain_timeout)` for the listener and its QUIC partner (with `drain_timeout > 0`):
   `lds.draining` is set and the deadline taken; `takeDrainSnapshotUnlocked(lds, lds.drain_conns)` gives each
   connection of the listener a token and the same per-state marks as the server's drain (idle connections closed,
   keep-alive operations woken, HTTP/2 GOAWAY); the persistent handler sessions of the connections are told to end.
   The listener is marked as draining (`HttpListener::isDraining()`), so that the server refuses an upgrade, a CONNECT
   or an event stream started on it.
3. The dedicated connections of the listener are closed with their closing handshake, and its streaming handlers are
   told to stop.
4. `waitListenerDrain()` for both listeners: `lds.dispatch_count`, then `lds.drain_conns`, until the deadline. A wait
   in an HTTP/1.1 request of the listener ends that request's own count first (`endOwnDispatch()`, as in qore#6508);
   a wait in an HTTP/2 or HTTP/3 request (its pool task sets the thread-local `running_stream_conn`) exempts the
   request's connection (`HttpDrainToken::exempt()`; for HTTP/3 the listener's `h3_token` is released), which is
   then closed with the others, so its own response is not sent. A listener that is no longer registered (the
   controller stopped) counts as drained.
5. `closeListenerConnections()`: for a TCP listener, `flushCallbacksByOwner(lhash)` first waits for the accept
   completions in progress (and discards those not started), so no accept registers a connection after the close.
   On a callback-dispatcher worker (a stop in an inline request) `flushCallbacksByOwner()` returns at once, as a
   worker never blocks there; the guard is then `acceptRefusedUnlocked()`: `handleAcceptComplete()` refuses (closes
   in its own thread) a connection of a listener whose `lds.closed` is set, which can happen after the stop returned.
   Then `lds.closed` is set, the listener's drain ends (`lds.draining` cleared, `listener_drains` decremented) and the
   listener is recorded in `removed_listeners`, so that a request that returns later closes its connection
   (`handleRequestResult()`) and a connection submitted later is canceled (`submitConnectionOpIntern()`, qore#5991); a
   request whose header completes later is not dispatched or re-registered for its body. The entries of the
   listener are removed, HTTP/2 operations get a GOAWAY and are aborted, the HTTP/3 operation is aborted (its
   sessions get a CONNECTION_CLOSE), the operations are canceled and the sockets closed in this thread.
6. `waitListenerDispatches()`: the HTTP/1.1 requests whose handlers still run end; then `closeListenerConnections()`
   again closes the connections that their results registered.
7. The QUIC partner is stopped (`drain_partner.stop()`), outside the server's `lm`: its own stop finds its drain
   started (`startListenerDrain()` keeps the deadline), closes its sessions and waits for its requests; the server's
   notification (`listenerStopped()`, under `lm`) later finds it stopped.
8. `cThreads.wait()`, then the qore#7690 tail: the listener is removed from the controller, its socket closed, its
   dedicated connections stopped, and the server notified.

An HTTP/1.1 dispatch that is handed to the handler pool always ends its count, with no cost on the success path
(qore#7769):
- every H1 error path ends the dispatch last, after the target's `onError()` or `cancelPreparedHttpRequest()` and
  the close of the connection (`endFailedDispatch()`, `endAbortedDispatch()`, `endUnsubmittedDispatch()`), with the
  dispatch set as the one that the thread runs meanwhile (`running_dispatch`), so that a drain that such a callback
  waits for ends it; their steps do not depend on each other.
- the dispatch is submitted with a runner of its own (`submitDispatchToHandlerPool()`, `dispatchTaskRunner()`,
  `runDispatchPoolTask()`, `runDispatchBody()`), whose success path has the shape of `runHandlerBody()`: the same
  handoff and work item, and a `try` around `saveDispatchContext()` only. When it raises an exception, the body is
  called with the thread-local `aborting_dispatch` set: the body reads it first and ends its dispatch
  (`endAbortedDispatch()`: the target's preparation is canceled and the connection closed, then the count ends, so
  that a stop of the listener returns only after the close; the request is not handled); the
  exception is then raised as before. Nothing is carried in the work item (a member or a key in it was measured at
  +18.5k and +5.5k Ir per pool request).
- a task that is still queued when the controller stops is run: the controller stops its pool with `stopWait()`,
  whose manager gives every queued task to a worker and, when no worker can be started (a thread limit), waits and
  retries instead of cancelling (`ThreadPool::worker()` leaves its loop on a failed start only with `stopflag`, which
  `stopWait()` does not set). A pool cancels its queued tasks only when it is stopped with `stop()`, or destroyed
  with tasks queued (`stopFromOwnWorker()`, the destructor). The controller's `stop()` ends the counts that are left
  after `stopWait()` (`endStaleDispatches()`), which covers a `stop()` from outside. One branch remains, under a
  thread limit only: when the controller is stopped in a task of its own pool and `background stop_pool()` cannot
  start its thread, the pool is not stopped and keeps running its tasks until it is destroyed (`start()` replacing
  it, or the destruction of the controller); its destructor then cancels the tasks still queued, whose counts are not
  ended.
- the HTTP/2, HTTP/3, SSE and WebSocket bodies of a task whose target's `saveDispatchContext()` raises an exception
  are skipped as before; their connections are counted by the drain, which waits for them until the drain timeout
  (not a hang). They are outside the scope of qore#7769.

The HTTP/2 and HTTP/3 tasks set `running_stream_conn` and clear it in their existing cleanup.

`removeListener()` also ends the drain of the listener (`listener_drains` is decremented and `draining` recomputed) and
releases its `h3_token`; `stop()` of the controller releases every `h3_token` and resets the flags. An HTTP/3
listener's `h3_token` is released by `endH3Drain()` when its operation ends, which runs while some drain runs or an
HTTP/3 listener stopped accepting (`h3_stop_tokens`).

### Test hooks

`HttpListener::dbgSetStopPhaseHook()` (debug builds of the library) reports `drain-started`, `drain-ended` or
`drain-timeout`, `closing-connections`, `connections-closed` and `stopped`;
`HttpAsyncSocketIoController::dbgSetAcceptHook()` holds an accept completion before its registration;
`HttpAsyncSocketIoController::getListenerConnectionCount()` counts the entries of a listener.
`examples/test/qlib/HttpServer/HttpServerListenerStopDrain.qtest` holds a request's result after its handler returned (a
wrapped async target) to show the response is delivered whole over HTTP/1.1, HTTP/2 and HTTP/3.
