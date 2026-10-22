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
| `ThreadPool` workers | **no** (opt out) — each task carries its own capture | `QTF_NO_SANDBOX_INHERIT`; `ThreadTask` captures per task (`include/qore/intern/ThreadPool.h`) |
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

## The thread limit

`QoreSandboxManager::setMaxThreads(n)` limits the number of threads that code governed by the sandbox may have
running at the same time: `background`, `call_async()`, the workers of `parallel_map()`/`parallel_foreach()`, and
threads a module starts with `q_start_thread()` on the sandboxed code's behalf — exactly the threads that inherit
the sandbox. The thread that enters the sandboxed Program from outside is not counted, and neither are the shared
opt-out threads (ThreadPool, async I/O), because they do not inherit the sandbox.

A thread is counted when its inheritance is captured (`QoreInheritedSandbox::capture()`), before the thread is
created, so the count is accurate the moment `background`/`q_start_thread()` returns. Starting a thread at the limit
raises `THREAD-CREATION-FAILURE` ("cannot start a thread: sandbox thread limit of N reached") in the starting code.
A counted thread stops counting when its work ends (`QoreInheritedSandbox::releaseSlot()`), which runs before the
thread's Program sees it end, so code waiting for a Program's threads to finish can rely on the slots being freed.
Lowering the limit does not affect threads already running.

## C++ API

| Symbol | Header | Purpose |
|---|---|---|
| `QTF_NO_SANDBOX_INHERIT` | `qore/qore_thread.h` | `q_start_thread()` flag: the new thread does not inherit and is not counted; for shared pools that capture per task |
| `QoreSandboxCapture` | `qore/QoreSandboxManager.h` | capture the current thread's sandbox at submit time |
| `QoreSandboxCaptureHelper` | `qore/QoreSandboxManager.h` | apply a capture around a task on a worker thread |
| `QoreInheritedSandbox` / `QoreInheritedSandboxHelper` | internal (`qore_thread_intern.h`) | libqore-internal capture/apply for `background` and `q_start_thread()`, with thread-limit accounting |

A module that starts a per-caller thread with `q_start_thread()` needs no change: inheritance is automatic. A module
that runs many callers' work on a shared thread it owns, or that runs Qore callbacks on a thread a native library
owns, must capture and apply the submitter's sandbox itself (see the module guide).

## Compatibility

This changes the behaviour of threads started with `q_start_thread()` by external modules: such a thread now inherits
the starting code's sandbox by default, and counts against the sandbox thread limit. A module pool that serves many
callers must set `QTF_NO_SANDBOX_INHERIT` and capture per task, as `ThreadPool` does. Code with no sandbox manager
attached is unaffected: capture finds no manager, nothing is inherited, and nothing is counted.
