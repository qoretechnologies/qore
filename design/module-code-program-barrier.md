# Module Code at the Program Barrier

Copyright (C) 2026 Qore Technologies, s.r.o.

## Status

Implemented.  This document records where user code runs when another Program calls it, which restrictions apply to
module code, and how calls into a module's code avoid the locks of the module's Program.

Relevant code:

- `lib/QoreClass.cpp` — `QoreSourceProgramHolder::setVariant()`, `qore_get_function_pgm_context()`,
  `qore_get_method_pgm_context()`
- `include/qore/intern/QoreClassIntern.h` — `QoreSourceProgramHolder`, `qore_class_private::getSourceProgramForCall()`
- `lib/Function.cpp` — `QoreFunction::evalFunction()` and its siblings, `QoreCallProgramEntryHelper`,
  `CodeEvaluationHelper::init()`, `qore_get_module_call_runtime_po_override()`,
  `qore_get_user_call_runtime_po_override()`
- `lib/JITRuntime.cpp` — `qore_rt_function_execution_program()`, `qore_rt_method_execution_program()`,
  `qore_rt_call_needs_call_program()`
- `lib/thread.cpp` — `runtime_module_call_inherits_caller_restrictions()`, `ProgramThreadCountContextHelper::set()`,
  `qore_stop_module_uncounted_entries()`
- `lib/QoreSandboxManager.cpp` — `qore_program_set_module_lifetime()`, `qore_release_module_program_pins()`
- `examples/test/ir/module-code-program-barrier.qtest` and the tests named in it

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
restrictions and its sandbox apply.  The JIT and AOT fast paths take the slow path for such calls.

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

The hook is also where the creator-restriction record of qore#5669 is set, for every caller, before the sandbox
decision.

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
   until every thread has left the entries it made without it.  The sequentially consistent increment-then-check of
   the thread and store-then-scan of the stopping thread guarantee that one of them sees the other.
3. **Thread data cache** (`ThreadProgramData::findModuleProgramData()`): during an uncounted entry, the thread's local
   data in the module's Program is cached in the thread, so the entry takes no lock of the Program; the cache is
   disabled when the thread's data is deleted at its end.  The top-level local variables added to the Program after
   the thread's first entry are instantiated for it as on an entry without the cache
   (`qore_program_private::instantiateThreadVarData()`).

4. **No debugger** (`qore_program_private::getDebugProgram()`): every entry into a Program checks whether a debug
   program is attached to it; a Program without one is checked without its `tlock`.  This applies to every Program.

Consequences: a module's Program does not count the threads running its code for calls made this way, so
`Program::getThreadList()` and the thread activity events of a module's Program do not show them.  Thread entry
handlers and thread initialization of the module's Program still run.
