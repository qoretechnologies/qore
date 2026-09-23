# Delayed GC with Deterministic Collection

**Status:** Design. The baseline in §8 was re-measured on the corrected collector on 2026-09-23.

**What it would enable:** removing `QoreParseOptions::ALLOW_OPAQUE_REFERENCES` and making
`@=` an ordinary part of the language, because a cycle held by an opaque edge would no
longer be a permanent leak.

**Companions:** [`design/dgc.md`](../design/dgc.md) (the present collector — read first).

## 1. What this proposes

A single collector thread per `libqore` process that re-derives recursive sets **off the
mutator's path**, following opaque (`@=`) edges that a synchronous scan deliberately skips,
and collects the cycles it finds.

The name states the constraint. The collection is **delayed** — it does not happen at the
dereference that closes the cycle. It must remain **deterministic** — every cycle that
becomes unreachable is still collected, every destructor still runs, and both still happen
before the owning `QoreProgram` is torn down. This is the opposite of the JVM's finalizer
contract, where a finalizer may never run at all, and it is the property that makes the
feature acceptable in a language whose memory correctness is a platform guarantee.

## 2. Why the present collector cannot reach these cycles

`@=` stores a strong reference the scanner refuses to follow. Exactly four sites read the
tag, and they are the four that decide whether an edge is walked:

- `needs_scan(const QoreValue&)` — `lib/AbstractQoreNode.cpp:744`
- the list and hash traversals in `RSetHelper::startNode()` — `lib/RSet.cpp:878`, `:890`
- `qore_object_private::scanMembersIntern()` — `lib/QoreObject.cpp:466`

A cycle running through such an edge is invisible to cycle detection by construction. Today
the only thing that breaks it is `qore_program_private_base::clearOpaqueTargets()`
(`lib/QoreProgram.cpp:2183`) at Program teardown, which is what bounds the leak to the
Program's lifetime rather than the process's. That bound is the entire safety argument for
the feature, and it is why the operator is gated.

## 3. What "deterministic" has to keep meaning

Three guarantees have to survive, and they are not the same guarantee:

1. **Completeness.** Every cycle that becomes unreachable is collected, and every
   destructor in it runs. Not "usually", not "if the collector gets a chance". A collector
   that yields to mutators — and §6.4 explains why this one must — cannot bound *when*, so
   completeness has to be underwritten by a backstop that cannot be starved: Program
   teardown, which already runs `clearOpaqueTargets()`, plus an explicit collection point.
2. **Order.** Within a collected set, destructors run in the depth-first order
   `qore_object_private::deleteOrDefer()` already produces, and cross-member references are
   released after the deferred object's deletion, exactly as `RSetDerefHelper` does now. A
   delayed collector changes *when* a set is collected, never the order inside it.
3. **A synchronous entry point.** Tests must be able to say "collect now" and assert the
   result. The repo forbids waits and polling in tests, so a background collector without a
   synchronous "run one pass and return" API is untestable by the project's own rules. This
   is a requirement on the design, not a convenience.

What is explicitly *given up*: the point in time at which an opaque cycle's destructors run.
Qore has no non-deterministic destruction today — acyclic objects die at scope exit, cycles
die at the dereference that closes them. This proposal introduces the first case where an
object holding a socket, a file descriptor or a transaction is released later than the
statement that dropped it. That is a language-visible change and has to be documented as
one.

## 4. Why Qore is not the JVM or Python

The analogy misleads, in Qore's favour, and the differences decide the design:

- **No root enumeration, ever.** The decision rule is `rcount == references`
  (`RSet::canDelete()`), so any reference the scanner cannot see counts as external and
  blocks collection. Thread stacks, C++ temporaries, `ReferenceHolder`s and module-held
  pointers are already counted. There are no stack maps, no safepoints and no conservative
  scanning, and there never need to be.
- **No write barriers, no generations, nothing on the allocation path.** Reference counting
  stays primary; the collector only ever handles cycles, which are a small fraction of the
  heap.
- **No stop-the-world, and none available.** Qore has no global interpreter lock and no
  safepoint protocol. The scanner's whole structure — try-lock, roll back, wait on a
  notifier, retry — exists *because* it cannot stop the world. A Qore collector is therefore
  necessarily concurrent and cooperative.

CPython's "subtract internal references over every tracked container" is unavailable for a
second reason: it needs an enumerable set of all containers, which Qore does not maintain
and should not start maintaining — it would cost an atomic list insertion and removal on
every hash and list allocation.

## 5. The design

### 5.1 Candidate-driven, never heap-wide

The root set already exists. `qore_program_private_base::opaque_targets`
(`include/qore/intern/qore_program_private.h:614`) records every target of an opaque
reference, tagged with its owning Program. Every cycle through an opaque edge contains a
registered target by construction — if `H —@=→ T` and the cycle is `H → T → … → H`, then `T`
is in it — so a pass rooted at the registry finds all of them, and only them.

This is what makes the feature tractable here and not in a general tracing collector: the
candidate set is small, exact, and already maintained.

A second candidate source is worth adding for a benefit unrelated to `@=`: objects whose
`canDelete()` returned "cannot delete". `design/dgc.md` documents cases where a set is
**stranded for the life of the process** because nothing re-triggers a scan. A periodic
re-derivation of those turns a permanent leak into a bounded one.

### 5.2 Rooting is not traversal

`6aa593e80` establishes that a scan *started at* a closure-bound local whose frame still
holds it can never collect anything: the frame's reference is a real reference and not an
edge, so every set through the variable sees `references > rcount`. The collector must
respect that and never root a pass at an `RObject` with `rrefs > 0` — closure-bound local
or otherwise.

It must **not** generalise that to traversal. `design/dgc.md` is explicit that a scan
started elsewhere still walks a reachable object's members even when that object has real
references, and says why: skipping those edges can omit a live owner of a shared container
from a recursive set, the container's one reference to an object behind it then looks
internal, the set comes out too small — **and it is collected prematurely**. That is the
dangerous direction, and `aa59e8cc9` is a worked example of it reaching real behaviour in
every execution mode.

So `rrefs > 0` disqualifies a node as a **root**, never as a **node of the graph**. A cycle
running `T → … → V → … → T` through a closure-bound variable `V` is found only if the pass
rooted at `T` enters `V`.

**The corollary is easy to get wrong.** The collector must test `rrefs` *before* it calls
`RSetHelper`, not merely tolerate the deferral. Since `db56d0ecc`, `checkDeferScan()`
invalidates the recursive set recorded for the object on every deferral — that is what makes
deferral safe. A collector that pokes deferred roots therefore does not just waste the
attempt: it destroys a set another scan has just built, and if that scan rebuilds it, the
next poke destroys it again. Candidate selection has to exclude `rrefs > 0` roots, or the
collector becomes a set-invalidation engine.

Once the frame releases the variable, `rrefs` reaches zero and `ClosureVarValue::deref()`
takes the deferred scan synchronously. The collector's interest in closure-bound locals is
therefore narrow: one that outlived its frame, is still held by a closure, and sits in a
cycle closed by an opaque edge. In that state it is an ordinary candidate.

### 5.3 The collection primitives already exist

Nothing new is needed to *perform* a collection:

- **To re-derive a set:** construct `RSetHelper rsh(*robj, xsink)` (`lib/RSet.cpp`). The
  constructor runs the whole scan, is already concurrency-hardened, already rolls back and
  retries, and already short-circuits on `q_disable_gc`.
- **To force a verdict and tear down:** reference and dereference a member of the set. This
  is exactly how watched nodes already re-check a set (`qore_dgc_node_dereferenced()`), and
  it drives `customDeref()` → `canDelete()` → collection with no new teardown path.

**One gap:** `RSetHelper` takes an `RObject&`, and hashes and lists are not `RObject`s.
`QoreValue::makeOpaqueHash()` / `makeOpaqueList()` register container targets, so a pass
cannot be rooted at one. Either root at an `RObject` reachable from the container, or extend
the scanner's entry point.

### 5.4 Triggering: event-driven, not periodic

A timer is the wrong mechanism and the repo's own rule against polling says so. The codebase
already has the right idiom in the watch mechanism: `qore_dgc_node_watch_count` is a global
relaxed atomic that costs one load on the fast path when nothing is watched.

Mirror it. While the opaque registry is non-empty, a `canDelete()` verdict of "cannot
delete" sets a dirty flag; the collector waits on a condition variable and sweeps the
registry when the flag is set. One relaxed load on a path that is already doing a locked
comparison, and no period to tune.

Event-driven triggering alone is *not* sufficient for completeness — the last external
reference can be dropped on an object whose refcount change reaches no registered target —
which is why the sweep is registry-rooted rather than starting from the object that fired
the trigger, and why §3's backstop is required.

### 5.5 The collector thread

One per process, following the signal thread's precedent:

- **Reserved TID.** The signal thread takes a reserved slot via
  `thread_list.getSignalThreadEntry()` (`lib/thread.cpp:3242`) rather than a user TID. The
  collector should do the same, so it does not consume one of `MAX_QORE_THREADS` and
  `Thread::list()` stays truthful.
- **Park, do not exit.** Starting on the registry's 0→1 transition and exiting on 1→0
  churns a thread per oscillation. Park on a condition variable instead; the observable
  property — no CPU, no scanning when there is nothing to collect — is identical.
- **The registry's shard locks are leaf locks.** Nothing may be acquired while one is held. The
  empty→non-empty transition of the registry must be detected under a shard lock and acted on after
  releasing it; with the registry striped, that needs a registry-wide atomic count of entries.
- **fork.** `fork()` already stops the signal thread first (`lib/ql_lib.qpp:1068`). The
  collector needs identical treatment, or a child inherits r-sections held by a thread that
  does not exist in it.
- **Shutdown.** The collector stops before Program teardown, or it races
  `clearOpaqueTargets()`.
- **Respect the existing switch.** `QLO_DISABLE_GARBAGE_COLLECTION` / `qore_disable_gc()` /
  `qore_is_gc_enabled()` already exist; the collector must honour them and should be
  separately disableable.

## 6. The four hard problems

### 6.1 A pass cannot be split

A scan's safety argument is that it holds the r-section of every object it reached until
commit and rolls back wholesale on conflict. An `rcount` committed from a window in which
the graph changed is **not** conservative: too high an `rcount` makes a live set look
collectable. So the collector can pace *between* passes but not *within* one — no "release
the locks, resume next slice" without a new safety argument such as a per-node epoch or a
write barrier.

The consequence bounds the whole feature. A single pass is effectively a stop-the-world over
the subgraph it walks. At the measured ~1 µs per node that is microseconds for a
2,000-entry registry and about a second for a million-node graph, holding shared r-sections
throughout. This is the strongest argument for keeping the candidate set small.

### 6.2 Two scan policies over one `rcount`

An opaque-following pass computes a *more* accurate `rcount` — an opaque edge is a real
internal reference; the scanner simply chooses not to follow it — so there is no safety
problem. There is a cost problem. The pass commits a larger component with larger counts;
the next synchronous scan does not follow those edges, computes a smaller one, and
`findUnchangedComponents()` never matches. Every synchronous scan touching an opaque edge
then becomes a **writing** scan: invalidate, reallocate, re-take every weak reference and
watch, write every object reached. That is the class of cost `design/dgc.md` records as 21%
of an eight-thread read-only scan.

Two ways out, and the choice should be made before any code is written:

- tag the `RSet` with the policy that built it and make a policy mismatch a
  non-replacement; or
- give the collector its own bookkeeping and never let it commit an `RSet` — a second
  traversal, with its own locking discipline.

### 6.3 Destructors on a collector thread

Mechanically this is solved by existing patterns: push the object's own Program context and
hold a `depRef` (never a strong reference) across the destructor; route exceptions as an
unhandled background-thread exception; purge thread resources per pass; and release every
r-section and rset lock **before** running any destructor, which `RSetDerefHelper` and
`LValueHelper::saveTemp()` already establish.

The unsolved part is not mechanical. Destructors are user code: they can block, take locks
an application thread holds, and re-enter the runtime. A collector blocked in a destructor
is a stalled collector, which is tolerable, but only if it holds nothing at that point.

### 6.4 Convergence and starvation

Two existing rules cut against a background scanner. While any thread waits for the
r-section, `tryRSectionLockSharedNotifyWaitRead()` admits no new scan — correct
prioritisation, but it means the collector can be refused admission indefinitely on a hot
graph. And the retry loop is deliberately unbounded, with a documented failure mode of a
pass that retries without registering a notification spinning at 100% CPU (the seven-hour
`qcc`).

The collector therefore needs what a synchronous scan is forbidden: a retry budget and a
"give up, try later" path. This is the one place a wait is justified.

## 7. What it would let us remove — and what it would not

**Would enable:** deleting the `ALLOW_OPAQUE_REFERENCES` gate. With the collector following
opaque edges, `@=` stops being a leak primitive and becomes a scan-cost hint —
semantically equivalent to `=` apart from collection latency. The option should be
*inverted* rather than deleted, to `PO_NO_OPAQUE_REFERENCES`, so sandboxed Programs can
still be denied a construct that defers destruction.

**Prerequisite, independent of the collector:** the global `opaque_lock`. Registration
counts *references, not assignments*, so every copy of an opaque value takes a process-wide
`QoreThreadLock` and a `std::map` operation (`QoreValue::ref()` / `discard()`). The header
states the premise outright — "Opaque assignment is a rare, deliberate operation, so one
global plain lock is enough" — and ungating `@=` destroys it. This must be fixed first,
whatever happens to the collector.

**Resolved 2026-09-23:** the registry is striped into 64 shards, each with its own leaf lock, selected by a hash
of the target's address (`design/dgc.md`, "Ownership and Program tracking").  The §8 registry benchmark with
`@=` on eight threads went from 46,206-49,853 to 53,683-61,603 ops/s (one thread: 45,726-46,452 to
52,848-94,675); it still does not scale with threads because every thread writes the same hub's hash, whose
lvalue lock serializes them, which no registry change can remove.

**Would not fix:** a separate gap found while auditing this area — `ALLOW_OPAQUE_REFERENCES`
is extended bit 88, and `PO_POSITIVE_OPTIONS` (`include/qore/Restrictions.h:168`) is a
legacy 64-bit mask, so the option escapes positive-option discipline. A child Program can
grant itself `@=` where it is correctly refused `:=`. Verified empirically; unrelated to
this design, and worth its own issue.

**Fixed 2026-09-23:** the positive and free options are enforced with the full-width masks
`QoreParseOptions::POSITIVE_OPTIONS` and `QoreParseOptions::FREE_OPTIONS`; a locked child is refused
`allow-opaque-references` by every route.

## 8. Cost on the corrected collector

Re-measured on 2026-09-23 against `develop` at `6ba8a893c`, i.e. after `db56d0ecc` (scans that were being
skipped are made again) and `6aa593e80` (a closure-bound local's frame holds a real reference).

### Registry register/unregister (release build, wall-clock)

A hub holds 2,000 entries in a hash, each entry points back at the hub, and the hub is reached through a
static class variable holding one of its entries, so a write at the hub is rooted at an object with no real
reference. One operation registers a new entry under a fresh key and removes it again. Three runs each:

| assignment | threads | mode | ops/s | objects walked per op (debug build) |
|---|---|---|---|---|
| `=` | 1 | tiered (default) | 198 - 268 | 8,006 |
| `=` | 1 | ast | 190 - 218 | 7,966 |
| `=` | 8 | tiered (default) | 848 - 924 | 250 |
| `=` | 8 | ast | 630 - 746 | — |
| `@=` | 1 | tiered (default) | 34,105 - 50,421 | 2 |
| `@=` | 1 | ast | 50,196 - 71,659 | 2 |
| `@=` | 8 | tiered (default) | 40,043 - 45,435 | — |
| `@=` | 8 | ast | 42,634 - 46,818 | — |

What the numbers say:

- `=` still walks the whole registry per registration and per removal - about four objects per entry, per
  operation - so the ~200x gap between `=` and `@=` stands on the corrected collector. The original 162
  ops/s (6,005 objects per registration) is the same order.
- `@=` does not scale: eight threads do no more work than one. Every copy and release of an opaque value takes
  the one global `opaque_lock` (§7), and this benchmark copies the value on each registration.
- With eight threads, scans of the same root are coalesced (a scan runs at most once per generation), so the
  objects walked per operation drop to ~250 while throughput rises only ~3.5x.

### Scan counts by shape (debug build, 100 writes; `examples/test/qore/misc/dgc-scan-avoidance`)

Objects walked by the scans the writes made. Every shape also asserts collection: nothing destroyed while the
set is held, everything destroyed once it is released.

| shape | ast | ir / jit / tiered | aot |
|---|---|---|---|
| plain local root | 0 | 0 | 0 |
| `self` writes in a method, root held by a list | 9 | 6 | 9 |
| closure-bound local root | 303 | 303 | 303 |
| non-root member (`root.peer.x`), root in a local | 300 | 300 | 300 |
| root held only by a list | 303 | 303 | 303 |
| registry growth, hub in a local | 0 | 10,300 | 10,300 |
| registry growth, hub held only by its own cycle | 5,150 | 25,748 | 15,450 |
| registry removal, same | 15,049 | 15,149 | 15,049 |
| registry growth with `@=` | 200 | 200 | 200 |
| confirming scan of an open cycle, holder in a list | 500 | 500 | 500 |
| server controller in a set, one op registered and removed per request (qore's async HTTP server shape) | 10,484 | 12,142 | 9,172 |

The server shape rebuilds the controller's recursive set twice per request (200 sets for 100 requests): the
registration and the removal are made in the controller's own methods, the deferral discards the set, and the
scan made when the method's real reference goes must rebuild it with the r-sections of the whole graph held
exclusively. This is the contention measured in qorus-core's request threads.

The compiled tiers diverge on the registry shapes because a compiled assignment
(`qore_rt_lv_path_assign()`) borrows the value and takes references of its own, releasing them after the
assignment; the first plain dereference of a new object makes the scan its constructor deferred, and in the
compiled tiers that happens once the entry is already linked into the registry, so each registration walks
it. The AST interpreter hands the value over and the deferred scan waits. Code built with `%modern` runs
tiered by default and qlib is shipped AOT, so the compiled columns are the ones production code pays.

### Other quantities

| quantity | value | source |
|---|---|---|
| scan cost | ~1 µs per node walked | 6,005 nodes at 162 ops/s, `6f090ef43`; consistent with the table above |
| leak per stranded object | ~1.2 kB | measured during `db56d0ecc` |
| registry memory | ~80 B per distinct target | `std::map` node; 2,000 targets ≈ 160 kB |
| trigger hot-path cost | one relaxed atomic load | by analogy with `qore_dgc_node_watch_count` |

The justification for this proposal therefore still holds on the corrected collector: a registry held with
`=` costs a walk of the registry per operation, and `@=` removes it. The same numbers show that the synchronous
collector can recover part of that cost on its own, without a background thread: a registry reached through a
real-referenced member of its own cycle is live, so an insertion there needs no walk (set-level pinning), and
the compiled tiers should not make a deferred scan before the value they are assigning is in place.

## 9. Open questions

1. Policy tagging or separate bookkeeping (§6.2)? This decision is structural and cannot be
   retrofitted cheaply.
2. What is the completeness backstop beyond Program teardown — an explicit
   `Program::collect()`, a quiescence detector, or both?
3. Container targets cannot root a scan (§5.3). Extend `RSetHelper`, or resolve to a
   reachable `RObject`?
4. Should the stranded-set candidate source (§5.1) ship independently? It fixes a documented
   permanent-leak class and needs none of the opaque machinery.
5. Does deferring destruction interact with sandboxing — an object holding a
   `QDOM_FILESYSTEM` or `QDOM_NETWORK` resource released on a thread with no Program
   context of its own?

## 10. Risk

This is the sharpest edge in the runtime, and the recent history says so. `rcount` too low
leaks; `rcount` too high frees objects that are still referenced. Both directions have been
hit in the last week: `db56d0ecc` was the benign direction, and `aa59e8cc9` — a removal
under a deferred scan root collecting an object that was still referenced, reproduced in
every execution mode — was the dangerous one. A concurrent collector makes scanning
continuous by design rather than incidental, so any latent ordering bug becomes a frequent
one.

Minimum bar before this is enabled anywhere: the full DGC suite under TSan on arm64, where
the shared-rsection races actually reproduce, and a synchronous collection API so the tests
assert rather than poll.

## 11. Related files

- [`design/dgc.md`](../design/dgc.md) — the present collector; "Opaque references", "The
  rrefs deferral", "Scan locking" and "Closed recursive sets" are the sections this builds
  on.
- `lib/RSet.cpp` — `RSetHelper`, `canDelete()`, `checkDeferScan()`, the watch registry.
- `include/qore/intern/RSet.h` — `RObject` fields, `RSetHelper::deferred()`.
- `include/qore/intern/qore_program_private.h` — the opaque target registry.
- `lib/QoreProgram.cpp` — `clearOpaqueTargets()`.
- `lib/QoreValue.cpp` — opaque representation, registration and release.
- `lib/thread.cpp`, `lib/QoreSignal.cpp`, `lib/ql_lib.qpp` — the runtime-thread and fork
  precedent.
- `examples/test/qore/misc/dgc-*.qtest` — the suites any change here must keep green.
