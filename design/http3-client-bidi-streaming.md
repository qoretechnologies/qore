# HTTP/3 Client Bidirectional Streaming

## Summary

The HTTP/3 client (`HttpClientIo`) lacks support for bidirectional streaming.
Requests are submitted atomically via `submitQuicRequest()` (HEADERS + body +
END_STREAM in one call).  There is no way for a client to open a stream, send
data incrementally, read server responses while still sending, and close the
write side independently — the pattern required by gRPC bidi and client-streaming
RPCs.

The HTTP/2 client already supports this via `Http2ClientStreamHandleImpl::sendData()`
and `writesDone()`.  This design covers adding the same capability for HTTP/3.

## Current State

### HTTP/2 client bidi streaming (working)

```
Http2ClientStreamHandleImpl
  ├── openStream(headers)         → HEADERS (no END_STREAM)
  ├── sendData(chunk)             → DATA
  ├── sendData(chunk)             → DATA
  ├── writesDone()                → DATA (empty, END_STREAM)
  ├── readData() ←─── server response DATA
  └── readTrailers() ←─── server HEADERS (END_STREAM)
```

Key components:
- `Http2ClientStreamHandleImpl.qc:209` — `sendData(data, end_stream)`
- `Http2ClientPollOperationImpl.qc:223` — `sendStreamData(stream_id, data, end_stream)`
- `Http2ClientConnectionImpl.qc:131` — `sendStreamData()` proxy
- `Socket::sendHttp2StreamData()` — C++ layer, calls `nghttp2_submit_data2()`

### HTTP/3 client (no bidi streaming)

```
Http3ClientPollOperationImpl
  └── submitRequest(url, method, path, headers, body)  → HEADERS + DATA + END_STREAM (atomic)
```

The only non-request data API is `sendDatagram()` (RFC 9297 QUIC datagrams),
which is unreliable and unrelated to stream-based bidi.

## Design

### Implemented API Surface

Added to `Http3ClientStreamHandle` (inherits `HttpClientStreamHandle`):

```qore
class Http3ClientStreamHandle inherits HttpClientStreamHandle {
    # Existing: request(), sendDatagram(), readDatagram()

    # Start a streaming request
    # body_streaming=False (default): HEADERS with END_STREAM (response-only streaming, e.g., SSE)
    # body_streaming=True: HEADERS without END_STREAM (bidirectional, e.g., gRPC bidi)
    int startStreaming(string method, string path, *hash<auto> headers,
            bool body_streaming = False)

    # Send incremental data on an open stream
    sendData(data data, bool end_stream = False)

    # Close the write side (empty DATA + END_STREAM)
    writesDone()

    # Read response data from the server
    # Returns: binary (body chunk), hash with end_stream (done),
    #          hash with error (error), or NOTHING (timeout)
    auto readData(*timeout timeout_ms)

    # Read trailing headers from completed response
    *hash<string, string> getTrailers()
}
```

Data delivery uses an unlimited-capacity `Channel(-1)` — the same pattern as
HTTP/2 streaming. The `handleStreamingResponse()` callback forwards body data
and `{"end_stream": True}` sentinel through the channel; `readData()` reads
from it via `recv()`.  This avoids blocking on the async I/O thread.

### C++ Layer Changes

**QuicSession** needs:
1. `openBidiStream(headers)` — submit HEADERS without END_STREAM via
   `nghttp3_conn_submit_request()` with a deferred data provider
2. `sendStreamData(stream_id, data, end_stream)` — feed DATA to nghttp3's
   deferred data provider; signal `pending_write_`
3. Stream lifecycle: don't call `markStreamComplete()` when server sends
   END_STREAM if the client side is still open (same fix as HTTP/2 — use
   half-closed remote state)

**Socket / QoreSocketObject** needs:
1. `submitQuicBidiRequest()` — like `submitQuicRequest()` but without
   END_STREAM on the initial HEADERS
2. `sendQuicStreamData(session_id, stream_id, data, end_stream)` — send
   DATA on an existing stream
3. `readQuicStreamDataBlock()` — already exists (server side uses it);
   needs to work for client-side streams too

### Qore Layer Changes

**Http3ClientPollOperationImpl.qc**:
- Add `submitBidiRequest()` — calls `sock.submitQuicBidiRequest()`
- Add `sendStreamData(stream_id, data, end_stream)` — calls
  `sock.sendQuicStreamData()`
- Modify `handleReading()` to support incremental response delivery
  (streaming callbacks, not just final response)

**Http3ClientConnectionImpl.qc**:
- Add `sendStreamData()` proxy (like HTTP/2)
- Add `openBidiStream()` method

**Http3ClientStreamHandleImpl.qc**:
- Add `sendData()`, `writesDone()`, `readData()`, `readTrailers()`
- Mirror `Http2ClientStreamHandleImpl` API

**HttpClientStreamHandle.qc** (base class):
- `sendData()` and `writesDone()` should be in the base class (currently
  only in Http2ClientStreamHandleImpl)

### Stream Lifecycle (RFC 9114 §4.1)

HTTP/3 request streams are bidirectional.  The stream state machine:

```
Client sends HEADERS (no END_STREAM)  →  stream "open"
Server sends HEADERS                  →  still "open"
Server sends DATA                     →  still "open"
Server sends HEADERS+END_STREAM       →  "half-closed (remote)" from client view
Client sends DATA                     →  still "half-closed (remote)"
Client sends DATA+END_STREAM          →  "closed"
```

`QuicSession::markStreamComplete()` must NOT erase client streams when only
the server has sent END_STREAM — same pattern as the HTTP/2 fix in
`Http2Session::markStreamComplete()`.

`QuicSession::takeCompletedStream()` must keep the stream in `streams_` (like
CONNECT tunnels) when the client side is still open, returning a copy for
response delivery.

### Flow Control Considerations

QUIC has per-stream and per-connection flow control.  For bidi streaming:
- Client sends are subject to the server's `initial_max_stream_data_bidi_remote`
- `sendQuicStreamData()` must handle `STREAM_DATA_BLOCKED` (backpressure)
- Use the existing `waitForQuicStreamDrain()` CV-based mechanism from the
  server-side streaming code

### Impact on module-grpc

Once HTTP/3 client bidi streaming is implemented, `GrpcClientStream` can
support gRPC streaming over HTTP/3 without changes — the same `sendData()` /
`writesDone()` / `readData()` API will be available on both HTTP/2 and HTTP/3
stream handles.

## Implementation Order

1. C++ `QuicSession`: `openBidiStream()`, `sendStreamData()`, stream
   lifecycle fix (half-closed remote)
2. C++ `Socket`/`QoreSocketObject`: `submitQuicBidiRequest()`,
   `sendQuicStreamData()` wrappers
3. Qore `Http3ClientPollOperationImpl`: `submitBidiRequest()`,
   `sendStreamData()`, incremental response handling
4. Qore `Http3ClientStreamHandleImpl`: `sendData()`, `writesDone()`,
   `readData()`, `readTrailers()`
5. Refactor `HttpClientStreamHandle` base class: move `sendData()` /
   `writesDone()` from Http2-specific to shared
6. Tests: mirror HTTP/2 bidi streaming tests for HTTP/3
7. module-grpc: enable gRPC-over-HTTP/3 streaming tests
