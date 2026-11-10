# Synchronous socket calls without a thread hand-off (#5587, step 2)

Copyright (C) 2026 Qore Technologies, s.r.o.

## Status

Implemented: sections 2 and 3, problems 1 to 3 (phase 1), and section 5 ("As implemented (phase 3)").

**Shelved: section 1 (phase 2) and section 4 (phase 4).**  Both were implemented and measured on the branch
`design/5587-sync-socket-inline` (the `socket-loop.qr` request went from about 251 to about 51 us, Python about 24),
and are not merged.  They make system calls on a socket from the calling thread (the first step, the close), so a
socket is no longer used only by its I/O thread, in the order of its controller's commands: a close by another thread
can then interleave with a step, which the controller's ordering excludes.  That branch had to handle the
interleavings one by one (a close shuts the descriptor down while a step runs, a connect could close or take over a
descriptor number it no longer owned, a registration could race a close); the model that every system call on a
socket is made by its I/O thread, which excludes them all, is the one the I/O design relies on, and the I/O work of
the project is on the asynchronous paths, which the shelved phases do not make faster.  Sections 1 and 4 are kept
below as the record of the design and its measurements.

The error for synchronous and asynchronous operations on the same socket is unchanged: each operation claims its
direction (`NB_SEND`, `NB_RECV`, `NB_CONNECT`), and a synchronous call made while an operation of the socket holds the
claim fails with `SOCKET-NON-BLOCK-ERROR` or `SOCKET-ASYNC-MODE-ERROR`.

## Goal

A synchronous `Socket` call costs about as much as the same call in Python, and less where possible, without
giving up the correctness or the scalability of the async I/O design.

The measure is the single-threaded accept/recv/send/close loop of `design/benchmarks/5587/socket-loop.qr`: about
251 us per request on develop after #5613 (157 `futex` calls per request), against about 200 us for Python (#5579).

## The rules this design keeps

- **One implementation of I/O.** Synchronous socket APIs are facades over the async I/O controller: they build the
  same poll operations as the async APIs, and the controller runs them (`design/async-socket-io.md`, "Canonical
  Socket I/O Model").  #5326 removed the earlier direct caller-thread I/O mode on purpose, so this design does not
  bring back a second path: every syscall is still made by a poll operation's `continuePoll()`, through the
  controller.
- **The controller does the work in an efficient state machine, and defers to another thread only when it must.**
  A poll operation is a non-blocking state machine; a step that needs Qore code runs on a callback worker.
- **Nothing on an I/O thread waits for another thread or runs Qore code**, so an I/O thread can neither deadlock nor
  stall.
- **The ownership of a socket is the controller's**: the direction claims (`NB_SEND`, `NB_RECV`, `NB_CONNECT`), the
  socket mutex that every `continuePoll()` holds, and the barriers that a cancel or a close waits for.

## Where the time goes today

A synchronous `Socket::recv()` (the other calls are alike; `lib/QC_Socket.qpp`, `socket_exec_poll_operation()`):

1. The caller creates a `SocketRecvPollOperation` with deferred initialization and takes the `NB_RECV` sequence
   claim (`SocketExecAsyncIoGuard`).
2. `AsyncIoControllerPriv::exec()` submits it (`submit()`: the sandbox context, `++async_io_count` under the
   socket mutex, a `SubmitOp` command under the controller lock, a wake of the I/O thread) and waits on a result
   `Queue`.
3. The I/O thread takes the command and runs the first `continuePoll()` at once (Phase 1, Step A), before any
   registration: it claims the direction (`setNonBlockFromAsyncController()`), and calls `::recv()` or `SSL_read()`
   under the socket mutex.
4. If the data is there, the operation completes; the result hash is pushed to the `Queue`, the caller wakes, and
   the controller's references are released on a callback worker (`releaseOffIoThread()`).  Only on `EAGAIN` does
   the I/O thread register the descriptor and wait.

So the I/O thread already makes the attempt optimistically.  When the attempt succeeds, which is every call of the
benchmark loop except the `connect()`, the I/O thread does exactly what the caller could have done; the cost is
the hand-off: two thread switches, and about 17 `futex` calls per call (157 per request for about 9 calls), where the
minimum is about 2.

`close()` is a controller command as well (`CancelAndCloseSocket`), and every HTTP/1 message method of `Socket`
makes one more round trip before its own operation, only to check that the socket has no HTTP/2 session
(`SocketExecHttp1AllowedPollOperation`).

## The design

### 1. The controller runs the first step of a synchronous operation on the submitting thread

`AsyncIoControllerPriv::exec()` (the entry point of every synchronous call) drives the operation's first
`continuePoll()` itself, on the calling thread, before it submits anything:

- **It is the controller's step.**  The same operation object and the same `continuePoll()` that the I/O thread
  would call, so there is still one implementation.  The thread runs it as a controller step, marked as
  `qore_in_async_io_continue_poll_worker()` marks a callback worker's step today, so `SocketSyncPoll::onIoExecutionPath()`
  is true and the nested synchronous APIs stay forbidden, as on an I/O thread.
- **With the controller's ownership.**  The operation is initialized with the claim the I/O thread would take
  (`setNonBlockFromAsyncController(dir, caller_tid)`), and the step runs under the socket mutex, which every
  `continuePoll()` on the socket holds.  Two steps on one socket therefore never overlap, whether one of them runs on
  the I/O thread or not, and an SSL object is never used by two threads at once (`qore_socket_private.h`: "the calls
  of an SSL object must not run concurrently").
- **It never waits.**  A `continuePoll()` is non-blocking by contract: it returns poll info instead of waiting, and a
  loop of non-blocking calls is bounded by `max_nonblock_ops`, as on the I/O thread.
- **If the operation is done**, `exec()` builds the result that the I/O thread would have delivered (the
  `SocketPollResultInfo` hash), releases the claim as `handleCompletion()` does, and returns it.  Nothing is
  submitted: no command, no wake, no `Queue`, no release on a callback worker.  The error number is the calling
  thread's own, so it needs no copying.
- **If the operation must wait**, `exec()` submits it with the poll info of the step.  A new flag of the `SubmitOp`
  command tells the I/O thread that the first step has run: Step A registers the descriptor for the events of the
  poll info instead of calling `continuePoll()` again, so the waiting case costs no more than today, minus one
  syscall.  The `fd_generation` snapshot is taken in the step, so a close between the step and the registration is
  reported as `SOCKET-CLOSED` by the existing generation check.  The timeout is armed at submission; the step took
  no measurable part of it.

Which operations: those whose `continuePoll()` is pure C++ (the operations of the synchronous `Socket` methods:
send, receive, accept, connect, the HTTP header and body reads, the TLS upgrades).  An operation declares it
(`AbstractPollOperation::allowsSubmitterFirstStep()`, false by default); a Qore-level poll operation never does.
The first step is skipped:

- on an async I/O execution path (`onIoExecutionPath()`), where `exec()` is already refused;
- for a bare `QoreSocket` (`qore_socket_exec_poll()`) with a TLS connection; see "Bare QoreSocket" below;
- when the socket has an operation in the same direction on the controller (the claim fails as today, with the
  same error);
- when cancellation is pending (`qore_check_cancel()`): the call raises `THREAD-CANCELLED` without I/O, which is the
  error it raises today, minus the I/O that today still happens on the I/O thread after the caller has given up (see
  "Problems found", 1).  Decided: a pending cancellation raises the exception and skips the I/O, also when the I/O
  would complete at once.

#### Bare QoreSocket

A bare `QoreSocket` (the C++ API, without a `QoreSocketObject`) has no socket mutex, and the `continuePoll()` of its
controller operations (`QoreSocketControllerDeferredPollOperation` and the others in `lib/QoreSocket.cpp`) takes none:
its private data (`qore_socket_private`) has many mutexes, each for one piece of state (`close_m` for the release of
the descriptor and the references to the TLS connection, `async_sequence_m` for the directional claims, the HTTP/2
and QUIC session locks, the event and warning queues, ...), but none that every operation holds for its step.  Its
operations are serialized today by running on one I/O thread, and by the directional claims, which exclude a second
operation in the same direction (a TLS upgrade claims every direction).

- **Without TLS:** a step on the calling thread can run while an operation of the other direction runs on the I/O
  thread: a `recv()` and a `send()` of one descriptor at the same time are full duplex for the system; the claim
  excludes an operation of the same direction and an upgrade; `IoUseHelper` holds the descriptor (`shared_fd`), so a
  close cannot release its number under the step.  No lock is needed, so the I/O thread never waits for the step and
  no deferral is needed.  The precondition is an audit, like the one above, that a receive and a send step of a bare
  socket share no state of `qore_socket_private` that is not atomic or not locked (the receive buffer is the receive
  direction's; the throughput and warning counters, the event queue and the TLS state cache are to be checked).
- **With TLS:** a step on the calling thread could call `SSL_write()` while the I/O thread calls `SSL_read()` on the
  same connection, which OpenSSL does not allow.  These need a lock that both take for the TLS calls: `close_m` is
  documented as held only for reference counts and the release of the descriptor, without I/O, and stays so, so it
  would be a new lock.  A bare TLS socket does not get the first step until a measurement shows that it matters.

#### An I/O thread never waits for a step

The I/O thread takes the socket mutex with a blocking lock in every `continuePoll()` and in `closeIo()`
(`SocketRecvPollOperationBase::continuePoll()`, `QoreSocketObject::closeIo()`).  A step on a calling thread holds
that mutex, so without more, an I/O thread that comes to an operation of the opposite direction on the same socket,
or to its close, would wait for the step: for a TLS handshake step, a stall of every socket of that I/O thread.  The
rules forbid that.

So the I/O thread does not wait for a step; it defers the socket's work, as it already defers a cancel behind a
worker's `continuePoll()` that is in flight (`pending_aborts`, `pending_socket_cancels`):

- The step sets an atomic "step in progress" on the socket after it takes the mutex, and clears it before it releases
  the mutex.
- The I/O thread tries the mutex first.  If it is held and the flag is set, the I/O thread sets a "deferred" flag,
  checks the step flag again (both sequentially consistent, so one of the two threads sees the other's store), and,
  if the step is still running, parks the socket's work (the continue, the cancel or the close) for that socket only
  and goes on with its other sockets.  If the mutex is held without the flag (a short section of another
  `QoreSocketObject` method, as today), it waits for it as today.
- The step, when it ends, clears its flag, and if the "deferred" flag is set, sends a command to the socket's I/O
  thread, which resumes the parked work.

This is the most delicate part of section 1 and is checked with a debug hook that holds a step at each point (before
the flag, with the flag set, just after clearing it) while the I/O thread comes to an operation or a close of the
socket, under TSan.

The step itself stays short: a step that is mostly CPU, such as a TLS handshake, is only allowed once the deferral is
in place and measured.

#### Code that asks which thread it runs on

The code that `continuePoll()` reaches asks in about 50 places whether it runs on an I/O thread
(`qore_on_async_io_thread()`, `onIoExecutionPath()`) and behaves differently.  Each place reachable from an
operation that allows the first step must ask `SocketSyncPoll::onIoExecutionPath()`, which the step makes true, or
be shown not to depend on the thread.

The audit of phase 1 followed the `continuePoll()` of the receive, send, accept, connect, TLS upgrade and HTTP
read operations of `Socket` into their poll states, `SSLSocketHelper` and `qore_socket_private`:

- **Fixed:** the TLS upgrade operations (and the TLS shutdown operation) asked `QoreSocket::isSecure()` whether the
  socket already had a TLS connection.  Off an I/O thread that is a controller round trip, made with the socket mutex
  held; on a thread that drives poll operations it is refused, and the socket was reported as not secure, so the
  upgrade of a TLS socket failed instead of completing at once.  They read the state directly now
  (`qore_socket_private::hasSsl()`).
- **No longer thread-dependent:** `OptionalNonBlockingHelper` (section 2).
- **Safe in a step, with a change of behavior:** the header and chunked-body reads check for cancellation and the
  sandbox limits (`qore_check_cancel()`), which on an I/O thread see the I/O thread's own state and on the calling
  thread see the caller's: a step can end with `THREAD-CANCELLED` or a sandbox limit error, which `exec()` returns as
  an error of the operation.  The certificate captured during a TLS handshake (`captureRemoteCert()`) is created in
  the calling thread's Program.
- **Safe:** the thread IDs taken when an operation is created (`controller_deferred_tid`), the thread-local state that
  one call sets and clears (`current_socket`, `lent_poll_info`), the sandbox manager captured with the operation, the
  socket event queues (they never block).
- **Not allowed the first step:** the stream operations (`sendFromInputStream()`, `recvToOutputStream()` and the
  chunked variants), which hand their stream between threads (`unassignThread()`); and, until it is changed, the
  chunked-body read (`SocketReadHttpChunkedBodyPollOperation`), which does not hold the socket mutex for its whole
  step but takes it for each part, so the "step in progress" flag cannot simply cover it.

So "the step holds the socket mutex" is a property that an operation must have to allow the first step, not one that
every operation has; `allowsSubmitterFirstStep()` is only true for the operations that hold it.

### 2. Sockets stay non-blocking

`OptionalNonBlockingHelper` sets `O_NONBLOCK` and, on a thread that is not an I/O thread, clears it again when it
goes out of scope; it also clears it when it was already set, as `set_non_blocking()` reports success then.  The I/O
thread expects every socket that it owns to be non-blocking (`design/async-socket-io.md`, "Sync I/O on Born-Non-Blocking
Sockets"), and every path that reads or writes a socket handles `EAGAIN`.

So a socket is made non-blocking when its descriptor is created (`socket()` and `accept4()` with `SOCK_NONBLOCK`
where available, `fcntl()` once otherwise) and stays so; the helper only checks the flag.  This removes two
`fcntl()` calls per step on threads other than an I/O thread, and is required for the step: otherwise a step on the
calling thread would leave the descriptor blocking for the I/O thread.  The descriptors of `File` and of the standard
streams are not sockets and keep their rule (`design/nonblocking-descriptor-io.md`).

### 3. A connect checks for completion before it waits

A non-blocking TCP `connect()` returns `EINPROGRESS`, also on the loopback interface, where the handshake is done by
the time the call returns: measured on Linux, 200 of 200 loopback connects returned `EINPROGRESS`, and 500 of 500
sockets were writable at a `poll()` with a timeout of 0 right after.  The connect state checks for writability with
a timeout of 0 (and reads `SO_ERROR`) before it returns `SOCK_POLLOUT`, so a connect that is already done completes
in its first step.  This helps the I/O thread too: it saves a registration and a pass of its loop for every loopback
or LAN connect.  A UNIX socket connects at once and needs nothing.

### 4. A close that the controller has nothing to do for

After sections 1 to 3, the two closes are the only hand-offs left in the benchmark loop.  A close is a controller
command because the controller may have state for the socket: an operation in its cache, a registration, a worker
`continuePoll()` in flight, or a hold on the descriptor (`design/async-socket-io.md`, "A socket closed while an I/O
call uses it", "A socket closed while the I/O thread registers its descriptor").

The socket counts that state in one atomic, changed by the controller wherever it changes the state (submission,
completion, registration and its removal, descriptor holds).  `close()` reads it under the socket mutex: when it is
zero, nothing on any I/O thread refers to the socket, and no submission can start while the mutex is held
(`submit()` takes it to count the operation), so the close is made on the calling thread with the code of the
controller's close (`closeIo()`); otherwise the command is sent as today.

This is the riskiest part of the design: the close paths have the most history of races (the targeted cancel of
#5585/#5586).  It is the last phase, and it is done only if the measurements of the earlier phases show that the
closes matter; the count must be shown to cover every kind of state the close command waits for, with the existing
close regressions (`accept-closed.qtest`, `ssl-close-during-io.qtest`, `isopen-close-race.qtest`, the close cases of
`socket-inline-config.qtest` and `socket-sync-io.qtest`) run under TSan.

### 5. A cheaper round trip for the calls that do wait

A call that has to wait goes through the controller as today, and its cost is the round trip.  About 17 `futex` calls
per call is several times what a hand-off needs (a wake of the I/O thread, a wake of the caller).  The first task is
to count where they come from, on a loop whose calls always wait (each receive is posted before the peer sends), with
`strace -f -k -e trace=futex` and per-lock counters in a debug build.  Places to look at, none of them measured yet:

- the result `Queue` (its mutex and condition) against a single-waiter completion that needs neither;
- the controller lock taken in `submit()` and in each of the three phases of the I/O thread's loop;
- `processed_cond` broadcasts at every Phase 1, whether or not anyone waits;
- the release of the controller's references on a callback worker (`releaseOffIoThread()`), which wakes a third
  thread for every operation, although the synchronous caller still holds its own reference, so the controller's
  release can never run the destructor there.

Every user of the controller benefits from this, including `HttpServer`, which is why it is part of the design even
though section 1 removes the round trip from the calls that need no wait.

#### As implemented (phase 3)

Measured on `design/benchmarks/5587/socket-wait-loop.qr`, a ping-pong between two threads over one connection in
which every receive waits (two waiting calls per round trip), with `strace -f -k -e trace=futex` for the call sites,
on the branch with the first steps of section 1 (where the sends complete on the calling thread): 44 `futex` calls
per round trip, 31 of them from the release of the controller's references on a callback worker (the dispatch, and
the worker's wake and wait), 6.5 from the result `Queue`.

- **The synchronous caller releases the controller's references.**  `exec()` gives the operation an
  `AsyncIoSyncRelease`; the references that the I/O thread drops for the operation (`PollInfo::cleanup()`, the
  operation reference of `deliverResult()`, the `Queue` object reference of a deferred delivery) go into it instead of
  to a callback worker, and the caller releases them on its own thread when its wait ends, on every return path.  A
  reference is handed over only after its last use on the I/O thread, as the caller can end its wait at any time (a
  cancellation): the operation reference before the push of the result, which does not use it, the `Queue` references
  after it.  One that the I/O thread drops after the caller's wait has ended is refused, and goes to a callback worker
  as before; the box's lock is held only to add or take references.  Nothing more runs on the I/O thread than before.
- **The operation object of a synchronous call holds its socket with a weak reference.**  Its `sock` member,
  which `SocketPollOperationBase::getReferencedSocketObject()` reads for the poll info, was an ordinary reference:
  setting it and deleting the operation's object each scanned for recursive references, about 10K instructions per
  waiting call.  Every read of the member is made in a step of the operation, while the controller holds the
  socket's object for it (`PollInfo::sock_obj`) or the call does, so the reference needs no strength of its own;
  `getReferencedSocketObject()` reads every kind of reference.  An opaque reference (`@=`) is not used: a Program
  that is torn down deletes the targets of its opaque references, which a call from another Program's thread on the
  socket would see.
- **A message that no logger can take costs no lock.**  `submit()` logs every submission at debug level; with no
  logger set, `AsyncIoControllerPriv::log()` took the controller's lock, the global logger's lock and the global
  controller's lock to find that out.  A process-wide count of the loggers set (`qore_async_io_logger_count`) is checked
  first.

Result, on the branch with the first steps: 9 `futex` calls per round trip instead of 44, and 288K user-space
instructions instead of 325K; the time per round trip did not change (about 25 µs, Python about 7.5 µs on the same
host): the worker's wakes were not on the critical path, which is two thread wakes per waiting call (the I/O thread
by `epoll`, then the caller by the `Queue`) where a blocking `recv()` has one.  Without the first steps (every send
and receive goes to the controller), 2026-11-10, release build: 18.4 `futex` calls, 444K user-space instructions and
about 50 µs per round trip.  The rest of the instructions are spread thinly (allocation, hashing of string keys,
building the operation object, the submission hash and the result hash).  Reaching Python's time needs the waiting
call to wait on its own thread, which is a change of the architecture (the controller would hand the wait to the
caller, with its cancellation, close and timeout handling) and is not part of this design.

## Problems found while writing this

These were in the code, independent of the design; each was shown by a test before it was fixed (phase 1).

1. **A cancelled synchronous call leaves its operation running.**  When the wait of `exec()` ends with
   `THREAD-CANCELLED` or `PROGRAM-INTERRUPTED`, `exec()` returns without cancelling the operation (`queue->shift()`
   in `AsyncIoControllerPriv::exec()`).  The operation keeps its direction claim until it completes or times out,
   so the next call on the socket in that direction can fail with `SOCKET-NON-BLOCK-ERROR`, and a receive that
   completes later consumes data that no caller gets.  `exec()` must cancel the operation by its key and wait for the
   cancel (a bounded wait on the I/O thread, which runs no Qore code) before it returns the error.
2. **An extra round trip for every HTTP/1 message call.**  `SocketExecHttp1AllowedPollOperation` is a controller
   operation that only reads whether the socket has an HTTP/2 session.  The check is a read of the session holder
   and can be made on the calling thread (or by the message operation's own first step), saving one round trip for
   each `readHTTPHeader()`, `sendHTTPMessage()` and the like.
3. **`getH2ActiveStreamId()` depended on the calling thread, and was unreachable.**  It looked up the stream in
   `h2_active_stream_ids` by the calling thread's ID, and was called on the I/O thread for `isDataAvailable()`.
   Its only use was the `SOCKET-H2-SYNC-ERROR` check of `isDataAvailable()`, which needed a server session with an
   active stream and `h2_receiving_frames` false; nothing ever set a stream for a server session (the only setter was
   the HTTP/2 client of `HTTPClient`, whose session is a client one), and `h2_receiving_frames` was never set, so the
   check could not fire on any thread.  `design/sync-httpclient-as-async-wrapper.md` and
   `design/http-client-manager-cpp-port.md` (phase P12) already listed the stream ID and the check as unreachable once
   HTTP/2 moved to the controller.  They are removed; `HTTPClient` keeps its own stream ID
   (`qore_httpclient_priv::h2_stream_id`).

## Correctness

| Concern | Today | With the first step |
|---|---|---|
| Two syscalls on one socket at once | one I/O thread per socket, and the socket mutex in every `continuePoll()` | the socket mutex in every `continuePoll()`, wherever it runs |
| An SSL object used by two threads | as above | as above: the step holds the mutex |
| Same-direction overlap | the claim fails at initialization on the I/O thread | the same claim, taken by the step |
| A close during the call | the cancel barrier and the close command; a waiting call ends with `SOCKET-CLOSED` | the close cannot run during the step: the I/O thread defers it until the step ends ("An I/O thread never waits for a step"); a waiting call is on the controller as today |
| An I/O thread waits for another thread | only for the short sections of other `QoreSocketObject` methods under the socket mutex | not for a step: its work for the socket is deferred, its other sockets go on |
| The descriptor closed or swapped between attempt and wait | the `fd_generation` check | the same check, with the snapshot taken in the step |
| Timeout | armed at submission | armed at submission; the step never waits |
| Cancellation | the wait is interruptible | the same wait; a pending cancellation is raised before the step (and problem 1 is fixed) |
| Sandbox | the network policy is resolved on the caller when the operation is created | unchanged |
| Errors and the error number | copied from the I/O thread | the caller's own |
| Deadlock of an I/O thread | impossible: no wait, no Qore code | unchanged: the step does not run on an I/O thread and waits for nothing, and the I/O thread does not wait for it |

## Scalability

The step takes no lock that the I/O threads share: only the socket's own mutex, which the call held at some point
anyway.  The I/O threads do less work (no command, no step, no result delivery for a call that needs no wait), and
CPU-heavy steps such as a TLS handshake run on the calling thread instead of delaying the other sockets of an I/O
thread.  A call that waits costs the same as today, and less after section 5.

## Phases

Each phase is measured (user-space instructions and `futex` calls per request, which do not depend on the load of
the host, and microseconds per request on an idle host, with Python's loop measured on the same host) and lands on
its own.

1. Sections 2 and 3, the audit of section 1, and problems 1 to 3, each with its test.
2. Section 1 with the deferral, for the receive, send and accept operations of `Socket`, then connect, then the
   HTTP reads and writes, then the TLS upgrades.  Expected: no hand-off for a call that needs no wait, which is every call of the benchmark
   loop but the closes.  **Shelved** (see "Status" above).
3. Section 5: count, then remove, the `futex` calls of a round trip (done: "As implemented (phase 3)").
4. Section 4, if the closes still matter after phase 2.  **Shelved** (see "Status" above).

The aim of the benchmark loop after phase 2: no `futex` calls in steady state except those of the two closes, and
fewer microseconds per request than Python on the same host.

## Tests

- **Unchanged and run in every phase:** `Socket/socket-sync-io.qtest`, `Socket/socket-inline-config.qtest`,
  `Socket/Socket.qtest` (claims, busy sockets, "a receive follows at once a receive that the controller ended" and
  its variants, concurrent sync send/recv), `Socket/accept-closed.qtest`, `Socket/ssl-close-during-io.qtest`,
  `Socket/isopen-close-race.qtest`, `AsyncSocketIo/AsyncIoThreadAffinity.qtest`, `AsyncSocketIo/SocketConfigAsyncIo.qtest`,
  the `HttpServer`, `HttpClientIo`, `WebSocket*`, `Http2`, `Http3` directories, and the same under TSan
  (`AGENTS.md`, "Running Tests Under ThreadSanitizer").
- **Changed:** `AsyncSocketIo/SocketControllerRoundTrips.qtest` pins the controller submissions of each call; it pins
  the new counts (none for a call that needs no wait).
- **New:**
  - a call that needs no wait makes no submission, and one that must wait makes exactly one, with no second
    syscall on the I/O thread (a debug counter of `continuePoll()` calls per operation);
  - a step and an operation of the opposite direction on the I/O thread, on one socket, under load, with TLS
    (no overlap: a debug assertion that the socket mutex is held in every step);
  - a close from another thread at each point of a call: before the step, during it (held with a debug hook), between
    the step and the registration, during the wait;
  - a descriptor stays non-blocking after calls on other threads;
  - a loopback connect completes without a registration, a connect to an unreachable address still times out;
  - problems 1 to 3, each failing before its fix;
  - the soak of `design/benchmarks/5587` (shutdown under load) for each phase, also under TSan.

## Decisions

- A call made while a cancellation is pending raises `THREAD-CANCELLED` and skips the I/O, also when the I/O would
  complete at once (decided 2026-11-08).
- A bare `QoreSocket` gets the first step without TLS, after the audit of its shared state; with TLS only with a new
  lock, if measurements show the need (see "Bare QoreSocket").
