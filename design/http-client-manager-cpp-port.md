# Port HttpClientConnectionManager basics to C++

**Status:** design proposal — not yet implemented
**Branch target:** `bugfix/socket_fixes` or a dedicated feature branch
**Supersedes:** the "sync HTTPClient dispatch via async C++ poll ops" plan
  in `design/sync-httpclient-as-async-wrapper.md`, which would have
  duplicated connection/pool management code between `QoreHttpClientObject`
  (C++) and `qlib/HttpClientIo/HttpClientConnectionManager.qc` (Qore).

This plan consolidates: port **only the basics** of connection pooling,
connection lifecycle, stream acquisition, and proxy handling to C++.
Retry, OAuth2, cookies, Alt-Svc, and protocol cache stay in Qore,
added via inheritance over the C++ base in `qlib/HttpClientIo/`.

---

## 1. Why this plan

### What the Phase 3 investigation revealed

Attempting to convert `QoreHttpClientObject::sendHttp2StreamData()` to
delegate to `Http2ClientPollOperationPriv` hit a structural wall:

1. `Http2ClientPollOperationPriv` expects to own the socket from
   `CONNECTING` onward and drives the full state machine.
2. The sync HTTPClient's socket is already connected (sync) and has an
   `Http2Session` on it.
3. Adopting the sync-connected socket into a new poll op isn't
   supported and would require synthesising poll-op internal state.
4. The only clean path is to rewrite `connect_unlocked()` to use
   `SocketConnectPollOperation` + `Http2ClientPollOperationPriv` from
   the start — effectively building a mini `HttpClientConnectionManager`
   inside `QoreHttpClientObject`.

Building that mini-manager in `QoreHttpClientObject` would be
~1500–2000 LOC of C++ and produce **zero unification** with the
existing Qore-level `HttpClientConnectionManager`. Both
implementations would be maintained forever, with HTTPClient's
C++ version slowly drifting behind HttpClientIo's Qore version as
new features land in one but not the other.

### Why porting the basics is strictly better

Splitting `HttpClientConnectionManager.qc` (1907 LOC) into "basics
that belong in C++" vs "features that belong in Qore":

| Functionality | LOC | Move to C++? |
|---|---|---|
| Pool management (host:port → list of connections) | ~400 | **Yes** |
| Connection creation (h1/h2/h3 dispatch + socket setup) | ~250 | **Yes** |
| Stream acquisition (`acquireStream()`) | ~200 | **Yes** |
| Proxy handling (parse, wire CONNECT tunnel) | ~150 | **Yes** |
| Protocol cache (host → observed proto) | ~100 | Optional |
| H2c upgrade probe | ~150 | Optional |
| Alt-Svc discovery + h3_probe | ~200 | Stay in Qore |
| Retry orchestration (`MaxRequestRetries` loop) | ~200 | Stay in Qore |
| Cookie jar integration | ~50 | Stay in Qore |
| OAuth2 token refresh (actually in HttpClientStreamHandle) | — | Stay in Qore |

**C++ basics scope: ~1000–1250 LOC of new C++.** Less than the
Phase 3 mini-manager, and it delivers real unification — both
`QoreHttpClientObject` and `qlib/HttpClientIo/HttpClientConnectionManager.qc`
use the same C++ base for pool + lifecycle. Dual maintenance ends
for the basics; the features layered on top stay in Qore for
flexibility.

---

## 2. Target architecture

### C++ layer (new, in `libqore`)

```
// include/qore/intern/QC_HttpClientConnectionManagerBase.h
// + C++ impl in lib/QoreHttpClientConnectionManagerBase.cpp

class HttpClientConnectionBase : public AbstractPrivateData {
    // Abstract base for H1/H2/H3 connection wrappers.
    // Owns a QoreSocketObject and a poll-op priv; exposes a small
    // C++ API for submission + teardown.
public:
    virtual HttpClientProtocol getProtocol() const = 0;
    virtual QoreObject* getPollOperationObject() const = 0;  // for controller submit
    virtual int getActiveStreamCount() const = 0;
    virtual bool isReady() const = 0;
    virtual bool isClosed() const = 0;
    virtual void waitForReady(int timeout_ms, ExceptionSink*) = 0;
    virtual void closeConnection(ExceptionSink*) = 0;

    // Submission: returns {stream_id, future} via the same
    // PromiseAction + q_future_get_blocking pattern Phase 1/2a
    // already proved out.
    virtual QoreHashNode* submitRequest(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        ExceptionSink*) = 0;

    // For H2 streaming (mid-stream writes):
    virtual int sendStreamData(int64_t stream_id, const BinaryNode* data,
        bool end_stream, ExceptionSink*) = 0;

    // Common state (protected):
protected:
    QoreSocketObject* sock_obj;         // ref-held
    QoreObject* poll_op_obj;            // SocketPollOperation wrapper (ref-held)
    std::string target_host;
    int target_port;
    bool ssl_required;
    bool proxy_tunnel;
    std::atomic<int> active_stream_count{0};
};

class Http1ClientConnection : public HttpClientConnectionBase {
    // Wraps Http1ClientPollOperationPriv.
    // Single-stream, serial.
    Http1ClientPollOperationPriv* priv;  // owned by poll_op_obj
};

class Http2ClientConnection : public HttpClientConnectionBase {
    // Wraps Http2ClientPollOperationPriv.
    // Multi-stream via nghttp2.
    Http2ClientPollOperationPriv* priv;  // owned by poll_op_obj
};

class Http3ClientConnection : public HttpClientConnectionBase {
    // Wraps Http3ClientPollOperationPriv (or QUIC session manager).
    Http3ClientPollOperationPriv* priv;
};

class HttpClientConnectionManagerBase : public AbstractPrivateData {
    // C++-level connection pool + lifecycle.  No retry, no OAuth2,
    // no cookies, no Alt-Svc — those are added by the Qore subclass.
public:
    struct Options {
        HttpClientProtocol protocol = HttpClientProtocol::Auto;
        int max_connections_per_host = 0;      // 0 = unlimited
        int max_streams_per_connection = 0;    // 0 = unlimited
        int connect_timeout_ms = 30000;
        int request_timeout_ms = 60000;
        std::string proxy_url;                  // empty = no proxy
        // ... plus TLS cert config, SNI, ALPN, etc.
    };

    DLLEXPORT HttpClientConnectionManagerBase(const Options&, ExceptionSink*);
    DLLEXPORT virtual ~HttpClientConnectionManagerBase();

    // Acquire a connection for the given origin URL.
    // May reuse a pooled connection or create a new one.
    // The returned pointer is a *borrowed* ref — pool still owns it.
    DLLEXPORT HttpClientConnectionBase* acquireConnection(
        const char* scheme, const char* host, int port,
        ExceptionSink*);

    // Release a connection back to the pool (called after all streams
    // on it are closed or if the user no longer needs it).
    DLLEXPORT void releaseConnection(HttpClientConnectionBase*);

    // Force-close a connection (e.g. after a fatal error) and remove
    // it from the pool.
    DLLEXPORT void closeAndEvict(HttpClientConnectionBase*, ExceptionSink*);

    // Convenience: single-request acquire + submit + await + release.
    // This is what QoreHttpClientObject::send_internal() becomes
    // once fully converted — the whole sync dispatch collapses to
    // one call into this method.
    DLLEXPORT QoreHashNode* request(const char* method,
        const char* scheme, const char* host, int port, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        int timeout_ms, ExceptionSink*);

private:
    Options opts;
    std::mutex pool_lock;
    std::unordered_map<std::string, std::vector<HttpClientConnectionBase*>> pool;
    // Key format: "host:port" for direct, "proxy:host:port:target:port" for tunneled
    // Proxy URL parsed into internal struct
    // ...
};
```

### Qore layer (`qlib/HttpClientIo/`, rewritten to inherit)

```qore
# qlib/HttpClientIo/HttpClientConnectionManager.qc
public class HttpClientConnectionManager
        inherits Qore::HttpClientConnectionManagerBase {  # <-- new C++ base
    private {
        # Qore-only state: retry orchestration, Alt-Svc cache,
        # protocol cache, cookie jar, pending_alt_svc queue
    }

    # The base constructor handles pool/proxy/options; Qore layer
    # adds its own features.
    constructor(*hash<HttpClientConnectionManagerOptions> opts) :
            HttpClientConnectionManagerBase(translateOptions(opts)) {
        # ... Qore-level init
    }

    # acquireStream() stays in Qore — wraps acquireConnection() in
    # retry + protocol cache + Alt-Svc lookup.
    HttpClientStreamHandle acquireStream(string url, *timeout rto) {
        # 1. Drain pending_alt_svc
        drainPendingAltSvc();
        # 2. Check protocol_cache, retries etc.
        # 3. Delegate to the C++ base:
        HttpClientConnectionBase conn = self.acquireConnection(scheme, host, port);
        # 4. Wrap in a Qore HttpClientStreamHandle
        return new HttpClientStreamHandle(conn, rto ?? request_timeout, logger);
    }
}
```

### `QoreHttpClientObject` (C++, minimal change)

```cpp
struct qore_httpclient_priv {
    // Existing fields stay for now — during the transition, both
    // paths coexist.
    my_socket_priv* msock;          // legacy sync socket
    Http2SessionPtr h2_session;     // legacy sync H2 session

    // New: the C++ connection manager, lazily created on first use.
    // When present, sync HTTPClient delegates H2/H3 work through it.
    std::unique_ptr<HttpClientConnectionManagerBase> conn_mgr;

    HttpClientConnectionManagerBase& getConnMgr(ExceptionSink*);
};

// Converted sendHttp2StreamData — no more priv->m across
// sendPendingDataBlocking:
int QoreHttpClientObject::sendHttp2StreamData(int32_t stream_id,
        const BinaryNode* data, bool end_stream, int timeout_ms,
        ExceptionSink* xsink) {
    HttpClientConnectionBase* conn = http_priv->getActiveConnection();
    if (!conn || conn->getProtocol() != HttpClientProtocol::H2) {
        xsink->raiseException("HTTP2-ERROR", "HTTP/2 is not active");
        return -1;
    }
    return conn->sendStreamData(stream_id, data, end_stream, xsink);
}
```

---

## 3. File layout

| New file | Contents |
|---|---|
| `include/qore/HttpClientConnection.h` | Public `HttpClientConnectionBase` declaration (DLLEXPORT) |
| `include/qore/HttpClientConnectionManager.h` | Public `HttpClientConnectionManagerBase` declaration (DLLEXPORT) |
| `include/qore/intern/QC_HttpClientConnection.h` | QPP binding header |
| `include/qore/intern/QC_HttpClientConnectionManager.h` | QPP binding header |
| `include/qore/intern/QoreHttpClientConnectionBase.h` | Internal impl helpers |
| `lib/QC_HttpClientConnection.qpp` | Qore bindings for the connection classes |
| `lib/QC_HttpClientConnectionManager.qpp` | Qore bindings for the manager |
| `lib/QoreHttpClientConnectionBase.cpp` | Base class implementation |
| `lib/QoreHttp1ClientConnection.cpp` | H1-specific implementation |
| `lib/QoreHttp2ClientConnection.cpp` | H2-specific implementation |
| `lib/QoreHttp3ClientConnection.cpp` | H3-specific implementation |
| `lib/QoreHttpClientConnectionManagerBase.cpp` | Manager implementation |

| Changed file | Change |
|---|---|
| `qlib/HttpClientIo/HttpClientConnection.qc` | Inherit from C++ base, delete migrated code |
| `qlib/HttpClientIo/HttpClientConnectionManager.qc` | Inherit from C++ base, delete migrated code |
| `qlib/HttpClientIo/Http{1,2,3}ClientConnectionImpl.qc` | Inherit from C++ base impl |
| `lib/QoreHttpClientObject.cpp` | Add `conn_mgr` field; converted methods delegate |
| `CMakeLists.txt` | Add new QPP + CPP sources |
| `lib/QoreNamespace.cpp` | Register new classes |

---

## 4. Phased migration order

Each phase is an independently-reviewable commit. Phases land in
order because each depends on the previous.

### Phase P1 — Prerequisite primitives (DONE)

- `q_future_get_blocking` C++ helper — committed `0f2889dad`
- PromiseAction round-trip unit test — committed `9548f67d1`

### Phase P2 — `HttpClientConnectionBase` abstract + H1 concrete (DONE)

- Add the abstract base class + DLLEXPORT API
- Implement `Http1ClientConnection` wrapping `Http1ClientPollOperationPriv`
- QPP binding: `qclass Http1Connection [ns=Qore]` (renamed from
  `Http1ClientConnection` to avoid colliding with the existing
  `HttpClientIo::Http1ClientConnection` module class)
- Unit test: create an H1 connection manually, submit via poll op,
  await Future, verify response — end-to-end without a manager

Committed `8916617f1` (initial P2) + `c00efa356` (rename fix for the
namespace collision discovered via the scenario 4 loop).

Test count: 119 → 141 (+22 assertions).

### Phase P3 prep — close-hook contract + setOwner API (DONE)

Foundation for P3 manager-driven pool eviction.  Adds the close-hook
infrastructure (see section 7.1) and the owner-string setter (see
section 7.4) so P3's manager can plug in cleanly.

- `AbstractHttpPollConnectionPriv::onClosedHook` virtual + one-shot
  `on_closed_hook_fired` guard in `setClosed()`
- `Http1ClientConnection::setOwner` / `setManager` / `onClosedHook`
  override
- Forward-declared abstract `HttpClientConnectionManagerBase` in
  `include/qore/HttpClientConnectionManager.h`
- Two new unit tests covering the one-shot hook semantics from both the
  I/O thread (connect refused) and the app thread (closeConnection)
  paths

Test count: 141 → 150 (+9 assertions).

### Phase P3 — `HttpClientConnectionManagerBase` with H1-only pool

- Add the concrete manager class with pool + proxy parsing
- Per-key creation serialization (mirroring the Qore manager's
  `creating` hash + condition variable)
- `tryReserveStream` / `releaseStreamReservation` API (see section 7.6)
- H2/H3 protocol branches stub out (raise `PROTOCOL-NOT-IMPLEMENTED`)
- Manager calls `connection->setManager(this)` after creation and
  `setManager(nullptr)` before destruction (per section 7.3)
- Manager calls `connection->setOwner("http-mgr-%p")` before submission
  so `cancelByOwner` can clean up on shutdown (per section 7.4)
- Unit tests: acquireConnection → submitRequest → release → acquire
  again (verify pooling); max_connections_per_host enforcement;
  closeAll lifecycle

Deliverable: ~700-800 LOC + ~150 LOC unit tests.  Still nothing uses
it yet.

### Phase P4 — `Http2ClientConnection`

- H2 concrete class wrapping `Http2ClientPollOperationPriv`
- Manager wires up H2 branch
- Unit test: H2 request via manager

Deliverable: ~300 LOC + unit test.

### Phase P5 — `Http3ClientConnection`

- H3 concrete class wrapping `Http3ClientPollOperationPriv`
- Manager wires up H3 branch

Deliverable: ~300 LOC + unit test.

### Phase P6 — `qlib/HttpClientIo/HttpClientConnectionManager.qc` inherits C++ base

- Rewrite the Qore class to inherit from the C++ base
- Delete pool, connection creation, acquireConnection from the Qore
  code (moved to C++)
- Keep retry, Alt-Svc, protocol cache, cookie jar in Qore
- All existing HttpClientIo tests must still pass

Deliverable: ~200 LOC added + ~1000 LOC deleted from the .qc file.

### Phase P7 — `QoreHttpClientObject` adds `conn_mgr` field

- Add the field and lazy initialization
- No functional change yet; sync path still used

Deliverable: ~50 LOC.

### Phase P8 — Convert `sendHttp2Connect` to use `conn_mgr`

- The first HTTPClient method that delegates
- Gated initially on opt-in flag for safety
- Test: existing `HTTPClient.qtest` H2 CONNECT tests must pass

Deliverable: ~200 LOC.

### Phase P9 — Convert `sendHttp2StreamData` + `readHttp2StreamData`

- Remaining H2 methods on sync HTTPClient
- At this point all H2 on sync HTTPClient goes through `conn_mgr`

Deliverable: ~150 LOC.

### Phase P10 — Convert `send_internal` H1 path

- The big one. Change the main sync dispatch to use `conn_mgr`.
- Legacy sync code stays as fallback during the transition.

Deliverable: ~400 LOC.

### Phase P11 — Convert `send_internal` H2/H3 paths

- Complete the migration of `send_internal`.

Deliverable: ~200 LOC.

### Phase P12 — Delete the legacy sync dispatch

- Remove `msock`/`h2_session` from `qore_httpclient_priv` once all
  paths delegate to `conn_mgr`.
- Clean up `brecv()`/`isDataAvailable()` `h2_cond` policy branches
  (already raise `SOCKET-H2-SYNC-ERROR` in `78a047e41` — they
  become unreachable).

Deliverable: ~800 LOC deleted.

---

## 5. Migration order rationale

- **Bottom-up**: start with the leaf class (`HttpClientConnection`)
  and build the manager on top. Each phase is independently testable.
- **H1 first**: simpler state machine than H2, easier to validate the
  C++-to-poll-op wiring end-to-end. Once H1 works, H2/H3 are variations
  on the same pattern.
- **HttpClientIo converts before HTTPClient**: P6 re-points the
  existing Qore `HttpClientConnectionManager` at the C++ base.
  Because HttpClientIo already uses the same C++ poll ops under the
  hood, this should be mostly a refactor, not a behaviour change —
  and we get a regression-test canary for the C++ base before
  touching HTTPClient.
- **HTTPClient converts last**: by the time P7–P12 run, the C++ base
  has already been exercised by the full HttpClientIo test suite, so
  the risk of introducing HTTPClient regressions is minimised.

---

## 6. Non-goals

- **Not migrating retry, OAuth2, cookies, or Alt-Svc to C++.** These
  stay in Qore because (a) they're feature-rich and Qore's looser
  typing is a good fit, (b) they change more frequently than the
  basics and benefit from the faster iteration cycle, (c) they're
  not on the hot path.
- **Not breaking the public Qore API of `HTTPClient` or
  `HttpClientConnectionManager`.** Both classes keep their existing
  method signatures and exception contracts. Downstream code that
  catches specific exception codes continues to work.
- **Not consolidating connection pools across `HTTPClient` and
  `HttpClientIo` users at runtime.** Each instance still gets its
  own `HttpClientConnectionManagerBase`. A future phase could add a
  process-global shared pool if needed.

---

## 7. Resolved design questions and contracts

The questions originally listed in this section have been resolved as
the work progressed.  This section now records the contracts that the
P3+ phases must honor.

### 7.1 Connection close notification — `onClosedHook`

**Decision (Phase P2 prep, committed alongside P3 prep work):** the
manager learns that a pooled connection has closed via a virtual hook
on `AbstractHttpPollConnectionPriv` — *not* via lazy polling, *not* via
the controller's `onComplete` dispatch path, and *not* via an extra
worker thread.

```cpp
// include/qore/AbstractHttpPollConnection.h
class AbstractHttpPollConnectionPriv : public AbstractPrivateData {
    // Fired exactly once on the first CONNECTING/READY → CLOSED transition.
    DLLEXPORT virtual void onClosedHook() {}

private:
    bool on_closed_hook_fired = false;  // protected by `lock`, gates the hook
};
```

`setClosed()` is modified to fire the hook exactly once after releasing
its internal lock and after signaling all `ready_notifiers`.  Subsequent
`setClosed()` calls (e.g., I/O thread idle timeout racing with app
thread `closeConnection`) skip the hook to keep manager-side eviction
idempotent.

`Http1ClientConnection` overrides the hook to forward to a registered
manager via a back-pointer:

```cpp
// include/qore/intern/QoreHttp1ClientConnection.h
class Http1ClientConnection : public HttpClientConnectionBase {
    QoreThreadLock onclose_lock;                     // outermost lock
    HttpClientConnectionManagerBase* manager_ = nullptr;  // back-pointer

    void setManager(HttpClientConnectionManagerBase* mgr);
    void onClosedHook() override;  // forwards to manager_
};
```

H2 and H3 connections (Phase P4 / P5) follow the same pattern: override
`onClosedHook` and forward to their registered manager.  No additional
infrastructure required — the C++ inheritance from
`AbstractHttpPollConnectionPriv` is already in place for both.

The Phase P6 rewire of `HttpClientIo::HttpClientConnection` to inherit
from the C++ base can switch from its current Qore-side `onComplete`
eviction path to `onClosedHook` and drop the
`handleConnectionResult`/`onComplete` MRO override machinery.

### 7.2 Lock ordering

Connection close-hook callbacks acquire locks in this strict order:

```
Http1ClientConnection::onclose_lock        (per connection)
   ↓
HttpClientConnectionManagerBase::pool_lock (per manager — shared_mutex)
   ↓
AsyncIoControllerPriv::m                   (controller)
```

No path may take these locks in any other order.  In particular:

- The connection's `onClosedHook` reads `manager_` under `onclose_lock`,
  then **releases** `onclose_lock` before invoking
  `manager_->onConnectionClosed(this)` — this prevents lock inversion
  for app-thread paths that take `pool_lock` first.
- The manager destructor takes `pool_lock` (drains pool into a local
  vector), **releases** it, then walks each connection and takes
  `onclose_lock` to null the back-pointer.  The locks are never held
  simultaneously, so there is no inversion with the I/O-thread path.

### 7.3 Manager / connection lifetime contract

To prevent UAF when the I/O thread fires `onClosedHook` concurrently
with manager destruction:

1. Manager destructor MUST call `connection->setManager(nullptr)` on
   every connection it has ever owned BEFORE freeing manager state.
2. `setManager(nullptr)` takes the connection's `onclose_lock`, so any
   in-flight hook invocation is either fully complete (manager pointer
   read; method already finished) or has not yet read the pointer
   (will see nullptr after `setManager(nullptr)` returns).
3. After `setManager(nullptr)` returns for all connections, the manager
   may safely free its state.

This is the "back-pointer-nulling-under-lock" pattern.  Connection
back-pointers are raw (no ref counting) — refs would be circular and
require breaking via the same nulling pattern anyway.

### 7.4 Owner string for controller submission

**Decision:** the manager calls a setter on the connection BEFORE
construction submits the poll op to the controller.

```cpp
// include/qore/intern/QoreHttp1ClientConnection.h
class Http1ClientConnection {
    void setOwner(const char* owner);  // call before submission
    std::string owner_str;              // used by buildAndSubmit
};
```

When called before `buildAndSubmit`, the owner string is used in the
controller's `SocketPollOperationInfo` hash.  When not called, a
per-instance default `"http1-cpp-conn-%p"` (using `this`) is used.
The manager uses an owner string like `"http-mgr-%p"` so that
`AsyncIoController::cancelByOwner` can clean up all manager-owned
connections in one call (e.g., on manager shutdown).

**Phase P2 limitation:** P2's `Http1ClientConnection` constructor
submits eagerly, so calling `setOwner` after construction has no
effect on the already-submitted op.  Phase P3 introduces a two-phase
construction model where the manager creates the connection unsubmitted,
calls `setOwner`, then triggers submission.  The setter is in place now
so the API surface is stable for P3.

### 7.5 Pool key format

**Decision:** for direct connections, the pool key is `"host:port"`
(matching the existing Qore manager).  For proxied connections, the
key includes the proxy: `"proxy_host:proxy_port|target_host:target_port"`
— this allows the same target to be cached separately when reached
through different proxies (or directly).

### 7.6 `tryReserveStream` / `pending_stream_count` in C++

**Decision:** Yes, the C++ base replicates the existing Qore
`tryReserveStream` / `releaseStreamReservation` API.  H1 connections
use it trivially (max 1 stream); H2 needs it for real in P4 to atomically
check capacity and reserve a slot.  Defining the API in the C++ base
in P3 keeps the abstraction clean across H1/H2/H3 from day one.

### 7.7 Cookie jar / Alt-Svc / retry orchestration

**Decision (per the basics-vs-features split in section 1):** these
stay in the Qore layer.  The C++ manager does NOT touch cookies, Alt-Svc,
or retry — those features are layered on top by the Qore subclass in
Phase P6 onward.

---

## 8. Risks

### Scope creep
The plan is ~12 phases, each committable. If every phase is
executed back-to-back, total effort is realistically **4-6 weeks**
of focused work. Partial execution leaves the tree with two
coexisting implementations (the C++ base plus the legacy sync
HTTPClient dispatch), which is fine but must not drag on indefinitely.

### Regression risk in HttpClientIo
Phase P6 rewires `HttpClientConnectionManager.qc` to inherit from
the C++ base. HttpClientIo is used by `RestClientIo`, `WebSocketClient`,
`DataProvider`, `Qorus`, etc. A bug in the C++ base is a bug for
every downstream user. Mitigation: P2–P5 land with aggressive
unit tests exercising each protocol's lifecycle before P6 touches
production code.

### Concurrency between pool operations and poll-op continuePoll
The pool manipulates connection state from worker threads;
`continuePoll` runs on the I/O thread.  The full lock ordering for the
combined stack is documented in section 7.2 — `onclose_lock` (per
connection) → `pool_lock` (per manager) → controller `m`.
Any deviation deadlocks. The test matrix must include concurrent
acquire/release under load.

### Ownership lifetime
`HttpClientConnectionBase` holds a `QoreObject* poll_op_obj` ref
*plus* a raw pointer to the poll op priv. If the `QoreObject` is
freed (e.g. the HTTPClient is destroyed while the controller still
has the poll op queued), the raw pointer dangles.  Mitigated by P2's
exception-safe `buildAndSubmit`: member pointers are committed
atomically only after submission succeeds, and `closeConnection` aborts
+ cancels in the controller before deref.  Manager destruction
follows the contract in section 7.3.

### Testing cost
Each phase needs its own unit tests. Eight new C++ test files
(one per class or concept). Probably ~2500 LOC of test code on
top of the ~1250 LOC of implementation. Budget accordingly.

---

## 9. Concrete next step (for the next session)

P1, P2, and the P3 prep work are all done.  The next step is the P3
manager class itself.  See section 4 (Phase P3) for the deliverable
list and section 7 for the contracts (close hook, lock ordering,
lifetime, owner string, pool key, stream reservation).

Read:
- `include/qore/intern/QC_Http1ClientPollOperationBase.h` — the
  poll op this class wraps
- `include/qore/AsyncCompletionAction.h` — PromiseAction API
- `include/qore/QoreFuture.h` — Promise/Future API
- `include/qore/intern/QC_Future.h` — `q_future_get_blocking`
- `qlib/HttpClientIo/HttpClientConnection.qc` — the Qore class
  being ported (for API parity reference)
- `design/sync-httpclient-as-async-wrapper.md` — the investigation
  that led to this plan

Estimated effort for P2: one focused session (4-6 hours).

---

## 10. References

- `design/sync-httpclient-as-async-wrapper.md` — the original plan
  this one supersedes; retains historical context
- `design/async-socket-io.md` — lock-yielding sync-wait infrastructure
  that the C++ connection base will build on
- `include/qore/AsyncCompletionAction.h` — the async-completion
  primitives that glue C++ poll ops to Qore Futures
- `include/qore/intern/QC_Future.h` — `q_future_get_blocking`
  helper (committed `0f2889dad`)
- `qlib/HttpClientIo/*.qc` — the Qore implementation being
  partially ported
- `lib/QoreHttpClientObject.cpp` — the sync HTTPClient that will
  eventually delegate to the new C++ manager
