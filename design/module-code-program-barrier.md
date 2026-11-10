# Module Code at the Program Barrier

Copyright (C) 2026 Qore Technologies, s.r.o.

## Status

Implemented.  This document records where user code runs when another Program calls it, which restrictions apply to
module code, and how calls into a module's code avoid the locks of the module's Program.

Relevant code:

- `lib/QoreClass.cpp` — `QoreSourceProgramHolder::setVariantSlow()`, `qore_get_function_pgm_context()`,
  `qore_get_method_pgm_context()`
- `include/qore/intern/QoreClassIntern.h` — `QoreSourceProgramHolder`, `qore_class_private::getSourceProgramForCall()`
- `lib/Function.cpp` — `QoreFunction::evalFunction()` and its siblings, `QoreCallProgramEntryHelper`,
  `CodeEvaluationHelper::init()`, `qore_get_module_call_runtime_po_override()`,
  `qore_get_user_call_runtime_po_override()`
- `lib/JITRuntime.cpp` — `qore_rt_function_execution_program()`, `qore_rt_method_execution_program()`,
  `qore_rt_call_needs_call_program()`
- `lib/thread.cpp` — `runtime_module_call_inherits_caller_restrictions()`, `runtime_module_call_override_in_effect()`,
  `ProgramThreadCountContextHelper::set()`, `qore_stop_module_uncounted_entries()`,
  `find_thread_sandbox_stack_ref_intern()`
- `lib/QoreIRInterpreter.cpp` — `ensureInterpreterInlineIRFunctionState()`, `tryExecuteInterpreterInlineIRFunction()`
- `lib/QoreSignal.cpp` — `QoreSignalManager::removeProgramHandlers()`; `lib/ModuleManager.cpp` —
  `QoreModuleManager::delUser()`
- `lib/QoreSandboxManager.cpp` — `qore_program_set_module_lifetime()`, `qore_release_module_program_pins()`
- `include/qore/intern/ThreadPool.h` — `ThreadTask`; `lib/QC_ThreadPool.qpp` — `ThreadPoolThread::worker()`
- `examples/test/ir/module-code-program-barrier.qtest`, `examples/test/ir/module-code-cleanup.qtest` and the tests
  named in them

## Where user code runs

User code runs in the Program that defines it, in every execution mode:

| Code | Program |
|---|---|
| a method (static or not) | the source Program of its class (`qore_class_private::spgm`) |
| a function | the Program that defined its variants (`UserVariantBase::pgm`) |
| a closure | the Program that created it |
| a builtin function | the Program of the call (explicit, else the caller's) |

A function and a method of a module therefore behave alike: both resolve the names they look up at run time
(`call_function()`, `create_object()`, `load_module()`, reflection) in the module's Program, use its thread-local data
and create objects owned by it.  A function imported with `Program::importFunction()` or inherited by a child Program
runs in the Program it comes from, as a method of an imported class does.

`QoreSourceProgramHolder` keeps that Program allocated while the thread enters and runs it: `set()` for a class,
`setVariant()` for a function (the Program is identified by address and ID, as it can have been deleted; a call after
its deletion raises `PROGRAM-ERROR`).  The holder is declared before the helper that enters the Program.

A call with an explicit Program that neither defines the function nor runs it (`QoreProgram::callFunction()` of an
imported function) enters that Program first (`QoreCallProgramEntryHelper`), so the call is made from it: its
restrictions and its sandbox apply.  The JIT and AOT fast paths take the slow path for such calls.  The explicit
Program of a call through a call reference is the Program that made the reference; when code of a third Program
makes the call, an AST call gives its argument expressions, which read the caller's locals: the helper evaluates them
in the caller's context before it enters the Program (see
`examples/test/qore/classes/Program/call-reference-program-args.qtest`).

## Restrictions of module code

A module is a capability: it is checked when it is loaded first (it inherits the loader's restrictions), and any
Program that imports it afterwards can use it.  Module code runs with its own Program's options; at run time only the
sandbox governs it.  `qore_get_module_call_runtime_po_override()` is the single boundary hook for every call into module
code (functions, methods, closures, call references, nested module calls):

- for a sandboxed calling context (`runtime_module_call_inherits_caller_restrictions()`: the caller's or the running
  Program is sandboxed, the thread runs for sandboxed code, or module code already runs for a sandboxed caller), the
  caller's restrictions are composed into the module code's effective options, as before; the override carries
  `QoreCallParseOptionsOverride::sandboxed`, so the restrictions pass on to the module code it calls even when its own
  options have them already
- otherwise module code runs with its own options; the caller's parse options restrict the caller's own code only

The QCF_OWN_PROGRAM_STATE, QCF_CURRENT_THREAD_ONLY and QCF_MODULE_OWNED relief and checks, the call-reference lockdown
and the per-application timer isolation all key on the composed override, so they apply to sandboxed callers exactly
as before.  The override is cleared when module code calls code of another Program.  `CodeEvaluationHelper` computes
it once per crossing; `UserVariantBase::eval()` does not compute it again.

The override records the module Program it was computed for (`QoreCallParseOptionsOverride::override_pgm`, kept in
`ThreadData::runtime_po_override_pgm`).  A call of module code by code of the same module, made in the module's Program
while that override is in effect (`runtime_module_call_override_in_effect()`), keeps it instead of computing it again:
the override it would compute is the same, as the caller's effective options already hold the module's options and the
restrictions it inherits, and it runs for the same application and sandbox.  Any other override (that of code of
another Program, `LoadTimeParseOptionsHelper`, an entry into the module's Program without a crossing such as a signal
handler) records no Program, so the next call computes the override.

The caller's restrictions are composed and recorded (`QoreCallParseOptionsOverride::caller_restrictions`) only for a
sandboxed caller; the hook records nothing for any other caller (the creator-restriction record of qore#5669 is not
part of this change).

## Calls into a module's Program without its locks

Entering another Program normally takes a reference to it (the class lock or the global Program registry lock),
its thread count (its `plock` and `tidmap`) and its thread-local data (its `tlock`).  All of these are shared by every
thread that calls into the Program, so concurrent calls into a popular module contend for them.

The Program of a registered user module (source or AOT) is deleted only by the module manager's cleanup:

1. **Module lifetime** (`qore_program_set_module_lifetime()`, called by `QoreModuleManager::addModule()`): the Program
   keeps a dependency reference until `qore_release_module_program_pins()`, which `qore_cleanup()` calls once no other
   thread can run Qore code (after the thread pool threads and the signal thread have ended, before the binary modules
   are deleted).  A call that has found, with a reference held, that the Program has module lifetime caches that in
   the variant (`UserVariantBase::pgm_module_lifetime`) or the class (`qore_class_private::spgm_module_lifetime`);
   later calls take no reference and no lock.
2. **Uncounted entries** (`ProgramThreadCountContextHelper::set()`): while allowed, entering a Program with module
   lifetime takes no thread count; each thread counts its own entries (`ThreadData::module_uncounted`).
   `QoreModuleManager::delUser()` calls `qore_stop_module_uncounted_entries()` before it clears the module Programs:
   later entries take the thread count (and fail once the teardown has started), and it waits on a condition variable
   until every thread has left the entries it made without it.  It checks the data of every thread, whatever its
   status (`QoreThreadList::anyThreadData()`): the signal thread (TID 0, not on the TID list) runs a handler set by
   module code in the module's Program, and a foreign thread being deregistered can still run module code.  The
   sequentially consistent increment-then-check of the thread and store-then-scan of the stopping thread guarantee
   that one of them sees the other.
3. **Thread data cache** (`ThreadProgramData::findModuleProgramData()`): during an uncounted entry, the thread's local
   data in the module's Program is cached in the thread, so the entry takes no lock of the Program; the cache is
   disabled when the thread's data is deleted at its end.  The top-level local variables added to the Program after
   the thread's first entry are instantiated for it as on an entry without the cache
   (`qore_program_private::instantiateThreadVarData()`).

4. **No debugger** (`qore_program_private::getDebugProgram()`): every entry into a Program checks whether a debug
   program is attached to it; a Program without one is checked without its `tlock`.  This applies to every Program.

## Cost of the barrier

A call within a Program, the usual case, must cost no more than it did before module code ran in its own Program; a
crossing pays for entering the Program, and nothing else:

- **Inline fast paths**: `QoreSourceProgramHolder::setVariant()`, `qore_class_private::getSourceProgramForCall()`,
  `qore_get_user_call_runtime_po_override()` and the holder's destructor are inline for the current Program, the
  explicit Program of the call (held by the caller of `QoreProgram::callFunction()`, so it takes no reference and not
  the registry lock), a Program with module lifetime and code that is not module code; the rest is out of line.  The
  caller's effective options are read only when an override is composed.
- **Sandbox lookup**: cancellation points run on every loop iteration and look up the sandbox manager of the thread
  (`find_thread_sandbox_stack_ref_intern()`), which checks the current Program and the Program left by every context
  on the thread's chain.  A lookup that finds none records that in the current context
  (`ProgramThreadCountContextHelper::sandboxChainClear()`) with the generation of the managers
  (`qore_sandbox_manager_generation`, incremented after a manager is set on any Program), so later lookups in the same
  context skip the chain; not under a module initialization or policy barrier, which end the walk early.
- **Context**: `ProgramThreadCountContextHelper` keeps the thread's data for its exit, and the override helper for its
  restore (`RuntimeParseOptionsOverrideHelper`, which is also embedded in `CodeEvaluationHelper` instead of allocated).
  The context leaves the members that `set()` writes before they are read uninitialized, as one is created on every
  call; the Program switch updates the data of the thread's `RuntimeConfig` directly (`rc_get_tls_impl()`, looked up
  once per thread) and checks the sandbox manager and sandbox flags of a Program inline.
- **Override**: a call of code that is not module code clears the caller's override without building an override
  (`RuntimeParseOptionsOverrideHelper::setCleared()`); a crossing into module code builds it in place
  (`qore_set_user_call_runtime_po_override_slow()`).
- **Function Program**: a call without a resolved variant (a call reference, `call_function()`) takes the Program of
  the function from a user variant that the function keeps once it has looked one up
  (`QoreFunction::getProgramVariant()`).
- **IR inlining**: the IR interpreter inlines the body of a function of another Program as it does a static method of
  another Program: it takes the function's Program with `setVariant()` and enters it.  A call with an explicit Program
  that did not define the function is not inlined, and a call into another Program is never evaluated as a native leaf
  without entering it.

## Entries into module code at the module cleanup

A signal handler that module code sets belongs to the module's Program and stays installed until that Program is
deleted, while the signal thread runs until the end of `qore_cleanup()`.  `QoreModuleManager::delUser()` therefore
removes the handlers of every Program with module lifetime (`QoreSignalManager::removeProgramHandlers()`) right after
`qore_stop_module_uncounted_entries()`, before Phase 1 clears the module Programs' data: otherwise a handler could enter
a module's Program between the wait for its threads (`waitForTermination()`) and `clearNamespaceData()`.  A handler of
one of them that is running when they are removed ends first, with the release of the signal thread's local data
after it, unless the signal thread itself runs the cleanup.

Consequences:

- a module's Program does not count the threads running its code for calls made this way: `Program::getThreadList()`,
  the idle and active transitions of the module's Program (`ProgramThreadHub`) and the debugger's view of the threads
  in the module's Program (`getThreadsInProgram()`, `breakProgram()`, `breakProgramThread()`, which use the thread
  count's `tidmap`) do not include them; they are threads of the calling Programs, which count them
- the module cleanup waits for every thread in module code before it clears any module's data, where it used to wait
  for the threads of each module's Program just before clearing that Program.  A thread that is not ended before the
  cleanup (an asynchronous I/O, foreign or signal thread) and waits in one module's code for another module's data to
  be destroyed now keeps the cleanup waiting; before, that depended on the order in which the modules were cleared.
  A thread blocked in module code with nothing to release it kept the cleanup waiting before as well
- thread entry handlers and thread initialization of the module's Program still run

The handlers are not the only way in: a signal handler of a Program that is not a module's (one that module code
created and keeps in the module's data), a thread pool or asynchronous I/O thread and a foreign thread run outside the
threads that `qore_cleanup()` waits for.  So the cleanup also closes the Programs with module lifetime to every thread
but its own (`qore_close_module_programs()`, right after `qore_stop_module_uncounted_entries()`): a later entry from
another thread (`ProgramThreadCountContextHelper::set()`) raises `PROGRAM-ERROR` instead of taking the thread count.
The threads that entered before are waited for by `waitForTermination()` in Phase 1, as on develop.  That wait is not
bounded: a thread blocked in module code for good keeps the cleanup waiting, as clearing the data under it would
destroy what it uses; the close ensures that only entries made before it can do so.

The work that the cleanup hands to other threads is part of the cleanup, and the module Programs stay open to it
(`qore_module_cleanup_delegation()`, `QoreModuleCleanupDelegateHelper`, `ThreadData::module_cleanup_delegate`):

- a ThreadPool task that the cleanup thread submits (`ThreadTask` records it when it is created, in the submitting
  thread), while it runs, is cancelled or released, until the worker has cleared the thread-local data the task
  created
- a background thread that the cleanup thread starts (`BGThreadParams`), for its whole life up to the deletion of its
  data
- in turn, such work of a thread that runs such work

The destructors that the cleanup runs (of the objects in the module Programs' data in Phase 1, and of the objects in
the Programs that a native binary module deletes in Phase 2) are module code that can hand work to a ThreadPool and
wait for its result, as the destructor of an object that makes a REST call does: refusing that work would make the
destructor wait forever, which is a hang, not a refusal
(`examples/test/module-cpp-api/program-owner/program-owner-teardown.qtest`).  Such work runs while the cleanup thread
waits for it, as the cleanup thread would run it itself.  Work handed over by any other thread (a ThreadPool task or
a background thread of a foreign thread, a task that was submitted before the close) is refused, as is a foreign thread
itself, which the cleanup cannot have started (`examples/test/ir/module-code-cleanup.qtest`).
