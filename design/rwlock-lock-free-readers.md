# Read-write lock: lock-free readers

`qore_var_rwlock_priv` (`include/qore/intern/qore_var_rwlock_priv.h`) is the read-write lock behind every object's
members (`qore_object_private::rml`, extended with the r-section by `qore_rsection_priv`), every global and
closure-bound variable, and the public `QoreRWLock` and `QoreVarRWLock` classes. Every read of a member or variable
takes and releases its read lock, so the read path is on the hottest path there is.

## Cost model

It used to be a mutex (`l`) plus condition variables: a read lock locked `l` to increment the reader count and the
unlock locked it again to decrement it - two mutex acquisitions per read, contended by every thread reading the same
object or variable. Readers now touch only atomics; `l` is taken by writers, by the r-section, and by a reader only
while a writer holds, or is taking, the lock. A read of an uncontended lock is two atomic read-modify-writes (the
increment and the decrement of `readers`); it never blocks unless a writer holds the lock.

What remains shared is the reader count itself: every reader of one object or variable writes the same cache line.
Removing that would take readers that touch no shared state at all (RCU-style reclamation of the values a writer
replaces), which Qore's lvalue model - containers mutated in place while uniquely owned - does not allow as it is.

## State

- `readers` (`std::atomic_int`): read locks held, including the one under an r-section. Readers change it with no
  lock.
- `write_claim` (`std::atomic_bool`): set while a writer holds the lock or is checking whether it can take it.
  Written only under `l`.
- `write_tid` (`std::atomic_int`): the writer's TID, or -1. Written only under `l`.
- `write_waiting` (`std::atomic_int`): writers waiting for the lock, incremented under `l` before they check for
  readers; read without `l` by the last reader to leave.
- `read_waiting`, the condition variables, and all r-section state: only under `l`, as before.

## The handshake

**Reader** (`tryReadFast()`, used by `rdlock()` and `tryrdlock()`): if `write_claim` is set, fall back to the slow path.
Otherwise increment `readers`, then load `write_claim`; if it is still clear, the read lock is held. If it is set now,
undo the increment (`unlockRead()`) and fall back. The slow path takes `l`, waits while `write_tid` is set, and
increments `readers` under `l`.

**Writer** (`tryClaimWriteIntern()`, with `l` held, from `wrlock()` and `trywrlock()`): store `write_claim`, then load
`readers`; if there are none, set `write_tid` - the lock is held, and the claim stays set until `unlock()`. If there
are readers, clear the claim again and wait on `write_cond`.

Both stores and loads are sequentially consistent, so in any interleaving either the reader sees the claim and backs
out, or the writer sees the reader and waits for it. Under `l`, `write_claim` is set exactly when `write_tid` is: a
claim that finds readers is dropped before `l` is released to wait. That is what lets the r-section functions keep
testing `write_tid` under `l` unchanged.

**Last reader leaving** (`unlockRead()`): decrement `readers`; if that left none, load `write_waiting` and, if a writer
waits, take `l` and signal it. A writer increments `write_waiting` before it checks for readers, so if the writer saw
this reader, this reader sees the writer: a wakeup cannot be lost. The writer holds `l` from its check until it waits,
so the signal cannot arrive before it waits.

**Unlock** (`unlock()`) tells a write unlock from a read unlock by `write_tid` alone, without `l`: while it is set, no
other thread holds the lock (the r-section held on top of the caller's own write lock is released through
`rSectionUnlock()`, not `unlock()`). The write unlock clears `write_tid`, then `write_claim` (sequentially
consistent, so a reader that sees the claim clear sees everything written under the write lock), and notifies as before.

## Readers keep their preference

A writer that finds readers drops its claim and waits; readers arriving meanwhile get in. This is the existing
contract (see `lib/QoreRWLock.cpp`): it is what allows a thread holding the read lock to take it again. Blocking new
readers while a writer waits would deadlock a thread that re-enters the read lock while a writer is queued behind its
first one. As before, a steady stream of readers can starve a writer.

## Regression coverage

- `examples/test/qore/threads/concurrent-var-access.qtest` - readers and writers on a global variable, an object
  member and a whole-value replacement (no lost update, no value going backwards, no torn value), and member reads
  while cycles are built and collected around the object (the r-section shares the lock).
- The DGC tests in `examples/test/qore/misc/` exercise the r-section under contention.
- ThreadSanitizer, on a module-free equivalent of the test in every execution mode, reports no race.
