# Program Thread Entry and Exit Handlers

Copyright (C) 2026 Qore Technologies, s.r.o.

## Purpose

An application that embeds Qore code in `Program` objects often keeps a context of its own for each Program: the
identity of the code, the restrictions that apply to it, the tenant it belongs to. A natural place for that context
is a `thread_local` variable of the embedding Program, set when a thread starts running a child Program's code.

`Program::setThreadInit()` cannot keep such a context correct. Its code runs only on a thread's first entry into a
Program (`ThreadProgramData::saveProgram()` returns true once per thread and Program). Threads are shared: an async
I/O callback worker (`AsyncIoController::submitTask()`, timer and socket callbacks, and every qlib dispatcher built on
them) or a `ThreadPool` worker runs the callbacks of every Program that uses it. A worker that runs a callback of
Program A, then one of B, then one of A again keeps B's context for A's second callback: A's thread initialization
code does not run again. If A is sandboxed and B is not, A's code runs with B's privileges.

`Program::setThreadEntryHandler(*code on_enter, *code on_exit)` runs code on every entry into a Program and on the
matching exit, so the embedding code can save its context, set the Program's, and restore the saved one.

## API

```
bool Program::setThreadEntryHandler(*code on_enter, *code on_exit)
    [dom=THREAD_CONTROL,EMBEDDED_LOGIC; flags=NAMED_ARGS]
```

- `on_enter` is called with no arguments; its return value is kept by the entry and passed to `on_exit`.
- `on_exit` is called with one argument: that value (or `NOTHING` when there is no `on_enter`).
- Both null clears the handlers. The return value is true if the Program had handlers before, as for
  `setThreadInit()`.

The name parallels `setThreadInit()`: both are per-Program, per-thread hooks installed by the embedding code. "Entry
handler" names the pair, as every exit belongs to an entry. One method sets both handlers, so a pair is replaced
atomically: a thread never runs an entry handler of one pair and the exit handler of another. The token passed from
`on_enter` to `on_exit` lets the handlers save and restore without a stack of their own, and keeps nested entries
correct by construction. The parameters are `*code` rather than typed callables (`code<auto()>`), as typed code
parameters change how a closure argument's return type is checked at parse time.

There is no function form like `set_thread_init()`: a function acts on the calling Program, and code must not set its
own handlers (see Sandboxing).

## Semantics

### Entry

A thread *enters* a Program when it starts running that Program's code while its *entered Program* is another one or
none. The runtime keeps the entered Program per thread (`ThreadData::entry_pgm`); it differs from the current Program
(`ThreadData::current_pgm`), which is also set without running a Program's code: to parse or commit a Program, to look
up its metadata (`QoreProgramContextHelper`, `ProgramRuntimeParseContextHelper`, `ProgramRuntimeParseAccessHelper`).

Entries:

- a call that switches to the Program (`ProgramThreadCountContextHelper::set()`): functions, methods, constructors,
  destructors, closures, call references, `Program::callFunction()` and friends, signal handlers, async I/O
  callbacks, debugger callbacks, module initialization;
- a call into the current Program when it is not the entered one (same helper, the path that used to return early);
- a thread started in the Program by `background`, for its whole life;
- a parse commit started from another Program (`ProgramRuntimeParseCommitContextHelper`), which runs initializers.

A call within the entered Program is not an entry and runs no handler ("re-entry into the same Program").

### Nesting

Entries nest. A calls the embedding code (core), which calls B, which calls core, which calls A:

```
enter A  (saves none, sets A)
  enter B  (saves A, sets B)
    enter A  (saves B, sets A)
    exit A   (restores B)
  exit B   (restores A)
exit A   (restores none)
```

The code of core runs in the context of the Program that called it; that is intended (embedding code called by a
child sees which child called it). Exits run in the reverse order of entries, including when an exception propagates.

### First entry and thread initialization

On a thread's first entry into a Program, `on_enter` runs first and the thread initialization code second, so the
initialization code runs in the context `on_enter` set. `ThreadProgramData::saveProgram()` no longer runs the
initialization code; its callers do, after the entry handler:

- `ProgramThreadCountContextHelper::set()` runs it after the switch is complete (current Program, local data,
  context helper chain, runtime configuration, sandbox call budget) and after the entry handler; before this change
  it ran inside `saveProgram()`, before the context helper chain and runtime configuration were updated. A first
  entry whose `on_enter` fails has already created the thread's local data for the Program, so later entries are not
  "first" any more: `ThreadLocalProgramData::thread_init_pending` is set when a runtime entry creates the data and
  cleared when the initialization code runs, so it runs on the thread's first entry that succeeds.
- A background thread runs it in `op_background_thread()`, after the thread has been registered in its Program, under
  the inherited sandbox (`design/thread-sandbox-inheritance.md`), and after the entry handler; before this change it
  ran in `QoreThreadList::activate()`, before the inherited sandbox was applied. Its exceptions are still handled at
  once and do not stop the thread.

### Thread exit

A background thread started in the Program exits its entry after its expression has returned, its object references
have been released and its thread resources purged, and before it releases the Program's thread count. Its
thread-local data is destroyed after that; destructors that run then enter their Program as any call does. No handler
runs because a thread ends: every exit is paired with an entry.

### Exceptions

- If `on_enter` raises an exception, the entry fails: the switch is undone (thread count, local data, context chain,
  sandbox budget), no `on_exit` runs, the exception is raised in the caller's sink, and the Program's code does not
  run (callers of `ProgramThreadCountContextHelper` already abort on an exception after `set()`, as for a failed
  `incThreadCount()`). A background thread whose entry fails runs nothing and releases its expression.
- `on_exit` runs even when an exception is pending (an entry ending by an exception, a destructor run while the stack
  unwinds). Its exceptions are appended after the pending ones; the exit always completes.
- Each handler runs with a sink of its own, so a pending exception in the caller's sink neither stops it nor is
  mistaken for its failure. C++ `AbstractException`s are converted.

### Recursion

While a handler runs on a thread (`ThreadData::entry_handler_depth`), entries run no handlers. A handler normally
calls into the embedding Program, which can have handlers of its own; without this rule two Programs whose handlers
are each other's code would recurse without end.

### Replacement and thread safety

Handlers are an immutable, reference-counted `QoreThreadEntryHandlers` object, stored in
`qore_program_private::thread_entry_handlers` (atomic, written under `tlock`). An entry references the object under
`tlock` and keeps the reference until its exit, so the exit handler that runs is the one paired with the entry handler
that ran, with its token. A replacement applies to later entries. The old object is released outside the lock, as
releasing closures can run destructors.

### Program destruction

- A thread cannot enter a Program being destroyed: `incThreadCount()` fails before the entry handler runs. A
  background thread exits its entry before it releases the thread count, and a thread inside the Program holds the
  count until its exit handler has run, so deletion waits for exit handlers.
- The destroying thread can still enter the Program to run destructors of its data
  (`waitForTerminationAndClear()`); those entries run the handlers. The handlers are cleared after
  `clearNamespaceData()`, next to the thread initialization code, and again in `del()`.
- A handler belonging to a Program that has been deleted fails as any call into it does (`PROGRAM-ERROR`); the entry
  fails and the child's code does not run.
- The Program holds strong references to its handlers and to its thread initialization code in C++ members, and its
  Program object reports them to the garbage collector as Pattern B private data (`design/dgc.md`), so a cycle
  through them is collected like any other (issue #5590); see "Garbage collection" below.

## Garbage collection

A handler or thread initialization closure that refers to the Program object, directly or through other objects or
Programs (two Programs whose handlers call each other's Program, or an application object that owns its Program and
that its handlers refer to), forms a reference cycle through C++ members of the Program. The Program object reports
those members to the scanner, so the cycle is found and collected when nothing outside it refers to it:

- **The reporting object.** `qore_program_private::scan_holder` is the Program object that holds the Program as its
  private data, set by the `Program` constructors and `ProgramControl::createEvalProgram()`
  (`qore_program_private::setScanHolder()`). It is cleared (`clearScanHolder()`) wherever the object releases the
  Program or hands it out: every such path goes through `KeyList` (`derefAll()`, `getAndClearPtr()`,
  `getAndRemovePtr()`), which calls `qore_object_private_data_released()` for the entry first. That covers the
  destructor, the methods that hand the Program to a user module (`loadApplyToUserModule()` and the others), the
  `!Program` parameters that take it from its object (`load_user_module_with_program()`,
  `Program::loadUserModuleWithProgram()`, and the C++ API `QoreObject::getAndClearPrivateData()`), and an object
  whose constructor failed, which releases its private data without a destructor. `qore_program_private::del()`
  clears the holder if the object still holds the Program when its data is deleted (a Program cleared at the end of
  the process by `qore_cleanup()` while its owner holds the object, #6088). It is held with a weak reference and changed under `tlock`. Only this object reports the values, so each value is
  reported once. A Program without a Program object (the main Program, one created in C++) reports nothing: no
  object of the cycle could hold it.
- **Scanning.** `qore_object_private::scanMembers()` probes `CID_PROGRAM` and calls
  `qore_program_private::scanMembers()`, which tries `tlock` (a held lock makes the scan due again,
  `RSetHelper::containerContended()`) and reports `thr_init`, `on_enter` and `on_exit`. The object counts in
  `scan_private_data` (`scan_registered`) only while one of them needs a scan, so a Program with no code, or with
  function references only, costs scans nothing.
- **Changes.** `setThreadInit()` and `setThreadEntryHandler()`, under `tlock`, mark the object with
  `RObject::edgesRemoved()` when they give up a value that needs a scan (it is released after the lock), and mark
  each value stored with `qore_dgc_value_stored()` (`scanValuesChangedIntern()`), as a Queue does.
- **Threads running the handlers.** A thread that ran the entry handler holds a reference to the
  `QoreThreadEntryHandlers` until it has run the exit handler. Each reference to that object holds a reference to
  each handler as well (`QoreThreadEntryHandlers::ref()`), so the thread's hold is a reference from outside the
  cycle: `RSet::canDelete()` keeps the cycle and watches the closure, and the thread's release when it leaves the
  Program rechecks it. The cycle is then collected on that thread, while it still runs in the Program: the
  Program object is deleted at once and the Program is kept until the thread has left it (`Program::destructor()`). The thread initialization code is referenced by the thread while it runs
  (`doThreadInit()`), which has the same effect.

`examples/test/qore/classes/Program/program-code-cycles.qtest` builds each cycle - handlers, thread initialization
code set with `setThreadInit()` and with `set_thread_init()`, a handler referring to its own Program, an object that
owns its Program, a Program created by `ProgramControl::createEvalProgram()` - releases the last outside reference,
and checks through destructors that the Programs and the values the code refers to are destroyed; it also checks
that a cycle held from outside or by a thread running the handlers is kept until that reference goes, and that a
Program taken from its object by a call, or released by an object whose constructor failed, is released. It is clean
under valgrind.

## Sandboxing

The method has the functional domains of `setThreadInit()` (`THREAD_CONTROL`) and of creating Programs
(`EMBEDDED_LOGIC`): a Program restricted with either cannot call it, at parse time or through a dynamic call. The
handlers decide the context in which every thread entering the Program runs, which is a decision of the code that
embeds the Program.

The calling code also cannot set the handlers of the Program it runs in (`THREAD-ENTRY-HANDLER-ERROR`). Code normally
has no Program object for itself, but embedding code could pass one; replacing or clearing its own handlers would let
child code run with whatever context the thread had before, the defect the handlers fix.

Handlers run in the thread's current context, including the sandbox of the calling code (sandbox resolution walks the
context helper chain, which includes the entry), so a handler cannot be used to run code without the caller's
restrictions.

## Performance

- No handlers: a switch into a Program reads `thread_entry_handlers` once (relaxed atomic load) and stores the entered
  Program twice (entry and exit); a call within the entered Program compares one pointer
  (`td->entry_pgm != pgm`). The JIT fast paths that skip `ProgramThreadCountContextHelper` for calls within the
  current Program add one relaxed atomic load.
- With handlers: each entry takes `tlock` to reference the handlers, calls `on_enter`, allocates a one-element
  argument list and calls `on_exit` on exit. A handler in another Program is itself a Program switch.

## Hook points and switch-path audit

All points that change `ThreadData::current_pgm` or `tlpd` were classified:

| Site | Runs the Program's code? | Handling |
|---|---|---|
| `ProgramThreadCountContextHelper::set()` / destructor (`lib/thread.cpp`) | yes | entry frame; used by AST (`CodeEvaluationHelper`, `UserVariantExecHelper`), the IR interpreter, every JIT and AOT runtime call helper (`lib/JITRuntime.cpp`, `lib/QoreAOTRuntime.cpp`), signals, async I/O, logging, reflection, debugger, module manager, `QoreExternalProgramContextHelper` |
| same helper, current-but-not-entered path | yes | entry frame without a switch |
| `op_background_thread()` | yes | entry frame for the thread's life |
| `ProgramRuntimeParseCommitContextHelper` | yes (initializers) | entry frame |
| JIT direct calls that skip the helper when `exec_pgm == caller_pgm` (`qore_rt_call_fast*`, `qore_rt_call_direct_aot`) | yes | also take the helper when `thread_entry_pending()` |
| IR inline calls, JIT/AOT self-recursive and same-program direct native calls (`isApproachBEligible()`) | yes, but only from code already running in the same Program | none needed: the caller's entry covers them |
| `QoreProgramContextHelper`, `ProgramRuntimeParseContextHelper`, `ProgramRuntimeParseAccessHelper`, `CurrentProgramRuntime*ParseContextHelper`, `thread_ensure_local_program_data()` | no (lookups, parsing, lazy IR build) | none; a call into the Program made under them is an entry through the not-entered path |
| `QoreThreadList::activate()` | no (sets up the thread's data) | thread initialization moved to the background thread's entry |

The not-entered path makes the audit robust: whatever sets the current Program without entering it, the next call
into that Program's code enters it.

Helpers that are kept beyond `set()` keep the sink given to `set()` for their destructor; the async I/O worker and the
logger bridge gave a sink that went out of scope first, which is fixed (the sink now outlives the helper).

## Tests

- `examples/test/qore/classes/Program/thread-entry-handler.qtest`: single entry through every call kind, no re-entry,
  A-B-A on one `submitTask()` worker and on one timer worker (`setMaxCallbackWorkers(1)` before the pools exist),
  nesting, first entry and thread initialization (also after a failed first entry), threads started in the Program,
  exceptions in both handlers and during unwinding, deleted handler Programs, deletion while a thread is inside,
  release of replaced handlers, replacement while inside and under concurrency, clearing with `NOTHING` while inside,
  the exit handler getting the value of its own entry's handler, parse commit, functional domains, own Program. CI
  runs it in every execution mode.
- `examples/test/qore/security/thread-sandbox-inheritance.qtest`: the entry handler and the thread initialization code
  of a thread started for sandboxed code run under the inherited sandbox.
- `examples/test/ir/ThreadEntryHandlerModes.qtest`: a driver and a fixture module run in AST, IR, JIT and tiered mode
  with native compilation checked, and with the module and the driver compiled by `qcc`.
- `examples/test/qore/classes/Program/program-code-cycles.qtest`: cycles through the handlers and the thread
  initialization code are collected (issue #5590); see "Garbage collection".
