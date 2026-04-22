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

## What needs to be added in HttpServerAsyncIo

A quick scan suggests the I/O-thread machinery is already in place; what's
missing is the **handler-facing API** for "give me a duplex
framed-message channel for this H2 stream". Sketch:

- `HttpServerAsyncIo::registerStreamingHandler(path, handler)` where the
  handler receives a `StreamingRequestContext` containing
  - request headers + metadata,
  - a `Channel` for inbound DATA frames (already exposed for SSE/WS),
  - methods to send response headers, push DATA frames, and submit
    trailers (mostly already exposed for SSE).

The gRPC-specific bits (length-prefixed frame header, gRPC trailers,
`grpc-status` / `grpc-message`) live entirely in `GrpcServer` /
`GrpcServerStream` on top of that primitive — they're already there in
the current code, just sitting on the wrong substrate.

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
   markers + a one-version overlap.
7. **Remove the deprecated APIs** in the next major Qore version. Strip
   the corresponding code paths in `qore_socket_private` and
   `Http2Session` — significant simplification of the lock model.

## Risks

- `HttpServerAsyncIo`'s streaming-response path is currently exercised by
  SSE and WebSocket. gRPC will be the first consumer that needs both
  client-streaming **inbound** *and* server-streaming **outbound** on the
  same logical RPC (bidi), and that needs trailer-based status reporting.
  Some additional API surface is likely required — see the audit step
  above.
- `RemoteServiceGrpcHandler` (Qorus relay) marshalls per-stream state
  through static `stream_hash{stream_id} = stream` tables. The thread
  identity changes when handlers run on the async worker pool instead of
  per-connection background threads — `RemoteServiceGrpcHandler::callLocalStreamWrite/Read`
  callbacks need re-checking that they don't depend on the calling
  thread being the GrpcServer connection thread.
- ArrowFlight has its own framing on top of gRPC that may have
  thread-affinity assumptions — needs explicit verification before flip.

## Acceptance criteria

- All `module-grpc` tests pass 50/50 under repeated execution
  (currently `streaming: server stream` was the smoking gun for the
  recently-fixed bug; serves as a good regression canary).
- All Qorus `grpc.qtest` test cases pass (both `remote:` and `local:`
  modes).
- `arrow-flight.qtest` passes.
- No remaining call sites of `Socket::*Http2*` sync APIs in
  `module-grpc/qlib/`. `git grep -n 'startPollReadHttp2Request\|readHttp2StreamDataBlock\|submitHttp2StreamingResponseHeaders\|sendHttp2StreamData\|sendHttp2Trailers\|cleanupHttp2Stream\|isHttp2StreamComplete' qlib/` returns empty.
- A run of `valgrind` on the streaming tests shows no new leaks from
  the migration (baseline against current sync version).

## Out of scope for this doc

- The exact `HttpServerAsyncIo` streaming API additions (cover in a
  follow-up design doc once Phase 1 audit completes).
- Deprecation timeline for the sync `Socket::*Http2*` server APIs (cover
  with the version-policy doc).
