# Issue #5351 — read path (interactive DataStream on H2/H3): KICKOFF

Read this first. Deep detail + reconnaissance is in `handoff.md` (§9.10–§9.15). Proof-of-concept that
the approach works is `interactive-bidi-ryw-probe.qr` (run it: see "Validated foundation" below).

## Goal
Make a SQL session over the remote streaming API support **interactive read-your-writes** within one
transaction on HTTP/2 and HTTP/3: a `SELECT` must see prior **uncommitted** writes in the same session,
with the client sending an op, reading its result, then deciding the next op. This realizes the design
intent that DataStream over H2/H3 is a fully bidirectional, interactive stream (today the server path is
recv-all-then-send).

## Acceptance criteria (the gate)
1. `qore/examples/test/qlib/HttpServer/` — a NEW qtest proving an opt-in stream handler does interactive
   send→reply→send on **both H2 and H3** (model it on `interactive-bidi-ryw-probe.qr`).
2. **Regression:** the existing recv-all-then-send path is untouched for every non-opt-in handler —
   run the full HttpServer / DataStream / RestHandler stream test suites; all green, no warnings.
3. Qorus `test/streams.qtest` reaches **11/11** — specifically the two currently-failing read-your-writes
   cases: `sqlutil streams` ("select stream after upsert") and `multi stream type persistent session`.
4. Qorus `test/sqlutil-session-stream.qtest` stays green (currently 8/8).
5. If any C++ changed: valgrind clean on affected tests (`qore -b`). Expectation: **no C++ changes** —
   the C++ rail already supports bidi (it's what SSE/WebSocket use).

## Validated foundation (already proven — do not re-litigate feasibility)
- Interactive read-your-writes WORKS on H2 via the socket-handler bidi primitive: a blocking
  `cx."header-info".stream_data_queue.get()` loop + `cx."header-info".send_stream_data(...)`, on the
  handler's dedicated thread. Proof: `qore design/5351/interactive-bidi-ryw-probe.qr`
  (`LD_LIBRARY_PATH=build qore design/5351/interactive-bidi-ryw-probe.qr` → prints
  `INTERACTIVE-RYW: WORKS`; select sees uncommitted inserts).
- The Connect MODULE's bidi (`ConnectServerStream.read` is non-blocking, drain-and-exit) is the WRONG
  abstraction — do NOT build on it.
- Root cause of recv-all-then-send: `qlib/HttpServerUtil.qm` `AbstractStreamRequest::handleRequest()`
  (~1722–1916) reads ALL inbound (to line 1864) before `sendResponse()` (1907).

## What is already done & committed (do NOT redo)
Qore (branch develop):
- `045234d12f` HttpServer: multiplexed persistent sessions stream-scoped (the WRITE-side leak fix).
Qorus (branch develop):
- `d2bbe2b26` sqlutil: stream-scoped session handler (`SqlUtilSessionStream` in `system/sqlutil-v8.0.qsd`).
- `418d4f86d` + `fcc215d06` `RemoteSqlSession` client driver + insert/upsert/update/delete ops
  (`Classes/Streams.qc`).
- `35c6f4750` DbRemoteSend/DbRemoteBase WRITE path migrated to `RemoteSqlSession` (conn_opts.session).
  `streams.qtest` is 9/11 after this; the 2 failures are the read-your-writes cases this work fixes.
These run on the DataStream send-all path (fine for writes). The read path below makes the session
interactive so reads work too.

## Implementation order (build & verify layer by layer; commit each green increment)
**Phase A — Qore core: opt-in interactive stream handler on H2/H3.**
Give an opt-in body-streaming handler the dedicated-socket + stream-context model SSE/WebSocket use
(submit 200 streaming headers early via `submitHttp2StreamingResponseHeaders`/`submitQuicResponseStreaming`,
`request_helper.setDedicated()`, inbound frame queue, outbound writer, `wakeSocket`), driving an
interleaved read→reply loop instead of recv-all-then-send. Default off (new overridable, e.g.
`AbstractStreamRequest::wantsInteractiveStreaming()` → False).
- Files: `qlib/HttpServerUtil.qm` (`handleRequest` ~1722, `sendResponse` ~1928, new overridable ~2620);
  `qlib/HttpServer.qm` dispatch (body_streaming ~3170-3300; dedicated-socket/SSE pattern ~3807-3955;
  streaming-response send ~5660-5900). Reuse the SSE H2/H3 setup (3911-3951) as the template for a
  POST→bidi handler. Model the handler on `WebSocketConnection::setStreamContext()` +
  `AsyncSocketIo::Http2StreamContext`/`Http3StreamContext` (see `qlib/WebSocketHandler.qm`).
- Verify: new standalone qtest (acceptance #1) on H2 AND H3; regression suites (acceptance #2).
- Build: `cmake --build build --target HttpServer-qmod HttpServerUtil-qmod` (qmod targets;
  see CMakeLists). Test with `LD_LIBRARY_PATH=build` and `QORE_MODULE_DIR=qlib`.

**Phase B — module-yaml DataStream: interactive hooks.**
`DataStreamRequestHandler.qm` `AbstractDataStreamRequestHandler`: override `wantsInteractiveStreaming()`
(connect mode) and make `recvDataImpl` enqueue exactly one reply per inbound op, `sendDataImpl` return it,
so the interleaved loop produces a reply per op. Client `DataStreamClientIo.qm`: an interactive bidi
entry (interleaved send/read) for connect mode — or have `RemoteSqlSession` use the low-level
`HttpClientIo::HttpClientStreamHandle` directly (interleaved `sendData`/`readData`), as the probe does.

**Phase C — Qorus server: SqlUtilSessionStream interactive.**
`system/sqlutil-v8.0.qsd`: opt in; per inbound op execute on the held `dsp` (SELECT sees uncommitted
writes) and write the reply; reuse the existing rollback-on-abandon lifetime (destructor / recvDataDone).
Deploy: copy the `.qsd` to `$OMQ_DIR/system/` and `bin/qctl restart`.

**Phase D — Qorus client: reads through the session.**
`Classes/Streams.qc`: `RemoteSqlSession.select(table, args)` writes the op + reads result rows;
migrate `DbRemoteReceive` to route reads through `conn_opts.session` when a transaction is active.
Build: `cmake --build build --target QorusClientBase.qm` then `cp build/QorusClientBase.qm
/home/david/src/Qorus/test/qlib/QorusClientBase.qm`.
- Verify: `streams.qtest` 11/11 + `sqlutil-session-stream.qtest` 8/8 (acceptance #3, #4).

## Hard-won constraints (gotchas — read before coding Phase A)
- **The handler thread must NOT touch the H2 session directly.** By design the I/O thread owns the
  nghttp2/QUIC session; the handler thread reaches it only via the queue + `submit*`/`wakeSocket`
  indirection (see the comment at `HttpServer.qm:3227-3239`). Outbound DATA must go through the
  stream-context/`wakeSocket` path, NOT a direct `sendHttp2Data*` from the handler thread.
- Inbound differs by path: body-streaming uses `register_body_queue` (handler registers a Queue the I/O
  thread fills); the dedicated-socket/ext-CONNECT path uses a pre-populated `stream_data_queue`. Phase A
  must reconcile which one the interactive handler reads from.
- A streaming send producer (CallbackInputStream / `startPollSendHttp2StreamingResponse`) is PULL-based
  and must not block; interactive replies must use the push+`wakeSocket` model (SSE/WebSocket), not a
  blocking pull. (This is why the naive CallbackInputStream approach fails.)
- H1 keeps the legacy connection-scoped model (connection==stream, already correct) — the interactive
  session is the H2/H3 path only.
- Test env: `export OMQ_DIR=/home/david/src/Qorus/test;
  export QORE_MODULE_DIR=/home/david/src/Qorus/test/qlib:/home/david/src/Qorus/test/user/modules`.
  Qorus must be running (`qctl status`). Build Qore with the install prefix matching `which qore`.
- Run audit-changes (`/audit-changes`) before each commit; commit each green increment on develop.

## Kickoff prompt for the next session (paste verbatim)
> Implement the issue #5351 read path (interactive read-your-writes for the SQL session on HTTP/2 and
> HTTP/3). Read `design/5351/KICKOFF.md` and `design/5351/handoff.md` (§9.10–§9.15) in the Qore repo
> first; run `design/5351/interactive-bidi-ryw-probe.qr` to confirm the validated foundation. Implement
> Phases A→D in order, building and testing at each layer, committing each green increment on develop,
> and running /audit-changes before each commit. Acceptance: a new H2+H3 interactive qtest passes, the
> existing recv-all-then-send regression suites stay green, Qorus `test/streams.qtest` reaches 11/11, and
> `test/sqlutil-session-stream.qtest` stays 8/8. Do not build on the Connect module's drain-and-exit
> bidi. Keep the handler thread off the H2/QUIC session lock (use the stream-context + wakeSocket model).
