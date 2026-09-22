# Call stack publication

Every function and method call pushes a location on the calling thread's call stack and pops it again on return.
The stack is a linked list of `QoreStackLocation` objects that live in the C++ frames of the calls that pushed them,
and `ThreadData::current_stack_location` points to the innermost one. The thread reads its own stack for exception
call stacks and caller-sensitive APIs; other threads read it only through `get_all_thread_call_stacks()`
(`QoreThreadList::getAllCallStacks()`), which the debugger (`DebugProgramControl`) uses.

## Cost model

Pushes and pops happen on every call, on every thread; walks of another thread's stack are rare. The protocol keeps
the per-call side as cheap as possible and puts the cost of any synchronization on the walker.

Until 2026 every push and pop took a **process-wide** read lock (`QoreThreadList::stack_lck`, a `QoreRWLock`) so that
a walker could take the write lock and freeze every thread. That was two contended lock operations on one mutex
shared by every thread, on every call. Replacing it measured +7% single-threaded, +19% at 8 threads and +41% at
16 threads on a call-heavy benchmark with no shared data (interleaved A/B, median of 5, 32-core machine).

## The protocol

State, per thread (`ThreadData` in `lib/thread.cpp`):

- `current_stack_location` - `std::atomic<const QoreStackLocation*>`, written only by the owning thread.
- `stack_walked` - `std::atomic_bool`, set by a walker for the duration of a walk.
- `stack_walk_lck` - held by a walker for the whole walk; serializes walks of one stack, and is what the owner waits
  on if it pops during a walk.

**Push** (`publish_pushed_stack_location()`): link the new location to the current one, then store it with release
ordering. No handshake: a walker either starts below the new location or sees it fully linked, and a location a
walker can reach is only freed by a pop.

**Pop** (`publish_popped_stack_location()`): store the restored location, then load `stack_walked`; if a walk is in
progress, take and release `stack_walk_lck`, which returns once the walk has finished. This is one half of a Dekker
handshake.

**Walk** (`QoreThreadList::walkCallStack()`): take `stack_walk_lck`, store `stack_walked`, load
`current_stack_location`, walk, clear `stack_walked`, release the lock. The other half of the handshake.

A full memory barrier must separate the store from the load on both sides. Then either the popping thread sees the
walker's flag and waits for the walk to end, or the walker's load comes after the pop's store and it starts from the
restored location, never reaching the one that is about to be destroyed. Any pop after the walker's store sees the
flag, so no location reachable from the walker's snapshot is destroyed during the walk.

### Asymmetric barrier

Where the operating system supports it, the walker issues the barrier for every thread:
`membarrier(MEMBARRIER_CMD_PRIVATE_EXPEDITED)` on Linux, registered once in `init_stack_walk_barrier()` (called from
`init_qore_threads()` before any other thread exists). A pop then needs only a compiler fence between its release
store and its load of the flag - on x86-64, two plain moves with no locked instruction. The load must still be an
**acquire**: when it reads the value that a finished walk cleared, it is what orders this thread's reuse of the
popped location's memory after that walk's reads of it. A relaxed load (the first version) lets a weakly ordered
processor perform the following stores before the load, while the walk may still be reading; ThreadSanitizer
reported exactly that. Acquire costs nothing extra on x86-64. Where `membarrier` is not
available (other operating systems, old kernels, or a seccomp profile that denies it) pops use sequentially
consistent operations instead. `stack_walk_heavy_barrier` records the choice; it is set once and never changes,
because a pop relying on the walker's barrier while a walk went without one would be unsafe.

The walker's own thread reads its stack directly: it cannot pop a location while it is in `getAllCallStacks()`.

The handshake protects every location reachable from the walker's snapshot only because the stack changes strictly
last-in, first-out: a location is linked to the one that was current when it was pushed, and popping it restores
exactly that one. Debug builds assert this on every restore (`publish_popped_stack_location()`).

## Frames must be complete while they are published

Other threads can read a location from the moment it is pushed until it is popped, so it must be fully constructed
before the push and fully intact until the pop. The helper that pushes and pops a location,
`QoreProgramStackLocationHelper`, therefore has to be the **last data member** of the location's class
(`QoreInternalCallStackLocationHelper`, `QoreIRInlineCallStackLocation`, `QoreJITStackLocation`): members are
constructed in declaration order after the class's vtable is in place, and destroyed in reverse order, so the helper
pushes the location after every member its virtual methods read has been initialized and pops it before any of them
is destroyed. As a base class it pushed the location before the derived class's members were constructed, while
virtual calls still resolved to the abstract `QoreStackLocation` (a walker could hit a pure virtual call or copy a
half-constructed `std::string`), and popped it only after those members had been destroyed. ThreadSanitizer found
both with `examples/test/qore/stack/concurrent-call-stacks.qtest`.

Values a location reports must not be assigned after the push either: `CodeEvaluationHelper` passes the program it
records for a cross-program call to `update_get_runtime_stack_location()` (`frame_pgm`) instead of overwriting `pgm`
afterwards, and `QoreJITStackLocation::getProgram()` chooses between its explicit and saved program when called.

`CodeEvaluationHelper` pushes from `init()`, called in its constructor body, and pops in its destructor body, so it
satisfies the rule without the member helper.

### Known limitation: external module frames

`QoreExternalRuntimeStackLocationHelper` (public API, subclassed by binary modules such as jni and python) pushes
the location in its own constructor and pops it in its destructor, i.e. before the module's derived class is
constructed and after it has been destroyed. A walk that reaches such a frame in either window reads a partially
constructed or destroyed object. Fixing it needs an API addition that lets the module push after its constructor has
run and pop before its destructor runs (the class is part of the binary ABI, so the base class cannot simply stop
pushing), plus the corresponding change in each module.

## Thread teardown

A walker holds the thread list lock (`QoreThreadList::lck`) while it walks, and that is what keeps each listed
thread's `ThreadData` in place. `QoreThreadList::deleteData()` and its variants therefore take the thread out of the
list under `lck` before deleting its data; they used to delete it first, which left `getAllCallStacks()` a window in
which it read freed memory.

## Regression coverage

- `examples/test/qore/stack/concurrent-call-stacks.qtest` - a parked thread's stack read from another thread, and
  all stacks read while threads call functions and while threads are created and exit.
- `examples/test/qore/stack/get-call-stack.qtest` - a thread's own stack.

The protocol was validated with ThreadSanitizer (a `-fsanitize=thread` build of the `libqore` and `qore` targets,
run on a module-free script doing the same as `concurrent-call-stacks.qtest`) in both barrier modes. The fallback
mode can be forced without a build option by making `membarrier` fail, e.g.
`strace -f -e trace=membarrier -e inject=membarrier:error=ENOSYS qore ...`; the syscall must be in the traced set
(`-e trace=none` silently disables the injection).
