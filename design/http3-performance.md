# HTTP/3 Performance: Analysis and Optimization Roadmap

## Current State (2026-02-26)

### Benchmark Results (localhost, release build, 500 iterations)

| Test | HTTP/1.1 | HTTP/2 | HTTP/3 | H3 vs H2 |
|------|----------|--------|--------|-----------|
| Sequential req/s (single conn) | 565 | 1,932 | 1,367 | 70.8% |
| Sequential avg latency | 1.75 ms | 0.52 ms | 0.73 ms | 1.40x |
| Sequential P99 latency | 2.81 ms | 0.77 ms | 0.87 ms | 1.13x |
| Concurrent 20 req/s | 941 | 1,675 | 2,716 | **1.62x** |
| Concurrent 50 req/s | 673 | 1,542 | 2,356 | **1.53x** |
| Latency P99 (20 in-flight) | 25 ms | 97 ms | **9.5 ms** | **10x better** |
| Latency P99 (50 in-flight) | 55 ms | 205 ms | **43 ms** | **4.8x better** |
| Large body 100KB MB/s | 14 | 17 | **77** | **4.5x** |
| Large body 512KB MB/s | 31 | 34 | **171** | **5.0x** |
| Connection setup | 1.8 ms | 1.8 ms | 5.0 ms | 2.8x slower |

**Summary:** HTTP/3 excels at concurrency, large body transfer, and tail latency.
The one area where HTTP/2 leads is sequential single-connection throughput (1.4x
faster), due to the inherent overhead of QUIC running in userspace vs TCP in the
kernel.

### Sequential Request Hot Path

A single sequential HTTP/3 request on an established connection goes through:

```
mgr.request(url, method, path)
  -> parse_url(url)                          # URL parsing every time
  -> getConnection(url_info)                 # RWLock read, pool scan
  -> new Http3ClientStreamHandle(conn)       # creates Mutex + Condition
  -> stream.request(method, path)
    -> conn.isReady()                        # Mutex lock
    -> submitAsync()
      -> stream handle lock                  # Mutex lock
      -> conn.submitRequest()
        -> connection lock                   # Mutex lock
        -> poll_op.submitRequest()
          -> stream_lock                     # Mutex lock
          -> sock.submitQuicRequest()        # C++ -> socket lock -> session mtx_
        -> poll_notifier.notify()
    -> drivePoll() loop
      -> connection lock                     # Mutex lock
      -> continuePoll()                      # C++ -> socket lock -> session mtx_
        -> recv(EAGAIN)                      # syscall
        -> sendPendingPackets()              # session mtx_ + sendto() syscall
        -> recv-after-send attempt           # syscall (usually EAGAIN)
      -> Socket::poll()                      # poll() syscall, blocks for response
      -> connection lock                     # Mutex lock
    -> drivePoll() again
      -> connection lock                     # Mutex lock
      -> continuePoll()                      # C++ -> socket lock -> session mtx_
        -> recv(response)                    # syscall
        -> ngtcp2/nghttp3 processing
        -> stream completes
      -> callback dispatched
      -> connection lock                     # Mutex lock
    -> state is Complete, return response
```

**~15 lock/unlock cycles** per request vs HTTP/2's ~2 (single socket lock held
throughout send+receive).

---

## Implemented Optimizations

These optimizations were implemented in the initial performance pass:

1. **Recv-after-send** (`lib/QuicPollOperations.cpp`): After `sendPendingPackets()`,
   try non-blocking recv before returning to poll(). On low-latency paths, the
   response may already be in the socket buffer.

2. **Eliminate non-blocking toggle** (`lib/QuicPollOperations.cpp`): Stay in
   non-blocking mode across RESPONSE_READY instead of 2x fcntl() per response.

3. **Lock-free `getActiveStreamCount()`** (`Http3ClientPollOperation.qc`): Qore int
   is atomic; removed lock for read-only access.

4. **Conditional `stream_capacity_cond.broadcast()`** (`Http3ClientPollOperation.qc`):
   Track waiter count; skip lock+broadcast when no STREAM_ID_BLOCKED waiters.

5. **Inline `handlePollResult()` in `drivePoll()`** (`Http3ClientConnection.qc`):
   Combine lock acquisitions for the callback-dispatched fast path.

**Result:** +7.6% sequential throughput, -21% P99 latency.

---

## Future Optimization Opportunities

### HIGH IMPACT

#### H1. Direct request API (bypass connection manager abstractions)

**Problem:** Each `mgr.request()` call creates transient objects and parses the URL:
- `parse_url()` on every request (string parsing + hash creation)
- `new Http3ClientStreamHandle` with Mutex + Condition (allocated + GC'd per request)
- `getConnection()` RWLock read + pool scan
- Multiple method calls through the abstraction layers

**Proposal:** Add a `requestOnConnection()` method to `Http3ClientConnection` that
submits a request and drives the poll loop directly, without creating intermediate
objects:

```qore
# Current (7+ method calls, object creation per request):
hash<auto> resp = mgr.request(url, "GET", "/path");

# Proposed (1 method call, no transient objects):
Http3ClientConnection conn = mgr.getOrCreateConnection(url);
hash<auto> resp = conn.requestSync("GET", "/path", headers, body, timeout);
```

The `requestSync()` method would:
- Submit the request directly on the poll operation
- Drive the poll loop inline (no StreamHandle, no callback indirection)
- Return the response hash directly

**Expected impact:** ~0.05-0.10ms per request (object creation + method dispatch).

**Complexity:** Medium. New method on `Http3ClientConnection`, existing API unchanged.

#### H2. Consolidate lock acquisitions in the request submission path

**Problem:** Submitting a request acquires 5+ locks sequentially:
1. Connection lock (state check)
2. Stream lock (capacity check + submit)
3. Socket lock (C++ submitQuicRequest)
4. Session mutex (nghttp3 submit)
5. Connection lock again (last_activity)

**Proposal:** Restructure so the connection lock is held for the entire submit path,
eliminating redundant state checks:

```qore
int submitRequest(...) {
    AutoLock al(lock);
    # Single state check
    if (state != Ready) { throw ... }
    last_activity = now_us();
    # Submit under connection lock (stream_lock acquired inside)
    return poll_op.submitRequest(...);
    # poll_notifier.notify() after unlock
}
```

This eliminates 2 lock/unlock cycles per request.

**Expected impact:** ~0.02-0.04ms per request.

**Complexity:** Low. Restructure existing lock ordering.

#### H3. Move sequential request hot path to C++

**Problem:** The Qore interpreter adds overhead per method call (~2-5us each).
The sequential request path makes ~15 Qore method calls per request.

**Proposal:** Add a C++ `requestSync()` method on `SocketQuicClientPollOperation`
that combines submit + poll + recv + response extraction in a single C++ function
call:

```cpp
// Single C++ call replaces: submitRequest + drivePoll loop + getOutput
QoreHashNode* SocketQuicClientPollOperation::requestSync(
    const char* method, const char* path,
    const strcase_str_map_t& headers,
    const void* body, size_t body_len,
    int timeout_ms, ExceptionSink* xsink);
```

This would:
1. Submit the request to nghttp3
2. Call `sendPendingPackets()` to send the request
3. Loop: recv → process → check completed, with poll() for blocking
4. Build and return the response hash

All under a single socket lock hold (matching HTTP/2's pattern).

**Expected impact:** ~0.10-0.15ms per request (eliminate Qore method dispatch +
lock overhead + condition variable signaling).

**Complexity:** High. New C++ method, exposed via QPP, Qore wrapper method.

### MEDIUM IMPACT

#### M1. Connection-level request pipelining

**Problem:** Sequential requests submit one at a time. The client blocks until the
response arrives before submitting the next request. This means the server is idle
while the client processes the response and prepares the next request.

**Proposal:** Allow pre-submitting the next N requests while waiting for responses.
The connection manager could accept a batch of requests and pipeline them:

```qore
list<hash<auto>> responses = mgr.requestBatch(url, requests);
```

Or expose a lower-level pipeline API:
```qore
conn.submitRequest("GET", "/path1", ...);
conn.submitRequest("GET", "/path2", ...);
# Drive poll until both complete
list<hash<auto>> responses = conn.driveUntilComplete(2, timeout);
```

**Expected impact:** Significant for batch workloads. The recv-after-send
optimization would catch responses much more often since the server is processing
request N while the client is sending request N+1.

**Complexity:** Medium. New API surface, existing poll machinery sufficient.

#### M2. 0-RTT QUIC resumption

**Problem:** QUIC connection establishment is 3x slower than TCP+TLS (5ms vs 1.8ms).
Each new QUIC connection requires a full handshake.

**Proposal:** Implement 0-RTT session resumption per RFC 9001 Section 4.6.1:
- Cache TLS session tickets from completed handshakes
- On reconnection to the same server, use the cached ticket for 0-RTT
- Send the first HTTP/3 request in the initial QUIC packet (0-RTT data)

This would reduce connection establishment from ~5ms to ~2ms (comparable to TCP+TLS)
for subsequent connections to the same server.

**Expected impact:** 60% reduction in connection setup latency for repeat connections.

**Complexity:** High. Requires ngtcp2 0-RTT API, TLS ticket storage, replay
protection.

#### M3. Server-side optimization: reduce wakeup latency

**Problem:** The server blocks in `poll()` between requests. When a client request
arrives, the server must:
1. Return from poll() (kernel wakeup latency)
2. Process the request through the async I/O framework
3. Run the handler
4. Send the response

For sequential requests, this adds ~30-60us of latency that could be reduced.

**Proposal:** On the server side, after sending a response, try a non-blocking
recv immediately (mirror of the client-side recv-after-send optimization).
If another request has already arrived (pipelined or from another client),
process it without going back to poll(). This is essentially a server-side
"busy poll" optimization.

**Expected impact:** ~0.02-0.05ms per request in sequential workloads.

**Complexity:** Medium. Change in `SocketQuicServerPollOperation::continuePoll()`.

#### M4. Reduce recursive mutex overhead in QuicSession

**Problem:** `QuicSession::mtx_` is a `std::recursive_mutex`, which is 2-3x slower
than `std::mutex` on Linux. It's recursive because some code paths (e.g.,
nghttp3 callbacks) re-enter the session while the mutex is held.

**Proposal:** Audit the lock acquisition patterns and restructure to use a regular
`std::mutex`. The recursive calls can be refactored to use an internal unlocked
helper + a public locked wrapper:

```cpp
// Instead of:
void QuicSession::foo() {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    bar();  // also acquires mtx_
}

// Use:
void QuicSession::foo() {
    std::lock_guard<std::mutex> lock(mtx_);
    barLocked();  // assumes lock is held
}
```

**Expected impact:** ~0.01-0.03ms per request (2-4 recursive mutex acquisitions
per request at ~5-10us overhead each).

**Complexity:** Medium. Requires careful audit of all lock acquisition call chains.

### LOW IMPACT (but good for code quality)

#### L1. Pre-allocate stream callback storage

**Problem:** `stream_callbacks` and `stream_ctxs` hashes are modified on every
request (insert on submit, remove on complete). Qore hash operations involve
string key hashing and memory allocation.

**Proposal:** Use a fixed-size array indexed by `stream_id % max_streams` instead
of a hash. QUIC stream IDs are sequential, so modular indexing works well.

**Expected impact:** ~0.005ms per request.

#### L2. Avoid body data copy in submitRequest

**Problem:** `QuicSession::submitRequest()` copies the request body into the
stream's `body_data_` buffer. For large POST bodies, this is an O(n) copy.

**Proposal:** Use a zero-copy approach: pass a pointer to the caller's buffer
and keep it alive until nghttp3 has consumed it (via the `acked_data` callback).

**Expected impact:** Negligible for small requests; significant for large POST bodies.

#### L3. Lock-free completed stream queue

**Problem:** `completed_streams_` queue in QuicSession is protected by `mtx_`.
The producer (nghttp3 callback) and consumer (continuePoll) are often the same
thread, but the lock is still acquired.

**Proposal:** Use a single-producer/single-consumer lock-free queue for the
completed streams path. The `has_completed_streams_` atomic flag already avoids
the lock for the "no completed streams" fast path; this would also avoid it for
the "has completed stream" path.

**Expected impact:** ~0.005ms per request.

---

## Architectural Considerations

### Why HTTP/3 is inherently slower for sequential single-connection

TCP+TLS (HTTP/2) benefits from:
- **Kernel-space transport:** TCP segmentation, acknowledgment, retransmission, and
  flow control all happen in the kernel. The application just calls `write()` and
  `read()` on the socket fd.
- **Single lock:** The Qore HTTP/2 implementation holds one socket lock for the
  entire send+receive cycle.
- **Stream abstraction:** TCP provides an ordered byte stream; HTTP/2 frames are
  written/read sequentially on this stream.

QUIC (HTTP/3) runs in userspace:
- **Per-packet overhead:** Every outgoing packet must be individually constructed,
  encrypted (AES-GCM), and sent via `sendto()`. Every incoming packet must be
  received via `recvfrom()`, decrypted, and processed through ngtcp2+nghttp3.
- **Multiple locks:** The QUIC session, socket, and Qore abstraction layers each
  have their own synchronization.
- **Datagram model:** UDP provides individual datagrams, not a stream. The QUIC
  layer must reconstruct ordering, handle loss, and manage flow control.

The inherent per-request overhead of QUIC vs TCP on localhost is approximately
0.10-0.15ms, which cannot be eliminated — it's the cost of running the transport
layer in userspace.

### Where HTTP/3 wins decisively

Despite the sequential overhead, HTTP/3 outperforms HTTP/2 in scenarios that
matter for real-world applications:

1. **No head-of-line blocking:** TCP packet loss stalls ALL HTTP/2 streams. QUIC
   packet loss only affects the specific stream(s) in that packet. This manifests
   as dramatically lower tail latency under concurrent load (P99: 9.5ms vs 97ms).

2. **Independent stream processing:** Each QUIC stream has its own flow control
   window. A slow stream doesn't block fast streams.

3. **Better large body throughput:** QUIC's congestion control operates per-stream,
   allowing better utilization of the available bandwidth. The 5x throughput
   advantage at 512KB likely reflects more efficient buffer management.

---

### Multi-connection QUIC on a single I/O thread

All H2 and H3 client connections share the same `AsyncSocketIoController` I/O
thread.  Fair scheduling across multiple QUIC connections is ensured by a
**per-connection packet budget** in the C++ `continuePoll()` implementation.

**Packet budget:** `SocketQuicClientPollOperation::continuePoll()` processes at
most `QUIC_CLIENT_RECV_BUDGET` (32) packets per call.  At ~1μs/packet this gives
~32μs per connection, ensuring the I/O thread cycles through all connections
before any QUIC timer expires.  When the budget is exhausted and more packets
remain, `continuePoll()` returns `poll_timeout_ms=0` to trigger an immediate
re-poll.

**Why HTTP/2 doesn't need a budget:** TCP connections have kernel-level buffering.
When the I/O thread is busy with connection A, data for connection B accumulates
in the kernel's TCP receive buffer.  TCP also has no userspace timers.

**Historical note:** Before the packet budget was introduced, QUIC connections
used dedicated I/O threads (`dedicated_thread: True`) to avoid starvation on the
shared event loop.  The dedicated-thread model didn't scale: N H3 connections
required N dedicated threads, causing throughput collapse at higher concurrency.
The packet budget + shared event loop eliminates this by providing fair scheduling
similar to how nginx handles all QUIC connections on the same event loop as TCP.

---

## Benchmark Reproduction

```bash
# Build release
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j$(nproc)

# Run cross-protocol benchmark
QORE_MODULE_DIR=qlib LD_LIBRARY_PATH=build build/qore \
    examples/test/qlib/HttpServer/http3_local_benchmark.qr 500 50 -v

# Run HTTP/3 stress tests
QORE_MODULE_DIR=qlib LD_LIBRARY_PATH=build build/qore --enable-debug \
    examples/test/qlib/HttpServer/HttpServerHttp3Stress.qtest -v
```
