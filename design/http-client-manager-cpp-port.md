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

### Phase P2 — `HttpClientConnectionBase` abstract + H1 concrete

- Add the abstract base class + DLLEXPORT API
- Implement `Http1ClientConnection` wrapping `Http1ClientPollOperationPriv`
- QPP binding: `qclass Http1ClientConnection [ns=Qore; vparent=HttpClientConnection]`
- Unit test: create an H1 connection manually, submit via poll op,
  await Future, verify response — end-to-end without a manager

Deliverable: ~500 LOC + unit test. Nothing uses it yet.

### Phase P3 — `HttpClientConnectionManagerBase` with H1-only pool

- Add the manager class with pool + proxy parsing
- H2/H3 protocol branches stub out (return "not yet implemented")
- Unit test: acquireConnection → submitRequest → release → acquire again
  (verify pooling)

Deliverable: ~600 LOC + unit test. Still nothing uses it.

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

## 7. Open design questions

1. **`HttpClientConnectionBase` ownership model.** Does the manager
   own the connection priv directly, or does it own a `QoreObject*`
   wrapping a QPP-bound class? The latter integrates with the Qore
   reference-counting garbage collector; the former is lighter
   weight. Probably QPP-bound class for consistency with other
   libqore classes that cross the C++/Qore boundary.
2. **How does the Qore HttpClientIo layer observe connection
   events?** Currently Alt-Svc discovery watches response headers
   on every response; if the response parsing moves to C++, the
   Qore layer needs a hook. Option: the C++ layer emits an event
   (via `QoreEventNotifier` or a queue) and the Qore layer drains
   it before returning to the user. Needs prototyping.
3. **Proxy connection caching.** The Qore manager caches proxied
   connections by `(proxy, target)` pair. The C++ base needs the
   same keying. Straightforward but needs to be spec'd precisely.
4. **Connection state transitions under lock.** The Qore manager
   holds `pool_lock` (RWLock) for pool operations; the C++ manager
   will use `std::mutex`. The lock-ordering rules for the combined
   stack (Qore manager lock → C++ base lock → poll op stream_lock →
   socket priv->m) need to be documented and verified.
5. **Cookie jar integration.** The CookieJar is shared across
   connections in the Qore implementation. If cookies stay in Qore
   but connections move to C++, the cookie integration needs a
   request/response hook. Simpler if cookies also move to C++;
   deferred decision.

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
`continuePoll` runs on the I/O thread. Lock ordering must be
strict: pool lock → connection lock → poll-op stream_lock → socket
priv->m. Any deviation deadlocks. The test matrix must include
concurrent acquire/release under load.

### Ownership lifetime
`HttpClientConnectionBase` holds a `QoreObject* poll_op_obj` ref
*plus* a raw pointer to the poll op priv. If the `QoreObject` is
freed (e.g. the HTTPClient is destroyed while the controller still
has the poll op queued), the raw pointer dangles. Either the manager
cancels the poll op explicitly on connection teardown, or we use
only `QoreObject*` refs and pay the virtual-dispatch cost. Needs
a deliberate decision and a destructor test.

### Testing cost
Each phase needs its own unit tests. Eight new C++ test files
(one per class or concept). Probably ~2500 LOC of test code on
top of the ~1250 LOC of implementation. Budget accordingly.

---

## 9. Concrete next step (for the next session)

Phase P2: write `include/qore/HttpClientConnection.h` +
`include/qore/intern/QC_HttpClientConnection.h` +
`lib/QC_HttpClientConnection.qpp` +
`lib/QoreHttpClientConnectionBase.cpp` +
`lib/QoreHttp1ClientConnection.cpp`. Plus a unit test that creates
an H1 connection manually, submits a request via the C++ API with
a `PromiseAction`, awaits the Future via `q_future_get_blocking`,
and verifies the response.

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
