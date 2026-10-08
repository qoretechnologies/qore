/*
    VLock.cpp

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
#include <qore/AutoVLock.h>

#include <cassert>
#include <cerrno>
#include <string>
#include <vector>

struct qore_obj_notification_s {
    QoreObject* obj;
    std::string member;

    DLLLOCAL qore_obj_notification_s(QoreObject* o, const char* m) : obj(o), member(m) {}
};

typedef std::vector<qore_obj_notification_s> qore_notify_list_t;

struct qore_avl_private {
    qore_notify_list_t *notify_list;

    DLLLOCAL qore_avl_private() : notify_list(0) {
    }

    DLLLOCAL ~qore_avl_private() {
        if (notify_list)
            delete notify_list;
    }

    DLLLOCAL void add(QoreObject* obj, const char* mem) {
        //printd(5, "qore_avl_private::add(%p, '%s')\n", obj, mem);
        if (!notify_list)
            notify_list = new qore_notify_list_t;
        else {
            for (qore_notify_list_t::iterator i = notify_list->begin(), e = notify_list->end(); i != e; ++i)
                if ((*notify_list)[0].obj == obj && (*notify_list)[0].member != mem)
                    return;
        }

        notify_list->push_back(qore_obj_notification_s(obj, mem));
    }
};

AutoVLock::AutoVLock(ExceptionSink* n_xsink) : o(0), xsink(n_xsink), priv(0) {
    //printd(5, "AutoVLock::AutoVLock() this=%p\n", this);
}

AutoVLock::~AutoVLock() {
    //printd(5, "AutoVLock::~AutoVLock() this=%p size=%d\n", this, size());
    del();
    if (priv && priv->notify_list) {
        ExceptionSink xsink2;

        for (qore_notify_list_t::iterator i = priv->notify_list->begin(), e = priv->notify_list->end(); i != e; ++i) {
            // run member notifications regardless of exception status
            //printd(5, "posting notification to object %p, member %s\n", i->obj, i->member.c_str());
            i->obj->execMemberNotification(i->member.c_str(), &xsink2);
        }
        xsink->assimilate(xsink2);
    }

    delete priv;
}

AutoVLock::operator bool() const {
    return lock.isSet();
}

void AutoVLock::del() {
    if (lock.isSet()) {
        lock.unlockAndClear();
        if (o) {
            o->tDeref();
            o = nullptr;
        }
    }
    assert(!o);
}

void AutoVLock::clear() {
    if (lock.isSet()) {
        lock.clear();
        if (o) {
            o = nullptr;
        }
    }
    assert(!o);
}

void AutoVLock::set(QoreVarRWLock* n_m) {
    assert(!lock.isSet());
    lock.set(n_m);
}

void AutoVLock::set(QoreObject* n_o, QoreVarRWLock* n_m) {
    assert(!lock.isSet());
    o = n_o;
    lock.set(n_m);
}

QoreVarRWLock* AutoVLock::getRWL() const {
    return lock.getRWL();
}

QoreObject* AutoVLock::getObject() const {
    return o;
}

void AutoVLock::addMemberNotification(QoreObject* obj, const char* member) {
    // ignore member notifications for updates made within the class
    if (obj == runtime_get_stack_object() || !obj->hasMemberNotification()) {
        return;
    }

    if (!priv) {
        priv = new qore_avl_private;
    }

    priv->add(obj, member);
}

VLock::VLock(int tid) : waiting_on(nullptr), tid(tid) {

}

int VLock::condWait(QoreCondition& cond, AbstractSmartLock* asl, int64 timeout_ms) {
    if (!cancel_op) {
        return timeout_ms ? cond.wait2(&asl->asl_lock, timeout_ms) : cond.wait(&asl->asl_lock);
    }
    int rc = cond.waitWithInterrupt(&asl->asl_lock, timeout_ms ? timeout_ms : -1);
    if (rc == QORE_COND_RESULT_INTERRUPTED) {
        return QORE_VLOCK_WAIT_CANCELLED;
    }
    return rc == QORE_COND_RESULT_TIMEOUT ? ETIMEDOUT : 0;
}

int VLock::waitIntern(QoreCondition& cond, AbstractSmartLock* asl, ExceptionSink* xsink, int timeout_ms) {
    int rc = condWait(cond, asl, timeout_ms);
    if (rc != QORE_VLOCK_WAIT_CANCELLED) {
        return rc;
    }
    // a request that is not to be delivered (out of the thread's scope, now dropped) leaves the caller to recheck
    // the lock and wait again
    return raiseCancel(xsink) ? -1 : 0;
}

int VLock::waitOn(AbstractSmartLock* asl, VLock* vl, ExceptionSink* xsink, int timeout_ms) {
    assert(!waiting_on.load());
    waiting_on.store(asl);

    int rc = 0;
    AbstractSmartLock* vl_wait = vl->waiting_on.load();
    //printd(5, "VLock::waitOn(asl=%p) vl_wait=%p other_tid=%d\n", asl, vl_wait, vl->tid);
    if (vl_wait && find(vl_wait)) {
        // NOTE: we throw an exception here anyway as a deadlock is a programming mistake and therefore should be
        // visible to the programmer (even if it really wouldn't technically deadlock at this point due to the
        // timeout)
        if (timeout_ms) {
            xsink->raiseException("THREAD-DEADLOCK", "TID %d and %d would deadlock on the same resources; this "
                "represents a programming error, so, even though a %s method was called with a timeout and therefore "
                "would not technically deadlock at this point, this exception is thrown anyway", vl->tid, tid,
                asl->getName());
        } else {
            xsink->raiseException("THREAD-DEADLOCK", "TID %d and %d have deadlocked trying to acquire the same "
                "resources", vl->tid, tid);
        }
        rc = -1;
    }

    if (!rc) {
        //printd(0, "AbstractSmartLock::block() this=%p asl=%p about to block on VRMutex owned by TID %d\n", this,
        //    asl, vl ? vl->tid : -1);
        rc = waitIntern(asl->asl_cond, asl, xsink, timeout_ms);
        //printd(0, "AbstractSmartLock::block() this=%p asl=%p regrabbed lock\n", this, asl);
    }

    waiting_on.store(nullptr);
    return rc;
}

int VLock::waitOn(AbstractSmartLock* asl, QoreCondition *cond, VLock* vl, ExceptionSink* xsink, int timeout_ms) {
    assert(!waiting_on.load());
    waiting_on.store(asl);

    int rc = 0;
    AbstractSmartLock* vl_wait = vl->waiting_on.load();
    //printd(5, "VLock::waitOn(asl=%p) vl_wait=%p other_tid=%d\n", asl, vl_wait, vl->tid);
    if (vl_wait && find(vl_wait)) {
        // NOTE: we throw an exception here anyway as a deadlock is a programming mistake and therefore should be
        // visible to the programmer (even if it really wouldn't technically deadlock at this point due to the
        // timeout)
        if (timeout_ms) {
            xsink->raiseException("THREAD-DEADLOCK", "TID %d and %d would deadlock on the same resources; this "
                "represents a programming error, so, even though a %s method was called with a timeout and therefore "
                "would not technically deadlock at this point, this exception is thrown anyway", vl->tid, tid,
                asl->getName());
        } else {
            xsink->raiseException("THREAD-DEADLOCK", "TID %d and %d have deadlocked trying to acquire the same "
                "resources", vl->tid, tid);
        }
        rc = -1;
    }

    if (!rc) {
        //printd(0, "AbstractSmartLock::block() this=%p asl=%p about to block on VRMutex owned by TID %d\n", this,
        //    asl, vl ? vl->tid : -1);
        rc = waitIntern(*cond, asl, xsink, timeout_ms);
        //printd(0, "AbstractSmartLock::block() this=%p asl=%p regrabbed lock\n", this, asl);
    }

    waiting_on.store(nullptr);
    return rc;
}

int VLock::waitOn(AbstractSmartLock* asl, vlock_map_t& vmap, ExceptionSink* xsink, int timeout_ms) {
    assert(!waiting_on.load());
    waiting_on.store(asl);

    int rc = 0;
    for (vlock_map_t::iterator i = vmap.begin(), e = vmap.end(); i != e; ++i) {
        AbstractSmartLock* vl_wait = i->second->waiting_on.load();
        //printd(5, "VLock::waitOn(asl=%p, vmap size=%d) vl_wait=%p other_tid=%d\n", asl, vmap.size(), vl_wait,
        //    i->second->tid);
        if (vl_wait && find(vl_wait)) {
            // NOTE: we throw an exception here anyway as a deadlock is a programming mistake and therefore should be
            // visible to the programmer (even if it really wouldn't technically deadlock at this point due to the
            // timeout)
            if (timeout_ms) {
                xsink->raiseException("THREAD-DEADLOCK", "TID %d and %d would deadlock on the same resources; this "
                    "represents a programming error, so, even though a %s method was called with a timeout and "
                    "therefore would not technically deadlock at this point, this exception is thrown anyway",
                    i->second->tid, tid, asl->getName());
            } else {
                xsink->raiseException("THREAD-DEADLOCK", "TID %d and %d have deadlocked trying to acquire the same "
                    "resources", i->second->tid, tid);
            }
            rc = -1;
            break;
        }
    }

    if (!rc) {
        //printd(0, "AbstractSmartLock::block() this=%p asl=%p about to block on VRMutex owned by TID %d\n", this,
        //    asl, vl ? vl->tid : -1);
        rc = waitIntern(asl->asl_cond, asl, xsink, timeout_ms);
        //printd(0, "AbstractSmartLock::block() this=%p asl=%p regrabbed lock\n", this, asl);
    }

    waiting_on.store(nullptr);
    return rc;
}

#ifdef DEBUG
void VLock::show(class VLock* vl) const {
    //printd(0, "VLock::show() this=%p, vl=%p vl->waiting_on=%p (in this=%p)\n", this, vl, vl ? vl->waiting_on : 0, vl ? find(vl->waiting_on) : 0);
}
#endif

void VLock::push(AbstractSmartLock* g) {
    //printd(5, "VLock::push() this=%p asl=%p size=%d\n", this, g, size());
    push_back(g);
}

int VLock::pop(AbstractSmartLock* g) {
    assert(begin() != end());

    if (g == back()) {
        pop_back();
        return 0;
    }

    abstract_lock_list_t::iterator i = end();
    --i;
    --i;
    while (*i != g) {
        --i;
    }

    erase(i);
    return -1;
}

AbstractSmartLock* VLock::find(AbstractSmartLock* g) const {
    for (auto& i : *this) {
        if (i == g) {
            return g;
        }
    }
    return nullptr;
}
