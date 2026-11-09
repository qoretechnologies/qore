# Program Thread Lifecycle Events

Copyright (C) 2026 Qore Technologies, s.r.o.

Issue: [#5633](https://github.com/qoretechnologies/qore/issues/5633)

## Purpose

An embedder (QoreRepl, an IDE, a supervisor) needs to know when threads started in a Program by `background` start
and end, and how they ended: normally, by an exception, by cancellation. Active Program code blocks parsing, loading
and resetting, so the embedder also needs to know when the Program becomes idle.

The existing mechanisms do not provide this:

- `Program::getThreadList()`, `getThreadCount()` and `waitForThread(s)()` give snapshots and blocking waits, not
  events. `getThreadCount()` counts Program *entries* (`qore_program_private::thread_count`), not threads: nested
  entries count twice, and any thread calling into the Program (an evaluation worker, a pool worker, an I/O
  callback) is counted while it is inside.
- `Program::setThreadEntryHandler()` (`design/program-thread-entry-handlers.md`) runs code on every entry and exit,
  not on a thread's start and end, receives no outcome, and its exit handler runs while the thread still holds the
  Program's thread count.
- The outcome of a background thread is only visible when `ExceptionSink::handleExceptions()` prints it to stderr at
  the end of `op_background_thread()`.

This design adds a native, Program-scoped event stream delivered to Qore `Queue` objects, and the QoreRepl and
`module-ncurses` consumers built on it. Existing thread entry handlers are unchanged.

## Decisions

| Question | Decision |
|---|---|
| Delivery | The runtime pushes typed event hashes into subscriber `Queue`s; no subscriber code runs on the emitting thread |
| Queue size | A subscription queue must be unlimited (`Queue::max() == -1`), so a push never blocks; checked at subscription (a Queue's maximum is fixed at construction) |
| Completion boundary | `FINISHED` is published after the thread has left the Program and its thread-local data has been destroyed, and before its TID is released |
| Descendants | Threads started in the subscribed Program, including by its background threads; each event carries the parent thread's identity. Threads started in other Programs (including child Programs created by the code) are not included |
| REPL default verbosity | `summary` |

## Core API

### Methods

```
hash<ProgramThreadSubscription> Program::subscribeThreadEvents(Queue queue,
        *hash<ProgramThreadEventOptions> opts) [dom=THREAD_CONTROL,EMBEDDED_LOGIC; flags=NAMED_ARGS]
bool Program::unsubscribeThreadEvents(int id) [dom=THREAD_CONTROL,EMBEDDED_LOGIC]
```

- `subscribeThreadEvents()` raises `PROGRAM-THREAD-EVENT-ERROR` when the queue has a maximum size, when the `events`
  option is 0 or has bits other than the `PTE_*` constants, when the calling code runs in the target Program (as for
  `setThreadEntryHandler()`: code must not observe or control its own embedding), or when the Program is being
  destroyed. `unsubscribeThreadEvents()` raises it when the calling code runs in the target Program.
- The returned `ProgramThreadSubscription` holds the subscription `id` (unique in the process), the `program_id`,
  the hub sequence number `seq` of the snapshot, `idle` (the Program's activity state at the snapshot), and the list
  of live background threads (`threads`, `list<hash<ProgramThreadInfo>>`, in the order they started, each with
  `cancel_requested` set if a `CANCEL_REQUESTED` was published for it). The registration and the snapshot are taken
  atomically under the hub lock: every event with a sequence number greater than `seq` is delivered to the queue,
  and no event at or below it is.
- `unsubscribeThreadEvents()` returns `False` if the subscription does not exist (already removed, or ended by
  `CLOSED`). Events already pushed stay in the queue; the consumer discards them by `subscription_id`.
- `ProgramThreadEventOptions` has one key, `events`: a bit mask of `PTE_STARTED`, `PTE_CANCEL_REQUESTED`,
  `PTE_FINISHED`, `PTE_ACTIVITY` and `PTE_CLOSED` (`PTE_ALL` for all). The default, `PTE_DEFAULT`, is every event
  but `ACTIVITY`, which a Program that is called often publishes often.
- The queue is held as the Queue's private data, as `Socket::setEventQueue()` holds its queue: the Program never
  keeps the consumer's `Queue` object alive, so no garbage-collection cycle can run through it. If the consumer
  deletes the `Queue` object, the private data stays alive and receives the events until the subscription is
  removed or the Program is destroyed, so a consumer removes its subscription when it stops reading.

Hashdecls (`ProgramThreadSubscription`, `ProgramThreadEventOptions`, `ProgramThreadInfo`, `ProgramThreadEvent`) are
declared in `lib/QC_Program.qpp` and initialized in `StaticSystemNamespace::StaticSystemNamespace()` so they appear
in the documentation; the `PTE_*` constants are declared in the same file.

### Thread identity

TIDs are reused (`QoreThreadList::get()` scans for free entries once the array has been used), so each background
thread also gets a process-unique `thread_id` (64-bit, from a global atomic counter) when it is created. Events and
snapshot entries carry both. `parent_tid` and `parent_thread_id` identify the thread that ran `background`
(`parent_thread_id` is 0 when the parent was not itself a background thread of a Program, ex: the main thread or an
evaluation worker).

### Event hash

`hash<ProgramThreadEvent>`:

| Key | Type | Description |
|---|---|---|
| `subscription_id` | `int` | the subscription the event was delivered for |
| `program_id` | `int` | the Program's ID |
| `seq` | `int` | the hub sequence number; strictly increasing across all events of the Program |
| `event` | `string` | `STARTED`, `CANCEL_REQUESTED`, `FINISHED`, `ACTIVITY` or `CLOSED` |
| `tid` | `*int` | the thread's TID (thread events) |
| `thread_id` | `*int` | the thread's unique ID (thread events) |
| `parent_tid`, `parent_thread_id` | `*int` | the starting thread (thread events) |
| `name` | `*string` | the thread's name when available (`STARTED`: as created; `FINISHED`: as last set) |
| `start` | `*date` | when the thread started (thread events) |
| `end`, `duration_us` | `*date`, `*int` | when it finished and the elapsed time (`FINISHED`) |
| `running` | `int` | the number of background threads that have published `STARTED` and not `FINISHED`, after the event |
| `idle` | `*bool` | `True` if the Program became idle, `False` if it became active (`ACTIVITY`) |
| `outcome` | `*string` | `COMPLETED`, `EXCEPTION`, `CANCELLED`, `INTERRUPTED`, `EXITED` or `START_FAILED` (`FINISHED`) |
| `error` | `*hash<ExceptionInfo>` | the first exception that ended the thread (see "Event values" below) |
| `exception_count` | `*int` | the number of exceptions that ended the thread |
| `cleanup_error` | `*hash<ExceptionInfo>` | the first exception raised by the thread's teardown, if any |
| `cancel_reason` | `*string` | the reason given with the request (`CANCEL_REQUESTED`) |

## Lifecycle

### Order

For each thread exactly one `STARTED` and one `FINISHED` are delivered, in that order, with zero or more
`CANCEL_REQUESTED` between them. All events of a Program are delivered to each queue in `seq` order.

### Start

- **Parent side** (`do_op_background()` and its IR/JIT/AOT twin `do_op_background_helper()` in `lib/thread.cpp`): the
  `thread_id` and the parent identity are recorded in `BGThreadParams`. If preregistration in the Program or native
  thread creation fails, the thread never existed: no event is published, and the exception is raised to the code
  that ran `background`.
- **`STARTED`** is published by the new thread after it has registered in the Program (`startThread()` /
  `registerNewThread()`), before the entry handler and thread initialization code run. From this point the thread
  is in the hub's live set and in snapshots.
- If the entry handler fails, the thread's expression does not run; it publishes `FINISHED` with outcome
  `START_FAILED` and the handler's exception in `error`.

### Outcome

The outcome is taken immediately after the thread's expression returns (`btp->exec()`), before
`end_thread_cancellation()`, the debugger detach (`dbgClearTermination()` can remove `THREAD-CANCELLED`) and the
teardown steps that append to the same `ExceptionSink`:

| Outcome | Condition |
|---|---|
| `COMPLETED` | no exception; includes a thread that caught a cancellation and returned normally |
| `EXITED` | the thread called `thread_exit` |
| `CANCELLED` | the first exception is `THREAD-CANCELLED` |
| `INTERRUPTED` | the first exception is `PROGRAM-INTERRUPTED` or `SANDBOX-TIMEOUT` |
| `EXCEPTION` | any other exception |
| `START_FAILED` | the entry handler failed |

A cancellation request does not determine the outcome; only the exception that ended the thread does. Exceptions
raised after the snapshot (object release, `purge_thread_resources()`, the exit handler, thread-local destructors)
are reported in `cleanup_error` and do not change the outcome.

### Finish

`op_background_thread()` currently ends with: exit handler (`entry_frame.exit()`), sandbox slot release, Program
deregistration (`btp->del()`), thread-local data destruction (`td->del()`), `handleExceptions()`, thread cleanup
functions, and TID release (`thread_list.deleteDataRelease()`).

`FINISHED` is published after `td->del()` and before `deleteDataRelease()`:

- the thread no longer counts in the Program and its thread-local destructors have run, so the event means the
  thread's work is over;
- the TID is still held, so a later thread reusing it cannot publish `STARTED` before this `FINISHED`.

The Program may be destroyed once the thread has deregistered, so the thread holds a reference to the Program's
hub (see below), not to the Program, until it has published.

### Cancellation requests

`CANCEL_REQUESTED` is published when a request is delivered to a background thread of the Program while its code
runs, from `Program::cancelThread()`, `Program::cancelThreads()` (including threads caught by the cancellation wave
as they start) and `cancel_thread()` / `qore_cancel_thread()`. "Requested" never means "terminated": the outcome is
reported by `FINISHED`.

- The thread records its hub and `thread_id` in its `ThreadEntry` (`lifecycle_hub`, `lifecycle_thread_id`) under
  `thread_list.lck` after publishing `STARTED` (`QoreThreadList::setLifecycleHub()`), and clears them when its code
  has ended, right after its outcome is taken (`clearLifecycleHub()`). `QoreThreadList::cancelThread()` references
  the hub under the lock when the request is new (the `QCR_THREAD_CANCEL` bit was not set) and publishes after
  releasing it, so the request is published once: further requests while one is pending publish nothing, and a
  request after `clear_thread_cancel()` publishes again.
- A request made before the hub was set (by a cancellation wave in `startThread()`, or by a canceller that had the
  TID from the starting thread) is found pending by `setLifecycleHub()` and published by the thread itself, right
  after its `STARTED`.
- A canceller that publishes after the thread's `FINISHED` finds no live record and publishes nothing; the hub lock
  orders the two, so a `CANCEL_REQUESTED` is never delivered after the thread's `FINISHED`.

### Activity

With `PTE_ACTIVITY`, `ACTIVITY` is published when the Program's thread count (`thread_count`, the value
`getThreadCount()` returns and `waitForThreads()` waits on) becomes zero (`idle: True`) and when it leaves zero
(`idle: False`).
Every function that changes `thread_count` - `incThreadCount()`, `decThreadCount()`, `preregisterNewThread()` (a
parent starting a background thread) and `cancelPreregistration()` (a thread that failed to start) - detects the
transition under `plock` and publishes after releasing it; `parse_count`, which `waitForThreads()` also waits on, is
not part of it: the event reports code running in the Program, not parsing. This is what lets an embedder resume a
request blocked by active Program code without a waiting thread; as an idle event does not reserve the Program, the
embedder must still recheck before parsing.

- Each transition increments the Program's `activity_gen` under `plock`. The transition is published after `plock`
  is released by a `ProgramThreadActivityPublisher` declared before the lock, which holds a reference to the hub, as
  another thread can destroy the Program once its thread count has been released. Transitions published by
  different threads can reach the hub out of order; the hub keeps the generation of the last one recorded and drops
  older ones, so the last state delivered is the Program's state.
- The cost without an `ACTIVITY` subscription is the increment and one atomic load (`hasActivityInterest()`). A
  subscription records its interest before it reads the Program's state (`activity_gen` and `thread_count` under
  `plock`): a transition whose publisher read no interest was made under `plock` before the subscription read the
  state, so the snapshot's `idle` includes it.
- A background thread leaves the Program before it publishes `FINISHED` (see "Finish"), so the `ACTIVITY` event of
  the Program becoming idle can come before the `FINISHED` of its last thread, whose `running` still counts it.
- A call into the Program from outside (`Program::callFunction()`, for example) can make more than one transition:
  the Program can become active and idle more than once for one call.

### Program destruction

When the Program is destroyed (`waitForTerminationAndClear()`, or `qore_program_private::del()` for a Program that
was not cleared), the hub is closed: no subscription can be made, and `CLOSED` is published once all background
threads have published `FINISHED` (by the last of them if some had left the Program but not published yet), and the
subscriptions are removed. A destroyed Program publishes nothing else.

### Event values

Events refer to no objects or code: an event must not extend the life of the thread's values, and pushing one to a
queue must need no garbage-collection scan (the hub pushes with `QoreQueue::pushAndTakeRef()`, which does not count
values for the scanner, as `Socket` events do). `error` and `cleanup_error` are built by
`program_thread_event_error()` from the exception (`QoreException::makeExceptionObjectWithoutChain()`), without
chained exceptions (`next`), and with `err`, `desc` and `arg` left empty when their value needs a scan
(`needs_scan()`). Releasing an event therefore never runs Qore code, which is why it can be released under the hub
lock.

### fork()

In a child process, only the forking thread exists. `qore_program_private::fixupAfterFork()` calls
`ProgramThreadHub::forkChild()`, which reinitializes the hub lock, drops the records of the other threads without
events (with the hub references they held), sets the activity state to the child's, and publishes `CLOSED` if the
Program was being destroyed and none of its threads remain. `ThreadEntry::abandonAfterFork()` clears the entries'
hub fields. Subscriptions are kept: the queues are the child's copies.

## Implementation

### The hub

`ProgramThreadHub` (`include/qore/intern/ProgramThreadHub.h`, `lib/ProgramThreadHub.cpp`) is reference-counted and
owned by `qore_program_private` (created with the Program). Each background thread references it from
`BGThreadParams` until it has published `FINISHED`. It holds:

- a mutex that is a leaf lock: no other lock is taken while it is held but the lock of a subscription's queue, and
  no Qore code runs while it is held;
- the sequence counter;
- the live set: `thread_id` → `{tid, identity, name, start, zone, cancel_requested}`, maintained whether or not
  there are subscribers so that a late subscription gets a correct snapshot (one insert and one remove per
  background thread);
- the subscriptions: `{id, Queue*, events}`;
- the activity state: the generation and `idle` flag of the last transition recorded, and an atomic count of the
  subscriptions that take `ACTIVITY` events.

Dates use the time zone of the Program when the thread started (`currentTZ()` in the new thread, whose current
Program is the thread's Program).

### Publication

The event hash is built before taking the hub lock. Under the lock the sequence number is assigned, the live set is
updated, and the event is pushed to each matching queue with the subscription ID; releasing values happens after the
lock. Pushing under the lock gives one total order across threads. A push to an unlimited queue takes only the
queue's own mutex and does not block or run Qore code; a queue in an error state discards the event. Publication
raises no exceptions, so nothing reaches the thread's `ExceptionSink`: it must not change the evaluated program's
behavior. Releasing the queues of removed subscriptions happens after the lock: it can run the destructors of values
that the consumer pushed to its queue, if it holds the last reference to the queue's data.

With no subscribers, publishing a thread event costs the live-set update; publishing `CANCEL_REQUESTED` and
`ACTIVITY` costs one atomic load.

### Garbage collection

A subscription references the queue's private data, not the `Queue` object, so the Program holds no reference that
the scanner would need to follow: the `Queue` object, and any values the consumer pushed to it, are released when
the consumer releases the object, as for a `Socket` event queue.

## QoreRepl

### Session events

`QoreRepl` subscribes its Program with `PTE_ALL` and runs one dispatcher thread for the session (started with
the first listener, ended by `close()`). The dispatcher belongs to the REPL's own Program, so it, the evaluation
worker of a frontend and any capture threads are never reported as user threads: only `background` threads of the
evaluated Program are.

```
int QoreRepl::addEventListener(code<nothing(hash<ReplEvent>)> listener)
bool QoreRepl::removeEventListener(int id)
list<hash<ReplThreadInfo>> QoreRepl::getBackgroundThreads()
```

`ReplEvent` types: `thread-started`, `thread-cancel-requested`, `thread-finished`, `eval-started`, `eval-finished`,
`program-idle`, `program-busy`, `program-replaced`. Listener exceptions are caught and do not stop delivery.

`/reset` unsubscribes the old Program, subscribes the new one, and publishes `program-replaced` with the new
snapshot; events of an old subscription still in the queue are discarded by `subscription_id`. `/threads` uses
`getBackgroundThreads()` (age, parent, cancel requested). `/cancel` and `/join` keep their semantics; the
`REPL-THREADS-ACTIVE` guard keeps using `getThreadCount()`, as any entry blocks parsing.

### Notices

`QoreRepl::formatThreadNotices(list<hash<ReplEvent>> events)` turns a batch of events into notice lines, grouping by
type within the batch (ex: `3 threads started (17, 18, 19)`); a thread that starts and finishes within one batch is
reported once, as finished. Grouping uses the batch the consumer drained; there is no timer.

Verbosity is the `thread_notifications` option and the `/notify` command:

| Level | Notices |
|---|---|
| `all` | every event, one line each |
| `summary` | grouped starts, finishes, cancellation requests and failures (default) |
| `failures` | `EXCEPTION`, `INTERRUPTED` and `START_FAILED` only |
| `off` | none |

Examples:

```
Thread 17 started
Thread 17 finished (40.0s)
Thread 18: cancellation requested
Thread 18 cancelled
Thread 19 failed: ERR: description (<repl:3>:1)
```

### Other frontends

- `QoreReplActionSession::setEmitter()` (`design/action-session-services.md`) emits the session events.
- The WebSocket connection sends `thread` and `activity` messages; `QoreReplApi.asyncapi.yaml` documents them.
- The linenoise `run()` loop prints queued notices before each prompt.

## module-ncurses (`bin/qrepl`)

- The listener posts each event to an `Application::createEventScope()` scope; the UI thread formats each drained
  batch with `formatThreadNotices()` and appends a `notice` transcript block, which keeps the editor draft, cursor,
  focus and the transcript's reading position.
- The status bar shows `Threads: N` (compact `T<N>`) while N is greater than zero.
- `program-idle` closes the running-threads and `/join` dialogs and resubmits the pending request through
  `start_evaluation()`, which rechecks the Program first; the `thread_monitor` task and its `waitForThreads()` call
  are removed.
- View > Threads, a `--thread-notices=<level>` option, and `/notify` set the verbosity.
- `close_session()` removes the listener and closes the scope before the backend closes.

## Tests

- `examples/test/qore/classes/Program/thread-lifecycle-events.qtest` (`# test-platform: macos` for thread creation
  and cancellation): each outcome; caught and deferred cancellation; a request published once while pending and
  again after it was cleared; a cancellation wave that cancels a thread before its `STARTED`; entry handler failure
  (`START_FAILED`); a teardown exception (`cleanup_error`); the snapshot; nested background threads (parent
  identity); short-lived threads; concurrent subscriptions while threads start and finish (no missed or duplicate
  events, by snapshot and `seq`); TID reuse (more threads than the thread list holds, the events of each TID
  alternating); event masks; activity transitions; unsubscription; several subscriptions; a queue deleted by its
  owner; Program destruction with and without threads running (`CLOSED` last); a limited queue, invalid events and
  self-subscription rejected; sandbox restrictions. Counters and gates only, no sleeps. Clean under valgrind with
  `qore -b --exec-mode=ir`.
- `examples/test/ir/ThreadLifecycleEventModes.qtest`: the events of threads started by code in each execution mode
  (`ExecModeHarness.qm`), as compiled code starts threads through `do_op_background_helper()`.
- `examples/test/qlib/QoreRepl/QoreReplThreadEvents.qtest`: listener delivery, reset and stale events, grouping per
  verbosity level, exclusion of infrastructure threads, close with threads active; WebSocket and action session
  event tests.
- `module-ncurses/test/ncurses-qrepl-live.qtest`: notices preserve the draft and cursor, live status count, dialogs
  close on idle and resume or cancel the right request, `/notify`, quit with threads active.

## Phases

1. Core: hub, `STARTED`/`FINISHED` with outcomes, subscription and snapshot, Program destruction,
   `CANCEL_REQUESTED` and `ACTIVITY` (phases 1 and 2 of the plan were done together, so that no event constant was
   published without its events).
2. QoreRepl: session events, state, notices, verbosity, reset handling.
3. QoreRepl: action session emitter, WebSocket messages, AsyncAPI.
4. module-ncurses: qrepl notices, status count, dialog dismissal, settings.
5. Documentation and release notes.

Each phase's tests pass, without warnings, before the next starts.
