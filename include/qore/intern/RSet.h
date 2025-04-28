/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  RSet.h

  Qore Programming Language

  Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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

#include <set>
#include <atomic>

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
        rwaiting = 0,       // the number of threads waiting for a scan of this object
        rcycle = 0,         // the recursive cycle/transaction number to see if the object has been scanned since a transaction restart
        ref_inprogress = 0, // the number of dereference actions in progress
        ref_waiting = 0,    // the number of threads waiting on a dereference action to complete
        rref_waiting = 0,   // the number of threads waiting on an rset invalidation to complete
        rrefs = 0;          // the number of "real" refs (i.e. refs not possibly part of a recursive graph)

    // set of objects in a cyclic directed graph
    RSet* rset = nullptr;

#ifdef _QORE_CYCLE_CHECK
    // set of objects pointing at this object
    std::set<RObject*> refmap;
#endif

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

    DLLLOCAL void derefDone(bool del);

    DLLLOCAL int refs() const {
        return references;
    }

    DLLLOCAL void setRSet(RSet* rs, int rcnt);

    // check if we should defer the scan, marks the object for a deferred scan if necessary
    // returns 0 if the scan can be made now, -1 if deferred
    DLLLOCAL int checkDeferScan();

    DLLLOCAL void removeInvalidateRSet();
    DLLLOCAL void removeInvalidateRSetIntern();

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

    // returns true if a lock error has occurred and the transaction should be aborted or restarted; the rsection lock is held when this function is called
    DLLLOCAL virtual bool scanMembers(RSetHelper& rsh) = 0;

    // returns true if the object needs to be scanned for recursive references (ie could contain an object or closure or a container containing one of those)
    /** @param scan_now scan will be made now
    */
    DLLLOCAL virtual bool needsScan(bool scan_now) = 0;

    // deletes the object itself
    DLLLOCAL virtual void deleteObject() = 0;

    // returns the name of the object
    DLLLOCAL virtual const char* getName() const = 0;
};

// use a vector set for performance
typedef vector_set_t<RObject*> rset_t;

/* Qore recursive reference handling works as follows: objects are sorted into sets making up
   directed cyclic graphs.

   The objects go out of scope when all nodes have references = the number of recursive references.

   If any one member still has valid references (meaning a reference count > # of recursive refs), then *none*
   of the members of the graph can be dereferenced.
 */

// set of objects in a recursive directed graph
class RSet {
public:
    QoreRWLock rwl;

    DLLLOCAL RSet() : acnt(0), valid(true) {
    }

    DLLLOCAL RSet(RObject* o) : acnt(0), valid(true) {
        set.insert(o);
    }

    DLLLOCAL RSet(bool n_valid) : acnt(1), valid(n_valid) {
    }

    DLLLOCAL ~RSet() {
        //printd(5, "RSet::~RSet() this: %p (acnt: %d)\n", this, acnt);
        assert(!acnt);
    }

    DLLLOCAL void deref() {
        bool del = false;
        {
            QoreAutoRWWriteLocker al(rwl);
            if (valid) {
                valid = false;
            }
            //printd(5, "RSet::deref() this: %p %d -> %d\n", this, acnt, acnt - 1);
            assert(acnt > 0);
            del = !--acnt;
        }
        if (del) {
            delete this;
        }
    }

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
    DLLLOCAL int canDelete(int ref_copy, int rcount);

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

    // called when rolling back an rset transaction
    DLLLOCAL bool pop() {
        assert(!set.empty());
        set.erase(set.begin());
        return set.empty();
    }

#ifdef DEBUG
    DLLLOCAL unsigned getCount() const {
        return acnt;
    }
#endif

protected:
    rset_t set;
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
        //printd(6, "RSet::invalidateIntern() this: %p\n", this);
    }
};

typedef std::vector<RObject*> rvec_t;
class RSetHelper;
//typedef std::set<RSet*> rs_set_t;
typedef vector_set_t<RSet*> rs_set_t;

struct RSetStat {
    RSet* rset = nullptr;
    int rcount = 0;
    bool in_cycle : 1,
        ok : 1;

    DLLLOCAL RSetStat() : in_cycle(false), ok(false) {
    }

    DLLLOCAL RSetStat(const RSetStat& old) : rset(old.rset), rcount(old.rcount), in_cycle(old.in_cycle), ok(old.ok) {
    }

    DLLLOCAL void finalize(RSet* rs = nullptr) {
        assert(!ok);
        assert(!rset);
        rset = rs;
    }
};

class QoreClosureBase;

class RSetHelper {
    friend class RSectionScanHelper;
    friend class RObject;
#ifdef _QORE_CYCLE_CHECK
    friend class RSetContextHelper;
    friend class RSetScanContextHelper;
#endif
public:
    DLLLOCAL RSetHelper(RObject& obj);

    DLLLOCAL ~RSetHelper() {
        assert(ovec.empty());
        assert(!lcnt);
    }

    DLLLOCAL unsigned size() const {
        return fomap.size();
    }

    DLLLOCAL void add(RObject* ro) {
        if (fomap.find(ro) != fomap.end())
            return;
        rset_t::iterator i = tr_out.lower_bound(ro);
        if (i == tr_out.end() || *i != ro)
            tr_out.insert(i, ro);
    }

    // returns true if a lock error has occurred, false if otherwise
    DLLLOCAL bool checkNode(AbstractQoreNode* n) {
        return checkIntern(n);
    }

    // returns true if a lock error has occurred, false if otherwise
    DLLLOCAL bool checkNode(RObject& robj) {
        return checkIntern(robj);
    }

#ifdef _QORE_CYCLE_CHECK
    DLLLOCAL void setScanContext(RObject* scan_context) {
        this->scan_context = scan_context;
    }
#endif

protected:
    // these must be a map and a set for performance reasons
    typedef std::map<RObject*, RSetStat> omap_t;
    typedef std::set<QoreClosureBase*> closure_set_t;
    // map of all objects scanned to rset (rset = finalized, 0 = not finalized, in current list)
    omap_t fomap;

    // map scanned hashes to ensure they are only scanned once
    typedef std::set<QoreHashNode*> hset_t;
    hset_t hset;

    // map scanned lists to ensure they are only scanned once
    typedef std::set<QoreListNode*> lset_t;
    lset_t lset;

    typedef std::vector<omap_t::iterator> ovec_t;
    // current objects being scanned, used to establish a cycle
    ovec_t ovec;

    // list of RSet objects to be invalidated when the transaction is committed
    rs_set_t tr_invalidate;

    // set of RObjects not participating in any recursive set
    rset_t tr_out;

    // RSectionLock notification helper when waiting on locks
    RNotifier notifier;

    // set of scanned closures
    closure_set_t closure_set;

#ifdef _QORE_CYCLE_CHECK
    RObject* scan_context = nullptr;
#endif

#ifdef DEBUG
    int lcnt;
    DLLLOCAL void inccnt() { ++lcnt; }
    DLLLOCAL void deccnt() { --lcnt; }
#else
    DLLLOCAL void inccnt() {}
    DLLLOCAL void deccnt() {}
#endif

    // rollback transaction due to lock error
    DLLLOCAL void rollback();

    // commit transaction
    DLLLOCAL void commit(RObject& obj);

    // returns true if a lock error has occurred, false if otherwise
    DLLLOCAL bool checkIntern(RObject& obj);
    // returns true if a lock error has occurred, false if otherwise
    DLLLOCAL bool checkIntern(AbstractQoreNode* n);

    // queues nodes not scanned to tr_invalidate and tr_out
    DLLLOCAL bool removeInvalidate(RSet* ors, int tid = q_gettid());

    DLLLOCAL bool inCurrentSet(omap_t::iterator fi) {
        for (size_t i = 0; i < ovec.size(); ++i) {
            if (ovec[i] == fi) {
                return true;
            }
        }
        return false;
    }

    // returns true if there is a lock failure
    DLLLOCAL bool addToRSet(omap_t::iterator oi, RSet* rset, int tid);

    DLLLOCAL void mergeRSet(int i, RSet*& rset);

    DLLLOCAL void mergeRSetIntern(RSet*& rset, RSet* old_rset);

    DLLLOCAL bool makeChain(int i, omap_t::iterator fi, int tid);

#ifdef _QORE_CYCLE_CHECK
    DLLLOCAL void setObjectScanContext(RObject& obj, int rcount);
#endif

private:
    DLLLOCAL RSetHelper(const RSetHelper&);
};

#ifdef _QORE_CYCLE_CHECK
class RSetContextHelper {
public:
    DLLLOCAL RSetContextHelper(RSetHelper& rsh) : rsh(rsh), scan_context_save(rsh.scan_context) {
    }

    DLLLOCAL ~RSetContextHelper() {
        if (rsh.scan_context != scan_context_save) {
            rsh.scan_context = scan_context_save;
        }
    }

private:
    RSetHelper& rsh;
    RObject* scan_context_save;
};

class RSetScanContextHelper {
public:
    DLLLOCAL RSetScanContextHelper(RSetHelper& rsh, RObject* new_context) : rsh(rsh),
            scan_context_save(rsh.scan_context) {
        rsh.scan_context = new_context;
    }

    DLLLOCAL ~RSetScanContextHelper() {
        if (rsh.scan_context != scan_context_save) {
            rsh.scan_context = scan_context_save;
        }
    }

private:
    RSetHelper& rsh;
    RObject* scan_context_save;
};
#endif

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
        deferred_scan;

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
    DLLLOCAL void finalDeref(qore_object_private* obj) {
        assert(!qo);
        qo = obj;
    }

    // mark that we will be deleting the object
    // (and therefore need to wait for any in progress dereferences to complete before deleting)
    DLLLOCAL void willDelete() {
        assert(!del);
        del = true;
    }
};

#endif
