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

### The phases of a compile, and detaching it

The deletion of a Program cancels its queued work, and waits for the work in progress only while that work reads
Program data.  The compile thread takes the compile lock before it claims work from the queue, so a teardown never
waits for another thread's synchronous compile: work that waits for the lock is still queued, where it is cancelled.  The phase of the work in progress (`QoreJIT::bg_in_progress_phase`, under the queue lock) is:

|!Phase|!What the compile thread does|!The teardown of a Program of the work
|`Lowering`|lowers the Qore IR to LLVM IR, reading the IR, the types and the callees of the Programs|waits
|`Llvm`|optimizes the self-contained `llvm::Module`, generates its code and emits it (`LLJIT::addIRModule()`, `lookup()`)|detaches the work
|`Installing`|caches and installs the entry points in the variants (short; no LLVM code runs)|waits
|`Detached`|finishes the LLVM work of a detached compile and discards the result|does not wait

LLVM can take without bound (in CI a compile stayed in LLVM's block frequency analysis for more than 400 s), so the
LLVM phase reads nothing of a Program: `compileFunctionInternal()` and `compileFunctionBatchInternal()` read the
optimization level from the Program and copy the names of the functions before they end the lowering phase
(`QoreJIT::bgEndLowering()`); after it, they read only the module, the copied names and the `batch_only` flags of the
work's own `BatchCallee` copies.  The code embeds the addresses of Program objects (variants, IR instructions, deopt
counters) as constants, which is harmless for code that never runs.  The LLJIT has no definition generator, lazy
re-export or materialization unit of Qore's that reads Program data: the runtime helpers are absolute symbols, other
symbols come from LLJIT's process symbol generator, and the debugger support plugin reads only the object.

A waiter that finds the work in the LLVM phase detaches it under the queue lock (`detachInProgressLocked()`): it takes
the work's references to the variants and its private IR, sets the phase and `bg_in_progress_detached`, and returns
after releasing the references (`releaseDetachedRefs()`).  The references are released while its Program exists, as
the compile thread released them before it finished the work; the Programs of the work are listed in
`bg_detach_releases` meanwhile, and the teardown of another of them waits for the release, as the release can destroy
its variants; the references of cancelled queued work are released in the same way, before the waiter waits for
anything, so that two teardowns never wait for each other.  The waiter resets the root function's `jit_compile_state`
to 0, as for cancelled work: a root of a surviving Program (whose batch folded in a callee of the deleted one) is
submitted again when it is called.  Work that refers to a deleted Program is refused for good (state 2): the root's IR
still calls the callee of the deleted Program, so it would be refused, after its batch is lowered again, on every call.

When the LLVM phase ends, the compile thread decides under the queue lock whether to publish the result
(`bgBeginInstall()`): a detached compile discards it.  A background compile adds its module with a `ResourceTracker`
of its own, which a discarded compile removes (`bgDiscardModule()`), so no symbol of the module stays defined; the
symbols it defines are recorded (`defined_batch_symbols`, `defined_fast_entry_abi`) and its entry points cached
(`compiled_functions`) only after the decision, so no variant is marked compiled with an entry of a discarded module,
and the functions can be compiled again under the same names.  The compile thread checks `bg_in_progress_detached`
between the steps of the LLVM phase to skip the steps that remain, and the optimizer skips its optional passes once it
is set (`registerShouldRunOptionalPassCallback()`); a pass that is running, or LLVM's code generation, is not
interrupted.  `bg_active_work` counts a detached compile until the compile thread finishes it, so
`QoreJIT::waitForBgCompileQueue()` without a Program waits for it.  A synchronous compile (top-level code in the jit
mode, generic specializations) does not wait for a detached compile: it falls back to the IR tier when the compile
lock is held by one (`lockForSyncCompile()`), as it does once the compiler stops.

Process exit detaches the work in the LLVM phase in the same way (`QoreJIT::stopBackgroundCompiler()`, called by
`qore_cleanup()` and `qore_exit_process()`), and does not join the compile thread then (`bg_thread_abandoned`): the
thread ends when LLVM returns, without touching the JIT again.  `QoreJIT::shutdown()` leaves the LLJIT to the
operating system then, as its `ExecutionSession` cannot be destroyed under an active compile; the compile thread holds
the compile lock, so a synchronous compile is refused instead of waiting for it (`lockForSyncCompile()`).

`exit()` must not run while the thread is in LLVM: it destroys LLVM's static objects (its `cl::opt` options and pass
registries, which code generation reads), and the compile crashes in them.  So the process ends with `_exit()`, after
flushing the standard I/O streams, if the thread still runs at the end: `qore_fork_child_quick_exit()`, which the
`qore` and `qr` programs, `qore_exit_process()`, the executables compiled with `qcc` and embedding programs call at
the end, asks `QoreJIT::exitQuickly()`.  If the thread has left its loop (`bg_thread_done`), it is joined there and the
process exits as usual.  A process that exits without calling it (an embedding program that returns from `main()`)
waits for the thread in the library's `atexit()` handler (`qore_jit_shutdown_at_exit()`), as before; that protects
the static objects of LLVM registered before the handler (its global options), but not the function-local statics LLVM
creates during its first compile, so such a process should call it.  Exit-time handlers (module `atexit()` handlers,
static destructors, coverage and sanitizer reports) are skipped only when a compile is still running in LLVM at the
very end of the process.

`examples/test/ir/BgCompileTeardownDetach.qtest` tests it with the debug build's holds of the compile thread
(`dbg_hold_bg_compile()` at the start of the lowering, at the start of the LLVM phase, at its end, or before the
result is installed, with
`dbg_wait_bg_compile_held()`, `dbg_continue_bg_compile()` and `dbg_wait_bg_compile_teardown_waiting()`) and the count
of discarded compiles (`dbg_get_bg_compile_discarded_count()`).

## Caches that outlive their objects

A cache that is not deleted with the objects it holds (a `thread_local` or process-wide cache) must not identify
them by their addresses, nor by the ID of their Program: the allocator gives the memory of freed functions, variants
and IR to the objects created next, so an object created later can have every address that the cache holds, and code
is freed while its Program lives as well.  An Expression parses its code into its Program, and deleting the
Expression frees that code with the closures in it (`qore_program_private::deleteExpression()`;
`~QoreClosureParseNode()` deletes the `UserClosureFunction`), so the closure of the next Expression gets the
addresses of the freed one in the same Program, on every platform.  A deleted Program frees its code too, and macOS
gives its memory to a Program parsed right after it at once.

Such a cache is keyed by an identity that is never reused: the serial of a variant (`UserVariantBase::serial`) or of
a closure function (`UserClosureFunction::serial`), both from one 64-bit counter (`UserVariantBase::getNextSerial()`).
The IR of a variant is published once and never replaced (`UserVariantBase::setCachedIR()`), and a closure function
has one variant, so a variant or a closure function with the serial cached, which the closure called holds, has the
variant and the IR cached.  The serial makes the comparisons of addresses and of the Program ID redundant, so a call
compares no more than it did before:

- the IR interpreter's cache of the last closure called inline (`tryExecuteInterpreterInlineIRClosure()`) compares
  the serial of the closure function, which the call has at hand, and the caller's Program, as a closure runs inline
  only in code of its own Program; the Program cached is that of the live variant, so it cannot be another Program at
  its address;
- the cache of the last closure that native code ran as a leaf (`tryExecClosureNativeLeaf()`) compares the serial of
  the variant, which the call has at hand, and the number of arguments.

Before the serial, the caches were keyed by addresses: a closure with the addresses of a freed closure ran the freed
IR, or the IR and the signature of the function that had been given that memory: a wrong result, a
`RUNTIME-TYPE-ERROR` for a parameter of another function, an `IR-EXEC-ERROR` or a crash.
`examples/test/ir/ClosureCacheExpressionReuse.qtest` tests closures of deleted Expressions, which reuse the memory
in every run; `examples/test/ir/ClosureCacheProgramReuse.qtest` and `examples/test/ir/ClosureCacheProgramRounds.qtest` test
closures of deleted Programs, which reuse it only when the allocator does: the first runs its driver several times
in each execution mode, the second creates many Programs and starts each round of them with another heap layout.
macOS's allocator gave the addresses in most runs, glibc's in none, so both declare macOS as a platform they must run
on.  A library with debugging counts the calls that found an entry made for a freed closure at the same addresses
(`dbg_closure_call_cache_program_misses()`), and `ClosureCacheProgramReuse.qtest` fails on macOS if no run was given
the addresses, so that it cannot stop reproducing the reuse unnoticed.  The source Program that released a class
(`qore_class_private::released_spgm_id`) is recorded by its ID, which is never reused, for the same reason.

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
holds at most `QORE_AOT_PATH_CACHE_MAX_ENTRIES` entries, and evicts its oldest entry for each new one once it is full
(`QoreAOTContext::hashdecl_path_cache_order` and `type_path_cache_order`, a ring of pointers to the keys in the map's
nodes): the entries of destroyed classes are never found again; clearing the full map made a working set larger than the
map resolve every path again, and evicting the entry that the map iterates first evicted the newest ones, so a live
working set in a map full of dead entries found its entries in about 40% of its calls (99% first in, first out).
`examples/test/ir/AOTPathCacheProgramReuse.qtest` declares such subclasses in Programs created one after another, and
more subclasses in one Program than a map holds.

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

A thread that ends, or whose values are cleared between tasks, releases its values itself.  It enters the Program owning
each variable first, once for all of its values (`Var::tl_pgm`, the Program that parses or loads it, identified by its
ID; `ThreadData::releaseThreadLocalVarsOfProgram()` and `qore_program_private::enterWithoutContext()`), and takes the
values out of the registry: the Program can be deleted in another thread at the same time, and its teardown waits for
the thread (step 1 or 3).  The thread releases the values whose release runs code of the Program while it is in the
Program (`ThreadData::tlvReleaseRunsCodeOf()`: an object of a class of the Program or created in it, a closure of the
Program, and lists and hashes holding such values), so that the Program can still run their destructors, which enter the
Programs of their classes as nested calls; it leaves the Program before it releases the others, as the teardown must not
wait for code of other Programs, which can wait for the thread deleting the Program (a lock that it holds: the teardown
waited for ever).  The members of an object of another class are not checked: their release follows the destructor of
the object, and a destructor of a member of a class of the Program enters it itself, which raises `PROGRAM-ERROR` if the
teardown has marked the Program by then.  A Program that the thread cannot enter (its teardown has marked it, or it has
been deleted) releases the variable itself, or has released it, or, for a variable that another Program imports, the
last Program holding it does: the thread hands the value over to the registry
(`ThreadLocalVarRegistry::Entry::orphans`), and the release of the variable releases it.  The values were released in
whichever state their Program was, often after another thread had deleted it, and their destructors raised
`PROGRAM-ERROR` (up to 4% of the values of pool workers clearing their values while Programs were deleted).  The owner
of an imported variable that is deleted first releases the value of the deleting thread only; the values of the other
threads, also those handed over by threads that ended or cleared their values since, are released with the last Program
importing the variable.

`ThreadLocalVarRegistry` records the value of every thread by the variable's serial (`release_thread_local_var()`).
No thread uses a variable that is released this way, so the releasing thread takes the value out of the other
thread's entry without changing that thread's map; the thread erases the empty entry the next time it looks up a
variable (`ThreadData::eraseDeletedThreadLocalVars()`).  A thread that ends, or whose values are cleared between tasks
(`clear_all_program_thread_local_data()`), takes its entries out of the registry before it releases their values
(`ThreadData::releaseThreadLocalVars()`).  The destructors that the release runs cannot assign the thread's
variables then (`thread_local_vars_finalizing()`: `ThreadData::finalizing` while the thread ends,
`ThreadData::tlv_clearing` while a ThreadPool or AsyncIoController worker releases the values of its task in
`clear_all_program_thread_local_data()`, which releases the values again after clearing its other data: the code
clearing the thread resources and the thread-local data of the Programs can assign the variables), so a second pass
finds no value: the clearing between tasks allowed it,
and a destructor giving a variable a value whose destructor gave it a value again kept the worker releasing values for
ever, and `ThreadPool::stopWait()` waiting for it.  The registry's lock is a leaf lock: no value is released under it.
`examples/test/qore/vars/thread-local-program-reuse.qtest` checks the release with the variable's Program in a worker
thread, reads in a worker thread and in the deleting thread with Programs created one after another, the release at
the end of a thread, an inherited variable, the clearing between the tasks of a ThreadPool worker and between the
callbacks of an AsyncIoController worker, an imported variable whose owner is deleted first, and the values that
threads ending and pool workers release while their Program is deleted (deterministic in its result: every value is
released while the Program can run its destructor).

## References to a Program's lvalues

A reference (`lvalue_ref` in `include/qore/intern/lvalue_ref.h`) names the Program of its lvalue (`lvalue_ref::pgm`),
and its lvalue expression names variables of that Program, which are freed with it; a function of the Program can return
a reference to one of its variables, which outlives the Program.  Using the reference entered the freed Program
(`ProgramThreadCountContextHelper::set()` crashed) or, for a reference held by a variable, resolved the freed variable.
The reference now holds a dependency reference to its Program (`QoreProgram::depRef()`), which keeps the Program
allocated, and its address from being given to another Program, while the reference exists, without a global lock on its
use; a Program refuses an entry once its teardown has marked it (`qore_program_private::canEnterIntern()`).  The thread
stays in the Program until the lvalue has been used: the teardown released a variable while another thread assigned it
through a reference (a data race that TSan reported).  The `LValueHelper` keeps the entries
(`QoreLValueReferencePrograms`, which leaves them if the helper's construction throws) and leaves them in its destructor
right after the lvalue's locks are released and the write-back of a static variable, before member write gates and
accessors, the recursive-reference scan and the release of the values that the write replaced, which run code of other
Programs and of modules (a jni or python callback taking its lock while another thread deletes the Program would wait
for ever).  A reference held by a local or closure-bound variable, or a parameter, is resolved with a
`RuntimeReferenceHelper` (`LocalRefHelper`), which enters the Program with its context; the helper leaves it when it is
destroyed, before the caller uses the lvalue, so it hands its thread count over to the `LValueHelper`
(`ProgramThreadCountContextHelper::takeThreadCount()`, `RuntimeReferenceHelperBase::takeProgramEntry()`,
`LValueHelper::adoptReferenceProgram()`), as does `LValueHelper(const ReferenceNode&)`.  Entering the Program a second
time instead made 4 threads using references held by local variables 66 to 88% slower.  A reference held by a global
variable or an object member is resolved without a helper: the `LValueHelper` enters the Program without switching to it
(`LValueHelper::keepReferenceProgram()`, `QoreReferenceProgramHelper`, `qore_program_private::enterWithoutContext()`,
which a parse in progress does not refuse, as no code of the Program runs).  A reference to a lvalue of the current
Program, the usual case, enters nothing: it costs the comparison of the Programs
(`ProgramThreadCountContextHelper::set()`, whose failure `RuntimeReferenceHelperBase` reads from the helper instead of
the exception sink, or `qore_reference_program_needs_entry()`) and the test in `QoreLValueReferencePrograms::release()`,
which has no destructor of its own (the `LValueHelper` constructors that can enter a Program leave it when they throw);
a release build executes no more instructions per use than before the fix.  Across Programs, the thread's data is looked
up once per use: `qore_reference_program_needs_entry()` compares the current Program, searches the Programs that the
thread deletes only while it deletes one (`ThreadData::pgm_teardown_depth`), and the entry records the TID for its exit.
`examples/test/qore/vars/reference-program-entry.qtest` holds a thread after an assignment through a reference held by a
local variable, a closure-bound variable and a parameter, and checks that it is still in the Program; the debug build's
`dbg_hold_reference_lvalue()` holds it.  The removal through a reference held by a local or closure-bound variable
enters it while it removes the value.  A reference to a lvalue of a Program whose teardown has marked it, or has ended,
raises `PROGRAM-ERROR`.  It recorded the Program's ID and took a reference to it by ID for each use, which took the
global lock of the Program registry and made a loop using references across Programs 10 to 24% slower.
`examples/test/qore/vars/reference-deleted-program.qtest` uses references to a thread_local, a global and a
closure-bound local variable of a deleted Program through local, closure-bound and global variables, and writes through
a global variable holding a reference into a Program with a parse pending, in every execution mode.

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
