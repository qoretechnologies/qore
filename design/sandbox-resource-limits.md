# Sandbox resource limits

Copyright (C) 2026 Qore Technologies, s.r.o.

Issue: [qore#5581](https://github.com/qoretechnologies/qore/issues/5581).

## Overview

`SandboxManager` gets enforced limits on **wall time**, **CPU time**, **stack** and **memory**. Commit 2e12b303af
removed the earlier setters because nothing enforced them; this design puts them back only together with their
enforcement, in four phases:

| Phase | Limit | Exception | Status |
|---|---|---|---|
| 1 | wall time per call, plus interrupt reason codes | `SANDBOX-TIMEOUT` | implemented |
| 2 | stack size per call | `STACK-LIMIT-EXCEEDED` | implemented |
| 3 | CPU time per call | `SANDBOX-TIMEOUT` | implemented |
| 4 | memory, then the `q_sb_*` allocation API | `SANDBOX-MEMORY-LIMIT` | implemented |

It builds on [cooperative-cancellation.md](cooperative-cancellation.md) (how a request reaches a running or blocked
thread), [thread-sandbox-inheritance.md](thread-sandbox-inheritance.md) (which threads a sandbox governs) and
[sandbox-provenance.md](sandbox-provenance.md) (code that runs under a sandbox it was created in).

## Decisions

| Question | Decision | Reason |
|---|---|---|
| Scope of the time limits | **per call**: each entry into a sandbox from code it does not govern starts a new budget | a Program-lifetime limit is useless for long-lived Programs (services, request handlers); a per-call limit bounds each request |
| Work started by a timed call | shares the call's **absolute** deadline (and, in phase 3, its CPU budget) | otherwise `background` or a `ThreadPool` task would escape the limit |
| Who a timeout stops | only the threads of the expired call | other callers of the same sandbox are unaffected; a sandbox-wide interrupt is still available with `requestInterrupt()` |
| Nested timed sandboxes | every enclosing call's deadline stays in force | an inner sandbox cannot extend the time its caller was given |
| Memory passed out of the sandbox | stays charged until it is freed | otherwise a sandbox gets around its limit by handing data to the host |
| Nested memory limits | every ancestor with a limit is charged | as for time: an inner sandbox cannot use more than its caller was given |

## Phase 1: wall-time limit and reason codes

### API

| C++ (`QoreSandboxManager`) | Qore (`SandboxManager`) | Notes |
|---|---|---|
| `setWallTimeLimit(int64 ms)` | `setWallTimeLimit(int max_wall_time_ms)` | 0 = unlimited; a negative value raises `SANDBOX-CONFIG-ERROR` in Qore and is treated as 0 in C++ |
| `getWallTimeLimit()` | `getWallTimeLimit()` | for a composite (provenance) manager: the tightest constituent limit, as `getMaxThreads()` |
| — | `SandboxConfigInfo::wall_time_limit` | |
| `createLockdown()` | `createLockdown()` | 5 s wall time |
| `createWebSafe()` | `createWebSafe()` | 30 s wall time |
| `copy()` | `copy()` | copies the limit |

Changing the limit affects calls that start afterwards; a call in progress keeps the deadline it started with (as
lowering `setMaxThreads()` does not stop running threads).

### What a "call" is

A thread **enters** a manager `M` with a wall-time limit when it starts running code governed by `M` while it has
no active call budget for `M`. The entry points are the places where the governing sandbox of a thread changes:

| Seam | Where | Budget |
|---|---|---|
| a Program with `M` attached becomes current (`run()`, `callFunction()`, `callMethod()`, module and signal callbacks, AOT entry points) | `ProgramThreadCountContextHelper::set()` | **new** |
| code created under `M` runs elsewhere (methods of objects and call references with sandbox provenance) | `QoreSandboxProvenanceHelper::apply()` | **new** |
| a thread started by a timed call (`background`, `q_start_thread()`, `call_async()`, `parallel_map()`) | `QoreInheritedSandboxHelper` | **inherited** from the capture |
| work continued for a timed call on a shared thread (`ThreadPool` task, async I/O operation and callback, `QoreSandboxCapture`) | `QoreSandboxContextHelper`, `QoreSandboxCaptureHelper` | **inherited** from the capture |

A re-entry into `M` while the thread already has a budget for `M` (sandboxed code calling a module that calls back
into the sandbox) does not start a new budget: the deadline cannot be extended by bouncing through other Programs.

Provenance contexts do **not** capture budgets: a method of an object that the sandbox created, called by the host
long after the call that created it returned, is a new call with its own deadline, not a continuation of an expired
one. Continuation captures (`QoreSandboxContext` for threads, pool tasks and async I/O) do capture them.

A composite manager (an enforcement view of provenance) is expanded to its constituents: each constituent with a
limit gets its own budget.

### Data structures

```
QoreSandboxCallBudget  (include/qore/intern/QoreSandboxLimits.h, ref-counted, one per call)
    QoreSandboxManager* manager      the manager the budget belongs to (referenced); identity for re-entry checks
    int64 limit_ms, deadline_us      monotonic clock (q_get_monotonic_us())
    std::atomic<bool> expired
    std::vector<int> tids            threads attached to the budget        (guarded by the watchdog lock)
    heap position                    entry in the watchdog's deadline map  (guarded by the watchdog lock)

ThreadData::sandbox_budgets         QoreSandboxBudgetStack: frames {QoreSandboxCallBudget*, const void* owner}
QoreSandboxContext::budgets         budgets captured for a continuation (referenced); empty for provenance
```

- The thread's vector is only read and written by the thread itself. The owner token is the helper that attached the
  budget; the helper detaches it on exit. A public helper (`QoreSandboxProvenanceHelper`) needs no new member: it
  checks whether the top entry is its own.
- A budget is referenced by the frames and contexts that use it. The watchdog's map does not hold a reference: the
  destructor removes the budget from the map, and the watchdog takes a reference with a try-reference (increment
  only while the count is above zero) before it acts on a budget. A finished call therefore does not keep memory
  alive until its deadline.

### The watchdog

One process-wide thread (named `qore-sb-watchdog`, within the 15-character thread name limit), started on the first
timed call:

- it waits on a condition for the earliest deadline in a `std::multimap<int64, budget*>`; a new earliest deadline
  signals it. This is event-driven: there is no polling interval;
- at expiry it sets `expired`, copies the attached TIDs under the watchdog lock, and then, under `thread_list.lck`,
  sets the **limit bit** in each thread's cancellation word and wakes the thread with the existing per-thread wake
  paths (`wakeCondWaiter()`, `signalWaitingFd()`);
- it is stopped by `qore_cleanup()`, and in a child created with `fork()` it is started again on demand, like the
  condition waker. The Qore `fork()` function restarts it at once in the child if calls are pending, so that a call in
  progress in the forking thread stays bounded;
- lock order: the watchdog lock is a leaf (nothing is acquired while it is held). Expiry releases it before taking
  `thread_list.lck`. Fork handler: `QFLM_ACQUIRE` at `QFLR_LEAF`.

### Reason codes

`ThreadEntry::cancel_requested` changes from `std::atomic<bool>` to a bit set:

| Bit | Set by | Cleared by | Exception |
|---|---|---|---|
| `QCR_THREAD_CANCEL` | `cancel_thread()` | `clear_thread_cancel()`, an out-of-scope drop | `THREAD-CANCELLED` |
| `QCR_SANDBOX_LIMIT` | the watchdog | the thread itself, when it has no expired budget left | `SANDBOX-TIMEOUT` |

- The hot path is unchanged: one seq_cst load of the word; waits that registered for a wakeup see any bit.
- `clear_thread_cancel()` clears only `QCR_THREAD_CANCEL`, so sandboxed code cannot clear a timeout. Catching
  `SANDBOX-TIMEOUT` inside the sandbox does not help either: the next cancellation point raises it again. It stops
  being raised when the thread leaves the expired call (the frame that attached the budget is popped).
- The bit is a hint; the truth is the thread's own budgets. On seeing the bit, `qore_check_cancel()` looks for an
  expired budget on the thread; if there is none (the thread left the call after the watchdog copied the TIDs, or the
  TID was reused), it clears the bit under `thread_list.lck` after checking again, and continues. A waiter woken that
  way resumes its wait, exactly as for an out-of-scope cancellation request.
- The sandbox-wide interrupt flag (`QoreSandboxManager::interrupt_requested`) becomes a bit set as well
  (`interrupt_flags`, with `QSB_INTR_REQUESTED`), and `checkIOInterrupt()` maps its bits to the exception, so phase 4
  can add the memory over-limit bit to the same word as the issue proposes.
- Order in `qore_check_cancel()`: the limit bit first (it cannot be cleared and ends the whole call), then a thread
  cancellation, then a sandbox-wide interrupt.
- Cancellation deferral (`defer_thread_cancel()`, cleanup critical sections) defers `SANDBOX-TIMEOUT` like the other
  reasons, so cleanup code can finish. `defer_thread_cancel()` is in the `THREAD_CONTROL` domain: a sandbox that must
  not be able to postpone its timeout must not grant it (`PO_NO_THREAD_CONTROL`), as for interrupts today.
- The deferral of a trusted class's destructor (qore#5634) never defers a limit: `qore_check_cancel()` checks the
  limit bit before that deferral, so a destructor in a sandbox call cannot run unbounded, and the destructor of a
  sandboxed class runs under no deferral at all (see "Destructors" in
  [cooperative-cancellation.md](cooperative-cancellation.md#destructors)).

### Exception

`SANDBOX-TIMEOUT`, description: `<operation>: the sandbox wall time limit of <N> ms was exceeded`. When several
budgets have expired, the outermost one is reported.

### Failing closed

A frame that cannot be recorded (no memory for the budget or the frame, or the watchdog thread cannot be started)
must not let the call run without its limits. The stack records the helper that failed (`lost_owner`), the limits
of the call (`lost_kinds`: `QSB_LIMIT_WALL_TIME`, `QSB_LIMIT_STACK`) and their values, and sets the limit bit; with
a stack limit it also closes the thread's stack guard at the current position (restored when the helper leaves),
because recursion reaches no cancellation point. Until the helper leaves, every cancellation point (and with a stack
limit, the next stack check) raises `SANDBOX-ERROR` naming exactly the limits that cannot be enforced:

```
<operation>: the sandbox wall time limit of 300 ms and the sandbox stack size limit of 262144 bytes cannot be
enforced for this call: the resources to track them could not be allocated
arg: {"wall_time_limit": 300, "max_stack_size": 262144}
```

Nothing further is recorded meanwhile. A continuation captured meanwhile carries one of three static sentinel
budgets (`qore_sandbox_lost_budget(kinds)`, one per combination of limits), so the threads it starts fail the same
way; the values are not known there, so the description omits them and the `arg` keys have no value.

`SANDBOX-TIMEOUT` and the sandbox `STACK-LIMIT-EXCEEDED` carry the same kind of `arg` (`{"wall_time_limit": ms}`,
`{"max_stack_size": bytes}`), so code can tell which limit ended a call without parsing the description.

The debug-only builtin `dbg_sandbox_fail_next_budgets(count)` makes the next budget recordings fail, which tests
every fail-closed path deterministically (`testFailClosed()` in `sandbox-limits.qtest`).

### Limitations (documented)

- Native code that never reaches a cancellation point (a long regex, a sort of a huge list, a blocking third-party
  call that is not a cancellable wait) is stopped when it returns, as with `requestInterrupt()`.
- Cancel callbacks (`registerCancelCallback()`, used by database drivers to cancel a running query) are registered
  per manager, and a timeout targets threads, so a timeout does not invoke them; a thread blocked in such a query
  sees the timeout when the query returns. Thread-targeted cancel callbacks would serve `cancel_thread()` as well and
  are a follow-up.
- The wake-up latency is the scheduling latency of the watchdog plus the distance to the next cancellation point.

### Tests (`examples/test/qore/security/sandbox-limits.qtest`)

- API: setters, getters, negative values, configuration key, presets, `copy()`.
- A busy loop (`while`, `for`, `foreach`, `do`) raises `SANDBOX-TIMEOUT` in the `ast`, `ir`, `jit` and `tiered`
  modes (`sandbox-limits-exec-modes.qtest`, through `ExecModeHarness`; with `jit`, the trace shows that each loop
  ran as native code), and in an AOT-compiled module called from the sandbox. A Program created at runtime does not
  take the execution mode of the command line, so the script sets it with `Program::setExecMode()`.
- Blocked waits end at the deadline: `sleep()`, `Queue::get()`, `Mutex`, `Condition`, `Counter`.
- Per call: two calls in sequence each get the full limit; a call that finishes in time leaves nothing behind; the
  host continues normally after catching the exception.
- Concurrency: a timed-out call does not affect another thread's call into the same sandbox.
- Escape attempts: catching the exception and continuing, `clear_thread_cancel()`, bouncing through another Program,
  a `background` thread, a `ThreadPool` task, all stopped at the original deadline.
- Nested sandboxes: the outer deadline stops code in the inner sandbox; the inner limit applies on its own.
- Provenance: a closure and an object method created in the sandbox and called later by the host get a new budget.
- Negative: a manager without a limit, a Program without a manager, and a manager whose limit was set to 0 never
  raise `SANDBOX-TIMEOUT`.

## Phase 2: stack limit

### API

| C++ (`QoreSandboxManager`) | Qore (`SandboxManager`) | Notes |
|---|---|---|
| `setMaxStackSize(int64 bytes)` | `setMaxStackSize(int max_stack_bytes)` | 0 = unlimited; negative: `SANDBOX-CONFIG-ERROR` in Qore, 0 in C++ |
| `getMaxStackSize()` | `getMaxStackSize()` | tightest constituent limit for a composite manager |
| — | `SandboxConfigInfo::max_stack_size` | |
| `createLockdown()` / `createWebSafe()` | same | 1 MB / 2 MB (the removed presets had recursion depths of 100 / 200) |

### Design

- The stack limit uses the **same call records** as the wall-time limit: a budget is started when the entered
  manager has a wall-time **or** a stack limit (`QoreSandboxCallBudget::stack_bytes`); only budgets with a wall-time
  limit are scheduled with the watchdog and track their threads. The re-entry rule is therefore the same: a thread
  that re-enters a sandbox it is already running a call of does not start a new measurement, so sandboxed code
  cannot gain stack by calling back into itself through trusted code.
- Each frame of `QoreSandboxBudgetStack` whose budget has a stack limit **tightens the thread's own guard**
  (`ThreadData::stack_limit`) to `entry position - stack_bytes` (stack growing down) if that is tighter, and
  restores the saved guard when the frame is popped (`qore_sandbox_tighten_stack()` / `qore_sandbox_restore_stack()`
  in `lib/thread.cpp`). This is the technique of `QoreElidedStackHelper`; both are strictly scoped, so they nest.
- `check_stack()` (interpreter, IR, JIT and AOT through `qore_rt_check_stack()`), `q_check_stack()` and
  `q_thread_stack_reserve_exceeded()` all read `ThreadData::stack_limit`, so every tier is covered with no new hook
  and no cost on the call path. Nothing caches the guard.
- A continuation (a thread the call starts, a pool task) attaches the call's budgets and applies the stack limit
  from its own position at that point: the limit bounds the stack the work uses, on whatever thread it runs.
- With nested sandboxes, every enclosing frame's guard stays in force, because a frame only ever tightens it.
- A thread with no stack guard of its own (`QTF_NO_STACK_GUARD`, guard 0) gets the sandbox guard.
- Exception: `STACK-LIMIT-EXCEEDED`, `this thread's stack has exceeded the sandbox stack size limit of <N> bytes`
  when the active guard is the one a frame set (`QoreSandboxBudgetStack::findStackLimit()`), otherwise the existing
  message.

### Tests

- API, configuration key, `copy()`, presets, negative value.
- Unbounded recursion raises `STACK-LIMIT-EXCEEDED` naming the sandbox limit; a moderate depth is allowed; without
  the limit the same code reaches a much greater depth, and the host has its whole stack again after the call.
- Re-entry: the sandbox recurses to two thirds of the depth it can reach and then re-enters itself through trusted
  code; the re-entered code reaches less than three quarters of the full depth (measured in the same mode, so this
  does not depend on frame sizes). Observed in a debug build with a 256 KB limit: full depth 93 (ast) / 44 (ir, jit),
  after the re-entry 29 / 23.
- A `background` thread started by the call, and an inner sandbox without a stack limit of its own.
- All execution modes and AOT-compiled module code (`sandbox-limits-exec-modes.qtest`).

## Phase 3: CPU-time limit

### API

| C++ (`QoreSandboxManager`) | Qore (`SandboxManager`) | Notes |
|---|---|---|
| `setCPUTimeLimit(int64 ms)` | `setCPUTimeLimit(int max_cpu_time_ms)` | 0 = unlimited; negative: `SANDBOX-CONFIG-ERROR` in Qore, 0 in C++ |
| `getCPUTimeLimit()` | `getCPUTimeLimit()` | tightest constituent limit for a composite manager |
| — | `SandboxConfigInfo::cpu_time_limit` | |
| `createLockdown()` / `createWebSafe()` | same | 1 s / 10 s (the values the removed presets documented) |

### Design

- **What counts:** the CPU time of every thread attached to the call's budget, from the moment it attaches (the
  thread that entered, threads the call starts, pool tasks and async I/O operations running for it). The same call
  records serve all three per-call limits; a budget is timed (managed by the watchdog) if it has a wall-time or a
  CPU-time limit.
- **Attachments:** a budget holds one `Attachment` per thread (`tid`, a reference `count`, the thread's
  `QoreThreadCpuClock` and its CPU time at attach as `baseline_ns`). A thread attached twice (a continuation of a call
  it is already running, such as an operation run at once on the thread that submitted it) is counted once. When a
  thread's last attachment ends, it adds `now - baseline` to the budget's `cpu_used_ns`, so the CPU time of threads
  that have left the call keeps counting.
- **Thread CPU clocks:** `pthread_getcpuclockid()` + `clock_gettime()` (Linux, BSDs; configure check
  `HAVE_PTHREAD_GETCPUCLOCKID`), or the Mach thread port + `thread_info(THREAD_BASIC_INFO)` on macOS. Another thread
  can read them, which the watchdog does under its lock; an attached thread is alive, because it detaches under the
  same lock before it ends. Where neither is available, `init()` fails and the call fails closed (`SANDBOX-ERROR`
  naming the CPU limit, `QSB_LIMIT_CPU_TIME`).
- **Scheduling (no polling):** each timed budget has one event in the watchdog's map: the wall deadline or the next
  CPU check, whichever comes first. A CPU check is due at the earliest point the CPU time could be used up:
  `now + (limit - used) / attached threads`, at least `QSB_MIN_CPU_CHECK_US` (1 ms) away. At a check the watchdog
  measures `cpu_used_ns + sum(clock - baseline)`; if the limit is reached it expires the budget (`expired_kind =
  QSB_LIMIT_CPU_TIME`, then the same limit bits and wakeups as the wall-time limit), otherwise it schedules the next
  check. The first thread's event is computed when it starts the call; a thread joining a call with a CPU limit
  schedules a check at once (the CPU time may now run out sooner); a call with no attached thread uses no CPU time
  and has no CPU check until a thread attaches.
- **Overshoot:** at most the 1 ms floor per attached thread, plus the scheduling latency of the watchdog, plus the
  distance to the next cancellation point. The CPU clock is never read at cancellation points.
- **Exception:** `SANDBOX-TIMEOUT`, `<operation>: the sandbox CPU time limit of <N> ms was exceeded`,
  `arg: {"cpu_time_limit": N}`; the wall-time one has `arg: {"wall_time_limit": N}`.

### Tests

- API, configuration key, `copy()`, presets, negative value.
- A busy loop (`while`, `foreach`) is stopped with the CPU message and `arg`; it cannot be stopped before the limit
  in wall-clock time either.
- A call that sleeps for three times its CPU limit completes (waiting is not counted).
- The calling thread blocks on a queue while 4 threads it started spin: only their CPU time can end the call, so the
  test would hang if it did not count.
- With the debug-only `dbg_sandbox_cpu_used()` (the call's CPU time as the watchdog measures it): two tasks run one
  after the other on a one-thread pool; the second, which starts after the first has left the call, sees at least
  the total the first saw (observed: 1656.6 ms then 1656.8 ms; without the detach accounting it would be ~0.2 ms).
- Fail-closed: a CPU-only failure and a failure with all three limits name exactly those limits.

## Phase 4: memory

### API

| C++ (`QoreSandboxManager`) | Qore (`SandboxManager`) | Notes |
|---|---|---|
| `setMemoryLimit(int64 bytes)` | `setMemoryLimit(int max_memory_bytes)` | 0 = unlimited; negative: `SANDBOX-CONFIG-ERROR` in Qore, 0 in C++ |
| `getMemoryLimit()` | `getMemoryLimit()` | tightest constituent limit for a composite manager |
| `getMemoryUsage()` | `getMemoryUsage()` | the bytes charged to the sandbox, including what threads have borrowed |
| — | `SandboxConfigInfo::memory_limit` | |
| `createLockdown()` / `createWebSafe()` | same | 10 MB / 100 MB (the values the removed presets documented) |
| `q_sb_malloc(size, xsink)`, `q_sb_realloc(ptr, size, xsink)`, `q_sb_free(ptr)` | — | exported functions for modules; 16-byte header |

Unlike the time and stack limits, the memory limit is **per sandbox**, not per call: it bounds the sandbox's live
data across calls and threads.

### Objects (`include/qore/intern/QoreSandboxMemory.h`, `lib/QoreSandboxMemory.cpp`)

```
QoreSandboxMemoryAccount   one per QoreSandboxManager (created with it); atomic used and limit; referenced by the
                           manager and by every charge, so it outlives the manager while data is charged to it
QoreSandboxCharge          one per call into a sandbox with a memory limit: the accounts it charges (its own and
                           those of the enclosing call's charge, innermost first, referenced); one atomic word with
                           references (frames, captured continuations; units of 2^44) and charged bytes; whoever
                           takes it to zero deletes it, so data keeps its charge alive after the call ended
QoreSandboxMemTls          thread_local {charge, balance}: the thread's current charge and the bytes it has borrowed
QoreSandboxChargeRecord    {charge, charged} in the private data of each piece of storage
```

- The call budget of phases 1-3 gets the charge (`QoreSandboxCallBudget::charge`); a budget is created if the
  manager has any limit, now including a memory limit. A frame whose budget has a charge makes it the thread's current
  charge and restores the previous one when it is popped (returning the borrowed balance), so continuations (threads,
  pool tasks, async I/O) charge the call that started them, and a re-entry keeps the charge.
- An inner sandbox without a memory limit of its own creates no charge: the thread keeps charging the outer call.

### Charging

- **Storage records what it charged** (`QoreSandboxChargeRecord`): `sync(size)` is called wherever the size of the
  storage changes, `release()` when it is freed or its buffer leaves it (`giveBuffer()`), `swap()` when buffers are
  swapped. Releases always use the recorded size, so a growth site that is not synced only makes accounting less
  precise; it can never unbalance an account or leave a dangling charge. Empty storage never records a charge.
- **Adoption:** storage without a charge that is synced with a non-zero size while the thread has a current charge is
  charged in full to that charge (the sandbox pays for a value it grew, even if the host created it). Storage with a
  charge charges every change to its own charge, whichever thread makes it (the host growing a string the sandbox
  returned charges the sandbox).
- **Where:** `qore_string_private` (`sb`, synced with `allocated` at every allocation site), `qore_list_private`
  (synced with `allocated * sizeof(QoreValue)`), `qore_hash_private` (an estimate per member: the member, its key
  and its list and map nodes, kept in `sb_bytes` always and synced on add, remove and clear; objects are hashes),
  `BinaryNode` (no private data and a public layout: a sparse table `node -> {charge, size}` under a leaf mutex,
  consulted only while the thread has a charge or charged binaries exist, `qore_sb_binary_count`).
- **Fast path:** `qore_sb_charge()` / `qore_sb_release()` against the thread's own charge only adjust the
  thread-local balance. A refill borrows `missing + chunk` (256 KB, shrinking to an eighth of the smallest remaining
  limit near a limit) with one `fetch_add` on the charge and each account; a release beyond 512 KB of balance returns
  all but 256 KB. Charging another charge (data of another call, or a thread outside any call) goes to the atomics
  directly. Code outside a sandbox call with a memory limit pays a thread-local load and a null check, and only where
  storage is (re)allocated, not on every append. The TLS uses the default model: libqore is `dlopen()`ed, and musl
  rejects initial-exec TLS there.
- **Nested:** a charge lists the accounts of every enclosing call with a memory limit, and charges/releases all of
  them; the inner and outer usage both rise and both fall when the data is freed, on any thread.

### Enforcement

- A refill that takes an account over its limit sets the thread's `QCR_SANDBOX_LIMIT` bit (the same bit as the time
  limits). At the next cancellation point, `qore_check_cancel()` (after the expired-budget check) returns the
  thread's balance and checks the accounts of its charge: if one is over its limit it raises
  `SANDBOX-MEMORY-LIMIT` (`<operation>: the sandbox memory limit of N bytes was exceeded (U bytes in use)`,
  `arg: {"memory_limit": N}`), otherwise it clears the bit. The bit is therefore cleared once usage is below the limit
  again: code that catches the exception and frees memory can go on (`clearSandboxLimit()` also checks memory).
- Where the size of a large allocation is known in advance with an `ExceptionSink`, `qore_sb_check_alloc()` raises
  `SANDBOX-MEMORY-LIMIT` before allocating: `strmul()`, `range()`, `q_sb_malloc()`, `q_sb_realloc()`.
- The allocator never fails because of a limit. Overshoot: one operation plus the balances threads have borrowed.
- Fail-closed (phase 1-3 mechanism): a charge that cannot be allocated fails the call with `SANDBOX-ERROR` naming
  the memory limit (`QSB_LIMIT_MEMORY`).

### Benchmark (Release builds, 2026-11-05)

Hot loops in a `Program` (minimum of 5 runs, 4 rounds each; a `Program` created at runtime runs in the default
tiered mode): `develop` (3c7407bc10) against this branch without a sandbox, and this branch in a sandbox with a 4 GB
memory limit (accounting active).

| Loop | develop | branch, no sandbox | branch, memory-limited sandbox |
|---|---|---|---|
| `s += "x"` (100k) | 10-11 ms | 10-11 ms | 11-12 ms |
| `string s = "abcdefgh" + i` (1M) | 340-366 ms | 333-354 ms | 353-378 ms |
| `l += i` (2M) | 298-338 ms | 290-306 ms | 303-318 ms |
| `h{"k" + i} = i` (500k) | 371-395 ms | 373-389 ms | 381-393 ms |
| `b += binary("abcd")` (2M) | 312-339 ms | 293-312 ms | 323-340 ms |

Without a sandbox the branch is within the noise of `develop`. With accounting active the cost is a few percent,
about 10% for binary appends (the sparse table's mutex at each reallocation, the price of not changing the public
layout of `BinaryNode`).

Found on the way, not related to this work: `s += "x"` on a local string in a function is quadratic on `develop`
(about 5 us per append at 400k appends, 2.6x the time for 2x the appends at 10k-20k), in all execution modes.

### Tests

- `sandbox-limits.qtest`: API and presets; string, list, hash, binary and object growth stopped (usage back to 0 after
  each call); `strmul()` / `range()` refused in advance; data returned to the host stays charged and is released when
  the host frees it, growth by the host is charged, host data passed in is not; recovery after catching the
  exception and freeing; a thread started by the call; an inner sandbox without a limit, with a higher one and with a
  lower one (charged to both, released from both); an account that outlives its manager and Program.
- `module-cpp-api/sandbox-memory/sandbox-memory.qtest`: `q_sb_malloc()` / `q_sb_realloc()` / `q_sb_free()` through
  the `cppapiuser` test module: charged in a call, resized and freed by the host, refused beyond the limit (and the
  buffer unchanged), not charged outside a sandbox call.

## Documentation

- `doxygen/lang/222_sandboxing.dox.tmpl`: the limits, the per-call model and its limitations.
- `design/module-sandboxing-audit-guide.md`: `SANDBOX-TIMEOUT` reaches modules through `qore_check_cancel()`.
- `design/cooperative-cancellation.md`: the reason bits of the cancellation word.
- Release notes: the 3.0 sandbox entry mentions the enforced wall-time limit (the API is unreleased, so this is not a
  separate entry).
