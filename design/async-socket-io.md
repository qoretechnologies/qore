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

- **I/O thread autostop race**: When the cache is empty for 2s, the I/O thread sets `io_exiting=true`
  and breaks out of the main loop.  A concurrent `submit()` could pass the lock-free `running` check
  (still true), push a `SubmitOp` to the cmdq, and the exiting thread would silently drop it during
  cleanup.  Fixed by: (1) setting `running=false` atomically with `io_exiting=true`, (2) re-queuing
  `SubmitOp` commands during exit cleanup instead of dropping them, (3) post-push verification in
  `submit()` — if the I/O thread exited during the push, `startIntern()` restarts it.
- **PROGRAM-ERROR shutdown race**: During program shutdown, `cancelByProgram` cancels all ops and
  flushes workers.  But the I/O thread could dispatch new callbacks between the flush and `ptid`
  being set, causing workers to hit `PROGRAM-ERROR`.  Fixed by `markProgramShuttingDown()` /
  `clearProgramShuttingDown()` on `QoreCallDispatcher` — workers check the set before calling
  `evalMethod` and silently discard callbacks for dying programs.
- **`releaseCurrentOp` leak**: `HttpAcceptPollOperationPriv` and `Http2PollOperationPriv` obtained
  their inner poll operation's private data via `getReferencedPrivateData()`, which adds an
  independent ref.  `releaseCurrentOp()` set `current_op = nullptr` without deref, leaking the
  private data (and all its indirect data: sockets, SSL sessions, H2 streams).  Fixed by adding
  `current_op->deref(xsink)` before nulling.  Any new wrapper that uses `getReferencedPrivateData()`
  must follow this pattern.
- **`SocketPollOperationBase(self)` tRef rule**: The `QoreObjectWeakRefHolder self` member already
  calls `tRef()` in its constructor, so subclass constructors using the
  `SocketPollOperationBase(QoreObject* self)` form must NOT call `self->tRef()` again — that
  double-tRef is never matched by a second tDeref and leaks the QoreObject's C++ memory.
  The `setSelf()` method is safe because `QoreObjectWeakRefHolder::reset()` stores the pointer
  without tRef, then adds exactly one tRef in the body.
- **`setValue` vs `setMemberValue` in QPP constructors**: Use `self->setValue("member", val, xsink)`
  (not `self->setMemberValue("member", cls, val, xsink)`) when initializing Qore members from a
  QPP constructor body.  `setMemberValue` goes through class-context member access with DGC
  object-counting that can produce a leaked ref when the object is later destroyed.  `setValue`
  performs a simple member assignment that matches the pattern used by `SocketPollOperation` and
  other working poll operations.
- **Avoid duplicate Qore member refs to the same object**: If a composite poll operation wraps an
  inner operation that already holds a Qore "sock" member pointing to a Socket, the outer operation
  must NOT also store the same Socket as its own "sock" member.  During member hash cleanup,
  `doDelete` on the Socket from the outer object sets status to `OS_DELETED` before the inner
  operation releases its ref, leaving the Socket with a non-zero ref count.
- **Inline handler dispatch**: `onHttpRequest()` must never run inline on a `call_dispatcher` worker.
  Body-reading requests call `processNativeRequest()` which does synchronous socket I/O — this
  interferes with the async I/O model when run on the wrong thread.  All requests without an inline
  result from `tryInlineRequest()` must be dispatched via `handler_pool`.
### Socket Lifecycle Guarantee

The C++ layer guarantees correct socket fd cleanup — Qore-level code must not be required to call
`close()` for correctness.  Two mechanisms enforce this:

1. **Poll operations close on EOF:** When `recv(MSG_PEEK)` returns 0 (EOF) or a fatal error in
   `continuePoll()`, the operation calls `closeIo()` / `close()` immediately before returning the
   terminal state.  This prevents CLOSE_WAIT fd accumulation — the kernel holds fds in CLOSE_WAIT
   indefinitely until the application explicitly closes them.

2. **Controller auto-close via `needsCloseOnComplete()`:** `SocketPollOperationBase` provides a
   virtual `needsCloseOnComplete()` (default: false).  Poll operations override it to return true
   for terminal states (CLOSED, TIMEOUT, ERROR) where the connection is dead.  In Phase 3, after
   removing a completed operation from the cache, the controller checks this method and auto-closes
   the socket if true.  This is defense-in-depth: even if a poll operation's `continuePoll()` forgets
   to close, the controller catches it.

SOCK_DGRAM sockets (HTTP/3 QUIC shared UDP) are exempt — closing them would break other sessions
sharing the fd.

### Poll Operation Reference Lifecycle

A poll operation wrapper class typically holds two kinds of state with overlapping lifetimes:

1. A `unique_ptr<AbstractPollState> poll_state` that owns whatever inner connect/read/write/SSL/QUIC
   poll state is currently driving the operation.  Inner poll states commonly capture **raw pointers**
   into the underlying socket (e.g. `qore_socket_private*`) and dereference those pointers from
   their destructor (e.g. to close racing fds, release SSL handles, or trim QUIC sessions).
2. A refcounted reference to the owning object (`QoreSocketObject* sock`, `QoreHttpClientObject*
   client`, etc.) whose private data the inner poll state's raw pointers refer to.

**The wrapper's `deref()` (or any other path that releases the owning reference) must release
`poll_state` BEFORE deref'ing the owning object.**  Concretely:

```cpp
void MyPollOperation::deref(ExceptionSink* xsink) {
    if (ROdereference()) {
        // ... any non-pointer cleanup (clearNonBlock, etc.) ...
        poll_state.reset();      // (1) destroy inner poll state while sock is still alive
        sock->deref(xsink);      // (2) may free sock
        delete this;             // (3) safe — poll_state is already null
    }
}
```

If steps (1) and (2) are reversed, `delete this` runs `~unique_ptr<AbstractPollState>` after `sock`
has already been freed, and the inner poll state's destructor dereferences a dangling pointer.
The same rule applies to `abort()`-style paths and any other code that releases the owning
reference: `poll_state.reset()` first, then drop the ref.

This contract is required for **every** wrapper that holds both a `poll_state` and a refcounted
owning reference, including HTTPClient-level wrappers, WebSocket/SSE wrappers, and any future
poll-op composition.

### Sync I/O on Born-Non-Blocking Sockets

Since the AsyncIoController redesign, sockets are born **non-blocking** — the I/O thread expects
`O_NONBLOCK` to be set on every socket it owns, and worker code that calls into a socket while the
controller still owns it inherits that state.  Any sync code path that calls a raw I/O primitive
on such a socket — `::recv`, `::send`, `::accept`, `SSL_read`, `SSL_write`, `SSL_peek`,
`SSL_connect`, `SSL_accept` — must handle the non-blocking return values explicitly:

- Plain TCP: `EAGAIN` / `EWOULDBLOCK` — must wait via `isDataAvailable(timeout, ...)` /
  `isWriteFinished(timeout, ...)` and retry, not raise an error.
- SSL: `SSL_ERROR_WANT_READ` / `SSL_ERROR_WANT_WRITE` — must wait on the same primitives (or via
  `doSSLUpgradeNonBlockingIO()` for handshake paths) and retry.

Both `isDataAvailable()` and `isSocketDataAvailable()` accept `timeout_ms < 0` as "wait forever",
which preserves the historical "no explicit timeout" semantics.  `OptionalNonBlockingHelper` is a
no-op when the socket is already non-blocking, so RAII flipping is harmless on the new-style
controller-owned sockets but still correct on legacy blocking sockets.

`SSL_MODE_AUTO_RETRY` does **not** make a non-blocking socket behave like a blocking one — it only
covers internal TLS bookkeeping (NewSessionTicket, post-handshake renegotiation).  Sync SSL code
that relies on AUTO_RETRY for I/O retry on a non-blocking socket is incorrect.

#### Invariant: sync Socket APIs must never run on the async I/O controller thread

Sync Socket APIs block — either directly on a syscall or indirectly via
`isDataAvailable()`/`isWriteFinished()`/`asyncIoWait()`.  Running any such call on the
async I/O controller's own I/O thread would deadlock the controller: the wait primitives
block the very thread that is supposed to deliver the readiness events.

Every sync Socket entry point in `qore_socket_private` (`recv`, `recvAll`, `recvBinary`,
`recvBinaryAll`, `recvToOutputStream`, `send`, `sendFromInputStream`,
`sendHttpChunkedBody{FromInputStream,Trailer}`, `sendHttp{Message,Response}`,
`readHTTPHeader`, `readHTTPHeaderString`, `readHttpChunkedBody{,Binary}`, `accept_internal`
— except the special `timeout_ms == 0` single-shot non-blocking path — `connectINET`,
`connectUNIX`, `upgradeClient/ServerToSSLIntern`) and the higher-level wrappers
(`QoreHttpClientObject::send_internal`, `QoreHttpClientObject::connect`,
`qore_ftp_private::checkConnectedUnlocked`, `qore_ftp_private::connect`) calls
`SocketSyncPoll::assertNotOnIoThread()` before doing any blocking work.  Violation raises
`SOCKET-SYNC-ON-IO-THREAD-ERROR` in both debug and release builds; debug builds additionally
`abort()` so the core file points at the offending frame.

In the current architecture this invariant is held **structurally**: Qore poll operations
are always worker-dispatched, `onComplete` callbacks go to the worker pool, and queue
pushes are non-blocking.  User Qore code therefore never runs on the I/O thread and the
assertion is defense-in-depth — it catches misuse introduced by future refactors or by
C++ poll states that accidentally call into a sync Socket path.

The canonical bridge from sync to async is `SocketSyncPoll::run(state, timeout_ms, ...)`
(`include/qore/intern/SocketSyncPoll.h`), which loops an `AbstractPollState`'s
`continuePoll()` and waits for `SOCK_POLLIN` / `SOCK_POLLOUT` between iterations.  New sync
Socket APIs should be implemented as thin wrappers over this helper rather than
hand-rolled EAGAIN / `WANT_READ` retry loops.

If a poll-state needs to wake on more than just the Socket's primary fd (e.g. a QUIC
migration candidate watching a second UDP fd), it can override
`AbstractPollState::getExtraWaitFds()` to return a vector of `ExtraWaitFd` entries.
`SocketSyncPoll::run()` notices a non-empty vector and calls the internal `waitMultiFd()`
helper instead of `asyncIoWait()`, building a combined `pollfd` array (primary fd +
extras) and calling `::poll()` directly so any fd's readiness wakes the wait.

#### Lock-yielding during sync wait phases

The sync I/O helpers in `qore_socket_private` (`brecv`, `sendIntern`, `accept_intern`,
`doSSLRW`, `doSSLUpgradeNonBlockingIO`) all need to wait for readiness between retries.
Historically the **outer Socket mutex** (`my_socket_priv::m`) was held during the wait,
which blocked concurrent async I/O controller operations on the same Socket — a
long-running sync wait in a handler thread would stall unrelated async ops until it
returned.

The fix is **Option B**: release the outer mutex during the wait and re-acquire it before
retrying the syscall.  To detect a racing close/fd swap during the unlocked window, every
sync helper uses `SocketSyncPoll::waitReleasingLock(sock, outer_lock, ...)`, which:

1. Snapshots `qore_socket_private::fd_generation` and the primary fd under the lock.
2. Releases the lock via `AutoUnlocker`.
3. Calls `asyncIoWait()` to wait for readiness on the primary fd (or hits the timeout).
4. Re-acquires the lock on scope exit.
5. Checks the snapshotted generation against `pinfo.fd_generation`.  On mismatch, raises
   `SOCKET-CLOSED` — the fd the caller was waiting on has been closed or swapped
   (`close_and_reset`, QUIC migration, etc.) and any retry would operate on stale state.

`fd_generation` is bumped in every code path that closes or swaps the fd:

- `qore_socket_private::close_and_reset()` — on every close
- `SocketQuicClientPollOperation::migrateConnection()` — after the migration fd swap

The back-pointer from `qore_socket_private::outer_lock` to `my_socket_priv::m` is wired by
the `my_socket_priv` constructors (out-of-line in `lib/QoreSocket.cpp` because the priv
struct is only forward-declared in `QC_Socket.h`).  Bare `QoreSocket` instances (those
not wrapped in a `my_socket_priv`) leave `outer_lock == nullptr`; the sync helpers fall
back to the original non-yielding wait path in that case, which is safe because there is
no outer mutex to contend for.

**SSL full-yield safety.**  The OpenSSL API forbids concurrent calls on the same `SSL*`.
The lock-yielding wait preserves this invariant because direction-level exclusion via
`NB_SEND`/`NB_RECV` is already enforced at the sync Socket API layer — no other thread
can start a concurrent same-direction SSL operation on the SSL* while we are mid-retry.
The `SSL_read`/`SSL_write` calls themselves remain under the outer mutex; only the
between-retry wait phases yield.

**Debug test hook.**  Debug builds expose a `dbg_force_fd_swap_next_wait(Socket)`
builtin (in `lib/ql_debug.cpp`) that arms a one-shot flag on the Socket.  The next
lock-yielding wait on that Socket will bump `fd_generation` inside the wait window, so
the re-acquire detects the simulated swap and the sync I/O operation aborts with
`SOCKET-CLOSED`.  See `examples/test/qore/classes/Socket/Socket.qtest`
`fdGenerationTest` for the end-to-end regression test.  The hook is DEBUG-only — the
builtin is simply not registered in release builds.

### QUIC Keepalive Policy

QUIC keepalive (`ngtcp2_conn_set_keep_alive_timeout`) must be **gated on active stream count** for
client connections, mirroring curl's `cf_ngtcp2_setup_keep_alive` (`lib/vquic/curl_ngtcp2.c`).
The three-state model:

1. Peer announced no `max_idle_timeout`            → keepalive disabled (`UINT64_MAX`)
2. No active streams (`streams_.empty()`)          → keepalive disabled (`UINT64_MAX`)
3. Streams in flight                               → keepalive = `peer.max_idle_timeout / 2`

State (2) is the critical case: an idle client connection with no streams must let the **peer's**
idle timer close it.  If we keep pinging the peer ourselves we reset the peer's idle timer on every
ping and the connection lives forever — accumulating UDP fds, an entry in the controller cache, and
recurring `ngtcp2_conn_get_expiry()` work on the I/O thread for as long as the process runs.

The transition into state (2) is triggered by removing the last stream from `streams_`.  Every code
path that mutates `QuicSession::streams_` (add, erase, clear) must call `updateKeepAliveLocked()`
under `mtx_`.  Adding a new stream-mutation site without that call silently re-introduces the leak.

Long-lived multiplexed connections (CONNECT tunnels, server-initiated streams, WebSocket-over-H3)
keep at least one entry in `streams_` for their lifetime, so the helper sees `!streams_.empty()`
and keepalive stays at half-idle for those — no regression for active multiplexed use.

`set_keep_alive_timeout(UINT64_MAX)` takes effect on the **next** `ngtcp2_conn_get_expiry()` call
without needing any auxiliary signal — ngtcp2 recomputes the keepalive expiry dynamically each time
from `keep_alive.timeout`, and a value of `UINT64_MAX` returns `UINT64_MAX` from the expiry helper.
No `pending_write_` bump is required.

### Thread Scaling

I/O thread and callback worker thread counts are capped at `hardware_concurrency()` (the number of
logical CPUs), not a fixed constant.  I/O threads are CPU-bound (epoll/kqueue + continuePoll), so
more threads than CPUs adds context switching overhead without benefit.  The `QORE_IO_THREADS` env
var accepts any positive integer; `setMaxIoThreads(0)` auto-detects from `hardware_concurrency()`.

Any changes to queue or EventNotifier handling must preserve the acknowledge-and-recheck protocol described
above. Any changes to HTTP/2 frame processing must preserve the flush-after-receive pattern and the extended
CONNECT rejection layers.
