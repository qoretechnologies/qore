# Cooperative Cancellation for Qore

## Overview

Qore provides cooperative cancellation at two levels:

| Level | Scope | Requested by | Exception |
|-------|-------|-------------|-----------|
| **Program interrupt** | All threads in a program | Sandbox controller via `SandboxManager::requestInterrupt()` | `PROGRAM-INTERRUPTED` |
| **Thread cancellation** | One specific thread | Any thread (same program) via `cancel_thread(tid)` | `THREAD-CANCELLED` |

Both levels are checked at the same **cancellation points** through a single C++ function. This document covers the architecture, the C++ and Qore APIs, and the implementation guide for binary modules.

### Use Cases

- **Sandbox environments** — stop runaway user code (program interrupt)
- **Web applications** — cancel request-handling threads on timeout (thread cancel)
- **Task supervisors** — cancel worker threads that exceed a deadline (thread cancel)
- **IDE integrations** — cancel long-running computations (either level)
- **Graceful shutdown** — cancel specific threads in dependency order (thread cancel)

## C++ API

### The Primary Check Function

Module authors call **one function** at cancellation points:

```cpp
#include <qore/qore_thread.h>

// Checks BOTH thread cancellation AND program interrupt.
// Returns true if cancelled/interrupted (exception already raised).
DLLEXPORT bool qore_check_cancel(ExceptionSink* xsink,
    const char* operation = "operation");
```

This function:
1. Checks the per-thread cancellation flag (one atomic load — cheap)
2. Checks the program-level interrupt via `QoreSandboxManagerHelper`
3. Raises the appropriate exception (`THREAD-CANCELLED` or `PROGRAM-INTERRUPTED`) if either is set
4. Returns `false` with zero overhead when neither is active

**This replaces `qore_check_io_interrupt()`**, which has been removed.

### Thread Cancellation Control

```cpp
// Request cancellation of a specific thread (same program only).
// Returns 0 on success, -1 if thread not found/not active/wrong program.
DLLEXPORT int qore_cancel_thread(int tid, const char* reason = nullptr);

// Clear the cancellation flag for the current thread.
DLLEXPORT void qore_clear_thread_cancel();
```

### Lower-Level APIs

These are used internally and by `QoreCondition::waitWithInterrupt()`. Module authors
normally don't need these — use `qore_check_cancel()` instead.

```cpp
// QoreSandboxManagerHelper — RAII access to the program's sandbox manager.
// Acquires a strong reference under lock, preventing use-after-free.
QoreSandboxManagerHelper smh;
if (smh) {
    smh->isInterruptRequested();          // non-throwing check
    smh->checkIOInterrupt(xsink, "op");   // throwing check
    smh->registerCancelCallback(ctx, cb); // register cancel callback
    smh->unregisterCancelCallback(ctx);   // unregister cancel callback
}

```

### Constants

```cpp
// Recommended polling interval for blocking operations (500ms)
#define QORE_IO_POLL_INTERVAL_MS 500
```

## Implementation Patterns for Modules

### Pattern 1: Pre-Operation Check

Always check before starting a potentially blocking operation:

```cpp
int myBlockingOperation(ExceptionSink* xsink) {
    if (qore_check_cancel(xsink, "my operation")) {
        return -1;  // Exception already raised
    }

    // Proceed with operation...
}
```

### Pattern 2: Polling During Long Operations

For operations that may block for extended periods, use polling with timeouts:

```cpp
ssize_t myLongRead(void* buf, size_t len, int timeout_ms, ExceptionSink* xsink) {
    // Check before starting
    if (qore_check_cancel(xsink, "reading data")) {
        return -1;
    }

    // Poll with short timeouts
    int remaining = timeout_ms;
    while (remaining > 0 || timeout_ms < 0) {  // timeout_ms < 0 means infinite
        if (qore_check_cancel(xsink, "reading data")) {
            return -1;
        }

        // Use chunk timeout (don't exceed remaining time)
        int chunk = QORE_IO_POLL_INTERVAL_MS;
        if (timeout_ms >= 0 && remaining < chunk) {
            chunk = remaining;
        }

        ssize_t rv = do_nonblocking_read_with_timeout(buf, len, chunk);

        if (rv > 0) {
            return rv;  // Success
        }
        if (rv < 0 && errno != ETIMEDOUT && errno != EAGAIN) {
            return rv;  // Real error
        }

        if (timeout_ms >= 0) {
            remaining -= chunk;
        }
    }

    errno = ETIMEDOUT;
    return -1;
}
```

### Pattern 3: Library Cancellation API (Cancel Callback)

If the underlying library provides a cancellation mechanism, register a cancel callback
so that the sandbox manager's `requestInterrupt()` can invoke it directly. This is the
recommended pattern for database modules.

```cpp
class MyDatabaseConnection {
    db_handle_t* handle;

public:
    int executeQuery(const char* sql, ExceptionSink* xsink) {
        // Pre-check
        if (qore_check_cancel(xsink, "executing query")) {
            return -1;
        }

        // Register cancel callback (RAII)
        QoreSandboxManagerHelper smh;
        if (smh) {
            smh->registerCancelCallback(this, [this]() -> bool {
                db_cancel_query(handle);
                return true;
            });
        }

        int rc = db_query_sync(handle, sql);

        // Unregister (also happens in destructor of smh, but explicit is clearer)
        if (smh) {
            smh->unregisterCancelCallback(this);
        }

        return rc;
    }
};
```

#### RAII Cancel Helper Pattern

For cleaner code, create an RAII helper class (as done in module-oracle, module-pgsql, module-sybase):

```cpp
class MyCancelHelper {
    QoreSandboxManagerHelper smh;

public:
    MyCancelHelper(db_handle_t* handle) {
        if (smh) {
            smh->registerCancelCallback(handle, [handle]() -> bool {
                db_cancel_query(handle);
                return true;
            });
        }
    }

    ~MyCancelHelper() {
        if (smh) {
            smh->unregisterCancelCallback(/* context */);
        }
    }
};

// Usage:
int executeQuery(const char* sql, ExceptionSink* xsink) {
    if (qore_check_cancel(xsink, "executing query")) {
        return -1;
    }
    MyCancelHelper cancel_guard(handle);
    return db_query_sync(handle, sql);
}
```

### Pattern 4: Custom Stream/Socket Wrapper

For libraries that support custom I/O callbacks (e.g., MongoDB), wrap the stream:

```cpp
static ssize_t interruptible_read(library_stream_t* stream, void* buf,
                                   size_t len, int timeout_ms) {
    interruptible_stream_t* s = (interruptible_stream_t*)stream;

    if (qore_check_cancel(/* no xsink in callback — use errno */)) {
        errno = EINTR;
        return -1;
    }

    // Polling read with cancel checking
    int remaining = timeout_ms;
    while (remaining > 0 || timeout_ms < 0) {
        if (qore_is_cancel_requested()) {
            errno = EINTR;
            return -1;
        }

        int chunk = (remaining > QORE_IO_POLL_INTERVAL_MS || timeout_ms < 0)
                    ? QORE_IO_POLL_INTERVAL_MS : remaining;

        ssize_t rv = library_stream_read(s->base, buf, len, chunk);
        if (rv >= 0 || (errno != ETIMEDOUT && errno != EAGAIN)) {
            return rv;
        }

        if (timeout_ms >= 0) {
            remaining -= chunk;
        }
    }

    errno = ETIMEDOUT;
    return -1;
}
```

### Pattern 5: Periodic Check in Fetch Loops

For row-by-row fetching, check every N rows to avoid excessive overhead:

```cpp
int rows_fetched = 0;
while (db_fetch_row(stmt)) {
    if (++rows_fetched % 100 == 0 && qore_check_cancel(xsink, "fetching rows")) {
        return -1;
    }
    process_row(stmt);
}
```

## Database Driver Considerations

### Common DB Library Cancellation APIs

| Database | Cancellation API | Notes |
|----------|------------------|-------|
| Oracle OCI | `OCIBreak(svchp, errhp)` | Cancels current operation on service context |
| MySQL | `mysql_kill(conn, thread_id)` | Kills query from another connection |
| PostgreSQL | `PQcancel(cancel, errbuf, len)` | Requires PGcancel object from `PQgetCancel()` |
| SQLite | `sqlite3_interrupt(db)` | Safe to call from any thread |
| ODBC | `SQLCancel(stmt)` | Cancels statement execution |
| FreeTDS/Sybase | `ct_cancel(conn, cmd, CS_CANCEL_ALL)` | Cancels current command batch |
| JDBC (via JNI) | `Statement.cancel()` | Thread-safe query cancellation |

### Implementation Strategy by Library Type

**Type A: Cancel from Another Thread (Recommended)**
- Execute query synchronously in the current thread
- Register a cancel callback that calls the library's cancel API
- The callback is invoked from the interrupting thread when `requestInterrupt()` or `cancel_thread()` triggers
- Examples: Oracle (`OCIBreak`), PostgreSQL (`PQcancel`), Sybase (`ct_cancel`), SQLite (`sqlite3_interrupt`), ODBC (`SQLCancel`)

**Type B: Async API with Polling**
- Start query asynchronously
- Poll for completion with cancel checking between polls
- Cancel if interrupted
- Examples: Some Oracle OCI modes, modern PostgreSQL async

**Type C: Custom I/O Callbacks**
- Wrap socket/stream operations with cancel-aware I/O
- Check cancel during read/write
- Examples: MongoDB (libmongoc custom streams)

**Type D: Cross-Connection Cancel**
- Execute query on one connection
- Cancel from another connection/handle
- More complex; requires managing a second handle
- Examples: MySQL (`mysql_kill` requires separate MYSQL handle)

### Current Module Status

| Module | Cancel API Used | Cancel Callback | Pre-Checks | Periodic Fetch Checks |
|--------|----------------|-----------------|------------|----------------------|
| module-oracle | `OCIBreak()` | `QoreOracleCancelHelper` | Yes | Yes (rows + LOB chunks) |
| module-pgsql | `PQcancel()` | `QorePGCancelHelper` | Yes | — |
| module-sybase | `ct_cancel()` | `QoreSybaseCancelHelper` | Yes | Yes (every 100 rows) |
| module-mysql | — | **TODO** | Yes | Yes (every 100 rows) |
| module-odbc | — | **TODO**: `SQLCancel()` | Yes | Yes (every 100 rows) |
| module-sqlite3 | — | **TODO**: `sqlite3_interrupt()` | **TODO** | **TODO** |
| module-ssh2 | N/A (polling) | N/A | Yes | Yes (wait loop) |
| module-openldap | — | **TODO**: `ldap_abandon_ext()` | Yes | — |
| module-zmq | N/A (polling) | N/A | Yes | Yes (poll loop) |
| module-process | — | **TODO**: `terminate()` | **TODO** | **TODO** |
| module-jni | — | **TODO**: `Statement.cancel()` | Minimal | — |
| module-python | — | **TODO**: `PyErr_SetInterrupt()` | GIL-level | — |
| module-v8 | — | **TODO**: `TerminateExecution()` | **TODO** | **TODO** |
| mongodb (in-tree) | N/A (stream wrapper) | N/A | Yes | Yes (stream I/O) |

All modules with existing `qore_check_io_interrupt()` calls need a mechanical replacement to `qore_check_cancel()`.

## Qore-Level API

### Thread Cancellation

```qore
#! Requests cooperative cancellation of a thread
/** @param tid the thread ID to cancel
    @param reason optional reason string included in the THREAD-CANCELLED exception

    The target thread will receive a THREAD-CANCELLED exception at the next
    cancellation point (loop iteration, blocking wait, sleep, I/O operation).

    @return True if the cancellation request was delivered, False if the thread
    was not found or not active

    @throw THREAD-CANCEL-ERROR cannot cancel the current thread (use throw instead),
    cannot cancel TID 0

    @par Example:
    @code{.py}
    int tid = background long_running_task();
    sleep(5s);
    cancel_thread(tid, "timeout exceeded");
    @endcode

    @since Qore 2.3

    @see thread_cancelled()
    @see clear_thread_cancel()
*/
bool cancel_thread(int tid, *string reason);

#! Returns True if cancellation has been requested for the current thread
/** @since Qore 2.3 */
bool thread_cancelled();

#! Clears the cancellation flag for the current thread
/** Call this after catching a THREAD-CANCELLED exception if the thread
    should continue running (e.g., to complete cleanup operations).

    @since Qore 2.3
*/
nothing clear_thread_cancel();
```

### Program Interrupt (Existing)

```qore
SandboxManager sm();
pgm.setSandboxManager(sm);
sm.requestInterrupt();   # interrupt all threads
sm.clearInterrupt();     # clear the flag
sm.isInterruptRequested();
```

## Architecture

### Per-Thread Cancellation Flag

Added to `ThreadEntry` in `QoreThreadList.h`:

```cpp
class ThreadEntry {
public:
    // ... existing fields ...

    // Per-thread cooperative cancellation flag
    std::atomic<bool> cancel_requested{false};

    // Optional cancellation reason
    QoreStringNode* cancel_reason = nullptr;

    DLLLOCAL void cleanup() {
        // ... existing cleanup ...
        cancel_requested.store(false, std::memory_order_relaxed);
        if (cancel_reason) {
            cancel_reason->deref();
            cancel_reason = nullptr;
        }
    }
};
```

`ThreadEntry` is the right location because:
- Fixed global array indexed by TID — accessible from any thread without TLS lookup
- Already used for cross-thread operations (`cancelAllActiveThreads`)
- Cleaned up when the TID slot is released

### `qore_check_cancel()` Implementation

```cpp
bool qore_check_cancel(ExceptionSink* xsink, const char* operation) {
    // 1. Check thread-level cancellation (cheap: one atomic load)
    int tid = q_gettid();
    if (tid >= 0 && thread_list.entry[tid].cancel_requested.load(std::memory_order_acquire)) {
        QoreStringNode* reason = thread_list.entry[tid].cancel_reason;
        if (reason) {
            xsink->raiseException("THREAD-CANCELLED",
                new QoreStringNodeMaker("%s: thread %d cancelled: %s",
                    operation, tid, reason->c_str()));
        } else {
            xsink->raiseException("THREAD-CANCELLED",
                new QoreStringNodeMaker("%s: thread %d cancelled", operation, tid));
        }
        return true;
    }

    // 2. Check program-level interrupt
    QoreSandboxManagerHelper smh;
    if (smh && smh->checkIOInterrupt(xsink, operation)) {
        return true;
    }

    return false;
}
```

### `qore_cancel_thread()` Implementation

```cpp
int qore_cancel_thread(int tid, const char* reason) {
    if (tid <= 0 || tid >= MAX_QORE_THREADS) {
        return -1;
    }
    AutoLocker al(thread_list.lck);
    if (!thread_list.entry[tid].active()) {
        return -1;
    }
    // Security: same-program only
    ThreadData* td = thread_list.entry[tid].thread_data;
    if (td && td->current_pgm != getProgram()) {
        return -1;
    }
    if (reason) {
        if (thread_list.entry[tid].cancel_reason) {
            thread_list.entry[tid].cancel_reason->deref();
        }
        thread_list.entry[tid].cancel_reason = new QoreStringNode(reason);
    }
    thread_list.entry[tid].cancel_requested.store(true, std::memory_order_release);
    return 0;
}
```

### Cancellation Points in Qore Core

#### Loop Statements

All loop statements check at each iteration:

```cpp
// WhileStatement.cpp, DoWhileStatement.cpp, ForStatement.cpp, ForEachStatement.cpp
if (qore_check_cancel(xsink, "while loop")) {
    break;
}
```

#### Blocking Primitives

`QoreCondition::waitWithInterrupt()` always polls at 500ms intervals. This is used by
user-facing primitives (`Condition`, `Queue`, `Counter`, `Gate`, etc.):

```cpp
int QoreCondition::waitWithInterrupt(pthread_mutex_t* m, int64 timeout_ms,
                                      ExceptionSink* xsink) {
    if (qore_check_cancel(xsink, "condition wait")) {
        return QORE_COND_RESULT_INTERRUPTED;
    }

    const int poll_interval = QORE_IO_POLL_INTERVAL_MS;
    int64 remaining_timeout = timeout_ms;

    while (true) {
        int effective_timeout;
        if (timeout_ms < 0) {
            effective_timeout = poll_interval;
        } else {
            effective_timeout = remaining_timeout > poll_interval
                ? poll_interval : remaining_timeout;
        }

        int rc = wait2(m, effective_timeout);
        if (rc == 0) {
            return QORE_COND_RESULT_SUCCESS;
        }
        if (rc != ETIMEDOUT) {
            return QORE_COND_RESULT_TIMEOUT;
        }

        if (qore_check_cancel(xsink, "condition wait")) {
            return QORE_COND_RESULT_INTERRUPTED;
        }

        if (timeout_ms >= 0) {
            remaining_timeout -= effective_timeout;
            if (remaining_timeout <= 0) {
                return QORE_COND_RESULT_TIMEOUT;
            }
        }
    }
}
```

**Note**: Internal infrastructure (parser locks, `QoreThreadList::lck`, etc.) uses plain
`wait()` / `lock()` and is NOT a cancellation point.

#### Other Check Points

- `ql_lib.qpp` — sleep/usleep, process waits
- `QoreQueue.cpp` — queue operations
- `QoreCounter.cpp` — counter waits
- `QoreFile.cpp` — file I/O
- `ManagedDatasource.cpp` — database operations
- `StreamPipe.cpp` — stream pipe I/O
- `BackquoteNode.cpp` — command execution

### Exception Behavior

`THREAD-CANCELLED` is a **normal, catchable exception**:

```qore
try {
    long_running_operation();
} catch (hash<auto> ex) {
    if (ex.err == "THREAD-CANCELLED") {
        clear_thread_cancel();  # reset flag so cleanup code doesn't re-trigger
        cleanup_resources();
        rethrow;
    }
}
```

After catching, the cancel flag **remains set**. The next cancellation point will raise
the exception again. Call `clear_thread_cancel()` to continue running.

If the exception propagates uncaught to the background thread's top level, it's logged
to stderr (existing behavior for unhandled background thread exceptions).

### Thread Cleanup

When a cancelled thread's exception propagates to the top of `op_background_thread()`,
the existing cleanup path handles everything — no special cleanup path is needed:
- `purge_thread_resources()` — thread resources
- `td->del()` — thread data
- `xsink.handleExceptions()` — logs unhandled exception
- `thread_list.deleteDataRelease()` — releases TID slot (clears cancel flag)

### Security: Who Can Cancel Whom?

`qore_cancel_thread()` verifies `td->current_pgm == getProgram()`. This prevents:
- Sandboxed code from cancelling host threads
- One program's threads from cancelling another program's threads

Self-cancellation (cancelling the current thread) is an error — use `throw "THREAD-CANCELLED"` directly.

## Performance

**Loop checks**: `qore_check_cancel()` first does one atomic load for the thread cancel
flag (~1-2ns, L1 cache hit). Only if that passes does it construct the `QoreSandboxManagerHelper`
RAII helper for the program interrupt check. For the common case (not cancelled), the cost is
one TLS read + one atomic load.

**Blocking waits**: `waitWithInterrupt()` always polls at 500ms intervals. This adds at most
2 `pthread_cond_timedwait()` syscalls per second per waiting thread — negligible overhead.

**Polling interval**: Use `QORE_IO_POLL_INTERVAL_MS` (500ms) for consistency across all
blocking operations.

**Periodic fetch checks**: Check every 100 rows/iterations in tight loops to amortize overhead.

**Cancel callbacks**: Invoked synchronously from `requestInterrupt()`. Keep them fast and
thread-safe. Use atomic pointers for handles that may become invalid.

## Testing

### Thread Cancellation Tests

```qore
# Cancel a background thread in an infinite loop
int tid = background sub() { while (True) {} }();
sleep(100ms);
cancel_thread(tid, "test");
# Thread should exit with THREAD-CANCELLED

# Cancel a thread blocked in sleep
int tid = background sub() { sleep(60s); }();
sleep(100ms);
cancel_thread(tid, "test");
# Thread should wake within ~500ms

# Cancel a thread blocked in Queue::get()
Queue q();
int tid = background sub() { q.get(); }();
sleep(100ms);
cancel_thread(tid);
# Thread should unblock within ~500ms

# clear_thread_cancel() allows continued execution
int tid = background sub() {
    try {
        while (True) {}
    } catch (hash<auto> ex) {
        if (ex.err == "THREAD-CANCELLED") {
            clear_thread_cancel();
            # should be able to run code here without re-triggering
            return "cleaned up";
        }
    }
}();
sleep(100ms);
cancel_thread(tid);

# Same-program security
Program pgm(PO_NEW_STYLE);
# pgm should NOT be able to cancel threads in the parent program

# Both program interrupt and thread cancel active simultaneously
# Thread cancel is detected first (more specific)
```

### Program Interrupt Tests

```qore
# Pre-requested interrupt
SandboxManager sm();
pgm.setSandboxManager(sm);
sm.requestInterrupt();
pgm.callFunction("do_io_operation");
# Should fail immediately with PROGRAM-INTERRUPTED

# Runtime interrupt
background sub() { do_long_blocking_operation(); }();
sleep(100ms);
sm.requestInterrupt();
# All program threads interrupted within ~500ms

# No sandbox (zero overhead path)
do_io_operation();  # Works normally, no overhead
```

## Checklist for Module Updates

- [ ] Replace `qore_check_io_interrupt()` with `qore_check_cancel()` in all check points
- [ ] Replace inline `QoreSandboxManagerHelper` + `isInterruptRequested()` patterns with `qore_check_cancel()`
- [ ] Use `qore_check_cancel()` for pre-operation checks
- [ ] Implement polling for long operations using `QORE_IO_POLL_INTERVAL_MS`
- [ ] Register cancel callback via `QoreSandboxManagerHelper` if library has a cancel API
- [ ] Check every ~100 rows in fetch loops
- [ ] Ensure zero overhead when no cancellation is active
- [ ] Add tests for pre-requested and runtime cancellation
- [ ] Document any limitations (e.g., "interrupt may not be immediate")

## Reference Implementations

- **Cancel callback (RAII)**: `module-oracle/src/oracle.h` — `QoreOracleCancelHelper`
- **Cancel callback (atomic handle)**: `module-pgsql/src/QorePGConnection.h` — `QorePGCancelHelper`
- **Non-blocking polling**: `module-ssh2/src/SSH2Client.h` — `waitSocketUnlocked()`
- **Custom stream wrapper**: `modules/mongodb/src/QoreMongoStream.cpp`
- **ZMQ poll loop**: `module-zmq/src/QoreZSock.cpp`

## Implementation Phases

### Phase 1: Core Infrastructure (Qore)
- Add `cancel_requested` atomic flag and `cancel_reason` to `ThreadEntry`
- Implement `qore_check_cancel()`, `qore_cancel_thread()`, `qore_clear_thread_cancel()`
- Add Qore-level functions: `cancel_thread()`, `thread_cancelled()`, `clear_thread_cancel()`

### Phase 2: Update Qore Core Check Points
- Replace all `QoreSandboxManagerHelper` + `isInterruptRequested()` in loop statements
- Replace all `qore_check_io_interrupt()` calls
- Update `QoreCondition::waitWithInterrupt()` to use `qore_check_cancel()` and always poll
- Update sleep/usleep, Queue, Counter, and other blocking primitives

### Phase 3: Update All Modules
- Mechanical replacement of `qore_check_io_interrupt()` → `qore_check_cancel()` in all modules
- Add missing cancel callbacks where library APIs exist (sqlite3, odbc, v8, process, jni, python, openldap)
- Add missing pre-checks where absent (sqlite3, v8, process)

### Phase 4: Tests
- Thread cancellation tests (loop, sleep, queue, counter, I/O)
- `clear_thread_cancel()` continuation test
- Same-program security test
- Program interrupt + thread cancel interaction test
- ThreadPool task cancellation test
- Per-module cancellation tests
- Performance regression test

### Phase 5: Documentation
- Remove `interruptible-io-module-guide.md` and `safe-thread-cancellation.md` (replaced by this document)
- Update module developer guide
- Release notes for Qore 2.3

## Open Questions

1. **Naming**: `cancel_thread()` vs `interrupt_thread()`? Using "cancel" is clearer than "interrupt" (avoids confusion with OS signals) and matches `pthread_cancel` terminology while being safe/cooperative.

2. **Mutex/RWLock cancellation**: Should `cancel_thread()` wake threads blocked in `Mutex::lock()`? This would require changing mutex to use timed waits. Could be deferred to a later phase.

3. **`join_thread(tid)`**: Currently there's no way to wait for a background thread to finish. `cancel_thread()` is more useful when paired with a join. Could be implemented separately using a per-thread condition variable signaled at thread exit.

4. **Clearable cancel**: The current design allows `clear_thread_cancel()`. An alternative is non-clearable (once cancelled, always cancelled). The clearable design is more flexible and matches Java's model.

## Version History

- **Qore 2.0**: Initial implementation of program interrupt infrastructure
- **Qore 2.1**: Added `QoreSandboxManagerHelper` RAII class for safe access; removed raw `QoreSandboxManager*` from public API to prevent use-after-free; modules audited and updated for interruptible I/O and sandboxing
- **Qore 2.3**: Unified cancellation API (`qore_check_cancel`); added per-thread cancellation (`cancel_thread`, `thread_cancelled`, `clear_thread_cancel`)
