# Native thread cleanup regression

Copyright (C) 2026 Qore Technologies, s.r.o.

The native regression holds pthread TLS destructors at deterministic barriers.
An external worker must remain in `tp_thread_counter` until its native join has
completed, including three destructor passes. It also exercises concurrent first
creation, repeated use of the cleanup queue, default/custom stacks, invalid
stack rejection and recovery, and shutdown without starting external workers.

Use the existing Debug build configured with the installed Qore prefix:

```bash
cmake --build build-debug --target qore qore-native-thread-cleanup-test -j4
LD_LIBRARY_PATH=build-debug build-debug/qore-native-thread-cleanup-test
LD_LIBRARY_PATH=build-debug build-debug/qore-native-thread-cleanup-test --empty
LD_LIBRARY_PATH=build-debug valgrind --error-exitcode=99 --leak-check=full \
  --show-leak-kinds=all build-debug/qore-native-thread-cleanup-test
```

The executable disables Qore signal handling at initialization, the embedded
API equivalent of `qore -b`. On Linux, run this additional mode as an unprivileged
user to exercise real `pthread_create` failure for both the cleanup worker and
an external worker, followed by successful retry:

```bash
LD_LIBRARY_PATH=build-debug build-debug/qore-native-thread-cleanup-test --creation-failure
```

That mode temporarily reduces only its own process's `RLIMIT_NPROC` soft limit
and restores it immediately after the creation attempt. It does not alter the
resource limits of other processes. The ordinary mode is portable and requires
no resource-limit changes.

## Explicit process exit

Run the subprocess matrix against either a Release or Debug build:

```bash
cmake --build build --target qore qore-native-thread-cleanup-test -j4
python3 examples/test/qore/classes/ThreadPool/explicit_exit.py \
  --qore build/qore --native build/qore-native-thread-cleanup-test --repeat 5
```

The runner selects each executable's build library and gives every child a
five-second timeout. It covers normal cleanup, explicit exit without a reaper,
an idle reaper, natural process return without `qore_cleanup()`, empty/stopped
ThreadPools, active workers, TLS cleanup held after Qore TID release (default and
custom stacks), and exits from signal handlers.
It checks exit statuses 0, 17 and 255, Qore output, and deterministic C stdio and
`atexit` markers: idle exits flush and run callbacks, while immediate exits with
active native cleanup or from signal handlers bypass both. Native barriers stay
closed in active cases, so an accidental wait for native cleanup fails by timeout.
The managed-signal modes check native signal-thread TLS completion from an `atexit`
callback, detecting a missing join without depending on timing. They also cover an
idle registered handler and a blocked callback, including one whose registration
was replaced while it was running; blocked callbacks must not be joined by exit.

Run the existing `ThreadPool.qtest`, AsyncIoController unit/logger suites, and
HTTP shutdown/consumer regressions with the new `libqore`, debugging enabled,
and local module paths. For regex-heavy runs under Valgrind, set
`QORE_PCRE2_NO_JIT=1` before starting Qore.

## Independent libc failure-path finding

On Fedora glibc `2.43-8.fc44`, real kernel `pthread_create` failures retain native
TLS allocation storage. The native `--creation-failure` mode passes its functional
assertions and Qore counter rollback checks but Valgrind reports 704 bytes in
two possible-loss records inside glibc. Its ordinary mode, plain HTTP, and the
SOAP consumer tests report zero errors and zero lost allocations. No reports are
suppressed. Valgrind also emits its existing Qore DWARF reader warning.

`pthread_creation_failure.c` isolates the failure without Qore, OpenSSL or XML.
After 1,000 rejected creations, the native allocator retains an additional
287,712 bytes, both with the default glibc stack cache and with caching disabled.
The smaller two-failure experiment reports 544 bytes under Valgrind. This is an
independent libc failure path, not just native thread cleanup still in progress:
all successfully created threads in the reproducer are joined.

```bash
cc -g -pthread examples/test/qore/classes/ThreadPool/pthread_creation_failure.c \
  -o /tmp/qore-pthread-creation-failure
/tmp/qore-pthread-creation-failure
```

Run as an unprivileged Linux user. The XML interoperability execution record
tracks this dependency finding for P9 environment verification; it remains open.
No application workaround, system library change or upstream report is applied.
