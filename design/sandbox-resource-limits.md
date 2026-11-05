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
| 3 | CPU time per call | `SANDBOX-TIMEOUT` | planned |
| 4 | memory, then the `q_sb_*` allocation API | `SANDBOX-MEMORY-LIMIT` | planned |

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

### Exception

`SANDBOX-TIMEOUT`, description: `<operation>: the sandbox wall time limit of <N> ms was exceeded`. When several
budgets have expired, the outermost one is reported.

### Failing closed

A frame that cannot be recorded (no memory for the budget or the frame, or the watchdog thread cannot be started)
must not let the call run without its limit. The stack records the helper that failed (`lost_owner`) and sets the
limit bit; until that helper leaves, every cancellation point raises `SANDBOX-ERROR` ("the sandbox limits cannot be
enforced for this call"), and nothing further is recorded. A continuation captured meanwhile carries a
static sentinel budget (`qore_sandbox_lost_budget()`) that has the same effect on the threads it starts.

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

- `setCPUTimeLimit(int ms)`: the CPU time of all threads attached to the call's budget, summed.
- Per-thread CPU clocks: `pthread_getcpuclockid()` + `clock_gettime()` on Linux, `thread_info(THREAD_BASIC_INFO)` on
  macOS. A detaching thread adds its consumption to the budget's total.
- The watchdog schedules the budget at `now + remaining / attached_threads` (the earliest point the budget could be
  exhausted), measures, and either expires it with the limit bit (`SANDBOX-TIMEOUT`, naming the CPU limit) or
  reschedules. The CPU clock is never read at cancellation points.

## Phase 4: memory

As proposed in the issue, with the decisions above:

- an **account** per limited manager, separate from the manager and referenced by every charged storage, so it
  outlives the manager and the Program while bytes are charged to it;
- the current account cached in `ThreadData`, updated by the same seams that maintain the call budgets;
- per-thread borrowed chunks (`fetch_add` on the account), shrinking near the limit;
- the charged account recorded in the private storage of strings, lists, hashes and binaries; release on free from
  any thread;
- nested managers: the thread's chunk is drawn from every ancestor account with a limit;
- an over-limit bit in `interrupt_flags` (raised as `SANDBOX-MEMORY-LIMIT` at the next cancellation point), cleared
  when usage drops; checks before allocation where an `ExceptionSink` is available;
- `q_sb_malloc()`, `q_sb_realloc()`, `q_sb_free()` for modules.

## Documentation

- `doxygen/lang/222_sandboxing.dox.tmpl`: the limits, the per-call model and its limitations.
- `design/module-sandboxing-audit-guide.md`: `SANDBOX-TIMEOUT` reaches modules through `qore_check_cancel()`.
- `design/cooperative-cancellation.md`: the reason bits of the cancellation word.
- Release notes: the 3.0 sandbox entry mentions the enforced wall-time limit (the API is unreleased, so this is not a
  separate entry).
