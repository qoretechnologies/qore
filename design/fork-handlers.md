# Fork handlers: process-wide locks in a child created with fork()

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
| 100 | signal manager (`QSM.mutex`, with `QSM.tcount`) | ACQUIRE | thread list, signal thread counter |
| 150 | JIT compile lock (`QoreJIT::compile_mutex`) | TRY | JIT caches, JIT queue, type caches, environment |
| 200 | async I/O controller singleton | ACQUIRE | Program cleanup callbacks |
| 300 | AOT module state | ACQUIRE | condition waker, thread list |
| 310 | plugin registry | ACQUIRE | thread list (cancellation checks), environment |
| 320 | time zone manager (`QTZM`) | ACQUIRE | thread list (cancellation checks) |
| 400 | thread counters (`thread_counter`, `tp_thread_counter`) | ACQUIRE | thread list, condition waker (cancellable waits) |
| 500 | thread list (`thread_list.lck`) | ACQUIRE | condition waker, stack lock |
| 510 | condition waker (`cond_waker.lck`) | ACQUIRE | — |
| 520 | stack lock (`stack_lck`) | ACQUIRE | — (only `pthread_create()`) |
| 900 | AOT PC map attach lock (`g_aot_pc_attach_lock`) | ACQUIRE | AOT PC map lock, environment |
| 1000 | leaf locks: caches and registries | ACQUIRE | environment, debug output, allocation tracking |
| 1500 | environment (`SystemEnvironment`) | ACQUIRE | debug output, allocation tracking |
| 1900 | debug output (`printd()`) | ACQUIRE | allocation tracking |
| 1950 | allocation tracking (only with `ENABLE_ALLOC_TRACKING`) | ACQUIRE | — |

The environment lock ranks near the top: its holders only copy strings, create processes and read the
environment, so it is a leaf, and every lock that reads the environment while held (there are hundreds of such
reads) ranks below it.

The striped leaf lock arrays (the 64 constant value locks and the 64 opaque target shard locks) are acquired like
other leaf locks, except with ThreadSanitizer: its deadlock detector handles at most 128 locks held by one thread,
which the stripes alone would reach, so TSan builds only initialize them again in the child (`QFLM_STRIPED`).

### Rules

- A handler is registered when its object is constructed (static initialization, or the first use of a
  function-local static) and must not be registered while another registered lock is held.
- A thread must not call `fork()` while it holds a registered lock that is acquired before `fork()`: it would wait
  for itself. Holding the environment lock across `fork()` does not make the forking thread wait for itself, but
  the handlers would then acquire every other lock while it is held, and a thread holding one of them while it waits
  for the environment lock would deadlock with the forking thread, so it is not allowed either (an assertion in
  debug builds; see `SystemEnvironment`). Process creation with `fork()` needs no hold, since the handlers hold the
  lock while the process is copied; `posix_spawn()` runs no handlers, so its callers hold the read lock.
- No user code runs while a registered `ACQUIRE` lock is held. The thread list lock used to be held while a
  Program's thread initialization code ran (`QoreThreadList::activate()`); that code now runs after the lock is
  released.

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
- **Async I/O**: the default controller of the parent is abandoned (its I/O threads and workers do not exist, and on
  Linux its descriptors are shared with the parent): it is neither stopped nor freed in the child, and a new default
  controller is created when it is first needed (outside the singleton lock, since its constructor reads the
  environment and creates descriptors). A reference to the parent's controller raises `ASYNC-IO-ERROR` in
  the child.

## What is not covered

- Per-object locks that a thread of the parent held at the moment of the fork: object r-sections, variable locks,
  socket, queue and mutex objects, namespaces. A child that uses such an object waits forever, as before.
- Locks of third-party libraries (OpenSSL, LLVM, PCRE2, c-ares) and of the C++ runtime (the guard of a function-local
  static that another thread was initializing, for the statics that do not read the environment, and the per-object
  once flags of IR lowering).
- Process-wide locks of modules (for example the locks of the in-tree `i18n`, `logger_bin` and `sshutil` modules): a
  module that keeps such a lock needs its own `pthread_atfork()` handlers.
- A child should still execute a command or exit soon after `fork()` in a multithreaded program, as the
  documentation of `fork()` says.
