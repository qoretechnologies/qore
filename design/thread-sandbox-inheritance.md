# Thread Sandbox Inheritance

Copyright (C) 2026 Qore Technologies, s.r.o.

## Overview

A unit of work that a sandboxed Program causes to run must be governed by that Program's sandbox even when it runs
on another thread. Qore's fine-grained sandbox — the `QoreSandboxManager` attached to a Program with
`setSandboxManager()`, which carries filesystem and network policy, program interrupts, cancellation scope and a
thread limit — is resolved on each thread by a call-stack walk (`find_thread_sandbox_manager_ref_intern`,
`lib/thread.cpp`). A freshly started thread has no caller stack, so without extra work the walk finds nothing and
the new thread runs unsandboxed.

This document describes how a new thread inherits the sandbox of the code that started it, so that **a thread can
never run with more privilege than the code that caused it**. It complements
[cooperative-cancellation.md](cooperative-cancellation.md) (how interrupts and cancellation reach a thread) and
[module-sandboxing-audit-guide.md](module-sandboxing-audit-guide.md) (the two sandbox mechanisms).

## The model

When a thread is started on behalf of Qore code, the sandbox that governs the starting thread is captured and
applied on the new thread **as the outermost caller of everything the thread runs**. Concretely:

- the capture is a `QoreSandboxContext` (the managers that govern the starting thread, plus the ID of the Program
  the manager was found on), held for the life of the thread;
- on the new thread the context is installed as `ThreadData::inherited_sandbox_context`, which
  `find_thread_sandbox_manager_ref_intern()` consults **last**, after the current Program, the enclosing caller
  Programs, and the call-program-context fallback (`lib/thread.cpp`);
- so a Program the thread enters that has a sandbox manager of its own still governs the code it runs, exactly as in
  a synchronous call from the starting code. The inherited sandbox only applies where nothing else does — which is
  the case that was previously unsandboxed.

The inherited sandbox also sets the thread's **cancellation scope**: a thread started on behalf of a sandboxed
Program is in that Program's cancel scope wherever it runs (`check_cancel_in_scope()`, `get_cancel_scope_pgm_id()`),
so `cancel_thread()` from the sandboxed code reaches it and a program interrupt of that sandbox stops it. A thread
started for an *unrestricted* caller is not in any sandbox's scope, so sandboxed code cannot cancel it.

### Which crossings inherit

| Seam | Inherits the starter's sandbox? | Where |
|---|---|---|
| `background` operator | **yes** | `do_op_background()` captures, `op_background_thread()` applies (`lib/thread.cpp`) |
| `q_start_thread()` (all three overloads) | **yes**, unless `QTF_NO_SANDBOX_INHERIT` | `q_capture_start_thread_sandbox()`, `q_run_thread()` applies (`lib/thread.cpp`) |
| `call_async()`, `parallel_map()`, `parallel_foreach()` | **yes** (they use `q_start_thread()`) | `lib/ql_thread.qpp` |
| async I/O timer callback | **yes** — the sandbox of the code that added the timer | `addTimer()` captures, delivered via the callback worker (`lib/AsyncIoControllerPriv.cpp`) |
| `ThreadPool` workers | **no** (opt out) — each task carries its own capture; the workers count against the creating Program's sandbox (see "The thread limit") | `QTF_NO_SANDBOX_INHERIT`; `ThreadTask` captures per task (`include/qore/intern/ThreadPool.h`) |
| async I/O controller I/O and callback threads | **no** (opt out) — each operation carries its own capture | `QTF_NO_SANDBOX_INHERIT`; `AsyncIoSandboxContext` per operation (`lib/AsyncIoControllerPriv.cpp`) |
| `ml` `OnnxSessionPool` async/leader threads | **no** (opt out) — each request/batch carries its own capture | `QTF_NO_SANDBOX_INHERIT`; `QoreSandboxCapture` per request (`modules/ml`) |

### Default inherit, opt-out for shared workers

A thread that serves **one** unit of work for **one** caller (`background`, `call_async`, a module's per-caller
worker) inherits by default — this is the safe default and needs nothing from the module author.

A thread that serves **many** callers — a worker pool or an event loop shared across Programs or sandboxes — must
**not** inherit the sandbox of whichever caller happened to start it, or that one caller's sandbox would govern
everyone's work. Such a thread sets `QTF_NO_SANDBOX_INHERIT` and instead:

1. captures the submitter's sandbox when the work is submitted, in the submitting thread, with `QoreSandboxCapture`
   (public, `qore/QoreSandboxManager.h`) or, for libqore-internal code, `QoreSandboxContext` /
   `AsyncIoSandboxContext`;
2. applies it around that submitter's work on the worker thread with `QoreSandboxCaptureHelper` (public) /
   `QoreSandboxContextHelper` (internal).

`ThreadPool` (`include/qore/intern/ThreadPool.h`) is the reference implementation of this pattern.

An applied capture governs the thread only as its **outermost caller** (step 4 of the lookup in
`find_thread_sandbox_stack_ref_intern()`): a sandboxed Program that the thread runs decides with its own sandbox, and
inside a system-policy call (`SandboxManager::createSystemPolicyCall()`) no sandbox does.  That is right for running
work on behalf of the submitter, but not for an object that checks later operations against the policy of the code
that created it, wherever they run (module-jni's `QoreSandbox.Policy`, qore#5644).  Such an object checks against
`QoreSandboxCapture::getPolicyManagerRef()` — the manager that a thread with no Qore context of its own resolves with
the capture applied, whatever thread calls it — **in addition to** the policy of the current thread
(`QoreSandboxManagerHelper(QoreSandboxManagerHelper::Policy)`), so that the stricter one wins and a capture never
lowers the sandbox of the code that uses it.  A capture made inside a system-policy call is sandboxed
(`isSandboxed()`) but has no policy manager, as the call has the system policy.  The `QoreSandboxCapture` copy
constructor derives a capture from a held one without applying it.

An applied context never lowers the sandbox of code that runs inside its scope: a sandbox check on the worker uses
the context's manager together with the managers of the Programs entered after the context was applied (the
current Program and the program-context frames above `ThreadData::sandbox_context_pgm_ctx`), combined so that the
stricter rule wins; a policy barrier pushed inside the scope stops the walk as it does without a context.  A context
captured inside a policy barrier (`SandboxManager::createSystemPolicyCall()`) while sandboxed code was on the stack
has a manager but no network or filesystem policy, so without this a sandboxed Program called by the task
(`Program::callFunction()`) would run with no policy at all.  In debug builds, a socket or QUIC operation created
while the current Program has a sandbox asserts that the policy lookup found one
(`qore_socket_ref_policy_sandbox_manager()`).  Test: examples/test/qore/security/sandbox-context-policy.qtest.

## The thread limit

`QoreSandboxManager::setMaxThreads(n)` limits the number of threads that code governed by the sandbox may have
running at the same time: `background`, `call_async()`, the workers of `parallel_map()`/`parallel_foreach()`, and
threads a module starts with `q_start_thread()` on the sandboxed code's behalf — exactly the threads that inherit
the sandbox. The thread that enters the sandboxed Program from outside is not counted, and neither are the async
I/O threads, which do not inherit the sandbox.

The threads of a `ThreadPool` do not inherit a sandbox either, but they count against the sandbox of the Program that
created the pool (the Program running the constructor), if it has one: the pool takes a reference to that Program's
`QoreSandboxManager` when it is created (keeping it if the Program later gets another manager), its own thread holds a
slot from construction until the pool has stopped (the constructor raises `THREAD-CREATION-FAILURE` if none is free),
and each worker holds one of its thread slots while it is in the pool, also while idle. The slots are taken and
released under the pool's lock: a slot is taken before a worker is started, and released when a worker leaves the pool
(idle release, `done()` not requeuing it) or when the pool stops, so the slots held are always the pool's workers, and
a worker that is still exiting never holds one. When no slot is free, queued tasks wait for one of the pool's busy
workers or for a slot that another thread of the sandbox releases (see below), and idle workers beyond the limit
(`min_idle_threads`) are not started. A pool with no worker could not run a queued task, so `submit()` reserves the
slot of the first worker in the submitting thread when the pool has none, raising `THREAD-CREATION-FAILURE` there if
the sandbox has no free slot. A pool created by a Program without a sandbox manager serves every caller and is not
limited; a module's Program inherits the manager of the Program that loads the module first
(`inherit_module_sandbox_manager()`), so a pool that the code of a module first loaded by sandboxed code creates is
counted (qore#5635; test: `examples/test/qore/security/thread-pool-thread-limit.qtest`).

A pool whose queued tasks wait for a slot is woken by its own workers becoming free, by a slot that any other thread
of the sandbox releases and by a raised limit (`setMaxThreads()` notifies like a release): the pool's thread registers in a registry of waiting pools (keyed by manager, in
`lib/QoreSandboxManager.cpp`) before it tries to take a slot, and `QoreSandboxManager::releaseThreadSlot()` wakes the
pools registered for the manager (and for the members of a composition) after releasing the slot. Registering before
the attempt and reading the registry count after the release (both sequentially consistent) means either the pool
gets the slot or the release sees the pool. A notifier takes the registry lock, references the waiting pools, releases
the registry lock and then takes each pool's lock to signal it; a pool releases its own slots with its lock held
quietly (`releaseThreadSlotQuiet()`) and notifies after unlocking, so pool locks never nest. A pool does not wake
itself for its own release, and after a failed attempt followed by its own release (the rollback of a composition)
it tries again only when another thread released a slot, so it cannot spin. A notifier references a waiting pool
only while the pool still has references (`optRef()`), so a pool being destroyed is never revived.

A thread is counted when its inheritance is captured (`QoreInheritedSandbox::capture()`), before the thread is
created, so the count is accurate the moment `background`/`q_start_thread()` returns. Starting a thread at the limit
raises `THREAD-CREATION-FAILURE` ("cannot start a thread: sandbox thread limit of N reached") in the starting code.
A counted thread stops counting when its work ends (`QoreInheritedSandbox::releaseSlot()`), which runs before the
thread's Program sees it end, so code waiting for a Program's threads to finish can rely on the slots being freed.
Lowering the limit does not affect threads already running.

### Restricted callers and module code

`PO_NO_THREAD_CONTROL` restricts the code of the Program that has it, when that code is parsed; it does not stop
module code that the Program calls from starting threads, including threads that run the Program's own closures (a
module function or method doing `background c()` with a closure that the caller passed in). The `background` operator
in module code is checked against the module's own parse options only. Such threads inherit the caller's sandbox and
count against its thread limit like any other thread started on the sandboxed code's behalf, so the sandbox thread
limit bounds them. A user module that the restricted Program loads first inherits its restrictions and cannot use
`background`; module threads come from modules that the host loaded already or loads with a parse option mask.

`PO_NO_THREAD_CONTROL` does not stop a Program from creating a `ThreadPool` (the class is in the `THREAD_CLASS`
domain; `PO_NO_THREAD_CLASSES` refuses it); the workers of a pool that the sandboxed Program creates count against its
thread limit as described above. The `MODULE_OWNED` builtins (`call_async()`,
`parallel_map()`, `parallel_foreach()`, the `AsyncIoController` timer and task submitters) additionally refuse to run
the code of a caller that may not control threads; a module's own closure that calls the caller's code is accepted, and
its thread is counted as well.

A blanket run-time refusal of `background` in module code for restricted callers is deliberately not implemented:
many modules run their callers' code on their own threads (pollers calling subclass methods, `HttpServer` handlers,
WebSocket event callbacks, logger appenders), and the sandbox already governs those threads.

Tests: `examples/test/qore/security/module-thread-limit.qtest` (a real user module, `SandboxThreadFixture.qm`, running
the closures of a `PO_NO_THREAD_CONTROL` caller) and `examples/test/qore/security/thread-sandbox-inheritance.qtest`.

### Default limit

A `QoreSandboxManager` has no thread limit (0) unless one is set, and this default is kept deliberately (qore#5635):

- no single number suits every host: a pipeline or service host legitimately runs many threads per sandbox, while a
  preview or tenant host needs a small bound;
- a non-zero default would silently limit every existing sandbox, including those a host creates only for a
  filesystem or network policy, and the members of a composed sandbox, whose tightest positive limit applies;
- the process-wide thread table (`MAX_QORE_THREADS`) still bounds all threads, and the presets `createLockdown()` (1)
  and `createWebSafe()` (4) set a limit.

Without a limit, sandboxed code can exhaust the process's threads for the host and for other sandboxes, so the
documentation (`SandboxManager::setMaxThreads()`, the sandboxing guide's resource limits, best practices and security
checklist) states that a host running untrusted code must set one. In Qorus, every tenant and interface sandbox should
get a limit with a configurable default and an administrator override; this is host configuration, not a libqore
change.

## C++ API

| Symbol | Header | Purpose |
|---|---|---|
| `QTF_NO_SANDBOX_INHERIT` | `qore/qore_thread.h` | `q_start_thread()` flag: the new thread does not inherit and is not counted; for shared pools that capture per task |
| `QoreSandboxCapture` | `qore/QoreSandboxManager.h` | capture the current thread's sandbox at submit time |
| `QoreSandboxCaptureHelper` | `qore/QoreSandboxManager.h` | apply a capture around a task on a worker thread |
| `QoreSandboxCapture::getPolicyManagerRef()` | `qore/QoreSandboxManager.h` | the captured policy manager, for explicit checks on any thread (with the thread's own policy) |
| `QoreSandboxCapture(const QoreSandboxCapture&)` | `qore/QoreSandboxManager.h` | copy a capture without applying it |
| `QoreInheritedSandbox` / `QoreInheritedSandboxHelper` | internal (`qore_thread_intern.h`) | libqore-internal capture/apply for `background` and `q_start_thread()`, with thread-limit accounting |

A module that starts a per-caller thread with `q_start_thread()` needs no change: inheritance is automatic. A module
that runs many callers' work on a shared thread it owns, or that runs Qore callbacks on a thread a native library
owns, must capture and apply the submitter's sandbox itself (see the module guide).

## Compatibility

This changes the behaviour of threads started with `q_start_thread()` by external modules: such a thread now inherits
the starting code's sandbox by default, and counts against the sandbox thread limit. A module pool that serves many
callers must set `QTF_NO_SANDBOX_INHERIT` and capture per task, as `ThreadPool` does. Code with no sandbox manager
attached is unaffected: capture finds no manager, nothing is inherited, and nothing is counted.
