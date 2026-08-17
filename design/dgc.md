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
| `rrefs` | "Real" refs: references known *not* to be part of a cycle (set by `realRef()`, e.g., method-call helpers, background thread ownership). If `rrefs > 0` the object is still in active use, and cycle scanning is **deferred**. |
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

If `rrefs > 0`, `scanMembersIntern` returns immediately after setting `deferred_scan = true` and invalidating the current rset:

```cpp
if (rrefs) {
    ...
    if (!deferred_scan) { deferred_scan = true; }
    removeInvalidateRSetIntern();
    return false;
}
```

Scans are retried after the last `realDeref()` drops `rrefs` to 0. If a `realRef()` is leaked (never dereferenced), cycle collection for that object never runs.

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
    ReferenceHolder<Queue> q(...);   if (*q) q_priv->scanMembers(*this, rsh);
    ReferenceHolder<TreeMapData> tm(...); if (*tm) tm->scanMembers(*this, rsh);
}
```

**When you introduce a new C++ container class that can hold `QoreValue` and be stored as private data, you must add a `scanMembers` method and register it in this dispatcher**, and the enclosing object must set `scan_private_data = true` (or override `needsScan` to return true when appropriate).

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

- `include/qore/intern/RSet.h` — `RObject`, `RSet`, field declarations.
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
