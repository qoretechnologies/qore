# Fork handlers: process-wide locks in a child created with fork()

Copyright (C) 2026 Qore Technologies, s.r.o.

A child process created with `fork()` runs only the thread that called `fork()`. Every lock that another thread
held at that moment stays locked in the child, and every condition variable still counts the waiters of the
parent. Before these handlers, a Qore `fork()` made while another thread started or exited waited forever in the
child: `QoreSignalManager::postFork()` restarts the signal thread, which needs the thread list's lock, and threads
hold that lock when they start and exit.

The library therefore registers its process-wide locks with fork handlers (`include/qore/intern/QoreForkHandlers.h`,
`lib/QoreForkHandlers.cpp`). One set of `pthread_atfork()` handlers runs them for every `fork()` in the process,
whoever calls it: Qore's `fork()`, the `fork()` fallbacks of `system()` and the backquote operator, and native code
in modules or the embedding program. `posix_spawn()` does not run them (glibc, musl and macOS implement it without
the fork handlers), so `system()` and the backquote operator are not affected.

## How a handler works

Each handler belongs to one rank (the lock order below) and one mode:

| Mode | Before `fork()` | In the parent | In the child |
|---|---|---|---|
| `QFLM_ACQUIRE` | acquires the lock (waits for its holder) | releases it | initializes it again (it is held by the forking thread) |
| `QFLM_TRY` | acquires the lock only if it is free | releases it if acquired | initializes it again; the owner learns from `wasAcquired()` whether the protected data is consistent |
| `QFLM_RESET` | nothing | nothing | initializes it again; the owner repairs or abandons the protected data |

- The prepare handlers run in increasing rank order with the registry lock held; the parent and child handlers run
  in decreasing rank order.
- `ACQUIRE` is for locks whose critical sections are short and never block on another thread: the forking thread
  waits for them, and the data they protect is consistent when the process is copied.
- `TRY` and `RESET` are for locks that can be held for a long time or across operations that wait for other threads
  (module loading, LLVM compilation, running compiled initializers): waiting for them before `fork()` could
  deadlock the parent, which before these handlers could always fork.
- Conditions bound to a lock are initialized again in the child too: their waiters do not exist there, and a glibc
  condition variable with waiters that never return can block a later signal or broadcast.
- A lock is initialized again in the child rather than unlocked: a recursive mutex held by the forking thread
  cannot be unlocked there on glibc (its owner is the parent's thread ID). It is first reset by assigning its static
  initializer, since a held `pthread_rwlock_t` cannot be initialized again on macOS (`EBUSY`); the environment lock
  is only reset that way.

## Lock order

A thread holding a lock of a given rank may only acquire locks of a higher rank (or locks that are not registered).
Locks of the same rank are never held together. The prepare handlers acquire in increasing rank order, so they
cannot deadlock with a thread that keeps to the order.

| Rank | Lock | Mode | May acquire while held |
|---|---|---|---|
| 0 | the handler registry | (held by the handlers) | — |
| 40 | AOT lazy constant initialization lock | RESET | anything (runs compiled initializers) |
| 50 | module manager (`QMM.mutex`) | RESET | anything (held across `dlopen()`) |
| 60 | module locks held across user code or input (`QFR_MODULE_RESET`: linenoise) | RESET | anything |
| 100 | signal manager (`QSM.mutex`, with `QSM.tcount`) | ACQUIRE | thread list, signal thread counter |
| 150 | JIT compile lock (`QoreJIT::compile_mutex`) | TRY | JIT caches, JIT queue, type caches, environment |
| 200 | async I/O controller singleton | ACQUIRE | Program cleanup callbacks |
| 300 | AOT module state | ACQUIRE | condition waker, thread list |
| 310 | plugin registry | ACQUIRE | thread list (cancellation checks), environment |
| 320 | time zone manager (`QTZM`) | ACQUIRE | thread list (cancellation checks) |
| 400 | thread counters (`thread_counter`, `tp_thread_counter`) | ACQUIRE | thread list, condition waker (cancellable waits) |
| 450 | module locks whose holders check for cancellation (`QFR_MODULE_CANCELLABLE`: the sshutil registry) | ACQUIRE | thread list, leaf locks |
| 500 | thread list (`thread_list.lck`) | ACQUIRE | condition waker, stack lock |
| 510 | condition waker (`cond_waker.lck`) | ACQUIRE | — |
| 520 | stack lock (`stack_lck`) | ACQUIRE | — (only `pthread_create()`) |
| 1000 | leaf locks: caches and registries (among them the JIT caches and queue, the AOT pending constants, the AOT regex cache, the content hashes of the sources of linked AOT modules, the DBI object registry, and the QUIC idle-close test hook of debug builds), and the leaf locks of modules (`QFR_MODULE_LEAF`: the i18n locale data fallbacks, the logger_bin event start time and id sequence, the ml ONNX provider list) | ACQUIRE | environment, debug output, allocation tracking |
| 1100 | LLVM target initialization (`qore_llvm_initialize_targets()`): held while LLVM registers its targets, once per process | ACQUIRE | — |
| 1500 | environment (`SystemEnvironment`) | ACQUIRE | debug output, allocation tracking |
| 1900 | debug output (`printd()`) | ACQUIRE | allocation tracking |
| 1950 | allocation tracking (only with `ENABLE_ALLOC_TRACKING`) | ACQUIRE | — |

The environment lock ranks near the top: its holders only copy variables and change the environment, so it is a
leaf, and every lock that reads the environment while held (there are hundreds of such reads) ranks below it. Code
that creates processes or calls libraries that read the environment holds another lock, which the handlers do not
acquire (see "The environment" below).

The striped leaf lock arrays (the 64 constant value locks and the 64 opaque target shard locks) are acquired like
other leaf locks, except with ThreadSanitizer: its deadlock detector handles at most 128 locks held by one thread,
which the stripes alone would reach, so TSan builds only initialize them again in the child (`QFLM_STRIPED`).

### Rules

- A handler is registered when its object is constructed (static initialization, or the first use of a
  function-local static) and must not be registered while another registered lock is held.
- A thread must not call `fork()` while it holds a registered lock that is acquired before `fork()`: it would wait
  for itself. Holding the environment lock across `fork()` (inside an `AtomicEnvironmentSetter`) does not make the
  forking thread wait for itself, but the handlers would then acquire every other lock while it is held, and a
  thread holding one of them while it waits for the environment lock would deadlock with the forking thread, so it
  is not allowed either (an assertion in debug builds; see `SystemEnvironment`). Process creation with `fork()`
  needs no hold, since the handlers hold the lock while the process is copied; `posix_spawn()` runs no handlers, so
  its callers hold a `QoreEnvironmentReadLock` (the environment change lock, which `fork()` does not wait for).
- No lock that the prepare handlers wait for (`ACQUIRE`, the registry, and the locks of modules registered with
  `qore_register_fork_handler()` in that mode) may be held across a call that can wait for the lock of the dynamic
  linker: `dlopen()`, `dlclose()`, and every library function that can call them. On macOS, `fork()` takes that lock
  before it runs the `pthread_atfork()` handlers (see "The dynamic linker" below), so such a hold deadlocks with any
  `fork()` in the process, made by any code.
- No user code runs while a registered `ACQUIRE` lock is held. The thread list lock used to be held while a
  Program's thread initialization code ran (`QoreThreadList::activate()`); that code now runs after the lock is
  released.
- A `RESET` lock that can be held across `fork()` by the forking thread itself must count its holds per thread, so
  that the child can take it again as often (the AOT lazy constant initialization lock runs compiled initializers,
  which can call `fork()`).
- A recursive lock must not be created during static initialization: the recursive mutex attributes are only
  initialized by `qore_init()`, and a `QoreRecursiveThreadLock` created before is not recursive. Locks that a handler
  needs before their first use are created in `qore_init()` (`qore_aot_runtime_init_once()`).

### The dynamic linker

On macOS (checked on macOS 26, arm64), `fork()` calls `libSystem_atfork_prepare()`, which takes the lock of the
dynamic linker before it runs the `pthread_atfork()` prepare handlers, and releases it after the parent and child
handlers:

```
libSystem_atfork_prepare:
    bl _dyld_dlopen_atfork_prepare        ; dyld4::RuntimeLocks::takeDlopenLockBeforeFork()
    bl _pthread_atfork_prepare_handlers   ; the handlers registered with pthread_atfork(): the library's
    bl _libSC_info_fork_prepare
    ...
    bl _dyld_atfork_prepare               ; the other locks of the dynamic linker
    bl _malloc_fork_prepare
    ...
```

`dlclose()` takes the same lock (`dyld4::APIs::dlclose()` calls `takeDlopenLockBeforeFork()`), and so does
`dlopen()`; `dlsym()` takes the loaders lock, which `_dyld_atfork_prepare()` takes after the handlers, so it is not
part of this order. Every lock that a prepare handler waits for therefore ranks after the lock of the dynamic linker
on macOS, and must not be held across `dlopen()` or `dlclose()`, including those that libraries make internally.
glibc and musl take their loader locks after the handlers (musl) or only reset them in the child (glibc), but the
rule holds on every platform, since the library's lock order is the same everywhere.

The module manager's lock is held across `dlopen()` and `dlclose()`; it is reset in the child and never acquired
before `fork()`. The other library code that calls `dlopen()` or `dlclose()` holds no registered lock.

### The environment

The process environment is protected by two read-write locks (`lib/SystemEnvironment.cpp`):

| Lock | Held shared | Held exclusively | Before `fork()` | In the child |
|---|---|---|---|---|
| environment lock (rank 1500) | to copy a variable: `SystemEnvironment::get()`, `isSet()`, `valueExists()`, `QoreEnvironmentValue` | to change the environment: `SystemEnvironment::set()`, `unset()`, `AtomicEnvironmentSetter` | acquired exclusively | reset |
| environment change lock (not ranked) | to use the environment with the C library directly: `QoreEnvironmentReadLock` (passing `environ` to `posix_spawn()` or `execvp()`, `popen()`, `system()`, `localtime_r()`, c-ares channel creation and the start of a c-ares lookup, which reads `HOSTALIASES`, OpenSSL's default CA paths, the ENV hash) | to change the environment, before the environment lock | not acquired | reset |

- A change takes the change lock and then the environment lock, both exclusively, so while the forking thread holds
  the environment lock, no other thread is changing the environment, and the child gets a consistent copy.
- The handler does not wait for the holders of the change lock: they run library code that can take a long time and
  can wait for other locks, among them the lock of the dynamic linker that the forking thread holds. c-ares calls
  `dlopen()` and `dlclose()` whenever it creates a channel on macOS (`ares_init_sysconfig_macos()`), and creating a
  channel while holding the environment lock deadlocked `qorus-core` with a thread calling `fork()` (the forking
  thread held the lock of the dynamic linker and the library's other locks, and waited for the environment lock;
  the thread creating the channel held the environment lock and waited in `dlclose()`; every other thread waited for
  one of the forking thread's locks).
- A holder of the change lock can call `fork()`; it cannot change the environment (`EDEADLK`).
- The change lock is always acquired before the environment lock (an assertion in debug builds). A change waits for
  the change lock, so it must not hold a lock that the prepare handlers wait for, nor one that a holder of the
  change lock can wait for: the environment is changed by Qore code (`setenv()`, `unsetenv()`), which runs with no
  such lock held, and by modules, which must keep to the same rule. In particular, a change must not be made in code
  that runs with the lock of the dynamic linker held (static initializers and destructors of a library that is being
  loaded or unloaded), since a holder of the change lock can wait for that lock.
- How this is enforced: the environment lock is only held inside `lib/SystemEnvironment.cpp`, around `getenv()`,
  `setenv()` and `unsetenv()` and the copies of the values, and by `AtomicEnvironmentSetter`, whose holders must only
  read and change the environment; everything else that reads the environment holds the change lock.
  `examples/test/qore/functions/fork_environment_lock_order.qtest` checks the c-ares channel creation with a
  stand-in for the lock of the dynamic linker (`dbg_fork_lock_cycle_arm()` in debug builds): a `pthread_atfork()`
  prepare handler registered after the library's handlers runs before them, as the dynamic linker takes its lock,
  and the channel creation waits for the stand-in in its hold, as c-ares waits in `dlclose()`. The debug hooks detect
  the cycle from the state of both threads (the forking thread, holding the stand-in, is about to wait for a lock
  that the channel creation holds) and make the channel creation fail without calling c-ares (on macOS, c-ares would
  then wait for the real lock of the dynamic linker), so the test fails instead of hanging, on every platform.

### Module locks

Modules register fork handlers for their process-wide locks with the exported `qore_register_fork_handler()`
(`include/qore/QoreForkHandler.h`, documented in the module guide) at a rank of `QoreForkRank`, in their
initialization function, and remove them in their delete function. The rank comes from what the lock's holders do:
the i18n, logger_bin and ml locks only protect containers (leaves); the holders of the sshutil registry lock check
for cancellation, which takes the thread list lock, and nothing is held when it is taken (450, between the thread
counters and the thread list); the linenoise lock is held while a thread waits for terminal input and while user
callbacks run, so it is only reset in the child, which takes it again as often as the forking thread held it
(counted per thread by `LinenoiseLockHelper`), since a callback can call `fork()`.

`pthread_atfork()` in each module would not keep the order: modules register after the library, so their prepare
handlers would run before all of the library's, and their handlers cannot be removed when a module is unloaded on
musl or macOS.

## Children that exit without exit-time handlers

A child created with `fork()` exits with `exit()`, which runs the `atexit()` handlers and C++ static destructors of
every library in the process. Third-party libraries have exit-time code that assumes the process that loaded them:
libnode's libuv joins its thread-pool threads (a hang in the child) and Node's static `SigintWatchdogHelper`
destroys a Mach semaphore that a macOS child does not have (an abort); nats.c's `natsLib_Destructor()` takes a lock
that a parent thread can hold; the Oracle Instant Client frees OpenSSL state under such locks. A module cannot change
that code, so it requests a quick exit from its child handler with `qore_fork_child_exit_quickly()`
(`include/qore/QoreForkHandler.h`).

- **The flag:** `qore_fork_quick_exit_generation` holds the fork generation (`qore_fork_generation`, incremented
  first in the child handler of the registry) in which a request was made, 0 for none. A request only counts while
  the registry runs the child handlers (`qore_fork_in_child_handlers`), so it is never in effect in the parent,
  where the generation is 0, and any number of modules can make it in handlers of any rank. A child that this
  child creates with `fork()` has the next generation, so it exits quickly only if a module requests it again in
  that `fork()`; `exec()` replaces the process memory, so a program it starts begins without the request.
  `qore_fork_child_exits_quickly()` compares the two generations; it takes no lock and allocates nothing.
- **The exits:** `qore_fork_child_quick_exit(rc)` flushes the standard I/O streams and calls `_exit(rc)` if the
  request is in effect, and returns otherwise.
  - `qore_main_intern()` calls it after `qore_cleanup()`, so programs, threads and module delete functions are torn
    down as in any process (an uncaught exception gives exit status 3 there).
  - `qore_exit_process()` (`exit()`, `GetOpt` errors) calls it where it called `exit(rc)`: in a single-threaded
    process, after stopping the signal thread, the JIT background compiler, the external thread reaper and the
    condition waker. No program is torn down there in any process. With other threads running (or from the signal
    thread, for a `SIGTERM` without a handler), it already ends with `_Exit(rc)`, without flushing.
  - The executables that `qcc` compiles call it after `qore_cleanup()` too: the runtime entry points of their
    generated `main()` (`qore_aot_run()`, `qore_aot_run_v2()` and `qore_aot_run_v3()` in `QoreAOTRuntime.cpp`) and the
    `main()` that `qcc` writes for executables linked from script objects (`qcc-main.cpp`).
  - Embedding programs that return from `main()` call it themselves after `qore_cleanup()`.
- **What is skipped:** every `atexit()` handler and C++ static destructor, of every library and module, including the
  library's own (`qore_jit_shutdown_at_exit()` is not needed: the background compiler is stopped before).

Tests: `examples/test/module-cpp-api/fork-exit/fork-exit.qtest` with the `forkexita` (an `atexit()` handler that
blocks forever in a child) and `forkexitb` (a static destructor that aborts in a child) test modules, whose child
handlers (at `QFR_MODULE_LEAF` and `QFR_MODULE_RESET`) request a quick exit twice each. The exit-time code writes a
marker to a pipe before it blocks or aborts, so the scenario sees whether it ran without waiting for time. The
scenario also runs compiled with `qcc` as an AOT executable, and `ForkExitEntryLib.qr` and `ForkExitEntry.qr` are
linked into one executable from script objects, whose children end their main program.

## One-time initialization

A function-local static is initialized under a guard of the C++ runtime (and `std::call_once()` under its once
flag). A child created with `fork()` while another thread was initializing it waits for the guard forever on musl
and macOS, and with libstdc++'s static guards also on glibc. The fork handlers make the window wide for every static
that reads the environment: the environment lock is held exclusively while the process is copied, so a thread that
starts such an initialization then blocks inside the guard.

- Settings read from the environment are `constinit` objects without a guard: `QoreEnvFlag` (whether a variable is
  set) and `QoreLazyValue<T>` (a value computed from the environment), both in `QoreLibIntern.h`. Threads that read
  one for the first time at the same time can all compute it; they all return the value that the first of them
  stored. `constinit` makes the compiler reject one that would need a guard.
- One-time initializations of libraries (c-ares, the layered TLS BIO method, the QUIC TLS backend) and the JIT
  object run in `qore_init()`, before any other thread exists: `qore_network_init_once()`, `QoreJIT::init()`.
- The JIT initializes LLVM under its compile lock instead of a once flag.

## What the child repairs

Besides the locks:

- **Thread list**: only the forking thread's entry is kept; the entries of the other threads are freed (their
  thread data is not, since Programs can still refer to it), with their cancellation state, wait registrations
  and wakeup descriptors.
- **Thread counters**: `thread_counter` and `tp_thread_counter` keep only the forking thread's share
  (`qore_thread_counter_share`, `qore_tp_thread_counter_share`: its own run, and the ThreadPool task it runs), so
  that the end of the child's Programs and `qore_cleanup()` do not wait for threads that do not exist.
- **Programs**: each Program's locks are initialized again, its thread counts keep only the forking thread, and
  parse ownership held by another thread is released; the thread data locks of the other threads that are
  registered with a Program family are initialized again.
- **Signal manager**: after a `fork()` other than Qore's (which stops the signal thread before and restarts it
  after), the signal thread does not exist in the child; its state is cleared, and the next signal handler starts a
  new one. A block set by another thread's Qore `fork()` is lifted.
- **Condition waker, external thread reaper**: their threads do not exist in the child; they are started again
  when they are needed.
- **JIT**: the background compiler's thread does not exist in the child; it is forgotten (its work in progress is
  lost, and that function stays in the tier it is in) and started again with the next background compilation. If
  another thread held the compile lock, the LLVM objects can be inconsistent: the JIT compiles nothing in the child
  (the code runs in the IR tier), and its LLVM objects are not destroyed there.
- **AOT modules**: module initializations and shadow Program populations claimed by other threads are released, as
  if they had failed, so that they are run again instead of waited for (a failed pass leaves only pending entries,
  which the next pass fills); the per-module shadow build locks are initialized again. The lazy constant
  initialization lock is initialized again and held again as often as the forking thread held it; an initializer
  that another thread was running stays marked as running, so a read of its constant reports
  `AOT-PENDING-CONSTANT` instead of running it twice or waiting for it.
- **Async I/O**: the default controller of the parent is abandoned (its I/O threads and workers do not exist, and on
  Linux its descriptors are shared with the parent): it is neither stopped nor freed in the child, and a new default
  controller is created when it is first needed (outside the singleton lock, since its constructor reads the
  environment and creates descriptors). A reference to the parent's controller raises `ASYNC-IO-ERROR` in
  the child.

- **Database connections**: the locks of every `Datasource`, `DatasourcePool` and `SQLStatement` are initialized
  again by the child handler of the DBI object registry, and the connections and statements of the parent are given
  up without calling the drivers, so that the child neither uses nor ends the parent's sessions; see
  `design/dbi-fork.md`.

## What is not covered

- Per-object locks that a thread of the parent held at the moment of the fork: object r-sections, variable locks,
  socket, queue and mutex objects, namespaces. A child that uses such an object waits forever, as before. The locks
  of the DBI objects are the exception (see "What the child repairs").
- Locks of third-party libraries (OpenSSL, LLVM, PCRE2, c-ares) and of the C++ runtime (the guard of a function-local
  static that another thread was initializing, for the statics that do not read the environment, and the per-object
  once flags of IR lowering).
- Process-wide locks of modules that do not register fork handlers (the in-tree modules do; see "Module locks").
- A child should still execute a command or exit soon after `fork()` in a multithreaded program, as the
  documentation of `fork()` says.
