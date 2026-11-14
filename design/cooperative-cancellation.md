# Cooperative Cancellation for Qore

## Overview

Qore provides cooperative cancellation at two levels:

| Level | Scope | Requested by | Exception |
|-------|-------|-------------|-----------|
| **Program interrupt** | All threads in a program | Sandbox controller via `SandboxManager::requestInterrupt()` | `PROGRAM-INTERRUPTED` |
| **Thread cancellation** | One specific thread | Any thread in scope (see [Program Scope](#program-scope-who-can-cancel-whom)) via `cancel_thread(tid)` | `THREAD-CANCELLED` |

A third reason uses the per-thread path: when the deadline of a sandbox call with a wall time limit passes, the
threads of that call raise `SANDBOX-TIMEOUT`.  The per-thread request is a bit set (`QCR_THREAD_CANCEL`,
`QCR_SANDBOX_LIMIT`); `clear_thread_cancel()` clears only the explicit request.  See
[sandbox-resource-limits.md](sandbox-resource-limits.md).

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
2. Checks the program-level interrupt via `QoreSandboxManagerHelper`, only while an interrupt is requested of any
   sandbox manager (see "Cost of a check" below)
3. Raises the appropriate exception (`THREAD-CANCELLED` or `PROGRAM-INTERRUPTED`) if either is set
4. Returns `false` with zero overhead when neither is active

### Cost of a check

A loop statement checks at the start of every iteration in every execution mode (see "Loop Statements"), so a check
with no request pending must cost no more than a few loads: the thread's data, its deferral counts, its request bits
(`QCR_THREAD_CANCEL`, `QCR_SANDBOX_LIMIT`) and `qore_sandbox_interrupt_count`, the number of sandbox managers whose
interrupt is requested.  Resolving the sandbox manager that governs the thread walks its Program-context chain, its
sandbox provenance and contexts and takes a reference to the manager (or creates a composition), which costs far more
than the loop body of a tight loop; it is done only while the count is not 0.

The count is never less than the number of managers with the `QSB_INTR_REQUESTED` bit set, so it is 0 only while no
interrupt is requested and a check sees an interrupt exactly as it did when it always looked up the manager:
`requestInterrupt()` increments it before it sets the bit (and decrements it again if the bit was already set),
`clearInterrupt()` decrements it only after it has cleared a set bit, and the destructor of a manager, which does not
clear the bit, decrements it if it finds the bit set, when no other thread can request or clear the manager's interrupt
any more (the manager is reference-counted, and every caller holds a reference).  For a moment the count can be
higher than the number of set bits by the number of requests and clears in progress (a request from its increment
until it sets the bit or takes the increment back, a clear from its clearing of the bit until its decrement), which
only makes a check look up the manager.  The order matters: if a request set the bit first, a clear between its
setting of the bit and its increment would decrement the count first, and a check in between would read 0 while the
interrupt of another manager is pending, and miss it (a waiter would still be woken by the request's wakeup, but a
check in a loop, or the only check of a short call, would not).  `requestInterrupt()` changes the count before it
wakes the waiting threads, in the same seq_cst order as the bit, so a waiter that registers its wait and then reads 0
is woken by the wakeup that follows.

Debug builds count the lookups (`dbg_get_cancel_sandbox_lookup_count()`): `loop-cancellation-points/fast-path.q`
checks that a loop makes none without a requested interrupt, in every execution mode and with AOT, and that every kind
of request still reaches the check; `dbg_sandbox_interrupt_request_window()` clears an interrupt in the middle of its
request, so the script checks the order deterministically.

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
// Interrupts and cancel callbacks use the governing sandbox (All); policy checks use Policy.
QoreSandboxManagerHelper smh(QoreSandboxManagerHelper::All);
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
        QoreSandboxManagerHelper smh(QoreSandboxManagerHelper::All);
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
    QoreSandboxManagerHelper smh(QoreSandboxManagerHelper::All);

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
| process waits (`system()`, backquote child exit) | `QoreChildProcessWaiter` on a `QoreCondition` |

#### Killing a command's process group

A command that `system()` or backquote runs under a `SandboxManager` leads a process group of its own
(`POSIX_SPAWN_SETPGROUP`; the group exists once `posix_spawn()` has succeeded, so no `setpgid()` result decides
whether the group is killed).  When the wait is cancelled or interrupted, `qore_kill_child_process()` or the
interrupted path of `qore_wait_child_process()` (`BackquoteNode.cpp`) sends `SIGKILL` to the group, waits for the
leader with `waitid(WNOWAIT)`, kills the rest of the group (`qore_kill_child_process_group()`), and only then reaps
the leader: its unreaped PID keeps the group ID from being reused, so no signal can reach another group.

- **Linux**: a signal sent to a process group reaches every process in it, including one that a member is creating
  with `fork()` (the kernel restarts a `fork()` that a signal interrupts); the first signal is enough.
- **macOS**: a process that a member is creating with `fork()` when the signal is sent does not receive it.  Its
  creator has been killed and ends once `fork()` returns, so the new process is in the group before its creator has
  ended.  The group is killed and listed (`proc_listpgrppids()`) again while the list shows a process not yet waited
  for or one newly found to have ended; otherwise every process in the group has been sent `SIGKILL` after it was
  created, and each one is waited for with kqueue (`EVFILT_PROC`, `NOTE_EXIT`); when any of them end, the group is
  killed and listed again.  It returns when no process but the leader is left, or when none has ended for
  `QORE_PGROUP_EXIT_STALL_MS` (2 s): a killed process held by an uninterruptible wait would hold the call
  indefinitely, and it has been sent `SIGKILL` after it was created, so it ends when that wait ends.  Killing the
  group again only after the leader has ended is not enough: a subshell being created when the leader was killed
  goes on starting processes, and a process that it is creating when the group is killed the second time escapes.
- **Elsewhere**: the group is killed once more after the leader has ended, which reaches every process that the
  leader itself created.

`system.qtest` and `backquote.qtest` interrupt commands whose shell, or whose subshells after the shell has ended,
are starting processes, and read a FIFO that every process of the command holds open to end-of-file.

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
    QoreSandboxManagerHelper smh(QoreSandboxManagerHelper::All);
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

The check is at the start of every iteration, before the condition of a `while` or `for` statement and before the
body of a `do-while` statement, and the operation it names is the statement's: `"while loop"`, `"do-while loop"`,
`"for loop"` or `"foreach loop"`.  The iterations of expressions (`map`, `select`, `foldl`, `foldr`, hash `map`,
map/select, list and hash literals, the lazy pipelines) and of the `context` statement are not cancellation points.
A few expressions check every 100 elements in their C++ code, with a label of their own: the string join of `foldl`
and `foldr` (`"string fold join"`, `QoreFoldlOperatorNode`), the streaming operators (`QoreStreamingOperatorNode`),
range selectors with negative offsets (`"negative offset range selector"`) and the evaluation of a runtime hash
value whose members need evaluation (`"hash expression evaluation"`).

**The AST interpreter is the reference** for every other execution mode: IR, native (JIT and tiered) and AOT code
must check exactly where it does, with its labels, and nowhere else (issue #5641).  Their runtime helpers for
expressions (fused map/foldl, hash-key projections, select loops, list construction, literals, string pipelines,
slice removal) therefore have no cancellation checks, and an expression that the IR lowers into a loop of its own
checks with `QoreIROpcode::CheckCancelEvery` (an element count, a `QoreIRCancelPoint` and the limit of a drop
operator) where the AST does, after it counted an element and before the next element is taken: the string join of a
pipeline every 100 joined values (`QoreFoldlOperatorNode`, also when the pipeline is not fused into a string join); a
streaming operator every 100 elements that it took (first, any, all, count) or returned (take, takewhile, takeuntil),
and a drop operator also every 100 elements that it skipped (`QoreStreamingOperatorNode`); a drop operator inside a
pipeline only while it skips elements (`QoreFunctionalStreamingOperator::skipDropPrefix()`,
`QoreIRCancelPoint::DropSkip`).  The check is made only on the path that counted the element, so an element that a
stage rejects does not check the same count again.  A foreach statement over a fused pipeline checks once per
iteration of its body, through blocks of its own (`foreach.fused.enter`, `foreach.fused.again`), as the header of its
loop is also entered for every element that a stage rejects.  Building the IR of an AOT closure on its first call is
no cancellation point either.

The IR interpreter, native (JIT and tiered) code and AOT code follow the same rule (issue #5641): the IR lowering
sets `QoreIRBasicBlock::loop_cancel` on the block that starts an iteration of a loop statement, and the IR interpreter
(`QoreIROpcode::Br`) and the native code (`QoreIRToLLVM`) call `qore_check_cancel()` with the statement's label on
every unconditional branch into such a block.  Only unconditional branches may enter one (`QoreIRVerifier` rejects
any other), which is why a `do-while` statement takes the next iteration through a latch block (`do.latch`).  The
check raises the exception in the scope of the loop: the loop's own branches, and a `continue` or `break` statement
inside a `try` block in the loop, take the exception target in force where the loop starts
(`FlowTarget::loop_exception_target`).  An AOT helper outlined from a region that starts an iteration
(`QoreAOT.cpp`) has its call block checked instead, as the helper enters the region without a branch.
`is_loop_header`, which also marks the headers of expression loops, only counts iterations for on-stack replacement.
The kind is serialized in AOT code with the block's loop-header flag.

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

#### Debugger callbacks

A debugger callback (`QoreDebugProgramWithQoreObject::callMethod()` in `QC_DebugProgram.h`) runs the debugger's
code on the debugged thread, also while the thread handles a cancellation (the debugger's `onException` callback for
the `THREAD-CANCELLED` exception, for example).  The callback is not a cancellation point of its own, so the debugged
code behaves as without a debugger: `DebugProgramControl` takes the locks of its state with the cancellation deferred
(`DebugProgramControl::withLock()`), so that a contended lock does not raise a pending cancellation again in its
code; its waits for its clients stay cancellation points.  A cancellation that the debugger's code raised and handled
itself (counted with `qore_get_cancel_raise_count()`) is raised for the debugged code when the callback returns, if it
is still pending, so that it is not lost.

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
  With a caller-supplied timeout they wait out the whole remaining timeout in one wait.  (The
  destructor deferral of [Destructors](#destructors) is not reported by `qore_is_cancel_deferred()`:
  a sandbox limit can still be delivered then.)

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

#### Debugger callbacks: a deliberate unbounded deferral

The callbacks of a `DebugProgram` (`QoreDebugProgramWithQoreObject::callMethod()`,
`include/qore/intern/QC_DebugProgram.h`) run the debugger's Qore code on the debugged thread with
a deferral (qore#5794), except for a debugger object of a sandboxed Program.  The debugger's code
then always completes, its loops and lock waits never raise a cancellation that the debugged code
would not raise without a debugger, and the debugged code is cancelled where it would be without
one.  The wait of a stopped thread for its clients is inside the callback, so it is the one
unbounded wait in a deferral: `cancel_thread()`, `Program::cancelThreads()`, a program interrupt
and a sandbox limit do not end a stopped thread.  It is released by the debugger: a client's
command, `DebugProgramControl::terminateProgram()`, `shutdown()` (also when the last client
disconnects), or the end of debugging.  A cancellation that arrives while a callback runs is raised
when the callback returns (`qore_is_thread_cancel_requested()` before and after), so that it is not
lost when no other cancellation point follows; one that was pending before the callback is raised
where the debugged code raises it.

#### Without thread control: synchronized state

Code that must also run under `PO_NO_THREAD_CONTROL` (a module whose user may be sandboxed) cannot call
`defer_thread_cancel()`.  It keeps the state its cleanup changes in **`synchronized` methods** and
wakes its waiters with a **`Queue` push or a `Counter` decrement**: none of these is a cancellation
point (the lock of a `synchronized` method is a `VRMutex` acquired without a `VLockCancellationPoint`,
and `Queue::push()` and `Counter::dec()` never wait), so the cleanup completes on a cancelled
thread.  Only the *waits* (`Queue::get()`, `Counter::waitForZero()`) are cancellation points, which
is what the waiter wants.  The cleanup methods contain **no loop**: a loop statement is a cancellation
point at each iteration, so on a cancelled thread it raises the cancellation again and skips the rest of
the cleanup (a `map` expression is not one, in any execution mode).  A waiter registers itself in a
synchronized method before it waits and the cleanup pushes only while a waiter is registered, so no
wakeup is lost and the queue does not grow without one:

```qore
synchronized leave(int token) {
    remove live{token};
    if (waiters) {
        wake.push(True);    # not a cancellation point
    }
}

waitOthers() {
    while (beginWait()) {   # synchronized: false once nothing is left; registers the waiter
        on_exit endWait();  # synchronized
        wake.get();         # the cancellation point is the wait itself
    }
}
```

`WebSocketClient`'s thread accounting (`WscLibraryThreads`), its handoff of an action to a
`connectAsync()` worker (`WscActionHandoff`, with `WscActionWaiters` recording an action that a
cancelled thread cannot release; each thread waiting for an action waits on a Queue of its own and
releases the recorded actions again right after it registers, so a record made before it registered
is released by itself and a record made afterwards wakes it, even before it waits; one waiter is
woken at a time, and a waiter that stops waiting without the action wakes the next one as it
unregisters, so no wakeup needs a loop; the connections that a cancelled thread took are released
by recursion instead of a loop), and the Discord heartbeat record (`DiscordHeartbeat`) use this
pattern.  A step that releases the client's lock (e.g. a subclass's `explicitDisconnect()`) and is
cancelled cannot take the lock again: it records its action like a cancelled `connectAsync()` caller.  `WscLibraryThreads::waitOthers()` supports a single waiter (the destructor): it pushes
one wakeup per change while a waiter is registered.

A **destructor running on a cancelled thread** takes Mutex locks (the handshake lock, the client lock,
the heartbeat lock) before it reaches this bookkeeping; it completes because the destructor of a trusted
class runs with cancellation deferred (see [Destructors](#destructors) below).

A libqore primitive for "cleanup-only" lock acquisition that is not a cancellation point is not
added: a `synchronized` method already is one, it cannot be held past its method (so it cannot be
used to evade cancellation outside the method), and C++ module code takes `QoreThreadLock`
(`pthread_mutex_t`), which is never a cancellation point.  A primitive on `Mutex` itself could not
tell cleanup from ordinary code, so it would let any code block uninterruptibly on a contended
lock.

The deferral is only for cleanup that runs **while the cancelled code is still on the stack**.
Cleanup the runtime runs *after* that code has returned uses the stronger rule in
[Thread Cleanup and Worker Recycling](#thread-cleanup-and-worker-recycling).

### Destructors

Issue [qore#5634](https://github.com/qoretechnologies/qore/issues/5634).  The most common cleanup on a
cancelled thread is a destructor: a local object released while `THREAD-CANCELLED` (or
`PROGRAM-INTERRUPTED`) unwinds the stack, or an explicit `delete`.  A destructor that takes a `Mutex`,
waits on a `Condition`, `Queue` or `Counter`, or runs a loop would stop at its first cancellation
point, leaving work half done: `WebSocketClient` would skip the disconnect and the wait for its own
threads while those threads still use the object, and the same holds for `HttpServer`, connection
pools and any user class that owns threads or connections.  The rule is decided **per class in the
destructor chain** (each class's own destructor code, not per object), in
`qore_class_private::execDestructor()`, `execBaseClassDestructor()` and
`execBaseClassSystemDestructor()` with `QoreDestructorCancelHelper` (`qore_thread_intern.h`), so it
holds in every execution mode (the AST and IR interpreters, JIT and tiered native code, AOT-compiled
code): every destructor of every tier is dispatched there.

| Class | Defined by | Destructor on a cancelled or interrupted thread |
|---|---|---|
| trusted | a builtin class (`sys`), or a Program whose code is not sandboxed (system code, modules loaded by trusted code) | runs with thread cancellation and program interrupts deferred; the request is raised at the first cancellation point after it, with its original reason and scope |
| sandboxed | a sandboxed Program (`qore_class_private::isSandboxedCode()`, `qore_program_is_sandboxed()`; see [Which Program is sandboxed](#which-program-is-sandboxed)) | never runs under a deferral; skipped if a request is pending |

**Trusted classes: the destructor deferral.**  `ThreadData::dtor_cancel_defer_count` is a second
deferral depth, pushed for the destructor code of a trusted class.  `qore_check_cancel()` checks it
**after** the sandbox-limit bit and before the thread cancellation and the program interrupt:

```cpp
if (td && td->cancel_defer_count) return false;      // cleanup critical sections, thread teardown
bits = thread_list.getCancelBits(tid);
if (bits & QCR_SANDBOX_LIMIT) { /* raise an expired budget or a memory limit */ }
if (td && td->dtor_cancel_defer_count) return false; // a trusted destructor: never a sandbox limit
/* thread cancellation, program interrupt */
```

- **A sandbox limit is never deferred in a destructor.**  CPU and wall-time budgets and memory limits
  are enforced only through `qore_check_cancel()` (`QCR_SANDBOX_LIMIT`), so a destructor deferral that
  returned before that check would let a destructor in a sandbox call run unbounded: a trusted
  destructor waiting for something that sandboxed code controls, or looping in code that sandboxed
  code supplies.  An expired budget or a memory limit therefore raises `SANDBOX-TIMEOUT` /
  `SANDBOX-MEMORY-LIMIT` at the destructor's next cancellation point, and the pending cancellation
  stays pending.  The destructor is then cut short, which is the accepted cost: the call it runs in
  is over its limits.
- **Waits register while only the destructor deferral is active, for a sandbox limit only.**
  `qore_is_cancel_deferred()` reports only `cancel_defer_count`, so condition waits, descriptor waits
  and sleeps in a trusted destructor stay registered for a wakeup: a sandbox limit wakes them at once.
  They are registered as *limits only* (`ThreadEntry::waiting_limits_only` for condition waits, under
  the wait registry lock; `ThreadEntry::waiting_fd_limits_only` for descriptor waits, stored before
  `waiting_fd`): `cancelThread()` and `SandboxManager::requestInterrupt()` do not wake them
  (`wakeCondWaiter(tid)`, `signalWaitingFd(tid)`), and only `requestSandboxLimit()` does
  (`limit = true`).  A wakeup that delivers nothing would end the wait spuriously: a caller that waits
  again would restart its timeout, and `Condition::wait()` would return 0 unsignalled.
- **Sandboxed code called by a trusted destructor does not get the deferral.**  A trusted base
  class's destructor can call a method that a sandboxed subclass overrides (`WebSocketClient`'s
  destructor calls `disconnect()` and `explicitDisconnect()`), or a callback that sandboxed code
  supplied.  See [Sandboxed code never runs deferred](#sandboxed-code-never-runs-deferred).
- **The explicit deferral of trusted code also defers a sandbox limit.**  `QoreCancelDeferralHelper`
  in trusted C++ code and the `terminating` deferral of thread teardown guard bounded cleanup that must
  also complete after a timeout, such as the abandonment of an HTTP request whose wait the timeout
  ended (`HttpClientConnectionManagerBase::abandonRequest()`), or the rollback of a transaction left
  open at thread exit.  Sandboxed code never runs under it.
- **The wall-time limit is the hard stop.**  A trusted destructor that waits on state the sandbox
  controls (a gate that sandboxed code is to open) cannot be ended by a cancellation or a program
  interrupt, which are deferred; only a sandbox limit ends it.  A host that must be able to stop such
  calls gives the sandbox a wall-time limit (`SandboxManager::setWallTimeLimit()`).

**Sandboxed classes: skipped when a request is pending.**  For a sandboxed class,
`QoreDestructorCancelHelper` saves both deferral depths and sets them to zero for the destructor (a
trusted destructor that releases the object, a cleanup critical section or thread teardown can be in
effect where the object is released), then calls `qore_check_cancel(nullptr)`: if a request is
pending that a cancellation point would deliver now (a thread cancellation in scope, a program
interrupt, an expired budget or a memory limit), the class's destructor code is not run.  The
object's members and private data are released afterwards as for every deleted object
(`qore_object_private::doDeleteIntern()` → `cleanup()`), which is what `QoreObject::obliterate()`
does without running a destructor; the destructors of the objects the members hold run under these
rules again.  The request stays pending.  Without a pending request, a sandboxed destructor runs as
before, and a cancellation arriving while it runs is delivered at its next cancellation point.

- **Per class.**  A sandboxed subclass of a trusted base class (a class derived from
  `WebSocketClient`) skips only its own destructor; the trusted base destructor still runs, with the
  deferral.
- **Which Program decides.**  `qore_class_private::spgm` is the Program that defined the class (for an
  imported class, the source Program), and whether it is sandboxed is read when the destructor runs,
  so a manager set after the class was parsed counts.  When that Program releases the class
  (`deleteClassData()`), the state is latched in `released_sandboxed` under the same lock; the flag of
  [Which Program is sandboxed](#which-program-is-sandboxed) survives the manager, which
  `qore_program_private::deleteData()` clears first.  A builtin class is always trusted, also when a
  binary module created it while a sandboxed Program was current.
- **Side effects.**  A cancelled or interrupted sandboxed program's destructors do not run, so
  external state that only its destructor would undo is not undone; resources held by builtin or
  module objects are still released, because their classes are trusted.  A trusted destructor that
  blocks forever can no longer be interrupted by a cancellation or interrupt (only by a sandbox
  limit); that is a bug in the trusted code.  `Program::cancelThreads()` (for example in a REPL) takes
  effect after the destructors of the Program's trusted classes that are running have returned.

#### Which Program is sandboxed

`qore_program_private_base::sandboxed` is a sticky flag (never cleared) that `qore_program_is_sandboxed()`
reads together with the manager.  It is set:

- when a sandbox manager is set on the Program (`QoreProgram::setSandboxManager()`);
- when a Program is created while the creating code is sandboxed (issue #5659): decided in the
  `qore_program_private` constructor, which every `QoreProgram` constructor runs, by
  `qore_caller_is_sandboxed()`, so it holds wherever a Program is created: by Qore code
  (`Program::constructor()`), by `ProgramControl::createEvalProgram()`, by the module manager for a user
  module or an AOT module, or by a module's C++ code (`module-python`'s `PythonProgram`, for example).  The
  creating code is sandboxed if a sandboxed Program is on the thread's call stack (also one without a
  manager of its own, which sandboxed code created and trusted code now calls), or if a sandbox governs
  the thread (trusted code that sandboxed code called).  Such a Program has no manager of its own: the code
  it runs is governed by its callers, but its classes must not count as trusted, or sandboxed code could
  get the destructor deferral by defining its class in a child Program, or in a Program that a module
  creates for it;
- for a user module that a sandboxed Program without a manager of its own loads by a path it chose
  (`inherit_module_sandbox_manager()`);
- when sandboxed code, or code acting for it, parses code in a Program
  (`qore_program_private::markSandboxedParse()`, called by `parse()`, `parsePending()` and
  `parseExpression()` before they switch to the Program, so for `Program::parse()`,
  `Program::parsePending()`, `Expression` and a user module's source alike).  Whether a Program is trusted
  is decided when it is created, but code that the sandbox supplies later is its code too: without this, a
  trusted Program that sandboxed code can reach (one it was handed, one that a library's initialization
  created and returns, a child Program created before its parent got a sandbox manager) would give a class
  that sandboxed code defines in it the trusted deferral.  This fails safe: the classes that the Program
  already had, which trusted code defined, are sandboxed from then on as well.

Parsing code in a Program and loading a module into it (`Program::parse()`, `Program::loadModule()` and
their variants) switch the thread's current Program to the target Program with a
`ProgramRuntimeParseContextHelper` or one of its variants, which used to hide the calling Program from
sandbox resolution: the initializers of code that sandboxed code parsed in a trusted Program, and the
initialization of a module that it loaded into one by a path it chose, ran ungoverned, the module manager
checked the load as a trusted one, and the module's Program was trusted.  `QoreCallerSandboxHelper`,
created before the switch (in the parse functions of `qore_program_private`,
`QoreModuleManager::runTimeLoadModule()` and `registerAOTStaticModuleIntern()`), keeps a sandboxed caller
the caller: `qore_caller_is_sandboxed()` stays true while it exists, and the sandbox that governs the
caller is applied as the provenance of the code that runs.  Under a module initialization barrier set
later, neither is seen, so library code that such a call loads is initialized as library code.  A user
module that sandboxed code chose, loaded into a Program that has no manager, gets the sandbox of that code
(`inherit_module_sandbox_manager()`), as when it loads the module into its own Program.

A user module that trusted code loads while it serves sandboxed code is sandboxed, and stays sandboxed for
every later caller (the module manager binds such a module to the tenant that chose it in any case); this is
fail-safe.

Under a module initialization barrier (`QoreModuleInitSandboxBarrierHelper`), the code that loads library
code is not looked at: the Program of a library module, and every Program that its initialization creates
(the global context Program of `module-jni`, the Program of an AOT library module, a Program that the init
code of a source library module creates), is trusted also when sandboxed code loads the library first;
otherwise whether shared library state is trusted would depend on which code loaded the library first.
Before #5659, `Program::constructor()` used `qore_caller_sandbox_ref()`, which gives library code that is
initialized for an untrusted loader the loader's sandbox (right for the modules that the library code
chooses, not for what it creates), so such a Program was sandboxed.  The same holds for code that the library's
initialization parses: it is the library's code, so a library whose initialization parses code that
sandboxed code supplied (registered with the library before it was loaded, for example) must parse it in a
sandboxed Program itself.

A Program that trusted code creates with no sandbox governing it is trusted, also when C++ code creates
it.  The same child-Program bypass
was checked in the other code that asks whether a Program has a manager: `QoreModuleManager::mayChangeModulePath()`
now uses the flag; the reflection privilege check (`QoreReflection.cpp`) fails closed for a Program
without a manager; the governing-sandbox resolution (`find_thread_sandbox_stack_ref_intern()`) walks the
callers of the child Program's code, and objects and closures created there carry their provenance, so
the child's code is governed wherever it runs.

#### Sandboxed code never runs deferred

`ProgramThreadCountContextHelper::set()` saves both deferral depths (`cancel_defer_count` and
`dtor_cancel_defer_count`) and sets them to zero when the thread enters a sandboxed Program, also when
that Program is already the current one (a builtin class's destructor runs in its caller's Program;
thread teardown runs in the thread's Program); `restoreDeferral()` adds them back when the context is
left (added, because the `terminating` deferral of thread teardown can be pushed while the context is
active and stays).  So no deferral reaches sandboxed code:

- a trusted destructor's deferral (see above);
- a cleanup critical section of trusted code that calls sandboxed code: `defer_thread_cancel()` in a
  library module around observer notifications (`AbstractWatchDataProviderBase::notifyObservers()`) and
  checkpoint persistence (`persistCheckpoint()`), or `QoreCancelDeferralHelper` around a completion action;
- thread teardown, which runs thread-resource callbacks (`set_thread_resource()`) and thread-local
  destructors under the `terminating` deferral;
- `defer_thread_cancel()` called by sandboxed code: the function only calls its code when the calling
  Program is sandboxed (`ql_thread.qpp`).  It is neutralised rather than refused: sandboxed code that
  has the `THREAD_CONTROL` domain and uses it for cleanup keeps working, and its cleanup is stopped by
  the host's cancellation, interrupt or limit like any other sandboxed code.  Refusing it would break
  such code without protecting anything that the neutralisation does not.

Trusted code keeps every deferral.  `AbstractWatchDataProviderBase::notifyObservers()` still runs the
observers in `defer_thread_cancel()`, so a trusted observer's notification completes when polling is
stopped (`WatchDataProviderShutdown.qtest`, "notification not interrupted by cancellation"); a sandboxed
observer is stopped by a cancellation, an interrupt or a limit.  Before this, the deferral returned
before the limit check, so a sandboxed observer of a `PO_NO_THREAD_CONTROL` sandbox with a 300 ms
wall-time limit that blocked ran without bound.

#### Timed waits woken early

A condition wait can end without what it waits for: a spurious wakeup, a wakeup for a request that is
not delivered (an out-of-scope cancellation that is dropped, the program interrupt of another sandbox,
since `requestInterrupt()` wakes every registered wait), or another waiter that took what it waited
for.  `Counter::waitForZero()`, `Queue::get()` / `push()`, `Mutex::lock()`, `RWLock::readLock()` /
`writeLock()` and `Gate::enter()` wait again in a loop; with a timeout they wait only for the rest of it
(`QoreWaitDeadline`, `qore_thread_intern.h`), so the timeout is not restarted.  `Condition::wait()`
returns 0 for such a wakeup, as a POSIX condition variable may; this is documented, and callers wait
in a loop on their predicate.

**Tests.**  `examples/test/qore/threads/destructor-cancellation.qtest` (trusted destructors through
Mutex, Condition, Queue and Counter waits on a cancelled thread, an explicit `delete` with other
references, a throwing destructor, nested destructors, program interrupts and
`Program::cancelThreads()`; sandboxed destructors skipped under a cancellation and an interrupt, run
without one, released by a trusted destructor and inside `defer_thread_cancel()`, of a Program that
sandboxed code created, and of an object that outlives its Program; the mixed hierarchy; a trusted
destructor calling sandboxed code through a closure, a call reference, a method,
`call_object_method()`, `sort()` and `map` callbacks, `Program::callFunction()` into another sandbox and
a three-level override; wall-time and memory limits ending a trusted destructor, also on a cancelled
thread; a deferred cancellation not waking a destructor's Condition wait; a script run in every
execution mode and as an AOT-compiled executable), `sandboxed-code-deferral.qtest` (sandboxed observers
and checkpoint callbacks of a watch provider, `defer_thread_cancel()` called by sandboxed code and by
trusted code with a sandboxed callback, a sandboxed thread resource in thread teardown, stopped by a
wall-time limit, an interrupt and a cancellation; trusted code keeping its deferral),
`wait-deadline.qtest` (the timed waits woken over and over end at their timeout), and
`examples/test/qlib/WebSocketClient/WebSocketClientStaleShutdown.qtest` (`WebSocketClient` and
`DiscordWebSocketClient` destroyed on a cancelled thread close the connection and stop the heartbeat
thread).

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

`Program::cancelThread(tid, reason)` (and `ProgramControl::cancelThread()`, with which a debugger ends the threads
of a program it debugs; qore#5854) uses the Program as an explicit target scope. It checks membership under the
Program lock, releases that lock before entering the thread registry, and sends the Program ID with the request.
The target's normal cancellation checkpoint rejects a request if the ID was recycled outside that scope.
`Program::cancelThreads(reason)` also sets a cancellation wave under the Program lock: preregistered threads and
descendants receive cancellation when they register. The wave ends when the Program's activity count reaches
zero, so subsequent evaluations start normally. Calls from inside the target Program are rejected to avoid
cancelling the coordinating caller itself.

`Program::waitForThreads(limit)` waits on the existing Program condition for zero activity and no parse owner.
`waitForThread(tid, limit)` waits for a specific ID to leave the Program. Thread exits and parse-owner release
broadcast under the Program lock; cancellation wakes the same condition through the standard condition-waker
path. The wait checks its predicate with the lock held, has one monotonic deadline, and rejects self-waits.
It observes Program activity, not final process-level TLS teardown; `thread_join()` provides the latter guarantee.
An idle notification does not reserve the Program or permit concurrent parsing. REPL embedders recheck before
submitting pending work and deliver the worker's completion to their UI event queue rather than polling.

Timed condition waits must not probe recursive mutex ownership with `pthread_mutex_trylock()`: success adds a
recursion level and prevents the condition wait from releasing the mutex. The timed and untimed paths both
delegate mutex-contract checking to the pthread condition operation.

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
- Remove `safe-thread-cancellation.md` (replaced by this document); the module author's guide is the `interruptible_io_module_guide` page (`doxygen/lang/228_interruptible_io_module_guide.dox.tmpl`), and this document holds the design
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
  (`defer_thread_cancel()` / `qore_push_cancel_deferral()`); trusted destructors run with cancellation
  deferred and sandboxed destructors are skipped when a request is pending (see [Destructors](#destructors));
  made thread teardown and pooled
  worker recycling end the request rather than run under it; added per-thread cancellation (`cancel_thread`, `thread_cancelled`, `clear_thread_cancel`); replaced 500ms polling in `QoreCondition::waitWithInterrupt` with broadcast-on-cancel (`ThreadEntry::waiting_on`), eliminating O(N) wakeup contention when many threads share a single condition variable; made condition waits broadcast only after holding the waiter's mutex (directly or through the condition waker thread), fixing a lost wakeup when a request was made just before the waiter blocked; replaced every remaining periodic cancellation check in blocking waits (descriptor waits via a per-thread wakeup channel, `sleep()`/`usleep()`, `StreamPipe`, `Datasource` lock, `channel_select()`, `Mutex`/`RWLock`/`Gate`/`Condition` and their `Auto*` helpers) with a direct wakeup; replaced the same-program restriction on `cancel_thread()` with the target-evaluated program scope rule (see [Program Scope](#program-scope-who-can-cancel-whom)), making a thread blocked inside a child program cancellable by the program that called into it
