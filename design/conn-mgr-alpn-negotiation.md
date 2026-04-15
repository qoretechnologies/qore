# Per-connect ALPN negotiation in HttpClientConnectionManagerBase

**Status:** implemented (phases 1–6 landed on `bugfix/socket_fixes`)
**Branch:** `bugfix/socket_fixes`
**Builds on:** `design/http-client-manager-cpp-port.md` (the C++ conn_mgr
  port, phases P1–P7, which this work extends)
**Resolved:** `needs_legacy_h2` bypass deleted — AUTO+SSL routes through
  `HttpClientProtocol::NEGOTIATE` via the conn_mgr

## 1. Goal

Remove the final entry from the `needs_legacy_h2` bypass in
`QoreHttpClientObject`:

```cpp
// lib/QoreHttpClientObject.cpp — current state after HEAD
bool needs_legacy_h2 = h2_enabled
    && http2_mode == HTTP2_MODE_AUTO && connection.ssl;
if (use_conn_mgr && !needs_legacy_h2) {
    return startPollSendRecvConnMgr(...);
}
// else: fall through to the legacy path that does per-socket ALPN
```

After this work lands, `needs_legacy_h2` is empty and both `send_internal`
and `startPollSendRecv` route **every** non-WebSocket-upgrade request
through the conn_mgr unconditionally.  The only remaining bypass
(`is_ws_upgrade`) is a legitimate architectural exception and is not in
scope here.

## 2. Why the bypass exists today

The conn_mgr's protocol is fixed at manager creation time.
`HttpClientConnectionManagerBase::Options::protocol` is one of
`H1 | H2 | H3`, and every connection the manager creates uses that
protocol.  That is the right model for:

- `HTTP2_MODE_DISABLED` — H1 only.
- `HTTP2_MODE_REQUIRED` — always H2 (over SSL via ALPN `"h2"`, over
  plain TCP via H2C preface).
- `HTTP2_MODE_H2C_DIRECT` — always H2 over plain TCP.
- `HTTP3_MODE_REQUIRED` — always H3.

It is the wrong model for `HTTP2_MODE_AUTO` over SSL, which has to
decide **h2 vs http/1.1 per connection** based on what the peer
advertises in TLS ALPN.  The same HTTPClient instance talking to two
different hosts in AUTO mode may correctly end up with an H2 pool for
one and an H1 pool for the other.  A fixed-protocol manager cannot
represent this: picking H1 upfront precludes upgrading, picking H2
upfront fails on h1-only peers.

The legacy `send_internal` path sidesteps the issue by doing ALPN
itself on a single socket and then driving either an H1 request or an
H2 session inline.  That's why the bypass routes AUTO+SSL there.

## 3. Problem statement

The conn_mgr needs a way to:

1. **Negotiate the protocol at connect time** on a per-connection
   basis when the caller hasn't committed to a specific protocol.
2. **Pool both H1 and H2 connections for the same origin**, so that
   subsequent requests to `(host, port)` can reuse an existing H2
   connection if one exists, fall back to H1 if only H1 connections
   exist, or start a new negotiation if none are available.
3. **Return a concrete typed connection** (`Http1ClientConnection*`
   or `Http2ClientConnection*`) to the caller, so the existing
   dispatch via `HttpClientConnectionBase::submitRequestWithAction`
   works without change.

## 4. Options considered

### Option A — `NEGOTIATE` protocol mode in the manager (chosen)

Add `HttpClientProtocol::NEGOTIATE` as a fourth enum value alongside
`H1`/`H2`/`H3`.  When a manager's `opts.protocol == NEGOTIATE`:

- The pool maps `(host, port, ssl)` → a list that can contain **both**
  `Http1ClientConnection*` and `Http2ClientConnection*` for the same
  key.
- On a pool miss, `acquireConnection` creates a new
  `NegotiatingConnectionPollOp` that runs TCP connect + TLS handshake
  with ALPN offer `{"h2", "http/1.1"}`.  When the handshake completes,
  the poll op reads the negotiated ALPN id from the socket, wraps the
  **already-connected** socket in either `Http1ClientConnection` or
  `Http2ClientConnection`, and publishes the concrete connection to
  the pool + the awaiting caller.
- `getConnMgr` in `qore_httpclient_priv` sets `opts.protocol =
  NEGOTIATE` whenever `http2_mode == AUTO && connection.ssl`.

**Pros:** single conn_mgr instance per HTTPClient; pool reuse is
normal; the poll-op dispatch via `submitRequestWithAction` is unchanged
because the manager still returns a concrete `HttpClientConnectionBase*`.

**Cons:** needs alternate constructors on `Http1ClientConnection` and
`Http2ClientConnection` that adopt an already-connected socket (today
they create and connect their own), and a new poll op type for the
negotiation phase.

### Option B — Dual manager per HTTPClient

Give each `qore_httpclient_priv` both an H1 and an H2 manager.  First
request probes ALPN once with a throwaway socket, records the
negotiated protocol on the HTTPClient, pins to the matching manager
for the lifetime of the connection pool, and reconnects re-probe.

**Pros:** no manager-internal changes; each manager stays
single-protocol.

**Cons:** two pools per client with no sharing across clients to the
same origin, a throwaway socket per cold cache, re-probe on every
reconnect is inefficient, and "pin to one protocol for lifetime" is
wrong for clients that talk to multiple origins with different ALPN
support.  Does not unify with the existing conn_mgr design.

### Option C — Caller passes a probe result

Have `qore_httpclient_priv` do ALPN probing itself, then instantiate
the manager with the resolved protocol.

**Pros:** zero manager changes.

**Cons:** probe-then-use is a TOCTOU race (the probed connection is
closed before the real request), defeats connection reuse for the
first request, and reintroduces legacy-path ALPN handling in exactly
the place we're trying to move logic **out of**.

**Decision:** Option A.  Options B and C fail the unification goal of
`http-client-manager-cpp-port.md` — they keep ALPN handling in
`QoreHttpClientObject` rather than in the reusable C++ manager.

## 5. Target architecture (Option A)

### 5.1 New enum value

`include/qore/HttpClientConnection.h`:

```cpp
enum class HttpClientProtocol {
    H1,         //!< HTTP/1.1 over TCP or TCP+TLS
    H2,         //!< HTTP/2 over TCP (h2c direct) or TCP+TLS (ALPN "h2")
    H3,         //!< HTTP/3 over QUIC
    NEGOTIATE   //!< TCP+TLS with ALPN offer {"h2", "http/1.1"};
                //!< manager decides H1 vs H2 per connection
};
```

`NEGOTIATE` is only valid in combination with SSL — the manager rejects
`NEGOTIATE` for plain-HTTP schemes at construction time (there is no
ALPN without TLS, and AUTO-over-plain already means H1 in the legacy
path).

### 5.2 New poll op: `NegotiatingConnectionPollOp`

New file: `include/qore/intern/NegotiatingConnectionPollOp.h` +
`lib/NegotiatingConnectionPollOp.cpp`.

Responsibilities:

- Own a freshly-created `QoreSocketObject*` for the duration of the
  TCP connect + TLS handshake.
- Configure ALPN `{"h2", "http/1.1"}` on the socket **before**
  initiating TCP connect.
- Submit a `SocketConnectPollOperation` for the connect phase,
  chained with an SSL handshake phase.
- On handshake success: read the negotiated ALPN via
  `QoreSocket::getAlpnProtocol()`, decide the concrete protocol, and
  publish the result (the connected socket + the negotiated id) to
  the awaiting caller.
- On any failure: propagate the error through the usual
  `AbstractHttpPollConnectionPriv` close hook so the manager can
  surface it to `waitForReadyOrError`.

The poll op inherits from `AbstractHttpPollConnectionPriv` so it
integrates with the existing manager state machine
(`CONNECTING → READY → CLOSED`).

### 5.3 Alternate constructors on `Http1`/`Http2ClientConnection`

Today both constructors take `(target_host, target_port, ssl_required)`
and create + connect their own socket.  Add an alternate constructor
that adopts an already-connected socket:

```cpp
// include/qore/intern/QoreHttp1ClientConnection.h
class Http1ClientConnection : public HttpClientConnectionBase {
public:
    // Existing: create-and-connect
    DLLLOCAL Http1ClientConnection(const char* host, int port,
        bool ssl_required, ExceptionSink* xsink,
        HttpClientConnectionManagerBase* mgr,
        const Http1SslConfig& ssl_cfg);

    // NEW: adopt an already-connected (and TLS-handshook if needed)
    // socket.  Used by NegotiatingConnectionPollOp after ALPN-driven
    // protocol selection.  The constructor skips the connect phase
    // and submits the poll op starting from the READY state for its
    // internal H1 keep-alive tracking.
    DLLLOCAL Http1ClientConnection(QoreSocketObject* adopted_sock,
        std::string host, int port, bool ssl_required,
        ExceptionSink* xsink,
        HttpClientConnectionManagerBase* mgr);
    ...
};
```

Same alternate constructor on `Http2ClientConnection` — it installs
the `Http2Session` over the already-connected socket (the ALPN has
already selected `"h2"`) and begins processing the H2 preface.

The existing "create-and-connect" path stays for `REQUIRED` /
`H2C_DIRECT` / `H3_REQUIRED` modes where the caller knows the protocol
up front.

### 5.4 Manager-side changes

`include/qore/HttpClientConnectionManager.h` and
`lib/QoreHttpClientConnectionManagerBase.cpp`:

- `Options::protocol = HttpClientProtocol::NEGOTIATE` is accepted and
  validated in the constructor (reject if not SSL).
- `createConnection()` grows a case for `NEGOTIATE`:
  1. Create a new `NegotiatingConnectionPollOp`.
  2. On success of the negotiation, the poll op invokes a manager
     callback (registered via a setter on the poll op) to construct
     the concrete `Http1`/`Http2ClientConnection` via the new
     alternate constructor.
  3. The concrete connection is inserted into the pool and returned
     to the caller.
- The pool data structure changes from "all connections for a key are
  the same protocol" to a mixed list.  `acquireConnection` prefers H2
  over H1 on reuse (H2 is multiplexed and almost always cheaper), and
  on a pool miss enters negotiation once per key (subsequent requests
  on the same key during an in-flight negotiation wait for the result
  via the existing `create_cond_` mechanism — same as the current
  P3 implementation).

### 5.5 HTTPClient-side changes

`qore_httpclient_priv::getConnMgr` gets one new branch:

```cpp
if (http3_mode.load(...) == HTTP3_MODE_REQUIRED) {
    opts.protocol = HttpClientProtocol::H3;
} else if (h2_effective) {
    // REQUIRED or H2C_DIRECT
    opts.protocol = HttpClientProtocol::H2;
} else if (http2_mode == HTTP2_MODE_AUTO
        && msock->socket->priv->ssl_verify_mode != -1   // i.e. this is a
                                                        // TLS scheme
        && gm != HTTP2_MODE_DISABLED && !ld) {
    opts.protocol = HttpClientProtocol::NEGOTIATE;      // NEW
} else {
    opts.protocol = HttpClientProtocol::H1;
}
```

`http2_active` is currently mirrored at manager-creation time from
`opts.protocol == H2`.  Under `NEGOTIATE`, the effective protocol isn't
known until a connection is actually acquired.  Two options:

- **Option α**: keep `http2_active` as a mirror of `opts.protocol` and
  accept that `isHttp2Active()` is `false` for NEGOTIATE clients even
  after they've talked to an h2 peer.  Callers that care about the
  runtime protocol use `getActualProtocol()` (new, see below).
- **Option β**: refresh `http2_active` after each successful request
  by reading the acquired connection's `getProtocol()`.  More
  expensive but matches legacy semantics.

**Chosen:** Option α — fewer places to update, and adding a
`getActualProtocol()` method is a small forward-compatible improvement
that also helps the current H2 tests that check `isHttp2Active()` know
they need a fresh API call.

### 5.6 Eliminating the bypass

With NEGOTIATE in place, the bypass in `startPollSendRecv` and
`send_internal` collapses to:

```cpp
// both entry points:
if (use_conn_mgr && !is_ws_upgrade) {
    return startPollSendRecvConnMgr(...);   // or send_internal_conn_mgr
}
```

`needs_legacy_h2` is deleted, along with its comment block.
`is_ws_upgrade` is renamed to `needs_socket_handover` to convey the
architectural reason rather than the list of modes.

## 6. File layout

| File | Status | Change |
|---|---|---|
| `include/qore/HttpClientConnection.h` | modified | add `HttpClientProtocol::NEGOTIATE` |
| `include/qore/intern/QoreHttp1ClientConnection.h` | modified | add adopt-socket constructor |
| `include/qore/intern/QoreHttp2ClientConnection.h` | modified | add adopt-socket constructor |
| `include/qore/intern/NegotiatingConnectionPollOp.h` | **new** | ~120 LOC header for the poll op |
| `include/qore/HttpClientConnectionManager.h` | modified | accept `NEGOTIATE` in options, mixed-protocol pool, new callback hook |
| `lib/QoreHttp1ClientConnection.cpp` | modified | adopt-socket ctor impl (~60 LOC) |
| `lib/QoreHttp2ClientConnection.cpp` | modified | adopt-socket ctor impl + `Http2Session::adoptConnected` call (~80 LOC) |
| `lib/NegotiatingConnectionPollOp.cpp` | **new** | ~400 LOC: connect → TLS handshake → ALPN read → wrap |
| `lib/QoreHttpClientConnectionManagerBase.cpp` | modified | NEGOTIATE dispatch in `createConnection`, mixed-protocol pool (~150 LOC changed) |
| `lib/QoreHttpClientObject.cpp` | modified | `getConnMgr` NEGOTIATE branch, delete bypass comment + condition |

**Total estimated:** 800–1000 new LOC + ~300 LOC of existing code
touched.  Comparable in scope to phases P4/P5 of
`http-client-manager-cpp-port.md`.

## 7. Phased implementation order

Phases are incremental and keep the test suite green after each one.

### Phase 1 — Enum + manager plumbing (no behavior change)

1. Add `HttpClientProtocol::NEGOTIATE` to the enum.
2. Teach the manager to accept `NEGOTIATE` in `Options::protocol` —
   construct succeeds, but `createConnection` raises
   `HTTPCLIENT-NEGOTIATE-NOT-IMPLEMENTED` if reached.
3. `HTTPClient`-level changes still go through the existing bypass;
   NEGOTIATE is not yet wired into `getConnMgr`.
4. No tests change.

Commit after Phase 1: a small, reviewable refactor that introduces
the new enum value and placeholder dispatch.

### Phase 2 — Adopt-socket constructors

1. Add the alternate constructor on `Http1ClientConnection` — adopts
   a connected socket, initializes the H1 priv's internal state to
   match "just finished handshake, ready to send first request".
2. Add the alternate constructor on `Http2ClientConnection` — creates
   an `Http2Session` on the adopted socket, sends the H2 client preface,
   and enters the normal `Http2ClientPollOperationPriv` loop.
3. Unit test: construct each from an existing connected socket and
   verify `submitRequestWithAction` works.  These unit tests are
   valuable independently of the negotiation work.

Commit after Phase 2: two new constructors plus direct unit tests.

### Phase 3 — `NegotiatingConnectionPollOp`

1. Implement the new poll op: TCP connect → TLS handshake with ALPN
   offer → read negotiated id → callback to the manager.
2. The manager callback calls the Phase 2 adopt-socket constructor to
   produce a concrete connection.
3. Wire `createConnection` for `NEGOTIATE` to go through this path.
4. Unit test: spin up a local test server with ALPN `"h2"` and verify
   the manager returns an `Http2ClientConnection`.  Repeat with a
   server that only offers `"http/1.1"` and verify
   `Http1ClientConnection`.

Commit after Phase 3: the negotiation path is fully functional for
fresh connections, but not yet pooled for reuse.

### Phase 4 — Mixed-protocol pool

1. Extend the pool data structures in
   `HttpClientConnectionManagerBase` to allow both H1 and H2
   connections under a single `(host, port, ssl)` key.
2. `acquireConnection` under `NEGOTIATE`:
   - Prefer an existing H2 connection if any.
   - Otherwise prefer an existing H1 connection.
   - Otherwise enter negotiation (Phase 3 path).
3. `releaseConnection`, `closeAndEvict`, `closeAll`, and
   `cleanupIdleConnections` adjust for the mixed case.
4. Unit test: issue two AUTO-mode requests to the same origin,
   verify the second reuses the H2 connection created by the first.

Commit after Phase 4: negotiation + reuse both work.

### Phase 5 — HTTPClient wiring

1. `qore_httpclient_priv::getConnMgr` sets
   `opts.protocol = NEGOTIATE` for `http2_mode == AUTO && ssl`.
2. Remove `needs_legacy_h2` from both `send_internal` and
   `startPollSendRecv`; the block becomes
   `if (use_conn_mgr && !is_ws_upgrade) { delegate; }`.
3. Rename `is_ws_upgrade` to `needs_socket_handover` (with the doc
   block explaining why).
4. Run the full test matrix — all four tests that historically failed
   with REQUIRED+SSL (now passing) should remain passing, and
   `Http2.qtest::testHttp2PollAutoSsl*` and similar AUTO+SSL tests
   should all pass through the conn_mgr path.
5. Delete any tests or comments that explicitly check the
   `needs_legacy_h2` bypass.

Commit after Phase 5: **`needs_legacy_h2` is gone**.

### Phase 6 — `isHttp2Active` refresh for NEGOTIATE clients

1. Add `QoreHttpClientObject::getActualProtocol() -> string` returning
   `"http/1.1"` / `"h2"` / `"h3"` / `"negotiate"` based on the
   last-used connection (cached on `qore_httpclient_priv` after each
   request).
2. Update `isHttp2Active()` to also consult this cache so callers in
   AUTO+SSL mode that have already talked to an h2 peer see
   `isHttp2Active() == true`.
3. Tests that check `isHttp2Active()` after an AUTO+SSL request
   should now pass.

Commit after Phase 6: API semantics match legacy for NEGOTIATE
clients, though callers get a slightly more informative alternative in
`getActualProtocol()`.

## 8. Non-goals

- **H3 discovery via Alt-Svc** stays out of scope.  H3 is upgraded via
  `Alt-Svc` from a previous h1 or h2 response, which is a separate
  higher-level state machine on the HTTPClient and is not affected by
  per-connect ALPN.  That path is documented in the existing
  `http-client-manager-cpp-port.md` § 6 "Features staying in Qore".
- **Connection migration** between H1 and H2 at runtime.  A connection
  is either H1 or H2 for its lifetime.  A client that re-connects
  (e.g. after server close) may end up with a different protocol, and
  that's fine — the new connection is just a new pool entry.
- **`HTTP2_MODE_H2C_UPGRADE`**.  That mode was removed by RFC 9113
  and already raises an up-front error.  No negotiation path is
  provided for it.
- **Proxy CONNECT + ALPN**.  When going through an HTTP proxy with
  CONNECT, the ALPN offer happens on the TLS handshake **inside** the
  tunnel.  This is already how proxy + TLS works; no change required.
  Phase 5 test plan should include one AUTO+SSL request through a
  proxy to confirm.

## 9. Risks

- **Connection lifetime races during negotiation**.  The
  `NegotiatingConnectionPollOp` holds the raw socket until the
  handshake completes, then hands it to a concrete connection class.
  If another thread sees the pool miss and starts its own negotiation
  in parallel, we risk two negotiations for the same origin.
  **Mitigation:** reuse the `create_cond_` machinery from the existing
  Phase P3 implementation — one negotiation per origin at a time,
  subsequent callers wait on the condition.  Already well-understood.
- **Adopt-socket constructors drift from create-and-connect
  constructors**.  Two code paths for establishing the same kind of
  connection is a maintenance burden.  **Mitigation:** factor out a
  helper that both constructors call for post-connect state
  initialization, so only the "how we got the socket" differs.
- **`isHttp2Active` semantics change under NEGOTIATE**.
  Phase 6 bridges this, but there is a window (Phase 5 → Phase 6)
  where `isHttp2Active()` may return `false` for AUTO+SSL clients that
  have actually connected over h2.  **Mitigation:** land Phases 5 and
  6 in the same commit.
- **Test fixture gap**.  Not all existing h2 tests use a server that
  advertises both protocols in ALPN.  The Phase 3 unit tests need
  dedicated servers that advertise only `"h2"` and only `"http/1.1"`
  to exercise both branches.
- **H2 negotiation fallback with keep-alive pings**.  `Http2ClientConnection`
  installs a periodic PING timer (from the keepalive-pings work).  If
  negotiation lands us on H1 instead, the H1 connection must NOT have
  an H2 PING timer.  The adopt-socket constructor already only
  initializes the relevant priv, so this is automatically correct —
  but the Phase 3 unit tests should assert absence of the H2 PING
  timer on an H1 result.

## 10. Open questions

1. **Should the adopt-socket constructor support proxy tunnels from
   day one, or is proxy+NEGOTIATE a later phase?**  Current
   `Http2ClientConnection` proxy support is already partial
   ("HTTPS through HTTP proxy is not yet implemented for HTTP/2
   connections" — see `createConnection` at
   `lib/QoreHttpClientConnectionManagerBase.cpp:376`).  Suggest:
   inherit the same limitation — proxy+NEGOTIATE raises
   `HTTPCLIENT-PROXY-ERROR` in Phase 3, and fix both at once in a
   later phase.

2. **Should the `HttpClientConnectionManagerBase` singleton shared
   pool handle AUTO-mode clients differently from REQUIRED-mode
   clients?**  Today each HTTPClient has its own manager.  A shared
   pool across HTTPClients to the same origin is a future
   optimization and out of scope here, but the NEGOTIATE design
   should not preclude it.

3. **ALPN offer list ordering** — Chrome/Firefox offer
   `h2, http/1.1` with h2 first.  We'll do the same.  Open question:
   should we support a client option to force ordering (e.g. for
   testing h1-only paths)?  Suggest: not yet, can add a manager option
   later if needed.

4. **What happens on an ALPN mismatch where the server picks
   something we didn't offer?**  Per RFC 7301, the server MUST reject
   the connection with a `no_application_protocol` alert in that case.
   So we never see a mismatched protocol on a successful handshake.
   The poll op should still validate the returned id against the
   offer list and raise `HTTP-CLIENT-ALPN-ERROR` on an unexpected id,
   as a defense against buggy server implementations.

## 11. Success criteria

- [ ] `needs_legacy_h2` is deleted from both `send_internal` and
      `startPollSendRecv`.
- [ ] `Http2.qtest` passes with zero tests skipped for "h2 through
      conn_mgr" reasons.
- [ ] `HTTPClient.qtest` passes.
- [ ] `HttpClientIo.qtest` passes.
- [ ] All SSE tests pass.
- [ ] A new test in `Http2.qtest` exercises AUTO+SSL with a server
      that offers both protocols and verifies h2 is selected.
- [ ] A new test in `Http2.qtest` exercises AUTO+SSL with a server
      that offers only `"http/1.1"` and verifies the client falls
      back to h1 via the same manager.
- [ ] A new test verifies two sequential AUTO+SSL requests reuse the
      same h2 connection.

## 12. References

- `design/http-client-manager-cpp-port.md` — the base conn_mgr port
  (phases P1–P7).  Section 7.2 covers the lock ordering that the
  new poll op must respect.
- RFC 7301 — Transport Layer Security (TLS) Application-Layer
  Protocol Negotiation Extension.
- RFC 7540 § 3.3 — HTTP/2 starting with HTTPS (ALPN).
- RFC 9113 (obsoletes 7540) — same.
- `lib/QoreHttpClientObject.cpp` — `getConnMgr` at
  ~`:1263`, `send_internal` bypass at ~`:8785`, `startPollSendRecv`
  bypass at ~`:1563`.
- `lib/QoreHttpClientConnectionManagerBase.cpp` `createConnection`
  at ~`:344` — where the new `NEGOTIATE` case goes.
