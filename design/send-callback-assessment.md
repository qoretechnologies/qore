# Assessment: send_callback / reply_sent Elimination — Current Status

## Background

This work eliminates the `reply_sent` / direct socket write path in `AbstractStreamRequest::sendResponse()` (HttpServerUtil.qm) by introducing a `send_callback` response field. Instead of calling `sendHTTPResponseWithCallback()` directly on the socket from the handler thread, the callback is stored in the response hash and invoked by the response-sending layer. This also fixes a C++ `sizeof` bug in `sendHttpChunkedBodyFromInputStream()`.

## Completed Work

### 1. C++ sizeof bug (DONE)
- **File**: `include/qore/intern/qore_socket_private.h:3445`
- **Fix**: Changed `sizeof(max_chunk_size)` to `max_chunk_size`
- **Effect**: InputStream-based chunked responses now read the correct buffer size (e.g. 4096 bytes) instead of `sizeof(size_t)` (8 bytes on 64-bit)

### 2. send_callback implementation (DONE)
All Qore-level changes are complete and all Qore HTTP server tests pass:
- `HttpServerStreamCallback.qtest` — 32/32 tests pass
- `HttpServerAsyncIo.qtest` — 15/15 tests pass
- `HttpServerAsyncHttp2.qtest` — 17/17 tests pass
- `HttpServerAsyncHttp2Streaming.qtest` — 9/9 tests pass

Files modified in Qore:
- `HttpServerUtil.qm` — Added `send_callback`/`send_timeout` to hashdecls; replaced direct socket write + `reply_sent = True` with `rv.send_callback = \send()`
- `HttpServer.qm` — `sendReply()`, `doResponse()`, `sendResponseSync()`, `translateToRequestResult()` all handle `send_callback`; added `handlePersistentConnectionSync()`
- `HttpServerAsyncIo.qm` — Added `send_callback`/`send_timeout` to `HttpRequestResultInfo`
- `HttpAsyncSocketIoController.qc` — `send_callback` handling in fused and direct send paths
- `RestHandler.qm` — `raw_response` check includes `send_callback`

### 3. handlePersistentConnectionSync() — persistence continuity fix (DONE)
- **Problem**: After a persistent handler released persistence (e.g. between DataStream SELECT and ROLLBACK), the old code immediately returned the connection to async I/O, breaking thread affinity for thread-bound database transactions.
- **Fix**: Removed the early return to async I/O. Instead, we continue processing on the same thread with an idle timeout that returns to async I/O only when no more requests arrive.
- **Result**: The original PERSISTENCE-ERROR in Qorus `streams.qtest` is **fixed** — the transaction test (beginTransaction → INSERT → SELECT → ROLLBACK → SELECT) now passes.

### 4. Thread-local cleanup on exit from handlePersistentConnectionSync() (DONE)
- Added `on_exit { phi.clear(); removeUserThreadContext(); remove_thread_data("svc_listener"); }` to clean up when the function exits
- Added `svc_listener` cleanup to `getPersistentClosedNotification()` in both `sqlutil-v7.1.qsd` and `RemoteRestStreamRequestHandler.qc`

### 5. getStreamHandler() stale svc_listener defense (DONE)
- **File**: `AbstractQorusCoreService.qc` — `getStreamHandler()` method
- **Fix**: Changed the `Continue-Persistent` condition from `(!svc_listener)` to `(!svc_listener || !cx.uctx.persistent_data)` so that a stale `svc_listener` without active persistence data does not prevent persistence setup
- **Result**: `Continue-Persistent` requests now correctly establish persistence even when dispatched to a thread with stale `svc_listener` from a previous failed session

## Resolved Issue

### SQLUTIL-ERROR: "operation 'beginTransaction' can only be executed in a persistent call" (FIXED)

**Test**: `test/streams.qtest` — `sqlutilStreams()` test case, line 930 (`DbRemoteSend INSERT`)
**Symptoms**: 4 of 5 sub-tests pass; the 5th (DbRemoteSend INSERT) failed with the above error.
**Status**: Fixed by items 4 and 5 above.

## Root Cause Analysis

### The Connection Lifecycle Problem

When `handlePersistentConnectionSync()` takes over a connection for a persistent handler, it continues processing ALL subsequent requests on that connection until the idle timeout expires or the connection closes. This is by design — it preserves thread affinity.

The test sequence on a single `qrest` connection:

1. **Transaction test** (lines 811-839): beginTransaction → UPSERT → SELECT → ROLLBACK → SELECT
   - Persistence is established, then released by ROLLBACK/persistenceThreadTerminate()
   - Connection stays in `handlePersistentConnectionSync()` waiting for more requests

2. **GET request** (line 841): `qrest.get("remote/datasources/omquser/up")`
   - Processed inside `handlePersistentConnectionSync()` as a non-persistent request

3. **Timeout test** (lines 845-855): DataStream UPSERT with `Qorus-Connection: Persistent` and `timeout=1`
   - **Processed inside `handlePersistentConnectionSync()`** — this is a request on the same connection
   - `getStreamHandler()` sees `Persistent` header → sets up persistence → saves `svc_listener` to thread-local data
   - The timeout error occurs in the stream handler
   - Client calls `qrest.disconnect()` → server gets SOCKET-CLOSED
   - `handlePersistentConnectionSync()` exits → `on_exit` cleans up `uctx` but **NOT `svc_listener`**
   - **Thread returns to handler pool with stale `svc_listener` in thread-local data**

4. After disconnect, `qrest` creates a **new TCP connection** for subsequent requests.

5. **DbRemoteReceive SELECT tests** (lines 859-926): Multiple SELECTs via async I/O
   - These may or may not land on the thread with stale `svc_listener`

6. **DbRemoteSend INSERT** (line 930): `beginTransaction()` sends POST with `Qorus-Connection: Continue-Persistent`
   - If this request lands on the handler pool thread with stale `svc_listener`:
   - `getStreamHandler()` checks: `Continue-Persistent && !get_thread_data("svc_listener")`
   - `svc_listener` EXISTS (stale!) → condition is **FALSE** → **persistence setup is SKIPPED**
   - `cx.uctx` does NOT get `persistent_data`
   - `SqlUtilPersistentOperationBase::constructor()` checks `cx.uctx.persistent_data` → NOTHING
   - **Throws SQLUTIL-ERROR**

### Why This Is Timing/Thread-Dependent

The error only occurs when the `beginTransaction` request (step 6) is dispatched to the same handler pool thread that handled the timeout test (step 3). With a small handler pool, this is likely but not guaranteed — explaining why this could appear intermittent.

### Two Contributing Factors

**Factor 1: Incomplete thread-local cleanup in `handlePersistentConnectionSync()` on_exit**

The `on_exit` block at line 4223-4226 cleans up:
- `phi.clear()` — clears persistent handler info ✓
- `removeUserThreadContext()` — removes "uctx" from thread-local data ✓
- **Missing**: `remove_thread_data("svc_listener")` — NOT cleaned up ✗

**Factor 2: The `beginTransaction()` method uses `Continue-Persistent` header**

In `Streams.qc` line 507:
```qore
remote.post(uripath, NOTHING, {"Qorus-Connection": "Continue-Persistent"}, \info);
```

The `Continue-Persistent` code path in `getStreamHandler()` (line 424-425) requires `!get_thread_data("svc_listener")` to be true. With stale `svc_listener`, this condition fails and persistence setup is skipped. The `Persistent` header, by contrast, always triggers persistence setup regardless of `svc_listener`.

## Implemented Fixes

### Fix 1: Clean up `svc_listener` in `handlePersistentConnectionSync()` on_exit (APPLIED)

**File**: `HttpServer.qm`

The `on_exit` block now cleans up all thread-local data:

```qore
on_exit {
    phi.clear();
    removeUserThreadContext();
    remove_thread_data("svc_listener");
}
```

This ensures ALL thread-local data from persistent sessions is cleaned up when `handlePersistentConnectionSync()` exits, regardless of the reason (normal completion, SOCKET-CLOSED, timeout, or error).

### Fix 2: Defense-in-depth in `getStreamHandler()` (APPLIED)

**File**: `AbstractQorusCoreService.qc`

The `Continue-Persistent` condition was changed from:
```qore
if (is_persistent || (is_continue && !svc_listener)) {
```
to:
```qore
if (is_persistent || (is_continue && (!svc_listener || !cx.uctx.persistent_data))) {
```

This ensures that even if a stale `svc_listener` exists in thread-local data, persistence setup still proceeds when there is no active persistence data in the connection context.

### Design Note

The `Continue-Persistent` header is a design choice: it tries to reuse an existing persistent session rather than always creating a new one. This is correct behavior — the header is intended for subsequent operations within an already-established persistent session. The issue was purely stale thread-local data, not the header choice.
