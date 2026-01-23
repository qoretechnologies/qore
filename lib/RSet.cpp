/* -*- indent-tabs-mode: nil -*- */
/*
    RSet.cpp

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

#include <qore/Qore.h>
#include "qore/intern/QoreObjectIntern.h"

RObject::~RObject() {
   assert(!rset);
}

bool RObject::scanCheck(RSetHelper& rsh, AbstractQoreNode* n) {
#ifdef _QORE_CYCLE_CHECK
    rsh.setScanContext(this);
#endif
    return rsh.checkIntern(n);
}

void RObject::setRSet(RSet* rs, int rcnt) {
    assert(rml.checkRSectionExclusive());
    printd(QRO_LVL, "RObject::setRSet() this: %p %s rs: %p rcnt: %d\n", this, getName(), rs, rcnt);
    if (rset) {
        // invalidating the rset removes the weak references to all contained objects
        rset->invalidateDeref();
    }
    rset = rs;
    rcount = rcnt;
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
#ifdef _QORE_CYCLE_CHECK
    refmap.clear();
#endif
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

    // mark that we have a dereference action in progress
    ++ref_inprogress;

    return rv_refs;
}

void RObject::derefDone(bool del) {
    AutoLocker al(rlck);
    // decrement the in progress count, if it's the last thread, and there are waiting threads, then wake one up
    if ((!--ref_inprogress) && ref_waiting) {
        // we have to use broadcast here because the condition variable is shared
        rcond.broadcast();
        assert(!del);
    } else if (del) {
        // if we are going to delete the object, then wait for all other in-progress calls to complete first
        while (ref_inprogress) {
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
            printd(QRO_LVL, "RObject::checkDeferScan() this: %p (%s) rrefs: %d scan OK\n", this, getName(), rrefs);
            return 0;
        }
        if (deferred_scan) {
            printd(QRO_LVL, "RObject::checkDeferScan() this: %p (%s) rrefs: %d deferred_scan already set\n", this,
                getName(), rrefs);
            return -1;
        }
        printd(QRO_LVL, "RObject::checkDeferScan() this: %p (%s) rrefs: %d setting deferred_scan\n", this, getName(),
            rrefs);
        deferred_scan = true;
        // if there is no rset, we can return immediately, no rset can be attached while rrefs > 0
        if (!rset)
            return -1;
        // otherwise we need to invalidate the rset and ensure that
        // rrefs does not go to zero until this is done
        rref_wait = true;
    }

    removeInvalidateRSet();
    AutoLocker al(rlck);
    rref_wait = false;
    if (rref_waiting)
        rcond.broadcast();

    return -1;
}

void RObject::removeInvalidateRSet() {
    QoreAutoVarRWWriteLocker al(rml);
    removeInvalidateRSetIntern();
}

void RObject::removeInvalidateRSetIntern() {
    assert(rml.checkRSectionExclusive());
    if (rset) {
        // invalidating the rset removes the weak references to all contained objects
        rset->invalidateDeref();
        rset = 0;
        rcount = 0;
    }
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
int RSet::canDelete(int ref_copy, int rcount) {
    printd(QRO_LVL, "RSet::canDelete() this: %p valid: %d\n", this, valid);

    if (q_disable_gc)
        return 0;

    if (!valid)
        return -1;

    {
        QoreAutoRWReadLocker al(rwl);
        if (!valid)
            return -1;

        // to avoid race conditions, we only delete in the thread where the references == rcount for the current
        // object
        if (ref_copy != rcount)
            return 0;

        for (rset_t::iterator i = begin(), e = end(); i != e; ++i) {
            // we do not need to lock the object here; if there are no external references, then no external changes can be made
            if (!(*i)->isValid())
                return 0;
            if ((*i)->rcount != (*i)->refs()) {
                printd(QRO_LVL, "RSet::canDelete() this: %p cannot delete graph obj %p '%s' rcount: %d refs: %d\n",
                    this, *i, (*i)->getName(), (*i)->rcount, (*i)->refs());
                return 0;
            }
            printd(QRO_LVL, "RSet::canDelete() this: %p can delete graph obj %p '%s' rcount: %d refs: %d\n", this, *i,
                (*i)->getName(), (*i)->rcount, (*i)->refs());
        }
    }

    // invalidate the rset
    QoreAutoRWWriteLocker al(rwl);
    if (!valid)
        return -1;

    invalidateIntern();

    printd(QRO_LVL, "RSet::canDelete() this: %p can delete all objects in graph\n", this);
    return 1;
}

robject_dereference_helper::robject_dereference_helper(RObject* obj, bool real) : o(obj) {
    refs = obj->deref(real, do_scan, deferred_scan);
    del = !refs;
}

robject_dereference_helper::~robject_dereference_helper() {
    o->derefDone(del);

    if (del && qo) {
        qo->tDeref();
    }
}

class RSectionScanHelper {
protected:
    RSetHelper* orsh;
    RObject* ro;
    unsigned size;

public:
    DLLLOCAL RSectionScanHelper(RSetHelper* n_orsh, RObject* n_ro) : orsh(0), ro(n_ro) {
        int tid = q_gettid();

        // if we already have the rsection lock, then ignore; already processed (either in fomap or tr_out)
        if (n_ro->rml.hasRSectionLock(tid))
            return;

        // try to lock
        if (ro->rml.tryRSectionLockNotifyWaitRead(&n_orsh->notifier)) {
            ro = 0;
            return;
        }

        orsh = n_orsh;
        size = n_orsh->size();
        orsh->inccnt();
    }

    DLLLOCAL ~RSectionScanHelper() {
        if (!orsh)
            return;

        // if no objects were added to the set, then unlock the lock
        if (orsh->size() == size) {
            ro->rml.rSectionUnlock();
            orsh->deccnt();
            return;
        }

        // otherwise try to add the lock to the list to be released at the end of the scan
        orsh->add(ro);
    }

    DLLLOCAL bool lockError() const {
        return !ro;
    }
};

bool RSetHelper::checkIntern(AbstractQoreNode* n) {
    if (!needs_scan(n)) {
        return false;
    }

    printd(5, "RSetHelper::checkIntern() checking %p %s\n", n, get_type_name(n));

#ifdef _QORE_CYCLE_CHECK
    RSetContextHelper rsc(*this);
#endif

    switch (get_node_type(n)) {
        case NT_OBJECT: {
            QoreObject* obj = reinterpret_cast<QoreObject*>(n);
            return checkIntern(*qore_object_private::get(*obj));
        }

        case NT_LIST: {
            QoreListNode* l = reinterpret_cast<QoreListNode*>(n);
            // check if the list has already been scanned
            lset_t::iterator lmi = lset.lower_bound(l);
            if (lmi != lset.end() && *lmi == l) {
                return false;
            }
            lset.insert(lmi, l);

            ListIterator li(l);
            while (li.next()) {
                if (li.getValue().hasNode() && checkIntern(li.getValue().getInternalNode())) {
                    return true;
                }
            }
            return false;
        }

        case NT_HASH: {
            QoreHashNode* h = reinterpret_cast<QoreHashNode*>(n);
            // check if the hash has already been scanned
            hset_t::iterator hmi = hset.lower_bound(h);
            if (hmi != hset.end() && *hmi == h) {
                return false;
            }
            hset.insert(hmi, h);

            HashIterator hi(h);
            while (hi.next()) {
                assert(hi.get().getInternalNode() != h);
                if (hi.get().hasNode() && checkIntern(hi.get().getInternalNode())) {
                    return true;
                }
            }
            return false;
        }

        case NT_RUNTIME_CLOSURE: {
            QoreClosureBase* c = reinterpret_cast<QoreClosureBase*>(n);
            // do not lock or scan if the closure cannot contain any closure-bound local vars with objects or closures
            // (also not through a container)
            if (!c->needsScan()) {
                printd(QRO_LVL, "RSetHelper::checkIntern() closure %p: no scan\n", c);
                return false;
            }
            closure_set_t::iterator ci = closure_set.lower_bound(c);
            // return false if already scanned
            if (ci != closure_set.end() && *ci == c) {
                printd(QRO_LVL, "RSetHelper::checkIntern() found dup closure %p\n", c);
                return false;
            }
            // insert into scanned closure set
            closure_set.insert(ci, c);

            // do not scan any closure object since weak references are used
            // iterate captured lvars
            const cvar_map_t& cmap = c->getMap();

            for (cvar_map_t::const_iterator i = cmap.begin(), e = cmap.end(); i != e; ++i) {
                // do not grab the lock if the lvalue cannot contain an object or closure (also not through a
                // container)
                if (!i->second->needsScan(true)) {
                    printd(QRO_LVL, "RSetHelper::checkIntern() closure %p: var %s: no scan\n", c,
                        i->first->getName());
                    continue;
                }
                RSectionScanHelper rssh(this, i->second);
                if (rssh.lockError()) {
                    return true;
                }
#ifdef DEBUG
                unsigned csize = size();
#endif

                if (checkIntern(*i->second)) {
                    return true;
                }

#ifdef DEBUG
                if (csize != size()) {
                    printd(QRO_LVL, "RSetHelper::checkIntern() closure var '%s' found rref (type: %s)\n",
                        i->first->getName(), i->second->val.getTypeName());
                } else {
                    printd(QRO_LVL, "RSetHelper::checkIntern() closure var '%s' no rref; size: %d (type: %s)\n",
                        i->first->getName(), csize, i->second->val.getTypeName());
                }
#endif
            }

            return false;
        }

        case NT_REFERENCE:
            return lvalue_ref::get(static_cast<ReferenceNode*>(n))->scanReference(*this);
    }

    return false;
}

bool RSetHelper::removeInvalidate(RSet* ors, int tid) {
    // get a list of objects to be invalidated
    rvec_t rovec;

    {
        QoreAutoRWReadLocker al(ors->rwl);

        if (!ors->active())
            return false;

        // first grab all rsection locks
        for (rset_t::iterator ri = ors->begin(), re = ors->end(); ri != re; ++ri) {
            // if we already have the rsection lock, then ignore; already processed (either in fomap or tr_out)
            if ((*ri)->rml.hasRSectionLock(tid))
                continue;

            if ((*ri)->rml.tryRSectionLockNotifyWaitRead(&notifier)) {
                printd(QRO_LVL, "RSetHelper::removeInvalidate() obj %p '%s' cannot enter rsection: tid: %d\n", *ri,
                    (*ri)->getName(), (*ri)->rml.rSectionTid());

                // release other rsection locks
                for (unsigned i = 0; i < rovec.size(); ++i) {
                    rovec[i]->rml.rSectionUnlock();
                    deccnt();
                }
                return true;
            }
#ifdef DEBUG
            // we always have the rsection lock here
            inccnt();
#endif

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

    // now process old rset
    for (unsigned i = 0; i < rovec.size(); ++i) {
        assert(rovec[i]->rml.hasRSectionLock());
        assert(rovec[i]->rset == ors);

        if (rovec[i]->isValid()) {
            tr_out.insert(rovec[i]);
        }
    }

    return false;
}

bool RSetHelper::addToRSet(omap_t::iterator oi, RSet* rset, int tid) {
    // ensure that the current object is not in the rset
    assert(rset->find(oi->first) == rset->end());

    // queue mark object as finalized
    oi->second.finalize(rset);
    assert(oi->second.rset);

    // insert into new rset
    rset->insert(oi->first);

    // queue any nodes not scanned for rset invalidation
    if (oi->first->rset) {
        assert(rset != oi->first->rset);
        if (removeInvalidate(oi->first->rset, tid)) {
            return true;
        }
    }

    printd(QRO_LVL, " + %p '%s': finalized with rset: %p (rcount: %d)\n", oi->first, oi->first->getName(), rset,
        oi->second.rcount);

    assert(!oi->second.in_cycle);
    oi->second.in_cycle = true;
    assert(oi->second.rset);
    // increment rcount
#ifdef _QORE_CYCLE_CHECK
    printd(QRO_LVL, " + %p '%s': second.rset: %p final: %d ok: %d rcount: %d -> %d (<- %p)\n", oi->first,
        oi->first->getName(), oi->second.rset, oi->second.in_cycle, oi->second.ok, oi->second.rcount,
        oi->second.rcount + 1, scan_context);
#endif
    ++oi->second.rcount;
#ifdef _QORE_CYCLE_CHECK
    setObjectScanContext(*oi->first, oi->second.rcount);
#endif
    return false;
}

void RSetHelper::mergeRSet(int i, RSet*& rset) {
    omap_t::iterator oi = ovec[i];
    assert(oi->second.rset);
    assert(rset);
    assert(oi->second.rset != rset);

    printd(QRO_LVL, " + %p '%s': already finalized with rset: %p (size: %d, rcount: %d) current rset: %p "
            "(size: %d)\n", oi->first, oi->first->getName(), oi->second.rset, (int)oi->second.rset->size(),
            oi->second.rcount, rset ? rset : 0, rset ? (int)rset->size() : 0);

    if (rset->size() > oi->second.rset->size()) {
        printd(QRO_LVL, " + %p '%s': rset: %p (%d) assimilating %p (%d)\n", oi->first, oi->first->getName(), rset,
            (int)rset->size(), oi->second.rset, (int)oi->second.rset->size());
        // merge oi->second.rset into rset and retag objects
        RSet* old_rset = oi->second.rset;
        mergeRSetIntern(rset, old_rset);
    } else {
        printd(QRO_LVL, " + %p '%s': oi->second.rset: %p (%d) assimilating %p (%d)\n", oi->first,
            oi->first->getName(), oi->second.rset, (int)oi->second.rset->size(), rset, (int)rset->size());
        // merge rset into oi->second.rset and retag objects
        RSet* old_rset = rset;
        rset = oi->second.rset;
        mergeRSetIntern(rset, old_rset);
    }
}

void RSetHelper::mergeRSetIntern(RSet*& rset, RSet* old_rset) {
    for (rset_t::iterator i = old_rset->begin(), e = old_rset->end(); i != e; ++i) {
        rset->insert(*i);
        omap_t::iterator noi = fomap.find(*i);
        assert(noi != fomap.end());
        noi->second.rset = rset;
    }
    delete old_rset;
}

bool RSetHelper::makeChain(int i, omap_t::iterator fi, int tid) {
    for (++i; i < (int)ovec.size(); ++i) {
#ifdef _QORE_CYCLE_CHECK
        RSetScanContextHelper rsc(*this, i == 0 ? scan_context : ovec[i - 1]->first);
#endif
        if (!ovec[i]->second.rset) {
            printd(QRO_LVL, " + %p '%s': adding parent to rset\n", ovec[i]->first, ovec[i]->first->getName());
            // add the object to the rset and increment rcount
            if (addToRSet(ovec[i], fi->second.rset, tid)) {
                return true;
            }
        } else if (!fi->second.rset) {
            if (addToRSet(fi, ovec[i]->second.rset, tid)) {
                return true;
            }
        } else if (ovec[i]->second.rset != fi->second.rset) {
            assert(fi->second.rset);
            // NOTE: do not increment rcount when merging rsets
            mergeRSet(i, fi->second.rset);
        }
    }
    return false;
}

#ifdef _QORE_CYCLE_CHECK
void RSetHelper::setObjectScanContext(RObject& obj, int rcount) {
    assert(scan_context);
    assert(obj.refmap.find(scan_context) == obj.refmap.end());
    obj.refmap.insert(scan_context);
    assert(obj.refmap.size() == (unsigned)rcount);
}
#endif

// XXX RSectionScanHelper
bool RSetHelper::checkIntern(RObject& obj) {
#ifdef DEBUG
    bool hl = obj.rml.hasRSectionLock();
#endif
    if (obj.rml.tryRSectionLockNotifyWaitRead(&notifier)) {
        printd(QRO_LVL, "RSetHelper::checkIntern() + obj %p '%s' cannot enter rsection: rsection tid: %d\n", &obj,
            obj.getName(), obj.rml.rSectionTid());
        return true;
    }
#ifdef DEBUG
    if (!hl) {
        inccnt();
    }
#endif

    // check object status; do not scan invalid objects or objects being deleted
    if (!obj.isValid()) {
        obj.rml.rSectionUnlock();
        deccnt();
        return false;
    }

    int tid = q_gettid();

    // see if the object has been scanned
    omap_t::iterator fi = fomap.lower_bound(&obj);
    if (fi != fomap.end() && fi->first == &obj) {
        printd(QRO_LVL, "RSetHelper::checkIntern() + found obj %p '%s' rcount: %d in_cycle: %d ok: %d\n", &obj,
            obj.getName(), fi->second.rcount, fi->second.in_cycle, fi->second.ok);

        if (fi->second.ok) {
            assert(!fi->second.in_cycle);
            printd(QRO_LVL, " + %p '%s' already scanned and ok\n", &obj, obj.getName());
            return false;
        }

        if (fi->second.in_cycle) {
            assert(fi->second.rset);
            // check if this object is part of the current cycle already - if
            // 1) it's already in the current scan vector, or
            // 2) the parent object of the current object is already a part of the recursive set
            if (inCurrentSet(fi)) {
#ifdef _QORE_CYCLE_CHECK
                printd(QRO_LVL, " + recursive obj %p '%s' already finalized and in current cycle "
                    "(rcount: %d -> %d) <- %p\n", &obj, obj.getName(), fi->second.rcount, fi->second.rcount + 1,
                    scan_context);
#endif
                ++fi->second.rcount;
#ifdef _QORE_CYCLE_CHECK
                setObjectScanContext(obj, fi->second.rcount);
#endif
                // rcount can never be more than real references for the target object
                assert(fi->first->references >= fi->second.rcount);
            } else if (!ovec.empty()) {
                // FIXME: use this optimization in the loop below
                if (ovec.back()->second.rset == fi->second.rset) {
#ifdef _QORE_CYCLE_CHECK
                    printd(QRO_LVL, " + %p '%s': parent object %p '%s' in same cycle (rcount: %d -> %d) <- %p\n", &obj,
                        obj.getName(), ovec.back()->first, ovec.back()->first->getName(), fi->second.rcount,
                        fi->second.rcount + 1, scan_context);
#endif
                    ++fi->second.rcount;
#ifdef _QORE_CYCLE_CHECK
                    setObjectScanContext(obj, fi->second.rcount);
#endif
                    // rcount can never be more than real references for the target object
                    assert(fi->first->references >= fi->second.rcount);
                    return false;
                }

                // see if any parent of the current object is already in the same recursive cycle, if so, we have a
                // new chain (quick comparison first)
                for (int i = ovec.size() - 1; i >= 0; --i) {
                    if (fi->second.rset == ovec[i]->second.rset) {
                        printd(QRO_LVL, " + recursive obj %p '%s' already finalized, cyclic ancestor %p '%s' in "
                            "current cycle\n", &obj, obj.getName(), ovec[i]->first, ovec[i]->first->getName());
                        return makeChain(i, fi, tid);
                    }
                }

                // see if any parent of the current object is already in a recursive cycle to be joined, if so, we
                // have a new chain (slower comparison second)
                for (int i = ovec.size() - 1; i >= 0; --i) {
                    if (fi->second.rset->find((ovec[i])->first) != fi->second.rset->end()) {
                        printd(QRO_LVL, " + recursive obj %p '%s' already finalized, cyclic ancestor %p '%s' in "
                            "current cycle\n", &obj, obj.getName(), ovec[i]->first, ovec[i]->first->getName());
                        return makeChain(i, fi, tid);
                    }
                }

                printd(QRO_LVL, " + recursive obj %p '%s' already finalized but not in current cycle\n", &obj,
                    obj.getName());
                return false;
            }
        } else {
            if (!inCurrentSet(fi)) {
                printd(QRO_LVL, " + recursive obj %p '%s' not in current cycle\n", &obj, obj.getName());
                return false;
            }
        }

        // finalize current objects immediately
        RSet* rset = fi->second.rset;
#ifdef DEBUG
        if (rset)
            printd(QRO_LVL, " + %p '%s': reusing RSet: %p\n", &obj, obj.getName(), rset);
#endif

        // issue #xxxx: save in_cycle state and rset before walk-back to detect forward-edges
        // that need to be counted when:
        // 1. An object joins an existing cycle (was not in_cycle, successor was)
        // 2. Two separate cycles are merged (both were in_cycle but different rsets)
        std::vector<bool> was_in_cycle;
        std::vector<RSet*> was_rset;
        was_in_cycle.reserve(ovec.size());
        was_rset.reserve(ovec.size());
        for (auto& it : ovec) {
            was_in_cycle.push_back(it->second.in_cycle);
            was_rset.push_back(it->second.rset);
        }

        int i = (int)ovec.size() - 1;
        while (i >= 0) {
#ifdef _QORE_CYCLE_CHECK
            RSetScanContextHelper rsc(*this, i == 0 ? scan_context : ovec[i - 1]->first);
#endif
            assert(i >= 0);

            // get iterator to object record
            omap_t::iterator oi = ovec[i];

            // add object to rset or merge rsets
            if (!oi->second.rset) {
                if (!rset) {
                    rset = new RSet;
                    printd(QRO_LVL, " + %p '%s': rcycle: %d second.rset: %p new RSet: %p\n", oi->first,
                        oi->first->getName(), obj.rcycle, oi->second.rset, rset);
                }

                if (addToRSet(oi, rset, tid)) {
                    return true;
                }
            } else if (!rset) {
                rset = oi->second.rset;
                if (addToRSet(fi, rset, tid)) {
                    return true;
                }
            } else if (oi->second.rset != rset) {
                assert(rset);
                // NOTE: do not increment rcount when merging rsets
                mergeRSet(i, rset);
            }

            if (oi->first == &obj) {
                break;
            }

            --i;
        }

        // issue #xxxx: count forward-edges that were not counted during the walk-back
        // The edge from ovec[j] to ovec[j+1] needs to be counted in ovec[j+1]'s rcount if:
        // 1. ovec[j] was not in a cycle before, and ovec[j+1] was already in a cycle
        //    (the edge wasn't counted because ovec[j+1] was already finalized when first encountered)
        // 2. Both were in cycles but DIFFERENT rsets before, and are now in the same rset
        //    (the edge wasn't counted because they were in separate cycles)
        for (size_t j = 0; j + 1 < ovec.size(); ++j) {
            omap_t::iterator curr_oi = ovec[j];
            omap_t::iterator next_oi = ovec[j + 1];

            // Skip if they're not in the same rset now
            if (!curr_oi->second.rset || next_oi->second.rset != curr_oi->second.rset) {
                continue;
            }

            bool should_count = false;

            if (!was_in_cycle[j] && was_in_cycle[j + 1]) {
                // Case 1: ovec[j] just joined the cycle, ovec[j+1] was already in a cycle
                should_count = true;
            } else if (was_in_cycle[j] && was_in_cycle[j + 1] && was_rset[j] != was_rset[j + 1]) {
                // Case 2: Both were in cycles but different rsets - now merged
                should_count = true;
            }

            if (should_count) {
                printd(QRO_LVL, " + %p '%s': counting forward-edge to %p '%s' (rcount: %d -> %d)\n",
                    curr_oi->first, curr_oi->first->getName(),
                    next_oi->first, next_oi->first->getName(),
                    next_oi->second.rcount, next_oi->second.rcount + 1);
                ++next_oi->second.rcount;
            }
        }

        return false;
    } else {
        printd(QRO_LVL, "RSetHelper::checkIntern() + adding new obj %p '%s' setting rcount = 0 (current: %d "
            "rset: %p)\n", &obj, obj.getName(), obj.rcount, obj.rset);

        // insert into total scanned object set
        fi = fomap.insert(fi, omap_t::value_type(&obj, RSetStat()));

        // check if the object should be iterated
        if (!obj.needsScan(true)) {
            // remove from invalidation set if present
            tr_out.erase(&obj);

            printd(QRO_LVL, "RSetHelper::checkIntern() obj %p '%s' will not be iterated since object count is 0\n",
                &obj, obj.getName());
            fi->second.ok = true;
            assert(!fi->second.rset);
            return false;
        }
    }

    // push on current vector chain
    ovec.push_back(fi);

    // remove from invalidation set if present
    tr_out.erase(&obj);

    // recursively check data members
#ifdef _QORE_CYCLE_CHECK
    RSetScanContextHelper sch(*this, &obj);
#endif
    if (obj.scanMembers(*this)) {
        return true;
    }

    // remove from current vector chain
    ovec.pop_back();

    return false;
}

class RScanHelper {
public:
    RObject& obj;
    int rcycle;

    DLLLOCAL RScanHelper(RObject& o) : obj(o), rcycle(o.rcycle) {
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

    DLLLOCAL bool needScan() const {
        return rcycle == obj.rcycle;
    }
};

RSetHelper::RSetHelper(RObject& obj) {
#ifdef DEBUG
    lcnt = 0;
#endif
    if (q_disable_gc) {
        return;
    }

    // if the scan should be deferred
    if (obj.checkDeferScan()) {
        return;
    }

    printd(QRO_LVL, "RSetHelper::RSetHelper() this: %p (%p %s) ENTER\n", this, &obj, obj.getName());

    RScanHelper rsh(obj);

    while (true) {
        if (checkIntern(obj)) {
            rollback();
            // wait for foreign transaction to finish if necessary
            notifier.wait();

            if (!rsh.needScan()) {
                printd(QRO_LVL, "RSetHelper::RSetHelper() this: %p (%p: %s) TRANSACTION COMPLETE IN ANOTHER THREAD\n",
                    this, &obj, obj.getName());
                return;
            }
            printd(QRO_LVL, "RSetHelper::RSetHelper() this: %p (%p: %s) RESTARTING TRANSACTION: %d\n", this, &obj,
                obj.getName(), obj.rcycle);
            continue;
        }

        break;
    }

    if (obj.isValid()) {
        commit(obj);
    }

    printd(QRO_LVL, "RSetHelper::RSetHelper() this: %p (%p) EXIT\n", this, &obj);
}

void RSetHelper::commit(RObject& obj) {
#ifdef DEBUG
    bool has_obj = false;
#endif

    assert(obj.rml.checkRSectionExclusive());

    // invalidate rsets
    for (rs_set_t::iterator i = tr_invalidate.begin(), e = tr_invalidate.end(); i != e; ++i) {
        (*i)->invalidate();
    }

    // unlock rsection
    for (rset_t::iterator i = tr_out.begin(), e = tr_out.end(); i != e; ++i) {
        assert(fomap.find(*i) == fomap.end());
#ifdef _QORE_CYCLE_CHECK
        (*i)->refmap.clear();
#endif
        (*i)->rml.rSectionUnlock();
        deccnt();
    }

    // finalize graph - exit rsection
#ifdef DEBUG
    // DEBUG
    for (omap_t::iterator i = fomap.begin(), e = fomap.end(); i != e; ++i) {
        RObject* tobj = i->first;

        assert(qore_var_rwlock_priv::get(tobj->rml)->write_tid >= -1);

        RSet* rs = i->second.rset;
        int rcount = i->second.rcount;
        assert(tr_out.find(tobj) == tr_out.end());
        tobj->setRSet(rs, rcount);

        if (tobj == &obj)
            has_obj = true;
    }

    for (omap_t::iterator i = fomap.begin(), e = fomap.end(); i != e; ++i) {
        RSet* rs = i->second.rset;
        assert(!rs || (rs->size() == rs->getCount()));

        if (!rs) {
            continue;
        }
        for (rset_t::iterator ri = rs->begin(), re = rs->end(); ri != re; ++ri) {
            RObject* o = (*ri);
            assert(o->rset == rs);
        }
    }

    for (omap_t::iterator i = fomap.begin(), e = fomap.end(); i != e; ++i) {
        i->first->rml.rSectionUnlock();
        deccnt();
    }
#else
    for (omap_t::iterator i = fomap.begin(), e = fomap.end(); i != e; ++i) {
        i->first->setRSet(i->second.rset, i->second.rcount);
    }
    for (omap_t::iterator i = fomap.begin(), e = fomap.end(); i != e; ++i) {
        i->first->rml.rSectionUnlock();
    }
#endif

    assert(fomap.empty() || has_obj);
    assert(!lcnt);
}

void RSetHelper::rollback() {
    for (omap_t::iterator i = fomap.begin(), e = fomap.end(); i != e; ++i) {
        if (i->second.rset) {
            RSet* r = i->second.rset;
            if (r->pop())
                delete r;
        }
#ifdef _QORE_CYCLE_CHECK
        i->first->refmap.clear();
#endif
        i->first->rml.rSectionUnlock();
        deccnt();
   }

    // exit rsection of objects in tr_out
    for (rset_t::iterator i = tr_out.begin(), e = tr_out.end(); i != e; ++i) {
        assert(fomap.find(*i) == fomap.end());
#ifdef _QORE_CYCLE_CHECK
        (*i)->refmap.clear();
#endif
        (*i)->rml.rSectionUnlock();
        deccnt();
    }

    fomap.clear();
    ovec.clear();
    tr_out.clear();
    tr_invalidate.clear();

    assert(!lcnt);

#ifdef _POSIX_PRIORITY_SCHEDULING
    sched_yield();
#endif
}
