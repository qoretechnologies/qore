# Program teardown and shared-class lifetime

How a `Program` that is deleted keeps the code and data of its classes valid for the other Programs and threads
that use them, and where the teardown can still wait for ever.  The code is in `lib/QoreProgram.cpp`
(`qore_program_private::waitForTerminationAndClear()`), `include/qore/intern/QoreClassIntern.h`,
`lib/QoreClass.cpp`, `lib/thread.cpp` and `lib/QoreJIT.cpp`.

## Teardown sequence

`waitForTerminationAndClear()`:

1. waits, under `plock`, for every thread in the Program to leave it (`waitForAllThreadsToTerminateIntern()`);
2. runs the cleanup callback (`qore_call_program_cleanup_callback()`, which cancels the Program's asynchronous I/O)
   outside `plock` and before `ptid` is set: the callback delivers results through dispatcher threads that enter
   the Program, and `incThreadCount()` refuses every thread but the tearing-down one once `ptid` is set;
3. sets `ptid` under `plock` and waits again for every thread in the Program to leave it: threads can enter it
   while the callback runs (the dispatcher threads, a thread calling a call reference into the Program, a method of
   a class of the Program called from a Program sharing the class); no thread but this one can enter it afterwards;
4. deletes the data: thread resources, the exec-class object, saved objects, opaque targets, local and global
   variables, the namespaces (which release the values of the static class variables, see below), the code.

A thread that deletes a Program it runs code in does not wait for itself: `deferDerefIfRunning()` keeps the
reference, and the thread runs the teardown when it leaves the Program's code (`decThreadCount()`).
`waitForAllThreadsToTerminateIntern()` discounts all of the calling thread's entries (`tidmap[tid]`), so a
teardown that runs while the thread is still in the Program (nested in its code) does not wait for itself.

The debug build calls `qore_program_private_base::dbg_teardown_hook` after step 2 (it can wait) and in step 3
after `ptid` is set (under `plock`; it must not wait).  `dbg_hold_program_teardown()`,
`dbg_wait_program_teardown_held()` and `dbg_continue_program_teardown()` use it to enter a Program from another
thread exactly between steps 2 and 3 (`examples/test/qore/classes/Program/program-teardown-entry.qtest`).

## Code of a class called from another Program

A Program that inherits a public class runs the class's code in the Program that defines it (its source Program,
`qore_class_private::spgm`), which can be deleted at the same time in another thread.  Every call into the code
of a class from a thread that is not already in the source Program takes a reference to it
(`QoreSourceProgramHolder`, `qore_class_private::getSourceProgramForCall()`, `getReferencedSourceProgram()`
under `gate.asl_lock`) and enters it (`incThreadCount()`), so the teardown waits for the call (step 1 or 3) or the
call raises `PROGRAM-ERROR` ("the Program accessed has already been deleted").  The reference is released after
the thread has left the Program.  A thread that is already in the source Program (`td->current_pgm == spgm`) uses
it without a reference, as its own entry keeps the Program from being deleted.  The paths are the regular method
calls (`qore_get_method_pgm_context()`), the native code's method calls (`qore_rt_method_execution_program()` in
`lib/JITRuntime.cpp`, which uses `qore_get_method_pgm_context()`), the method lookup of dynamic method calls
(`qore_class_private::getMethodForEval()`), the IR interpreter's inlined method calls and `CodeContextHelperBase`
(thread.cpp).  A source Program whose last reference has been released is being deleted: the reference cannot be
taken, and the call raises `PROGRAM-ERROR` (`QoreSourceProgramHolder::sourceDeleted()`), as entering a Program whose
teardown has started does, unless the calling thread is the one deleting it: its exit hooks, its cleanup callback and
the destructors run while its data is deleted call into its classes, directly or through other Programs, after its
last reference has been released (a Program container released by `load_user_module_with_program()`, for example),
and the Program stays allocated until that deletion returns.  The record covers all of
`waitForTerminationAndClear()`.  The
thread records the Programs whose data it deletes in a thread-local list (`qore_program_torn_down_by_current_thread()`,
which compares addresses only).  A method that does not switch Programs runs in the caller's; the native code never falls back to the
Program of the variant, which is the deleted source Program.

The debug build calls `qore_class_private::dbg_source_program_taken` once a call has taken the source Program and
before it enters it; `dbg_hold_source_program_call()`, `dbg_wait_source_program_call_held()` and
`dbg_continue_source_program_call()` delete the Program, and the Program whose copy of the class held its last
reference, at that point.

## Static class variables

The values of the static variables of a class are released by the Program that defines the class when it is
deleted, even when other Programs share them (`QoreClassList::clear()` passes the Program to the entry whose
class is the original, `qore_class_private::clear()`/`releaseVarValues()`; a namespace without a Program releases
them too).  Each released variable records the ID of the Program that released it (`QoreVarInfo::setOwnerReleased()`;
the ID, as the address of the deleted Program can be reused) before its value is released under its lock; a read
or write from another Program afterwards raises `PROGRAM-ERROR`, as a method call does
(`QoreVarInfo::checkOwnerReleased()`, called by `getLValue()` and `getReferencedValue()` before the initializer runs
and again under the variable's lock, so an access racing with the release raises the same error).  The accessors
that cannot raise exceptions (`getRuntimeReferencedValue()`, `getAsBigInt()` and the others, and the C++ API's
`QoreExternalStaticMember::getValue()`) return no value then.  Code of the defining Program (a destructor run while
the values are released) sees `NOTHING`.  The variables themselves are deleted by the last Program sharing them
(`data_refs`).

## Background JIT compiles

A background compile lowers the IR of the Programs whose code it compiles (the root function and its folded-in
callees) in the compiler thread.  `QoreJIT::enqueueBgCompile()` accepts the work only when the Program of every
IR function is known (`QoreIRFunction::pgm` and `pgm_id`, or the Program of its variant and the ID recorded with it,
`UserVariantBase::pgm_id`, for IR built by AOT code), still has a reference, and does not wait for its compiles (`qore_program_private::bg_compile_closed`, set by
`QoreJIT::waitForBgCompileQueue(QoreProgram*)` under the queue lock before the Program's data is deleted).
`qore_program_private::runWithRegisteredPrograms()` looks the Programs up by ID (`QoreIRFunction::pgm_id`) in
`programid_to_program_map` under its read lock, so a Program is told from a later one at the same address.  The
Programs found are stored in the work (`BgCompileWork::pgms`) and decide which work the deletion of a Program
cancels or waits for.  Refused work leaves the function in its tier for good (`jit_compile_state` 2), as it would be
refused again; only work refused because the compiler stops can be submitted again.  `dbg_close_program_bg_compile()`, `dbg_get_bg_compile_refused_count()` and
`dbg_program_registered_with_id()` test it.

## Caches that outlive a Program

A cache that is not deleted with a Program (a `thread_local` or process-wide cache) must not identify the
Program's objects by their addresses alone: when a Program is deleted, the allocator gives the memory of its
functions, variants, IR and of the Program itself to the objects of the next one, so an object of a later Program
can have every address that the cache holds.  macOS reuses freed memory at once, so a Program parsed right after
another with the same code gets the same addresses for its objects in many runs.  Such a cache also holds the ID of
the Program (`qore_program_private::getProgramId()`, `UserVariantBase::pgm_id`), which is never reused, and is used
only for the same ID.  The objects cached with the ID stay valid while that Program lives: a variant and its IR are
freed only with the Program, and the IR of a variant is published once and never replaced
(`UserVariantBase::setCachedIR()`).

- The IR interpreter's cache of the last closure called inline (`tryExecuteInterpreterInlineIRClosure()`) holds the
  ID of the caller's Program, which is the Program of the closure's variant;
- the cache of the last closure that native code ran as a leaf (`tryExecClosureNativeLeaf()`) holds the ID of the
  Program of the variant.

Before they held the ID, a closure of a new Program with the addresses of a closure of a deleted one ran the freed
IR of the deleted closure, or the IR and the signature of the function that had been given that memory: a wrong
result, a `RUNTIME-TYPE-ERROR` for a parameter of another function, an `IR-EXEC-ERROR` or a crash.
`examples/test/ir/ClosureCacheProgramReuse.qtest` tests it; a test cannot make the allocator reuse the addresses, so
it creates many Programs, starting each round of them with another heap layout.  The caches that resolve type and
hashdecl paths in native code (`qore_rt_resolve_hashdecl_path_cached()` and
`qore_rt_resolve_full_type_path_cached()`) hold a generation that is never reused for the same reason.

## Where the teardown can wait for ever

Steps 1 and 3 wait without a timeout for the threads in the Program; the teardown hangs if one of them never
leaves.  The second wait adds no new kind of wait; it covers the threads that entered while the callback ran:

- an asynchronous I/O callback delivered during the cleanup callback runs user code; if it blocks until something
  that happens only after the Program is deleted, the teardown waits for ever, as it does for any thread that is in
  the Program when it is deleted;
- two Programs deleted at the same time, each by a thread that is in the other one (or a thread that tears a
  Program down while it holds a lock that a thread in that Program waits for), wait for each other;
- the teardown of a Program deleted while the deleting thread ran its code runs when the thread leaves the code,
  while it can still be in the code of the Program that called it, holding that Program's locks; a thread in the
  deleted Program waiting for those locks makes it wait for ever.

These are the waits of a Program deleted while its threads run, which the API documents: deleting a Program waits
for the threads in it.  `Program::waitForThreads()` with a timeout and `Program::cancelThreads()` let a caller
bound them before deleting the Program.
