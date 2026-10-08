/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreQueueIntern.h

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

#ifndef _QORE_QOREQUEUEINTERN_H

#define _QORE_QOREQUEUEINTERN_H

#include <qore/QoreThreadLock.h>
#include <qore/QoreCondition.h>

#include <atomic>
#include <string>

class qore_object_private;
class RSetHelper;

class QoreQueueNode {
public:
    QoreValue node{};
    QoreQueueNode* prev,
                    * next;
    // true if the value is counted in the queue's scan count (see qore_queue_private::scan_count); only values
    // added through the Queue class are counted, so only those can be uncounted when they are taken
    bool scan_counted = false;
    DLLLOCAL QoreQueueNode(QoreValue n, QoreQueueNode* p, QoreQueueNode* nx) : node(n), prev(p), next(nx) {
    }

#ifdef DEBUG
    DLLLOCAL ~QoreQueueNode() {
        assert(!node);
    }
#endif

    DLLLOCAL void del(ExceptionSink* xsink) {
        node.discard(xsink);

#ifdef DEBUG
        node = QoreValue();
#endif
        delete this;
    }

    DLLLOCAL QoreValue takeAndDel() {
        QoreValue rv = node;
#ifdef DEBUG
        node = QoreValue();
#endif
        delete this;
        return rv;
    }
};

#define QW_DEL     -1
#define QW_TIMEOUT -2
#define QW_ERROR   -3

class qore_queue_private {
    friend class qore_object_private;

private:
    enum queue_status_e { Queue_Deleted = -1 };

    mutable QoreThreadLock l;
    QoreCondition read_cond,   // read Condition variable
                    write_cond;  // write Condition variable
    QoreQueueNode* head,
                    * tail;
    std::string err;
    QoreStringNode* desc;
    // len, read_waiting and write_waiting are only written with the lock held, but size(), empty(),
    // getReadWaiting() and getWriteWaiting() read them without the lock, so they are atomic; these readers only
    // report a snapshot of the counter and no other data is published through it, so relaxed ordering is
    // sufficient everywhere: the lock orders the writers and every reader that takes the lock
    std::atomic<int> len;   // the number of elements currently in the queue (or -1 for deleted)
    int max;   // the maximum size of the queue (or -1 for unlimited); set only when the queue is created
    std::atomic<unsigned> read_waiting,   // number of threads waiting on reads
                write_waiting;  // number of threads waiting on writes

    // issue #3101: maintain a count of all scanable objects in the queue
    /* While the count is not zero, the Queue object's private data scan count includes the queue, so the collector
       scans the queue's values.  The object's count is changed under the queue's lock, in the same critical section
       as the transition of this count that requires it: changed after the lock is released, a take could decrement
       it before the push that made this count non-zero has incremented it, and the object would not be scanned
       while the queue holds values.  This orders the queue's lock before the object's rlck, which is safe because
       rlck is a leaf lock: no code acquires a queue lock while holding it, and the collector only ever tries the
       queue's lock (see scanMembers()).
    */
    int scan_count = 0;

    //! The object whose private data scan count includes the queue; set while scan_count is not 0
    /** Values are counted only when they are added through the Queue object, but they can be taken, and the queue
        cleared, through the public C++ API, which has the queue and not its object; the queue keeps the object so
        that these uncount it too.  Held with a weak reference, which keeps the object allocated but not alive; the
        reference is released after the queue's lock (see ScanHolderRelease).
    */
    qore_object_private* scan_holder = nullptr;

    DLLLOCAL int waitReadIntern(ExceptionSink *xsink, int timeout_ms);
    DLLLOCAL int waitWriteIntern(ExceptionSink *xsink, int timeout_ms);

    DLLLOCAL void pushNode(QoreValue v);
    DLLLOCAL void pushIntern(QoreValue v);
    DLLLOCAL void insertIntern(QoreValue v);

    DLLLOCAL void clearIntern(ExceptionSink* xsink);

    //! Releases the weak reference to a scan holder when it goes out of scope
    /** Declared before the queue's lock is taken, so that the reference is released after the lock: the last weak
        reference frees the object.
    */
    class ScanHolderRelease {
    public:
        DLLLOCAL ScanHolderRelease() = default;
        DLLLOCAL ScanHolderRelease(const ScanHolderRelease&) = delete;
        DLLLOCAL ScanHolderRelease& operator=(const ScanHolderRelease&) = delete;

        DLLLOCAL ~ScanHolderRelease();

        qore_object_private* obj = nullptr;
    };

    // records that values the queue may have reported to a scan are given up; called with the lock held
    DLLLOCAL void markRemovedIntern();

    // adds a value to the scan count; called with the lock held
    DLLLOCAL void countIntern(QoreObject& self);

    // removes a counted value from the scan count; called with the lock held
    DLLLOCAL void uncountIntern(ScanHolderRelease& release);

    // removes all values from the scan count; called with the lock held
    DLLLOCAL void uncountAllIntern(ScanHolderRelease& release);

    // releases the scan holder once the scan count is 0; called with the lock held
    DLLLOCAL void releaseScanHolderIntern(ScanHolderRelease& release);

    // the following helpers must be called with the lock held
    DLLLOCAL int getLenIntern() const {
        return len.load(std::memory_order_relaxed);
    }

    DLLLOCAL void setLenIntern(int new_len) {
        len.store(new_len, std::memory_order_relaxed);
    }

    // a plain load and store, as the lock excludes other writers
    DLLLOCAL void addLenIntern(int delta) {
        setLenIntern(getLenIntern() + delta);
    }

    DLLLOCAL static void incWaitingIntern(std::atomic<unsigned>& waiting) {
        waiting.store(waiting.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
    }

    DLLLOCAL static void decWaitingIntern(std::atomic<unsigned>& waiting) {
        unsigned current = waiting.load(std::memory_order_relaxed);
        assert(current);
        waiting.store(current - 1, std::memory_order_relaxed);
    }

    DLLLOCAL unsigned getReadWaitingIntern() const {
        return read_waiting.load(std::memory_order_relaxed);
    }

    DLLLOCAL unsigned getWriteWaitingIntern() const {
        return write_waiting.load(std::memory_order_relaxed);
    }

    // called in the lock; returns -1 if not possible (cannot write to the queue) or 0 of OK
    DLLLOCAL int checkWriteIntern(ExceptionSink* xsink, bool always_error = false);

public:
    DLLLOCAL qore_queue_private(int n_max = -1) : head(0), tail(0), desc(0), len(0), max(n_max), read_waiting(0), write_waiting(0) {
        assert(max);
        //printd(5, "qore_queue_private::qore_queue_private() this: %p max: %d\n", this, max);
    }

    DLLLOCAL qore_queue_private(const qore_queue_private &orig) : head(0), tail(0), err(orig.err), desc(orig.desc ? orig.desc->stringRefSelf() : 0), len(0), max(orig.max), read_waiting(0), write_waiting(0) {
        AutoLocker al(orig.l);
        if (orig.getLenIntern() == Queue_Deleted) {
            return;
        }

        QoreQueueNode* w = orig.head;
        while (w) {
            pushIntern(w->node.refSelf());
            w = w->next;
        }

        //printd(5, "qore_queue_private::qore_queue_private() this=%p head=%p tail=%p waiting=%d len=%d\n", this, head, tail, waiting, len);
    }

    // queues should not be deleted when other threads might
    // be accessing them
    DLLLOCAL ~qore_queue_private() {
        //QORE_TRACE("qore_queue_private::~qore_queue_private()");
        //printd(5, "qore_queue_private::~qore_queue_private() this=%p head=%p tail=%p len=%d\n", this, head, tail, len);
        assert(!head);
        assert(!tail);
        assert(getLenIntern() == Queue_Deleted || !getLenIntern());
        assert(!desc);
        assert(!scan_holder);
    }

    // push at the end of the queue and take the reference - can only be used when len == -1; the reference is
    // released if the queue is deleted or in an error state
    DLLLOCAL void pushAndTakeRef(QoreValue n);

    // push at the end of the queue
    DLLLOCAL void push(ExceptionSink* xsink, QoreObject* self, QoreValue n, int timeout_ms, bool& to);

    // insert at the beginning of the queue
    DLLLOCAL void insert(ExceptionSink* xsink, QoreObject* self, QoreValue n, int timeout_ms, bool& to);

    DLLLOCAL QoreValue shift(ExceptionSink* xsink, QoreObject* self, int timeout_ms, bool& to);
    DLLLOCAL QoreValue pop(ExceptionSink* xsink, QoreObject* self, int timeout_ms, bool& to);

    // returns a snapshot of the queue's state; does not take the lock
    DLLLOCAL bool empty() const {
        return !len.load(std::memory_order_relaxed);
    }

    // returns a snapshot of the queue's size; does not take the lock
    DLLLOCAL int size() const {
        return len.load(std::memory_order_relaxed);
    }

    DLLLOCAL int getMax() const {
        return max;
    }

    // returns a snapshot of the number of threads waiting to read; does not take the lock
    DLLLOCAL unsigned getReadWaiting() const {
        return read_waiting.load(std::memory_order_relaxed);
    }

    // returns a snapshot of the number of threads waiting to write; does not take the lock
    DLLLOCAL unsigned getWriteWaiting() const {
        return write_waiting.load(std::memory_order_relaxed);
    }

    DLLLOCAL void clear(ExceptionSink* xsink, QoreObject* self = nullptr);
    DLLLOCAL void destructor(ExceptionSink* xsink);

    DLLLOCAL void setError(const char* err, const QoreStringNode* desc, QoreObject* self, ExceptionSink* xsink);

    DLLLOCAL void clearError();

    DLLLOCAL bool scanMembers(RObject& obj, RSetHelper& rsh);

    DLLLOCAL static void destructor(QoreQueue& q, ExceptionSink* xsink) {
        q.priv->destructor(xsink);
    }

    DLLLOCAL static qore_queue_private* get(QoreQueue& q) {
        return q.priv;
    }
};

#endif // _QORE_QOREQUEUEINTERN_H
