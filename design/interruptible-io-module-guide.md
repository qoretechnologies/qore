# Interruptible I/O for Qore Binary Modules

## Overview

Qore 2.0 introduces a sandboxing mechanism that allows I/O operations to be interrupted on demand. This is essential for:

- Sandbox environments where user code must be stoppable
- Web applications with request timeouts
- IDE integrations requiring responsive cancellation
- Any scenario where blocking operations must be interruptible

This document describes the API and implementation considerations for making binary modules support interruptible I/O.

## Public API

The following API is available in `<qore/QoreSandboxManager.h>`:

### Core Functions

```cpp
#include <qore/QoreSandboxManager.h>

// Get the sandbox manager for the current program context (may be nullptr)
DLLEXPORT QoreSandboxManager* runtime_get_sandbox_manager();

// Check if an interrupt has been requested and optionally raise an exception
// Returns true if interrupted (and exception was raised if xsink provided)
DLLEXPORT bool qore_check_io_interrupt(ExceptionSink* xsink = nullptr);
```

### QoreSandboxManager Methods

```cpp
class QoreSandboxManager {
public:
    // Check if an interrupt has been requested
    bool isInterruptRequested() const;

    // Request an interrupt (called by controlling code)
    void requestInterrupt();

    // Clear the interrupt flag
    void clearInterrupt();
};
```

### Constants

```cpp
// Recommended polling interval for long I/O operations (500ms)
#define QORE_IO_POLL_INTERVAL_MS 500
```

## Implementation Patterns

### Pattern 1: Pre-Operation Check

Always check for interrupt before starting a potentially blocking operation:

```cpp
int myBlockingOperation(ExceptionSink* xsink) {
    // Check before starting
    if (qore_check_io_interrupt(xsink)) {
        return -1;  // Exception already raised
    }

    // Proceed with operation...
}
```

### Pattern 2: Polling During Long Operations

For operations that may block for extended periods, use polling with timeouts:

```cpp
ssize_t myLongRead(void* buf, size_t len, int timeout_ms, ExceptionSink* xsink) {
    QoreSandboxManager* sm = runtime_get_sandbox_manager();

    // Fast path: no sandbox manager attached
    if (!sm) {
        return do_blocking_read(buf, len, timeout_ms);
    }

    // Check before starting
    if (sm->isInterruptRequested()) {
        xsink->raiseException("PROGRAM-INTERRUPTED", "I/O operation interrupted");
        return -1;
    }

    // Poll with short timeouts
    int remaining = timeout_ms;
    while (remaining > 0 || timeout_ms < 0) {  // timeout_ms < 0 means infinite
        // Check for interrupt
        if (sm->isInterruptRequested()) {
            xsink->raiseException("PROGRAM-INTERRUPTED", "I/O operation interrupted");
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

### Pattern 3: Library Cancellation API

If the underlying library provides a cancellation mechanism, use it:

```cpp
class MyDatabaseConnection {
    db_handle_t* handle;

public:
    int executeQuery(const char* sql, ExceptionSink* xsink) {
        QoreSandboxManager* sm = runtime_get_sandbox_manager();

        // Check before starting
        if (qore_check_io_interrupt(xsink)) {
            return -1;
        }

        // Start async query if sandbox is active
        if (sm) {
            // Start query asynchronously
            db_query_async(handle, sql);

            // Poll for completion with interrupt checking
            while (!db_query_complete(handle)) {
                if (sm->isInterruptRequested()) {
                    // Cancel the query using library API
                    db_cancel_query(handle);
                    xsink->raiseException("PROGRAM-INTERRUPTED", "query interrupted");
                    return -1;
                }
                usleep(QORE_IO_POLL_INTERVAL_MS * 1000);
            }

            return db_get_result(handle);
        }

        // No sandbox - use synchronous call
        return db_query_sync(handle, sql);
    }
};
```

### Pattern 4: Custom Stream/Socket Wrapper

For libraries that support custom I/O callbacks (like MongoDB), wrap the stream:

```cpp
// Custom stream that checks for interrupts during I/O
typedef struct {
    library_stream_t vtable;    // Must be first
    library_stream_t* base;     // Wrapped stream
} interruptible_stream_t;

static ssize_t interruptible_read(library_stream_t* stream, void* buf,
                                   size_t len, int timeout_ms) {
    interruptible_stream_t* s = (interruptible_stream_t*)stream;

    QoreSandboxManager* sm = runtime_get_sandbox_manager();
    if (!sm) {
        return library_stream_read(s->base, buf, len, timeout_ms);
    }

    if (sm->isInterruptRequested()) {
        errno = EINTR;
        return -1;
    }

    // Polling read with interrupt checking
    int remaining = timeout_ms;
    while (remaining > 0 || timeout_ms < 0) {
        if (sm->isInterruptRequested()) {
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

## Database Driver Considerations

### Common DB Library Cancellation APIs

| Database | Cancellation API | Notes |
|----------|------------------|-------|
| Oracle OCI | `OCIBreak(svchp, errhp)` | Cancels current operation on service context |
| MySQL | `mysql_kill(conn, thread_id)` | Kills query from another connection |
| PostgreSQL | `PQcancel(cancel, errbuf, len)` | Requires PGcancel object from `PQgetCancel()` |
| SQLite | `sqlite3_interrupt(db)` | Safe to call from any thread |
| ODBC | `SQLCancel(stmt)` | Cancels statement execution |
| FreeTDS | `dbcancel(dbproc)` | Cancels current command batch |

### Implementation Strategy by Library Type

**Type A: Async API Available**
- Use async query execution
- Poll for completion with interrupt checking
- Cancel if interrupted
- Examples: Some Oracle OCI modes, modern PostgreSQL

**Type B: Cancellation from Another Thread**
- Execute query in current thread
- Have a monitoring thread check for interrupts
- Call cancel API from monitoring thread if interrupted
- Examples: MySQL, PostgreSQL PQcancel

**Type C: Custom I/O Callbacks**
- Wrap socket/stream operations
- Check interrupt during I/O
- Examples: MongoDB (libmongoc)

**Type D: Timeout-Based**
- Set short statement/connection timeouts
- Retry with interrupt checking between attempts
- Less responsive but works with any library

## Exception Handling

When an interrupt is detected, raise a consistent exception:

```cpp
if (sm->isInterruptRequested()) {
    xsink->raiseException("PROGRAM-INTERRUPTED",
        "operation interrupted by sandbox manager");
    return -1;
}
```

The `qore_check_io_interrupt()` helper does this automatically:

```cpp
if (qore_check_io_interrupt(xsink)) {
    return -1;  // Exception already raised with "PROGRAM-INTERRUPTED"
}
```

## Performance Considerations

1. **Zero Overhead When Disabled**: Always check `runtime_get_sandbox_manager()` first. If it returns `nullptr`, use the fast synchronous path.

2. **Polling Interval**: Use `QORE_IO_POLL_INTERVAL_MS` (500ms) for consistency. This balances responsiveness with overhead.

3. **Avoid Excessive Checking**: Don't check on every byte read. For streaming operations, check every N bytes or N iterations:

```cpp
// For character-by-character reads
int chars_read = 0;
while ((c = read_char()) != EOF) {
    if (++chars_read % 100 == 0) {  // Check every 100 chars
        if (qore_check_io_interrupt(xsink)) {
            return -1;
        }
    }
    // process character...
}
```

4. **Connection vs Operation**: Consider whether to check at connection time, operation time, or both. Generally:
   - Check at connection time (catches pre-requested interrupts)
   - Check during long operations (catches runtime interrupts)

## Testing

### Test 1: Pre-Requested Interrupt

```qore
# Set interrupt BEFORE operation starts
Program pgm(PO_NEW_STYLE);
pgm.parse(code_that_does_io, "test");

SandboxManager sm();
pgm.setSandboxManager(sm);

# Request interrupt before calling
sm.requestInterrupt();

# Operation should fail immediately
pgm.callFunction("do_io_operation");

# Verify exception was raised
```

### Test 2: Runtime Interrupt

```qore
# Interrupt DURING a blocking operation
Counter cnt(1);

background sub() {
    cnt.dec();  # Signal ready
    do_long_blocking_operation();
}();

cnt.waitForZero();  # Wait for operation to start
sleep(100ms);       # Let it block
sm.requestInterrupt();

# Operation should be interrupted within ~500ms
```

### Test 3: No Sandbox (Fast Path)

```qore
# Verify operations work normally without sandbox
# (no SandboxManager attached)
do_io_operation();  # Should work normally
```

## Checklist for Module Updates

- [ ] Include `<qore/QoreSandboxManager.h>`
- [ ] Check `runtime_get_sandbox_manager()` at operation start
- [ ] Implement polling for long operations using `QORE_IO_POLL_INTERVAL_MS`
- [ ] Use library cancellation API if available
- [ ] Raise `PROGRAM-INTERRUPTED` exception on interrupt
- [ ] Ensure zero overhead when no sandbox manager is attached
- [ ] Add tests for pre-requested and runtime interrupts
- [ ] Document any limitations (e.g., "interrupt may not be immediate")

## Reference Implementation

See `modules/mongodb/src/QoreMongoStream.cpp` for a complete example of wrapping a library's I/O layer with interrupt support.

## Version History

- **Qore 2.0**: Initial implementation of interruptible I/O infrastructure
