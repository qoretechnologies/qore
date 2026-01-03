/*
    QoreQueue.cpp

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
    AutoLocker al(&l);
    if (read_waiting) {
        xsink->raiseException("QUEUE-ERROR", "Queue deleted while there %s %d waiting thread%s for reading", read_waiting == 1 ? "is" : "are", read_waiting, read_waiting == 1 ? "" : "s");
        read_cond.broadcast();
    }
    if (write_waiting) {
        xsink->raiseException("QUEUE-ERROR", "Queue deleted while there %s %d waiting thread%s for writing", write_waiting == 1 ? "is" : "are", write_waiting, write_waiting == 1 ? "" : "s");
        write_cond.broadcast();
    }

    clearIntern(xsink);
    len = Queue_Deleted;
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
    scan_count = 0;
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
            ++read_waiting;
            // Use interruptible wait for sandbox support
            // Queue semantics: 0 = infinite wait, but waitWithInterrupt uses -1 for infinite
            int64 cond_timeout = (timeout_ms == 0) ? -1 : timeout_ms;
            rc = read_cond.waitWithInterrupt(l, cond_timeout, xsink);
            --read_waiting;
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

        if (len == Queue_Deleted) {
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
    while (max > 0 && len >= max) {
        if (!err.empty()) {
            xsink->raiseException(err.c_str(), desc->stringRefSelf());
            return QW_ERROR;
        }

        int rc;
        // issue #4077: do not call QoreCondition::wait() with a negative timeout value
        if (timeout_ms >= 0) {
            ++write_waiting;
            // Use interruptible wait for sandbox support
            // Queue semantics: 0 = infinite wait, but waitWithInterrupt uses -1 for infinite
            int64 cond_timeout = (timeout_ms == 0) ? -1 : timeout_ms;
            rc = write_cond.waitWithInterrupt(l, cond_timeout, xsink);
            --write_waiting;
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

        if (len == Queue_Deleted) {
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
    ++len;

    //printd(5, "qore_queue_private::pushNode(%p '%s') this: %p head: %p (%p) tail: %p (%p) read_waiting: %d len: %d\n", v, get_type_name(v), this, head, head->node, tail, tail->node, read_waiting, len);
}

void qore_queue_private::pushIntern(QoreValue v) {
    pushNode(v);
    //printd(5, "qore_queue_private::push_internal(%p) this: %p head: %p (%p) tail: %p (%p) waiting: %d len: %d\n", v, this, head, head->node, tail, tail->node, waiting, len);

    // signal waiting thread to wakeup and process event
    if (read_waiting) {
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
    len++;

    //printd(5, "qore_queue_private::insertIntern(%p) this: %p head: %p (%p) tail: %p (%p) waiting: %d len: %d\n", v, this, head, head->node, tail, tail->node, waiting, len);

    // signal waiting thread to wakeup and process event
    if (read_waiting) {
        read_cond.signal();
    }
}

int qore_queue_private::checkWriteIntern(ExceptionSink* xsink, bool always_error) {
    if (len == Queue_Deleted) {
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
    AutoLocker al(&l);
    if (len == Queue_Deleted || !err.empty()) {
        return;
    }

    assert(max == -1);

    printd(5, "qore_queue_private::pushAndTakeRef('%s') this: %p\n", n.getTypeName(), this);
    // reference value for being stored in queue
    pushIntern(n);
}

void qore_queue_private::push(ExceptionSink* xsink, QoreObject* self, QoreValue n, int timeout_ms, bool& to) {
    to = false;
    ValueHolder holder(n, xsink);

    bool inc_obj = false;
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
            ++scan_count;
            if (scan_count == 1) {
                inc_obj = true;
            }
        }
    }

    //printd(5, "qore_queue_private::push('%s') scan_count: %d inc: %d\n", n.getFullTypeName(), scan_count, inc_obj);
    if (inc_obj) {
        qore_object_private::get(*self)->incScanPrivateData();
    }
}

void qore_queue_private::insert(ExceptionSink* xsink, QoreObject* self, QoreValue n, int timeout_ms, bool& to) {
    to = false;
    ValueHolder holder(n, xsink);

    bool inc_obj = false;
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
            ++scan_count;
            if (scan_count == 1) {
                inc_obj = true;
            }
        }
    }

    //printd(5, "qore_queue_private::insert('%s') scan_count: %d inc: %d\n", n.getFullTypeName(), scan_count, inc_obj);
    if (inc_obj) {
        qore_object_private::get(*self)->incScanPrivateData();
    }
}

QoreValue qore_queue_private::shift(ExceptionSink* xsink, QoreObject* self, int timeout_ms, bool& to) {
    to = false;
    bool dec_obj = false;
    QoreValue rv{};
    {
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

        --len;
        if (write_waiting) {
            write_cond.signal();
        }

        sl.unlock();
        rv = n->takeAndDel();

        if (self && needs_scan(n)) {
            --scan_count;
            if (!scan_count) {
                dec_obj = true;
            }
        }
    }

    //printd(5, "qore_queue_private::shift('%s') scan_count: %d inc: %d\n", rv.getFullTypeName(), scan_count, dec_obj);
    if (dec_obj) {
        qore_object_private::get(*self)->decScanPrivateData();
    }

    return rv;
}

QoreValue qore_queue_private::pop(ExceptionSink* xsink, QoreObject* self, int timeout_ms, bool& to) {
    to = false;
    bool dec_obj = false;
    QoreValue rv{};
    {
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

        --len;
        if (write_waiting) {
            write_cond.signal();
        }

        sl.unlock();
        rv = n->takeAndDel();

        if (self && needs_scan(n)) {
            --scan_count;
            if (!scan_count) {
                dec_obj = true;
            }
        }
    }

    //printd(5, "qore_queue_private::pop('%s') scan_count: %d inc: %d\n", rv.getFullTypeName(), scan_count, dec_obj);
    if (dec_obj) {
        qore_object_private::get(*self)->decScanPrivateData();
    }

    return rv;
}

void qore_queue_private::clear(ExceptionSink* xsink, QoreObject* self) {
    bool dec_obj = false;
    {
        AutoLocker al(&l);

        if (checkWriteIntern(xsink)) {
            return;
        }

        if (read_waiting) {
            // the queue must be empty
            assert(!head);
            return;
        }

        if (scan_count) {
            dec_obj = true;
        }

        clearIntern(xsink);
        len = 0;

        if (write_waiting) {
            write_cond.signal();
        }
    }

    if (dec_obj) {
        qore_object_private::get(*self)->decScanPrivateData();
    }
}

void qore_queue_private::setError(const char* n_err, const QoreStringNode* n_desc, QoreObject* self, ExceptionSink* xsink) {
    bool dec_obj = false;
    {
        AutoLocker al(&l);
        if (len == Queue_Deleted) {
            return;
        }

        err = n_err;
        if (desc) {
            desc->deref();
        }
        desc = n_desc->stringRefSelf();

        if (scan_count) {
            dec_obj = true;
        }

        // clear the queue
        clearIntern(xsink);
        len = 0;

        if (read_waiting) {
            read_cond.broadcast();
        }
        if (write_waiting) {
            write_cond.broadcast();
        }
    }

    if (dec_obj) {
        qore_object_private::get(*self)->decScanPrivateData();
    }
}

void qore_queue_private::clearError() {
    AutoLocker al(&l);
    if (len == Queue_Deleted) {
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
