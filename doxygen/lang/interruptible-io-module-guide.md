# Interruptible I/O and Sandboxing for Binary Modules

Copyright 2026 Qore Technologies, s.r.o.

This guide tells binary module authors how to make a module's blocking operations end as soon as the
thread running them is cancelled (`cancel_thread()`, `qore_cancel_thread()`) or its `Program` is
interrupted (`SandboxManager::requestInterrupt()`), and how to keep the module inside a sandbox.  The
design behind it is in `design/cooperative-cancellation.md`; the sandboxing rules are in
`design/module-sandboxing-audit-guide.md`.

## The rule

Every blocking wait must end as soon as cancellation is requested, without a periodic timeout:

| Wait | Use |
|---|---|
| on descriptors the module owns | `qore_cancellable_poll()` |
| in a library's own event loop (epoll, kqueue, select, a library poll function) | add the descriptor from `qore_cancel_wakeup_register()` |
| inside a library that exposes no descriptor and no event loop, but offers a cancel call | a cancel callback (`QoreSandboxManagerHelper::registerCancelCallback()`) |
| on a `QoreCondition` | `QoreCondition::waitWithInterrupt()` |
| CPU-bound loops | `qore_check_cancel()` every 100 iterations (every 10 for expensive ones) |

Polling in `QORE_IO_POLL_INTERVAL_MS` slices is deprecated: it delays every cancellation by up to
500 ms and wakes every waiting thread twice a second.

All of these APIs are declared in `<qore/qore_thread.h>`, included by `<qore/Qore.h>`; the descriptor
APIs are available when `_QORE_HAS_CANCELLABLE_POLL` is defined (Qore 3.0 and later):

```cpp
#ifdef _QORE_HAS_CANCELLABLE_POLL
    // deterministic wait
#else
    // older libqore: poll in QORE_IO_POLL_INTERVAL_MS slices with qore_check_cancel()
#endif
```

## Checking for a request

`qore_check_cancel(xsink, "operation")` returns `true` and raises `THREAD-CANCELLED` or
`PROGRAM-INTERRUPTED` if a request is pending.  Call it before starting a blocking operation and in
long loops.  It costs one atomic load when nothing is pending.

## Waiting on descriptors: `qore_cancellable_poll()`

```cpp
int qore_cancellable_poll(struct pollfd* fds, unsigned nfds, int timeout_ms, ExceptionSink* xsink,
    const char* operation = "poll");
```

It waits like `poll(2)` with the thread's cancellation wakeup channel added to the descriptors, so a
request ends the wait at once — also a request made just before the wait starts.

| Return value | Meaning |
|---|---|
| `> 0` | the number of descriptors in `fds` with events (`revents` is set) |
| `0` | the timeout expired |
| `-1` | `poll(2)` failed: `errno` is set and no exception has been raised (`EINTR` is retried internally for the rest of the timeout) |
| `QORE_POLL_CANCELLED` | an exception has been raised: `THREAD-CANCELLED`, `PROGRAM-INTERRUPTED`, or `THREAD-ERROR` if the wakeup channel could not be created |

```cpp
int my_read(int fd, void* buf, size_t len, int timeout_ms, ExceptionSink* xsink) {
    struct pollfd pfd = {fd, POLLIN, 0};
    int rc = qore_cancellable_poll(&pfd, 1, timeout_ms, xsink, "reading the response");
    if (rc == QORE_POLL_CANCELLED) {
        return -1;
    }
    if (rc < 0) {
        xsink->raiseErrnoException("MY-READ-ERROR", errno, "poll() failed");
        return -1;
    }
    if (!rc) {
        xsink->raiseException("MY-TIMEOUT", "no response within %d ms", timeout_ms);
        return -1;
    }
    return read(fd, buf, len);
}
```

A timeout of `-1` waits without a timeout and `0` checks without waiting; `nfds` may be `0` for a
cancellable sleep.  While cancellation is deferred (`QoreCancelDeferralHelper`) and on a thread that
is not registered with Qore, nothing can be delivered and the call is a plain `poll(2)`.

When the function is called from a library callback that can only report `errno`, clear the
exception and return `EINTR`; the request stays pending and is raised at the module's next
`qore_check_cancel()`:

```cpp
ExceptionSink xsink;
int rc = qore_cancellable_poll(&pfd, 1, timeout_ms, &xsink, "library read");
if (rc == QORE_POLL_CANCELLED) {
    xsink.clear();
    errno = EINTR;
    return -1;
}
```

## A library's own event loop: the wakeup descriptor

```cpp
int qore_cancel_wakeup_register(ExceptionSink* xsink, const char* operation = "wait");
int qore_cancel_wakeup_check(ExceptionSink* xsink, const char* operation = "wait");
void qore_cancel_wakeup_unregister();
class QoreCancelWakeupHelper;   // RAII for register/unregister
```

1. Register before the wait.  The call returns the descriptor to watch; `QORE_CANCEL_WAKEUP_NONE` if
   there is nothing to watch (the thread is not registered with Qore, so it cannot be cancelled); or
   `-1` with the exception raised if a request was already pending.
2. Add the descriptor to the loop **for readability only** (`POLLIN`, `EPOLLIN`, `EVFILT_READ`, a
   `select()` read set).  Never read, write or close it.  On macOS it is a kqueue, which `poll()`,
   `select()` and another kqueue can watch.
3. When the descriptor is readable, call `qore_cancel_wakeup_check()`: it returns `-1` with the
   exception raised (end the wait), or `0` if there is nothing to deliver (continue waiting; the
   descriptor is no longer readable, so the loop does not spin).
4. After the wait, whatever ended it, unregister — `QoreCancelWakeupHelper` does it in its
   destructor.

```cpp
QoreCancelWakeupHelper cwh(xsink, "waiting for the broker");
if (*xsink) {
    return -1;
}
if (cwh.fd() >= 0) {
    my_loop_watch_read(loop, cwh.fd());
}
while (true) {
    int ready_fd = my_loop_wait(loop, timeout_ms);
    if (ready_fd == cwh.fd()) {
        if (qore_cancel_wakeup_check(xsink, "waiting for the broker")) {
            return -1;
        }
        continue;
    }
    // handle the event
}
```

There is no race window: registering checks for a request after storing the registration, so a
request made at any time either is raised by the registration or makes the descriptor readable; the
check drains the descriptor before it checks, so a request made after the drain makes it readable
again.  Registrations nest, and `qore_cancellable_poll()` can be called while one is active.

## Libraries that let the module supply the transport

If a library takes custom streams or I/O callbacks (custom stream initiators and stream vtables, OpenSSL
BIOs, socket callbacks), let the module own the socket, make it non-blocking, and wait with
`qore_cancellable_poll()` in the callbacks.  When the library can only report `errno` from a callback,
clear the exception and report `EINTR` as shown above.

## Libraries without hooks

A library that blocks on descriptors it does not expose can only be stopped by its own cancel call:
register a cancel callback with `QoreSandboxManagerHelper::registerCancelCallback()` (for example
`PQcancel()`, `OCIBreak()`, `sqlite3_interrupt()`, `SQLCancel()`).  The callback is called for a
program interrupt; for thread cancellation, check with `qore_check_cancel()` before and after the
call.  Only when no such call exists, poll in `QORE_IO_POLL_INTERVAL_MS` slices, and document the
delay.

## Cleanup that must complete

Cleanup that must run on a cancelled thread (releasing a lock it owns, closing a connection) runs
inside `QoreCancelDeferralHelper`; no cancellation is delivered while it is active, and the pending
request is raised at the first cancellation point after it.

## Sandboxing

Check every filesystem and network access against the program's sandbox before making it (see
`design/module-sandboxing-audit-guide.md`), and mark functions and methods with their functional
domains in `.qpp` files (`dom=FILESYSTEM`, `dom=NETWORK`, `dom=PROCESS`, ...), so that a `Program`'s
restrictions are enforced.

## Testing

- A cancellation test blocks a thread in the wait, cancels it, and checks that the call ends with
  `THREAD-CANCELLED` well under the old polling interval — deterministically: make the request only
  once the thread is known to be in (or about to enter) the wait, and use a wait without a timeout,
  so that a missed wakeup can never end it.
- A request made before the wait must be raised at once.
- Without a request, the wait must return the same results as before (readiness, timeouts).
- `examples/test/module-cpp-api/cancellable-poll/cancellable-poll.qtest` exercises both APIs from a
  test module's C++ code.
