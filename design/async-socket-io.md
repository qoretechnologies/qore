# Async Socket I/O Design

## Overview

The AsyncSocketIo module provides a shared async socket I/O controller that powers HttpServerAsyncIo, Http2ClientIo,
and other components that rely on non-blocking socket polling. The core abstraction is a SocketPollOperation that
represents a unit of work (accept, read/write, handshake, etc) executed by the I/O thread that polls
all active sockets via a shared EventLoop (kqueue/epoll/poll).

This document describes the controller's internal model, the Http2ClientIo client module, and the supported
integration points for other code.

## Components

- AsyncSocketIoController (qlib/AsyncSocketIo/AsyncSocketIoController.qc)
  - Owns the I/O thread and an EventNotifier for cross-thread signaling.
  - Maintains a cache of SocketPollOperation instances and an EventLoop for efficient polling.
  - Processes commands (Add, Cancel, CancelOwner, Quit, Wake) from a command queue.
  - Delivers results via callback (invoked in the I/O thread) or Queue.

- HttpServerAsyncIo (qlib/HttpServerAsyncIo)
  - Uses AsyncSocketIoController to perform accept and I/O operations.
  - Provides higher-level HTTP request handling and response writes.

- Http2ClientIo (qlib/Http2ClientIo)
  - Connection manager, connection, stream handle, and poll operation classes for HTTP/2 client multiplexing.
  - Integrates with AsyncSocketIoController for event-driven I/O, or runs standalone with sync polling.

- SocketPollOperation (C++ and Qore types)
  - A pollable operation that exposes state and poll interests.
  - Includes SSL/TLS negotiation state for accept operations where needed.
  - SocketHttp2ClientMultiplexPollOperation (C++) handles HTTP/2 client multiplexing at the frame level.

## Thread Model

- **I/O thread** — one per controller instance
  - Blocks in EventLoop::poll() over all registered sockets plus the EventNotifier.
  - Wakes on socket readiness or EventNotifier activity.
  - Processes commands, drives `continuePoll()` on operations, and delivers results directly.
  - Results are delivered via callback (called in the I/O thread) or pushed to a Queue.
  - The three-phase loop (snapshot → continuePoll → update/deliver) releases the lock during
    `continuePoll()` so worker threads can `submit()` concurrently.
  - At the end of Phase 1 (cache snapshot), `processed_seq` is set to `submit_seq` and
    `processed_cond` is broadcast.  This allows `waitForProcessing()` to block until the I/O
    thread has captured all previously submitted operations in its poll set.

- **Worker threads**
  - Submit operations via `submit()` or `exec()`.
  - The I/O thread delivers results back to workers via callback or Queue — there is no separate
    coordinator thread.
  - `submit()` increments `submit_seq` under the lock before waking the I/O thread.

## EventNotifier and Command Queue

The controller uses an EventNotifier (registered in the persistent EventLoop) to wake the I/O thread
whenever a command is enqueued. This replaces the previous pipe-based approach.

### Enqueueing (worker thread)

1. Acquire the controller lock (`m`).
2. Append the command to `cmdq`.
3. Release the lock.
4. Call `notifier.notify()` (outside the lock).

### Processing (I/O thread — `processCommands()`)

There is a race condition between enqueueing and notification:

- Worker: (1) enqueue command (with lock), (2) notify (without lock)
- I/O thread: (1) process commands (with lock), (2) acknowledge (without lock)

Race scenario: the I/O thread finds `cmdq` empty, the worker enqueues and notifies, then the I/O
thread acknowledges — consuming the notification without processing the new command.

The fix is a nested loop:

1. Acquire lock, drain `cmdq`, release lock.
2. Call `notifier.acknowledge()`.
3. Re-acquire lock, check if `cmdq` has new commands.
4. If new commands exist, loop back to step 1.
5. If `cmdq` is empty, return.

This guarantees every enqueued command is processed even when notifications and enqueues interleave.

### Commands

Commands are members of the `IoCommand` enum:

| Command       | Description                                    |
|---------------|------------------------------------------------|
| `Add`         | Add a new SocketPollOperation                  |
| `Cancel`      | Cancel a specific operation by key             |
| `CancelOwner` | Cancel all operations for a given owner       |
| `Quit`        | Stop the I/O thread                            |
| `Wake`        | Wake the I/O thread to re-poll (e.g., flush HTTP/2 frames) |

## Integration Guidelines

### submit()

```qore
*Queue submit(hash<SocketPollOperationInfo> info, *bool replace)
```

Submits an operation to the controller. The `SocketPollOperationInfo` hashdecl:

| Field         | Type                       | Description                                             |
|---------------|----------------------------|---------------------------------------------------------|
| `sock`        | `Socket`                   | Required. The socket to poll.                           |
| `spop`        | `AbstractPollOperation`    | Required. The poll operation to drive.                  |
| `poll_info`   | `*hash<SocketPollInfo>`    | Last poll info (normally omitted on first submit).      |
| `to`          | `date`                     | Timeout (default: 30s). Negative = no timeout.          |
| `owner`       | `string`                   | Required. Owner ID for `cancelByOwner()`.               |
| `other`       | `*hash<auto>`              | Free-form data returned with the result.                |
| `resultQueue` | `*Queue`                   | Optional shared result Queue.                           |
| `callback`    | `*code`                    | Optional completion callback (mutually exclusive with `resultQueue`). |
| `key`         | `*string`                  | Optional custom cache key (default: `sock.uniqueHash()`). |

When `callback` is provided, results are delivered by calling the callback in the I/O thread.
When `resultQueue` is provided, results are pushed to that Queue. Otherwise a new Queue is
created and returned.

The `replace` flag allows replacing an existing operation on the same key (used for connection
re-polling patterns like HTTP/2 persistent connections).

### exec()

```qore
hash<SocketPollResultInfo> exec(hash<SocketPollOperationInfo> info, *bool replace)
```

Blocking variant of `submit()`. Submits the operation and waits for the result via `Queue::get()`.
Cannot be used with callback mode.

### wake()

```qore
wake()
```

Sends a `Wake` command to the I/O thread, causing it to re-evaluate poll state. Used primarily
for HTTP/2 frame flushing when handler threads queue outbound frames while the I/O thread is
blocked in `poll()`.

### cancelByOwner()

```qore
int cancelByOwner(string owner)
```

Cancels all operations belonging to the given owner. Returns the number of operations canceled.
Blocks until all cancellations are complete (uses a Condition variable to synchronize with the
I/O thread). Used for bulk cleanup during shutdown (e.g., connection manager `closeAll()`).

### waitForProcessing()

```qore
bool waitForProcessing(*timeout to)
```

Blocks until the I/O thread's Phase 1 cache snapshot has captured all operations that were
submitted before the call. Returns `True` if all pending operations have been processed,
`False` if the timeout expired or the I/O thread is not running.

This provides a stronger guarantee than `waitReady()`: not only is the I/O thread running,
but it has actually polled all pending operations at least once. This is critical for test
and setup code where clients connect immediately after submitting accept operations — using
`waitReady()` alone allows a race where the I/O thread is running but hasn't yet entered
its poll loop with the new operations, causing clients to connect before the server is
listening.

**Mechanism**: `submit()` increments `submit_seq` under the controller lock. At the end of
Phase 1, the I/O thread sets `processed_seq = submit_seq` and broadcasts `processed_cond`.
`waitForProcessing()` captures the current `submit_seq` as the target and waits until
`processed_seq >= target`.

**Edge case**: If called before any `submit()`, both counters are 0 and it returns `True`
immediately (provided `tid != 0`). This is correct — there is nothing to wait for.

### cancel() / cancelByKey()

Cancel a single operation by socket unique hash or custom key.

## Http2ClientIo Module

The Http2ClientIo module provides a full HTTP/2 client multiplexing stack that integrates with
AsyncSocketIoController for event-driven I/O or runs standalone with synchronous polling.

### Http2ClientConnectionManager

`qlib/Http2ClientIo/Http2ClientConnectionManager.qc`

Connection pool manager with:

- **Connection pooling**: `hash<string, list<Http2ClientConnection>> pool` keyed by `"host:port"`.
  Protected by `RWLock pool_lock()`.
- **Load balancing**: `getConnection()` finds a connection with available stream capacity or creates
  a new one.
- **Controller integration**: All managers share the global AsyncSocketIoController singleton
  (via `getGlobalAsyncIoController()`). New connections are submitted to the controller with
  `to = -1s` (no timeout) and callback delivery. The callback re-submits the connection after
  handling each result, keeping it alive in the event loop.
- **Cleanup**: `closeAll()` calls `controller.cancelByOwner(self.uniqueHash())` for bulk cancellation
  of all managed connections.

### Http2ClientConnection

`qlib/Http2ClientIo/Http2ClientConnection.qc`

Wraps a Socket + Http2ClientPollOperation pair.

- **Thread safety**: `Mutex lock()` protects all state. `Condition ready_cond()` for waiting on
  connection readiness.
- **Single-driver pattern**: `bool driving` — only one thread drives the poll loop at a time.
  Other threads wait on `ready_cond` with a timeout.
- **Sync mode**: `drivePoll(timeout)` performs a single poll iteration. `driveToReady(timeout)`
  loops until the HTTP/2 handshake completes. Uses `EventNotifier poll_notifier()` to wake the
  blocked poll when new requests are submitted.
- **Controller mode**: `isControllerMode()` returns true when a controller reference is set.
  `submitRequest()` calls `controller_ref.wake()` after submitting to nghttp2, waking the I/O
  thread to pick up the new stream.
- **States**: `Connecting` → `Ready` → `Draining` (GOAWAY received) → `Closed`.

### Http2ClientStreamHandle

`qlib/Http2ClientIo/Http2ClientStreamHandle.qc`

Per-request handle providing three usage patterns:

- **Synchronous**: `request(method, path, headers, body)` — submits a request and blocks until
  the response arrives via `Condition response_cond()`.
- **Asynchronous**: `submitAsync(method, path, headers, body, callback)` — submits a request
  with a callback. The callback receives `(hash<auto> response, *hash<auto> error)`.
- **Streaming**: `startStreaming(method, path, headers)` initiates a streaming connection (e.g.,
  SSE). `readData(timeout)` reads from an internal `Queue data_queue`. An `end_stream` sentinel
  signals stream completion.

### Http2ClientPollOperation

`qlib/Http2ClientIo/Http2ClientPollOperation.qc`

Qore-level state machine wrapping the C++ multiplex poll operation.

**States:**

```
CONNECTING → SSL_UPGRADE → READING ⇄ WAIT_READ → CLOSED
```

| State           | Description                                                         |
|-----------------|---------------------------------------------------------------------|
| `connecting`    | TCP connection in progress (inner op: SocketPollOperationConnect)    |
| `ssl_upgrade`   | SSL/TLS handshake with ALPN for h2 negotiation                     |
| `reading`       | HTTP/2 connection active; reading frames and dispatching responses   |
| `wait_read`     | Paused after callback dispatch; `continueReading()` transitions back to `reading` |
| `closed`        | Connection closed (GOAWAY, error, or max empty reads)               |

**Callback dispatch pattern:**

1. `continuePoll()` calls `current_op.getOutput()` to get completed responses.
2. Looks up the stream callback under `stream_lock`.
3. Calls the callback outside the lock to prevent deadlock.
4. Sets `callback_dispatched = True` to signal the caller.
5. Transitions to `wait_read`. The caller checks `checkAndClearCallbackDispatched()` to decide
   whether to skip the next poll timeout (since a response was already delivered).

**Key methods:**

- `submitRequest(method, path, headers, body, callback, ctx)` — registers callback under
  `stream_lock` **before** submitting to nghttp2 (prevents race where response arrives before
  callback is registered).
- `continueReading()` — resumes reading after callback dispatch (`wait_read` → `reading`).
- `checkAndClearCallbackDispatched()` — returns true and clears the flag if a callback was
  dispatched during the last `continuePoll()`.

### SocketHttp2ClientMultiplexPollOperation (C++)

`include/qore/intern/QC_SocketPollOperation.h`

Low-level C++ poll operation for HTTP/2 client multiplexing.

**States (C++):**

| Constant           | Description                              |
|--------------------|------------------------------------------|
| `H2C_NONE`         | Initial state                            |
| `H2C_SEND_PREFACE` | Sending connection preface (SETTINGS)    |
| `H2C_RECV_PREFACE` | Receiving preface response               |
| `H2C_READING`      | Reading HTTP/2 frames                    |
| `H2C_CLOSED`       | Connection closed                        |

**Response queue**: `std::deque<QoreHashNode*> completed_responses` protected by `response_lock`.
`getOutput()` pops the front; `goalReached()` checks non-empty.

**CallbackGuard** — safe destruction pattern:

```cpp
struct CallbackGuard {
    std::mutex mutex;
    bool destroyed = false;
};
std::shared_ptr<CallbackGuard> callback_guard;
```

The `onStreamComplete` callback captures `callback_guard` by shared_ptr. The destructor sets
`destroyed = true` under the mutex. The callback checks the flag under the mutex before accessing
`this`, eliminating use-after-free when the poll operation is destroyed while a callback is
in flight.

**Session reuse**: At construction, checks `sock->priv->h2_session`. If it exists, the session
is reused (avoids recreating the nghttp2 session). The session is stored back on the socket for
the next poll operation on the same connection.

### Lock Hierarchy

The following lock ordering must be respected to prevent deadlocks:

1. `Http2ClientConnectionManager::pool_lock` (RWLock)
2. `Http2ClientConnection::lock` (Mutex)
3. `Http2ClientPollOperation::stream_lock` (Mutex)
4. `AsyncSocketIoController::m` (Mutex)
5. `SocketHttp2ClientMultiplexPollOperation::response_lock` (QoreThreadLock)

Key design decisions that prevent deadlock:
- Callbacks are delivered **outside** all locks.
- The I/O thread never calls back into the connection manager.
- `submit()` does not hold the caller's lock when calling into the controller.

## HTTP/2 Frame Flushing

HTTP/2 connections use nghttp2, which queues outbound frames (RST_STREAM, SETTINGS_ACK, GOAWAY, response
headers/data) in an internal buffer. These frames are not written to the wire until `sendPendingData()` is
called on the Http2Session. Failure to flush at the right points causes deadlocks where the remote peer waits
for a frame that is queued locally but never sent.

### Flushing in continuePoll() (C++ I/O thread)

`SocketHttp2ServerPollOperation::continuePoll()` processes inbound HTTP/2 frames via `receiveData()`. The
nghttp2 callbacks triggered during receive processing may queue outbound frames (e.g., SETTINGS_ACK in response
to SETTINGS, RST_STREAM for rejected streams). When `hasCompletedStreams()` returns true and the operation
transitions to `H2S_REQUEST_READY`, `sendPendingData()` **must** be called before returning. Otherwise, the
queued frames remain buffered and the client may block waiting for them.

This follows the nginx pattern: `ngx_http_v2_read_handler()` always calls `ngx_http_v2_send_output_queue()`
after processing received frames, ensuring all response frames generated during receive processing are flushed.

Reference: `~/src/nginx-1.29.4/src/http/v2/ngx_http_v2.c`

### Flushing from handler threads (Qore coordinator/handler threads)

When handler threads submit HTTP/2 responses via `submitHttp2Response()` (e.g., 501 for rejected WebSocket
CONNECT, 503 for handler pool failures), the response frames are queued in nghttp2 but the I/O thread may be
blocked in `poll()`. The handler thread **must** call `controllers[cinfo.controller_idx].wake()` after
submitting the response to wake the I/O thread and trigger a flush cycle.

The `wake()` call must be placed **outside** any try/catch block around the `submitHttp2Response()` call so
that the I/O thread is woken regardless of whether the submit succeeded or threw an exception.

### Client-side flushing

HTTP/2 clients (e.g., `sendHttp2Connect()`) must also flush pending data after each `receiveData()` call.
Processing received SETTINGS frames triggers SETTINGS_ACK generation in nghttp2, and the server may not
proceed until it receives the ACK. Without `sendPendingDataBlocking()` after receive, the client and server
can deadlock — each waiting for the other to send data that is queued but unflushed.

## HTTP/2 Extended CONNECT Rejection (RFC 8441)

When the server does not advertise `ENABLE_CONNECT_PROTOCOL` in its SETTINGS, extended CONNECT requests (i.e.,
CONNECT with a `:protocol` pseudo-header, used for WebSocket over HTTP/2) must be rejected. Different nghttp2
versions handle `:protocol` differently across platforms, so rejection is enforced at four layers:

### Layer 1: nghttp2 auto-rejection (some builds)

Some nghttp2 builds reject extended CONNECT at the frame level before calling application callbacks. The stream
is reset automatically, `onStreamCloseCallback` fires with `reset=true`, and `markStreamComplete()` adds the
stream to the completed queue.

**Handled by**: `SocketHttp2ServerPollOperation::getOutput()` in `lib/QoreSocket.cpp` skips streams where
`stream_info->reset` is true, preventing RST'd streams from reaching the application layer.

### Layer 2: onFrameRecvCallback (other builds)

Some nghttp2 builds accept the frame and deliver it to `onFrameRecvCallback`. When the stream has a
`:protocol` header but `ENABLE_CONNECT_PROTOCOL` is not set, the callback explicitly submits RST_STREAM via
`nghttp2_submit_rst_stream()`. `onStreamCloseCallback` sets `reset=true` when the RST_STREAM is sent during
`sendPendingData()`, and the stream is filtered by the same `getOutput()` check in Layer 1.

**Handled by**: `onFrameRecvCallback()` in `lib/Http2Session.cpp`.

### Layer 3: onInvalidHeaderCallback (defense-in-depth)

If nghttp2 considers `:protocol` an invalid header when `ENABLE_CONNECT_PROTOCOL` is not set, it calls
`onInvalidHeaderCallback`. The callback stores the `:protocol` value in `stream->connect_protocol` so that
Layer 2 (`onFrameRecvCallback`) can detect and reject the extended CONNECT. Without this, the header would
be silently dropped and the CONNECT processed without `:protocol`.

**Handled by**: `onInvalidHeaderCallback()` in `lib/Http2Session.cpp`.

### Layer 4: Client-side SETTINGS check (primary fix for silent drop)

On some nghttp2 versions (e.g., Alpine nghttp2 1.68.0), `:protocol` is silently dropped by the server's
nghttp2 without calling any callback — neither `onHeaderCallback` nor `onInvalidHeaderCallback`. The server
then processes a bare CONNECT (without `:protocol`), no RST_STREAM is sent, and the client times out.

The client detects this by checking the server's SETTINGS after they are received. If `ENABLE_CONNECT_PROTOCOL`
is not advertised, the client throws `HTTP2-CONNECT-ERROR` immediately instead of waiting for a response.
The check runs both before submitting the CONNECT (if SETTINGS have already been received) and in the receive
loop (after each `receiveData()` call processes incoming frames, which may include the server's SETTINGS).

**Handled by**: `sendHttp2Connect()` in `lib/QoreHttpClientObject.cpp` using
`Http2Session::isExtendedConnectRejected()`.

### NOTHING from getOutput()

When `getOutput()` returns NOTHING (because a RST'd stream was filtered or the read was empty),
`handleHttp2RequestReady()` in `HttpAsyncSocketIoController.qc` continues reading on the connection rather
than closing it. This allows the HTTP/2 connection to remain active for subsequent streams.

## Platform-Specific Fixes

### macOS accept() O_NONBLOCK Inheritance

File: `include/qore/intern/qore_socket_private.h`

On macOS/Darwin, `accept()` inherits `O_NONBLOCK` from the listening socket. In async I/O
mode, listener sockets are set non-blocking for `poll()` / `kqueue()`. Without clearing the
flag, accepted sockets perform non-blocking reads during SSL handshake, causing partial TLS
record reads that manifest as `SSL_R_PACKET_LENGTH_TOO_LONG`.

The fix clears `O_NONBLOCK` via `fcntl(F_SETFL)` on every accepted fd immediately after
`accept()` returns. The second `fcntl` return value is unchecked — failure is essentially
impossible on a just-accepted fd and non-critical.

### macOS kqueue Closed-fd Detection

File: `lib/QoreSocket.cpp`

On macOS, closing a monitored fd removes its kqueue registration without delivering an event.
If a socket is closed by another thread while the I/O thread is blocked in `kevent()`,
`kevent()` may return 0 (timeout) with no events for the closed fd. The fix scans all
monitored fds after `kevent()` returns and reports `SOCK_POLLERR` for any fd that has become
invalid (`fcntl(F_GETFD)` returns `EBADF`).

**Caller contract**: This means `Socket::poll()` may return results even when `kevent()`
returned 0 events. All callers handle this correctly:
- `AsyncSocketIoController` builds a `ready_sockets` presence map from the poll result list.
  A socket with `SOCK_POLLERR` is correctly treated as "ready", and `continuePoll()` detects
  the error state.
- `Http2ClientConnection::drivePoll()` ignores the `Socket::poll()` return value entirely.
- Test code checks for result presence, not specific event types.

## Failure Modes

Known failure modes include:

- **EventNotifier race**: Commands enqueued but notification consumed without processing. The nested
  acknowledge-and-recheck loop in `processCommands()` prevents this (see "EventNotifier and Command Queue").
- **Operations not resubmitted or removed correctly**, leading to hangs or timeouts.
- **HTTP/2 frames queued in nghttp2 but not flushed** to the wire (see "HTTP/2 Frame Flushing" above).
- **Handler thread submitting HTTP/2 responses without calling `wake()`**, leaving frames buffered until the
  next unrelated poll wakeup.
- **RST'd HTTP/2 streams reaching the application layer** (see "HTTP/2 Extended CONNECT Rejection" above).
  All four rejection layers must be maintained for cross-platform correctness.
- **Single-driver starvation** (client-side): If the driving thread blocks for too long in `drivePoll()`,
  other threads waiting on `ready_cond` may time out. Controller mode avoids this by delegating to the
  shared I/O thread.
- **SSL race after submit** (server-side): If clients connect immediately after `submit()` but
  before the I/O thread's Phase 1 snapshot captures the accept operation, the accept may not be
  polled in time. Combined with macOS `O_NONBLOCK` inheritance on accepted sockets, this produced
  `SSL_R_PACKET_LENGTH_TOO_LONG` in CI. Fixed by (1) `waitForProcessing()` for deterministic
  readiness, (2) clearing `O_NONBLOCK` on accepted fds, (3) kqueue closed-fd detection. Callers
  that need to connect immediately after submitting accept operations should use
  `waitForProcessing()` rather than `waitReady()`.
- **Callback dispatch ordering** (client-side): Stream callbacks are dispatched in the order responses
  complete (FIFO from `completed_responses` deque), not in submission order. Callers must not assume
  request-order delivery.
- **Use-after-free in stream callbacks**: The `CallbackGuard` pattern in
  `SocketHttp2ClientMultiplexPollOperation` prevents this — the destructor sets `destroyed = true` and
  callbacks check the flag before accessing the object.

Any changes to queue or EventNotifier handling must preserve the acknowledge-and-recheck protocol described
above. Any changes to HTTP/2 frame processing must preserve the flush-after-receive pattern and the extended
CONNECT rejection layers.
