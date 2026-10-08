/*
    QoreQueue.cpp

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
#include <qore/QoreQueue.h>
#include "qore/intern/QoreQueueIntern.h"
#include "qore/intern/QoreObjectIntern.h"

#include <cerrno>
#include <sys/time.h>

void Queue::deref(ExceptionSink* xsink) {
    if (ROdereference()) {
        priv->destructor(xsink);
        delete this;
    }
}

void qore_queue_private::destructor(ExceptionSink* xsink) {
    // declared before the lock, so that the weak reference is released after it
    ScanHolderRelease release;
    AutoLocker al(&l);
    if (unsigned rw = getReadWaitingIntern()) {
        xsink->raiseException("QUEUE-ERROR", "Queue deleted while there %s %d waiting thread%s for reading", rw == 1 ? "is" : "are", rw, rw == 1 ? "" : "s");
        read_cond.broadcast();
    }
    if (unsigned ww = getWriteWaitingIntern()) {
        xsink->raiseException("QUEUE-ERROR", "Queue deleted while there %s %d waiting thread%s for writing", ww == 1 ? "is" : "are", ww, ww == 1 ? "" : "s");
        write_cond.broadcast();
    }

    uncountAllIntern(release);
    clearIntern(xsink);
    setLenIntern(Queue_Deleted);
    if (desc) {
        desc->deref();
        desc = nullptr;
    }
}

void qore_queue_private::clearIntern(ExceptionSink* xsink) {
    while (head) {
        printd(5, "qore_queue_private::clearIntern() this: %p deleting %p (node '%s' %s)\n", this, head, head->node.getTypeName(), head->node.getType());
        QoreQueueNode* w = head->next;
        head->del(xsink);
        head = w;
    }
    head = nullptr;
    tail = nullptr;
}

qore_queue_private::ScanHolderRelease::~ScanHolderRelease() {
    if (obj) {
        obj->tDeref();
    }
}

void qore_queue_private::markRemovedIntern() {
    // the queue only reports its values to a scan while it has a scan holder (see scanMembers())
    if (scan_holder) {
        scan_holder->edgesRemoved();
    }
}

void qore_queue_private::countIntern(QoreObject& self) {
    qore_object_private* obj = qore_object_private::get(self);
    if (!scan_count++) {
        assert(!scan_holder);
        // the object's count follows the queue's under the lock (see scan_count)
        obj->incScanPrivateData();
        obj->tRef();
        scan_holder = obj;
    } else {
        // a queue is the private data of one object
        assert(scan_holder == obj);
    }
}

void qore_queue_private::uncountIntern(ScanHolderRelease& release) {
    assert(scan_count > 0);
    assert(scan_holder);
    if (!--scan_count) {
        releaseScanHolderIntern(release);
    }
}

void qore_queue_private::uncountAllIntern(ScanHolderRelease& release) {
    if (scan_count) {
        scan_count = 0;
        releaseScanHolderIntern(release);
    }
}

void qore_queue_private::releaseScanHolderIntern(ScanHolderRelease& release) {
    assert(!scan_count);
    assert(scan_holder);
    // the object's count follows the queue's under the lock (see scan_count)
    scan_holder->decScanPrivateData();
    assert(!release.obj);
    release.obj = scan_holder;
    scan_holder = nullptr;
}

int qore_queue_private::waitReadIntern(ExceptionSink *xsink, int timeout_ms) {
    // if there is no data, then wait for condition variable
    while (!head) {
        if (!err.empty()) {
            xsink->raiseException(err.c_str(), desc->stringRefSelf());
            return QW_ERROR;
        }

        int rc;
        // issue #4077: do not call QoreCondition::wait() with a negative timeout value
        if (timeout_ms >= 0) {
            incWaitingIntern(read_waiting);
            // Use interruptible wait for sandbox support
            // Queue semantics: 0 = infinite wait, but waitWithInterrupt uses -1 for infinite
            int64 cond_timeout = (timeout_ms == 0) ? -1 : timeout_ms;
            rc = read_cond.waitWithInterrupt(l, cond_timeout, xsink);
            decWaitingIntern(read_waiting);
            // Check for interrupt
            if (rc == QORE_COND_RESULT_INTERRUPTED) {
                return QW_ERROR;  // Exception already raised
            }
        } else {
            rc = QORE_COND_RESULT_TIMEOUT;
        }

        if (rc == QORE_COND_RESULT_TIMEOUT) {
#ifdef DEBUG
            // if an error has occurred, then it must be due to a timeout
            if (!timeout_ms)
                printd(0, "qore_queue_private::waitReadIntern(timeout_ms=0) this: %p pthread_cond_wait() returned rc: %d\n", this, rc);
#endif
            assert(timeout_ms);
            return QW_TIMEOUT;
        }

        if (getLenIntern() == Queue_Deleted) {
            xsink->raiseException("QUEUE-ERROR", "Queue has been deleted in another thread");
            return QW_DEL;
        }
    }

    if (!err.empty()) {
        xsink->raiseException(err.c_str(), desc->stringRefSelf());
        return QW_ERROR;
    }

    return 0;
}

int qore_queue_private::waitWriteIntern(ExceptionSink *xsink, int timeout_ms) {
    // if the queue is full, then wait for condition variable
    while (max > 0 && getLenIntern() >= max) {
        if (!err.empty()) {
            xsink->raiseException(err.c_str(), desc->stringRefSelf());
            return QW_ERROR;
        }

        int rc;
        // issue #4077: do not call QoreCondition::wait() with a negative timeout value
        if (timeout_ms >= 0) {
            incWaitingIntern(write_waiting);
            // Use interruptible wait for sandbox support
            // Queue semantics: 0 = infinite wait, but waitWithInterrupt uses -1 for infinite
            int64 cond_timeout = (timeout_ms == 0) ? -1 : timeout_ms;
            rc = write_cond.waitWithInterrupt(l, cond_timeout, xsink);
            decWaitingIntern(write_waiting);
            // Check for interrupt
            if (rc == QORE_COND_RESULT_INTERRUPTED) {
                return QW_ERROR;  // Exception already raised
            }
        } else {
            rc = QORE_COND_RESULT_TIMEOUT;
        }

        if (rc == QORE_COND_RESULT_TIMEOUT) {
#ifdef DEBUG
            // if an error has occurred, then it must be due to a timeout
            if (!timeout_ms)
                printd(0, "qore_queue_private::waitWriteIntern(timeout_ms=0) this: %p pthread_cond_wait() returned rc: %d\n", this, rc);
#endif
            assert(timeout_ms);
            return QW_TIMEOUT;
        }

        if (getLenIntern() == Queue_Deleted) {
            xsink->raiseException("QUEUE-ERROR", "Queue has been deleted in another thread");
            return QW_DEL;
        }
    }

    if (!err.empty()) {
        xsink->raiseException(err.c_str(), desc->stringRefSelf());
        return QW_ERROR;
    }

    return 0;
}

void qore_queue_private::pushNode(QoreValue v) {
    if (!head) {
        head = new QoreQueueNode(v, 0, 0);
        tail = head;
    } else {
        QoreQueueNode* qn = new QoreQueueNode(v, tail, 0);
        tail->next = qn;
        tail = qn;
    }
    addLenIntern(1);

    //printd(5, "qore_queue_private::pushNode(%p '%s') this: %p head: %p (%p) tail: %p (%p) read_waiting: %d len: %d\n", v, get_type_name(v), this, head, head->node, tail, tail->node, read_waiting, len);
}

void qore_queue_private::pushIntern(QoreValue v) {
    pushNode(v);
    //printd(5, "qore_queue_private::push_internal(%p) this: %p head: %p (%p) tail: %p (%p) waiting: %d len: %d\n", v, this, head, head->node, tail, tail->node, waiting, len);

    // signal waiting thread to wakeup and process event
    if (getReadWaitingIntern()) {
        read_cond.signal();
    }
}

void qore_queue_private::insertIntern(QoreValue v) {
    if (!head) {
        head = new QoreQueueNode(v, 0, 0);
        tail = head;
    } else {
        QoreQueueNode* qn = new QoreQueueNode(v, 0, head);
        head->prev = qn;
        head = qn;
    }
    addLenIntern(1);

    //printd(5, "qore_queue_private::insertIntern(%p) this: %p head: %p (%p) tail: %p (%p) waiting: %d len: %d\n", v, this, head, head->node, tail, tail->node, waiting, len);

    // signal waiting thread to wakeup and process event
    if (getReadWaitingIntern()) {
        read_cond.signal();
    }
}

int qore_queue_private::checkWriteIntern(ExceptionSink* xsink, bool always_error) {
    if (getLenIntern() == Queue_Deleted) {
        if (always_error) {
            xsink->raiseException("QUEUE-ERROR", "Queue has been deleted in another thread");
        }
        return -1;
    }
    if (!err.empty()) {
        xsink->raiseException(err.c_str(), desc->stringRefSelf());
        return -1;
    }
    return 0;
}

void qore_queue_private::pushAndTakeRef(QoreValue n) {
    {
        AutoLocker al(&l);
        if (getLenIntern() != Queue_Deleted && err.empty()) {
            assert(max == -1);

            printd(5, "qore_queue_private::pushAndTakeRef('%s') this: %p\n", n.getTypeName(), this);
            // the queue takes the reference
            pushIntern(n);
            return;
        }
    }

    // a deleted queue or a queue in an error state does not take the value, so the reference is released here,
    // outside the lock; this API has no exception sink, so any exception is reported by the temporary one
    ExceptionSink xsink;
    n.discard(&xsink);
}

void qore_queue_private::push(ExceptionSink* xsink, QoreObject* self, QoreValue n, int timeout_ms, bool& to) {
    to = false;
    ValueHolder holder(n, xsink);

    {
        AutoLocker al(&l);
        if (checkWriteIntern(xsink)) {
            return;
        }

        {
            int rc = waitWriteIntern(xsink, timeout_ms);
            if (rc == QW_TIMEOUT) {
                to = true;
            }
            if (rc) {
                return;
            }
        }

        pushIntern(holder.release());
        if (self && needs_scan(n)) {
            tail->scan_counted = true;
            countIntern(*self);
            // no scan follows the new edge (Pattern B in design/dgc.md); marked under the lock, as another thread
            // can take the value and release it as soon as the lock is released
            qore_dgc_value_stored(*qore_object_private::get(*self), n);
        }
    }
}

void qore_queue_private::insert(ExceptionSink* xsink, QoreObject* self, QoreValue n, int timeout_ms, bool& to) {
    to = false;
    ValueHolder holder(n, xsink);

    {
        AutoLocker al(&l);
        if (checkWriteIntern(xsink)) {
            return;
        }

        {
            int rc = waitWriteIntern(xsink, timeout_ms);
            if (rc == QW_TIMEOUT) {
                to = true;
            }
            if (rc) {
                return;
            }
        }

        insertIntern(holder.release());
        if (self && needs_scan(n)) {
            head->scan_counted = true;
            countIntern(*self);
            // no scan follows the new edge (Pattern B in design/dgc.md); marked under the lock, as another thread
            // can take the value and release it as soon as the lock is released
            qore_dgc_value_stored(*qore_object_private::get(*self), n);
        }
    }
}

QoreValue qore_queue_private::shift(ExceptionSink* xsink, QoreObject* self, int timeout_ms, bool& to) {
    to = false;
    QoreValue rv{};
    {
        // declared before the lock, so that the weak reference is released after it
        ScanHolderRelease release;
        SafeLocker sl(&l);

        if (checkWriteIntern(xsink, true)) {
            return QoreValue();
        }

#ifdef DEBUG
        //if (!head) printd(5, "qore_queue_private::shift(timeout_ms: %d) WAITING this: %p head: %p tail: %p waiting: %d len: %d\n", timeout_ms, this, head, tail, waiting, len);
#endif

        {
            int rc = waitReadIntern(xsink, timeout_ms);
            if (rc == QW_TIMEOUT) {
                to = true;
            }
            if (rc) {
                return QoreValue();
            }
        }

        //printd(5, "qore_queue_private::shift() GOT DATA this: %p head: %p (rv: %p '%s') tail: %p (%p) write_waiting: %d len: %d\n", this, head, head->node, get_type_name(head->node), tail, tail->node, write_waiting, len);

        QoreQueueNode* n = head;
        head = head->next;
        if (!head) {
            tail = nullptr;
        } else {
            head->prev = nullptr;
        }

        addLenIntern(-1);
        if (getWriteWaitingIntern()) {
            write_cond.signal();
        }

        // the value is handed to the caller with the queue's reference, which the object's recursive set may count
        // as internal: marked under the lock, before the caller can release it (see RObject::remove_gen)
        if (needs_scan(n->node)) {
            markRemovedIntern();
        }

        // the scan count is maintained under the lock, as in push() and insert(); the queue holds the object
        // whose count includes it, so a value taken through the C++ API without the Queue object is uncounted too
        if (n->scan_counted) {
            assert(!self || qore_object_private::get(*self) == scan_holder);
            uncountIntern(release);
        }

        sl.unlock();
        rv = n->takeAndDel();
    }

    return rv;
}

QoreValue qore_queue_private::pop(ExceptionSink* xsink, QoreObject* self, int timeout_ms, bool& to) {
    to = false;
    QoreValue rv{};
    {
        // declared before the lock, so that the weak reference is released after it
        ScanHolderRelease release;
        SafeLocker sl(&l);

        if (checkWriteIntern(xsink, true)) {
            return QoreValue();
        }

        {
            int rc = waitReadIntern(xsink, timeout_ms);
            if (rc == QW_TIMEOUT) {
                to = true;
            }
            if (rc) {
                return QoreValue();
            }
        }

        QoreQueueNode* n = tail;
        tail = tail->prev;
        if (!tail) {
            head = nullptr;
        } else {
            tail->next = nullptr;
        }

        addLenIntern(-1);
        if (getWriteWaitingIntern()) {
            write_cond.signal();
        }

        // the value is handed to the caller with the queue's reference, which the object's recursive set may count
        // as internal: marked under the lock, before the caller can release it (see RObject::remove_gen)
        if (needs_scan(n->node)) {
            markRemovedIntern();
        }

        // the scan count is maintained under the lock, as in push() and insert(); the queue holds the object
        // whose count includes it, so a value taken through the C++ API without the Queue object is uncounted too
        if (n->scan_counted) {
            assert(!self || qore_object_private::get(*self) == scan_holder);
            uncountIntern(release);
        }

        sl.unlock();
        rv = n->takeAndDel();
    }

    return rv;
}

void qore_queue_private::clear(ExceptionSink* xsink, QoreObject* self) {
    {
        // declared before the lock, so that the weak reference is released after it
        ScanHolderRelease release;
        AutoLocker al(&l);

        if (checkWriteIntern(xsink)) {
            return;
        }

        // A signalled reader remains in read_waiting until it reacquires this lock, so waiting
        // readers do not imply an empty queue. Check the data itself before skipping the clear.
        if (!head) {
            return;
        }

        // the queue holds the object whose count includes it, so a queue cleared through the C++ API without the
        // Queue object is uncounted too
        assert(!self || !scan_holder || qore_object_private::get(*self) == scan_holder);
        // the values are released below, under the lock, which can take references that the object's recursive set
        // counts as internal (see RObject::remove_gen)
        markRemovedIntern();
        uncountAllIntern(release);

        clearIntern(xsink);
        setLenIntern(0);

        if (getWriteWaitingIntern()) {
            write_cond.signal();
        }
    }
}

void qore_queue_private::setError(const char* n_err, const QoreStringNode* n_desc, QoreObject* self, ExceptionSink* xsink) {
    {
        // declared before the lock, so that the weak reference is released after it
        ScanHolderRelease release;
        AutoLocker al(&l);
        if (getLenIntern() == Queue_Deleted) {
            return;
        }

        err = n_err;
        if (desc) {
            desc->deref();
        }
        desc = n_desc->stringRefSelf();

        // the queue holds the object whose count includes it, so a queue cleared through the C++ API without the
        // Queue object is uncounted too
        assert(!self || !scan_holder || qore_object_private::get(*self) == scan_holder);
        // the values are released below, under the lock, which can take references that the object's recursive set
        // counts as internal (see RObject::remove_gen)
        markRemovedIntern();
        uncountAllIntern(release);

        // clear the queue
        clearIntern(xsink);
        setLenIntern(0);

        if (getReadWaitingIntern()) {
            read_cond.broadcast();
        }
        if (getWriteWaitingIntern()) {
            write_cond.broadcast();
        }
    }
}

void qore_queue_private::clearError() {
    AutoLocker al(&l);
    if (getLenIntern() == Queue_Deleted) {
        return;
    }

    err.clear();
    if (desc) {
        desc->deref();
        desc = nullptr;
    }
}

bool qore_queue_private::scanMembers(RObject& obj, RSetHelper& rsh) {
    // if we cannot lock the lock, then return false to ignore
    // blocking here or returning true could cause a deadlock
    if (l.trylock()) {
        return false;
    }
    AutoLocker al(l, true);

    // the values are only reported while the queue knows its object, so that every value it hands out after the
    // scan has read them marks the object (see scan_holder and RObject::remove_gen)
    if (!scan_count) {
        return false;
    }
    assert(&obj == scan_holder);

    QoreQueueNode* w = head;
    while (w) {
        //printd(5, "qore_object_private::checkIntern() scanning Queue value: '%s'\n", w->node.getFullTypeName());
        if (w->node.hasNode() && obj.scanCheck(rsh, w->node.getInternalNode())) {
            return true;
        }
        w = w->next;
    }

    return false;
}

QoreQueue::QoreQueue(int n_max) : priv(new qore_queue_private(n_max)) {
}

QoreQueue::QoreQueue(const QoreQueue& orig) : priv(new qore_queue_private(*orig.priv)) {
}

// queues should not be deleted when other threads might
// be accessing them
QoreQueue::~QoreQueue() {
   delete priv;
}

// push at the end of the queue and take the reference - can only be used when len == -1
void QoreQueue::pushAndTakeRef(QoreValue n) {
   priv->pushAndTakeRef(n);
}

// push at the end of the queue
void QoreQueue::push(ExceptionSink* xsink, QoreObject* self, QoreValue n, int timeout_ms, bool* to) {
    bool timeout;
    priv->push(xsink, self, n, timeout_ms, timeout);
    if (to) {
        *to = timeout;
    }
}

void QoreQueue::push(ExceptionSink* xsink, QoreValue n, int timeout_ms, bool* to) {
    push(xsink, nullptr, n, timeout_ms, to);
}

// insert at the beginning of the queue
void QoreQueue::insert(ExceptionSink* xsink, QoreObject* self, QoreValue n, int timeout_ms, bool* to) {
    bool timeout;
    priv->insert(xsink, self, n, timeout_ms, timeout);
    if (to) {
        *to = timeout;
    }
}

void QoreQueue::insert(ExceptionSink* xsink, QoreValue n, int timeout_ms, bool* to) {
    insert(xsink, nullptr, n, timeout_ms, to);
}

QoreValue QoreQueue::shift(ExceptionSink* xsink, int timeout_ms, bool* to) {
    bool timeout;
    QoreValue rv = priv->shift(xsink, nullptr, timeout_ms, timeout);
    if (to) {
        *to = timeout;
    }
    return rv;
}

QoreValue QoreQueue::pop(ExceptionSink* xsink, int timeout_ms, bool* to) {
    bool timeout;
    QoreValue rv = priv->pop(xsink, nullptr, timeout_ms, timeout);
    if (to) {
        *to = timeout;
    }
    return rv;
}

QoreValue QoreQueue::shift(ExceptionSink* xsink, QoreObject* self, int timeout_ms, bool* to) {
    bool timeout;
    QoreValue rv = priv->shift(xsink, self, timeout_ms, timeout);
    if (to) {
        *to = timeout;
    }
    return rv;
}

QoreValue QoreQueue::pop(ExceptionSink* xsink, QoreObject* self, int timeout_ms, bool* to) {
    bool timeout;
    QoreValue rv = priv->pop(xsink, self, timeout_ms, timeout);
    if (to) {
        *to = timeout;
    }
    return rv;
}

bool QoreQueue::empty() const {
    return priv->empty();
}

size_t QoreQueue::size() const {
    return priv->size();
}

ptrdiff_t QoreQueue::getMax() const {
    return priv->getMax();
}

size_t QoreQueue::getReadWaiting() const {
    return priv->getReadWaiting();
}

size_t QoreQueue::getWriteWaiting() const {
    return priv->getWriteWaiting();
}

void QoreQueue::clear(ExceptionSink* xsink) {
    priv->clear(xsink);
}

void QoreQueue::clear(ExceptionSink* xsink, QoreObject* self) {
    priv->clear(xsink, self);
}

void QoreQueue::setError(const char* err, const QoreStringNode* desc, ExceptionSink* xsink) {
    priv->setError(err, desc, nullptr, xsink);
}

void QoreQueue::setError(const char* err, const QoreStringNode* desc, QoreObject* self, ExceptionSink* xsink) {
    priv->setError(err, desc, self, xsink);
}

void QoreQueue::clearError() {
    priv->clearError();
}

Queue::Queue(int max) : QoreQueue(max) {
}

Queue::Queue(const Queue& old) : QoreQueue(old) {
}

Queue::~Queue() {
}

Queue* Queue::queueRefSelf() const {
   ((Queue*)this)->ref();
   return (Queue*)this;
}
