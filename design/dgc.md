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
| `rclosed` | The object's `RSet` while that set is **closed** (nothing in it references anything outside it), otherwise null. A scan that reaches the object from outside the set does not enter it. |
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

The frame that creates a closure-bound local variable holds a real reference to it until the variable goes out of
scope (`ThreadClosureVariableStack::instantiate()` / `releaseEntry()`), since a frame is never part of a cycle. A
write to the variable through a `\var` argument, or while a closure holds it, therefore defers its scan to the
frame's release; see `closure-bound-locals.md`. For the same reason the frame lends the object the variable holds
the real reference a plain local variable gives its object (`QoreLValue::closure_lent`, set by
`ClosureVarValue::lendFrameReference()` and cleared by `revokeFrameReference()` when the frame releases the
variable), so a write to that object - by the frame, through a `\var` argument, or by another thread through a
closure - is deferred as well.

A deferred scan is replayed by the first dereference that finds no real reference left while other references
remain. A constructor that writes to its members defers such a scan on the new object (its `self` holds a real
reference), so the new object must not be dereferenced again after it has been linked into a graph, or the replay
walks that whole graph: a registry adding one entry per operation would walk the registry each time. The AST
interpreter hands an assigned value over to the lvalue; the compiled tiers do the same for an lvalue path when the
value is not used again (`qore_rt_lv_path_assign_consume()` / `qore_rt_self_member_assign_consume()` in
`lib/JITRuntime.cpp`, the IR interpreter's `LValuePathAssign`): the value's reference passes to the assignment,
which owns it on failure as well, and a statement-form assignment has no result, so no reference is taken for one.
The value is handed over only when it has a cleanup slot of its own, this is its only use, it is not a weak-reference
load, and it is defined in the same block as the assignment - the use counts are static, so a value defined before a
loop and used once in its body would otherwise be handed over on the first iteration.
`examples/test/ir/lvalue-assign-ownership/lvalue-assign-ownership.qtest` checks that every tier releases values
exactly as the AST interpreter does.

A scan initiated at another object still traverses a reachable object's members under its r-section lock,
including when that object has real references. Skipping those edges can omit a live owner of a shared
container from a recursive set: the container's one physical reference to an object behind it can then look
entirely internal, and the smaller set would be collected prematurely. Including the owner completes the
cycle, and its real references keep `references > rcount`, preventing collection.

**A deferred scan marks the recursive set recorded for the object stale, every time it is deferred.** Because a
scan rooted elsewhere assigns a set to an object that has real references, a set can be attached between one
deferred scan and the next, and it describes the graph as it was when that scan ran: a hub whose container is grown
one entry at a time, while the hub is held by a real reference for each of those mutations, would otherwise keep
the `rcount` it was given when the container was shortest, and `RSet::canDelete()` would read the difference as a
live reference from outside the set for the life of the object. A removal under a deferred root is the other
direction: the set still counts the removed reference as internal, and without a rule a dereference would find the
references left from outside equal to the stale `rcount`s and collect objects that are still held.

`checkDeferScan()` therefore marks the set **stale** (`RObject::markRSetStale()`, `RSet::markStale()`) and clears
its closed mark, so that scans started elsewhere enter it again. `canDelete()` keeps a stale set - it neither
collects it nor rescans it - until the deferred scan is made: `qore_object_private::customDeref()` makes it when
the object's last real reference goes and its set is still stale, and a closure-bound variable's dereference does
the same. That scan, or any other scan that walks the set in the meantime, confirms the set in place when the
components are the ones recorded (clearing the mark, without a new set or the exclusive r-section of the graph)
or replaces it. A set that a scan started elsewhere has recorded since the deferral already describes the graph
after the change, so the deferred scan is not made for it. `rref_wait` counts the marks in flight, because more
than one thread can be making one at a time and the real references may only reach zero once the last has
finished, so the deferred scan always finds the mark in place.

The set used to be discarded on every deferral instead, which cost a new set - and the exclusive r-section of the
whole graph - each time: a plain local variable holding a cycle rebuilt its set when released, and the
server-controller shape in `dgc-scan-avoidance` walked half as much again (4,500 objects for 100 requests, now
3,000). `examples/test/qore/misc/dgc-deferred-scan-sets.qtest` covers the grown containers and the removals, for
list and hash containers.

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

### Knowing that a set's counts are current

`rcount` is computed once, when the rset is built, and `RSet::canDelete()` then compares it against the object's
*live* `references` on every deref. A member with `references > rcount` is held from outside the set, and the set
is kept. That verdict is only as good as the counts, and the graph can change after the scan without a new one:

- **An edge removed** between members — a container in the cycle losing its last holder outside the set, an
  object in a different set being collected, a value taken out of a C++ container. The true number of internal
  references is then *lower* than `rcount`, so the true number of outside references is *higher* than the one
  `canDelete()` reads: a verdict of "cannot delete" stays right. When the outside references go, `references`
  falls below `rcount` and `canDelete()` asks for a rescan (`ref_copy < rcount`, or a member with
  `rcount > references`).
- **An edge added** between members without a scan following it. The new reference is not in `rcount`, so it looks
  like one from outside, and "cannot delete" would stand for the life of the process: a plain deref of an object
  whose rset says "cannot delete" never triggers another scan.

Only the second can strand a set, and edges are added in few places. A write through an lvalue, and the C++ member
setters (`qore_object_private::setValueIntern()`, `merge()`), scan the object they wrote. Containers whose values
the scanner reports through private data (Pattern B below) store values with no scan. Each object therefore
carries a mutation generation:

- `RObject::edge_gen` is advanced by `RObject::edgesAdded()` wherever an edge may be added without a scan having
  followed it yet: by the scanning writers just before their scan (`~LValueHelper`, `setValueIntern()`, `merge()`),
  and by `qore_dgc_value_stored()`, which Pattern B containers call when they store a value that needs a scan. It
  marks the holder and every object and closure-bound variable that the value reaches without passing through an
  object, since the new edge can join the holder's component to theirs.
- A scan reads each object's generation before it follows the object's edges (`RSetHelper::startNode()`), and the
  commit records it as `RObject::scan_edge_gen` whether it assigns a new set or confirms the one in place. An edge
  added while the scan runs therefore leaves the two different even if the scan did not see it.
- A reference stored in such a container refers to a variable or object that is only found by evaluating it, so
  it advances `RSet::untracked_edge_epoch` instead; each set records the epoch its scan read, and a set whose
  epoch is behind is not trusted either.

`RSet::edgesUnchangedSinceScan()` is true when every member's generation is the recorded one and the epoch has
not moved. `RSet::keepNeedsRescan()` is what `canDelete()` asks before returning "cannot delete" — for the
triggering object, for a member, and for a list, hash, closure or reference in the set held from outside it: a
set with an edge added since its scan is rescanned first. The rescan records the current generations, so the set
is trusted again after it. A dereference that has rescanned once (and did scan, rather than finding that another
thread just had: `RSetHelper::skipped()`) trusts the set afterwards, so it cannot loop while other threads keep
adding edges; the mark stays for the next dereference.

This replaced a reference-count snapshot (`scan_refs`) that asked for a rescan whenever a member had lost references
since the scan. It could not tell an outside holder letting go — which leaves the counts exact — from an internal
edge that had gone, so every outside holder of a set member that let go rescanned the whole set: ten requests
releasing a shared controller in a 103-object graph walked it ten times, and object destruction in a server was
dominated by those scans. It also only noticed an added edge by chance, when a member happened to lose a reference
afterwards. `examples/test/qore/misc/dgc-scan-avoidance/dgc-scan-avoidance.qtest` has the shapes: outside holders
releasing a set rescan nothing (`release`), a set marked by a TreeMap is rescanned once (`marked`), cycles closed
only by values or references pushed into Queues are collected (`queue-cycle`, `queue-ref`), and threads pushing
members into a set while others release it (`concurrent marks`).

A dereference also saves its triggering reference count as `ref_copy`. Another thread can change the live
count while that dereference waits for a scan. `RSet::canDelete()` rejects superseded snapshots before
examining the set: a newer reference keeps the object alive or provides its own collection opportunity when
released. This applies to object and captured-variable dereferences alike.

## When a scan is triggered — and when it may be skipped

`LValueHelper::~LValueHelper` (`lib/Variable.cpp`) runs a scan whenever it holds an lvalue inside an `RObject`
(`robj`, set by `qore_object_private::getLValue` for object members and by `ClosureVarValue::getLValue` for
closure-bound and thread-safe local variables) **and** the lvalue's value needs scanning either before or after
the operation. Note that `obj_chg` is seeded from `before`, so *acquiring* an lvalue whose value already contains
objects triggers a scan even when nothing is assigned — the conservative choice, because objects may have been
removed.

Cost is proportional to the size of the object graph reachable from the lvalue, so a scan on a hot path is
expensive: `RSetHelper::scan()` walks every reachable hash, list, object, closure, and reference.

A closure-bound local variable that only its frame can use (`ClosureVarValue::frameExclusive()`: the frame holds its
only reference, and it is in no recursive set) is not made the scan root: nothing on the heap refers to it, so a
change to its value cannot close a cycle through it, and the write that later stores a reference or a closure to it
scans from the object or container written. See `closure-bound-locals.md`.

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

A removal navigates to the container, so the `RObject` that `robj` holds is whatever the path passed through
*before* the container — which is not necessarily the object the value is removed from, and for an object
reached as the value of a hash key or list element (`remove h.x.a`) there is none at all. Without a root there
is no scan, and the object's recursive set keeps counting a reference that no longer exists, which lets a later
dereference collect the set while its objects are still held from outside it.
`qore_object_private::takeMember()` and `takeMembers()` therefore report the object with
`LValueHelper::objectRemoved()`, which makes it the scan root when the path supplies none.

When the path does supply a root, its scan enters the object and repairs the set - unless that scan is deferred
because the root has real references (see "The rrefs deferral"): `remove c.x.peer` in a method of the object holding
`c`, or through a `\var` argument while the frame holds the variable. The deferred scan is made from the root only
when its last real reference is released. Meanwhile the set of `x` still counts the removed reference as internal,
so the dereference of the removed value, or of a member of the set, compares the references left from outside the
set - the hash still holding `x` and its peer - with the stale `rcount`s, finds them equal, and collects objects that
are still referenced. `objectRemoved()` therefore also records any object other than the root that lost a value
needing a scan (holding a weak reference to it), and `~LValueHelper` scans from each of them when
`RSetHelper::deferred()` reports that the root's scan was deferred. An assignment needs no such rule: its root is the
innermost `RObject` on the path, the one whose member changed, and deferring its scan invalidates that object's own
set. `examples/test/qore/misc/dgc-deferred-scan-sets.qtest` covers member and slice removals and a removal that
leaves a smaller cycle.

Removing an object, or a container, closure or reference that needs a scan still scans, because dropping an
edge can make a cycle collectable. `delete` needs no special case: deleting an object removes a value that needs
a scan. Any other removal (`lvh.remove()` of the whole value, or an unexpected container type) keeps the
conservative scan.

`QoreTypeSafeReferenceHelper::removeHashObjKey()`, the public C++ API, applies the same rule. The same helper can
also hand out the value for changes it cannot see (`getUnique()`, and `qore_type_safe_ref_helper_priv_t::get()`)
or change it in other ways (`assign()`, `setObjKey()`, `moveToHashObjKey()`). Each of these calls
`LValueHelper::disableRemovalScanSkip()`, so a helper that was used for anything but removals always scans.

Debug builds count the scans started by lvalue operations per thread (`dbg_get_lvalue_scan_count()`), and
`dbg_ref_remove_key()` / `dbg_ref_set_unique_remove_key()` call the public reference helper API;
`examples/test/qore/misc/dgc-remove-scan/dgc-remove-scan.qtest` uses it to check every removal kind in each
execution mode and from a compiled module, along with cycle collection after removals that skipped the scan.

### Closed recursive sets: the region a scan does not enter

A scan costs time linear in the graph reachable from its root, so a short-lived object that merely references a
long-lived one makes every one of its dereferences walk the whole long-lived graph. Nearly all of that work
re-derives a result an earlier scan already committed.

A recursive set is **closed** when every reference held by every node of its component — its objects,
closure-bound variables, lists, hashes, closures and references — points at another node of the same component.
Nothing in a closed set can reach a node outside it, so no cycle through it can include a node that a scan
started elsewhere is examining, and its membership and `rcount`s are already computed. A scan that reaches one
of its objects therefore records the reference as leaving the scanned graph and does not follow it
(`RSetHelper::checkNode(RObject&)`), which is also what keeps the *referring* component from being called
closed itself. `RSetHelper::findClosedComponents()` decides this at commit time and `RObject::setRSet()` records
the set in `rclosed`, which is read without a lock and only ever compared, never dereferenced. The scan always
enters the set it started in (`root_rset`): that set is the one being recalculated.

**The invariant that makes this safe: any change to a node of a closed set takes the set out of the closed
state.** Three rules establish it:

- An assignment through an lvalue runs its scan with `robj` set to the innermost `RObject` on the path, i.e. the
  object or closure-bound variable whose member changed, so the scan enters that object's set. A list or hash
  node of the set is reached only through such an `RObject`: any other holder gives the container a second
  reference, and `ensureUnique()` then copies it rather than changing the set's node.
- A removal's root is not necessarily in the set it changes (see above), so
  `qore_object_private::takeMember()` / `takeMembers()` call `RObject::clearRSetClosed()` whenever they take out
  a value that needs a scan.
- Values in a private data container (Pattern B below) are changed by the container's own methods, which make no
  scan of the object at all, so `RObject::valuesCanChangeWithoutScan()` keeps a set containing such an object
  from ever being marked closed.

Invalidating a set clears the mark for every member (`RSet::invalidateIntern()`), and so does marking it stale
when a scan of a member is deferred (`RObject::markRSetStale()`), so a set that is rescanned, collected, torn down
or changed under a deferred scan is never left marked. Nothing else may mark a set closed: a set whose mark outlives a
change to one of its nodes describes a graph that no longer exists, and the cycles through it are then never
found.

Debug builds count the objects that scans have entered in the current thread
(`dbg_get_scan_object_count()`), which measures what a scan actually walked;
`examples/test/qore/misc/dgc-closed-sets.qtest` uses it along with collection checks for each of the rules
above.

### A scan that finds the set already in place leaves it alone

A scan that cannot skip a recursive set still usually finds exactly the set that is already there. Replacing it
with an identical one is not free: `RSet::invalidateIntern()` releases the weak reference the set holds to every
member and removes the watches of its nodes, a new `RSet` is allocated and every member's weak reference and
each node's watch are taken again, and every object the scan entered is written. The last part is what costs
under load, because it makes every scan a writer of every object it reached.

`RSetHelper::findUnchangedComponents()` compares each component with the set its objects already have: the same
set for every object of the component, the same `rcount` for each of them, the same closed state, and the same
lists, hashes, closures and references with the same internal counts. When they match, `commit()` creates no set
and calls `RObject::confirmRSet()` in place of `setRSet()`: the set, its watches and `rcount` are left alone and
only the scan generation advances, so a scan waiting for the object still sees that its set is current. An
object in a component without a cycle that already has no set is left alone the same way.

`confirmRSet()` does refresh the reference count that `rcount` was computed against, and only when it has moved.
`RSet::canDelete()` compares a member's live reference count with that snapshot to tell a stale verdict from a
genuine reference from outside the set, and a snapshot left behind by an earlier scan makes that test read a
graph that no longer exists.

`prepareCommit()` skips a component that is left in place: the members of a set that is not being replaced do
not have to be locked for an invalidation that will not happen. When *no* component changed, `commit()` has
nothing to invalidate, create or replace, so it confirms and releases each object in one pass instead of the
four it makes when it has sets to assign.

**Only the object a scan started at advances its scan generation when nothing changed.** `RScanHelper` samples
the generation of the scan's own root and of no other object, so that is the only one a waiting scan reads. A
scan that changed something still advances the generation of every object it entered — that is what stops the
scans waiting on those objects from repeating work and making every scan in flight restart, and such a scan
holds the r-section exclusively anyway. A scan that changed nothing conflicts with no other scan, so writing
the generation of every object of a shared graph, in every scan, would buy only the occasional avoided rescan
while costing a contended cache line per object per scan. That write was 21% of an eight-thread read-only
scan before it was removed.

Debug builds count the recursive sets that scans have created (`dbg_get_rset_create_count()`), and
`examples/test/qore/misc/dgc-unchanged-sets.qtest` uses it to check that a second scan of an unchanged graph
creates none, that a new cycle still gets one, and that a set left in place is still collected and keeps the
watch of a node held from outside it.

## Scan locking: how the scanner stays deadlock-free and convergent

A scan (`RSetHelper`, `lib/RSet.cpp`) walks an arbitrary object graph and needs the r-section of every object
it reaches, in whatever order the traversal finds them. **It never blocks for a lock another thread could in
turn be waiting on** (it does wait for shared holders, which cannot wait; see below). Every lock a scan takes
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

### Every abandoned pass registers the notification its retry waits on

The restart loop in `RSetHelper::RSetHelper()` is deliberately unbounded: a scan that loses a race has to keep
trying until it wins. What makes it terminate is not a retry limit but `RNotifier::wait()`, which blocks until
the owner of the lock the pass could not take releases it. Every path that abandons a pass therefore goes
through `tryRSectionLockNotifyWaitRead()` or `tryRSectionLockSharedNotifyWaitRead()`, which register that
notification before returning `-1`: `RSetHelper::getNode()` when it cannot enter an object's r-section, and
`RSetHelper::removeInvalidate()` when it cannot lock a member of a set it has to replace.

**A pass must never ask to be retried without registering a notification.** Such a retry has nothing to wait
for, so `notifier.wait()` returns at once and the loop becomes a busy spin: the graph is walked from scratch,
abandoned at the same point, and walked again, at 100% CPU, forever. Nothing is written and no lock is held,
so from outside the process this is indistinguishable from a slow compile or a long-running script — a single
`qcc` invocation in this state consumed seven hours of CPU before it was noticed. `RSetHelper::retry_notified`
records that the notification was registered — it is not read back from the notifier, because the owner may
release the lock and clear the notification before the constructor looks at it — and the constructor asserts
it on every retry, so a new abandon path that forgets the notification fails immediately in a debug build
instead of hanging.

### Scans that change nothing share the r-section

A scan is a reader of the graph: it needs the objects it walks to hold still, not to have them to itself. Only
a scan that has to assign a recursive set needs the r-section exclusively, and after the previous section most
scans assign none.

`tryRSectionLockSharedNotifyWaitRead()` takes the r-section alongside other scans while still excluding
writers and exclusive holders, so threads scanning the same unchanged graph proceed at the same time instead of
aborting one another. When `findUnchangedComponents()` reports a component the scan would have to change, the
pass has recorded nothing: the scan rolls back and starts over with `exclusive` set, which is the protocol
exactly as it was before.

**Which mode a scan starts in is a prediction, because guessing wrong costs a second walk of the graph.**
`RObject::scan_wrote` records whether the last scan rooted at that object had to assign a set, and the next
scan started there uses it: an object whose graph has settled keeps finding the same sets, and an object that
is having a cycle built around it keeps changing them. It is true to begin with, because the first scan of a
new object nearly always assigns a set — which is exactly the case where starting shared would walk everything
twice.

Two rules keep the writing pass from being starved:

- An exclusive acquisition **waits** when the only conflict is shared holders, instead of registering a
  notification. That wait always ends: shared holders are scans, a scan never blocks (it releases every lock it
  holds before waiting on its notifier) and never takes an `RSet` write lock, so it cannot be waiting for
  anything the waiting scan holds. A conflict with a writer or with another exclusive holder still rolls back
  and waits on the notifier, because those can hold locks that the scan would have to wait for.
- While any thread waits for the r-section (`rsection_waiting`), `tryRSectionLockSharedNotifyWaitRead()` admits
  no new scan. Without this, a stream of read-only scans could keep both a writing scan and
  `qore_object_private::customDeref()` out of the r-section indefinitely, and collection would stop.

`RObject::scan_edge_gen` is atomic because two scans in shared mode can confirm the same set at once, and the
assertions that a scan holds the r-section of the object it is walking accept either mode
(`RSectionLock::checkRSectionHeld()`).

`examples/test/qore/misc/concurrent-cycle-scans.qtest` runs read-only scans beside scans that change the graph;
starvation there shows up as a test case that never returns.

### A dereference that has nothing to decide takes no lock

Almost every dereference of an object has no collection decision to make. The object is in no recursive set,
so there is no set to consult and nothing to collect, and the dereference returns having done nothing. That
outcome is the overwhelmingly common one: the reference a method call frame holds on `self` is released this
way on every call.

`qore_object_private::customDeref()` used to reach that outcome through the **exclusive** r-section. It took
the read lock, upgraded to the r-section with `QoreSafeRSectionReadLocker::acquireRSection()`, read `rset` and
`rcount`, and returned. The ordering was inverted: the lock was taken in order to read the state that decides
whether the lock was needed. Because `upgradeReadToRSection()` waits while `rs_tid != -1 || rs_shared`, every
thread calling *any* method on an object shared between threads serialized on that object — whatever the
object holds, whatever the method does and whether or not a scan was ever needed. Throughput on a shared
object peaked at four threads and went negative beyond it, while the same workload on a static method scaled.
This is the singleton, manager, registry and cached-table shape. `ClosureVarValue::deref()` had the same
shape for a variable captured by more than one closure, and took the r-section exclusively from the start.

Both now decide on a plain atomic load of `rset`:

- `RObject::rset` is `std::atomic<RSet*>`. It is still written only under the r-section held exclusively, by
  `setRSet()` and `removeInvalidateRSetIntern()`.
- **A null `rset` implies `rcount == 0`.** `setRSet()` is the only function that assigns a non-zero `rcount`,
  and it is passed 0 whenever the set is null (asserted there); `removeInvalidateRSetIntern()` clears both. Each
  publishes `rset` last, with release ordering, so a dereference that reads the pointer with acquire ordering
  never pairs it with the other value's previous state.
- A dereference that reaches the decision has already established `ref_copy != 0`, so `rcount != ref_copy`
  follows from a null set and the comparison never has to be made. The only other input is the deferred-scan
  flag, which `RObject::deref()` captured under `rlck` before any of this.

So a null set plus no deferred scan means "nothing to do", and that is read with no lock at all. Anything
else takes the read lock and the exclusive r-section exactly as before, and reads the state again there.

### Releasing the reference without `rlck`

Deciding that there is nothing to do is only half of it: releasing the reference used to take the object's `rlck`
mutex three times per method call (`QoreObject::customRef()`, then `RObject::deref()` and `RObject::derefDone()`),
plus two atomic operations on the weak reference count for the guard at the top of `customDeref()`. On a shared
object that mutex is one more lock every calling thread contends for. `RObject::tryFastDeref()` now does the whole
dereference with no lock:

- It decides **before** releasing the reference: no recursive set (`rset`), and no deferred scan due - a deferred scan
  is only made by a dereference that finds no real reference left, so `deferred_scan` matters only while `rrefs` is
  zero. Those are exactly the cases in which the locking path does nothing.
- It then releases the reference with one atomic decrement. If other references remain it returns, and it never
  touches the object again, so it does not register as a dereference in progress (`ref_inprogress`): the only reason
  to register is to make a deleting thread wait for a dereference that still uses the object.
- If it released the **last** reference, the thread becomes the deleter: `robject_dereference_helper`'s released-last
  constructor registers it under `rlck`, and the deletion waits for other threads' dereferences in progress as
  before. Those registered in the same critical section in which they released their references, before this one
  reached zero, so taking `rlck` orders it after them.
- A **real** reference is released there only while other real references remain (a compare-and-swap that never
  takes `rrefs` to zero). The last real reference goes through `rlck`, because it has to wait for rset
  invalidations in progress (`rref_wait`) and may have to make the deferred scan. `derefRealIntern()` decrements
  with a compare-and-swap too, so a concurrent lock-free decrement cannot make it the one that reaches zero without
  having waited.
- Taking a reference (`customRef()`, `realRef()`, `startCall()`, `ClosureVarValue::ref()`) is a plain atomic
  increment. Marking an object held in a local variable as a real reference (`setRealReference()`, which every
  method call does for `self`) is one too, and unmarking it (`unsetRealReference()`) follows the rule above: a
  compare-and-swap while other real references remain, `rlck` and `derefRealIntern()` for the last one. `qore_dgc_node_dereferenced()`, which takes a reference only if the member still has one, does it as a
  compare-and-swap now that references can reach zero without `rlck`.
- The weak-reference guard in `customDeref()` is only taken on the locking path, which is where scans can cascade
  back into the object.

Taking and releasing the reference that a method call holds on a shared object in no cycle therefore costs one
atomic increment and one atomic decrement of its reference count (two of each for a real reference) and a few plain
loads.

This is safe because the r-section never ordered a dereference against a scan that starts *after* it, only
against one already in progress. A dereference that took the r-section first saw the same null set that the
lock-free load sees, and the scan committed its set afterwards either way. The error in the other direction
cannot happen: a stale *non-null* read only costs one unnecessary upgrade, and the slow path re-reads
everything under the r-section. The fast path may only ever decline to skip, never skip wrongly.

Debug builds count the r-section acquisitions made by dereferences per thread
(`dbg_get_deref_rsection_count()`);
`examples/test/qore/misc/dgc-deref-fast-path.qtest` asserts that method calls on a shared object outside a
cycle take none, that an object inside one still does, and that cycles built around an object and through a
captured variable are still collected.


### A dereference keeps its object allocated until it returns

`RObject::derefDone()` makes a deleting dereference wait for the dereferences of other threads, but not for the
ones its own thread has in progress further up the stack (`deref_inprogress_own()` in `lib/RSet.cpp`): a
dereference re-entered on the same thread cannot finish until the inner one returns, so waiting for it would never
end. A dereference therefore cannot rely on its registration to keep the object allocated. Once it has released its
own reference, the only thing keeping the object alive is somebody else's reference, and on the same thread that
reference can go away underneath it:

- the scan it starts holds references on the graph it walks and releases them when it ends
  (`RSetHelper::releaseHeld()`), and releasing them can dereference this very object;
- another thread can release the object's other references meanwhile, so that the release on this thread is the
  last one, the object is deleted, its last weak reference goes away and its memory is freed while the outer
  dereference still goes on to take its r-section.

Both dereferences of an `RObject` therefore hold a weak reference on their own object for their whole duration:
`qore_object_private::customDeref()` and `ClosureVarValue::deref()`. A weak reference keeps the object allocated,
not alive, so this changes nothing about when it is deleted or when its value is released. The closure-bound
variable's dereference was missing it: a closure that captured a variable and ran on a handler thread released the
variable's other reference while the frame's release of its own was rescanning, and the frame took the r-section of a
freed variable (AOT-compiled `HttpServerAsyncIo` on arm64 macOS; `ut_closure_var_deref_rescan_reentry()` in
`lib/ql_debug.cpp` reproduces it deterministically with a debug hook).

In debug builds `RObject::tDeref()` asserts that it never frees an object with a dereference in progress on the same
thread (`qore_robject_deref_inprogress_on_this_thread()`), so a dereference path without the guard fails at the free
rather than at a later use of the freed memory.

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

**Every time such a container stores a value that needs a scan (`needs_scan()`), it must call
`qore_dgc_value_stored(holder, value)`** with the object whose private data it is. No scan follows the store, so
the recursive sets the new edge changes do not count it, and without the mark a verdict of "cannot delete" made
with those counts can stand for the life of the process; see "Knowing that a set's counts are current". Call it
after the value is in the container and while the value is still referenced: `Queue::push()` calls it under the
queue's lock, since another thread can take the value as soon as the lock is released. Removing a value needs no
call. `TreeMapData::put()`, `qore_queue_private::push()` / `insert()` and
`Http2ClientPollOperationBase::registerStreamQueue()` are the current callers.

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

## Opaque references: a strong reference the scanner does not follow

The `@=` operator (the **opaque reference assignment operator**, gated by
`QoreParseOptions::ALLOW_OPAQUE_REFERENCES` / `%allow-opaque-references`) stores a reference that is
**owned** like an ordinary one but that a scan does not follow. It exists because a scan rooted at a
hub walks the hub's whole container graph: registering one connection in a 2,000-entry registry walked
6,005 objects and cost 162 ops/s on one thread. Holding the registry's entries with `:=` did not fix
that — it stayed linear, because the entry hashes are themselves graph nodes — and `:=` would also
make the holder's own entries collectable while it still uses them.

The two primitives are opposite trades:

| | `:=` (weak) | `@=` (opaque) |
|---|---|---|
| owns the target | no | **yes** |
| scanner follows it | no | no |
| failure mode when misused | the target is collected while still in use (dangling) | the cycle is never collected (leak) |

`@=` is therefore strictly safer than `:=` — a mistake leaks rather than dangles — but the leak is
**unrecoverable by cycle detection**. Use it only where the assigned value can be shown not to
reference its holder back.

### Representation

An opaque reference is stored inline in the `QoreValue` NaN-boxed encoding under the 12-bit tag family
`0xFFD` (`TAG12_OPAQUE`), with the kind — object, hash or list — in bits 48-51 and the target pointer
in bits 0-47. It costs no allocation, unlike the `WeakReferenceNode` wrapper that `:=` uses, and it is
carved out of the double range by one extra comparison in `QoreValue::isFloat()`, exactly as inline
short strings are.

The representation is **transparent to every consumer except the collector**: `getType()`,
`getTypeName()`, `getInternalNode()` and `isPointer()` all report the target, so ordinary code,
serialization, pseudo-methods and binary modules treat an opaque reference exactly like a normal one
and need no changes. Only four places look at the tag, and they are the four that decide whether the
collector follows an edge:

- `needs_scan(const QoreValue&)` — `lib/AbstractQoreNode.cpp`
- the list and hash traversals in `RSetHelper::startNode()` — `lib/RSet.cpp`
- `qore_object_private::scanMembersIntern()` — `lib/QoreObject.cpp`

This is the whole reason the traversal sites take a `QoreValue` rather than a raw `AbstractQoreNode*`:
the tag is still in hand where the decision is made. **A new traversal site must check `isOpaque()`,
or it will follow opaque edges and silently undo the feature** — silently, because following the edge
is not incorrect, merely slow, so nothing fails.

### Ownership and Program tracking

`QoreValue`'s reference-counting paths treat an opaque reference exactly like a pointer value, which
`isPointer()` returning true for it gives for free. The one addition is that every strong reference
held through an opaque value is registered with the target's owning `QoreProgram`
(`qore_program_private::registerOpaqueTarget()`), balanced in `ref()`, `discard()` and
`takeNodeIntern()`. Registration counts **references, not assignments**: each copy takes its own
reference, so each copy must take its own registration, or the first release would untrack a target
other copies still hold.

`qore_program_private::clearOpaqueTargets()` then deletes every still-registered target during
`waitForTerminationAndClear()`, next to `clearSavedObjects()` and for the same reason: deleting a
target releases its members and breaks the cycle the collector could not see, and it has to happen
while the Program is still valid (`ptid == tid`, data not yet cleared) because the destructors it runs
are user code. This is what bounds the leak an opaque reference can cause to the lifetime of the
Program that created it instead of the lifetime of the process.

Every copy and every release of an opaque value updates its target's entry, from whichever thread makes it, so
the registry is striped: `qore_program_private_base::opaque_shards` holds 64 shards, each with its own lock and
map, and a target's entry always lives in the shard selected by a hash of its address, so all updates of one
target's count are made under one lock. Each shard lock is a **leaf lock**: nothing else may be acquired while it
is held — another shard's lock included — and no user code runs under it. `clearOpaqueTargets()` takes the
Program's entries out of each shard in turn, one lock at a time, and deletes nothing until it has released the
last one. `examples/test/qore/misc/dgc-opaque-registry-concurrency.qtest` copies and releases opaque values from
eight threads, including while the owning Program is torn down.

### Rules

4. An opaque reference is invisible to the scanner. A cycle running through one cannot be collected by
   cycle detection; it is broken only by the holder releasing it, or by Program teardown.
5. `@=` is only correct where the programmer can show the target does not reference the holder back,
   or where the leak is bounded and intended.
6. `QoreParseOptions::ALLOW_OPAQUE_REFERENCES` is a positive option (`QoreParseOptions::POSITIVE_OPTIONS`): a
   Program whose parse options are locked cannot give it to itself — not with the directive,
   `setParseOptions()` or the Program constructor — and only receives it from the Program that creates it. The
   legacy integer mask `PO_POSITIVE_OPTIONS` cannot hold an extended option, so code enforcing the rule must use
   the full-width mask; `examples/test/qore/classes/Program/positive-options.qtest` checks every positive option.
   A module's Program is not locked, so a module may enable the option for its own code.

## Debugging a suspected cycle leak

1. **Count survivors.** Instrument `qore_object_private` ctor/dtor with an atexit dump. For each surviving object, also print `references`, `rrefs`, whether `rset` is non-null, and `rcount`. (See `lib/QoreObject.cpp` around the `qo_register`/`qo_unregister` hooks used during the H2-connection-manager leak investigation.)
2. **Find the blocking member.** For any rset whose objects aren't collecting, look for a member where `references > rcount`. That's the object holding an "external" (DGC-invisible) ref.
3. **Trace the ref.** Grep the C++ code that interacts with the leaked class for `->ref()` / `->deref()` pairs. A ref taken on a `QoreObject*` stored as a raw pointer with no corresponding internal-member slot is the bug.
4. **Fix at the C++ layer.** Either move the ref into a `private:internal` member (Pattern A) or provide a `scanMembers` (Pattern B). Never push cycle-breaking responsibility into `.qc` / `.qm` code — that violates the platform guarantee.

## Diagnosing scan contention

Under load, threads parked in `RSetHelper::RSetHelper()` say only that scans are waiting. Two diagnostics say for
which objects, how much of the graph they walk and whom they wait for.

**Scan accounting** (`QORE_SCAN_STATS=<path>`, read once at startup, so it is enabled for the whole process or not
at all; when off, a scan costs one branch on a constant and a node one more). Each scan is accounted by the name of
its root — an object's class name or a closure-bound variable's name — in a fixed table of 1,024 rows claimed
without a lock (the last row, `<other>`, collects every root that does not fit). A scan accumulates its counts
locally and adds them with relaxed atomic operations when it ends, so accounting adds no lock and no shared write
per node. The columns: scans that ran; scans deferred because the root had real references; scans skipped because
another thread had just scanned the root; passes abandoned on a lock conflict (`restarts`); passes restarted to
take the r-sections exclusively; graph nodes entered (objects, closure-bound variables and containers); total time,
time blocked waiting for another thread, and the longest single scan. The table is written to `<path>.<pid>` at
exit — every process of a cluster inherits the variable, and each writes its own file — and read at any time with
`get_scan_stats()`. Root names are copied when a row is claimed and never freed, because a class name is freed with
its Program while the table lives as long as the process.

**Scan waits** are always recorded, because they are only written where a scan already blocks: when a pass
abandons on a lock held by another thread, `setNotificationIntern()` records that thread on the notifier, the scan
records the name of the object it could not lock (copied then, since the object can be freed while the scan waits),
and the constructor registers the wait in a small registry for as long as it lasts. `get_scan_waits()` returns the
waiting threads with their scan root, the object each waits for and the thread holding it; a debugger attached to a
process calls `qore_dump_scan_waits()` (exported C function, no debug information needed), which only tries the
registry lock, so it cannot hang a process stopped while another thread updates the registry.

Debug builds add `dbg_hold_object_lock()` and `dbg_scan_wait_notify()`, which make a wait deterministic for
`examples/test/qore/misc/dgc-scan-accounting/dgc-scan-accounting.qtest`.

## Related files

- `include/qore/intern/RSet.h` — `RObject`, `RSet`, field declarations, `edge_gen`, `rclosed`, `rset` and
  its `rcount` invariant, the container-edge and per-container counting memos.
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
- `examples/test/qore/misc/dgc-closed-sets.qtest` — closed recursive sets: the scan cost, the three rules that
  keep the mark accurate, and collection through and around them.
- `examples/test/qore/misc/dgc-unchanged-sets.qtest` — a scan that finds the set already in place leaves it,
  its watches and its counts alone.
- `examples/test/qore/misc/dgc-deferred-scan-sets.qtest` — a container grown one entry at a time while its
  holder has real references: every deferred scan discards the set recorded for the holder.
- `examples/test/qore/misc/dgc-deref-fast-path.qtest` — dereferences outside a recursive set take no
  r-section, dereferences inside one still do, and cycles formed around shared values are still collected.
- `examples/test/qore/misc/dgc-scan-avoidance/dgc-scan-avoidance.qtest` — the scans made by member writes to a
  recursive set held in each common way (plain local, method call, closure-bound local, non-root member, list,
  registry, opaque registry, long-lived holder), per execution mode and from a compiled module, each paired with
  its collection. The checks are included in the worker's main program, because code in a module loaded from
  source does not take the execution mode given on the command line.
