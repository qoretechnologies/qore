# Sync HTTPClient as an async wrapper over HttpClientIo

**Status:** design proposal — not yet implemented
**Branch:** `bugfix/socket_fixes`
**Motivation:** unify the two HTTP client implementations (legacy sync
`HTTPClient` and modern async `HttpClientIo`) into one backend, so every
feature added to `HttpClientIo` is immediately available through the
legacy `HTTPClient` API, and the legacy sync code can be decommissioned
by delegating to the async path behind the scenes.

---

## 1. Current state

The Qore tree has two parallel HTTP client implementations:

### Sync `HTTPClient` (legacy)

- **C++ class:** `QoreHttpClientObject` (`lib/QoreHttpClientObject.cpp`)
- **Qore class:** `HTTPClient` (`lib/QC_HTTPClient.qpp`)
- **Socket ownership:** one `QoreSocketObject` owned directly
  (`msock->socket`), connected synchronously via
  `connect_unlocked()` calls through `QoreSocket::connect()` /
  `connectSSL()`.
- **Lock:** `SafeLocker sl(priv->m)` at the top of every public method
  — the HTTPClient's outer mutex serialises everything.
- **HTTP/2:** uses `priv->h2_session` (an `Http2Session` instance owned
  by the socket's `qore_socket_private`). Writes go through
  `h2_session->sendStreamData` + `h2_session->sendPendingDataBlocking`
  under the outer mutex. Reads go through
  `h2_session->receiveData` also under the outer mutex.
- **Controller interaction:** none. The socket is not registered with
  the `AsyncIoController`; the HTTPClient drives its own I/O via
  sync send/recv with blocking loops.

### Async `HttpClientIo` (modern)

- **Entry point:** `HttpClientIo::HttpClientConnectionManager` →
  `acquireStream()` → `HttpClientStreamHandle`
  (`qlib/HttpClientIo/HttpClientStreamHandle.qc`)
- **Connection model:** `HttpClientConnection` objects hold a
  `HttpClientPollOperation` (`Http1`/`Http2`/`Http3ClientPollOperationImpl`)
  that is submitted to the global `AsyncIoController`. The controller
  drives the socket non-blockingly on its I/O thread.
- **Request submission:** `conn.submitRequest(method, path, headers, body)`
  returns `{stream_id, future}`. The Future is resolved on the I/O
  thread via a C++ `PromiseAction` attached to the stream completion.
- **Sync wait pattern:** `HttpClientStreamHandle::request()` at
  `HttpClientStreamHandle.qc:247` is the canonical example: submit async,
  block on `future.get(remaining)`, translate exceptions.

### The duplication problem

Every H1/H2/H3 feature is implemented twice — once in
`QoreHttpClientObject` (sync, lock-held) and once in `HttpClientIo`
(async, promise-backed). Examples:

| Feature | Sync `QoreHttpClientObject` | Async `HttpClientIo` |
|---|---|---|
| H2 CONNECT tunnel | `sendHttp2Connect` at `QoreHttpClientObject.cpp:4770` | `Http2ClientConnectionImpl::submitConnectRequest` |
| H2 stream write | `sendHttp2StreamData` at `QoreHttpClientObject.cpp:5107` | `Http2ClientConnectionImpl::sendStreamData` at line 138 |
| H2 stream read | `readHttp2StreamData` at `QoreHttpClientObject.cpp:5159` | `Http2ClientConnectionImpl::registerStreamQueue` + Queue consumer |
| H2 data-available | `isHttp2DataAvailable` at `QoreHttpClientObject.cpp:5250` | implicit in the Queue consumer pattern |
| Request send | `send_internal` (H1/H2/H3 branches) | `submitRequest` (protocol-agnostic) |

The async side is strictly more capable: it gets C++ poll-op fast paths,
connection pooling, OAuth2 token refresh, persistent-connection reuse,
and H2/H3 multiplexing "for free" from the framework. The sync side
has to reimplement each of these by hand and hold its outer mutex
across blocking syscalls.

---

## 2. Proposed architecture — composition delegation

**Goal:** `QoreHttpClientObject` becomes a thin C++ facade that owns an
internal `HttpClientIo::HttpClientConnection` (or the C++ equivalent)
and routes every public sync method through it, blocking the caller on
a `Future` or `Queue` for the result.

### 2.1 Ownership model

```cpp
class qore_httpclient_priv {
public:
    // Legacy sync path — kept during migration for methods not yet
    // converted, eventually removed entirely.
    my_socket_priv* msock;       // sync socket (removed when migration done)
    Http2SessionPtr h2_session;  // (removed)
    // ...

    // New async backend — owns an HttpClientConnection managed by the
    // global AsyncIoController.  Created lazily on first method that
    // routes through the new path.
    ReferenceHolder<HttpClientConnection> async_conn;

    // Synchronous request timeout inherited from the legacy API.
    int timeout_ms = HTTPCLIENT_DEFAULT_TIMEOUT;
};
```

During the migration:

- Methods that have been converted use `async_conn`.
- Methods that have not been converted still use `msock`/`h2_session`.
- Both paths can coexist on the same `QoreHttpClientObject` — they use
  different socket instances so there is no shared-state collision.
- When the last sync-path method is converted, `msock` and `h2_session`
  are removed and the legacy code path is deleted.

### 2.2 The await primitive

The existing `Future`/`HttpCancellableFuture` classes
(`HttpClientStreamHandle.qc:901`) are sufficient. A sync HTTPClient
method looks like:

```cpp
QoreHashNode* QoreHttpClientObject::sendHttp2Connect(const char* path,
        const QoreHashNode* headers, const char* protocol,
        QoreHashNode* info, ExceptionSink* xsink) {
    // 1. Ensure the async connection exists
    if (!http_priv->async_conn) {
        http_priv->createAsyncConnection(xsink);
        if (*xsink) return nullptr;
    }

    // 2. Translate args to the HttpClientIo surface
    hash<auto> headers_hash = ...;  // convert QoreHashNode -> hash<auto>

    // 3. Submit async, get a Future back
    ValueHolder submit_result(
        http_priv->async_conn->submitConnectRequest(
            path, headers_hash, protocol, xsink),
        xsink);
    if (*xsink) return nullptr;

    int32_t stream_id = submit_result->getAsBigInt("stream_id");
    QoreObject* future = submit_result->getKeyValue("future").get<QoreObject>();

    // 4. Block the sync caller on the Future with the HTTPClient timeout
    ValueHolder response(
        q_future_get_blocking(future, http_priv->timeout_ms, xsink),
        xsink);
    if (*xsink) {
        // Translate Future-side errors into HTTPClient-side errors
        translateFutureError(xsink);
        return nullptr;
    }

    // 5. Build the legacy return hash and populate `info`
    return buildLegacyConnectResponse(*response, stream_id, info, xsink);
}
```

The pattern in Qore-level code already exists at
`HttpClientStreamHandle.qc:247` — the C++ port is mechanical.

### 2.3 What the await primitive needs

`q_future_get_blocking` in the sketch above does not yet exist — it is
the C++-side equivalent of `Future::get(timeout)`. There are two ways
to provide it:

1. **Reuse the Qore-level `Future` class**: call into
   `AbstractQoreZoneInfo`-style `eval`ing of the `get(timeout)` method.
   Ugly but works.
2. **Add a DLLLOCAL C++ API on `Future`**: a direct
   `Future::waitForValue(int timeout_ms, ExceptionSink*)` that blocks
   the calling thread on the internal CV without going through the
   Qore method-call machinery. Cleaner, one-time cost.

Option (2) is recommended. The Future class already has the CV and
resolution state; exposing it to C++ is ~50 lines.

### 2.4 What does *not* change

- **The sync HTTPClient's public Qore API is unchanged.** All method
  signatures in `lib/QC_HTTPClient.qpp` stay the same. Callers see no
  behaviour difference except that blocking waits no longer hold the
  outer mutex across receive/send operations.
- **Connection pooling is preserved** — each `QoreHttpClientObject`
  gets its own `HttpClientConnectionManager` (or shares the global
  one), so existing tests that rely on connection reuse still work.
- **Authentication, OAuth2, cookies, TLS** all transfer to the async
  connection at creation time (mirroring how `RestClientIo`'s
  constructor configures its internal connection).

---

## 3. Migration order

Starting with the simplest methods unlocks the pattern; later methods
reuse the scaffolding.

| Phase | Method | Complexity | Gains |
|---|---|---|---|
| 1 | `q_future_get_blocking` C++ primitive | Small — exposes existing CV | Await primitive for all later phases |
| 2 | `QoreHttpClientObject::async_conn` lifecycle (create on demand, tear down on destructor) | Medium — ownership rules | Foundation for all method conversions |
| 3 | `sendHttp2StreamData` | Small — non-blocking queue + wake + Future resolution on flush | Proof-of-concept for H2 write path |
| 4 | `readHttp2StreamData` | Medium — needs a per-stream Queue registered with the poll op | H2 read path |
| 5 | `sendHttp2Connect` | Large — full request/response cycle, header translation, error mapping | Full H2 CONNECT tunneling |
| 6 | `isHttp2DataAvailable` | Small — check stream buffer; no socket wait needed in the async model | Eliminates the last manual lock-yielding helper |
| 7 | `send_internal` (H1 path) | Large — the main request dispatcher | Eliminates sync H1 send |
| 8 | `send_internal` (H2 path) | Large — merges with phase 5 | Eliminates sync H2 send |
| 9 | `send_internal` (H3 path) | Large — QUIC-specific | Eliminates sync H3 send |
| 10 | `connect` / `connectSSL` | Medium — leverages `HttpClientConnection::connect()` | Eliminates sync connect |
| 11 | Remove `msock`/`h2_session`/`quic_sessions` from `qore_httpclient_priv` | Cleanup | Final deletion of legacy state |

**Phase 1 → 3 is the minimum viable prototype** — once those land,
subsequent phases are variations on the same pattern.

---

## 4. Risks and open questions

### 4.1 Behaviour differences vs legacy sync HTTPClient

The legacy sync code has subtle behaviours that the async path may not
reproduce exactly:

- **`send_internal` retry/redirect loop** (`QoreHttpClientObject.cpp:5500+`)
  runs the full request in a sync retry loop on 3xx responses,
  connection errors, OAuth2 401 refresh, etc. The async path handles
  these via the `HttpClientConnectionManager` retry machinery, which
  has slightly different semantics (per-stream vs per-connection).
- **`sendHttp2StreamData` advisory recv probe** (line 5141) —
  the sync method opportunistically calls `receiveData(0, …)` to
  process RST_STREAM / WINDOW_UPDATE frames that arrive during a
  send. The async path processes these via the controller's poll
  loop, which runs continuously — but the ordering relative to the
  sync caller's next method call is different.
- **Error code translation** — legacy sync throws
  `HTTP2-FLOW-CONTROL`, `HTTP2-ERROR`, `HTTP2-EOF`, `HTTP2-STATE-ERROR`
  at specific points. The async path tends to throw
  `HTTPCLIENT-STREAM-CLOSED`, `HTTPCLIENT-TIMEOUT`,
  `HTTPCLIENT-REQUEST-ERROR`. Users who catch specific error names
  will see changes.

**Mitigation**: each converted method gets a test matrix comparing the
legacy behaviour (captured from the current implementation) with the
new behaviour. Any divergence is either fixed, or called out as a
breaking change in the release notes.

### 4.2 Shared connection pool vs per-HTTPClient pool

`HttpClientConnectionManager` maintains a connection pool keyed by
origin (scheme://host:port). Options for HTTPClient composition:

- **Shared global pool**: all `QoreHttpClientObject` instances route
  through one `HttpClientConnectionManager`, sharing pooled connections.
  Efficient but couples HTTPClient lifetime to the global manager.
- **Per-HTTPClient pool**: each `QoreHttpClientObject` gets its own
  manager. Simpler lifetime; matches legacy HTTPClient semantics where
  each instance owned its own connection. Memory overhead is minor.

**Recommendation**: per-HTTPClient pool initially, with a
`setSharedConnectionManager(mgr)` opt-in for advanced users who want
the efficiency of sharing.

### 4.3 The `HttpClientConnection` C++ surface

Currently `HttpClientConnection` is a Qore class defined in
`qlib/HttpClientIo/HttpClientConnection.qc` with a thin C++ wrapper.
Calling its methods from `QoreHttpClientObject` (which is pure C++)
requires either:

- **Method dispatch through Qore's eval machinery**: acquire a
  QoreObject reference to the connection, invoke methods via
  `QoreObject::evalMethod`. Works but has Qore-interpreter overhead.
- **A DLLLOCAL C++ API on `HttpClientConnectionPriv`**: direct C++
  methods that bypass the interpreter. Cleaner but requires exposing
  more of HttpClientIo's internals as DLLLOCAL.

**Recommendation**: start with eval-dispatch for the prototype; promote
hot paths to DLLLOCAL C++ after profiling shows the overhead matters.

### 4.4 Legacy HTTPClient's socket object surfaces

Some Qore code calls `httpclient.getSocket()` to get the underlying
Socket object for low-level access (e.g. setting peer cert callbacks,
reading socket options). The async backend owns its socket on the I/O
thread; exposing it to the sync caller would violate the no-sync-I/O
invariant.

**Mitigation**: `getSocket()` returns a snapshot/proxy object that
forwards query methods to the async connection's socket but rejects
I/O methods (`send`, `recv`, `readHTTPHeader`) with
`SOCKET-SYNC-ON-IO-THREAD-ERROR` or `SOCKET-HTTPCLIENT-ASYNC-ERROR`.

### 4.5 `HTTPClient::send` body-streaming callbacks

The legacy sync `send` supports a `recv_callback` / `send_callback`
for streaming body I/O (`QC_HTTPClient.qpp:~400`). These callbacks run
on the caller's thread and expect blocking semantics. The async backend
dispatches body data via `Queue`s, so the callbacks must be driven by
the sync wrapper: it reads from the queue and calls the callback
itself, blocking between reads.

**Design**: the sync wrapper spawns a per-request reader loop:
```cpp
while (true) {
    auto chunk = stream_queue.get(remaining_ms, xsink);
    if (*xsink || chunk.isNothing()) break;
    callback(chunk, xsink);
    if (*xsink) break;
}
```
No new primitives needed — `Queue::get(timeout)` already handles this.

---

## 5. Prototype milestones

The prototype covers phases 1-3 from the migration order. Each milestone
is a committable, testable increment.

### Milestone 1: `q_future_get_blocking`

- [ ] Add `DLLLOCAL QoreValue Future::waitForValue(int timeout_ms, ExceptionSink*)`
  to `lib/QC_Future.qpp`'s private data class
- [ ] Verify via a test that a Qore-level `Future.set(x)` from thread A
  unblocks a `waitForValue` call from C++ thread B
- [ ] No behaviour change to existing Future usage

**Output**: one new public DLLLOCAL C++ method, one regression test

### Milestone 2: `async_conn` lifecycle

- [ ] Add `ReferenceHolder<HttpClientConnection> async_conn` to
  `qore_httpclient_priv`
- [ ] `createAsyncConnection(xsink)` lazy initializer:
  - Builds a URL-matching `HttpClientConnection` (H1 or H2 auto-selected
    from protocol)
  - Copies SSL certs, headers, proxy config, timeouts from the sync
    HTTPClient's state
  - Registers with a per-HTTPClient `HttpClientConnectionManager`
- [ ] Teardown in destructor
- [ ] No user-visible API change yet

**Output**: the plumbing, no method conversions yet. Unit test verifies
construction + destruction with no leaks.

### Milestone 3: `sendHttp2StreamData` delegation

- [ ] Replace `QoreHttpClientObject::sendHttp2StreamData()` body with
  the delegation pattern from §2.2
- [ ] Legacy `h2_session->sendPendingDataBlocking()` call removed
- [ ] `Http2Session::sendStreamData` still queues data, but the flush
  is done by the async controller driving `async_conn`'s socket, not
  by a blocking call under the outer mutex
- [ ] Tests: `HTTPClient.qtest` and any WebSocket-over-H2 tests that
  exercise this method must still pass

**Output**: the first working delegation, proving the pattern end-to-end.

After milestone 3 lands, the remaining migration is mechanical —
each method follows the same translate-submit-await-translate shape,
and the pattern is well-established. The design doc is updated with
"phase N complete" notes as the work progresses.

---

## 6. Non-goals

- **Not rewriting the legacy HTTPClient's Qore API** — every method
  keeps the same signature, same exception names where possible, same
  documentation. Breaking the API would force a mass-migration of
  downstream code.
- **Not deleting the legacy HTTPClient class** — the class stays;
  only the internal implementation switches. The class deletion (if
  ever) is a separate, much later decision tied to Qore's release
  cycle policy.
- **Not touching Socket-level `sendHttp2StreamData`** — that method is
  already non-blocking and is what both the legacy and new paths
  ultimately call. It's fine as-is.
- **Not unifying the `HttpClientIo::Http2Session` with the Socket's
  `h2_session`** — they're separate state containers. The legacy
  `h2_session` on the socket is removed when phase 11 completes;
  HttpClientIo's H2 session remains as the single source of truth.

---

## 7. Open questions for review

1. **Timing**: this is a ~4-6 week project. Acceptable to land
   incrementally over several weeks, or do we want to cordon it off
   onto a feature branch and merge all at once?
2. **Shared vs per-HTTPClient pool** (§4.2): default behaviour choice.
3. **`q_future_get_blocking` vs alternative primitive** (§2.3):
   green-light the Future C++ API expansion, or prefer Queue-based
   signalling?
4. **Error code compatibility** (§4.1): which legacy error names MUST
   be preserved for downstream compatibility, and which can migrate
   to the HttpClientIo naming?
5. **`getSocket()` policy** (§4.4): reject I/O access entirely, or
   provide a best-effort proxy that tries to do the right thing?

---

## 8. References

- `lib/QoreHttpClientObject.cpp` — current sync implementation
- `qlib/HttpClientIo/HttpClientStreamHandle.qc:247` — canonical async→sync wait pattern
- `qlib/HttpClientIo/Http2ClientConnectionImpl.qc:138` — async sendStreamData
- `lib/QoreSocket.cpp:3925` — Socket-level sendHttp2StreamData (already non-blocking)
- `lib/QC_Http2ClientPollOperationBase.qpp:886` — C++ poll-op sendStreamData
- `design/async-socket-io.md` — lock-yielding sync-wait infrastructure this proposal builds on
