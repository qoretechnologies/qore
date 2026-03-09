# Master Plan: HTTP/2 and HTTP/3 Bidirectional Streaming

## Overview

Comprehensive plan for fixing, testing, and optimizing bidirectional streaming support
across the Qore ecosystem. Work spans three repos:

- **qore** — core HTTP/2/3 implementation, HttpServer module
- **module-yaml** — DataStream/Connect protocol (DataStream v1 + Connect/DataStream 2)
- **qorus** — integration tests with real services

## Recently Fixed

HTTP/2 flow control deadlock when body_streaming handler returns early with error:
- **Server**: RST_STREAM after error response in persistent loop + `io_wake()`
  (HttpServer.qm, HttpAsyncSocketIoController.qc)
- **Client**: Non-blocking `receiveData(0)` in `sendHttp2StreamData()` to process
  RST_STREAM (QoreHttpClientObject.cpp)
- **Client**: Reject sends to closed/reset streams in `sendStreamData()` (Http2Session.cpp)
- **Test**: Added `HTTP2-STREAM-RESET` to expected errors in streams.qtest

---

## Phase 1: Review & Optimize the receiveData(0) Fix

**Repo:** qore
**Priority:** CRITICAL
**Dependencies:** None

### Tasks

- [ ] **1.1** Optimize conditional flush in `sendHttp2StreamData()`
  - File: `lib/QoreHttpClientObject.cpp`
  - Only call `sendPendingDataBlocking(100)` when `receiveData(0)` actually received
    data AND there are pending frames to send
  - Check `nghttp2_session_want_write()` to gate the flush

- [ ] **1.2** Verify HTTP/3 path doesn't need equivalent fix
  - File: `lib/QoreHttpClientObject.cpp` (sendHttp3StreamData)
  - `driveQuicIo()` is already bidirectional — no fix needed
  - Document this architectural difference

- [ ] **1.3** Write throughput benchmark for receiveData(0) overhead
  - File: new test case in `examples/test/qlib/Http2/Http2Integration.qtest`
  - Send 10K small stream data blocks over HTTP/2, measure time
  - Normal streaming: 1000 blocks, verify all data received correctly

- [ ] **1.4** Document thread-safety invariants
  - File: `lib/QoreHttpClientObject.cpp`
  - `priv->m` → `Http2Session::m` lock ordering is consistent
  - SSL_read (receiveData) and SSL_write (sendPendingDataBlocking) are serialized under
    `priv->m`

---

## Phase 2: Connect Protocol Standalone Tests

**Repo:** module-yaml
**Priority:** HIGH
**Dependencies:** None (pure unit tests)

### Current State

`test/DataStreamUtil.qtest` has basic connect frame tests. Missing:
- Compression flag handling
- EndStreamResponse error formats
- Non-Connect error response detection (CONNECT-HTTP-ERROR)
- Boundary max_message_size edge cases
- Serialization error → EndStreamResponse

### Tasks

- [ ] **2.1** Test compressed frame roundtrip
  - File: `test/DataStreamUtil.qtest`
  - `connect_get_send` with compression function → CONNECT_FLAG_COMPRESSED
  - `connect_get_recv` with content-encoding → correct decompression

- [ ] **2.2** Test EndStreamResponse error formats
  - File: `test/DataStreamUtil.qtest`
  - Error JSON structure: `{"error": {"code": "...", "message": "..."}}`
  - Newline/CR truncation in error messages

- [ ] **2.3** Test non-Connect error response detection
  - File: `test/DataStreamUtil.qtest`
  - HTTP 400 with `text/plain` → `CONNECT-HTTP-ERROR` exception

- [ ] **2.4** Test boundary max_message_size
  - File: `test/DataStreamUtil.qtest`
  - Payload exactly at limit: should succeed
  - Payload at limit + 1: should fail

- [ ] **2.5** Test serialization error handling
  - File: `test/DataStreamUtil.qtest`
  - Callback returns unserializable data → EndStreamResponse with error

---

## Phase 3: HTTP/2 Bidirectional Streaming Tests

**Repo:** qore
**Priority:** HIGH
**Dependencies:** Phase 1

### Gap Analysis

No test exercises the low-level HTTP/2 extended CONNECT bidi APIs directly:
- `sendHttp2Connect` + `sendHttp2StreamData` + `readHttp2StreamData`
- RST_STREAM handling during active send (the just-fixed deadlock)
- Flow control under backpressure
- Concurrent bidi streams on same connection
- Client-initiated stream reset
- GOAWAY during streaming

### Tasks

- [ ] **3.1** Create test file with bidi echo handler
  - File: `examples/test/qlib/Http2/Http2BidiStreaming.qtest`
  - Handler implements `wantsBodyStreaming()`, reads via `readHttp2StreamDataBlock`,
    echoes via `submitHttp2Response` or streaming response

- [ ] **3.2** Test: sendHttp2Connect + bidi data exchange
  - Client CONNECT → server echoes → client reads response
  - Small payloads and large payloads (1MB)

- [ ] **3.3** Test: RST_STREAM during body send (the fixed deadlock)
  - Server reads first block → returns 400 → resets stream
  - Client sends multiple blocks → gets HTTP2-STREAM-RESET
  - Verify no deadlock

- [ ] **3.4** Test: Flow control under backpressure
  - Server reads slowly, client sends large data
  - Verify WINDOW_UPDATE processing, data completes
  - Verify MAX_STREAM_BUFFER (1MB) limit

- [ ] **3.5** Test: Concurrent bidi streams
  - Two CONNECT streams on same HTTP/2 connection
  - Exchange data on both simultaneously
  - Verify data isolation

- [ ] **3.6** Test: Client-initiated stream reset
  - Client sends CONNECT, starts sending, then resets
  - Server detects reset, stops reading

- [ ] **3.7** Test: GOAWAY during streaming
  - Server sends GOAWAY while client is streaming
  - Client handles gracefully

---

## Phase 4: HTTP/3 Bidirectional Streaming Tests

**Repo:** qore
**Priority:** HIGH
**Dependencies:** Phase 3 (reuse handler classes/patterns)

### Key Difference from HTTP/2

HTTP/3 uses `driveQuicIo()` which is inherently bidirectional — reads incoming UDP
packets and writes outgoing ones in a single call. Stream resets use QUIC's
`RESET_STREAM` + `STOP_SENDING` instead of HTTP/2's `RST_STREAM`.

### Tasks

- [ ] **4.1** Create test file
  - File: `examples/test/qlib/HttpServer/HttpServerH3BidiStreaming.qtest`
  - Mirror Phase 3 tests over HTTP/3

- [ ] **4.2** Test: sendHttp3Connect + bidi data exchange
  - Same as 3.2 but over HTTP/3

- [ ] **4.3** Test: STOP_SENDING/RESET_STREAM handling
  - Server returns error during body streaming → `resetQuicStream`
  - Client detects reset via `sendStreamData` returning error

- [ ] **4.4** Test: Concurrent QUIC streams
  - Multiple concurrent bidi CONNECT streams over HTTP/3

---

## Phase 5: DataStream Protocol-Specific Tests

**Repo:** module-yaml (primary), qore (server infrastructure)
**Priority:** HIGH
**Dependencies:** Phase 3 + Phase 4

### Gap Analysis

No test exercises **DataStreamClient** (integrated client with auto-negotiation) over
**HTTP/2 or HTTP/3**. Existing tests only use HTTP/1.1.

### Tasks

- [ ] **5.1** Create test file
  - File: `test/DataStreamClientH2.qtest`
  - HttpServer with async mode + TLS (forces HTTP/2 via ALPN)
  - Register ConnectHandler and AbstractDataStreamRequestHandler

- [ ] **5.2** Test: DataStreamClient with `protocol: "connect"` over HTTP/2
  - sendDataStream and recvDataStream work correctly
  - Verify data integrity

- [ ] **5.3** Test: DataStreamClient with `protocol: "connect"` over HTTP/3
  - Same as 5.2 but with HTTP/3 listener

- [ ] **5.4** Test: DataStreamClient with `protocol: "datastream"` (v1) over HTTP/2
  - Verify chunk boundary handling over HTTP/2 DATA frames

- [ ] **5.5** Test: Auto-negotiation over HTTP/2
  - `protocol: "auto"` → detects and caches Connect protocol
  - Verify fallback to DS v1 when server rejects Connect (415)

- [ ] **5.6** Test: Error handling through both protocols
  - COLUMN-ERROR propagation through Connect framing
  - COLUMN-ERROR propagation through DS v1 framing

---

## Phase 6: Qorus Integration Tests

**Repo:** qorus
**Priority:** MEDIUM
**Dependencies:** Phase 5

### Tasks

- [ ] **6.1** Verify HTTP version in existing tests
  - File: `test/streams.qtest`
  - Log and assert negotiated HTTP version

- [ ] **6.2** Add HTTP/2-forced stream tests
  - File: `test/streams.qtest`
  - New test case: `testHttp2ForcedStreams()`
  - Insert + select + commit, upsert with error, persistence recovery

- [ ] **6.3** Add HTTP/3-forced stream tests
  - File: `test/streams.qtest`
  - New test case: `testHttp3ForcedStreams()`
  - Guard with capability check (skip if H3 not available)

- [ ] **6.4** Test persistence recovery over HTTP/2 multiplexed connections
  - Verify `handleMultiplexedPersistentSync` queue-based path works

- [ ] **6.5** Test concurrent persistent transactions over HTTP/2
  - Two persistent transactions on separate HTTP/2 connections

---

## Phase 7: Performance Review & Optimization

**Repo:** qore
**Priority:** MEDIUM
**Dependencies:** Phase 3 + Phase 4

### Tasks

- [ ] **7.1** Profile receiveData(0) overhead
  - Measure per-call overhead (expected: <5 microseconds)
  - Compare throughput with/without the fix

- [ ] **7.2** Review send_buffer management in sendPendingDataBlocking
  - File: `lib/Http2Session.cpp`
  - Compaction at line 911 does `erase(begin, begin+offset)` — shifts all data
  - Consider circular buffer or deque if profiling shows this is hot

- [ ] **7.3** Review flow control window management
  - Check `no_auto_window_update` handling
  - Verify WINDOW_UPDATE frames are flushed promptly after receiveData

- [ ] **7.4** Review persistent queue dispatch latency
  - Measure time from I/O thread receiving request to handler thread processing it
  - Queue push at HttpAsyncSocketIoController.qc → Queue.get at HttpServer.qm

- [ ] **7.5** Consider batch frame processing in receiveData
  - Currently reads one brecv (up to 16KB) at a time
  - Consider loop to process all available data (check `hasSocketBufferedData()`)

---

## Phase 8: Error Handling Review

**Repo:** all
**Priority:** MEDIUM
**Dependencies:** Phase 3 + Phase 4

### Tasks

- [ ] **8.1** Verify resource cleanup on all error paths
  - `sendHttp2StreamData` error → `pending_body_data` cleanup via
    `onStreamCloseCallback` (line 1628 of Http2Session.cpp)
  - `sendHttp3StreamData` error → `streaming_body_data_` cleanup

- [ ] **8.2** Verify RST_STREAM handling in all server code paths
  - Handler pool path (HttpServer.qm after `handleHttp2MultiplexedResult`)
  - Persistent queue path (HttpServer.qm in `handleMultiplexedPersistentSync`)
  - Error path (HttpAsyncSocketIoController.qc catch block)
  - All HTTP error status codes (400, 408, 413, 500)

- [ ] **8.3** Test connection-level vs stream-level error isolation
  - RST_STREAM on one stream should not affect other active streams
  - File: `Http2BidiStreaming.qtest`

- [ ] **8.4** Test GOAWAY handling
  - New streams rejected, existing streams complete
  - Verify `isGoawayReceived()` and `getLastStreamId()`

- [ ] **8.5** Test connection loss during streaming
  - TCP connection drop mid-stream
  - Verify server persistent queue gets "close" message
  - Verify no resource leaks

---

## Implementation Sequencing

```
Phase 1 (optimize receiveData fix)
    |
    ├── Phase 2 (Connect unit tests) ── can run in parallel
    |
    v
Phase 3 (H2 bidi tests) ──> Phase 4 (H3 bidi tests)
    |                             |
    v                             v
Phase 5 (DataStream over H2/H3) ─┐
    |                              |
    v                              v
Phase 6 (Qorus integration)  Phase 7 (performance)
    |                              |
    v                              v
Phase 8 (error handling review) ───┘
```

- Phases 1 and 2 can run in parallel (different repos)
- Phases 3 and 4 can partially overlap (same repo, different test files)
- Phase 5 depends on 3+4 being stable
- Phases 6, 7, and 8 can run in parallel after Phase 5

## New Files to Create

| File | Repo | Phase |
|------|------|-------|
| `examples/test/qlib/Http2/Http2BidiStreaming.qtest` | qore | 3 |
| `examples/test/qlib/HttpServer/HttpServerH3BidiStreaming.qtest` | qore | 4 |
| `test/DataStreamClientH2.qtest` | module-yaml | 5 |

## Existing Files to Modify

| File | Repo | Phase |
|------|------|-------|
| `lib/QoreHttpClientObject.cpp` | qore | 1 |
| `lib/Http2Session.cpp` | qore | 1 |
| `test/DataStreamUtil.qtest` | module-yaml | 2 |
| `examples/test/qlib/Http2/Http2Integration.qtest` | qore | 1 |
| `test/streams.qtest` | qorus | 6 |
