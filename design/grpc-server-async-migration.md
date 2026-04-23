# GrpcServer over HttpServerAsyncIo — migration design

**Status:** proposed
**Author:** investigation arose from the gRPC server-streaming hang traced on
2026-04-22.
**Related docs:**
- `design/async-socket-as-standard.md` — sync→async migration on the client side
- `design/async-socket-io.md` — AsyncIoController internals
- `grpc-shutdown-deadlock.md` — earlier sync-API-driven gRPC issue

## Summary

`GrpcServer` (in `module-grpc/qlib/GrpcUtil/GrpcServer.qc`) is the only
remaining server-side consumer of the **sync HTTP/2 socket API**
(`Socket::startPollReadHttp2Request` + `drivePoll` + `readHttp2StreamDataBlock`
+ `isHttp2StreamComplete` + `submitHttp2StreamingResponseHeaders` +
`sendHttp2StreamData` + `sendHttp2Trailers` + `cleanupHttp2Stream`).
This API sits on top of `qore_socket_private` and `Http2Session` directly,
runs from a per-connection background thread, and has produced a long tail of
sync/async impedance bugs:

- close-vs-poll deadlock — fixed via `prepareForClose()` + `markClosed()`
  (see `grpc-shutdown-deadlock.md`).
- `brecv` `outer_lock` leak — `readHttp2StreamDataBlock` did not hold
  `priv->m`, but `brecv → SocketSyncPoll::waitReleasingLock` unconditionally
  unlock-then-locks `outer_lock`, leaving the handler thread holding
  `priv->m` it never asked for and self-deadlocking the next H2 socket call
  (`submitHttp2StreamingResponseHeaders`, `sendHttp2StreamData`).
  Fixed today (2026-04-22) by making `readHttp2StreamDataBlock` honor
  `brecv`'s contract and acquire `priv->m` up front.
- Lock-asymmetry between `readHttp2StreamDataBlock` (lock-free) and the
  isHttp2Stream\* queries (locked) — same root cause as above.

Each fix is a stop-the-bleeding patch on a fundamentally fragile design.
The proper structural fix is to move `GrpcServer` off the sync-H2-socket
path entirely and onto **HttpServerAsyncIo** — the async server framework
already used by every other Qorus HTTP entry point (REST, MCP, A2A, SSE,
WebSocket, HTTP/3).

**Target end state:** `module-grpc` calls zero sync H2 socket APIs. No
handler thread blocks on socket I/O. No handler thread acquires
`my_socket_priv::m` for HTTP/2 operations. Inbound DATA is drained by
the I/O thread into per-stream queues registered via
`Http2PollOperationBase::registerStreamQueue`; outbound HEADERS, DATA,
and trailers are enqueued by handlers via new async methods that take
only `Http2Session::m` and wake the I/O thread to flush. The existing
`SocketHttp2FlushPollOperation` on the I/O thread remains unchanged.

## Goals

1. Eliminate `GrpcServer`'s direct use of `Socket::*Http2*` sync APIs.
2. Run the gRPC accept loop, request dispatch, and per-stream
   read/write through the existing `AsyncIoController` /
   `SocketHttp2ServerPollOperation` / `Http3ServerPollOperation` /
   `ChannelAction` infrastructure.
3. Preserve the public `GrpcServer` / `GrpcClient` / `GrpcServerStream` /
   `GrpcClientStream` API so existing handlers and callers (Qorus
   `RemoteServiceGrpcHandler`, `ArrowFlight`, `module-grpc` test suite)
   keep working unchanged.
4. Once migrated, deprecate the sync server-side H2 socket APIs
   (`Socket::startPollReadHttp2Request`, `readHttp2StreamDataBlock`,
   `isHttp2StreamComplete`, `submitHttp2StreamingResponseHeaders`,
   `sendHttp2StreamData`, `sendHttp2Trailers`, `cleanupHttp2Stream`,
   `flushHttp2`, `cleanupHttp2Stream`, `startPollHttp2Flush`,
   `setHttp2ActiveStream`, `getHttp2ActiveStream`) and remove them in
   the next major version.

## Non-goals

- Changing the gRPC handler signatures users write
  (`sub(hash<auto> request, GrpcServerStream stream, hash<string,string> metadata)`
  for server-streaming, etc.).
- Migrating client-side gRPC (`GrpcClient`/`GrpcChannel`) — already on
  `HttpClientIo` / `HttpClientStreamHandle`. No changes required.
- Removing `Socket::poll()` or other lower-level sync helpers used outside
  the H2 server path.

## Current architecture

```
Test / handler thread (per connection)
  └── GrpcServer::connectionLoop
        ├── client.startPollReadHttp2Request({headers_only: True})
        ├── drivePoll(read_op)            // sync poll loop, takes priv->m
        ├── read_op.getOutput()           // request hash
        ├── if !client_streaming:
        │     ├── readStreamBody(client, stream_id)       // sync poll loop
        │     │     ├── client.readHttp2StreamDataBlock(stream_id, 5s)
        │     │     │     // brecv → waitReleasingLock → priv->m unlock/lock
        │     │     │     // ← THIS is where the recent bugs cluster
        │     │     └── client.isHttp2StreamComplete(stream_id)
        │     └── handleRequest(client, request)
        │           └── handleServerStream / handleUnary / handleClientStream / handleBidiStream
        │                 ├── handler(request, stream, metadata)
        │                 │     └── stream.write({...})
        │                 │           ├── client.submitHttp2StreamingResponseHeaders(...)
        │                 │           └── client.sendHttp2StreamData(...)
        │                 └── client.sendHttp2Trailers(...)
        └── client.cleanupHttp2Stream(stream_id)
```

Every line touching `client.*Http2*` round-trips through
`QoreSocketObject` → `qore_socket_private` → `Http2Session` with a sync
`AutoLocker(priv->m)`. The accept loop spawns one Qore background thread
per connection; that thread blocks in `readHttp2StreamDataBlock` for the
duration of the request body.

## Target architecture

```
AsyncIoController I/O thread (singleton)
  └── HttpServerAsyncIo listener
        ├── SocketHttp2ServerPollOperation (existing)
        │     ├── headers_only dispatch
        │     ├── DATA frame → ChannelAction → per-stream Channel
        │     └── END_STREAM → close Channel
        └── dispatches to handler worker pool
              └── GrpcStreamSession (NEW thin wrapper)
                    ├── Channel-driven read of incoming gRPC framed messages
                    ├── HttpServerAsyncIo write API for outgoing framed messages + trailers
                    └── handler(request, stream, metadata) ← unchanged user API
```

`GrpcServer` becomes a `HttpServerAsyncIo`-registered handler (or a thin
wrapper that owns its own `HttpServerAsyncIo` listener for standalone gRPC
ports). Per-connection threads disappear — the I/O thread drives the H2
session, and handler invocations run on the worker pool already used by
the rest of the async stack.

`GrpcServerStream::read()` becomes `channel.recv(timeout)` with gRPC frame
reassembly on top.
`GrpcServerStream::write()` becomes a call into HttpServerAsyncIo's
streaming-response API, which already knows how to push DATA frames
through the H2 session without sync `priv->m` acquisition.
Trailers and `cleanupHttp2Stream` are likewise async.

## What needs to be added — concrete Phase-1 surface

The I/O-thread machinery is already in place (`Http2PollOperation`
inbound dispatch, per-stream inbound `Queue` via
`registerStreamQueue`, `SocketHttp2FlushPollOperation` outbound flush).
What's missing is a write side that does not traverse
`my_socket_priv::m`. The sync methods (`sendHttp2StreamData`,
`submitHttp2StreamingResponseHeaders`, `sendHttp2Trailers`) each take
`my_socket_priv::m` in `QoreSocketObject` and then
`Http2Session::m` one frame deeper; the latter is where the actual
staging buffers (`pending_body_data`, `pending_data_providers`) live,
so the former is redundant for enqueue.

### C++ additions on `Http2Session`

Three new methods, mirroring the sync ones but taking **only**
`Http2Session::m`, reusing the existing staging buffers, and waking
the I/O thread (`AsyncIoControllerPriv::wakeSocket`) before return:

- `submitStreamingResponseHeadersAsync(stream_id, status, headers, xsink)`
- `sendStreamDataAsync(stream_id, data, len, end_stream, xsink)`
- `submitTrailersAsync(stream_id, trailers, xsink)`

Existing sync methods remain; the new ones sit alongside them.

### Qore additions in `qlib/AsyncSocketIo/`

New class `AsyncHttp2ServerStream` — the *only* stream-side API that
`GrpcServerAsync` will use. Deliberately no overlap with
`Http2StreamContext` (which wraps sync methods) so the "nothing sync"
bar is visually enforceable.

```qore
public class AsyncHttp2ServerStream {
    constructor(Socket sock, int stream_id, Queue inbound);
    *binary read(timeout t = 5s);   # Queue::get, NOTHING = END_STREAM
    bool readsDone();
    sendHeaders(int status, hash<string, string> headers);
    sendData(binary data, bool end_stream = False);
    sendTrailers(hash<string, string> trailers);
    reset(int error_code = 8 /* CANCEL */);
}
```

Handlers are already dispatched by `HttpAsyncSocketIoController`
with `HttpConnectionInfo` carrying `sock` + `header_info.stream_id`;
`GrpcServerAsync` constructs one `AsyncHttp2ServerStream` per RPC
from those inputs plus the Queue returned by
`Http2PollOperation::registerStreamQueue`. No new
`registerStreamingHandler` API is needed on `HttpServerAsyncIo`.

### Prior art for the trailer API shape

HTTP/2 response trailers are **not** a new pattern in the Qore codebase
— they are already used on the sync side by
`module-yaml/qlib/Connect/GrpcProtocolAdapter.qc:174-196`, which does the
exact `submitHttp2StreamingResponseHeaders` → `sendHttp2StreamData` →
`sendHttp2Trailers` → `startPollHttp2Flush` + `Socket::poll` dance that
`GrpcServer` does today, and also knows how to produce **gRPC-Web
trailers** (base64-encoded trailer frame appended to the response body;
see `GrpcProtocolAdapter.qc:261-269`). What we are adding here is
specifically the **async** equivalent — gRPC on the async path is
the first consumer, not the first use of trailers overall.

Keep the new async primitive shape-minimal — "send trailers for this
stream" — and leave encoding variants (gRPC-Web base64, DataStream's
HTTP/1.1 chunked `DataStream-Error`) to adapter layers on top, so
`GrpcProtocolAdapter` can reuse the primitive cleanly when/if it
migrates.

## Migration plan (suggested phases)

1. **Audit `HttpServerAsyncIo` streaming API**: identify what's missing
   for the gRPC use case (server-streaming, client-streaming, bidi). Add
   the gap APIs as new exports without changing existing behavior.
2. **Add a parallel `GrpcServerAsync` class** in `module-grpc/qlib/GrpcUtil/`
   alongside the existing `GrpcServer`. Implement on top of
   `HttpServerAsyncIo`. Keep the public `GrpcServer` API.
3. **Port `module-grpc` tests** against `GrpcServerAsync`, verify all 86
   pass + no flakes under repeated runs.
4. **Switch `GrpcServer` to alias `GrpcServerAsync`** (single-line swap).
   Run `module-grpc` + `arrow-flight` + Qorus `grpc.qtest` test suites.
5. **Same migration for ArrowFlight server** (uses the same base).
6. **Deprecate the sync H2 server socket APIs** with `@deprecated`
   markers + a one-version overlap. Blocked on resolving the
   `Connect/GrpcProtocolAdapter` consumer (see Risks): either port
   `ConnectHandler` to `HttpServerAsyncIo` first, or document an
   explicit sync-path exemption before deprecation can land.
7. **Remove the deprecated APIs** in the next major Qore version. Strip
   the corresponding code paths in `qore_socket_private` and
   `Http2Session` — significant simplification of the lock model.

## Risks

- `HttpServerAsyncIo`'s streaming-response path is currently exercised by
  SSE and WebSocket. gRPC will be the first consumer that needs both
  client-streaming **inbound** *and* server-streaming **outbound** on the
  same logical RPC (bidi), and that needs **async** trailer-based status
  reporting. (Trailers themselves are not novel — the sync side already
  has `Connect/GrpcProtocolAdapter` for HTTP/2 trailers and
  `DataStreamRequestHandler` for HTTP/1.1 chunked trailers — but async
  trailer submission on `HttpServerAsyncIo` is new.) Some additional API
  surface is likely required — see the audit step above.
- `RemoteServiceGrpcHandler` (Qorus relay) marshalls per-stream state
  through static `stream_hash{stream_id} = stream` tables. The thread
  identity changes when handlers run on the async worker pool instead of
  per-connection background threads — `RemoteServiceGrpcHandler::callLocalStreamWrite/Read`
  callbacks need re-checking that they don't depend on the calling
  thread being the GrpcServer connection thread.
- ArrowFlight has its own framing on top of gRPC that may have
  thread-affinity assumptions — needs explicit verification before flip.
- **Second sync consumer of the H2 server APIs.**
  `module-yaml/qlib/Connect/GrpcProtocolAdapter::handleGrpcRequest`
  (called from `ConnectHandler`, which inherits the sync
  `HttpServer::AbstractHttpRequestHandler`) calls the same sync
  `submitHttp2StreamingResponseHeaders` / `sendHttp2StreamData` /
  `sendHttp2Trailers` / `startPollHttp2Flush` methods `GrpcServer` does.
  Before Phase 6/7 can deprecate and remove those APIs, the Connect
  side has to be handled — either migrate `ConnectHandler` onto
  `HttpServerAsyncIo` as well, or carve out an explicit sync-path
  exemption. This decision is currently open.

## Acceptance criteria

- All `module-grpc` tests pass 50/50 under repeated execution
  (currently `streaming: server stream` was the smoking gun for the
  recently-fixed bug; serves as a good regression canary).
- All Qorus `grpc.qtest` test cases pass (both `remote:` and `local:`
  modes).
- `arrow-flight.qtest` passes.
- No remaining call sites of `Socket::*Http2*` sync APIs in
  `module-grpc/qlib/`. `git grep -nE 'startPollReadHttp2Request|readHttp2StreamDataBlock|submitHttp2StreamingResponseHeaders|sendHttp2StreamData|sendHttp2Trailers|cleanupHttp2Stream|isHttp2StreamComplete|startPollHttp2Flush|flushHttp2|setHttp2ActiveStream|getHttp2ActiveStream' qlib/` returns empty.
- `module-grpc` handler threads do not *hold* `my_socket_priv::m` while
  performing HTTP/2 work. The new async write path follows the
  `waitForHttp2StreamDrain` precedent (`QoreSocketObject.cpp:1056-1068`):
  a brief `priv->m` acquisition to copy the `Http2SessionPtr`, then all
  nghttp2 / session-state work runs under only the `Http2Session::m`
  recursive mutex. This eliminates the close-vs-poll lock inversion and
  the handler-vs-I/O-thread contention on `priv->m` that drove the
  sync-API bugs.
- A run of `valgrind` on the streaming tests shows no new leaks from
  the migration (baseline against current sync version).

## Out of scope for this doc

- Deprecation timeline for the sync `Socket::*Http2*` server APIs (cover
  with the version-policy doc).
- Converting SSE/WebSocket's existing write path off the sync Socket
  methods. That path is independent of this migration; flip it when
  convenient.
