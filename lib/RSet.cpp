/* -*- indent-tabs-mode: nil -*- */
/*
    RSet.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#include <qore/Qore.h>
#include "qore/intern/QoreObjectIntern.h"
#include "qore/intern/QoreClosureNode.h"
#include "qore/intern/lvalue_ref.h"
#include "qore/intern/qore_list_private.h"
#include "qore/intern/QoreHashNodeIntern.h"

#include <mutex>

#ifdef DEBUG
//! the number of objects that recursive-reference scans have entered in the current thread
static thread_local int64 scan_object_count = 0;

int64 q_get_scan_object_count() {
    return scan_object_count;
}

//! the number of recursive sets that scans in the current thread have created
static thread_local int64 rset_create_count = 0;

int64 q_get_rset_create_count() {
    return rset_create_count;
}

//! the number of times a scan in the current thread gave up a pass and waited to start over
static thread_local int64 rset_restart_count = 0;

int64 q_get_rset_restart_count() {
    return rset_restart_count;
}

//! the number of times a dereference in the current thread took an object's exclusive r-section
static thread_local int64 deref_rsection_count = 0;

int64 q_get_deref_rsection_count() {
    return deref_rsection_count;
}

void q_inc_deref_rsection_count() {
    ++deref_rsection_count;
}
#endif

RObject::~RObject() {
   assert(!rset.load(std::memory_order_relaxed));
}

RSetDerefHelper::~RSetDerefHelper() {
    // Releasing a shared container does not necessarily release its objects. Dereferencing every retained
    // member gives any surviving subcycle its own collection opportunity after the initiator is destroyed.
    // These normal dereferences also preserve objects resurrected by a user destructor.
    for (RObject* obj : objects) {
        obj->releaseCycleReference(xsink);
    }
}

bool RObject::scanCheck(RSetHelper& rsh, AbstractQoreNode* n) {
    return rsh.checkNode(n);
}

void RObject::setRSet(RSet* rs, int rcnt, bool closed) {
    assert(rml.checkRSectionExclusive());
    // the lock-free dereference path reads rset alone and infers rcount from it; see RObject::rset
    assert(rs || !rcnt);
    printd(QRO_LVL, "RObject::setRSet() this: %p %s rs: %p rcnt: %d closed: %d\n", this, getName(), rs, rcnt,
        (int)closed);
    RSet* old = rset.load(std::memory_order_relaxed);
    if (old) {
        // invalidating the rset removes the weak references to all contained objects and marks them as not closed
        old->invalidateDeref();
    }
    rcount = rcnt;
    // set after the old set is invalidated, which clears this flag for every object of that set
    rclosed.store(closed ? rs : nullptr, std::memory_order_relaxed);
    // record the reference count the scan saw, so a later canDelete() can tell a stale snapshot from a genuine
    // external reference
    scan_refs.store(references.load(std::memory_order_relaxed), std::memory_order_relaxed);
#ifdef DEBUG
    if (rcount > references) {
        printd(0, "RObject::setRSet() this: %p '%s' cannot set rcount %d > references %d\n", this, getName(), rcount,
            references.load());
    }
    assert(rcount <= references);
#endif
    if (rs) {
        rs->ref();
        // we make a weak reference from the rset to the object to ensure that it does not disappear while the rset is
        // valid
        tRef();
    }
    // The set is published last, with release ordering, so that everything that goes with it is already in
    // place for the dereference path that reads it with no lock.  That dereference only distinguishes a null
    // set from a non-null one and takes the rsection for anything else, where it reads all of this again.
    rset.store(rs, std::memory_order_release);
    // increment transaction count
    ++rcycle;
}

void RObject::derefRealIntern() {
    assert(rrefs > 0);
    // before allowing the real references to reach zero, we need to ensure that any rset invalidation action has
    // completed
    while (rrefs == 1 && rref_wait) {
        ++rref_waiting;
        rcond.wait(rlck);
        --rref_waiting;
    }
    assert(rrefs > 0);

    --rrefs;
}

// The objects this thread is currently dereferencing, innermost first.
//
// RObject::derefDone() waits for in-progress dereferences to finish before deleting, so that a
// delete cannot race with another thread that is mid-dereference.  That wait must exclude the
// dereferences this thread owns: a dereference can re-enter the same object on the same thread --
// RSetHelper::releaseHeld() drops the references a scan held on the edges it walked, and running
// those destructors can lead straight back to the object whose dereference started the scan -- and
// waiting for those would be waiting for itself, which no other thread can ever release.
//
// robject_dereference_helper is the only thing that starts and finishes a dereference, and it is
// an RAII object, so entries are pushed and popped in strict LIFO order on each thread.  The list
// is intrusive: each node is a member of the helper that owns the dereference, so this costs no
// allocation on the path of every object dereference and puts no bound on nesting depth.
//
// It must also be a type with a trivial destructor.  Dereferences still run after this thread's
// thread_local destructors have: glibc destroys thread_locals from exit() before running the
// static destructors registered ahead of them, and tearing down the static namespace dereferences
// the objects held by its constants.  A container here was therefore written to after it had been
// destroyed -- a use-after-free on the way out of every process, which a platform that reuses the
// freed block sooner than glibc turns into corruption of whatever took it over.
static thread_local robject_deref_frame* t_deref_inprogress = nullptr;

//! records that this thread has started a dereference of \a o in frame \a f
static void deref_inprogress_push(robject_deref_frame& f, const RObject* o) {
    f.o = o;
    f.next = t_deref_inprogress;
    t_deref_inprogress = &f;
}

//! returns the number of dereferences of \a o that this thread owns
static unsigned deref_inprogress_own(const RObject* o) {
    unsigned rv = 0;
    for (const robject_deref_frame* i = t_deref_inprogress; i; i = i->next) {
        if (i->o == o) {
            ++rv;
        }
    }
    return rv;
}

//! records that this thread has finished the dereference owning frame \a f
static void deref_inprogress_pop(robject_deref_frame& f) {
    assert(t_deref_inprogress == &f);
    t_deref_inprogress = f.next;
    f.next = nullptr;
    f.o = nullptr;
}

int RObject::deref(bool real, bool& do_scan, bool& rescan) {
    // the mutex ensures atomicity
    AutoLocker al(rlck);
    if (real) {
        derefRealIntern();
    } else {
        assert(rrefs >= 0);
    }

    // dereference the object and save the resulting value as the return value
    int rv_refs = --references;

    do_scan = !rrefs;

    if (do_scan) {
        rescan = deferred_scan;
        if (deferred_scan) {
            deferred_scan = false;
        }
    } else {
        rescan = false;
    }

    // mark that we have a dereference action in progress; the caller records its frame in the
    // per-thread stack (see t_deref_inprogress), which needs no lock because it is thread-local
    ++ref_inprogress;

    return rv_refs;
}

void RObject::derefDone(bool del, bool wait_only) {
    AutoLocker al(rlck);
    // decrement the in progress count, if it's the last thread, and there are waiting threads, then wake one up
    if ((!--ref_inprogress) && ref_waiting) {
        // we have to use broadcast here because the condition variable is shared
        rcond.broadcast();
        assert(!del);
    } else if (del || wait_only) {
        // either we will delete the object ourselves, or we handed off deletion but still need
        // other in-progress derefs to complete before our caller performs its weak-ref release
        // (otherwise that release can trigger deleteObject() while another thread is mid-deref)
        //
        // only dereferences owned by OTHER threads are waited for: a dereference that re-entered
        // this object on this thread is still on the stack below us and cannot make progress until
        // we return, so waiting for it would deadlock (see t_deref_inprogress)
        unsigned own = deref_inprogress_own(this);
        while (ref_inprogress > own) {
            ++ref_waiting;
            rcond.wait(rlck);
            --ref_waiting;
        }
    }
}

int RObject::checkDeferScan() {
    {
        AutoLocker al(rlck);
        // if we have a "real reference" (a reference that cannot be recursive), then we delay the scan
        if (!rrefs) {
            // we are making a scan now, so if the deferred_scan flag is set, unset it
            if (deferred_scan)
                deferred_scan = false;
            printd(QRO_LVL, "RObject::checkDeferScan() this: %p (%s) rrefs: %d scan OK\n", this, getName(),
                rrefs.load());
            return 0;
        }
        printd(QRO_LVL, "RObject::checkDeferScan() this: %p (%s) rrefs: %d deferring scan (already deferred: %d)\n",
            this, getName(), rrefs.load(), (int)deferred_scan);
        deferred_scan = true;
        // A scan started at another object reaches this one and assigns it a recursive set even while rrefs > 0,
        // so a set can be attached between one deferred scan and the next.  The graph has just changed again, so
        // a set recorded here no longer describes it and has to go, however many scans were deferred before:
        // returning early because deferred_scan was already set leaves such a set, and the rcount it recorded, in
        // place for the life of the object, and RSet::canDelete() then reads rcount != references as a live
        // reference from outside the set for ever.
        if (!rset.load(std::memory_order_relaxed))
            return -1;
        // otherwise we need to invalidate the rset and ensure that
        // rrefs does not go to zero until this is done
        ++rref_wait;
    }

    removeInvalidateRSet();
    AutoLocker al(rlck);
    // more than one thread can be invalidating at once, so the real references may only be released once the
    // last of them has finished
    assert(rref_wait);
    --rref_wait;
    if (!rref_wait && rref_waiting)
        rcond.broadcast();

    return -1;
}

void RObject::clearRSetClosed() {
    assert(rml.checkRSectionExclusive());
    RSet* rs = rclosed.load(std::memory_order_relaxed);
    if (rs) {
        // the object holds a reference to the set, which its rsection keeps in place
        assert(rs == rset.load(std::memory_order_relaxed));
        rs->clearClosed();
    }
}

void RObject::removeInvalidateRSet() {
    QoreAutoVarRWWriteLocker al(rml);
    removeInvalidateRSetIntern();
}

void RObject::removeInvalidateRSetIntern() {
    assert(rml.checkRSectionExclusive());
    RSet* rs = rset.load(std::memory_order_relaxed);
    if (rs) {
        // invalidating the rset removes the weak references to all contained objects
        rs->invalidateDeref();
        rcount = 0;
        // published last with release ordering, so that a dereference that reads a null set with acquire
        // ordering never sees the rcount that went with the set just released
        rset.store(nullptr, std::memory_order_release);
    }
}

namespace {
//! A list, hash, closure or reference whose outside references prevent the collection of its recursive set
struct NodeWatch {
    // the set, which removes the watch when it is invalidated
    const RSet* rset;
    // a member of the set, whose dereference rechecks the set, with a weak reference
    RObject* rep;
    // the number of references to the node held by the members of the set
    int internal;
};

// the watches are found by the address of the node, which the set keeps allocated with a weak reference; a counter
// per bucket of addresses avoids taking the lock for a node that cannot be watched
constexpr size_t NodeWatchBuckets = 4096;
std::atomic<unsigned> node_watch_buckets[NodeWatchBuckets];
std::mutex node_watch_lock;
std::unordered_map<const AbstractQoreNode*, NodeWatch> node_watches;

size_t node_watch_bucket(const AbstractQoreNode* n) {
    uint64_t v = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(n));
    v ^= v >> 33;
    v *= 0xff51afd7ed558ccdULL;
    v ^= v >> 33;
    return static_cast<size_t>(v & (NodeWatchBuckets - 1));
}

// called with node_watch_lock held; returns the member with its weak reference, which the caller releases
RObject* erase_node_watch(std::unordered_map<const AbstractQoreNode*, NodeWatch>::iterator i) {
    RObject* rep = i->second.rep;
    --node_watch_buckets[node_watch_bucket(i->first)];
    --qore_dgc_node_watch_count;
    node_watches.erase(i);
    return rep;
}

//! Watches a node of a recursive set that has references from outside the set
/** Called with the set's read lock held.
*/
void qore_dgc_watch_node(AbstractQoreNode* n, const RSet* rs, RObject* rep, int internal) {
    RObject* old_rep = nullptr;
    {
        std::lock_guard<std::mutex> al(node_watch_lock);
        auto i = node_watches.find(n);
        if (i != node_watches.end()) {
            if (i->second.rset == rs) {
                return;
            }
            // the node's previous set has been replaced without being invalidated
            old_rep = erase_node_watch(i);
        }
        rep->tRef();
        node_watches.emplace(n, NodeWatch{rs, rep, internal});
        ++node_watch_buckets[node_watch_bucket(n)];
        ++qore_dgc_node_watch_count;
    }
    if (old_rep) {
        old_rep->tDeref();
    }
}

//! Removes the watch of a node of a set that is being invalidated
void qore_dgc_unwatch_node(const AbstractQoreNode* n, const RSet* rs) {
    if (!node_watch_buckets[node_watch_bucket(n)].load(std::memory_order_relaxed)) {
        return;
    }
    RObject* rep;
    {
        std::lock_guard<std::mutex> al(node_watch_lock);
        auto i = node_watches.find(n);
        if (i == node_watches.end() || i->second.rset != rs) {
            return;
        }
        rep = erase_node_watch(i);
    }
    rep->tDeref();
}

void qore_dgc_node_weak_ref(AbstractQoreNode* n) {
    switch (n->getType()) {
        case NT_LIST:
            static_cast<QoreListNode*>(n)->weakRef();
            break;
        case NT_HASH:
            static_cast<QoreHashNode*>(n)->weakRef();
            break;
        case NT_RUNTIME_CLOSURE:
            static_cast<QoreClosureBase*>(n)->weakRef();
            break;
        case NT_REFERENCE:
            lvalue_ref::weakRef(static_cast<ReferenceNode*>(n));
            break;
        default:
            assert(false);
    }
}

void qore_dgc_node_weak_deref(AbstractQoreNode* n) {
    switch (n->getType()) {
        case NT_LIST:
            static_cast<QoreListNode*>(n)->weakDeref();
            break;
        case NT_HASH:
            static_cast<QoreHashNode*>(n)->weakDeref();
            break;
        case NT_RUNTIME_CLOSURE:
            static_cast<QoreClosureBase*>(n)->weakDeref();
            break;
        case NT_REFERENCE:
            lvalue_ref::weakDeref(static_cast<ReferenceNode*>(n));
            break;
        default:
            assert(false);
    }
}
}

std::atomic<unsigned> qore_dgc_node_watch_count{0};

void qore_dgc_node_dereferenced(AbstractQoreNode* n, ExceptionSink* xsink) {
    // the node is only accessed if it is watched, as the caller no longer holds a reference to it
    if (!node_watch_buckets[node_watch_bucket(n)].load(std::memory_order_relaxed)) {
        return;
    }
    RObject* rep;
    {
        std::lock_guard<std::mutex> al(node_watch_lock);
        auto i = node_watches.find(n);
        if (i == node_watches.end()) {
            return;
        }
        // the set keeps the node allocated while it is watched
        if (n->reference_count() > i->second.internal) {
            return;
        }
        rep = erase_node_watch(i);
    }

    // a temporary reference to the member is released like any other reference, which rechecks its set
    bool valid;
    {
        AutoLocker al(rep->rlck);
        valid = rep->references > 0;
        if (valid) {
            ++rep->references;
        }
    }
    if (valid) {
        ExceptionSink tmp;
        rep->releaseCycleReference(xsink ? xsink : &tmp);
    }
    rep->tDeref();
}

RSet::~RSet() {
    //printd(5, "RSet::~RSet() this: %p (acnt: %d)\n", this, acnt);
    assert(!acnt);
    releaseNodes();
}

void RSet::clearClosed() {
    QoreAutoRWReadLocker al(rwl);
    if (!valid) {
        return;
    }
    for (rset_t::iterator i = begin(), e = end(); i != e; ++i) {
        (*i)->rclosed.store(nullptr, std::memory_order_relaxed);
    }
}

void RSet::addNode(AbstractQoreNode* n, int internal) {
    assert(!acnt);
    qore_dgc_node_weak_ref(n);
    nodes.push_back(SetNode{n, internal});
}

void RSet::releaseNodes() {
    for (const SetNode& n : nodes) {
        qore_dgc_unwatch_node(n.node, this);
        qore_dgc_node_weak_deref(n.node);
    }
    nodes.clear();
}

#ifdef DEBUG
void RSet::dbg() {
    QoreAutoRWReadLocker al(rwl);

    printd(0, "RSet::dbg() this: %p valid: %d ssize: %d size: %d\n", this, (int)valid, (int)set.size(), (int)size());
    for (rset_t::iterator i = begin(), e = end(); i != e; ++i) {
        printd(0, " + %p '%s' rcount: %d refs: %d\n", *i, (*i)->getName(), (int)(*i)->rcount, (int)(*i)->refs());
    }
}
#endif

// if we return 1, the rset has been invalidated already
int RSet::canDelete(int ref_copy, int rcount, int scan_refs, RObject& initiator, RSetDerefHelper& cleanup) {
    printd(QRO_LVL, "RSet::canDelete() this: %p valid: %d\n", this, (int)valid);

    if (q_disable_gc)
        return 0;

    // Another reference operation superseded this dereference's snapshot. A rescan records the current
    // reference count, so comparing it with the old ref_copy would request the same rescan forever.
    // The newer reference either keeps the object alive or supplies its own collection opportunity when
    // released. Only a dereference whose snapshot is still current may decide to collect the cycle.
    if (ref_copy != initiator.refs()) {
        return 0;
    }

    if (!valid)
        return -1;

    bool need_rescan = false;
    {
        QoreAutoRWReadLocker al(rwl);
        if (!valid)
            return -1;

        // Invariant: rcount <= refs for every rset member. This is enforced at
        // scan time by RSetHelper. A post-scan violation means an intra-rset
        // edge was removed (e.g., a hash or data-member cleared) without
        // decrementing the target's rcount — rcount is now stale and the rset
        // must be rescanned before any collection decision can be made.
        // Without this rescan, a genuinely collectable cycle can be silently
        // stranded forever once any in-cycle ref drops.
        if (ref_copy < rcount) {
            need_rescan = true;
        } else if (ref_copy != rcount) {
            // ref_copy > rcount: the triggering object has live external refs outside the cycle -- unless its
            // own rcount is a stale snapshot.  See the member loop below: a reference the scan counted can be
            // dropped afterwards without invalidating the rset, and nothing would ever re-examine the verdict.
            if (scan_refs >= 0 && ref_copy < scan_refs) {
                need_rescan = true;
            } else {
                return 0;
            }
        } else {
            for (rset_t::iterator i = begin(), e = end(); i != e; ++i) {
                // no locking needed: if there are no external references, no external changes can be made
                if (!(*i)->isValid())
                    return 0;
                int r = (*i)->refs();
                if ((*i)->rcount > r) {
                    // stale rcount — rset needs rescan
                    need_rescan = true;
                    break;
                }
                if ((*i)->rcount != r) {
                    printd(QRO_LVL, "RSet::canDelete() this: %p cannot delete graph obj %p '%s' rcount: %d "
                        "refs: %d (scan_refs: %d)\n", this, *i, (*i)->getName(), (*i)->rcount, r,
                        (*i)->scan_refs.load());
                    // rcount < refs normally means a live reference from outside the rset.  But rcount is a
                    // snapshot taken when the rset was built, and a reference the scan DID count can be dropped
                    // afterwards without invalidating the rset -- a container holding the object can lose its
                    // last other holder, or an object in another rset can be collected.  The verdict cached here
                    // would then be wrong forever, because a plain deref of an object whose rset says "cannot
                    // delete" never triggers another scan.  Whenever a member has lost references since the
                    // scan, the snapshot no longer describes the graph: rescan instead of trusting it.  After
                    // the rescan scan_refs matches again, so this cannot loop.
                    int member_scan_refs = (*i)->scan_refs.load(std::memory_order_relaxed);
                    if (member_scan_refs >= 0 && r < member_scan_refs) {
                        need_rescan = true;
                        break;
                    }
                    return 0;
                }
                printd(QRO_LVL, "RSet::canDelete() this: %p can delete graph obj %p '%s' rcount: %d refs: %d\n",
                    this, *i, (*i)->getName(), (*i)->rcount, r);
            }
            // a list, hash, closure or reference in the set can also be held from outside the set
            if (!need_rescan) {
                for (const SetNode& n : nodes) {
                    int r = n.node->reference_count();
                    if (r > n.internal) {
                        // releasing the outside reference does not dereference any member, so a release that
                        // leaves only the set's references rechecks the set
                        qore_dgc_watch_node(n.node, this, *begin(), n.internal);
                        // a reference released before the watch was registered did not recheck the set
                        r = n.node->reference_count();
                        if (r > n.internal) {
                            printd(QRO_LVL, "RSet::canDelete() this: %p cannot delete graph node %p (%s) internal: "
                                "%d refs: %d\n", this, n.node, get_type_name(n.node), n.internal, r);
                            return 0;
                        }
                    }
                    if (r < n.internal) {
                        // a reference counted by the scan has been released
                        need_rescan = true;
                        break;
                    }
                }
            }
        }
    }

    if (need_rescan) {
        // Ask the caller to rescan, and leave the set in place: the scan recomputes the components from the
        // live graph, and when they are the ones already recorded it confirms the set and records the
        // reference count it saw, which is all this verdict was missing.  Throwing the set away first would
        // make that scan build an identical one, and because evaluating an object into an lvalue holds a
        // temporary reference that is released after the scan has recorded its count, the next dereference
        // asks for the same rescan again: one assignment of an object in a cycle cost five walks of the
        // graph and four discarded sets.  A scan that finds the graph really has changed replaces the set
        // as it always did.
        return -1;
    }

    // invalidate the rset
    QoreAutoRWWriteLocker al(rwl);
    if (!valid)
        return -1;

    // Retain members before dropping the set's weak references. The initiating object is already protected
    // by its dereference helper. Cleanup runs outside the r-section and rset locks, after its destructor.
    if (!nodes.empty()) {
        cleanup.reserve(set.size());
        for (RObject* member : set) {
            if (member != &initiator) {
                cleanup.add(member);
            }
        }
    }
    invalidateIntern();

    printd(QRO_LVL, "RSet::canDelete() this: %p can delete all objects in graph\n", this);
    return 1;
}

robject_dereference_helper::robject_dereference_helper(RObject* obj, bool real) : o(obj) {
    refs = obj->deref(real, do_scan, deferred_scan);
    del = !refs;
    // record the dereference this object now has in progress, so that a nested dereference of the
    // same object on this thread does not wait for it (see t_deref_inprogress)
    deref_inprogress_push(frame, obj);
}

robject_dereference_helper::~robject_dereference_helper() {
    // this dereference is finished as far as the wait in derefDone() is concerned: what it waits
    // for is the dereferences still below us on this thread's stack, and those of other threads
    assert(frame.o == o);
    deref_inprogress_pop(frame);

    // if finalDeref() handed off deletion, we do not claim deleter status (avoids the
    // waiter-vs-deleter assertion in derefDone), but we still wait for other in-progress
    // derefs to finish before the qo->tDeref() below, otherwise tDeref() can race with a
    // concurrent deref and trigger deleteObject() while another thread is mid-access
    o->derefDone(del && !handed_off, handed_off);

    if (del && qo) {
        qo->tDeref();
    }
}

bool RSetHelper::checkNode(AbstractQoreNode* n) {
    if (!needs_scan(n)) {
        return false;
    }

    NodeKind kind;
    switch (n->getType()) {
        case NT_OBJECT:
            return checkNode(*qore_object_private::get(*static_cast<QoreObject*>(n)));
        case NT_LIST:
            kind = NodeKind::List;
            break;
        case NT_HASH:
            kind = NodeKind::Hash;
            break;
        case NT_RUNTIME_CLOSURE:
            kind = NodeKind::Closure;
            break;
        case NT_REFERENCE:
            kind = NodeKind::Reference;
            break;
        default:
            return false;
    }

    assert(current >= 0);
    // a value held already cannot have been freed and replaced by another at the same address
    if (hold_edges && held.insert(n).second) {
        n->ref();
        held_nodes.push_back(n);
    }
    edges.push_back(ScanEdge{current, -1, n, kind});
    return false;
}

bool RSetHelper::checkNode(RObject& robj) {
    assert(current >= 0);
    // Nothing in a closed recursive set references a node outside that set, so the set cannot become part of a
    // larger cycle and the scan does not enter it: the reference is recorded as leaving the scanned graph, which
    // keeps the component holding it from being closed itself.  The set that the scan started in is always
    // entered - that set is the one being recalculated, and a mutation of any of its nodes starts a scan there.
    RSet* closed = robj.rclosed.load(std::memory_order_relaxed);
    if (closed && closed != root_rset) {
        printd(QRO_LVL, "RSetHelper::checkNode() obj %p '%s' is in closed rset %p; not entering it\n", &robj,
            robj.getName(), closed);
        nodes[current].open = true;
        return false;
    }
    if (hold_edges && held.insert(&robj).second) {
        robj.tRef();
        held_objects.push_back(&robj);
    }
    edges.push_back(ScanEdge{current, -1, &robj, NodeKind::Object});
    return false;
}

int RSetHelper::getNode(void* ptr, NodeKind kind, bool& lock_error) {
    auto i = node_map.find(ptr);
    if (i != node_map.end()) {
        return i->second;
    }

    if (kind != NodeKind::Object) {
        int id = static_cast<int>(nodes.size());
        nodes.emplace_back(ptr, kind);
        node_map.emplace(ptr, id);
        return id;
    }

    RObject& obj = *static_cast<RObject*>(ptr);
    // an rsection held by this thread before the scan is not released by the scan
    bool had_lock = obj.rml.hasRSectionLock();
    // a pass that changes nothing only needs the graph to hold still, so it shares the rsection with other
    // scans; a pass that has to change a recursive set takes it to itself
    bool shared = false;
    int rc = exclusive
        ? obj.rml.tryRSectionLockNotifyWaitRead(&notifier)
        : obj.rml.tryRSectionLockSharedNotifyWaitRead(&notifier, shared);
    if (rc) {
        printd(QRO_LVL, "RSetHelper::getNode() obj %p '%s' cannot enter rsection: rsection tid: %d\n", &obj,
            obj.getName(), obj.rml.rSectionTid());
        // the failed lock registered the notification that the constructor's retry waits for
        retry_notified = true;
        lock_error = true;
        return -1;
    }
    if (!had_lock) {
        inccnt();
    }

    // do not scan invalid objects or objects being deleted
    if (!obj.isValid()) {
        if (!had_lock) {
            if (shared) {
                obj.rml.rSectionUnlockShared();
            } else {
                obj.rml.rSectionUnlock();
            }
            deccnt();
        }
        node_map.emplace(ptr, -1);
        return -1;
    }

#ifdef DEBUG
    ++scan_object_count;
#endif
    int id = static_cast<int>(nodes.size());
    nodes.emplace_back(ptr, kind);
    ScanNode& n = nodes.back();
    n.unlock = !had_lock;
    n.unlock_shared = shared;
    // an object that cannot reference other objects is not part of any cycle
    n.leaf = !obj.needsScan(true);
    // an object whose private data container can be changed without a scan cannot be part of a closed set
    n.open = obj.valuesCanChangeWithoutScan();
    node_map.emplace(ptr, id);
    printd(QRO_LVL, "RSetHelper::getNode() + adding obj %p '%s' (leaf: %d)\n", &obj, obj.getName(), n.leaf);
    return id;
}

void RSetHelper::startNode(int id) {
    {
        ScanNode& n = nodes[id];
        n.index = n.lowlink = next_index++;
        n.on_stack = true;
    }
    stack.push_back(id);

    size_t begin = edges.size();
    current = id;
    const ScanNode& n = nodes[id];
    switch (n.kind) {
        case NodeKind::Object:
            if (!n.leaf) {
                static_cast<RObject*>(n.ptr)->scanMembers(*this);
            }
            break;

        case NodeKind::List: {
            // the list cannot be changed: it is shared, or its only holder is locked or cannot be changed itself
            ListIterator li(static_cast<QoreListNode*>(n.ptr));
            while (li.next()) {
                QoreValue v = li.getValue();
                // an opaque reference ('@=') is not an edge the collector may follow
                if (v.hasNode() && !v.isOpaque()) {
                    checkNode(v.getInternalNode());
                }
            }
            break;
        }

        case NodeKind::Hash: {
            HashIterator hi(static_cast<QoreHashNode*>(n.ptr));
            while (hi.next()) {
                QoreValue v = hi.get();
                // an opaque reference ('@=') is not an edge the collector may follow
                if (v.hasNode() && !v.isOpaque()) {
                    checkNode(v.getInternalNode());
                }
            }
            break;
        }

        case NodeKind::Closure: {
            // the closure's object is referenced weakly
            const cvar_map_t& cmap = static_cast<QoreClosureBase*>(n.ptr)->getMap();
            for (cvar_map_t::const_iterator i = cmap.begin(), e = cmap.end(); i != e; ++i) {
                // a variable that cannot contain an object or closure, also not through a container, is not scanned
                if (i->second->needsScan(true)) {
                    checkNode(*i->second);
                }
            }
            break;
        }

        case NodeKind::Reference:
            lvalue_ref::get(static_cast<ReferenceNode*>(n.ptr))->scanReference(*this);
            break;
    }
    current = -1;

    frames.push_back(ScanFrame{id, begin, edges.size()});
}

bool RSetHelper::scan(RObject& root) {
    bool lock_error = false;
    int root_id = getNode(&root, NodeKind::Object, lock_error);
    if (lock_error) {
        return true;
    }
    if (root_id < 0) {
        return false;
    }
    root_obj = &root;
    // the object's rsection is held, so its recursive set cannot be replaced while the scan runs
    root_rset = root.rset;

    // Tarjan's strongly connected components algorithm with an explicit stack, as the graph can be deeper than the
    // thread's stack allows
    startNode(root_id);
    while (!frames.empty()) {
        ScanFrame& frame = frames.back();
        if (frame.next < frame.end) {
            size_t ei = frame.next++;
            int from = frame.node;
            // the frame and edge references are invalid after a node is started
            int to = getNode(edges[ei].target, edges[ei].kind, lock_error);
            if (lock_error) {
                return true;
            }
            edges[ei].to = to;
            if (to < 0) {
                // the target is not scanned, so the source's component is not closed
                nodes[from].open = true;
                continue;
            }
            if (nodes[to].index < 0) {
                startNode(to);
            } else if (nodes[to].on_stack && nodes[to].index < nodes[from].lowlink) {
                nodes[from].lowlink = nodes[to].index;
            }
            continue;
        }

        int id = frame.node;
        frames.pop_back();
        if (nodes[id].lowlink == nodes[id].index) {
            // the node is the root of a component
            int component = static_cast<int>(component_size.size());
            unsigned size = 0;
            while (true) {
                int w = stack.back();
                stack.pop_back();
                nodes[w].on_stack = false;
                nodes[w].component = component;
                ++size;
                if (w == id) {
                    break;
                }
            }
            component_size.push_back(size);
            component_cyclic.push_back(size > 1);
        }
        if (!frames.empty()) {
            int parent = frames.back().node;
            if (nodes[id].lowlink < nodes[parent].lowlink) {
                nodes[parent].lowlink = nodes[id].lowlink;
            }
        }
    }
    assert(stack.empty());

    countInternalReferences();
    findClosedComponents();
    findUnchangedComponents();
    changed = false;
    for (char unchanged : component_unchanged) {
        if (!unchanged) {
            changed = true;
            break;
        }
    }
    if (changed && !exclusive) {
        // the scan has to assign a recursive set, which needs the rsection of every object it enters to
        // itself; the caller starts the scan over in exclusive mode
        need_exclusive = true;
        return false;
    }
    return prepareCommit();
}

void RSetHelper::countInternalReferences() {
    // a reference is internal if its holder is in the same component; a node that references itself forms a cycle
    for (const ScanEdge& e : edges) {
        if (e.to < 0 || nodes[e.from].component != nodes[e.to].component) {
            continue;
        }
        if (e.from == e.to) {
            component_cyclic[nodes[e.to].component] = true;
        }
    }
    for (const ScanEdge& e : edges) {
        if (e.to >= 0 && nodes[e.from].component == nodes[e.to].component
            && component_cyclic[nodes[e.to].component]) {
            ++nodes[e.to].internal;
        }
    }

    // Every internal reference is a reference held by a node of the component, so a component with more internal
    // references to a node than the node has in total was scanned inconsistently; it is not made a recursive set,
    // which would allow its collection
    for (const ScanNode& n : nodes) {
        if (n.component < 0 || !component_cyclic[n.component]) {
            continue;
        }
        int refs = n.kind == NodeKind::Object
            ? static_cast<RObject*>(n.ptr)->refs()
            : static_cast<AbstractQoreNode*>(n.ptr)->reference_count();
        if (n.internal > refs) {
            printd(0, "RSetHelper::countInternalReferences() node %p kind %d has %d internal references and %d "
                "references; not making a recursive set\n", n.ptr, static_cast<int>(n.kind), n.internal, refs);
            component_cyclic[n.component] = false;
        }
    }
}

void RSetHelper::findClosedComponents() {
    // A component is closed when every reference held by its nodes points at a node of the same component.  Such
    // a component is a complete region of the graph: no scan that reaches it from outside can find a cycle
    // through it, so the objects of the recursive set it becomes are not scanned again until one of its nodes
    // changes and takes the set out of the closed state; see design/dgc.md.
    component_closed.assign(component_size.size(), 1);
    for (const ScanEdge& e : edges) {
        if (e.to < 0 || nodes[e.from].component != nodes[e.to].component) {
            component_closed[nodes[e.from].component] = 0;
        }
    }
    // a node that references an object the scan did not enter, or whose values can change without a scan,
    // keeps its component open
    for (const ScanNode& n : nodes) {
        if (n.open) {
            component_closed[n.component] = 0;
        }
    }
}

void RSetHelper::findUnchangedComponents() {
    // A scan that finds exactly the recursive sets that are already in place has nothing to record.  Replacing
    // a set with an identical one releases and retakes a weak reference to every member, drops and re-registers
    // the watches of its nodes, and makes every scan a writer of every object it entered; keeping the set makes
    // a scan of an unchanged graph free of writes apart from the scan generation.
    size_t ncomp = component_size.size();
    component_unchanged.assign(ncomp, 1);
    std::vector<RSet*> comp_rset(ncomp, nullptr);
    std::vector<size_t> comp_objects(ncomp, 0), comp_nodes(ncomp, 0);

    for (const ScanNode& n : nodes) {
        int c = n.component;
        if (n.kind != NodeKind::Object) {
            ++comp_nodes[c];
            continue;
        }
        ++comp_objects[c];
        if (!component_unchanged[c]) {
            continue;
        }
        RObject* obj = static_cast<RObject*>(n.ptr);
        if (!component_cyclic[c]) {
            // the scan assigns no set to the objects of a component without a cycle
            if (obj->rset.load(std::memory_order_relaxed)) {
                component_unchanged[c] = 0;
            }
            continue;
        }
        RSet* rs = obj->rset.load(std::memory_order_relaxed);
        if (!rs || !rs->active() || obj->rcount != n.internal
            || obj->rclosed.load(std::memory_order_relaxed) != (component_closed[c] ? rs : nullptr)) {
            component_unchanged[c] = 0;
            continue;
        }
        if (!comp_rset[c]) {
            comp_rset[c] = rs;
        } else if (comp_rset[c] != rs) {
            component_unchanged[c] = 0;
        }
    }

    // the set must have exactly the objects and nodes that the scan found, with the same internal counts
    for (size_t c = 0; c < ncomp; ++c) {
        if (!component_unchanged[c] || !component_cyclic[c]) {
            continue;
        }
        RSet* rs = comp_rset[c];
        if (!rs || rs->size() != comp_objects[c] || rs->nodeCount() != comp_nodes[c]
            || !matchesComponent(*rs, static_cast<int>(c))) {
            component_unchanged[c] = 0;
        }
    }
}

bool RSetHelper::matchesComponent(RSet& rs, int component) {
    // The set cannot be invalidated here: every path that invalidates it needs the rsection or the write lock
    // of one of its objects, and the scan holds the rsection of all of them.  The counts are compared by the
    // caller, so finding each of the set's members and nodes in the component proves that the two are equal.
    for (rset_t::iterator i = rs.begin(), e = rs.end(); i != e; ++i) {
        auto ni = node_map.find(*i);
        if (ni == node_map.end() || ni->second < 0) {
            return false;
        }
        const ScanNode& n = nodes[ni->second];
        if (n.kind != NodeKind::Object || n.component != component) {
            return false;
        }
    }
    for (auto i = rs.nodeBegin(), e = rs.nodeEnd(); i != e; ++i) {
        auto ni = node_map.find(i->node);
        if (ni == node_map.end() || ni->second < 0) {
            return false;
        }
        const ScanNode& n = nodes[ni->second];
        if (n.kind == NodeKind::Object || n.component != component || n.internal != i->internal) {
            return false;
        }
    }
    return true;
}

bool RSetHelper::prepareCommit() {
    int tid = q_gettid();
    for (const ScanNode& n : nodes) {
        if (n.kind != NodeKind::Object || !component_cyclic[n.component]) {
            continue;
        }
        if (component_unchanged[n.component]) {
            // the set is kept, so its members are not locked for an invalidation that will not happen
            continue;
        }
        RObject* obj = static_cast<RObject*>(n.ptr);
        // the members of the object's current set that were not scanned are locked until the set is replaced
        RSet* ors = obj->rset.load(std::memory_order_relaxed);
        if (ors && removeInvalidate(ors, tid)) {
            return true;
        }
    }
    return false;
}

bool RSetHelper::removeInvalidate(RSet* ors, int tid) {
    if (tr_invalidate.find(ors) != tr_invalidate.end()) {
        return false;
    }

    // get a list of objects to be invalidated
    rvec_t rovec;

    {
        QoreAutoRWReadLocker al(ors->rwl);

        if (!ors->active())
            return false;

        // first grab all rsection locks
        for (rset_t::iterator ri = ors->begin(), re = ors->end(); ri != re; ++ri) {
            // if we already have the rsection lock, then ignore; already processed (either scanned or in tr_out)
            if ((*ri)->rml.hasRSectionLock(tid))
                continue;

            if ((*ri)->rml.tryRSectionLockNotifyWaitRead(&notifier)) {
                printd(QRO_LVL, "RSetHelper::removeInvalidate() obj %p '%s' cannot enter rsection: tid: %d\n", *ri,
                    (*ri)->getName(), (*ri)->rml.rSectionTid());

                // the failed lock registered the notification that the constructor's retry waits for
                retry_notified = true;

                // release other rsection locks
                for (unsigned i = 0; i < rovec.size(); ++i) {
                    rovec[i]->rml.rSectionUnlock();
                    deccnt();
                }
                return true;
            }
            inccnt();

            // check object status; do not scan invalid objects or objects being deleted
            if (!(*ri)->isValid()) {
                (*ri)->rml.rSectionUnlock();
                deccnt();
                continue;
            }

            rovec.push_back(*ri);
        }
    }

    // invalidate old rset when transaction is committed
    tr_invalidate.insert(ors);

    for (unsigned i = 0; i < rovec.size(); ++i) {
        assert(rovec[i]->rml.hasRSectionLock());
        assert(rovec[i]->rset.load(std::memory_order_relaxed) == ors);
        tr_out.insert(rovec[i]);
    }

    return false;
}

class RScanHelper {
public:
    RObject& obj;
    int rcycle;

    DLLLOCAL RScanHelper(RObject& o) : obj(o), rcycle(o.rcycle.load()) {
        AutoLocker al(obj.rlck);
        while (obj.rscan) {
            ++obj.rwaiting;
            obj.rcond.wait(obj.rlck);
            --obj.rwaiting;
        }
        obj.rscan = q_gettid();
    }

    DLLLOCAL ~RScanHelper() {
        AutoLocker al(obj.rlck);
        assert(obj.rscan == q_gettid());
        // we have to use broadcast here because the condition variable is shared
        if (obj.rwaiting)
            obj.rcond.broadcast();
        obj.rscan = 0;
    }

    //! Returns true if this object still has to be scanned, false if another thread has scanned it since
    /** The generation is sampled before the scan lock is acquired, so this reports scans committed by
        other threads both while waiting for the scan lock and while waiting for a conflicting
        transaction to finish.
    */
    DLLLOCAL bool needScan() const {
        return rcycle == obj.rcycle;
    }
};

RSetHelper::RSetHelper(RObject& obj, ExceptionSink* xsink) : xsink(xsink) {
    if (q_disable_gc) {
        return;
    }

    // if the scan should be deferred
    if (obj.checkDeferScan()) {
        return;
    }

    printd(QRO_LVL, "RSetHelper::RSetHelper() this: %p (%p %s) ENTER\n", this, &obj, obj.getName());

    // a scan that changes nothing shares the rsection of the objects it enters with other scans; guessing
    // that from the last scan made here avoids walking the graph twice when it does have to change a set
    exclusive = obj.scan_wrote.load(std::memory_order_relaxed);

    RScanHelper rsh(obj);

    // The scan lock serializes scans of this object, so threads that arrive while a scan is in flight
    // wait here; by the time they acquire it, that scan has committed and recalculated this object's
    // recursive set, and rescanning would only repeat the work.  Skipping it is not just an
    // optimization: a scan locks the r-section of every object it reaches, so a redundant scan makes
    // every other scan in flight that reaches any of those objects abort and restart.  When many
    // threads dereference the same object - every accepted connection releasing a local that holds a
    // shared server object, for example - the restarts stop converging and no scan finishes at all.
    // This is the same generation check made below after waiting for a conflicting transaction.
    if (!rsh.needScan()) {
        printd(QRO_LVL, "RSetHelper::RSetHelper() this: %p (%p: %s) ALREADY SCANNED IN ANOTHER THREAD\n", this,
            &obj, obj.getName());
        return;
    }

    while (true) {
        if (scan(obj)) {
            // The retry below only makes progress because notifier.wait() blocks until the owner of the lock
            // this pass could not take releases it.  A pass abandoned without registering that notification
            // has nothing to wait for, so this loop would spin at 100% CPU forever, walking the graph from
            // scratch every time, producing no output and never completing - a shape that is indistinguishable
            // from a slow compile from outside the process.
            assert(retry_notified);
#ifdef DEBUG
            ++rset_restart_count;
#endif
            rollback();
            // wait for foreign transaction to finish if necessary
            notifier.wait();

            if (!rsh.needScan()) {
                printd(QRO_LVL, "RSetHelper::RSetHelper() this: %p (%p: %s) TRANSACTION COMPLETE IN ANOTHER THREAD\n",
                    this, &obj, obj.getName());
                return;
            }
            printd(QRO_LVL, "RSetHelper::RSetHelper() this: %p (%p: %s) RESTARTING TRANSACTION: %d\n", this, &obj,
                obj.getName(), obj.rcycle.load());
            continue;
        }

        if (need_exclusive) {
            // the pass in shared mode found a recursive set that it has to change; nothing was changed, so
            // the scan simply starts over with the rsections it needs for that
            printd(QRO_LVL, "RSetHelper::RSetHelper() this: %p (%p: %s) RESTARTING WITH EXCLUSIVE RSECTIONS\n",
                this, &obj, obj.getName());
            rollback(false);
            exclusive = true;
            continue;
        }

        break;
    }

    commit();

    // record what this scan had to do, so the next scan started here picks the mode that fits
    obj.scan_wrote.store(changed, std::memory_order_relaxed);

    printd(QRO_LVL, "RSetHelper::RSetHelper() this: %p (%p) EXIT\n", this, &obj);
}

RSetHelper::~RSetHelper() {
    assert(!lcnt);
    releaseHeld();
}

void RSetHelper::releaseHeld() {
    if (held_nodes.empty() && held_objects.empty()) {
        return;
    }
    // the references are released after the scan's locks, as releasing them can run destructors
    held.clear();
    std::vector<AbstractQoreNode*> nvec;
    nvec.swap(held_nodes);
    std::vector<RObject*> ovec;
    ovec.swap(held_objects);
    ExceptionSink tmp;
    for (AbstractQoreNode* n : nvec) {
        n->deref(xsink ? xsink : &tmp);
    }
    for (RObject* o : ovec) {
        o->tDeref();
    }
}

void RSetHelper::commit() {
    if (!changed) {
        // The scan found every recursive set it would have assigned already in place, so there is nothing to
        // invalidate, nothing to create and nothing to replace: confirm each object and release it.  Only the
        // object the scan started at advances its generation; see RObject::confirmRSet().
        assert(tr_invalidate.empty());
        assert(tr_out.empty());
        for (const ScanNode& n : nodes) {
            if (n.kind != NodeKind::Object) {
                continue;
            }
            RObject* obj = static_cast<RObject*>(n.ptr);
            assert(qore_var_rwlock_priv::get(obj->rml)->write_tid >= -1);
            obj->confirmRSet(obj == root_obj);
            if (n.unlock) {
                unlockNode(n);
            }
        }
        assert(!lcnt);
        return;
    }

    // invalidate rsets
    for (rs_set_t::iterator i = tr_invalidate.begin(), e = tr_invalidate.end(); i != e; ++i) {
        (*i)->invalidate();
    }

    // unlock rsection
    for (rset_t::iterator i = tr_out.begin(), e = tr_out.end(); i != e; ++i) {
        assert(node_map.find(*i) == node_map.end() || node_map.find(*i)->second < 0);
        (*i)->rml.rSectionUnlock();
        deccnt();
    }

    // create a recursive set for each component with a cycle whose set is not already in place
    std::vector<RSet*> rsets(component_size.size(), nullptr);
    for (const ScanNode& n : nodes) {
        if (n.kind != NodeKind::Object || !component_cyclic[n.component]
            || component_unchanged[n.component]) {
            continue;
        }
        RSet*& rs = rsets[n.component];
        if (!rs) {
            rs = new RSet;
#ifdef DEBUG
            ++rset_create_count;
#endif
        }
        rs->insert(static_cast<RObject*>(n.ptr));
    }
    for (const ScanNode& n : nodes) {
        if (n.kind == NodeKind::Object || !component_cyclic[n.component]
            || component_unchanged[n.component]) {
            continue;
        }
        // every component with a cycle has an object or closure-bound variable
        assert(rsets[n.component]);
        rsets[n.component]->addNode(static_cast<AbstractQoreNode*>(n.ptr), n.internal);
    }

    // finalize graph - exit rsection
    for (const ScanNode& n : nodes) {
        if (n.kind != NodeKind::Object) {
            continue;
        }
        RObject* obj = static_cast<RObject*>(n.ptr);
        assert(qore_var_rwlock_priv::get(obj->rml)->write_tid >= -1);
        if (component_unchanged[n.component]) {
            // the object already has exactly this recursive set; the generation still advances, because this
            // scan changed another component and holds the rsection exclusively
            printd(QRO_LVL, "RSetHelper::commit() obj %p '%s' rset: %p unchanged\n", obj, obj->getName(),
                obj->rset.load(std::memory_order_relaxed));
            obj->confirmRSet(true);
            continue;
        }
        RSet* rs = rsets[n.component];
        printd(QRO_LVL, "RSetHelper::commit() obj %p '%s' rset: %p rcount: %d\n", obj, obj->getName(), rs,
            n.internal);
        obj->setRSet(rs, rs ? n.internal : 0, rs && component_closed[n.component]);
    }

#ifdef DEBUG
    for (RSet* rs : rsets) {
        if (!rs) {
            continue;
        }
        assert(rs->size() == rs->getCount());
        for (rset_t::iterator ri = rs->begin(), re = rs->end(); ri != re; ++ri) {
            assert((*ri)->rset.load(std::memory_order_relaxed) == rs);
        }
    }
#endif

    for (const ScanNode& n : nodes) {
        if (n.kind == NodeKind::Object && n.unlock) {
            unlockNode(n);
        }
    }

    assert(!lcnt);
}

void RSetHelper::unlockNode(const ScanNode& n) {
    assert(n.kind == NodeKind::Object);
    assert(n.unlock);
    RObject* obj = static_cast<RObject*>(n.ptr);
    if (n.unlock_shared) {
        obj->rml.rSectionUnlockShared();
    } else {
        obj->rml.rSectionUnlock();
    }
    deccnt();
}

void RSetHelper::rollback(bool yield) {
    for (const ScanNode& n : nodes) {
        if (n.kind == NodeKind::Object && n.unlock) {
            unlockNode(n);
        }
    }

    // exit rsection of objects in tr_out
    for (rset_t::iterator i = tr_out.begin(), e = tr_out.end(); i != e; ++i) {
        (*i)->rml.rSectionUnlock();
        deccnt();
    }

    assert(!lcnt);

    // the scan starts over from scratch
    nodes.clear();
    node_map.clear();
    edges.clear();
    frames.clear();
    stack.clear();
    component_size.clear();
    component_cyclic.clear();
    component_closed.clear();
    component_unchanged.clear();
    root_obj = nullptr;
    root_rset = nullptr;
    need_exclusive = false;
    changed = false;
    retry_notified = false;
    next_index = 0;
    current = -1;
    tr_out.clear();
    tr_invalidate.clear();
    // the held values are released by the destructor, after the scan lock of the object that the scan started at

#ifdef _POSIX_PRIORITY_SCHEDULING
    // a rollback made to start over with exclusive rsections is not waiting for another thread
    if (yield) {
        sched_yield();
    }
#endif
}
