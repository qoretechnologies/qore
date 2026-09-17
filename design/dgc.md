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

The cycle detector (`RSetHelper`, `lib/RSet.cpp`) traverses the graph reachable from a candidate object, divides it into strongly connected components, assigns the objects of each component with a cycle to an `RSet`, and computes each member's `rcount`. Detection runs opportunistically during `customDeref` (`lib/QoreObject.cpp`) when the normal path cannot prove the object is alive.

### The graph

The nodes of the graph are objects and closure-bound variables (both `RObject`s), lists, hashes, closures and
references. Each value held by a node is an edge: an object's members, a container's entries, a closure's captured
variables, a reference's variable and the values of Pattern B private data. The scan is Tarjan's strongly connected
components algorithm with an explicit stack, so a chain of objects or containers of any length is scanned without
recursion. Each node is visited once, and each of its edges is followed once, so a scan takes time linear in the size
of the reachable graph.

A strongly connected component with a cycle — more than one node, or a node that references itself — is a recursive
set. A reference is internal if the node holding it is in the same component as its target: an object's `rcount` is
the number of internal references to it, and the set records the number of internal references to each of its lists,
hashes, closures and references.

## The decision rule (`RSet::canDelete`)

See `RSet::canDelete()` in `lib/RSet.cpp`. When an object's `customDeref` has dropped its ref, we decide whether the whole rset can be torn down:

```
for each object in rset:
    if object.rcount > object.references:  stale — rescan
    if object.rcount != object.references: external ref exists → keep
for each list, hash, closure and reference in rset:
    if node.references < node.internal:    stale — rescan
    if node.references > node.internal:    external ref exists → watch the node, keep
all equal → invalidate rset, collect everything
```

The invariant that drives the entire design is this: **for an rset to be collectable, every node of its component must
have only internal references**. If any node has more references, *something outside the rset* still points at it,
and we must not free the cycle. A list, hash or closure shared with a holder outside the cycle — a local variable, an
object outside the cycle or an event registry — keeps every object reachable through it alive.

That means any ref held by code the scanner cannot see counts as "external" and blocks collection.

## What the scanner can and cannot see

`qore_object_private::scanMembersIntern()` in `lib/QoreObject.cpp` walks the object's data hashes. It iterates:

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

### Watched nodes

Releasing an outside reference to a list, hash, closure or reference in a recursive set does not dereference any
member of the set, so nothing would recheck the set. When `RSet::canDelete()` finds such a node with outside
references, it registers a watch for the node with a member of the set. `AbstractQoreNode::deref()` checks for
watches when a reference to a list, hash, closure or reference is released without freeing it; a global count and a
per-bucket count of watched addresses keep this check to two atomic reads when the node is not watched. When a
watched node has no more references than the set holds, the watch is removed and the member is referenced and
dereferenced, which rechecks the set like any other dereference.

The set keeps a weak reference to each of its non-object nodes, so that `canDelete()` can read their reference counts
and a watched node's address is not reused while the watch exists. Invalidating the set removes its watches and weak
references. The watch registration rereads the reference count after registering, so a reference released in between
is not missed.

The watch holds its member with a weak reference (`RObject::tRef()`), and the recheck can collect that member, so
every `RObject` must stay allocated while it has weak references: an object and a closure-bound variable both release
their initial weak reference when their last reference is released, and are deleted by the last `tDeref()`, never
directly.

Because each list and hash is a node of its own, a container shared by N objects is walked once per scan rather than
once for each object that references it, and its references from outside the cycle are never counted as internal.
`examples/test/qore/misc/dgc-graph-components.qtest` covers both.

### Completing collection through shared containers

Deleting one cycle member does not necessarily release an object reference. If that member only held a
shared container, another cycle member can keep the container and all its slots alive. Merely invalidating
the rset and deleting the initiating object would then strand the remaining cycle without a dereference
that could trigger another scan.

For recursive sets with lists, hashes, closures or references, `RSet::canDelete()` retains temporary strong references
to the other members in `RSetDerefHelper` before invalidating a collectable set. The initiating object is torn down normally. The helper then releases every
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
expensive: `RSetHelper::scan()` walks every reachable hash, list, object, closure, and reference.

`LValueHelper::suppressObjectScan()` skips that scan. It is only correct when the set of objects reachable from
the lvalue is provably unchanged. The one caller of it is complex-reference argument binding in
`QoreTypeSpec::acceptInput` (`lib/QoreTypeInfo.cpp`, `QTS_COMPLEXREF` / `QTS_COMPLEXHARDREF`): binding a
`reference<`*complex-type*`>` argument reads the referenced value and assigns it back to the same lvalue so that
the reference's type restriction is applied (which can fold a container's value type, producing a new node). The
scan is suppressed only when the assignment succeeded *and* the identical node is still in place afterwards —
i.e. nothing was folded, replaced, or removed — so no object can have entered or left the graph.

Do not suppress a scan on the strength of "the value looks the same". Compare node identity, and only after a
successful assignment; suppressing a scan when the graph did change leaves a cycle undetected, i.e. leaked.

### Removing values from a container

`remove` and `delete` of a hash key, object member, list element or slice navigate the `LValueHelper` to the
**container** and take the value out of it, so `before` describes the container, which needs a scan whenever it
holds any object. Without a further rule, removing a key that does not exist, or a scalar, from a hash that also
holds an object scans the whole graph reachable from the object holding the hash. Because the scan holds the
r-section of every object it reaches, and every dereference of those objects waits for it, a large graph turns
this into a process-wide stall.

Every removal path therefore calls `LValueHelper::startContainerRemoval()` right after navigating to the
container: `LValueRemoveHelper::doRemove()` (AST), `LValuePathUnary` in `lib/QoreIRInterpreter.cpp` (IR), and
`qore_rt_lv_path_unary()` in `lib/JITRuntime.cpp` (JIT and AOT, which also covers the shared
`executeLV*Remove()` helpers). It records the container node and, for a list or hash, its scan count. The
destructor skips the scan when `LValueHelper::removalKeptGraph()` shows that the removal cannot have changed the
reachable objects:

- The lvalue still holds the same container node. `ensureUnique()` replaces a shared container with a copy, and
  the copy is a node that no recursive set knows yet, so a copy always scans. The replaced node is held in the
  helper's temporaries until after the check, so its address cannot be reused by then.
- For a list or hash, the scan count is unchanged. Every container removal primitive decrements the count when
  the removed value needs a scan, and only the holder of the lvalue lock can change it, so an unchanged count
  after a pure removal means no scan-relevant value was taken.
- For an object, `qore_object_private::takeMember()` / `takeMembers()` report through
  `LValueHelper::objectRemoved()` whether any removed value needs a scan. An object's scan count cannot be
  used here: other threads can change the object's members through their own lvalues at the same time.

Removing an object, or a container, closure or reference that needs a scan still scans, because dropping an
edge can make a cycle collectable. `delete` needs no special case: deleting an object removes a value that needs
a scan. Any other removal (`lvh.remove()` of the whole value, or an unexpected container type) keeps the
conservative scan.

`QoreTypeSafeReferenceHelper::removeHashObjKey()` (the public C++ API) does not use this rule, since the same
helper can hand out mutable nodes (`getUnique()`) that a module can change without the helper knowing.

Debug builds count the scans started by lvalue operations per thread (`dbg_get_lvalue_scan_count()`);
`examples/test/qore/misc/dgc-remove-scan/dgc-remove-scan.qtest` uses it to check every removal kind in each
execution mode and from a compiled module, along with cycle collection after removals that skipped the scan.

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

A scanner only reports the values; the scan follows them after the scanner has released its lock, when the container
can have changed. `qore_object_private::scanMembers()` therefore reports private data values with
`RSetHeldEdgeHelper` in effect: the scan keeps each reported object with a weak reference, as it checks objects when
it reaches them, and each other node with a strong reference, which also keeps a list or hash from being changed in
place. Each value is held once, also when the scan restarts after a lock conflict, and the references are released
when `RSetHelper` is destroyed: after the scan's locks and after the scan lock of the object that the scan started
at, as releasing the last reference to a container can run destructors that scan that object again.

The dispatcher in `qore_object_private::scanMembers()` in `lib/QoreObject.cpp` enumerates every known container class and calls its scanner:

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

## Deleting long chains

Deleting an object releases the objects it references, and deleting those would recurse for each object in a chain.
`qore_object_private::deleteOrDefer()` deletes objects recursively up to a nesting depth of 16 in a thread and defers
deeper deletions to a loop with an explicit stack. The loop deletes the objects deferred while deleting an object
before the remaining objects deferred earlier, so destructors run in the same depth-first order as with recursion, and
the members retained by a collected recursive set are released after the deferred object's deletion.

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
  (`RSetHelper::scan()`: the component search, and `countInternalReferences()`: `rcount` assignment).
- `lib/QoreObject.cpp` — `scanMembers`, `scanMembersIntern`, `customDeref`, the dispatcher that calls private-data scanners.
- `lib/QoreQueue.cpp` — `qore_queue_private::scanMembers` (Pattern B reference implementation).
- `lib/QC_TreeMap.qpp` — TreeMap custom scanner (Pattern B).
- `lib/QC_StreamReader.qpp` — `internal_members` + `setValueIntern` (Pattern A reference implementation).
- `include/qore/QoreObject.h` — `realRef`/`realDeref`, `QoreObjectRealRefHelper`.
- `lib/Variable.cpp`, `lib/thread.cpp`, `lib/FunctionCallNode.cpp` — `realRef` call sites showing legitimate uses.
- `lib/Variable.cpp` — `LValueHelper::~LValueHelper` (scan trigger), `ClosureVarValue::getLValue` (`robj` for
  closure-bound and thread-safe locals).
- `lib/QoreTypeInfo.cpp` — `QoreTypeSpec::acceptInput`, the only `suppressObjectScan()` caller.
- `lib/Variable.cpp` — `LValueHelper::startContainerRemoval()` / `removalKeptGraph()`, the removal scan rule.
- `examples/test/qore/misc/dgc-remove-scan/dgc-remove-scan.qtest` — scan counts and cycle collection for
  `remove` and `delete` in every engine.
- `examples/test/qore/misc/reference-arg-binding.qtest` — cycle-collection and scan-cost regression tests for
  reference argument binding.
- `examples/test/qore/misc/shared-container-cycles.qtest` — cycles reached through a container
  shared with an object outside the recursive set.
- `examples/test/qore/misc/dgc-graph-components.qtest` — cycles held through lists, objects, closures and queues
  from outside, long cycles and chains in a thread with a small stack, and the scan time of shared containers.
- `examples/test/qore/misc/shared-container-dense-cycles.qtest` — dense graphs sharing one container.
