/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  RSet.h

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

#ifndef _QORE_INTERN_RSETHELPER_H

#define _QORE_INTERN_RSETHELPER_H

#include "qore/intern/RSection.h"
#include "qore/vector_set"
#include "qore/vector_map"

#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class RSet;
class RSetHelper;

class RObject {
    friend class robject_dereference_helper;

public:
    // read-write lock with special rsection handling
    mutable RSectionLock rml;
    // weak references
    QoreReferenceCounter tRefs;

    // ensures atomicity of robject reference counting and notification actions
    mutable QoreThreadLock rlck;

    QoreCondition rcond; // condition variable (used with rlck)

    int rscan = 0,          // TID flag for starting a recursive scan
        rcount = 0,         // the number of unique recursive references to this object
        scan_refs = -1,     // "references" as observed when rcount was assigned; -1 = never scanned
        rwaiting = 0,       // the number of threads waiting for a scan of this object
        ref_inprogress = 0, // the number of dereference actions in progress
        ref_waiting = 0,    // the number of threads waiting on a dereference action to complete
        rref_waiting = 0;   // the number of threads waiting on an rset invalidation to complete

    // The scan generation: incremented every time a committed scan assigns this object's recursive set,
    // so a thread that sampled an older value knows that another thread has scanned this object since.
    // Atomic because it is sampled without holding any lock, before waiting for the scan lock (see
    // RScanHelper in lib/RSet.cpp).
    std::atomic_int rcycle{0};

    // the number of "real" refs (i.e. refs not possibly part of a recursive graph)
    // atomic because it is read without rlck in some paths (customDeref, scanMembersIntern)
    std::atomic_int rrefs{0};

    // set of objects in a cyclic directed graph
    RSet* rset = nullptr;

    // reference count
    std::atomic_int& references;

    bool deferred_scan : 1, // do we need to make a scan when the object is eligible for it?
        needs_is_valid : 1,  // do we need to call isValidImpl()
        rref_wait : 1;       // rset invalidation in progress

    DLLLOCAL RObject(std::atomic_int& n_refs, bool niv = false) :
        references(n_refs), deferred_scan(false), needs_is_valid(niv), rref_wait(false) {
    }

    DLLLOCAL virtual ~RObject();

    DLLLOCAL void tRef() const {
#ifdef QORE_DEBUG_OBJ_REFS
        printd(QORE_DEBUG_OBJ_REFS, "RObject::tRef() this: %p tref %d->%d\n", this, tRefs.reference_count(),
            tRefs.reference_count() + 1);
#endif
        tRefs.ROreference();
    }

    DLLLOCAL void tDeref() {
#ifdef QORE_DEBUG_OBJ_REFS
        printd(QORE_DEBUG_OBJ_REFS, "RObject::tDeref() this: %p tref %d->%d\n", this, tRefs.reference_count(),
            tRefs.reference_count() - 1);
#endif
        if (tRefs.ROdereference())
            deleteObject();
    }

    // real: decrement rref too
    // do_scan: is the object eleigible for a scan? (rrefs = 0)
    // rescan: do we need to force a rescan of the object?
    // return value: the final reference value after the deref
    DLLLOCAL int deref(bool real, bool& do_scan, bool& rescan);

    // decrements rref
    DLLLOCAL void derefRealIntern();

    // wait_only=true means: we do not claim deleter status (so the "deleter vs. waiter" invariant
    // is not tripped) but we still wait for other in-progress derefs to complete — used by the
    // handed-off path in robject_dereference_helper so a following tDeref() does not fire
    // deleteObject() while another thread is still in its deref window
    DLLLOCAL void derefDone(bool del, bool wait_only = false);

    DLLLOCAL int refs() const {
        return references;
    }

    DLLLOCAL void setRSet(RSet* rs, int rcnt);

    // check if we should defer the scan, marks the object for a deferred scan if necessary
    // returns 0 if the scan can be made now, -1 if deferred
    DLLLOCAL int checkDeferScan();

    DLLLOCAL void removeInvalidateRSet();
    DLLLOCAL void removeInvalidateRSetIntern();

    //! Reports a value referenced by this object to the scan in progress; always returns false
    DLLLOCAL bool scanCheck(RSetHelper& rsh, AbstractQoreNode* n);

    // very fast check if the object might have recursive references
    DLLLOCAL bool mightHaveRecursiveReferences() const {
        return rset || rcount;
    }

    // if the object is valid (and can be deleted)
    DLLLOCAL bool isValid() const {
        return !needs_is_valid ? true : isValidImpl();
    }

    // if the object is valid (and can be deleted)
    DLLLOCAL virtual bool isValidImpl() const {
        assert(false);
        return true;
    }

    //! Reports each value referenced by this object to the scan with RSetHelper::checkNode(); the rsection lock is held
    /** @return false; a return value of true is ignored
    */
    DLLLOCAL virtual bool scanMembers(RSetHelper& rsh) = 0;

    // returns true if the object needs to be scanned for recursive references (ie could contain an object or closure or a container containing one of those)
    /** @param scan_now scan will be made now
    */
    DLLLOCAL virtual bool needsScan(bool scan_now) = 0;

    // deletes the object itself
    DLLLOCAL virtual void deleteObject() = 0;

    //! Releases a temporary strong reference retained while a collectable recursive set is torn down
    DLLLOCAL virtual void releaseCycleReference(ExceptionSink* xsink) = 0;

    // returns the name of the object
    DLLLOCAL virtual const char* getName() const = 0;
};

// use a vector set for performance
typedef vector_set_t<RObject*> rset_t;

//! Keeps the other cycle members alive until the initiating object's teardown finishes, then releases them
class RSetDerefHelper {
public:
    DLLLOCAL explicit RSetDerefHelper(ExceptionSink* xsink) : xsink(xsink) {
    }

    DLLLOCAL ~RSetDerefHelper();

    DLLLOCAL void reserve(size_t size) {
        objects.reserve(size);
    }

    DLLLOCAL void add(RObject* obj) {
        objects.push_back(obj);
        AutoLocker al(obj->rlck);
        assert(obj->references > 0);
        ++obj->references;
    }

    //! Moves the retained members to a vector that is empty; the caller releases them
    DLLLOCAL void take(std::vector<RObject*>& vec) {
        assert(vec.empty());
        vec.swap(objects);
    }

private:
    ExceptionSink* xsink;
    std::vector<RObject*> objects;

    RSetDerefHelper(const RSetDerefHelper&) = delete;
    RSetDerefHelper& operator=(const RSetDerefHelper&) = delete;
};

/* Qore recursive reference handling works as follows: a scan divides the graph of objects, closure-bound variables,
   lists, hashes, closures and references that are reachable from an object into strongly connected components.
   The objects and closure-bound variables of each component with a cycle make up a recursive set.

   The members of a set go out of scope when every reference to each object, closure-bound variable, list, hash,
   closure and reference in the component is held by another node of the component.

   If any member still has other references, *none* of the members of the graph can be dereferenced.
   See design/dgc.md.
 */

// set of objects in a recursive directed graph
class RSet {
public:
    QoreRWLock rwl;

    DLLLOCAL RSet() : acnt(0), valid(true) {
    }

    DLLLOCAL ~RSet();

    DLLLOCAL void invalidate() {
        QoreAutoRWWriteLocker al(rwl);
        if (valid) {
            invalidateIntern();
        }
    }

    DLLLOCAL void invalidateDeref() {
        bool del = false;
        {
            QoreAutoRWWriteLocker al(rwl);
            if (valid) {
                invalidateIntern();
                valid = false;
            }
            //printd(5, "RSet::invalidateDeref() this: %p %d -> %d\n", this, acnt, acnt - 1);
            assert(acnt > 0);
            del = !--acnt;
        }
        if (del) {
            delete this;
        }
    }

    DLLLOCAL void ref() {
        QoreAutoRWWriteLocker al(rwl);
        ++acnt;
    }

    DLLLOCAL bool active() const {
        return valid;
    }

    /* return values:
        -1: error, rset invalid
        0: cannot delete
        1: the rset has been invalidated already, the object can be deleted
    */
    DLLLOCAL int canDelete(int ref_copy, int rcount, int scan_refs, RObject& initiator, RSetDerefHelper& cleanup);

#ifdef DEBUG
    DLLLOCAL void dbg();

    DLLLOCAL static bool isValid(const RSet* rs) {
        return rs ? rs->valid : false;
    }
#endif

    DLLLOCAL bool assigned() const {
        return (bool)acnt;
    }

    DLLLOCAL void insert(RObject* o) {
        assert(set.find(o) == set.end());
        set.insert(o);
    }

    //! Adds a list, hash, closure or reference in the recursive set before the set is assigned
    /** @param n the node, which the set keeps a weak reference to
        @param internal the number of references to the node held by the members of the recursive set

        Collecting the set also releases the node's remaining references, so the members are released together
        by the dereferences of the retained members; see RSetDerefHelper.
    */
    DLLLOCAL void addNode(AbstractQoreNode* n, int internal);

    DLLLOCAL void clear() {
        set.clear();
    }

    DLLLOCAL rset_t::iterator begin() {
        return set.begin();
    }

    DLLLOCAL rset_t::iterator end() {
        return set.end();
    }

    DLLLOCAL rset_t::iterator find(RObject* o) {
        return set.find(o);
    }

    DLLLOCAL size_t size() const {
        return set.size();
    }

#ifdef DEBUG
    DLLLOCAL unsigned getCount() const {
        return acnt;
    }
#endif

protected:
    //! A list, hash, closure or reference in the recursive set
    struct SetNode {
        AbstractQoreNode* node;
        // the number of references to the node held by the members of the recursive set
        int internal;
    };

    rset_t set;
    std::vector<SetNode> nodes;
    unsigned acnt;
    bool valid;

    // called with the write lock held
    DLLLOCAL void invalidateIntern() {
        assert(valid);
        valid = false;
        // remove the weak references to all contained objects
        for (rset_t::iterator i = begin(), e = end(); i != e; ++i) {
            (*i)->tDeref();
        }
        clear();
        releaseNodes();
        //printd(6, "RSet::invalidateIntern() this: %p\n", this);
    }

    //! Removes the watches of the nodes and their weak references
    DLLLOCAL void releaseNodes();
};

typedef std::vector<RObject*> rvec_t;
typedef vector_set_t<RSet*> rs_set_t;

//! The number of watched lists, hashes, closures and references
/** A watched node is part of a recursive set that cannot be collected because the node has references from outside
    the set; releasing a reference to it can make the set collectable.
*/
DLLLOCAL extern std::atomic<unsigned> qore_dgc_node_watch_count;

//! Returns true if nodes of the given type can be watched
DLLLOCAL inline bool qore_dgc_watchable_type(qore_type_t t) {
    return t == NT_LIST || t == NT_HASH || t == NT_RUNTIME_CLOSURE || t == NT_REFERENCE;
}

//! Called after a reference to a list, hash, closure or reference was released while nodes are watched
/** Rechecks the recursive set of a watched node once its references are held by the set alone; the node must not be
    accessed unless it is watched, as the caller no longer holds a reference to it.
*/
DLLLOCAL void qore_dgc_node_dereferenced(AbstractQoreNode* n, ExceptionSink* xsink);

//! Finds the strongly connected components of the graph reachable from an object and assigns its recursive sets
/** The scan is a depth-first search with an explicit stack. Each object and closure-bound variable reached is locked
    in its rsection until the scan is committed; a lock that another thread holds ends the attempt, which is retried
    after that thread releases it. See design/dgc.md.
*/
class RSetHelper {
    friend class RSetHeldEdgeHelper;
public:
    //! Scans the graph reachable from an object
    /** @param obj the object
        @param xsink for exceptions raised when releasing the temporary references that the scan holds
    */
    DLLLOCAL RSetHelper(RObject& obj, ExceptionSink* xsink = nullptr);

    DLLLOCAL ~RSetHelper();

    //! Reports a value referenced by the node being scanned; always returns false
    DLLLOCAL bool checkNode(AbstractQoreNode* n);

    //! Reports an object or closure-bound variable referenced by the node being scanned; always returns false
    DLLLOCAL bool checkNode(RObject& robj);

private:
    enum class NodeKind : unsigned char {
        Object,
        List,
        Hash,
        Closure,
        Reference,
    };

    //! A node of the scanned graph
    struct ScanNode {
        // the RObject or AbstractQoreNode
        void* ptr;
        NodeKind kind;
        // true while the node is on the component stack
        bool on_stack = false;
        // true if the scan acquired the object's rsection
        bool unlock = false;
        // true for an object whose members are not scanned
        bool leaf = false;
        int index = -1;
        int lowlink = -1;
        int component = -1;
        // the number of references from nodes in the same component, if the component has a cycle
        int internal = 0;

        DLLLOCAL ScanNode(void* ptr, NodeKind kind) : ptr(ptr), kind(kind) {
        }
    };

    //! A reference from one node to another
    struct ScanEdge {
        int from;
        // the target node, or -1 if the target is not scanned or not yet reached
        int to;
        void* target;
        NodeKind kind;
    };

    //! A node whose edges are being followed
    struct ScanFrame {
        int node;
        size_t next;
        size_t end;
    };

    std::vector<ScanNode> nodes;
    // maps objects and nodes to their index in nodes, or to -1 for objects that are not scanned
    std::unordered_map<const void*, int> node_map;
    std::vector<ScanEdge> edges;
    std::vector<ScanFrame> frames;
    // the Tarjan component stack
    std::vector<int> stack;
    // the size of each component and whether it has a cycle
    std::vector<unsigned> component_size;
    std::vector<char> component_cyclic;
    int next_index = 0;
    // the node whose edges are being reported
    int current = -1;

    // true while the edges being reported are held by a private data container with its own lock, which may be
    // changed after the lock is released
    bool hold_edges = false;
    // the values held for such edges until the scan ends, also over restarts, as releasing them can run destructors
    // that scan the object that this scan started at
    std::unordered_set<const void*> held;
    std::vector<AbstractQoreNode*> held_nodes;
    std::vector<RObject*> held_objects;

    // list of RSet objects to be invalidated when the transaction is committed
    rs_set_t tr_invalidate;

    // objects of invalidated recursive sets that are not scanned
    rset_t tr_out;

    // RSectionLock notification helper when waiting on locks
    RNotifier notifier;

    ExceptionSink* xsink;

#ifdef DEBUG
    int lcnt = 0;
    DLLLOCAL void inccnt() { ++lcnt; }
    DLLLOCAL void deccnt() { --lcnt; }
#else
    DLLLOCAL void inccnt() {}
    DLLLOCAL void deccnt() {}
#endif

    //! Scans the graph; returns true on a lock error
    DLLLOCAL bool scan(RObject& root);

    //! Returns the index of the node for an object or other node, locking an object when it is first reached
    /** @return the index, or -1 if the object is not scanned or on a lock error, in which case lock_error is set
    */
    DLLLOCAL int getNode(void* ptr, NodeKind kind, bool& lock_error);

    //! Assigns the node its index, pushes it on the component stack and records its edges
    DLLLOCAL void startNode(int id);

    //! Counts the references within each component with a cycle
    DLLLOCAL void countInternalReferences();

    //! Locks the unscanned members of the recursive sets to be replaced; returns true on a lock error
    DLLLOCAL bool prepareCommit();

    // queues nodes not scanned to tr_invalidate and tr_out; returns true on a lock error
    DLLLOCAL bool removeInvalidate(RSet* ors, int tid);

    // commit transaction
    DLLLOCAL void commit();

    // rollback transaction due to lock error
    DLLLOCAL void rollback();

    //! Releases the temporary references held for the edges of private data containers
    DLLLOCAL void releaseHeld();

    DLLLOCAL RSetHelper(const RSetHelper&) = delete;
    DLLLOCAL RSetHelper& operator=(const RSetHelper&) = delete;
};

//! Reports the values of a private data container, which may be changed after its lock is released
/** The scan keeps the values alive until it ends: each object with a weak reference, as the scan checks objects when
    it reaches them, and each other node with a strong reference, which also prevents changes to a list or hash.
*/
class RSetHeldEdgeHelper {
public:
    DLLLOCAL explicit RSetHeldEdgeHelper(RSetHelper& rsh) : rsh(rsh), old(rsh.hold_edges) {
        rsh.hold_edges = true;
    }

    DLLLOCAL ~RSetHeldEdgeHelper() {
        rsh.hold_edges = old;
    }

private:
    RSetHelper& rsh;
    bool old;
};

class qore_object_private;

/** this class ensures that RObjects will not be deleted until all deref() calls are complete
 */
class robject_dereference_helper {
protected:
    RObject* o;
    qore_object_private* qo = nullptr;
    int refs;
    bool del,
        do_scan = false,
        deferred_scan,
        handed_off = false;

public:
    DLLLOCAL robject_dereference_helper(RObject* obj, bool real = false);

    DLLLOCAL ~robject_dereference_helper();

    // return our reference count as captured atomically in the constructor
    DLLLOCAL int getRefs() const {
        return refs;
    }

    // return an indicator if we have a deferred scan or not
    DLLLOCAL bool deferredScan() {
        if (deferred_scan) {
            deferred_scan = false;
            return true;
        }
        return false;
    }

    // return an indicator if we should do a scan or not
    DLLLOCAL bool doScan() const {
        return do_scan;
    }

    // mark for final dereferencing
    // another thread is already destroying the object: we hand off deletion responsibility
    // and the destructor will report del=false to derefDone so the waiter-vs-deleter invariant
    // in RObject::derefDone() is not tripped; the tDeref() still owes the original weak-ref
    // release and is unchanged
    DLLLOCAL void finalDeref(qore_object_private* obj) {
        assert(!qo);
        qo = obj;
        handed_off = true;
    }

    // mark that we will be deleting the object
    // (and therefore need to wait for any in progress dereferences to complete before deleting)
    DLLLOCAL void willDelete() {
        assert(!del);
        del = true;
    }
};

#endif
