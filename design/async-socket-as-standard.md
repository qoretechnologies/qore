# Async Socket as Standard

**Status:** complete
**Branch:** `bugfix/socket_fixes`
**Completed:** 2026-04-16

## Overview

The Qore async I/O subsystem is now the single dispatch path for all socket
protocols. Synchronous callers (HTTPClient, FtpClient, etc.) block on
`PromiseAction + q_future_get_blocking()` over the async path rather than
performing I/O directly. This eliminates the parallel sync/async code paths,
prevents sync/async mixing corruption via `SocketIoMode` enforcement, and
extends async coverage to FTP, SMTP, and POP3.

**Builds on:**
- `design/http-client-manager-cpp-port.md` — C++ connection manager (P1-P7)
- `design/sync-httpclient-as-async-wrapper.md` — original investigation
- `design/async-socket-io.md` — AsyncIoController architecture

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  Sync API (HTTPClient, FtpClient, SmtpClient, Pop3Client)  │
│  → PromiseAction + q_future_get_blocking()                  │
├─────────────────────────────────────────────────────────────┤
│  HttpClientConnectionManagerBase (C++ connection pool)      │
│  → acquireConnection / request / releaseConnection          │
│  → O(1) pool eviction via stashed pool_key_                 │
├─────────────────────────────────────────────────────────────┤
│  HttpClientConnectionBase (virtual dispatch)                │
│  → submitRequest / submitRequestStreaming                    │
│  → submitRequestStreamingSend / pushSendData / setTrailers  │
│  → Http1ClientConnection / Http2ClientConnection /          │
│    Http3ClientConnection                                    │
├─────────────────────────────────────────────────────────────┤
│  AsyncIoController (singleton, I/O thread pool)             │
│  → SocketPollOperationBase C++ fast path                    │
│  → PollPipeline declarative step machine                    │
├─────────────────────────────────────────────────────────────┤
│  Protocol Poll Operations                                   │
│  HTTP: Http1/2/3ClientPollOperationPriv (C++)               │
│  FTP:  FtpControlPollOperation + FtpDataPollOperation (C++) │
│  SMTP: SmtpSendPollOperation (PollPipeline, Qore)           │
│  POP3: Pop3StatPollOperation + Pop3RetrPollOperation         │
│        (PollPipeline, Qore)                                 │
└─────────────────────────────────────────────────────────────┘
```

## Key Components

### SocketIoMode Enforcement (Phase 1)

`SocketIoMode` enum on `my_socket_priv`: `Unclaimed`, `Sync`, `Async`.
`checkSyncAllowed()` / `checkAsyncAllowed()` wired into `checkNonBlock()`
and `SocketPollSocketOperationBase` constructor. Prevents corruption from
mixing sync and async I/O on the same socket.

### CONNECT Tunnel (Phase 2)

`Http1ClientConnection` proxy constructor passes `proxy_tunnel=true` to
the poll op, enabling HTTPS-through-HTTP-proxy via CONNECT + SSL upgrade.
Pool key format: `"proxy_host:proxy_port|target_host:target_port"`.

### Streaming Send Virtual Dispatch (Phase 4)

Three virtual methods on `HttpClientConnectionBase`:
- `submitRequestStreamingSend()` — submits headers without END_STREAM
- `pushSendData(const void*, size_t)` — pushes body chunks
- `setTrailers()` — sends HTTP trailers

H1 bridges to chunked TE via `QoreStringNode*`. H2 uses
`sendStreamData()` on the poll op. H3 goes through `QuicSession` API.
`send_internal_conn_mgr()` uses virtual dispatch instead of
`dynamic_cast<Http1ClientConnection*>`.

### Connection Manager Auth Challenge (Phase 9)

Single-retry for 401/407 responses in the conn_mgr path. Parses
`WWW-Authenticate` / `Proxy-Authenticate` headers and computes:
- **Basic**: base64-encoded `username:password`
- **Digest**: RFC 7616 MD5/SHA-256 challenge-response with `cnonce`

Retry is limited to one attempt and skipped when `error_passthru` is set.

### SMTP/POP3 via PollPipeline (Phase 8)

PollPipeline-native operations using static pipeline unrolling for
variable-length iteration (recipients, messages):

- `SmtpSendPollOperation` — connect + auth + MAIL FROM + unrolled RCPT TO
  + DATA + message body + QUIT
- `Pop3StatPollOperation` — connect + auth + STAT + LIST
- `Pop3RetrPollOperation` — unrolled RETR + optional DELE + QUIT
- `Pop3FetchPollOperation` — two-phase orchestrator (stat then retr)

Connection integration: `SmtpConnection.startPollSend(message)`,
`Pop3Connection.startPollFetch(do_delete)`.

### FTP Async (Phase 7)

C++ composite state machine:
- `FtpControlPollOperation` — 7-phase FSM (GREETING → AUTH → READY)
- `FtpDataPollOperation` — PASV/EPSV data channel transfer
- `FtpClientPollOperation` (Qore) — orchestrates control + data ops

Sync FtpClient dispatches via `submitCommand()` + `waitForCompletion()`.

## Deleted Code (Phase 5)

~2400 LOC of legacy sync dispatch removed:
- Manual socket I/O in `send_internal` (legacy HTTP/1.1 path)
- `msock` field, `h2_session` on socket, `h2_cond` sync-error checks
- `sendHttp2StreamData`/`readHttp2StreamData` manual nghttp2 dispatch
- `Http2Session::sendPendingDataBlocking`

## Key Design Decisions

1. **Sync-over-async via Future**: sync callers create a `PromiseAction`,
   submit to the I/O controller, and block on `q_future_get_blocking()`.
   No parallel code paths — the async path is the only dispatch path.

2. **Virtual dispatch over dynamic_cast**: `HttpClientConnectionBase`
   provides virtual methods for all request types. The manager and
   `send_internal_conn_mgr` use the base class pointer directly.

3. **PollPipeline for line protocols**: SMTP and POP3 use PollPipeline's
   declarative step API rather than C++ state machines. Static unrolling
   at construction time handles variable iteration (recipients, messages).

4. **O(1) pool eviction**: Each connection stores its pool key
   (`pool_key_`), enabling direct `unordered_map::find()` in
   `onConnectionClosed()` and `closeAndEvict()`.

5. **Channel draining fall-through**: H2/H3 can dispatch headers + body
   in a single channel event. The draining code falls through from header
   processing to the body check rather than using `continue`.
