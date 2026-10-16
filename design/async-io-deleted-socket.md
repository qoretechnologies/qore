# Async I/O controller: a deleted socket object must evict its operation

Copyright (C) 2026 Qore Technologies, s.r.o.

## Status

Implemented.  When the socket object backing a registered async I/O operation is deleted while the operation is
still in the controller's wait set, the operation is ended with a `SOCKET-CLOSED` error and removed, instead of
being re-queried on every I/O loop iteration forever.

Relevant code:

- `lib/AsyncIoControllerPriv.cpp` — `makeSocketWaitGenerationException()` (Phase 2) ends the operation when its
  socket object is deleted; `refreshSocketWaitGeneration()` and `hasSocketWaitGenerationChanged()` (Phase 3 and
  the sync-wait helper) no longer query a deleted object.
- `include/qore/intern/qore_socket_private.h`, `lib/QoreSocketObject.cpp`, `lib/ql_debug.cpp` — the DEBUG-only
  `dbg_force_async_io_deleted()` test hook.

## The defect (a client-triggerable denial of service)

The async I/O controller holds, per operation, the operation's `poll_info`, whose `socket` key references the
`Socket` QoreObject it is waiting on.  An explicit `delete` of that object — or deleting an object that owns it —
runs its destructor and sets its status to `OS_DELETED` while the wrapper lives on (a deleted-but-referenced
object), because the controller still holds a reference.

Three controller functions resolve the socket's private data with
`QoreObject::getReferencedPrivateData(CID_SOCKET, &xsink)`.  For an `OS_DELETED` object this **raises**
`OBJECT-ALREADY-DELETED`, and constructing that exception collects a full backtrace
(`qore_aot_collect_backtrace_locs()` → `_Unwind_Backtrace`).  The functions then clear the sink and return, so the
exception is dropped — but the operation is never ended.  A deleted socket also never establishes a wait
generation, so the existing stale-fd guard (`makeSocketWaitGenerationException()`, gated on
`socket_wait_generation_valid`) never fired for it.

The result: the operation stays in the wait set, its (still-registered, still-firing) fd keeps the controller
re-queuing it, and every iteration pays the cost of raising and dropping a backtrace exception in
`refreshSocketWaitGeneration()`.  The I/O thread spins at 100% CPU, HTTP serving stops, and the process will not
shut down.  A profiler sample shows ~85% of the I/O thread in `refreshSocketWaitGeneration` →
`getReferencedPrivateData` → `ExceptionSink::raiseException`.

It is reachable by an ordinary client.  A client that opens a log WebSocket with a large history
(`Qorus-Log-History: 10000`) and disconnects immediately — while the server is still draining the history frames
that `WebSocketHandler` queued and flushed through `enableDirectSend()` — leaves an operation registered on a
connection whose backing object is then torn down.  The next log activity starts the spin.  A small history, or
letting the drain finish before disconnecting, does not trigger it: the trigger is the disconnect during the
queued-frame drain.

## The fix

A deleted socket object is a terminal condition, detected without raising:

- `makeSocketWaitGenerationException()` checks `!obj->isValid()` **before** the wait-generation guard and before
  any `getReferencedPrivateData()` call.  `isValid()` reads the object status directly and never raises.  When the
  object is deleted it returns a `SOCKET-CLOSED` exception hash, which routes the operation through the controller's
  normal Phase 3 completion-and-eviction path: the operation is unregistered from the event loop, its waiter
  receives the error (an error its callers already handle), and its cache entry is erased.  The operation is gone
  after one iteration instead of spinning forever.
- `refreshSocketWaitGeneration()` and `hasSocketWaitGenerationChanged()` return early on `!obj->isValid()` rather
  than calling the raising `getReferencedPrivateData()`, so no backtrace is built even in the window before
  Phase 2 ends the operation.

`return` from the controller's own frame was never the issue; the three functions above are the only ones that
queried the socket object's private data on the hot path.

## Relation to the DatasourcePool delete-in-use fix (ba2611c2)

This is the same lifecycle class as `ba2611c2` ("deleting a DatasourcePool in use released its configuration
twice"): a Qorus portable group tears down a runtime object — a pool, a connection, a socket — while another
subsystem still holds and uses it.  `ba2611c2` fixed a double free on the DatasourcePool side; this fixes a spin
on the AsyncIoController side when the object still in use is a socket with a registered operation.  They are
distinct defects (double free vs. busy spin) with a common root cause (delete-while-in-use), fixed independently.

## Version exposure

The async I/O controller and its socket-wait machinery are **not in any released Qore**.  The controller was
added in `4279a740` (2026-03-02, QUIC/HTTP/3 support); socket I/O became controller-backed, introducing
`makeSocketWaitGenerationException()` and the `getReferencedPrivateData`-on-a-deleted-object pattern, in
`130795f2` (2026-05-03); and the `WebSocketHandler` `enableDirectSend()` queued-frame-drain path that triggers it
was added in `272625c4` (2026-06-02).  All are on `develop` only, heading to the unreleased Qore 3.0.0; the latest
released Qore is 2.2.1.  Only an instance running a Qore 3.0 development build is exposed.

## Coverage

`examples/test/qlib/AsyncSocketIo/DeletedSocketEviction.qtest` registers an operation on a readable socket (so the
controller keeps it queued), simulates the backing object being deleted with the one-shot `dbg_force_async_io_deleted()`
hook, and asserts the operation is evicted promptly with `SOCKET-CLOSED` rather than spinning.  Deleting the object
for real would close its fd, so the event-driven controller would stop re-queuing the operation and never reach the
deleted-socket path; the hook reproduces the production state — object gone, fd still firing — deterministically and
without a race, exactly as `dbg_force_fd_swap_next_wait()` does for the sibling stale-fd case.  With the fix reverted
the operation never completes and the test's bounded `Queue::get()` times out.
