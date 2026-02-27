# Modern Threading Primitives for Qore

## Overview

Qore 2.3.0 introduces a set of higher-level threading primitives that complement the existing low-level synchronization classes (Mutex, RWLock, Gate, Condition, Counter, Queue, ThreadPool). These primitives address common multi-threaded programming patterns that previously required manual wiring of multiple low-level components.

| Primitive | Purpose | Analogy |
|-----------|---------|---------|
| **Future / Promise** | Cross-thread result delivery | Java `CompletableFuture`, C++ `std::future/promise` |
| **WaitGroup** | Fork-join coordination | Go `sync.WaitGroup` |
| **Semaphore** | N-permit concurrency limiter | Java `Semaphore` |
| **AutoSemaphore** | RAII permit management | Like AutoLock for Mutex |
| **Channel** | Closeable inter-thread communication | Go channels |
| **ChannelIterator** | Iterate a Channel with `foreach` | Go `range` over channel |
| **call_async()** | Async function execution | Go goroutine with return |
| **parallel_map()** | Parallel collection mapping | Java `parallelStream().map()` |
| **parallel_foreach()** | Parallel collection iteration | Java `parallelStream().forEach()` |
| **channel_select()** | Multiplex on multiple Channels | Go `select` statement |

All primitives live in the `Qore::Thread` namespace (classes) or `Qore` namespace (functions) and require the `THREAD_CLASS` functional domain (plus `THREAD_CONTROL` for functions that spawn threads).

## Architecture

### Common Design Principles

1. **Cooperative cancellation via `waitWithInterrupt()`**: All blocking operations use `QoreCondition::waitWithInterrupt()` which atomically handles 500ms polling, `qore_check_cancel()` calls, and lock management. This is the same proven pattern used by Queue and Counter (see `design/cooperative-cancellation.md`). Returns `QORE_COND_RESULT_INTERRUPTED` when the thread is cancelled or a program interrupt occurs.

2. **Exception safety**: All destructors detect threads blocked on the object and raise appropriate errors before waking them, preventing resource leaks and deadlocks.

3. **Copy semantics**: Primitives that represent unique resources (Future, Promise, Channel, AutoSemaphore) throw on copy. Semaphore and WaitGroup support copy (creating independent instances).

4. **Timeout support**: All blocking operations accept an optional `timeout` parameter (0 = infinite wait).

### Implementation Files

| Header | QPP | Test |
|--------|-----|------|
| `include/qore/intern/QoreFuture.h` | `lib/QC_Future.qpp`, `lib/QC_Promise.qpp` | `examples/test/qore/threads/future.qtest` |
| `include/qore/intern/QoreWaitGroup.h` | `lib/QC_WaitGroup.qpp` | `examples/test/qore/threads/future.qtest` |
| `include/qore/intern/QoreSemaphore.h` | `lib/QC_Semaphore.qpp`, `lib/QC_AutoSemaphore.qpp` | `examples/test/qore/threads/semaphore.qtest` |
| `include/qore/intern/QoreChannel.h` | `lib/QC_Channel.qpp`, `lib/QC_ChannelIterator.qpp` | `examples/test/qore/threads/channel.qtest` |
| — | `lib/ql_thread.qpp` (functions) | `examples/test/qore/threads/parallel.qtest` |

All classes are registered in `get_thread_ns()` in `lib/thread.cpp`.

## Future and Promise

### Problem

The `background` operator discards return values (`rv.discard(&xsink)` at `lib/thread.cpp:2936`). Users must manually wire Queue + exception forwarding to get results from background threads.

### Design

Promise is the producer side, Future is the consumer side. One Promise creates exactly one Future via `getFuture()`. Multiple threads can call `Future::get()`.

```
┌──────────┐     getFuture()      ┌──────────┐
│  Promise  │ ──────────────────> │  Future   │
│  (writer) │                     │  (reader) │
└──────────┘                      └──────────┘
     │                                  │
     │ set(value)                       │ get() → value
     │ setError(err,desc)               │ get() → throws
     │ ~Promise() [if pending]          │
     └─────────────────────────────────>│
         signal via QoreCondition
```

### Internal State

```cpp
struct qore_future_private {
    QoreThreadLock lock;
    QoreCondition cond;
    QoreValue result;
    QoreException* exception;
    enum State { PENDING, RESOLVED, REJECTED, DELETED } state;
    int waiting;     // threads blocked in get()
};
```

### Key Invariants

- A Promise can only be set once (PENDING → RESOLVED or REJECTED)
- If a Promise is destroyed while still PENDING, the Future is rejected with `PROMISE-ERROR`
- If a Future is destroyed while threads are blocked in `get()`, they receive `FUTURE-ERROR`
- `getFuture()` can only be called once per Promise

### call_async()

Convenience function that creates a Promise, spawns a background thread wrapping the callable in try/catch, and returns the Future:

```qore
Future f = call_async(\long_running_task(), arg1, arg2);
# ... do other work ...
auto result = f.get();
```

## WaitGroup

### Problem

Counter works for fork-join but has inverted semantics (count down to zero) and no protection against misuse (dec below zero is a runtime error).

### Design

Forward-counting: `add(n)` increments, `done()` decrements, `wait()` blocks until zero.

```cpp
struct qore_waitgroup_private {
    QoreThreadLock lock;
    QoreCondition cond;
    int count;
    int waiting;
};
```

### Key Invariants

- `add()` with n < 1 throws `WAITGROUP-ERROR`
- `done()` when count is already 0 throws `WAITGROUP-ERROR`
- Copy creates an independent WaitGroup starting at count 0

## Semaphore and AutoSemaphore

### Problem

No N-permit concurrency limiter. Gate is reentrant/binary. Counter blocks on zero but doesn't provide acquire/release semantics.

### Design

Classic counting semaphore: `acquire()` decrements (blocking if 0), `release(n)` increments, `tryAcquire()` for non-blocking.

AutoSemaphore provides RAII: constructor acquires, destructor releases. Supports explicit `release()` and `acquire()` for advanced use cases with protection against double-release/double-acquire.

```cpp
struct qore_semaphore_private {
    QoreThreadLock lock;
    QoreCondition cond;
    int permits;
    int waiting;
    bool deleted;
};
```

## Channel and ChannelIterator

### Problem

Queue lacks close semantics. Consumers can't distinguish "temporarily empty" from "permanently done". No iteration protocol.

### Design

Go-style channel with buffered and unbuffered modes:

- **Buffered** (capacity > 0): FIFO buffer; `send()` blocks when full, `recv()` blocks when empty
- **Unbuffered** (capacity = 0): Synchronous handoff; `send()` blocks until `recv()` is ready and vice versa

```
Producer                    Channel                     Consumer
────────                    ───────                     ────────
send(value) ──────>  ┌─────────────────┐  ──────> recv() → value
                     │ [v1] [v2] [v3]  │
send(value) ──────>  │ (blocks if full)│  ──────> recv() (blocks if empty)
                     └─────────────────┘
close() ──────────>  closed=true         ──────> recv() → NOTHING (after drain)
```

### Internal State

```cpp
struct qore_channel_private {
    QoreThreadLock lock;
    QoreCondition send_cond, recv_cond;
    std::deque<QoreValue> buffer;
    int capacity;
    int send_waiting, recv_waiting;
    bool closed, deleted;
    // External waiter support for channel_select
    std::vector<QoreCondition*> external_waiters;
};
```

### NOTHING Value Handling

A key design requirement: NOTHING must be a valid value sent through a Channel. The `recv()` method uses a `has_value` output parameter (C++ level) to distinguish between:
- Receiving a NOTHING value (has_value=true, returned value is NOTHING)
- Channel closed and drained (has_value=false, returned value is NOTHING)

At the Qore API level, `recv()` returns NOTHING for both cases (closed+drained and NOTHING value). The `tryRecv()` method returns a hash with `has_value` and `value` keys to allow disambiguation. The ChannelIterator uses the C++ `has_value` parameter internally.

### External Waiter Mechanism

`channel_select()` requires waiting on multiple Channels simultaneously. This is implemented via an external waiter registration API on the Channel's private implementation:

```cpp
void registerExternalWaiter(QoreCondition* cond);
void deregisterExternalWaiter(QoreCondition* cond);
```

When any `send()` or `recv()` completes, all registered external waiters are signaled in addition to the Channel's own condition variables. This allows `channel_select()` to share a single condition variable across all case Channels.

## parallel_map and parallel_foreach

### Problem

No built-in way to process collections in parallel.

### Design

Both functions use a work-stealing pattern with `std::atomic<int> next_index`:

```
Main Thread                     Worker Threads (N)
───────────                     ──────────────────
Create ParallelExecState        │
Start N workers via q_start_thread()
                                ├── Worker 0: while (next_index < total)
                                │     results[idx] = func(items[idx])
                                ├── Worker 1: while (next_index < total)
                                │     results[idx] = func(items[idx])
                                └── ...
Wait on Counter
Check for errors
Return ordered results
```

### Exception Handling

Cross-thread exception propagation requires careful handling of Qore's `active_exceptions` counter:

- Worker threads use `ExceptionSink::getExceptionInfo()` (not `assimilate()`) to capture exception information. This properly decrements the worker thread's `active_exceptions` counter via internal `catchException()`.
- The exception info is stored as a `QoreHashNode*` (with err/desc/arg keys).
- The main thread re-raises the exception via `raiseExceptionArg()`.
- Only the first exception is captured; subsequent worker exceptions are cleared.

**Important**: `ExceptionSink::assimilate()` must NOT be used for cross-thread exception transfer because it moves exception nodes without decrementing the source thread's `active_exceptions` counter, causing an assertion failure at thread exit (`q_run_thread(): Assertion 'xsink.isException() || !td->active_exceptions' failed`).

## channel_select

### Design

Multiplexes across multiple Channel send/recv operations:

1. **Non-blocking try**: Try all cases with `trySend()`/`tryRecv()` in random order
2. **If none ready**: Register a shared `QoreCondition` with each Channel
3. **Wait loop**: Wait on the shared condition via `waitWithInterrupt()`, re-try all cases on wake
4. **Cleanup**: Deregister from all Channels

### Fairness

Ready cases are shuffled using `std::shuffle` with a thread-local RNG before each attempt, ensuring no channel is systematically preferred.

### All-Closed Detection

If all channels are in a terminal state (closed for send, closed+empty for recv), `channel_select()` returns NOTHING immediately instead of blocking forever. This is tracked via an `all_dead` flag updated on each `try_cases` iteration.

## Integration Points

### Cooperative Cancellation

All blocking operations (Future::get, WaitGroup::wait, Semaphore::acquire, Channel::send/recv, channel_select) use `QoreCondition::waitWithInterrupt()` for cancellation-aware blocking. This method:

1. Checks `qore_check_cancel()` before entering the wait
2. Waits on the condition variable with 500ms polling intervals
3. Re-checks `qore_check_cancel()` after each poll
4. Returns `QORE_COND_RESULT_INTERRUPTED` (2) if the thread is cancelled or a program interrupt occurs
5. Returns `QORE_COND_RESULT_TIMEOUT` (1) if the user-specified timeout expires
6. Returns `QORE_COND_RESULT_SUCCESS` (0) if the condition was signaled

All wait loops follow this canonical pattern (matching Queue and Counter):

```cpp
int64 cond_timeout = (timeout_ms <= 0) ? -1 : timeout_ms;  // map 0→infinite

AutoLocker al(&lock);
while (!condition_met && !deleted) {
    int rc = cond.waitWithInterrupt(&lock, cond_timeout, xsink);
    if (rc == QORE_COND_RESULT_INTERRUPTED) {
        // thread cancelled or program interrupt — clean up and return
        return -1;
    }
    if (condition_met || deleted) {
        break;
    }
    if (rc == QORE_COND_RESULT_TIMEOUT) {
        // user timeout expired — clean up and return
        return -1;
    }
}
```

**Important**: The timeout convention differs between the Qore API (0 = infinite) and `waitWithInterrupt()` (-1 = infinite). All primitives map the timeout: `int64 cond_timeout = (timeout_ms <= 0) ? -1 : timeout_ms;`

**Historical note**: An earlier implementation used manual `lock.unlock()` → `qore_check_cancel()` → `lock.lock()` → plain `cond.wait()` loops. This was replaced with `waitWithInterrupt()` to avoid race windows between unlock/relock and to use the same proven pattern as Queue and Counter that was hardened through the SmartMutex/RWLock signal-loss bug fixes (see `design/cooperative-cancellation.md`).

### Functional Domains

- Classes: `THREAD_CLASS` — required to create/use any threading class
- Functions that spawn threads (`call_async`, `parallel_map`, `parallel_foreach`): `THREAD_CLASS | THREAD_CONTROL`
- `channel_select`: `THREAD_CLASS | THREAD_CONTROL`

### Deterministic GC

Channel objects are scanned by the deterministic garbage collector (via `memberGate()`) since they hold `QoreValue` references in their internal buffer, similar to Queue.

## Migration Guide

### From Counter to WaitGroup

```qore
# Before (Counter)
Counter done(num_tasks);
for (int i = 0; i < num_tasks; ++i) {
    background sub() {
        # ... do work ...
        done.dec();
    }();
}
done.waitForZero();

# After (WaitGroup)
WaitGroup wg();
for (int i = 0; i < num_tasks; ++i) {
    wg.add();
    background sub() {
        on_exit wg.done();
        # ... do work ...
    }();
}
wg.wait();
```

### From Queue to Channel

```qore
# Before (Queue - no close semantics)
Queue q();
background sub() {
    for (int i = 0; i < 10; ++i) {
        q.push(i);
    }
    q.push(NOTHING);  # sentinel
}();
while (True) {
    auto val = q.get();
    if (!exists val) break;  # can't distinguish NOTHING value from sentinel
    process(val);
}

# After (Channel - proper close semantics)
Channel ch(10);
background sub() {
    for (int i = 0; i < 10; ++i) {
        ch.send(i);
    }
    ch.close();
}();
foreach auto val in (ch.iterator()) {
    process(val);  # NOTHING values work correctly
}
```

### From background + Queue to call_async

```qore
# Before
Queue q();
background sub() {
    try {
        auto result = expensive_computation();
        q.push({"value": result});
    } catch (hash<auto> ex) {
        q.push({"error": ex});
    }
}();
# ... do other work ...
auto msg = q.get();
if (msg.error) throw msg.error.err, msg.error.desc;
auto result = msg.value;

# After
Future f = call_async(\expensive_computation());
# ... do other work ...
auto result = f.get();  # exception propagation is automatic
```

### Parallel Collection Processing

```qore
# Before (manual thread pool)
Queue results();
Counter done(items.size());
for (int i = 0; i < items.size(); ++i) {
    background sub(int idx) {
        results.push({"index": idx, "value": process(items[idx])});
        done.dec();
    }(i);
}
done.waitForZero();
# ... manually sort results by index ...

# After
list<auto> results = parallel_map(\process(), items);
# results are already in order
```
