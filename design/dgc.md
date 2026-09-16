# Distributed Garbage Collection (DGC)

Qore's runtime detects and collects reference cycles of `QoreObject` instances automatically. **Qore code (`.q`, `.qm`, `.qc`) must never be asked to break cycles manually** — memory correctness is a platform guarantee, not an application concern. This document explains how DGC works and the rules C++ module authors must follow to keep that guarantee holding.

## Why cycles happen

Plain reference counting cannot reclaim objects that form a cycle (`A → B → A`); each participant's refcount stays ≥ 1 because its peer holds it. The standard idioms that produce cycles in Qore code are:

- Parent ↔ child back-pointers (connection ↔ manager, stream ↔ connection, observer ↔ subject).
- Handlers / listeners registered into a container held by their own target (`conn.poll_op.listeners[id] = self`).
- Closures captured by an object that also owns the closure's containing scope.

These patterns are fine in Qore code. DGC finds and breaks them at deref time without the Qore programmer thinking about it.

## Core concepts

Every `QoreObject` (via `RObject` in `include/qore/intern/RSet.h`) carries these fields on top of its plain `references` counter:

| Field | Meaning |
|---|---|
| `references` | Standard refcount (managed by `ref()`/`deref()`). When it reaches 0, the object is destroyed. |
| `rrefs` | "Real" refs: references known *not* to be part of a cycle (set by `realRef()`, e.g., method-call helpers, background thread ownership). If `rrefs > 0`, starting a scan at that object is **deferred**; scans initiated elsewhere still follow its edges. |
| `rcount` | Number of unique cyclic references pointing *at* this object, computed by the scanner. `rcount == references` ⇒ every ref to this object comes from inside the rset. |
| `rset` | Pointer to the `RSet` the object belongs to (a group of objects in one detected cycle). |
| `rml` | Read/write lock with a special "r-section" mode used during scans. |

The cycle detector (`RSetHelper`, `lib/RSet.cpp`) traverses the graph from a candidate object, assigns every object it visits to an `RSet`, and computes each member's `rcount`. Detection runs opportunistically during `customDeref` (`lib/QoreObject.cpp`) when the normal path cannot prove the object is alive.

## The decision rule (`RSet::canDelete`)

See `lib/RSet.cpp:199-266`. When an object's `customDeref` has dropped its ref, we decide whether the whole rset can be torn down:

```
for each member in rset:
    if member.rcount > member.references:  stale — rescan
    if member.rcount != member.references: external ref exists → keep
all equal → invalidate rset, collect everything
```

The invariant that drives the entire design is this: **for an rset to be collectable, every member must satisfy `rcount == references`**. If any member has `rcount < references`, *something outside the rset* still points at it, and we must not free the cycle.

That means any ref held by code the scanner cannot see counts as "external" and blocks collection.

## What the scanner can and cannot see

`qore_object_private::scanMembersIntern` (`lib/QoreObject.cpp:358`) walks the object's data hashes. It iterates:

1. `data` — the object's public member hash.
2. `cdmap` — one sub-hash per parent class, containing that parent's `private:internal` members.

Any reference stored as a `QoreValue` in either place is visible. Raw C++ pointers to `QoreObject` or to another object's private data (`AbstractPrivateData*`) held inside a C++ private struct are **invisible** unless the class provides a custom scanner (see below).

### The rrefs deferral

If `rrefs > 0`, `RObject::checkDeferScan()` defers starting a scan at that object. Scans are retried after the
last `realDeref()` drops `rrefs` to 0. If a `realRef()` is leaked, the external reference keeps the object alive.

A scan initiated at another object still traverses a reachable object's members under its r-section lock,
including when that object has real references. Skipping those edges can omit a live owner of a shared
container from a recursive set: the container's one physical reference to an object behind it can then look
entirely internal, and the smaller set would be collected prematurely. Including the owner completes the
cycle, and its real references keep `references > rcount`, preventing collection.

### Shared containers: the scan follows edges, not nodes

A `QoreHashNode` or `QoreListNode` can be referenced by more than one object. Each of those references is a
distinct edge in the object graph, and the scanner must follow **every** one of them: the traversal chain
(`ovec`) built along an edge is what establishes cycle membership for the objects behind the container, and each
edge is what contributes to their `rcount`.

`RSetHelper::checkIntern()` memoizes `(owning RObject, container)` in `hset` / `lset`. The owning object is
preserved through every nested container. Each owner therefore reaches a shared container's objects even
when two owners share the same outer container. Memoizing the immediately enclosing container would lose
the second owner's path through that shared outer container. A container is walked at most once per owner.

Memoizing on the container node alone is a cycle leak (fixed 2026-09-12). It drops every edge into a container
but the first one the DFS happens to reach, and the outcome then depends on traversal order:

- If the surviving edge comes from outside the candidate cycle, the objects behind the container never enter the
  rset, and anything they reference is left with `rcount < references`. `RSet::canDelete()` reads that as a live
  external reference, returns 0, and — because the rset stays valid — caches that verdict forever. Nothing
  re-evaluates it afterwards, because what later makes the graph collectable is a *container's* reference count
  dropping, which is invisible to the rset validity model. The cycle is stranded permanently.
- If the surviving edge comes from inside the cycle, the objects behind the container are placed in the rset with
  `rcount == references` even while an object outside the rset still reaches them through the same container.

The symptom of the first case, from `qore -d1` on a debug build:

```
scanMembersIntern() search <outside obj> key 'declared' 0x390a5600 (hash)   <- walked
scanMembersIntern() search <in-cycle obj> key 'attrs'   0x390a5600 (hash)   <- same node, nothing follows
setRSet() <obj behind the container>  rs: (nil)      rcnt: 0
RSet::canDelete() cannot delete graph obj <its target> rcount: 1 refs: 2
```

Regression coverage: `examples/test/qore/misc/shared-container-cycles.qtest`.

Traversal and reference counting use separate identities. Each physical container slot gets a stable scan-local
reference ID, including when a recursive walk reaches a later slot before the original iterator does. Direct
object members and closure captures get their own IDs. `orefs` records the incoming reference ID for each DFS
chain entry; the scan root has no incoming reference. A cycle-closing edge counts its own reference, and a
later merge counts a forward edge only if that reference has not already been counted.

This avoids both double-counting a shared slot when its original owner joins a cycle later, and counting an
external holder's reference as though it belonged to a different slot that closed the cycle. Separate slots
pointing at the same object retain their multiplicity. `shared-container-forward-edges.qtest` covers these
cases, nested shared containers, and both traversal orders.

### Completing collection through shared containers

Deleting one cycle member does not necessarily release an object reference. If that member only held a
shared container, another cycle member can keep the container and all its slots alive. Merely invalidating
the rset and deleting the initiating object would then strand the remaining cycle without a dereference
that could trigger another scan.

For scans which encountered shared containers, `RSet::canDelete()` retains temporary strong references to the other
members in `RSetDerefHelper` before invalidating a collectable set. The initiating object is torn down normally. The helper then releases every
retained reference outside the r-section and rset locks, giving remaining cycles their own collection
opportunity. Ordinary reference-count checks preserve members retained by a user destructor. Cleanup also
runs when a destructor raises a Qore exception. Graphs with unshared containers retain the ordinary cascading
dereference path. Mandatory reference release cannot be cancelled partway
through. This applies to both objects and closure-bound variables.

### `rcount` is a snapshot: `scan_refs` and stale verdicts

`rcount` is computed once, when the rset is built, and `RSet::canDelete()` then compares it against the object's
*live* `references` on every deref. That comparison self-corrects only for references the scan never counted: when
a genuinely external reference goes away, `references` falls to `rcount` and the cycle collects.

It does not self-correct when the graph changes in a way that would make the scan count *more* than it did —
a container in the cycle losing its last holder outside the rset, or an object in a different rset being
collected. Nothing invalidates the rset in those cases, `canDelete()` keeps reading `rcount < references` as a
live external reference, and it returns 0 forever: a plain deref of an object whose rset says "cannot delete"
never triggers another scan (`RObject::deref()` only sets `do_scan` when `rrefs` is 0 and only requests a rescan
when `deferred_scan` was set). The cycle is then stranded for the life of the process.

`RObject::scan_refs` records what `references` was when `rcount` was assigned (`RObject::setRSet()`). When
`canDelete()` finds a member with `rcount != references` *and* that member has lost references since the scan,
the snapshot no longer describes the graph, so it invalidates the rset and asks for a rescan instead of trusting
it. Members whose reference count is unchanged still return 0 immediately, so this costs no extra scans in the
ordinary "something outside still holds it" case, and after a rescan `scan_refs` matches again, so it cannot
loop.

A dereference also saves its triggering reference count as `ref_copy`. Another thread can change the live
count while that dereference waits for a scan. `RSet::canDelete()` rejects superseded snapshots before
examining the set: comparing an old `ref_copy` with each fresh `scan_refs` could otherwise request the same
rescan indefinitely and block every callback worker using that object. A newer reference keeps the object
alive or provides its own collection opportunity when released. This applies to object and captured-variable
dereferences alike.

## When a scan is triggered — and when it may be skipped

`LValueHelper::~LValueHelper` (`lib/Variable.cpp`) runs a scan whenever it holds an lvalue inside an `RObject`
(`robj`, set by `qore_object_private::getLValue` for object members and by `ClosureVarValue::getLValue` for
closure-bound and thread-safe local variables) **and** the lvalue's value needs scanning either before or after
the operation. Note that `obj_chg` is seeded from `before`, so *acquiring* an lvalue whose value already contains
objects triggers a scan even when nothing is assigned — the conservative choice, because objects may have been
removed.

Cost is proportional to the size of the object graph reachable from the lvalue, so a scan on a hot path is
expensive: `RSetHelper::checkIntern` walks every reachable hash, list, object, closure, and reference.

`LValueHelper::suppressObjectScan()` skips that scan. It is only correct when the set of objects reachable from
the lvalue is provably unchanged. The one caller today is complex-reference argument binding in
`QoreTypeSpec::acceptInput` (`lib/QoreTypeInfo.cpp`, `QTS_COMPLEXREF` / `QTS_COMPLEXHARDREF`): binding a
`reference<`*complex-type*`>` argument reads the referenced value and assigns it back to the same lvalue so that
the reference's type restriction is applied (which can fold a container's value type, producing a new node). The
scan is suppressed only when the assignment succeeded *and* the identical node is still in place afterwards —
i.e. nothing was folded, replaced, or removed — so no object can have entered or left the graph.

Do not suppress a scan on the strength of "the value looks the same". Compare node identity, and only after a
successful assignment; suppressing a scan when the graph did change leaves a cycle undetected, i.e. leaked.

## Scan locking: how the scanner stays deadlock-free and convergent

A scan (`RSetHelper`, `lib/RSet.cpp`) walks an arbitrary object graph and needs the r-section of every object
it reaches, in whatever order the traversal finds them. **It never blocks to get one.** Every lock a scan takes
is a try-lock:

- `tryRSectionLockNotifyWaitRead()` (`lib/RSection.cpp`) registers an `RNotifier` against the conflicting owner
  and returns `-1` rather than waiting.
- Pattern B private-data scanners (below) use `trylock()` and skip a contended container.

On a conflict the scan **rolls back** — releasing every r-section it holds — waits on the notifier until the
conflicting owner releases, and starts over. Because a waiting scan holds nothing, scans can never wait on each
other, and no lock ordering has to be maintained across a graph whose shape is only discovered as it is walked.

Two fields on `RObject` make the restart loop terminate:

| Field | Role |
|---|---|
| `rscan` | The scan lock: the TID of the thread scanning this object. One scan per object at a time; other threads wait for it. |
| `rcycle` | The scan generation: incremented by `setRSet()` for every object a scan commits. |

`RScanHelper` samples `rcycle` **before** waiting for `rscan`, and the sampled value is compared with the
current one after **both** waits — after acquiring the scan lock, and after waiting for a conflicting
transaction. A changed generation means another thread has already committed a scan that included this object,
so its recursive set is current.

**A scan of an object must run at most once per generation, on both wait paths.** Skipping a superseded scan is
not an optimization. A scan locks the r-section of every object it reaches, so a redundant scan forces every
other scan in flight that reaches any of those objects to abort and restart. When many threads dereference the
same object — every accepted connection releasing a local that holds a shared server object, for example — the
restarts stop converging: every thread sits in `RNotifier::wait()`, no scan finishes, and the work those
threads were doing never resumes. Native stacks then show many threads parked in `RSetHelper::RSetHelper()`
with nothing running, which reads like a deadlock but is the restart loop failing to converge.

### A scan may be re-entered under a write lock the calling thread already holds

The rollback-and-wait protocol is deadlock-free only while every lock a scan waits on belongs to *another*
thread. One case breaks that: code holding an object's `rml` **write lock** that then runs Qore code which
dereferences an object in the same recursive set. An lvalue operation deleting an object member does exactly
this — the deleted object's destructor releases a reference back into the container's cycle — so the scan
reaches the container, finds its write lock taken and would register a notification against a lock that only
the waiting thread itself can release.

The write lock is strictly stronger than the r-section: it excludes every reader, every writer and every other
r-section holder, so a thread holding it already has the exclusive access the r-section grants.
`tryRSectionLockNotifyWaitRead()` therefore treats `write_tid == q_gettid()` exactly as it treats
`rs_tid == q_gettid()` and grants the r-section directly — the equivalence `checkRSectionExclusive()` already
states. `rSectionUnlock()` suppresses its notification while that write lock is still held, since waiters
cannot acquire the r-section until `qore_var_rwlock_priv::unlock()` releases the write lock and notifies them
itself.

Callers must still not run Qore code under an lvalue lock. `~LValueHelper()` releases its locks *before*
discarding its own temporaries, and `LValueHelper::saveTemp()` exists so that a removed value's dereference —
and any destructor that dereference runs — happens after the locks are gone. Every lvalue path that deletes a
value defers the delete the same way: running a destructor under the lock also deadlocks a plain `rdlock()`
taken by that destructor, which no r-section rule can rescue.

Regression coverage: `examples/test/qore/misc/concurrent-cycle-scans.qtest`,
`examples/test/qore/misc/lvalue-delete-cycle-scan.qtest`, and `ut_dgc_scan_generation()` in
`lib/ql_debug.cpp` for the generation invariant itself.

## Rules for C++ module authors

The platform guarantee holds only if C++ code cooperates. There are two correct patterns.

### Pattern A — "internal_members": store QoreObject refs as DGC-visible members

**Use this when:** your C++ private data holds a pointer to another `QoreObject` (or to another object's private data), and the relationship is one-to-one (single slot, not a collection).

The reference must live in a `private:internal` member of the owning `QoreObject` so that:

1. The scanner sees it and can compute `rcount` correctly.
2. No Qore code can touch it (the member is `private:internal`, external mutation is impossible).
3. Destruction during cycle collection happens in the normal Qore teardown path.

The canonical example is `Qore::StreamReader` (`lib/QC_StreamReader.qpp`):

```cpp
qclass StreamReader [arg=StreamReader* sr; ns=Qore; internal_members=InputStream is];

StreamReader::constructor(Qore::InputStream[InputStream] is, *string encoding) {
    SimpleRefHolder<StreamReader> reader(new StreamReader(xsink, is.release(), ...));
    if (*xsink) return;
    self->setPrivate(CID_STREAMREADER, reader.release());
    qore_object_private* o = qore_object_private::get(*self);
    const qore_class_private* cls = qore_class_private::get(*QC_STREAMREADER);
    o->setValueIntern(cls, "is", static_cast<QoreObject*>(obj_is->refSelf()), xsink);
}
```

What to notice:

- `internal_members=InputStream is` declares a slot the scanner will walk, scoped to this class's `cdmap` entry.
- The C++ priv (`StreamReader* sr`) stores a raw `InputStream*` obtained via `is.release()` — it does *not* own a ref. That ref is held exclusively by the `is` internal member on the parent `QoreObject`.
- `obj_is` is the original `QoreObject` wrapping the `InputStream` (auto-generated by QPP from the `Qore::InputStream[InputStream] is` parameter). `refSelf()` produces the strong ref that's then handed to `setValueIntern`.

When the owning `StreamReader` object is destroyed (cycle or otherwise), its `cdmap[StreamReader]["is"]` slot derefs the wrapped `QoreObject`, which derefs the `InputStream` priv. No manual cleanup; no cycle leak.

**This technique must be used anywhere a C++ data structure maintains a reference to a `QoreObject` or to that object's private data and is itself private data of another `QoreObject`.** That is the scope of this rule — it is not optional. Raw pointers without a corresponding DGC-visible ref are tolerable *only* when the C++ code never retains them past a single synchronous call.

### Pattern B — custom scanner: visit C++ containers of `QoreValue`

**Use this when:** your C++ private data holds an internal collection of values (list, map, queue) that can contain `QoreObject` refs, and it would be impractical to shadow the whole collection in an internal member.

Implement `scanMembers` on the private class; the object scanner dispatches to it.

See `lib/QoreQueue.cpp:509`:

```cpp
bool qore_queue_private::scanMembers(RObject& obj, RSetHelper& rsh) {
    if (l.trylock()) return false;   // non-blocking: skip if contended
    AutoLocker al(l, true);

    QoreQueueNode* w = head;
    while (w) {
        if (w->node.hasNode() && obj.scanCheck(rsh, w->node.getInternalNode())) {
            return true;    // found a cycle edge
        }
        w = w->next;
    }
    return false;
}
```

`TreeMapData::scanMembers` (in `lib/QC_TreeMap.qpp`) follows the same shape.

The dispatcher in `qore_object_private::scanMembers` (`lib/QoreObject.cpp:399-442`) enumerates every known container class and calls its scanner:

```cpp
if (scan_private_data) {
    ReferenceHolder<Queue> q(reinterpret_cast<Queue*>(getScanPrivateData(CID_QUEUE)), &xsink);
    if (*q && qore_queue_private::get(**q)->scanMembers(*this, rsh)) return true;
    ReferenceHolder<TreeMapData> tm(static_cast<TreeMapData*>(getScanPrivateData(CID_TREEMAP)), &xsink);
    if (*tm && tm->scanMembers(*this, rsh)) return true;
}
```

**When you introduce a new C++ container class that can hold `QoreValue` and be stored as private data, you must add a `scanMembers` method and register it in this dispatcher**, and the enclosing object must set `scan_private_data = true` (or override `needsScan` to return true when appropriate).

**Look the private data up with `getScanPrivateData()`, never with a raising lookup such as `getReferencedPrivateData(key, xsink)`.** Most scanned objects do not have the data being probed for, so a raising lookup creates and discards an exception for nearly every object. Creating an exception captures the call stack, and call stack capture calls external language stack location helpers: the Python helper acquires the GIL. The scan holds r-sections at that point, so a thread that holds the GIL and waits for one of those objects (for example a Python thread whose Qore thread initialization dereferences an object) deadlocks with the scan. Nothing in a scan may raise an exception or call into user or foreign-language code.

Failing to do this produces exactly the symptom that motivated this document: a cycle with an "invisible" edge through a C++ container; DGC sees `rcount < references` on some member, calls `canDelete` → returns 0, and the cycle leaks forever.

### Raw pointers across cycles: acceptable cases

Holding a raw `QoreObject*` or `AbstractPrivateData*` without a ref is safe when:

- The pointer is obtained and released within one synchronous call (no storage across return).
- The pointer is guarded by an external lifetime invariant (e.g., "the manager's pool holds the ref; I'm inside a method called from the pool").
- The pointer is paired with a DGC-visible `QoreObject` ref stored elsewhere (Pattern A): the raw pointer is just a fast-access cache of the already-refcounted private data. This is how `Http2ClientPollOperationPriv::connection_priv` is documented (`include/qore/intern/QC_Http2ClientPollOperationBase.h:362-368`): `connection_priv` is raw; the strong ref lives in the `connection` internal member so DGC can walk it.

If you rely on Pattern A's invariant, verify that the internal-member slot is actually populated by the QPP constructor. A raw pointer *without* a corresponding `setValueIntern` call is the shape of a cycle leak.

## `rrefs` / `realRef()`: when to use it

`realRef()`/`realDeref()` bump `rrefs` and declare that a ref is provably not part of any cycle. Use it when a ref is held by runtime machinery that cannot be part of a Qore object graph:

- A background thread owning an object for its method call (`lib/thread.cpp:895`).
- Method-dispatch helpers pinning `self` for the duration of a call (`QoreObjectRealRefHelper` in `include/qore/QoreObject.h:838`).
- Variable moves in the runtime (`lib/Variable.cpp:63`).

While `rrefs > 0`, DGC will not scan the object. Leaking a `realRef()` without a matching `realDeref()` permanently disables cycle detection for that object and any cycle it participates in. Use `QoreObjectRealRefHelper` (RAII) in preference to raw pairs of calls wherever possible.

Do not use `realRef()` for references stored in C++ state that outlives a single call. Those refs belong in internal members (Pattern A) or must be made visible via a custom scanner (Pattern B).

## Debugging a suspected cycle leak

1. **Count survivors.** Instrument `qore_object_private` ctor/dtor with an atexit dump. For each surviving object, also print `references`, `rrefs`, whether `rset` is non-null, and `rcount`. (See `lib/QoreObject.cpp` around the `qo_register`/`qo_unregister` hooks used during the H2-connection-manager leak investigation.)
2. **Find the blocking member.** For any rset whose objects aren't collecting, look for a member where `references > rcount`. That's the object holding an "external" (DGC-invisible) ref.
3. **Trace the ref.** Grep the C++ code that interacts with the leaked class for `->ref()` / `->deref()` pairs. A ref taken on a `QoreObject*` stored as a raw pointer with no corresponding internal-member slot is the bug.
4. **Fix at the C++ layer.** Either move the ref into a `private:internal` member (Pattern A) or provide a `scanMembers` (Pattern B). Never push cycle-breaking responsibility into `.qc` / `.qm` code — that violates the platform guarantee.

## Related files

- `include/qore/intern/RSet.h` — `RObject`, `RSet`, field declarations, `scan_refs`, the container-edge and
  per-container counting memos.
- `include/qore/intern/RSection.h` — r-section lock semantics.
- `lib/RSet.cpp` — `canDelete`, `deref`, `checkDeferScan`, invalidation, and the scanner proper
  (`RSetHelper::checkIntern`: cycle traversal + `rcount` assignment).
- `lib/QoreObject.cpp` — `scanMembers`, `scanMembersIntern`, `customDeref`, the dispatcher that calls private-data scanners.
- `lib/QoreQueue.cpp` — `qore_queue_private::scanMembers` (Pattern B reference implementation).
- `lib/QC_TreeMap.qpp` — TreeMap custom scanner (Pattern B).
- `lib/QC_StreamReader.qpp` — `internal_members` + `setValueIntern` (Pattern A reference implementation).
- `include/qore/QoreObject.h` — `realRef`/`realDeref`, `QoreObjectRealRefHelper`.
- `lib/Variable.cpp`, `lib/thread.cpp`, `lib/FunctionCallNode.cpp` — `realRef` call sites showing legitimate uses.
- `lib/Variable.cpp` — `LValueHelper::~LValueHelper` (scan trigger), `ClosureVarValue::getLValue` (`robj` for
  closure-bound and thread-safe locals).
- `lib/QoreTypeInfo.cpp` — `QoreTypeSpec::acceptInput`, the only `suppressObjectScan()` caller.
- `examples/test/qore/misc/reference-arg-binding.qtest` — cycle-collection and scan-cost regression tests for
  reference argument binding.
- `examples/test/qore/misc/shared-container-cycles.qtest` — cycles reached through a container
  shared with an object outside the recursive set.
