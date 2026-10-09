# Cooperative Cancellation for Qore

## Overview

Qore provides cooperative cancellation at two levels:

| Level | Scope | Requested by | Exception |
|-------|-------|-------------|-----------|
| **Program interrupt** | All threads in a program | Sandbox controller via `SandboxManager::requestInterrupt()` | `PROGRAM-INTERRUPTED` |
| **Thread cancellation** | One specific thread | Any thread in scope (see [Program Scope](#program-scope-who-can-cancel-whom)) via `cancel_thread(tid)` | `THREAD-CANCELLED` |

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

**This replaces `qore_check_io_interrupt()`**.  The old function remains
exported as a deprecated binary-compatibility wrapper that delegates to
`qore_check_cancel()` so existing modules keep loading until they are rebuilt.

### Thread Cancellation Control

```cpp
// Request cancellation of a specific thread; see "Program Scope" below.
// Returns 0 if the request was delivered, -1 if the thread was not found or not active.
DLLEXPORT int qore_cancel_thread(int tid, const char* reason = nullptr);

// Clear the cancellation flag for the current thread.
DLLEXPORT void qore_clear_thread_cancel();

// Defer delivery of cancellation and program interrupt on the current thread while a bounded
// cleanup critical section runs; the pending request is left untouched.  Returns 0 if pushed,
// -1 if the current thread has no thread data (do not pop in that case).
DLLEXPORT int qore_push_cancel_deferral();
DLLEXPORT void qore_pop_cancel_deferral();

// True while delivery is deferred; blocking primitives use this to wait without polling.
DLLEXPORT bool qore_is_cancel_deferred();

// RAII wrapper (header-inline); use this rather than the raw push/pop.
class QoreCancelDeferralHelper;
```

See [Cleanup Critical Sections](#cleanup-critical-sections) for what this is for and what it
guarantees.

### Cancellable Waits

A blocking wait in a module must end as soon as cancellation is requested, without a periodic
timeout.  Two exported APIs (since Qore 3.0; `#ifdef _QORE_HAS_CANCELLABLE_POLL`) let a module wait
on the thread's cancellation wakeup channel (see [Wakeup descriptor](#wakeup-descriptor)):

```cpp
// Waits like poll(2) with the thread's wakeup channel added; EINTR is retried internally.
// Returns > 0 (descriptors with events), 0 (timeout), -1 (poll() failed, errno set, no exception),
// or QORE_POLL_CANCELLED (-2: THREAD-CANCELLED, PROGRAM-INTERRUPTED or THREAD-ERROR raised).
DLLEXPORT int qore_cancellable_poll(struct pollfd* fds, unsigned nfds, int timeout_ms,
    ExceptionSink* xsink, const char* operation = "poll");

// For a library's own event loop: the descriptor to watch for readability, QORE_CANCEL_WAKEUP_NONE
// (-2: nothing to watch), or -1 (exception raised: a request made before the call).
DLLEXPORT int qore_cancel_wakeup_register(ExceptionSink* xsink, const char* operation = "wait");
// When the descriptor is readable: -1 if the request was raised (end the wait), 0 to continue.
DLLEXPORT int qore_cancel_wakeup_check(ExceptionSink* xsink, const char* operation = "wait");
// After the wait, whatever ended it; once per successful register.
DLLEXPORT void qore_cancel_wakeup_unregister();

// RAII wrapper (header-inline) for register/unregister.
class QoreCancelWakeupHelper;
```

The register protocol has no race window:

- **Register, then check.**  `qore_cancel_wakeup_register()` stores the registration (seq_cst) and then
  checks for a request: a request made before the registration is raised by the call, and one made after
  it signals the channel, which stays readable until it is drained.
- **Check drains first.**  `qore_cancel_wakeup_check()` drains the channel and then checks: a requester
  stores the request before it signals, so a drained signal's request is seen by the check, and a
  signal made after the drain makes the descriptor readable again.  A wakeup that delivers nothing (an
  out-of-scope request that was dropped, a cleared program interrupt, or a request while cancellation
  is deferred) therefore leaves the descriptor unreadable, and the loop does not spin.
- **Unregister drains last.**  The outermost `qore_cancel_wakeup_unregister()` clears the registration
  under `thread_list.lck` and then drains, so the next wait starts unsignalled.
- **Nesting.**  Registrations nest (the depth is kept in `ThreadEntry::wake_reg_depth`, used only by the
  thread itself); a nested registration returns the same descriptor, and `qore_cancellable_poll()`
  called inside one neither registers nor unregisters the channel, and drains it only after a wakeup.
- The descriptor is only watched for readability (`POLLIN`, `EPOLLIN`, `EVFILT_READ` — a macOS kqueue
  can itself be watched by another kqueue, `poll()` and `select()`); the caller never reads, writes
  or closes it.

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
// Recommended polling interval for blocking operations in modules (500ms)
#define QORE_IO_POLL_INTERVAL_MS 500
```

Deprecated.  Neither `libqore` nor the in-tree modules poll any more: every blocking wait is woken
directly when cancellation is requested (see [Waking Blocked Threads](#waking-blocked-threads)).  The constant
remains for modules that still poll, until they are converted to
[Cancellable Waits](#cancellable-waits).

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

### Pattern 2: Waiting on Descriptors

For a wait on descriptors the module owns, use `qore_cancellable_poll()`; never poll in slices:

```cpp
ssize_t myRead(int fd, void* buf, size_t len, int timeout_ms, ExceptionSink* xsink) {
    struct pollfd pfd = {fd, POLLIN, 0};
    int rc = qore_cancellable_poll(&pfd, 1, timeout_ms, xsink, "reading data");
    if (rc == QORE_POLL_CANCELLED) {
        return -1;  // exception raised
    }
    if (rc < 0) {
        xsink->raiseErrnoException("MY-READ-ERROR", errno, "poll() failed");
        return -1;
    }
    if (!rc) {
        xsink->raiseException("MY-TIMEOUT", "timed out after %d ms", timeout_ms);
        return -1;
    }
    return read(fd, buf, len);
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

### Pattern 4: A Library's Own Event Loop or Stream Hooks

If a library waits in an event loop of its own, add the wakeup descriptor from
`qore_cancel_wakeup_register()` to the loop (see [Cancellable Waits](#cancellable-waits)).  If it
lets the module supply the transport (custom stream or I/O callbacks), let the module own the
descriptors and wait with `qore_cancellable_poll()` in the callbacks; a callback that can only report
`errno` clears the exception and reports `EINTR`.  The in-tree `mongodb` module is the reference: it
connects its own non-blocking socket in its `mongoc_client_set_stream_initiator()` initiator and
implements a `mongoc_stream_t` on it (libmongoc stacks its TLS stream on top, and
`mongoc_stream_poll()` calls the root stream's poll function, which reports every stream as failed on
cancellation so that libmongoc's topology scanner ends the command at once):

```cpp
static ssize_t my_stream_read(my_stream_t* s, void* buf, size_t len, int timeout_ms) {
    // recv() on the non-blocking socket; on EAGAIN:
    struct pollfd pfd = {s->fd, POLLIN, 0};
    ExceptionSink xsink;
    int rc = qore_cancellable_poll(&pfd, 1, remaining_ms, &xsink, "stream read");
    if (rc == QORE_POLL_CANCELLED) {
        xsink.clear();  // the library reports errors through errno; the request stays pending
        errno = EINTR;
        return -1;
    }
    ...
}
```

Only a library that waits internally on descriptors it exposes neither directly nor through hooks
still needs a periodic check (with `QORE_IO_POLL_INTERVAL_MS`) or, better, a cancel callback that
aborts the library's operation (Pattern 3).

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
- Examples: MongoDB (libmongoc custom streams on the module's own socket)

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
| mongodb (in-tree) | N/A (own socket stream, `qore_cancellable_poll()`) | N/A | Yes | Woken directly |

All modules with existing `qore_check_io_interrupt()` calls need a mechanical replacement to `qore_check_cancel()`.
The old symbol must remain exported until compatible module rebuilds are no longer required.

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

    @since Qore 3.0

    @see thread_cancelled()
    @see clear_thread_cancel()
*/
bool cancel_thread(int tid, *string reason);

#! Returns True if cancellation has been requested for the current thread
/** @since Qore 3.0 */
bool thread_cancelled();

#! Clears the cancellation flag for the current thread
/** Call this after catching a THREAD-CANCELLED exception if the thread
    should continue running.  The request is discarded and cannot be restored:
    a thread cannot re-cancel itself.

    @since Qore 3.0
*/
nothing clear_thread_cancel();

#! Calls the given code with cancellation and program interruption deferred
/** For a short cleanup critical section that must complete on a cancelled thread.
    The pending request survives the call untouched and is raised again at the first
    cancellation point after it returns.  Requires THREAD_CONTROL.

    @since Qore 3.0
*/
auto defer_thread_cancel(code cleanup, ...);
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

### Per-Thread Cancellation State

Added to `ThreadEntry` in `QoreThreadList.h`:

```cpp
class ThreadEntry {
public:
    // ... existing fields ...

    // Per-thread cooperative cancellation flag
    std::atomic<bool> cancel_requested{false};

    // Optional cancellation reason
    QoreStringNode* cancel_reason = nullptr;

    // The condition and mutex the thread is blocked on in a cancellable condition wait, and the
    // state of a wakeup handed to the condition waker thread; guarded by the wait registry lock.
    // See "Condition waits" below.
    QoreCondition* waiting_on = nullptr;
    pthread_mutex_t* waiting_mutex = nullptr;
    QoreCondition* wake_cond = nullptr;
    pthread_mutex_t* wake_mutex = nullptr;
    int wake_next = -1;
    bool wake_queued = false, wake_busy = false;

    // The thread's wakeup channel while it is blocked in a descriptor wait, and the channel
    // itself.  See "Wakeup descriptor" below.
    std::atomic<int> waiting_fd{-1};
    int wake_fd = -1;
    pid_t wake_pid = 0;
};
```

`ThreadEntry` is the right location because:
- Fixed global array indexed by TID — accessible from any thread without TLS lookup
- Already used for cross-thread operations (`cancelAllActiveThreads`)
- Cleaned up when the TID slot is released

### Waking Blocked Threads

A cancellation request (`cancelThread()`) or program interrupt (`SandboxManager::requestInterrupt()`)
wakes a blocked thread directly; no core wait has a periodic timeout for cancellation.  There are two
mechanisms, one per kind of wait, and both use the same lost-wakeup protocol (see "Race avoidance"
below):

| Wait | Mechanism | Registered in |
|---|---|---|
| condition variable (`QoreCondition`) | broadcast the condition after holding its mutex | `ThreadEntry::waiting_on` / `waiting_mutex` |
| descriptor (`poll()`) | signal the thread's wakeup channel, which is polled with the caller's descriptors | `ThreadEntry::waiting_fd` |

Converted waits:

| Wait | How it waits now |
|---|---|
| backquote / `backquote()` output read | `qore_cancellable_poll()` |
| `File` reads and `File::isDataAvailable()` on pipes, FIFOs and terminals | `qore_cancellable_poll()` |
| blocked writes to stdout / stderr streams | `qore_cancellable_poll()` |
| `sleep()`, `usleep()`, `File` read backoff, `File::lock()` retry interval | `qore_cancellable_sleep()` |
| `StreamPipe` reads, writes and close | `QoreCondition::waitWithInterrupt()` |
| `Datasource` transaction lock wait | `QoreCondition::waitWithInterrupt()` |
| `channel_select()` (with and without a timeout) | `QoreCondition::waitWithInterrupt()` |
| `Mutex`, `RWLock`, `Gate`, `AutoLock`, `AutoReadLock`, `AutoWriteLock`, `AutoGate`, `Condition::wait()` | `VLockCancellationPoint` |
| `Queue`, `Counter`, `AutoSemaphore` | `QoreCondition::waitWithInterrupt()` (the redundant outer poll loops were removed) |
| socket, TLS, HTTP/1, HTTP/2 and HTTP/3 synchronous operations | the caller waits for the async I/O controller's result on a `Queue` (`QoreCondition::waitWithInterrupt()`) |
| process waits (`system()`, backquote child exit) | `QoreChildWait` on a `QoreCondition` |

#### Wakeup descriptor

`qore_cancellable_poll()` (internal, `qore_thread_intern.h`) waits like `poll(2)` for the caller's
descriptors plus the thread's **wakeup channel**:

- **One channel per thread**, created lazily on the thread's first descriptor wait and stored in
  `ThreadEntry::wake_fd`: an `EVFILT_USER` kqueue on macOS, an `eventfd` on Linux, a non-blocking pipe
  elsewhere.  The polled descriptor is readable while the channel is signalled.
- **Signal-before-wait.**  The waiter stores `waiting_fd = wake_fd` (seq_cst), then checks for
  cancellation, then polls; the canceller stores the request flag (seq_cst), then — under
  `thread_list.lck` — loads `waiting_fd` and signals it.  Either the waiter sees the request or the
  canceller sees the registration; unlike a condition broadcast, the signal is not lost if it is
  made before the waiter blocks, because the channel stays readable until it is drained.
- **Drain after delivery.**  After the poll, the waiter clears `waiting_fd` under `thread_list.lck`
  and then drains the channel.  Cancellers only signal under that lock while the registration is set,
  so after the clear nothing can signal the channel any more and the drain always leaves it empty: the
  next wait starts unsignalled, whether the request was delivered, dropped as out of scope, or cleared
  with `clear_thread_cancel()`.  A wakeup without a deliverable request (an out-of-scope request that
  the check dropped, or a cleared program interrupt) resumes the poll for the rest of the timeout.
- **Deferral.**  While cancellation is deferred (`defer_thread_cancel()`), nothing can be delivered, so
  the wait is a plain `poll()` and the channel is not registered; the pending request is raised at the
  first cancellation point after the deferral ends.
- **Lifetime.**  The channel is closed when the TID is released (`ThreadEntry::cleanup()`) or when a
  foreign thread leaves a reserved TID (`QoreThreadList::deleteData()`), under `thread_list.lck`, so a
  canceller can never signal a closed descriptor.
- **fork and exec.**  Every channel descriptor is close-on-exec.  The creating process ID is recorded:
  a forked child that waits replaces the inherited channel with its own, so a cancellation in either
  process can never wake the other.  On macOS a kqueue is not inherited at all, and the child only
  forgets the descriptor number.
- **Failure.**  If the channel cannot be created (descriptor exhaustion), the wait raises
  `THREAD-ERROR` instead of waiting uninterruptibly.

`qore_cancellable_sleep()` sleeps in a timed `QoreCondition::waitWithInterrupt()` on a condition of
its own, which the request broadcasts; a remainder below one millisecond is slept with `qore_usleep()`.

#### Lock waits

The smart locks (`SmartMutex`, `RWLock`, `VRMutex`) are also used internally, for example for
`synchronized` methods, where a wait must not end with a cancellation exception.  A lock wait is
therefore a cancellation point only inside a `VLockCancellationPoint` (`VLock.h`), which the Qore-level
lock APIs set on the thread's `VLock` around their single wait.  `VLock::condWait()` then waits with
`QoreCondition::waitWithInterrupt()` on the lock's internal condition (or, for `Condition::wait()`, on
the user's condition).  A condition wait reacquires its lock under a `VLockCancellationSuspend` before
it raises the cancellation, so `Condition::wait()` always returns with the lock held, as before.

### Condition waits

`QoreCondition::waitWithInterrupt()` (implemented by the internal `qore_cond_wait_cancellable()`)
registers the condition **and its mutex** in the thread's `ThreadEntry` before it checks for a
request and blocks, so any cancellation source (`cancel_thread()` or
`SandboxManager::requestInterrupt()`) can wake the thread directly.  This replaced the per-waiter
500ms polling loop and eliminates O(N) wakeup contention when N threads share one `QoreCondition`.

#### Race avoidance

The waiter holds its mutex from its last check for a request until `pthread_cond_wait()` releases
it.  A broadcast made in that window without the mutex is lost, and the waiter sleeps until
something else signals its condition — for an uncontended `Semaphore` or `Counter`, forever.  (The
first version of this design broadcast without the mutex and relied on a seq_cst flag/pointer
pairing; the pairing only guarantees that the canceller *sees* the registration, not that the
waiter is asleep when it broadcasts.  The 500ms poll loops around most callers masked the lost
wakeup; removing them exposed it as a hang.)

A canceller therefore broadcasts only after it has held the waiter's mutex: holding it means that
the waiter has either not yet checked (and will see the request, which was stored before) or has
blocked (and the broadcast wakes it).

```
waiter (holds m):                          canceller:
  [registry lock] waiting_on = cond, m       store cancel_requested = true
  check cancel_requested → bail if set       [registry lock]
  pthread_cond_wait(cond, m)                   if waiting_on:
  [registry lock] clear registration;            trylock(m) ok → unlock(m), broadcast
    wait out a queued/running wakeup             busy        → queue to the waker thread
    with m released                          [waker thread]
                                               lock(m), unlock(m), broadcast(cond)
```

- **The canceller never blocks on the waiter's mutex.**  If `trylock()` fails — the waiter is in
  the window, or another thread holds the mutex, or the canceller holds it itself (a host thread
  can call `qore_cancel_thread()` while holding a mutex that the target waits with) — the wakeup is
  queued for the **condition waker thread** (`qore-cond-waker`), which blocks on the mutex until it
  is released and then broadcasts.  The thread is started on a thread's first cancellable
  condition wait, so a canceller can always hand a wakeup to it; it is stopped by `qore_cleanup()`
  and restarted after `fork()` in a child that waits.  If it cannot be started, a wait with an
  exception sink raises `THREAD-CREATION-FAILURE`; without one, the wait is not a cancellation
  point.
- **Lifetime.**  The registration is read and the mutex tried under the registry lock, which the
  waiter needs to clear its registration, so the condition and mutex are alive.  A queued or
  running wakeup keeps the waiter from returning: it releases its mutex (so that the waker can take
  it), waits until the wakeup has completed, and reacquires the mutex — never while holding the
  registry lock.  A condition wait may return after another thread has held its mutex in between,
  which callers already allow for.
- **Lock order:** waiter's mutex → registry lock; `thread_list.lck` → registry lock; the waker
  thread never blocks on a waiter's mutex while holding the registry lock.
- **Test.** `ut_cond_wait_cancel_in_window()` (debug builds, `run_unit_tests()`) holds a
  waiter in exactly that window with a test hook while the request is made.

#### `SandboxManager::requestInterrupt()`

Walks `thread_list` and wakes every thread's registered condition wait and descriptor wait, in
addition to invoking registered cancel callbacks.  The walk is unfiltered (no per-program filter)
because `SandboxManager` does not currently track its owning Program — spurious wakeups for threads
in other programs are harmless: those threads recheck their own program's interrupt state, find it
not set, and resume waiting.

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
int QoreThreadList::cancelThread(int tid, const char* reason, unsigned scope_pgm_id) {
    AutoLocker al(lck);
    if (tid <= 0 || tid >= MAX_QORE_THREADS) {
        return -1;
    }
    if (!entry[tid].active()) {
        return -1;
    }
    if (reason) {
        if (entry[tid].cancel_reason) {
            entry[tid].cancel_reason->deref();
        }
        entry[tid].cancel_reason = new QoreStringNode(reason);
    }
    // publish the scope before the flag; the target reads it only after observing the flag
    entry[tid].cancel_scope_pgm_id.store(scope_pgm_id, std::memory_order_release);
    entry[tid].cancel_requested.store(true, std::memory_order_seq_cst);
    // wake a condition wait (see "Condition waits") and a descriptor wait (see "Wakeup descriptor")
    wakeCondWaiter(tid);
    signalWaitingFd(tid);
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

`QoreCondition::waitWithInterrupt()` registers its condition and mutex (see "Condition waits") and
does a single `pthread_cond_wait` / `pthread_cond_timedwait`; no polling is required.  It is used by
the user-facing primitives (`Condition`, `Queue`, `Counter`, `Gate`, etc.).

**Note**: Internal infrastructure (parser locks, `QoreThreadList::lck`, etc.) uses plain
`wait()` / `lock()` and is NOT a cancellation point.

**Note**: SmartMutex, RWLock and VRMutex waits are cancellation points only when made through the
Qore-level APIs, which mark them with a `VLockCancellationPoint` (see "Lock waits" above); internal
uses of the same locks are not cancellation points.

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

### Cleanup Critical Sections

A cancellation request is **sticky**: it stays set until it is cleared, so every cancellation
point raises again — including the cancellation points inside the *cleanup* that runs while
unwinding from the cancellation that was just delivered.  That makes the ordinary `on_exit`
ownership-release idiom unsafe on a cancelled thread:

```qore
release() {
    # BROKEN on a cancelled thread: Mutex::lock() is itself a cancellation point, so it throws
    # before "owned" is cleared; the ownership leaks and every waiter blocks forever
    m.lock();
    on_exit m.unlock();
    owned = False;
    cond.broadcast();
}
```

`clear_thread_cancel()` is not a fix: it discards the flag, the reason, and the scope, and the
thread cannot restore them afterwards — `cancel_thread(gettid())` is rejected, because
self-cancellation is an error.  So the cleanup would have to choose between failing and silently
swallowing the cancellation.

The supported primitive is a **deferral**: a per-thread nesting depth
(`ThreadData::cancel_defer_count`) that makes `qore_check_cancel()` report "not cancelled"
*without touching the pending request*.

```cpp
bool qore_check_cancel(ExceptionSink* xsink, const char* operation) {
    ThreadData* td = thread_data.get();
    if (td && td->cancel_defer_count) {
        return false;
    }
    ...
}
```

Properties this gives the contract:

- **Both levels are deferred.** The early return precedes the program-interrupt check as well,
  so a cleanup section also completes under `SandboxManager::requestInterrupt()`.
- **Nothing is lost.** The flag, the reason string, and the scope program ID are untouched, so
  the first cancellation point after the depth returns to zero raises `THREAD-CANCELLED` (or
  `PROGRAM-INTERRUPTED`) with the original diagnostics.
- **Observation is unaffected.** `thread_cancelled()` / `qore_is_thread_cancel_requested()` do
  not consult the depth, so cleanup code can still see that its caller was cancelled.
- **No atomics.** Only the owning thread reads or writes the counter.
- **No polling.** The lock-acquisition primitives call `qore_is_cancel_deferred()` and wait
  without a poll interval, since no cancellation can be delivered while a deferral is active.
  With a caller-supplied timeout they wait out the whole remaining timeout in one wait.

The Qore-level API is a call, not an object, so the deferral is *structurally* bounded — it
cannot be stored in a member and outlive the cleanup, and it is released when the closure throws:

```qore
release() {
    defer_thread_cancel(sub () {
        m.lock();
        on_exit m.unlock();
        owned = False;
        cond.broadcast();
    });
}
```

It carries the `THREAD_CONTROL` functional domain, like `clear_thread_cancel()`, so a
`PO_NO_THREAD_CONTROL` program cannot defer at all.  In C++, use the header-inline RAII
`QoreCancelDeferralHelper` rather than the raw push/pop.

The deferral is only for cleanup that runs **while the cancelled code is still on the stack**.
Cleanup the runtime runs *after* that code has returned uses the stronger rule below.

### Thread Cleanup and Worker Recycling

A cancellation request applies to the code it was aimed at, and no further.  Once that code has
returned, `end_thread_cancellation()` clears the request outright — deferring it would be wrong,
because there is nothing left that should observe it, and leaving it set breaks two things:

1. **Teardown is Qore code.** `purge_thread_resources()`, `ThreadData::del()` (which discards
   `thread_local` values, running their destructors), and the debugger detach handlers all hit
   cancellation points.  With the flag still set, a destructor that takes a lock to release
   ownership throws instead — the same leak, moved into thread teardown.
2. **Pooled workers outlive their tasks.** A `ThreadPool` or async I/O worker is reused, so a
   request aimed at one task would cancel the next, unrelated task on the same thread at its
   first check point, with a stale reason.

Call sites, each placed immediately after the targeted code returns.  `terminating` sites also
push a cancellation deferral that is never popped, so a request delivered *while* teardown is
already running cannot abort it either; the deferral lives in the thread data and dies with it.  A
pooled worker must stay cancellable for its next task, so its per-task sites clear only.

| Site | Covers | `terminating` |
|---|---|---|
| `op_background_thread()`, after `btp->exec()` | `background` threads | yes |
| `q_run_thread()`, after `ta->run()` | `q_start_thread()` threads (including pool worker threads) | yes |
| `q_deregister_foreign_thread()` | foreign threads | yes |
| `q_deregister_reserved_foreign_thread()` | foreign threads keeping a reserved TID | yes |
| `delete_thread_local_data()` | the initial thread at process exit | yes |
| `clear_all_program_thread_local_data()` | worker recycling (backstop for both pools) | no |
| `ThreadPoolThread::worker()`, after the task's `handleExceptions()` | `ThreadPool` tasks | no |
| `QoreCallDispatcher::workerLoop()`, after the dispatch | async I/O work items | no |

`ThreadEntry::activate()` additionally clears the state on every activation.  A `QTS_RESERVED`
entry is reactivated *without* passing through `ThreadEntry::cleanup()`, so without this a
request delivered during one activation would leak into the next thread to use that TID.

After teardown, `thread_list.deleteDataRelease()` releases the TID slot, whose
`ThreadEntry::cleanup()` clears the state again.

### Program Scope: Who Can Cancel Whom?

Self-cancellation (cancelling the current thread) is an error — use `throw "THREAD-CANCELLED"` directly.

Otherwise a request carries a **scope**, computed on the requesting thread by
`get_cancel_scope_pgm_id()`:

| Requesting context | Scope | Effect |
|---|---|---|
| host (C++) code with no `Program` context | `0` (unscoped) | applies to any thread |
| an unrestricted `Program` — no `SandboxManager` on it or any enclosing caller `Program` | `0` (unscoped) | applies to any thread |
| a sandboxed `Program` | that program's ID | applies only to threads executing in it or under a call that originated in it |

The scope is the `Program` that the governing `SandboxManager` was found on, resolved with the same
order as the manager lookup itself (`find_thread_sandbox_manager_ref_intern()`: current `Program` →
enclosing callers, innermost first → call-program fallback). That is not necessarily the *current*
`Program`: when unrestricted library or module code is called from sandboxed code, the manager is
found on an enclosing caller `Program`, and the request must be scoped to that caller. Scoping it to
the current `Program` instead would widen it to every thread that has entered the — possibly shared
— module `Program`, including threads that never entered the sandboxed `Program`.

The scope is evaluated by the **target** thread at its next cancellation point
(`check_cancel_in_scope()`), against its current `Program` and its chain of enclosing caller
`Program`s (`ThreadData::current_pgm_ctx` → `ProgramThreadCountContextHelper::getOldProgram()`),
using the same resolution order as `qore_find_thread_sandbox_manager_ref()`.

Two consequences of evaluating on the target:

- **It is safe.** The chain consists of stack-allocated `ProgramThreadCountContextHelper`
  objects in the target's own frames; walking it from the requesting thread would be a
  use-after-free hazard.
- **Delivery is not the same as effect.** `cancel_thread()` returning `True` means the request
  was delivered; an out-of-scope request is dropped by the target when it observes it
  (`dropCancelRequest()`), which keeps the steady-state cost of a cancellation point at a single
  atomic load. The drop is conditional on the scope being unchanged, so a request that arrives
  while the target is evaluating an older one is never lost.

Rationale for the scope rule:

- An unrestricted `Program` can already terminate the process outright, so scoping its
  cancellation requests protects nothing while breaking the legitimate case — a host cancelling
  a thread that is running the host's own request inside a `Program` the host called into.
- A sandboxed `Program` gains nothing it did not already have: it can cancel threads that entered
  it (which the previous same-program rule also allowed), but cannot reach threads that never did.
- The primary control against untrusted code remains the `THREAD_CONTROL` functional domain:
  a `Program` created with `PO_NO_THREAD_CONTROL` cannot call `cancel_thread()` at all.

**Superseded rule (Qore 2.2).** `cancelThread()` originally required
`td->current_pgm == getProgram()` — the target had to be executing in the requesting `Program` at
the instant of the call. Because `current_pgm` tracks the innermost frame and changes on every
cross-`Program` call, the outcome depended on where the target happened to be at that moment: the
same thread was cancellable or not depending on timing, and a denial was indistinguishable from
"no such thread" (both returned `-1`/`False`). It also blocked the one direction that is
unambiguously legitimate, while a `Program`-wide `SandboxManager::requestInterrupt()` — which
stops *every* thread in the target program — remained unrestricted.

## Performance

**Loop checks**: `qore_check_cancel()` first does one atomic load for the thread cancel
flag (seq_cst on the load — required by the wait/cancel race; on x86 this is identical to a
plain load, on weak-memory architectures it adds one fence per check).  Only if that passes
does it construct the `QoreSandboxManagerHelper` RAII helper for the program interrupt check.
For the common case (not cancelled), the cost is one TLS read + one atomic load.

**Blocking waits**: `waitWithInterrupt()` does a single `pthread_cond_wait` /
`pthread_cond_timedwait`, no polling.  Per-wait overhead is two acquisitions of the wait registry
lock (register, clear).  This
scales well with N waiters on a shared cond — a major improvement over the previous polling
design, which had O(N) periodic mutex contention on the cond's underlying user-mutex (each
waiter waking every 500ms).

**Cancel/interrupt cost**: `cancel_thread()` is O(1) — a `trylock()` and at most one
`broadcast()`, or a queued wakeup.  `SandboxManager::requestInterrupt()` walks
`MAX_QORE_THREADS` (8192) entries under `lck` and wakes each registered wait.  Acceptable for
an event that fires rarely.

**Polling interval**: no core wait polls for cancellation.  `File::lock()` still retries `F_SETLK`
every 50ms, because a record lock offers nothing that could be waited on, but the wait between
attempts ends at once on cancellation.  `QORE_IO_POLL_INTERVAL_MS` (500ms) is only used by external
binary modules that have not been converted to [Cancellable Waits](#cancellable-waits) yet.

**Descriptor waits**: the first descriptor wait of a thread creates its wakeup channel (one kqueue,
eventfd or pipe); each wait adds one entry to the poll set, one seq_cst store, one
`thread_list.lck` acquisition and one non-blocking drain call.

**Periodic fetch checks**: Check every 100 rows/iterations in tight loops to amortize overhead.

**Hash key encoding scans**: `QoreHashKeyHelper` scans non-default ASCII-compatible
encodings in chunks of at most 100 bytes and checks `qore_check_cancel()` before
each chunk. A cancelled scan leaves the helper invalid with an empty key and
propagates the exception instead of falling through to encoding conversion.
Default-encoding keys and inline strings retain their constant-time selection.
The internal invariant-only scan has no exception sink and does not deliver
cancellation; runtime constructors always provide their sink. Cleanup deferral
uses the same cancellation mechanism as other native operations.

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
# Thread wakes at once

# Cancel a thread blocked in Queue::get()
Queue q();
int tid = background sub() { q.get(); }();
sleep(100ms);
cancel_thread(tid);
# Thread unblocks at once

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

# Program scope
Program child(PO_NEW_STYLE);
# a thread blocked inside child IS cancellable from the parent

SandboxManager sm();
Program sandboxed(PO_NEW_STYLE);
sandboxed.setSandboxManager(sm);
# sandboxed CAN cancel a thread that called into it, wherever that thread is now;
# sandboxed CANNOT cancel a thread that never entered it — the request is dropped

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
# All program threads interrupted at once

# No sandbox (zero overhead path)
do_io_operation();  # Works normally, no overhead
```

## Checklist for Module Updates

- [ ] Replace `qore_check_io_interrupt()` with `qore_check_cancel()` in all check points
- [ ] Replace inline `QoreSandboxManagerHelper` + `isInterruptRequested()` patterns with `qore_check_cancel()`
- [ ] Use `qore_check_cancel()` for pre-operation checks
- [ ] Wait on descriptors with `qore_cancellable_poll()`, or add the descriptor from `qore_cancel_wakeup_register()` to a library's own event loop; never poll with `QORE_IO_POLL_INTERVAL_MS` slices
- [ ] Register cancel callback via `QoreSandboxManagerHelper` if library has a cancel API
- [ ] Check every ~100 rows in fetch loops
- [ ] Ensure zero overhead when no cancellation is active
- [ ] Add tests for pre-requested and runtime cancellation
- [ ] Document any limitations (e.g., "interrupt may not be immediate")

## Reference Implementations

- **Cancel callback (RAII)**: `module-oracle/src/oracle.h` — `QoreOracleCancelHelper`
- **Cancel callback (atomic handle)**: `module-pgsql/src/QorePGConnection.h` — `QorePGCancelHelper`
- **Non-blocking polling**: `module-ssh2/src/SSH2Client.h` — `waitSocketUnlocked()`
- **Custom stream on the module's own socket**: `modules/mongodb/src/QoreMongoStream.cpp`
- **Cancellable waits from a module**: `examples/test/module-cpp-api/ql_cppapiuser.qpp` (`cpp_api_cancellable_poll()`, `cpp_api_wakeup_loop()`)
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
- Program scope tests (child program, nested child program, sandboxed requester in and out of scope,
  dropped request followed by an in-scope request, `thread_cancelled()` ignoring an out-of-scope request)
- Program interrupt + thread cancel interaction test
- ThreadPool task cancellation test
- Per-module cancellation tests
- Performance regression test

### Phase 5: Documentation
- Remove `safe-thread-cancellation.md` (replaced by this document); `doxygen/lang/interruptible-io-module-guide.md` is the module author's guide, and this document holds the design
- Update module developer guide
- Release notes for Qore 3.0

## Open Questions

1. **Naming**: `cancel_thread()` vs `interrupt_thread()`? Using "cancel" is clearer than "interrupt" (avoids confusion with OS signals) and matches `pthread_cancel` terminology while being safe/cooperative.

2. **Mutex/RWLock cancellation**: resolved — Qore-level lock waits are woken by the request (see "Lock waits").

6. **A public cancellable wait for modules**: resolved — see [Cancellable Waits](#cancellable-waits).
   External modules that still poll are converted in their own repositories.

3. **`join_thread(tid)`**: Currently there's no way to wait for a background thread to finish. `cancel_thread()` is more useful when paired with a join. Could be implemented separately using a per-thread condition variable signaled at thread exit.

4. **Clearable cancel**: The current design allows `clear_thread_cancel()`. An alternative is non-clearable (once cancelled, always cancelled). The clearable design is more flexible and matches Java's model.

5. **Per-program filtering of `requestInterrupt()` broadcasts**: `SandboxManager::requestInterrupt()` currently walks all threads and broadcasts every non-null `waiting_on` because the manager has no back-reference to its owning Program.  Spurious wakeups for other programs' threads are harmless (they recheck and resume waiting) but wasteful when many programs run concurrently.  A back-ref from `QoreSandboxManager` to `QoreProgram` would let us filter.

## Version History

- **Qore 2.0**: Initial implementation of program interrupt infrastructure
- **Qore 2.1**: Added `QoreSandboxManagerHelper` RAII class for safe access; removed raw `QoreSandboxManager*` from public API to prevent use-after-free; modules audited and updated for interruptible I/O and sandboxing
- **Qore 3.0**: Unified cancellation API (`qore_check_cancel`); added cleanup critical sections
  (`defer_thread_cancel()` / `qore_push_cancel_deferral()`) and made thread teardown and pooled
  worker recycling end the request rather than run under it; added per-thread cancellation (`cancel_thread`, `thread_cancelled`, `clear_thread_cancel`); replaced 500ms polling in `QoreCondition::waitWithInterrupt` with broadcast-on-cancel (`ThreadEntry::waiting_on`), eliminating O(N) wakeup contention when many threads share a single condition variable; made condition waits broadcast only after holding the waiter's mutex (directly or through the condition waker thread), fixing a lost wakeup when a request was made just before the waiter blocked; replaced every remaining periodic cancellation check in blocking waits (descriptor waits via a per-thread wakeup channel, `sleep()`/`usleep()`, `StreamPipe`, `Datasource` lock, `channel_select()`, `Mutex`/`RWLock`/`Gate`/`Condition` and their `Auto*` helpers) with a direct wakeup; replaced the same-program restriction on `cancel_thread()` with the target-evaluated program scope rule (see [Program Scope](#program-scope-who-can-cancel-whom)), making a thread blocked inside a child program cancellable by the program that called into it
