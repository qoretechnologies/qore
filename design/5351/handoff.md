# Implementation Handoff: Stream-Scoped Persistent HTTP Sessions (qore #5351)

**Goal:** Replace the *connection-scoped* HTTP persistent-session mechanism with a
*stream-scoped* one on HTTP/2 and HTTP/3. This is a **rip-and-replace** because
**Qorus is the only consumer** of the Qore HTTP-server persistence API — there is
no third-party client to keep compatible, so the wire protocol and both ends can
change together.

The fix is ultimately landed **in Qorus** (the consumer), riding on Qore
server-side support. Both repos change; they must ship together.

| Repo  | Path | Branch/rev when this doc was written |
|-------|------|--------------------------------------|
| Qore  | `/home/david/src/qore/git/qore` | `develop` @ `faf1a2f38` |
| Qorus | `/home/david/src/Qorus/git/qorus` | `develop` @ `88b72e42a` |

> File:line references are accurate as of the revs above. Re-grep before editing —
> these files are race-sensitive and change often.

---

## 1. Why connection-scoping is wrong (the bug)

The persistent session pins a **dedicated handler thread** that holds thread-local
resources (an open DB transaction / `AbstractSQLStatement`) across requests. Today
that binding is **per TCP/QUIC connection**:

- **H1:** connection is serial, so `connection == session`; connection-close ==
  session-end. **Correct** — leave it alone.
- **H2/H3:** one connection multiplexes many independent streams and is **pooled /
  reused**. The connection-close that would end the session on H1 **never happens**.
  Session release then depends on a best-effort end-of-session *request*
  (`Qorus-Connection: End-Persistent`). If that request is lost or misrouted on a
  degraded/pooled connection, the dedicated handler stays parked **forever** and its
  thread-local DB transaction leaks.

Observed downstream: Qorus SqlUtil persistent `omquser` transactions leaked →
datasource-pool exhaustion (this is the same leak class behind the #5339 work; see
`design`/memory `project_quic_persistent_session_leak.md`).

**Root cause:** session scope is bound to the wrong transport unit. The fix is to
bind it to a **single bidirectional stream**, whose full lifecycle (`END_STREAM`,
`RST_STREAM` / `RESET_STREAM`+`STOP_SENDING`, connection-close) is
transport-observable and reliably delivered.

---

## 2. Current architecture (what exists today)

### 2.1 Qore persistence API surface (the contract Qorus implements)

`qlib/HttpServerUtil.qm`, class `AbstractHttpRequestHandler`:

| Method | Line | Purpose |
|--------|------|---------|
| `isPersistent()` | 2631 | thread-local + `_persistent_threads{tid}` map → is this thread pinned? |
| `setPersistent(bool=True)` | 2754 | pin/unpin current thread; installs `staticPersistenceCleanup` thread-resource |
| `clearPersistentThread()` | 2646 | drop `_persistent_threads{tid}` |
| `getPersistentQueueFilter()` | 2673 | `*code<bool(hash)>` — which requests route to the dedicated thread (default: all) |
| `keepPersistentConnectionDedicated()` | 2709 | keep dedicated thread for the **connection** lifetime even when `isPersistent()` toggles per-request (default False) |
| `isPersistentSessionEndRequest(hdr)` | 2735 | handler recognizes its own end-of-session request (default False) |
| `notifyClosed(*code)` | 2779 | register close callback (thread-local) |
| `persistentClosed()` | 2854 | invokes the close callback |
| `getPersistentClosedNotification()` | `RestHandler.qm:650` | optional close callback supplier |
| `signalPersistentSessionClose(uh)` | 1197 (abstract) | force-close a session from outside the thread |

`HttpServer.qm` `HttpListener::signalPersistentSessionClose()` (4877) overrides →
`ctrl.signalPersistentQueuesForConnection(uh)`.

### 2.2 Server dispatch — `qlib/HttpServer.qm`

`handleRequest` post-processing decides whether to go persistent:

- **Multiplexed (H2/H3) branch** `5389–5458`: if `phi.handler.isPersistent()` &&
  `!isDedicated()`:
  - `Queue pq()`; `register_fn(sock_key, pq, phi.handler.getPersistentQueueFilter())`
    (`5404`) — **key = `sock_key` (connection)**.
  - send initial response inline (preserve thread affinity);
  - re-check `isPersistent()` after the response (`5447`);
  - `handleQueuedPersistentConnection(...)` (`5456`).
- **H1 branch** `5459–5503`: same shape, `sock_key = s.uniqueHash()` (`5489`),
  `register_fn(sock_key, pq)`, returns the socket to async I/O (`5497`), then
  `handleQueuedPersistentConnection`.

`handleQueuedPersistentConnection` (`5989–6310`): the dedicated thread loop.
- `entry = pq.get(ttl)` (`6065`).
- `QUEUE-TIMEOUT`: if `!isPersistent()` → exit; **else `continue` forever**
  (`6067–6082`) — the idle reaper deliberately never reaps a still-persistent
  handler. **This is the leak's parking spot.**
- close sentinel: `if (!entry || entry.type == "close") return` (`6088`).
- per request: `isPersistentSessionEndRequest(hdr)` check; `handleRequest()` on the
  dedicated thread; `resumeQueuedPersistentEntry(entry)` (`6203`).
- post request: re-check persistence; exit if cleared and not
  `keepPersistentConnectionDedicated()`.

### 2.3 Controller — `qlib/HttpServerAsyncIo/HttpAsyncSocketIoController.qc`

- `registerPersistentQueue(key, q, filter)` (`601`) → `persistent_queues{key}`.
- `deregisterPersistentQueue(key)` (`613`).
- `pushPersistentQueue(key, hdr, entry, before_push)` (`627`) — routes a request to
  the dedicated thread **iff** `filter(hdr)` passes (`643`). This is the
  N-requests-by-session-id routing.
- `signalPersistentQueuesForConnection(uh)` (`699`), `...ForPrefix(prefix)` (`717`),
  `signalPersistentQueuesToClose()` (`272`).
- **H2 dispatch** (`3685–3697`): `register_persistent_queue` closure calls
  `registerPersistentQueue(key,q,filter)` **and** `dispatch_h2op.registerPersistentSessionQueue(q)`
  — **connection-scoped** C++ binding. `dispatch_stream_id` **is in scope** (`3679`).
  Request routing: `pushPersistentQueue(dispatch_uh, hdr, {type:"request", conn_info,
  stream_id, ...})` (`3732`).
- **H3 dispatch** (`1824–1836`): `registerPersistentQueue(key,q,filter)` **and**
  `h3op.registerPersistentSessionQueue(session_id, q)` — **session-scoped** C++
  binding. `session_id` and `stream_id` both in scope (`1809–1810`).

### 2.4 C++ close-delivery rail (the #5339 rail)

**H2** — `include/qore/intern/QC_Http2PollOperationBase.h` + `lib/QC_Http2PollOperationBase.qpp`:
- State: single `Queue* persistent_session_queue` + `QoreObject* persistent_session_queue_obj`
  (`.h:331–332`) — **one per connection**.
- `registerPersistentSessionQueue(queue, queue_obj, xsink)` (`.qpp:574`) — replaces
  the single binding under `op_lock`.
- `deregisterPersistentSessionQueue(xsink)` (`.qpp:588`).
- `deliverPersistentSessionClose(xsink)` (`.qpp:606`): called from `continueReadPoll`
  (`.qpp:296`, op_lock held). **Fires only on `sock->takeHttp2PeerResetReportsForAsyncPoll()`
  (RST_STREAM)** — early-returns when empty (`618`). **No END_STREAM delivery.** Pushes
  `{"type":"close"}` to the one queue, clears the binding.
- QPP Qore methods: `registerPersistentSessionQueue(Queue)` (`.qpp:1139`),
  `deregisterPersistentSessionQueue()` (`.qpp:1151`).
- `drainStreamQueues` (`.qpp:649`) already detects per-stream end via
  `isHttp2StreamRemoteClosedForAsyncPoll(sid)` / `isHttp2StreamClosedForAsyncPoll(sid)`
  / `!sock->isOpen()` (`730`) and pushes a NOTHING sentinel to the **body** queue —
  this is the END_STREAM signal source to reuse for the persistent rail.

**H3** — `include/qore/intern/QC_Http3ServerPollOperation.h` + `lib/QC_Http3ServerPollOperation.qpp`:
- State: `std::unordered_map<int64_t, Queue*> persistent_session_queues_` keyed by
  **session_id** (`.h:377`); QoreObject refs held in QPP member hash
  `persistent_session_queue_objs` (`.qpp:992,1255,1271,1295,1304`).
- `registerPersistentSessionQueue(int64_t session_id, Queue*)` (`.h:222`),
  `deregisterPersistentSessionQueue(int64_t session_id)` (`.h:233`).
- `deliverSessionLifecycleEvents()` (`.qpp:710`): called from `continuePoll` OnExit
  (op_lock held). Drains `read_op->takeSessionLifecycleEvents(resets, closed)`;
  delivers `{"type":"close"}` per **session** on peer reset or session-close
  (de-duplicated by session). **Per-stream resets collapse to session granularity** —
  this is the H3 thing to make per-stream.
- QPP methods: `registerPersistentSessionQueue(int session_id, Queue)` (`.qpp:1249`),
  `deregisterPersistentSessionQueue(int session_id)` (`.qpp:1289`).
- `stream_queues` is already keyed by composite `"session_id:stream_id"` string
  (`.h:356`) — reuse that key shape for the persistent map.

Lock order (established, do not violate): `op_lock` → `sock->priv->m` → session
`mtx_`. Delivery runs on the I/O thread with `op_lock` already held; do **not**
re-acquire it.

### 2.5 Existing bidi-stream primitives (already present — reuse, don't reinvent)

- **H2 inbound body-queue**: `register_body_queue` (controller `3711`) →
  `Http2PollOperationPriv::registerStreamQueue(stream_id, q)` (`.qpp:428`). Drains DATA
  frames into a Queue; NOTHING sentinel on END_STREAM. Used by gRPC client-streaming/bidi.
- **H2 outbound**: `send_stream_data(*binary, end_stream)` closure (controller `3928`)
  → `Socket::sendHttp2StreamData` + `waitForHttp2StreamDrain` backpressure.
- **H3 inbound**: `h3op.registerStreamQueue(session_id, stream_id, q)` (controller `1775`).
- **H3 outbound**: `send_stream_data` (controller `1782`) →
  `listener.submitQuicStreamData` + `waitForQuicStreamDrain`.
- **Message framing option**: `WebSocketStreamFrameState` (RFC 8441 WS-over-H2,
  `include/qore/intern/WebSocketStreamFrameState.h`) decodes typed message hashes in
  C++ — usable as the per-message envelope for begin/data/commit if you want framed
  messages rather than raw DataStream chunks.

### 2.6 Qorus consumer (the code that actually changes)

**Client — `Classes/Streams.qc`:**
- `AbstractParallelStream::beginTransaction` (`582`): `session_id =
  get_random_bytes(16).toHex()`; `remote.post(uripath, NOTHING, {"Qorus-Connection":
  "Continue-Persistent", "Qorus-Transaction-Id": session_id})`; stored in
  `remote.conn_opts.trans_session_id`.
- Data-stream POST (`1176`): `sendDataStream(..., {"Qorus-Connection":
  "Continue-Persistent", "Qorus-Transaction-Id": trans_session_id})`.
- `endPersistentSession()` (`192`): `post(..., {"Qorus-Connection": "End-Persistent"})`.
- `commit()` (`598`), `rollback()` (`614`).
- Consumers: `DbRemoteSend` (`1026`), `DbRemoteReceive` (`1656`),
  `DbRemoteSelectStream::beginTransaction` (`2485`), `HttpRemoteSendFileStream` (`3021`).

**Server handler — `Classes/AbstractServiceHttpHandler.qc`:**
- `AbstractServiceHttpHandler`: `getPersistentQueueFilter()` (`1608`) → routes by
  `hdr."qorus-connection"`; `keepPersistentConnectionDedicated()` (`1628`) → `True`;
  `isPersistentSessionEndRequest(hdr)` (`1639`) → `qcon == "end-persistent"`.
- `AbstractServiceDataStreamResponseHandler`: same three overrides (`1777`, `1796`, `1807`);
  constructor (`1680`) captures `cx.uctx.persistent_data`, sets `rhdr."Qorus-Connection" =
  "Persistent"`, `registerActiveStream(...)`.

**SqlUtil — `system/sqlutil-v8.0.qsd`:**
- `SqlUtilPersistentDataHelper` (`273`): ctor `dsp.beginTransaction()` (thread-local
  txn); dtor (`289`) rollback logic gated on `dsp.currentThreadInTransaction()`;
  `connectionTerminated()` → `delete self` (`307`).
- `SqlUtilStreamBase` (`322`): captures `ph = cx.uctx.persistent_data`; re-acquires txn
  across requests; `isPersistent()` = `exists ph` (`387`); `getPersistentClosedNotification()`.
- `SqlUtilCommit/Rollback/BeginTransactionOperation` (`433`).

**Service plumbing — `Classes/AbstractQorusCoreService.qc`:**
- `persistenceRegister(code)` (`349`); `getStreamHandler` (`441`) detects
  `qorus-connection` `persistent`/`continue-persistent`, calls `getPersistenceObject`
  (`380`) → `addUserThreadContext`, `setListenerContext(listener, sock_key)` (`412`),
  `cx.uctx = h`.
- `registerActiveStream` (`507`), `stopAllStreams` (`595`).
- Example service: `test/stream-test/stream-test-v1.0.qsd` (`svc_persistence_register`).

**Wire protocol today:** headers `Qorus-Connection: Persistent|Continue-Persistent|End-Persistent`
and `Qorus-Transaction-Id: <hex>`, each on a **separate** request (= separate H2/H3
stream sharing the session id). End-of-session = an `End-Persistent` request.

---

## 3. Target architecture (rip-and-replace)

**Invariant:** a persistent session **is one bidirectional stream**. Open it once;
carry begin/data/commit/rollback as messages on it; closing the stream
(`END_STREAM` half-close, or `RST_STREAM`, or connection-close) ends the session,
deterministically delivered by the transport. **No session-id header, no
End-Persistent request, no per-request routing filter, no idle reaper** for H2/H3.

Why this kills the leak by construction: the dedicated thread's teardown is driven
by a transport event that is *always* delivered (END_STREAM/RST/close), not by a
best-effort application request that can be lost on a pooled connection.

### 3.1 Protocol over one stream

The session stream carries a sequence of typed messages in each direction:

```
client → server:  begin{datasource,opts} · data{chunk}* · (commit | rollback) · END_STREAM
server → client:  ack/result{...}* · final{status}
```

Two viable framings — pick one and use it both directions:
- **(A) DataStream chunks with a small JSON/YAML control envelope per message**
  (`{op:"begin"|"data"|"commit"|"rollback", ...}`). Closest to the existing
  DataStream wire format; least new C++.
- **(B) `WebSocketStreamFrameState` message framing** (typed message hashes decoded
  in C++). Heavier but gives clean message boundaries and an existing decoder.

Recommendation: **(A)** — reuse the DataStream framing already used by
`sendDataStream`/`recvDataStream`, just multiplexed onto one persistent stream
instead of N requests. End-of-session = client half-closes the stream (`END_STREAM`).

### 3.2 Transport selection

- **H2/H3**: full-duplex single stream → the model above. **Primary target.**
- **H1**: HTTP/1.1 is half-duplex on one connection; a true begin/data/commit bidi
  exchange on one stream is not cleanly expressible. **Keep H1 on the existing
  connection-scoped model** (it is already correct: `connection == stream`). The
  client picks the model by negotiated protocol.

So this is **not** "delete the old path entirely" — it is "old path = H1 only; new
stream-scoped path = H2/H3." The Qorus client must branch on the negotiated HTTP
version (it already knows it from the connection).

### 3.3 Thread affinity

Preserved: one dedicated handler thread services the one stream and holds the
thread-local DB transaction. Only the **binding key** moves connection → stream, and
the dedicated loop reads framed messages off the **stream data queue** directly
instead of being re-fed per-request through `pushPersistentQueue`.

---

## 4. Change checklist

### 4.1 Qore — C++ rail re-keyed to stream

**H2** (`QC_Http2PollOperationBase.{h,qpp}`):
1. Replace `persistent_session_queue` / `_obj` with
   `std::unordered_map<int32_t /*stream_id*/, std::pair<Queue*, QoreObject*>>`
   (mirror `stream_queues` keying). `.h:325–335`.
2. `registerPersistentSessionQueue(int32_t stream_id, Queue*, QoreObject*, xsink)` /
   `deregisterPersistentSessionQueue(int32_t stream_id, xsink)` — per-stream insert/erase
   under `op_lock`. `.qpp:574,588`.
3. `deliverPersistentSessionClose`: instead of "any reset → the one queue", iterate
   the per-stream map; for each bound stream_id, deliver `{"type":"close"}` when **that
   stream** ends — union of: `stream_id` ∈ `takeHttp2PeerResetReportsForAsyncPoll()`
   (RST), `isHttp2StreamRemoteClosedForAsyncPoll(stream_id)` (**END_STREAM — the new
   graceful trigger**), `isHttp2StreamClosedForAsyncPoll(stream_id)`, `!sock->isOpen()`
   (conn close). Always drain the reset-report vector even when the map is empty (avoid
   unbounded accumulation). `.qpp:606`.
4. QPP methods take `int stream_id`. `.qpp:1139,1151`.

**H3** (`QC_Http3ServerPollOperation.{h,qpp}`):
1. Re-key `persistent_session_queues_` from `int64_t session_id` to the composite
   `std::string "session_id:stream_id"` (same as `stream_queues`, `.h:356`). `.h:377`.
2. `registerPersistentSessionQueue(int64 session_id, int64 stream_id, Queue*)` /
   `deregister(...)`. `.h:222,233`; QPP `.qpp:1249,1289`.
3. `deliverSessionLifecycleEvents`: deliver per **stream** (the resets vector already
   carries `(session_id, stream_id)` pairs); add END_STREAM (stream remote-close) and
   keep session-close / connection-close (which fan out to all that session's streams).
   `.qpp:710`.

> Build/test/valgrind H2 and H3 after this step (see §6). The existing
> `HttpServerMultiplexedPersistentTeardown.qtest` encodes the **old** connection-scoped
> contract and will need rewriting in §4.4 — expect it to change meaning.

### 4.2 Qore — controller wiring

`qlib/HttpServerAsyncIo/HttpAsyncSocketIoController.qc`:
1. H2 `register_persistent_queue` (`3692`): →
   `dispatch_h2op.registerPersistentSessionQueue(dispatch_stream_id, q)`;
   deregister (`3696`): `...deregisterPersistentSessionQueue(dispatch_stream_id)`.
2. H3 `register_persistent_queue` (`1831`): →
   `h3op.registerPersistentSessionQueue(session_id, stream_id, q)`;
   deregister (`1835`) likewise.
3. The dedicated handler now reads the **stream data queue** for messages; the
   per-request `pushPersistentQueue(...)`/filter routing (H2 `3732`, generic `670`,
   `627`) is **no longer used for H2/H3** — gate it to H1 (§4.4).
4. `registerPersistentQueue` key for H2/H3 becomes per-stream
   (`uh + ":" + stream_id`) so `signalPersistentQueuesForConnection`/`...ForPrefix`
   close the right entry; verify the H3 shared-listener prefix logic (`2645`, `717`)
   still matches.

### 4.3 Qore — server dispatch

`qlib/HttpServer.qm`:
1. Multiplexed branch (`5389–5458`): bind by `(sock_key, stream_id)`; the dedicated
   loop consumes the framed session stream (via the body-queue) rather than re-dispatched
   per-request entries. Expose `cx.sock_key`/`cx.stream_id` accordingly.
2. `handleQueuedPersistentConnection` (`5989`): for H2/H3, drop the idle-TTL
   `continue`-forever branch (`6082`) — teardown is deterministic; the loop exits on the
   stream-end close sentinel (`6088`). Keep the H1 behavior unchanged.

### 4.4 Qore — gate legacy path to H1

1. Keep `getPersistentQueueFilter` routing, `keepPersistentConnectionDedicated`,
   `isPersistentSessionEndRequest`, the connection-scoped #5339 rail, and the idle-TTL
   loop **for H1 only**.
2. Rewrite/replace `examples/test/qlib/HttpServer/HttpServerMultiplexedPersistentTeardown.qtest`
   to assert the **stream-scoped** contract on H2/H3 (END_STREAM / RST / conn-close all
   tear down the bound stream; sibling streams unaffected). Add an H1 case that keeps the
   connection-scoped contract.

### 4.5 Qorus — client (`Classes/Streams.qc`)

1. Branch on negotiated HTTP version:
   - **H2/H3**: open **one** bidi stream for the session (one streaming
     `sendDataStream`/`recvDataStream` exchange held open); send begin/data/commit/rollback
     as framed messages on it; **half-close (END_STREAM) to end the session** instead of
     a separate `End-Persistent` POST. Drop `Qorus-Transaction-Id` and
     `conn_opts.trans_session_id`.
   - **H1**: unchanged legacy flow (Continue-Persistent + Transaction-Id + End-Persistent).
2. Update `beginTransaction` (`582`), data POST (`1176`), `endPersistentSession` (`192`),
   `commit` (`598`), `rollback` (`614`), and `DbRemoteSend`/`DbRemoteReceive`/
   `DbRemoteSelectStream`/`HttpRemoteSendFileStream` accordingly.

### 4.6 Qorus — server handler & service plumbing

1. `Classes/AbstractServiceHttpHandler.qc`: for H2/H3 the data-stream handler reads
   begin/data/commit/rollback from the single stream; `getPersistentQueueFilter` /
   `keepPersistentConnectionDedicated` / `isPersistentSessionEndRequest` overrides become
   **H1-only** (or removed if H1 persistence is dropped for these handlers).
2. `Classes/AbstractQorusCoreService.qc`: `getStreamHandler` (`441`) detects the session
   from **stream presence**, not the `qorus-connection` header; `setListenerContext`
   (`412`) carries `(listener, stream_key)`.

### 4.7 Qorus — SqlUtil (`system/sqlutil-v8.0.qsd`)

1. `SqlUtilPersistentDataHelper` lifetime tied to the **stream**: the dtor (`289`) still
   runs on the binding (dedicated) thread when the stream ends → existing rollback/commit
   logic with `currentThreadInTransaction()` works **unchanged** (this is the whole point
   — teardown now reliably fires). `connectionTerminated()` (`307`) → "stream terminated".
2. `SqlUtilStreamBase` (`322`): no longer re-acquires a txn per request; the single stream
   holds it for the session.
3. Keep `SqlUtilCommit/Rollback/BeginTransactionOperation` as the in-stream message ops.

---

## 5. Compatibility & rollout

- Wire protocol changes on H2/H3 → **new client and new server must ship together**.
  Because Qorus is the only consumer this is acceptable, **but** mixed-version Qorus
  clusters and `qrest` remote connections between old↔new nodes will not interoperate
  over H2/H3. Mitigation: client negotiates — fall back to the legacy
  Continue-Persistent flow if the server doesn't advertise stream-scoped support (a
  capability header on the listener, or simply: legacy on H1, new on H2/H3 with a
  version check). Decide the negotiation handshake early.
- H1 path is unchanged, so any H1 client keeps working.

---

## 6. Build / test / valgrind (Qore)

Match the install prefix (`which qore` → `/usr/bin/qore` → prefix `/usr`):

```
cd /home/david/src/qore/git/qore
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release   # build/  = Release (installed)
cmake -S . -B build-debug -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target <target> -- -B
```

- Run tests with `qore --enable-debug`; tests must use `%prepend-module-path ./qlib`
  (or relative `%requires`) so the **local** modules load, not installed Qorus ones.
- After C++ changes, **valgrind** affected tests once at the end (use `qore -b` to
  disable signals); `LD_LIBRARY_PATH=build-debug build-debug/qore` for the debug build.
- Relevant suites: `HttpServer`, `HttpServerAsyncHttp2`, `AsyncHttp2Streaming`,
  `AsyncHttp2Multiplexed`, `Http2`, `HttpServerAsyncIo`, `HttpClientIo`,
  `QuicLifecycle`, `Http3`, `QuicWs`, `QuicSse`, `Http3Integration`,
  `WebSocketClientAsyncIo`, and the rewritten `HttpServerMultiplexedPersistentTeardown`.
- New tests to add: graceful END_STREAM teardown, RST teardown, connection-close
  teardown (H2 & H3); pooled-connection leak scenario (session ends on stream-end while
  the connection stays open — the core regression); multiplexing isolation (sibling
  stream unaffected); multi-message single-stream session (begin/data/commit).
  **Deterministic waits only** — `Queue.get` on a signal, never poll/sleep.

Qorus: run the stream/SqlUtil persistence tests (e.g. `test/stream-test/...`,
`test/streams.qtest`) against the new local Qore build.

---

## 7. Gotchas (from prior work on this subsystem)

- **Lock order** `op_lock → sock->priv->m → session mtx_`; delivery runs with
  `op_lock` already held — never re-acquire it (it is non-recursive). The H2/H3
  `deliver*` methods already document this.
- **Ref ownership on Queue push**: `Queue::push` takes the ref on success; on error the
  caller must `deref`. Mirror the existing `deliverPersistentSessionClose` /
  `drainStreamQueues` ref dance exactly. Use `ReferenceHolder`/`SimpleRefHolder` — no
  raw `new` + manual deref.
- **Frame-state Queue ownership**: in frame-state mode the `Queue` ref is owned by the
  `WebSocketStreamFrameState` (its dtor derefs); do not double-deref. See
  `drainStreamQueues` `.qpp:754`.
- **Shutdown race**: `stream_queues_closed` short-circuit (`.qpp:474,539`) — a handler
  thread registering after `clearStreamQueues` must push a sentinel directly. Preserve an
  equivalent guard for the persistent-stream registration.
- **Teardown must run on the binding thread**: that is why the close sentinel releases the
  *dedicated* thread (not a cross-thread destroy) — `DatasourcePool` is thread-affine, so
  `currentThreadInTransaction()` is only true on the thread that began the txn. Do not
  "fix" leaks with cross-thread rollback.
- **No `Date.now()`-style polling / TTL reliance** for correctness — the whole point is
  to remove the idle-TTL dependency on H2/H3.
- **`.qmod` shadowing**: editing a qlib `.qm`/`.qc` and running a `.qtest` can silently
  load a stale compiled `.qmod`; rebuild the `<Module>-qmod` cmake targets or set
  `QORE_MODULE_DIR`/`%prepend-module-path` to the source.
- **Debug-only code paths**: CI runs Debug; verify `ql_debug.cpp`-style changes in
  `build-debug` (Release preprocesses some of it away).
- Background: `design/dgc.md` (DGC platform guarantee — store `QoreObject*` refs as
  internal members or expose via `scanMembers`), and memory note
  `project_quic_persistent_session_leak.md` (#5339 connection-scoped rail this
  supersedes for H2/H3).

---

## 8. Suggested phase order (on canoe)

1. **Qore C++ rail** (§4.1) — re-key H2 then H3; build + valgrind in isolation with a
   throwaway unit test that binds a stream queue and asserts close on END_STREAM/RST/close.
2. **Qore controller + dispatch** (§4.2, §4.3) — wire stream_id through; keep it building.
3. **Qore gate-to-H1 + tests** (§4.4, §6) — rewrite the teardown qtest; green the suites.
4. **Qorus server** (§4.6, §4.7) — handler + SqlUtil stream-scoped lifetime.
5. **Qorus client** (§4.5) — single-bidi-stream session + protocol negotiation.
6. **End-to-end** — Qorus stream/SqlUtil persistence tests against the new Qore; confirm
   the leak scenario (pooled connection, abandoned session) tears down deterministically.

Each phase: tests pass without warnings before moving on; valgrind once after the C++
work; audit (the `audit-changes` skill) before each commit; do not push without approval.
```

---

## 9. PROGRESS LOG / ADDENDUM (implementation session 2026-06-18)

### DONE — Qore side (committed on `develop` @ `045234d12f`, NOT pushed, NOT installed)
- Approach refined (confirmed with user): instead of re-keying the #5339 close-delivery
  rail (doc §4.1–§4.3), **gate the connection-scoped persistent machinery to HTTP/1.x**
  and let H2/H3 use the **existing body_streaming / extended-CONNECT bidi primitives**
  (whose `stream_data_queue` NOTHING sentinel already delivers deterministic teardown).
- `qlib/HttpServer.qm`: the multiplexed-persistent dispatch branch no longer parks a
  dedicated thread / registers a persistent queue / enters `handleQueuedPersistentConnection`.
  It sends the initial response inline (preserving one-shot streamed-SELECT thread affinity,
  which is also covered by the normal inline `send_callback`/`CallbackInputStream` path) and,
  if the handler is still `isPersistent()`, tears down its thread-local context
  (`phi.clear()` + `removeUserThreadContext()` + `remove_thread_data("svc_listener")`) so it
  cannot leak. H1 branch unchanged.
- The C++ rail re-key (orig doc §4.1/§4.2) was implemented, built clean, then **REVERTED**:
  unreachable under this approach. The pre-existing #5339 rail is now dead on the gated path
  and is a recommended **follow-up removal** (Http2/Http3 poll-op
  registerPersistentSessionQueue / deliverPersistentSessionClose / deliverSessionLifecycleEvents
  + controller `register_persistent_queue` H2/H3 closures).
- Test rewritten: `examples/test/qlib/HttpServer/HttpServerMultiplexedPersistentTeardown.qtest`
  now asserts the stream-scoped contract (multiplexed `setPersistent()` torn down promptly,
  exactly once, no reset/TTL/close; no resource leak over reused connection; clean destroy).
- Verified green (debug build): HttpServer 61, StreamCallback 37, AsyncHttp2Multiplexed 19,
  AsyncHttp2Streaming 14, StreamPeerReset 4, H3Streaming 24, QuicWs 12, Http2BidiStreaming 5,
  Teardown 6. Teardown valgrind-clean (0 lost, 0 errors).
- **`.qmod` gotcha**: editing `qlib/*.qm` requires rebuilding the qmod target
  (`cmake --build build --target HttpServer-qmod`) or the stale AOT `.qmod` (symlinked into
  qlib/ from build/qlib-qmod/) shadows the source silently.

### CONFIRMED REGRESSION (why Qorus MUST change before installing the new Qore)
With the gating, a `Continue-Persistent` `beginTransaction` over H2/H3 has its persistence
torn down right after the response → `SqlUtilPersistentDataHelper::destructor` runs
`dsp.rollback()` → every subsequent data POST gets a fresh txn, also rolled back. So H2/H3
Qorus DataStream sessions LOSE DATA, not just leak. The new Qore is therefore **not installed**
to avoid breaking the running Qorus.

### KEY LEVERAGE for the Qorus bidi work
- `DataStreamClientIo` (installed at `/usr/local/share/qore-modules/DataStreamClientIo.qm`)
  already supports a **`protocol: "auto"|"connect"|"datastream"`** option and tracks
  `m_active_streams` as `hash<string, HttpClientIo::HttpClientStreamHandle>` — i.e. a
  single-bidirectional-stream (extended-CONNECT) mode ALREADY EXISTS client-side. The session
  object should be built on this, NOT on a hand-rolled stream.
- Qore server already supports bidi via `AbstractHttpSocketHandler` (extended CONNECT,
  `startImpl` reads `cx."header-info".stream_data_queue`, writes `send_stream_data`) — proven
  by `Http2BidiStreaming.qtest` and the QUIC WS tests. The SqlUtil session handler should be
  such a handler, holding the `dsp` txn for the stream lifetime.

### REMAINING — Qorus bidi refactor (NOT started; spans repos)
Target: a persistent transaction = ONE bidirectional stream (extended-CONNECT) that holds the
`dsp` txn for the stream lifetime; framed messages carry begin/op/data/commit/rollback; END_STREAM
or reset ends it deterministically. Capability negotiation: server advertises support (response
header e.g. `Qorus-Persistent-Model: stream`); client uses bidi on H2/H3 when advertised, else
falls back to the legacy H1 connection-scoped flow (retained + correct in Qore).
- **Codebases involved**: Qorus (`Classes/Streams.qc`, `system/sqlutil-v8.0.qsd`,
  `Classes/AbstractServiceHttpHandler.qc`, `Classes/AbstractQorusCoreService.qc`); the
  **DataStream module repo** (DataStreamClientIo/DataStreamRequestHandler — installed-only,
  needs checkout if its connect-mode needs extension); possibly RestClientIo/HttpClientIo.
- **Client (Streams.qc)**: a session object holding one `protocol:"connect"` bidi stream; rewire
  `DbRemoteSend`/`DbRemoteReceive`/`DbRemoteSelectStream`/`HttpRemoteSendFileStream` to write
  framed ops/data into it and read framed results; `beginTransaction`/`commit`/`rollback` become
  open/finalize/close of the one stream; drop `Qorus-Transaction-Id`/`conn_opts.trans_session_id`
  on H2/H3. Negotiate via capability header; fall back to legacy flow on H1 / unsupported server.
  Hard part: the public multi-object transaction API (begin → many DbRemoteSend across tables →
  commit) must funnel all ops through the one held-open session stream.
- **Server (sqlutil-v8.0.qsd + AbstractServiceHttpHandler.qc)**: a bidi session handler that
  begins the `dsp` txn on the stream, dispatches framed ops to the existing insert/select/commit
  logic on the held txn, and on stream-end commits-if-asked-else-rolls-back + releases the dsp.
  The connection-scoped overrides (`getPersistentQueueFilter`/`keepPersistentConnectionDedicated`/
  `isPersistentSessionEndRequest`) become H1-only. Advertise the capability header.
- **Tests**: `test/streams.qtest`, `test/sqlutil-stream-stop.qtest`,
  `test/sqlutil-stop-persistent-helper.qtest`; e2e leak repro (pooled H2/H3 conn, abandoned
  session, `omquser` txn released) needs a live Qorus + datasource — REQUIRES USER ENVIRONMENT.
- **Build/install/restart**: install the new Qore (`build/`, prefix /usr/local) and the new
  Qorus together; restart Qorus; run the stream tests. Installing the new Qore WITHOUT the new
  Qorus breaks H2/H3 DataStream txns (see regression above).

### 9.1 PIVOTAL FINDING — the bidi primitive already exists (connect mode)
The DataStream protocol already implements a full BIDIRECTIONAL single-stream "connect" mode:
- **Client** (`module-yaml/qlib/DataStreamClientIo.qm`): `protocol: "auto"|"connect"|"datastream"`;
  `sendRecvDataStream(scb, recv_cb, eod_cb, method, path, timeout, \info, hdr)` opens ONE
  full-duplex stream; send callback returns framed YAML msgs (NOTHING => END_STREAM); recv
  callback gets each deserialized msg; eod callback on end. Low-level: `HttpClientStreamHandle`
  (`qore/qlib/HttpClientIo/HttpClientStreamHandle.qc`) `startStreaming`/`sendData(data,end)`/
  `readData(timeout)`/`writesDone()`. `protocol:"auto"` auto-detects + falls back to DSv1
  chunked (which forces H1) — built-in capability negotiation.
- **Server** (`module-yaml/qlib/DataStreamRequestHandler.qm`): `AbstractDataStreamRequestHandler`
  auto-detects connect mode (`connect_detect_protocol(cx.hdr)`); ONE handler instance lives for
  the whole stream on ONE thread (thread-affine — holds a DB txn safely); override
  `recvDataImpl(data)` (per inbound op frame), `sendDataImpl()` (return result frames; NOTHING=end),
  `recvDataDoneImpl(*err)` (commit/rollback + release on END_STREAM/error/reset). Framing = YAML +
  5-byte connect envelope (DataStreamUtil `connect_frame_message`/`connect_unframe_message`).
- **Consequence**: the connect path does NOT use setPersistent/connection-scoped persistence, so
  it is INDEPENDENT of the Qore gating — develop+test against the CURRENT installed Qore; the
  gating only makes the OLD multi-request path safe. The leak/regression is fixed by MOVING
  persistent SqlUtil sessions onto the connect bidi stream (handler instance = session = one stream
  = thread-affine txn, deterministic teardown in recvDataDoneImpl).

### 9.2 Remaining implementation (now tractable, ~Qorus-only)
- Server: a SqlUtil connect-session handler subclass that begins the dsp txn on stream open,
  dispatches each framed op {op:"insert"/"upsert"/"select"/"commit"/"rollback",...} to the existing
  op logic on the held dsp, streams back results, commits/rolls back in recvDataDoneImpl.
- Client: a session object using sendRecvDataStream(protocol auto->connect); funnel begin + N
  table ops + commit through the one stream. Rewire DbRemoteSend/Receive/SelectStream.
- Negotiation: rely on protocol:"auto" connect-detect + DSv1 fallback (H1) for old servers.
- Env: Qorus at /home/david/src/Qorus/test (OMQ_DIR); server not currently running; test deps
  (emqx/kafka) containers up. Tests: test/streams.qtest etc. Install new Qore + new Qorus together.

### 9.3 Concrete meta-protocol + file plan (pure execution remaining)
Wire protocol over ONE connect stream (YAML frames, full-duplex):
  client→server:  {cmd:"begin", datasource, [opts]}
                  {cmd:"op", op:"insert"|"upsert"|"update"|"delete"|"select"|"select_raw",
                             table?, args?}          # starts a sub-op
                  {cmd:"data", rows:[...]}*          # row batches for write ops
                  {cmd:"op_end"}                     # finalize current sub-op
                  ... (repeat op/data/op_end for multiple tables, all in ONE txn) ...
                  {cmd:"commit"} | {cmd:"rollback"}  # then END_STREAM (writesDone)
  server→client:  {status:"ok", op_result:{...}}* per op; {status:"error", err, desc} on failure;
                  for select ops, stream result rows back as {cmd:"data", rows:[...]} frames.

Server (system/sqlutil-v8.0.qsd):
  - New stream op "session" registered via ServiceApi::streamRegister("session","POST",factory).
  - Handler `SqlUtilSessionStream inherits AbstractServiceDataStreamResponseHandler` (connect mode):
    * recvDataImpl(frame): dispatch by frame.cmd. "begin"→ dsp=getDatasourcePool(ds,False);
      dsp.beginTransaction(); hold dsp as a member (thread-affine for the handler's life).
      "op"→ set current op + reuse the existing SqlUtilWriteStream/SqlUtilSelectStream row logic
      against the held dsp. "data"→ feed rows to current op. "op_end"→ finalize. "commit"→
      dsp.commit(); "rollback"→ dsp.rollback().
    * sendDataImpl(): drain a result queue (ok/error frames + select rows); NOTHING when done.
    * recvDataDoneImpl(*err): if still in txn → err? rollback : (committed? noop : rollback).
      This runs on END_STREAM / reset / connection drop → DETERMINISTIC stream-scoped teardown,
      on the SAME thread that began the txn. No setPersistent, no cx.uctx.persistent_data, no
      connection-scoped queue. The handler instance IS the session.
  - Keep the legacy per-op handlers (insert/select/begin/commit/...) for H1/old-client fallback.

Client (Classes/Streams.qc):
  - New `RemoteSqlTransaction` (or extend DbStreamConfig) holding ONE DataStreamClientIo connect
    stream opened to services/sqlutil?action=stream;stream=session;datasource=<ds> via
    sendRecvDataStream (protocol:"auto" → connect when server supports it, else DSv1/H1 fallback).
  - begin → open stream + send {cmd:"begin"}; each DbRemoteSend/DbRemoteReceive routes its rows
    through {cmd:"op"}+{cmd:"data"}*+{cmd:"op_end"} on the held stream; commit/rollback → send
    final frame + writesDone (END_STREAM). Drop Qorus-Transaction-Id/conn_opts.trans_session_id
    on the connect path.
  - Negotiation: protocol:"auto" already detects connect support and falls back to DSv1 chunked
    (forces H1) for old servers → no separate capability header strictly required, but a server
    response header (e.g. Qorus-Persistent-Model: stream) makes it explicit/observable.

Tests:
  - Standalone mechanism test (no Qorus): DataStreamClientIo(protocol:"connect") ↔ a minimal
    AbstractDataStreamRequestHandler session handler over HttpServer, with a FAKE txn resource,
    asserting begin/op/commit round-trips and that recvDataDoneImpl releases the resource on
    END_STREAM AND on client abort/reset (deterministic teardown). Fully runnable without a DB.
  - Qorus integration: test/streams.qtest + a new persistent-session-over-connect case; e2e leak
    repro (pooled H2/H3 conn, abandoned session, omquser txn released) — needs `qctl start`
    (systemdb pgsql `omq`) + datasources.

Env to run Qorus: OMQ_DIR=/home/david/src/Qorus/test; `bin/qctl start` (needs pgsql `omq` up).
Install new Qore (build/, prefix /usr/local) + new Qorus together; restart Qorus.

### 9.4 VERIFIED server-side session (committed Qorus develop d2bbe2b26)
- `system/sqlutil-v8.0.qsd`: `SqlUtilSessionStream inherits AbstractServiceDataStreamResponseHandler`
  + `streamRegister("session", "POST", ...)`. Frames: {cmd:"insert",table,rows[]} · {cmd:"commit"} ·
  {cmd:"rollback"}; lazy `dsp.beginTransaction()` on first insert; `getResponseHeaderMessageImpl`
  returns {status,rows}; `recvDataDoneImpl` rolls back any uncommitted txn (deterministic
  stream-scoped teardown); destructor safety rollback.
- KEY CORRECTNESS FACTS (verified):
  * The connect handler runs ALL frames + constructor + teardown on ONE thread (confirmed via TID
    logs) — so the dsp txn stays thread-affine. NO worker thread / NO ServiceApi::startThread needed
    (and startThread is REJECTED for sqlutil: "service has no 'stop' method").
  * `getDatasourcePool(name)` and `getSqlTable(dsp,name)` resolve to the SAME pool object per thread
    (thread-local pool cache `tld.dspNameCache`), so `dsp.commit()` commits the table inserts —
    committing "via the table" is NOT required (verified: select * shows rows persisted after commit).
  * Test gotcha: `omqservice.system.sqlutil.exec_sql(ds, sql)` returns COLUMN format
    `{col: [v,...]}`, not a list of row hashes — count is `res.cnt[0]`.
- Test harness pattern (test/sqlutil-session-stream.qtest): QorusClient::initFast() +
  getLocalSystemAuthToken() + QorusSystemRestHelperIo({url:qorus_get_local_url(), token});
  table via `omqservice.system.sqlutil.align_schema(ds, {tables:{...}}, {replace:True})`; drive the
  session via a DataStreamSendMessage subclass returning frames + sendDataStream(POST,
  "services/sqlutil?action=stream;stream=session;datasource=omquser"). Run:
  OMQ_DIR=/home/david/src/Qorus/test QORE_MODULE_DIR=.../test/qlib:.../test/user/modules qore <test>.
- Deploy a system-service edit: cp to $OMQ_DIR/system/, then `bin/qctl restart` (service `unload`
  does NOT reload source). Wait for core via `bin/qrest get system`.

### 9.5 REMAINING (client rewiring — the larger piece, NOT started)
- Server: extend SqlUtilSessionStream with upsert/update/delete (mirror insert; write ops, no send
  stream) and select (needs sendDataImpl streaming rows back — full-duplex).
- Client `Classes/Streams.qc`: a session object holding ONE connect stream; funnel
  begin + N table ops + commit through it; rewire DbRemoteSend/DbRemoteReceive/DbRemoteSelectStream/
  HttpRemoteSendFileStream. Hard part: the public multi-object transaction API (begin →
  multiple DbRemoteSend across tables → commit) must share the one held-open session stream.
  Negotiation: protocol:"auto" (connect when supported, DSv1/H1 fallback) or a capability header;
  keep the legacy multi-request flow on HTTP/1.x.
- E2e leak repro: pooled H2/H3 connection, abandoned session, confirm omquser txn released.

### 9.6 CLIENT pattern VALIDATED e2e (committed Qorus develop 277f3fa56)
test/sqlutil-session-stream.qtest now has 5/5 cases incl. two HELD-OPEN session cases proving
the multi-object (DbRemote) transaction shape: a `SessionDriver` (DataStreamSendMessage) holds ONE
`session` stream open on a background thread via `sendDataStream(self, "POST",
services/sqlutil?...stream=session;datasource=...)`, with `sendDataImpl(){ return m_q.get(); }`
blocking for frames pushed incrementally from the caller's thread (`insert(table,rows)` pushes
{cmd:insert,...}; `finish(*cmd)` pushes the optional commit/rollback frame + NOTHING = END_STREAM,
then `m_done.waitForZero()`). Verified: incremental multi-op commit persists; abandon (no commit)
rolls back. So `sendDataStream`'s send-callback model supports a long-lived stream produced
incrementally from another thread — the crux of the client rewiring. SessionDriver IS the reference
RemoteSqlSession implementation.

### 9.7 REMAINING client production wiring (de-risked; needs Qorus client-module rebuild)
QorusClientBase.qm is a built artifact (%include Classes/*.qc); client product changes need a Qorus
build to regenerate it, then `qctl restart`, then test.
Steps:
1. Add `public class RemoteSqlSession` to Classes/Streams.qc = the validated SessionDriver
   (constructor(remote, ds[, timeout]); background ioThread runs sendDataStream(self, session-url);
   sendDataImpl()=m_q.get(); insert/upsert/update/delete(...) push frames; commit()/rollback()
   call finish(cmd); destructor calls finish() so an abandoned txn rolls back). + a
   `RemoteSqlSessionSender` is unnecessary — make RemoteSqlSession itself the DataStreamSendMessage
   (as SessionDriver does).
2. Rewire DbRemoteBase (Classes/Streams.qc:2427): replace the conn_opts.trans_session_id
   connection-scoped model. On H2/H3 (or when the server advertises session support):
   beginTransaction() creates a RemoteSqlSession (held stream); DbRemoteSend/DbRemoteReceive route
   their rows as frames into the shared RemoteSqlSession instead of their own POSTs; commit()/
   rollback() call session.commit()/rollback(). Keep the legacy multi-request flow on HTTP/1.x
   (negotiation: protocol auto-detect / capability, fall back to legacy).
3. Server: extend SqlUtilSessionStream to upsert/update/delete (mirror insert) and select (needs
   sendDataImpl streaming rows back — full-duplex; the connect mode supports it).
4. Rebuild Qorus client module (regenerate QorusClientBase.qm), `qctl restart`, run streams.qtest +
   sqlutil-session-stream.qtest; then the leak repro (pooled H2/H3 conn, abandoned DbRemote txn,
   confirm omquser pool connection released).
5. Install the new Qore (build/, prefix /usr/local) together with the new Qorus.

### 9.8 STATE after building the full session mechanism (Qorus develop @ fcc215d06)
Committed, verified e2e vs live omquser (test/sqlutil-session-stream.qtest, 8/8):
- d2bbe2b26 server SqlUtilSessionStream (insert/commit/rollback + recvDataDoneImpl rollback)
- 277f3fa56 held-open client pattern validated
- 418d4f86d OMQ::RemoteSqlSession client driver (Classes/Streams.qc): held stream on a bg thread
  via a STATIC runIo (must NOT capture self, else destructor deadlocks); insert/commit/rollback;
  destructor finalizes (abandon => server rollback). RemoteSqlSessionSender = the queue-backed
  DataStreamSendMessage.
- fcc215d06 server+client upsert/update/delete ops.
The stream-scoped transaction MECHANISM is complete and proven for all write DML.

Client build/deploy loop (verified): edit Classes/Streams.qc ->
  `cmake --build build --target QorusClientBase.qm` -> cp build/QorusClientBase.qm to
  test/qlib/QorusClientBase.qm.  (restart does NOT regenerate it.)  Server service: cp
  system/sqlutil-v8.0.qsd to test/system/ + `bin/qctl restart`.

### 9.9 REMAINING: legacy DbRemoteSend/DbRemoteReceive/DbRemoteBase migration (large core refactor)
Why required: AbstractParallelStream::beginTransaction (Streams.qc:572) and DbRemoteBase
(Streams.qc:2484) use the connection-scoped persistent model (conn_opts.trans_session_id +
Continue-Persistent + separate begin/data/commit POSTs).  The committed Qore change gates that
model to HTTP/1.x, so on H2/H3 these break (begin's persistence torn down immediately => data
rolled back).  THEREFORE the new Qore must NOT be installed until this migration lands.
Plan:
- Server: add a "select" op to SqlUtilSessionStream that streams result rows back (full-duplex:
  sendDataImpl drains a row queue filled by recvDataImpl) so DbRemoteReceive/db.select work in-session.
- Client: route AbstractParallelStream / DbRemoteSend (write) and DbRemoteReceive (read) through a
  RemoteSqlSession held on the connection (e.g. conn_opts.session), shared across the objects of one
  DbRemote transaction; beginTransaction creates it, commit/rollback finalize it; standalone streams
  get a per-object session that self-commits at stream end.  DbRemoteSend.socketThreadImpl
  (Streams.qc:1151) currently does its own sendDataStream(stream=insert) — change it to push framed
  ops into the session.  DbRemoteBase.methodGate write ops route through the session too.
- Negotiation: protocol auto (connect on H2/H3, DSv1 on H1) — the session works on all transports;
  for old servers without the "session" stream, fall back to the legacy flow (capability/try-catch).
- Verify: the full test/streams.qtest must stay green (it exercises DbRemote txns incl. db.select
  between begin/commit, multi-table, openStream).  Then install new Qore + new Qorus together and
  run the leak repro (pooled H2/H3 conn, abandoned data-load txn, confirm omquser pool released).
Risk: this is a refactor of a core, widely-used, extensively-tested streaming API; must be done
carefully against streams.qtest, not rushed.

### 9.10 WRITE PATH MIGRATED + committed (Qorus develop 35c6f4750)
DbRemoteSend/DbRemoteBase write transactions now run over RemoteSqlSession (conn_opts.session
replaces conn_opts.trans_session_id). DbRemoteSend.socketThreadImpl feeds column-format blocks as
framed insert/upsert/update/delete ops (columnToRows conversion); commit/rollback/destructor finalize.
DbRemoteBase.beginTransaction/commit/rollback manage conn_opts.session; methodGate routes write ops
through an active session. Verified: sqlutil-session-stream.qtest 8/8; streams.qtest 9/11 (write
cases incl. 84-assertion H1/H2/H3 transport matrix, fresh-connection, concurrent isolation all pass).

### 9.11 READ PATH — interactive read-your-writes (the 2 remaining streams.qtest failures)
The 2 remaining failures (sqlutil streams "select stream after upsert"; multi stream type persistent
session) are PRE-EXISTING H2/H3 read-your-writes failures — a direct probe (rywtest.qr) confirms the
legacy connection-scoped persistent read-your-writes is ALREADY broken on multiplexed connections
(insert rows=0; in-txn select did not see the uncommitted insert), independent of any client change.
They test interactive read-your-writes WITHIN a remote transaction (a SELECT that must see uncommitted
writes in the same session).
FINDING: the high-level DataStreamClientIo sendDataStream/sendRecvDataStream is send-all-then-read
(doSendRecvDataStream runs sendConnectStream() then readConnectStream() sequentially), so it CANNOT do
interactive request/response. Interactive read-your-writes requires the low-level
HttpClientIo::HttpClientStreamHandle (interleaved sendData()/readData()) on the client AND a server
session handler that processes each inbound frame and writes a reply per message (the bidi echo test
in module-yaml/test/DataStreamClientIo.qtest proves the server CAN reply per-message via stream.write).
TO FINISH the read path:
- Rewrite RemoteSqlSession to drive the stream via the raw HttpClientStreamHandle (open via the
  connection manager / RestClientIo connect path), with interleaved send/read so select() can send a
  {cmd:"select",...} frame and block on readData() for the result frame.
- Add a "select" op to SqlUtilSessionStream that executes the select on the held dsp and writes result
  rows back as reply frames (interactive).
- Migrate DbRemoteReceive to route reads through conn_opts.session when a transaction is active.
- Update the low-level sqlutil-streams test (it uses qrest + Qorus-Connection: Persistent directly =
  the legacy protocol) to the session, or force it to H1.
- H1 NOTE: the write-path migration may regress H1 read-your-writes (writes->session, reads->legacy);
  the read-path migration fixes this on all transports. No test currently covers H1 read-your-writes.

### 9.12 READ PATH requires the Connect gRPC-style bidi API (not the DataStream API)
Interactive bidi (read a request, write a reply, repeat) is provided by the Connect module:
Connect::ConnectHandler::registerBidiStream(svc, method, sub (ConnectServerStream stream, meta) {
    while (*auto msg = stream.read()) { stream.write(reply); }
}) — proven in module-yaml/test/DataStreamClientIo.qtest:125-130.
The DataStream API (sendDataStream/recvDataStream) my SqlUtilSessionStream/RemoteSqlSession use is
send-all-then-read (fine for writes + final commit, cannot do interactive in-transaction reads).
Therefore the read path (interactive read-your-writes) is NOT an extension of the current session;
it needs the session reimplemented on the Connect bidi API (server registerBidiStream + a Connect
bidi client driving stream.read/write), OR the low-level HttpClientStreamHandle with a custom
interactive server handler. Substantial separate effort. The reported LEAK (write transactions) is
fixed without it.

### 9.13 VALIDATED: interactive read-your-writes WORKS on H2 via the socket-handler bidi primitive
Probe /tmp/sockbidi.qr (TLS H2, no Qorus): an AbstractHttpSocketHandler whose startImpl does a
BLOCKING loop `chunk = cx."header-info".stream_data_queue.get(10s)` + replies via
`cx."header-info".send_stream_data(binary(make_yaml(reply)))`, holding in-memory state for the stream
lifetime. Client: sendHttp2Connect("/bidi", NOTHING, "bidi-stream") then interleaved
sendHttp2StreamData(sid, frame) + readHttp2StreamData(sid, ms) (returns *binary directly; loop until a
frame arrives or isHttp2StreamClosed). RESULT: insert→ack, select→sees 1 row, insert→ack,
select→sees 2 rows. Interactive send→wait→send read-your-writes CONFIRMED WORKING.
=> The Connect MODULE's bidi (ConnectServerStream.read non-blocking, drain-and-exit) is the wrong
abstraction; the socket-handler primitive (blocking get + send_stream_data) is the right one.

PROPER FIX (read path) on this primitive:
- Server: SqlUtil session as an interactive socket-handler bidi endpoint exposed by the sqlutil
  service; blocking get-loop holds the dsp transaction; processes framed ops insert/upsert/update/
  delete/select/commit/rollback; SELECT executes on the held dsp (sees uncommitted writes) and writes
  result rows back. End-of-stream / error => rollback if uncommitted (already implemented in the
  current SqlUtilSessionStream lifetime logic — reuse it).
- Client: RemoteSqlSession drives the interactive bidi stream (sendHttp2Connect + interleaved
  send/read) instead of DataStream sendDataStream; select(table,args) sends a {cmd:"select",...} frame
  and blocks reading result rows.
- Consumers: DbRemoteSend writes (fire frame + read ack), DbRemoteReceive reads (interactive select),
  DbRemoteBase txn manager — all over conn_opts.session.
- H1: the connection-scoped legacy model stays correct (connection==stream); the session is for H2/H3.

### 9.14 PROPER FIX = make DataStream stream handlers interactive on H2/H3 (Qore-language layer; C++ rail already supports it)
Root cause of recv-all-then-send: HttpServerUtil.qm AbstractStreamRequest::handleRequest() (~1722-1907)
reads ALL inbound frames (H2: body_queue.get loop ~1836-1861; H3: readQuicStreamDataBlock loop
~1771-1799; end signal recvImpl({"hdr":NOTHING}) line 1864) BEFORE sendResponse() (line 1907). The
DataStream-v1 send buffering (lines 1954-1973) is secondary; Connect mode already streams the SEND via
CallbackInputStream (line 1953) but still only AFTER the full recv loop -> no interleaving.
The interactive machinery ALREADY EXISTS for socket handlers: cx."header-info".stream_data_queue
(inbound frame Queue, filled by the I/O thread) + cx."header-info".send_stream_data (outbound) +
submitHttp2StreamingResponseHeaders/submitQuicResponseStreaming (early headers) +
sendHttp2DataWithBackpressure/sendQuicDataWithBackpressure. WebSocketHandler uses these on a dedicated
handler thread (concurrent send/recv). C++ rail needs NO change.
PLAN:
- Qore HttpServer (HttpServerUtil.qm + HttpServer.qm): add an OPT-IN interactive path for stream
  request handlers on H2/H3 that, instead of the recv-all-then-send loop, hands the handler the inbound
  stream_data_queue + an outbound send_stream_data/early-header API on its dedicated thread (the same
  wiring WebSocket gets). Gate by a new overridable e.g. AbstractStreamRequest::wantsInteractiveStreaming()
  (default False). H1 keeps the legacy connection-scoped model (already correct).
- module-yaml DataStreamRequestHandler.qm: opt-in interactive hooks so AbstractDataStreamRequestHandler
  (connect mode) can block-read framed messages and write framed replies interleaved.
- Qorus SqlUtilSessionStream: opt in; the handler becomes a blocking read-op -> execute-on-held-dsp ->
  write-reply loop (SELECT sees uncommitted writes). Reuse existing rollback-on-abandon lifetime.
- Client RemoteSqlSession: drive the interactive bidi (validated primitive); select() sends op + reads rows.
- Rewire DbRemoteSend/Receive/Base; pin nothing to H1 except where connection-scoped is desired.

### 9.15 IMPLEMENTATION SCOPE — interactive DataStream on H2/H3 (turnkey)
Change sites (exact):
- qlib/HttpServerUtil.qm AbstractStreamRequest::handleRequest() (1722-1916): add opt-in branch — if
  wantsInteractiveStreaming() && (http2||http3): submit early 200 streaming headers, then loop
  { read 1 inbound frame (H2: body_queue.get; H3: readQuicStreamDataBlock) -> recv({"data":chunk}) ->
  drain send() -> write each reply via the outbound DATA send } until END_STREAM; then
  recvImpl({"hdr":NOTHING}) + final send() drain + end-stream. Default wantsInteractiveStreaming()=False
  (new overridable, ~line 2620) preserves recv-all-then-send for all existing handlers.
- qlib/HttpServer.qm dispatch (3171-3239 inbound H2 register_body_queue; 5382+, 5778/5796/5866/5882
  early-header + 5670/5707 startPollSendHttp2StreamingResponse outbound): expose an outbound
  send_stream_data + early-header API to the interactive stream handler thread, reusing the same
  AsyncSocketIo Http2StreamContext/Http3StreamContext + wakeSocket(sock) the WebSocket path uses
  (WebSocketHandler.qm setStreamContext). The handler thread sends DATA while the I/O thread feeds
  inbound; coordinate via the existing wake/backpressure (sendHttp2DataWithBackpressure /
  sendQuicDataWithBackpressure).
- module-yaml DataStreamRequestHandler.qm: wantsInteractiveStreaming()=connect_mode; ensure recvDataImpl
  enqueues one reply per inbound op and sendDataImpl returns it (per-frame), so the interleaved loop
  produces a reply per op.
- Qorus system/sqlutil-v8.0.qsd SqlUtilSessionStream: opt in; per-op execute on held dsp (SELECT sees
  uncommitted writes) + reply; reuse existing rollback-on-abandon destructor/recvDataDone lifetime.
- Qorus Classes/Streams.qc RemoteSqlSession: drive interactive bidi (validated primitive); select(table,
  args) writes op + reads result rows; DbRemoteReceive routes reads through conn_opts.session.
Test matrix: (a) new interactive read-your-writes qtest on H2 AND H3; (b) regression — existing
DataStream/RestHandler stream tests still pass (recv-all-then-send path untouched); (c) streams.qtest
11/11 incl. the 2 read-your-writes cases; (d) valgrind on HttpServer C++ if touched (expected: none).
