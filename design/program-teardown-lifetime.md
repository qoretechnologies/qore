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
   a class of the Program called from a Program sharing the class); no thread but this one can enter it afterwards,
   except a thread that is in it, whose nested calls into it the teardown waits for as well
   (`qore_program_private::canEnterIntern()`);
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

## Caches of a Program's code that outlive it

A cache that outlives the Programs whose code it describes (a thread-local or a static cache) must find its entries
by the ID of the Program as well as by the addresses of the code's objects.  The functions, variants, IR and
signatures of a Program are freed with it, and the Program created after it, its functions, variants, IR and the
closures they create can have the addresses of the deleted ones, so an entry found by addresses alone gives the new
code what was cached for the deleted code.  The thread-local caches of closure calls are found so:

- the IR interpreter's inline closure call (`tryExecuteInterpreterInlineIRClosure()` in `lib/QoreIRInterpreter.cpp`)
  finds its entry by the closure, its function and the ID of the calling Program, which owns the function (the call
  is only made inline when it does); it caches the variant, its IR and signature.  It found the entry by the
  Program's address, and ran the deleted Program's freed IR with its freed signature: a wrong result, a crash, or a
  reference argument released wrongly (`examples/test/qore/misc/dgc-opaque-object-local-release.qtest` saw a cycle
  collected while its Program lived, in the IR execution mode, on macOS);
- native code's call of a closure as a native leaf (`tryExecClosureNativeLeaf()` in `lib/JITRuntime.cpp`) finds its
  entry by the variant, its IR and the ID of the variant's Program; it caches the analysis of the IR (the leaf's
  operation) and the return type, and computed the deleted closure's operation.

A Program's functions and variants live as long as it does, so the ID identifies them while its code runs; the
inline call also checks that the cached variant is still its function's variant.  The source Program that released a
class (`qore_class_private::released_spgm_id`) is recorded by its ID for the same reason.
`examples/test/ir/ClosureCacheProgramReuse.qtest` creates Programs one after another, each with a closure computing
another operation, in every execution mode.  Whether a deleted Program's addresses are given to the next one
depends on the allocator and the layout of the process's heap, so the test runs its driver several times; macOS's
allocator gave them in most runs, glibc's in none, so the test declares macOS as a platform it must run on.  A library
with debugging counts the calls that found an entry of a deleted Program at the same addresses
(`dbg_closure_call_cache_program_misses()`), and the test fails on macOS if no run was given the addresses, so that it
cannot stop reproducing the reuse unnoticed.

A cache keyed by the metadata of a class can identify the class by its instance serial instead
(`qore_class_private::instance_serial`, which no other class ever has), or use the method dispatch epoch
(`qore_get_method_dispatch_epoch()`), which the destruction of a class or a hashdecl increments before the memory can be
reused (`qore_bump_method_dispatch_epoch()`), as do the methods added at parse time.  The type and hashdecl path caches
of an AOT module (`qore_rt_resolve_full_type_path_cached()` and `qore_rt_resolve_hashdecl_path_cached()` in
`lib/JITRuntime.cpp`: their thread-local front caches and `QoreAOTContext::type_path_cache` and `hashdecl_path_cache`)
are keyed by the receiver's type, which is freed with its class, while the module's context lives as long as the module.
A subclass of the module's generic class declared by a later Program at the deleted subclass's address was given the
type found for the deleted one (`hash<string, int>` for a `Box<string>`) or a type of a freed hashdecl.  The entries of
the contexts' maps are keyed by the receiver's type and the serial of its class
(`qore_rt_aot_path_cache_receiver_serial()`): a class's own type is identified by the serial of the class, and a
parameterized class type, which is never freed (a type built on a destroyed one is retired by
`qore_purge_derived_types()`), by its address and the serial of its base class; the receiver is live, so its class is,
and the class keeps the types of its type arguments.  Nothing is cached for a receiver type of another kind, which is
not the type of an object or a class, nor for a context without a Program.  The thread-local front caches record the
epoch, which a call compares at no cost, and refill an entry of an earlier epoch from the map.  The maps' entries
recorded the epoch first, but each method added at parse time and each class destroyed cleared every context's maps, so
the module's generic code resolved the paths of all of its receivers again while another thread created and deleted
Programs (calls over 300 receivers took 48% longer than without the other thread, 10% longer with the serials).  A map
holds at most `QORE_AOT_PATH_CACHE_MAX_ENTRIES` entries, and evicts an entry for each new one once it is full: the
entries of destroyed classes are never found again, and clearing the full map made a working set larger than the map
resolve every path again.  `examples/test/ir/AOTPathCacheProgramReuse.qtest` declares such subclasses in Programs
created one after another, and more subclasses in one Program than a map holds.

## Thread-local variables

Each thread keeps its value of a `thread_local` variable in a map of its own (`ThreadData::tlvmap` in
`lib/thread.cpp`), which only the thread changes.  The key is the variable's serial (`Var::tl_serial`), which is never
reused: the map was keyed by the variable's address, and a variable of a Program created after another one was deleted
could have the address of a deleted variable and was given the value (with its type) that the deleted variable had in
the thread.  In the threads of a shared pool, one Program's code read another Program's values, across sandboxes.

The values of every thread are released with the variable, as the value of a global variable is: when its Program
clears its variables (`Var::clearLocal()`, while the Program can still run their destructors), or, for a variable that
another Program imports (a public variable inherited by a child Program), when the last Program holding it releases it
(`Var::del()`); the Program clearing an imported variable releases its own thread's value only, as the other threads
can still use it through the importing Program.  They were released only in the deleting thread, and every other
thread kept them until it ended, with objects of the deleted Program's classes.

A thread that ends, or whose values are cleared between tasks, releases its values itself, and enters the Program
owning each variable first, once for all of its values (`Var::tl_pgm`, the Program that parses or loads it,
identified by its ID; `ThreadData::releaseThreadLocalVarsOfProgram()` and
`qore_program_private::enterForThreadLocalRelease()`): the Program can be deleted in another thread at the same time,
and its teardown waits for the thread (step 1 or 3), so the Program can still run the destructors of the values,
which enter the Programs of their classes as nested calls.  A Program that it
cannot enter (its teardown has marked it, or it has been deleted) releases the variable itself, or has released it,
or, for a variable that another Program imports, the last Program holding it does: the thread hands the value over to
the registry (`ThreadLocalVarRegistry::Entry::orphans`), and the release of the variable releases it.  The values
were released in whichever state their Program was, often after another thread had deleted it, and their destructors
raised `PROGRAM-ERROR` (up to 4% of the values of pool workers clearing their values while Programs were deleted).
The owner of an imported variable that is deleted first releases the value of the deleting thread only; the values of
the other threads, also those handed over by threads that ended or cleared their values since, are released with
the last Program importing the variable.

`ThreadLocalVarRegistry` records the value of every thread by the variable's serial (`release_thread_local_var()`).
No thread uses a variable that is released this way, so the releasing thread takes the value out of the other
thread's entry without changing that thread's map; the thread erases the empty entry the next time it looks up a
variable (`ThreadData::eraseDeletedThreadLocalVars()`).  A thread that ends, or whose values are cleared between tasks
(`clear_all_program_thread_local_data()`), takes its entries out of the registry before it releases their values
(`ThreadData::releaseThreadLocalVars()`).  The destructors that the release runs cannot assign the thread's
variables then (`thread_local_vars_finalizing()`: `ThreadData::finalizing` while the thread ends,
`ThreadData::tlv_clearing` while a ThreadPool or AsyncIoController worker clears the data of its task, for the whole
of `clear_all_program_thread_local_data()`), so a second pass finds no value: the clearing between tasks allowed it,
and a destructor giving a variable a value whose destructor gave it a value again kept the worker releasing values for
ever, and `ThreadPool::stopWait()` waiting for it.  The registry's lock is a leaf lock: no value is released under it.
`examples/test/qore/vars/thread-local-program-reuse.qtest` checks the release with the variable's Program in a worker
thread, reads in a worker thread and in the deleting thread with Programs created one after another, the release at
the end of a thread, an inherited variable, the clearing between the tasks of a ThreadPool worker and between the
callbacks of an AsyncIoController worker, an imported variable whose owner is deleted first, and the values that
threads ending and pool workers release while their Program is deleted (deterministic in its result: every value is
released while the Program can run its destructor).

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
