# Async Socket as Standard — Phased Implementation Plan

**Status:** approved — Phase 1 next
**Branch:** `bugfix/socket_fixes`
**Date:** 2026-04-12

## Context

Qore has two parallel HTTP client dispatch paths: the legacy synchronous path in
`QoreHttpClientObject` (manually drives socket I/O under `priv->m`) and the async
path via `HttpClientIo` (C++ poll ops + AsyncIoController). The connection manager
C++ port (Phases P1-P7) unified the connection pool layer, but the sync HTTPClient
still uses its own socket dispatch for actual requests.

Beyond HTTP, FTP/SMTP/POP3 are entirely synchronous with no async infrastructure.

**Goal:** Make async I/O the single dispatch path. Sync callers use
`PromiseAction + q_future_get_blocking()` to block on async operations. Add
`SocketIoMode` enforcement to prevent mixing. Extend async coverage to non-HTTP
protocols.

**Builds on:**
- `design/http-client-manager-cpp-port.md` — P1-P7 complete
- `design/sync-httpclient-as-async-wrapper.md` — original investigation
- `design/async-socket-io.md` — AsyncIoController architecture

---

## Phase 1: SocketIoMode Enforcement (~150 LOC + ~100 LOC tests)

**Why first:** Prevents corruption from mixing sync/async on the same socket.
Every subsequent phase benefits from this safety net. Small, self-contained,
immediately useful.

### 1a. Add IoMode flag to `my_socket_priv`

**File:** `include/qore/intern/QC_Socket.h` (class `my_socket_priv`, line 55+)

Add alongside the existing `non_block_flags` field (line 61):

```cpp
enum class SocketIoMode : uint8_t {
    Unclaimed = 0,   // No owner — either sync or async can claim
    Sync,            // Owned by sync caller (legacy HTTPClient, FTP, etc.)
    Async,           // Owned by async I/O controller
};

SocketIoMode io_mode = SocketIoMode::Unclaimed;
```

Add enforcement methods (alongside existing `checkNonBlock` at line 104):

```cpp
DLLLOCAL int checkSyncAllowed(ExceptionSink* xsink) {
    assert(m.trylock());
    if (io_mode == SocketIoMode::Async) {
        xsink->raiseException("SOCKET-ASYNC-MODE-ERROR",
            "cannot perform synchronous I/O on a socket managed by "
            "the async I/O controller");
        return -1;
    }
    return 0;
}

DLLLOCAL int checkAsyncAllowed(ExceptionSink* xsink) {
    assert(m.trylock());
    if (io_mode == SocketIoMode::Sync) {
        xsink->raiseException("SOCKET-SYNC-MODE-ERROR",
            "cannot perform async I/O on a socket with active "
            "synchronous operations");
        return -1;
    }
    return 0;
}

DLLLOCAL void setIoMode(SocketIoMode mode) {
    assert(m.trylock());
    io_mode = mode;
}
```

### 1b. Wire into sync entry points

Modify existing `checkNonBlock(xsink)` (line 104) to also call
`checkSyncAllowed`.  This automatically protects every sync code path that
already calls `checkNonBlock` (all sync Socket methods, HTTPClient
`send_internal`, FtpClient, etc.).

### 1c. Wire into async entry points

**File:** `include/qore/SocketPollOperation.h` (class `SocketPollSocketOperationBase`)

In the constructor that claims the socket, call `checkAsyncAllowed` and then
`setIoMode(Async)`. On destruction / close, reset to `Unclaimed`.

### 1d. Subsume `SOCKET-H2-SYNC-ERROR`

The existing `SOCKET-H2-SYNC-ERROR` checks in `qore_socket_private.h:2133` and
`:3201` become a special case of the broader `SocketIoMode` enforcement. Leave
them for now (they catch a more specific case), but they can be removed in Phase 5
cleanup.

---

## Phase 2: HTTPS-through-HTTP-proxy CONNECT Tunnel (~200 LOC + ~80 LOC tests)

**Why:** Unblocks Phase 3 (P10 conversion) for proxy users. The C++ poll op
already has the full 4-state CONNECT FSM (`PROXY_CONNECT_SEND` →
`PROXY_CONNECT_RECV` → SSL upgrade) in
`lib/QC_Http1ClientPollOperationBase.qpp:259-369`. The gap is that
`Http1ClientConnection::buildAndSubmit()` hardcodes `proxy_tunnel = false`
at `lib/QoreHttp1ClientConnection.cpp:132`.

### 2a. Add proxy fields to `Http1ClientConnection`

**File:** `include/qore/intern/QoreHttp1ClientConnection.h`

New constructor overload with proxy host/port/ssl fields.

### 2b. Update `buildAndSubmit` to use proxy

**File:** `lib/QoreHttp1ClientConnection.cpp:87`

When proxy fields are set:
- TCP connect target = `proxy_host:proxy_port` (not `target_host:target_port`)
- Pass `proxy_tunnel = (ssl_required)` to `Http1ClientPollOperationPriv` ctor
- Pass `is_proxy_plain = (!proxy_ssl)` to handle plain vs TLS proxy connections

### 2c. Update `createConnection` in manager

**File:** `lib/QoreHttpClientConnectionManagerBase.cpp:204-214`

Remove the `HTTPCLIENT-PROXY-ERROR` guard. Create proxy-aware connections.

### 2d. Update pool key for proxied connections

Pool key format for proxied: `"proxy_host:proxy_port|target_host:target_port"`
(already documented in `design/http-client-manager-cpp-port.md` section 7.5).

---

## Phase 3: Convert `send_internal` to Use `conn_mgr` (P10-P11) (~400 LOC)

**Why:** The core migration. After this phase, the sync HTTPClient delegates all
H1/H2/H3 dispatch through the async connection manager.

### 3a. Remove the proxy gating condition

**File:** `lib/QoreHttpClientObject.cpp:7676`

Remove `!proxy_connection.has_url()` from the gate (proxy now handled by Phase 2).
Keep the streaming exclusion for Phase 4.

### 3b. Enable `use_conn_mgr` by default

**File:** `lib/QoreHttpClientObject.cpp:1152`

Change `bool use_conn_mgr = false` to `true`. Existing `setConnMgrEnabled(false)`
API provides opt-out.

### 3c. Protocol selection in `getConnMgr`

**File:** `lib/QoreHttpClientObject.cpp:1158`

Derive protocol from `http2_mode` / `http3_mode` instead of hardcoding H1.

---

## Phase 4: Streaming Support in `conn_mgr` (~300 LOC + ~100 LOC tests)

**Why:** Removes the last `send_internal` gating condition. After this, the
legacy sync dispatch path is fully bypassed.

- Add `submitRequestStreaming()` virtual to `HttpClientConnectionBase`
- Implement in H1/H2/H3 connection classes via poll op's existing streaming API
- Add `requestStreaming()` to manager
- Remove streaming exclusion from `send_internal` gate

---

## Phase 5: Delete Legacy Sync Dispatch (P12) (~800 LOC deleted)

**Why:** Completes the HTTP migration. Removes dead code, reduces maintenance.

### What to delete

- Legacy `send_internal` path after conn_mgr delegation (~600 LOC)
- `msock` field from `qore_httpclient_priv`
- `h2_session` on `qore_socket_private`
- `setH2ActiveStreamId` / `getH2ActiveStreamId`
- `h2_cond` sync-error checks in `brecv()` / `isDataAvailable()`
- `Http2Session::sendPendingDataBlocking`
- Manual nghttp2 dispatch in `sendHttp2StreamData`, `readHttp2StreamData`,
  `sendHttp2Connect`

### What to keep

- `send_internal_conn_mgr` (rename to `send_internal`)
- `setConnMgrEnabled` API
- Redirect handling, content-type processing, body decompression

---

## Phase 6: Event Queue Notifications (~100 LOC)

Fire `QORE_EVENT_HTTP_CONTENT_LENGTH`, `QORE_EVENT_HTTP_CHUNKED_START/END`,
and `QORE_EVENT_HTTP_REDIRECT` from the conn_mgr path based on response headers.

---

## Phase 7: FTP Async Poll Operations (~1500 LOC + ~500 LOC tests)

**Why:** FTP is the largest non-HTTP sync protocol.

- `FtpControlPollOperation` — 7-state FSM (CONNECTING → GREETING → AUTH →
  READY → COMMAND_SEND → COMMAND_RECV → CLOSED)
- `FtpDataPollOperation` — PASV/EPSV connect or PORT accept + transfer
- `FtpClientPollOperation` — composite orchestrator with `submitGet()`,
  `submitPut()`, `submitList()`
- Wire into `QoreFtpClient` via `conn_mgr`-style lazy init

---

## Phase 8: SMTP/POP3 Async Poll Operations (~800 LOC + ~300 LOC tests each)

Qore-level poll operations for simpler line-based protocols:

- `SmtpPollOperation` — EHLO/STARTTLS/AUTH/MAIL/DATA state machine
- `Pop3PollOperation` — USER/PASS/STAT/RETR/DELE state machine
- `SmtpClientIo` / `Pop3ClientIo` modules following the HttpClientIo pattern

---

## Phase 9: Cleanup and Optimization

- `onConnectionClosed` O(1) optimization (stash pool key on connection)
- `getReferencedErrorInfo` on base class (public)
- Auth challenge support (401/407 automatic retry)

---

## Dependency Graph

```
Phase 1 (SocketIoMode) ──────────────────────────────────┐
Phase 2 (CONNECT tunnel) ─── Phase 3 (send_internal) ─── Phase 5 (delete legacy)
                              Phase 4 (streaming) ────────┘
                                                          Phase 6 (event queue)
Phase 7 (FTP async) ──── standalone, depends on Phase 1
Phase 8 (SMTP/POP3) ──── standalone, depends on Phase 1
Phase 9 (cleanup) ─────── depends on all above
```

Phases 1 and 2 can proceed in parallel. Phases 7 and 8 can proceed in parallel
with Phases 3-6 (independent protocol stacks).
