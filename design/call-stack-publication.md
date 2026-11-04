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

### Frames of native calls

Native code (JIT and AOT) pushes the frame of each call of user code that it makes directly, without a runtime
helper: `QoreNativeCallStackLocation` (`lib/JITRuntime.cpp`), constructed in storage of the caller's native frame by
`qore_rt_push_native_call_frame*()` and destroyed by `qore_rt_pop_native_call_frame()`. Its constructor pushes in its
body, after its members are initialized, and `push_native_call_stack_location()` (`lib/thread.cpp`) writes the values
it reports (the call's location, the caller's statement and program) before it publishes it; its destructor pops in
its body. The caller gets the thread's data once, when it is entered (`qore_rt_get_native_entry_slots()`), so a push
and a pop make no thread-local lookup.

The frame's location is the location of the call. The frame saves the caller's runtime location, which the callee
changes, and restores it when it is popped, so a line after a call that returned reports the caller's location. A
call that AOT code runs inline (a summary of the callee's body) pushes the frame too, with
`qore_rt_push_native_inline_call_frame_aot()`, which makes the location of the callee's body, held by the callee's
image, the runtime location until the pop; a summary proven not to raise an exception has no frame, as nothing can
see it.
The inlined body of a scalar subclass override (the bounded override alternatives of a method call site) keeps its
frame even then: the ordinary call of the override publishes its frame (a compact native-method frame), and readers on
other threads see it (`AOTOverrideFrameReaders.qr`). An inlined exact-class method body without exceptions still has no
frame.

AOT code keeps its runtime location as JIT code does: it stores the address of a constant `QoreProgramLocation` of its
image when the line changes (and at the start of every block), so every thread's stack, its own current line
included, can be read at any time; an exception takes its location and its call stack from them as they are.

### Compact native-method frames

The same-Program unary-integer AOT continuation uses
`QoreCompactNativeMethodStackLocation` after the complete live PIC admission.
It borrows the immutable admitted cache entry instead of copying three metadata
pointers into each frame. The entry owns the call name and identifies the
execution Program and user variant; the variant's statement block is immutable
for this lifetime. Source-stripped entry/debug statements are separate and do
not replace that block. The caller's AOT context owns the entry until after the
call returns, including entries retired from PIC slots.

The frame still stores the caller location and publishes on every call, even
when the compiler has proven the body pure. A concurrent stack reader can
observe that call. Its publishing helper is the last member: construction
initializes the borrowed entry and location before publishing, and destruction
unpublishes and waits for readers before either becomes invalid. Deferred frame
helpers save the caller's statement and location eagerly, install the immutable
callee body location, and restore both on every exit, including successful calls
that never materialize object/class context and calls with no parse location.
Cold overflow materialization does not overwrite the saved caller location.
Eager/native-v1 calls keep their existing frame. Receiver and closure bodies
obtain location slots through the explicit RuntimeConfig helper.

`AOTNativeMethodEntry.qtest` and `AOTNameMethodContinuation.qtest` exercise the
default frame and the eager control selected at runtime by
`QORE_DISABLE_AOT_NATIVE_METHOD_DEFERRED_CONTEXT=1`, including concurrent readers
and debugger behavior. The compact-frame experiment is qualified in release
and debug, not by a new sanitizer run; the earlier protocol validation below
must not be read as sanitizer qualification of this change.

### Known limitation: external module frames

`QoreExternalRuntimeStackLocationHelper` (public API, subclassed by binary modules such as jni and python) pushes
the location in its own constructor and pops it in its destructor, i.e. before the module's derived class is
constructed and after it has been destroyed. A walk that reaches such a frame in either window reads a partially
constructed or destroyed object. Fixing it needs an API addition that lets the module push after its constructor has
run and pop before its destructor runs (the class is part of the binary ABI, so the base class cannot simply stop
pushing), plus the corresponding change in each module.

For the same reason, no virtual function or data member may be added to `QoreStackLocation` or to any public class
derived from it (`QoreExternalStackLocation` and its subclasses): a module compiled against the previous header
would have no vtable slot or member at the new position, and the library calling or reading it would crash.

## What a walk observes

A walk reads the stack as it is at the walker's load of `current_stack_location`, and a thread that pops a location
during the walk waits until the walk has finished, with the restored location already published.  With the heavy
barrier, the walker's flag store and its load are separated by the `membarrier()` system call, which takes far longer
than a short call: every thread that keeps calling reaches its next pop in that time, sees the flag and waits, so the
walk observes each such thread just after a pop, never inside a short leaf frame.  Without the heavy barrier (macOS,
or `membarrier` unavailable) the load follows the store at once, and a walk samples a running thread at a random
point.  Both are correct snapshots; a frame is published for the whole of its call either way.  A concurrent reader
therefore cannot count on observing a leaf frame: on Linux the native fast paths (whose leaf frames last a few
nanoseconds) were seen in no sample at all.

Tests that must observe a frame use `dbg_read_stacks_at_frame()` (DEBUG builds only, `QDOM_DEBUG_HOOK`): the first
other thread that publishes a location with one of the requested call names (and, optionally, called by a frame with
one of the requested names) parks right after publishing it, and the caller of the function reads every thread's stack
while it is parked; the result reports which kind of call published the frame (`native-leaf`, `native` or
`ordinary`).  The check is made in the publishing functions of `lib/thread.cpp` behind one relaxed load of a flag, in
DEBUG builds only.  It is not made for the location of an external module (`update_get_runtime_external_stack_location()`,
see the known limitation above), whose virtual methods cannot be called when it is pushed.
`examples/test/ir/AOT*FrameReaders.qr` and `concurrent-call-stacks.qtest` use it.

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
