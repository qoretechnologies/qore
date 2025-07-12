/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QoreLoggerAppender.cpp LoggerAppender class definition */
/*
    Qore Programming Language

    Copyright (C) 2003 - 2025 Qore Technologies, s.r.o.

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

#include "qore_logger.h"
#include "QC_LoggerAppender.h"
#include "QC_LoggerFilter.h"
#include "QC_LoggerAppenderQueue.h"
#include "QC_LoggerAppenderWithLayout.h"
#include "QC_LoggerEvent.h"

QoreLoggerAppender::QoreLoggerAppender(ExceptionSink* xsink, QoreObject* self) : self(self) {
    setupMethods();
    setDirect(xsink);
}

QoreLoggerAppender::QoreLoggerAppender(ExceptionSink* xsink, QoreObject* self, const QoreStringNode* n) : self(self),
        name(n->stringRefSelf()) {
    setupMethods();
    setDirect(xsink);
}

void QoreLoggerAppender::setDirect(ExceptionSink* xsink) {
    const QoreClass* cls = self->getClass();
    const QoreMethod* m = cls->findMethod("pushEvent");
    assert(m);
    direct_push_event = m->isBuiltin();
    m = cls->findMethod("ensureAtomicOperations");
    assert(m);
    direct_ensure_atomic_operations = m->isBuiltin();
    m = cls->findMethod("serializeImpl");
    assert(m);
    if (m->isBuiltin()) {
        ReferenceHolder<QoreLoggerAppenderWithLayout> a(
            self->tryGetReferencedPrivateData<QoreLoggerAppenderWithLayout>(CID_LOGGERAPPENDERWITHLAYOUT, xsink),
            xsink
        );
        if (a) {
            al = a.release();
        }
    }
}

//! Returns the appender name
QoreStringNode* QoreLoggerAppender::getName() const {
    return name->empty() ? new QoreStringNode : name->stringRefSelf();
}

//! Sets the appender queue.
/**
    Data are passed to target device synchronously
    unless queue is assigned. In this case data are queued and
    dedicated process will process it asynchronously

    @param queue the queue to set

    @throw "LOGGER-ERROR" thrown if appender is opened
*/
void QoreLoggerAppender::setQueue(ExceptionSink* xsink, const QoreObject* queue,
        ReferenceHolder<QoreLoggerAppenderQueue>& holder) {
    QoreAutoRWWriteLocker awl(lock);
    if (active) {
        xsink->raiseException("LOGGER-ERROR", "Appender is opened");
        return;
    }
    //printd(5, "QoreLoggerAppender::setQueue() this: %p self: %p old queue: %p qq: %p new: %p\n", this, self,
    //    this->queue, qqueue, queue);
    if (qqueue) {
        qqueue->deref(xsink);
        qqueue = nullptr;
    }
    this->queue = const_cast<QoreObject*>(queue);
    if (queue) {
        self->setValue("q", queue->objectRefSelf(), xsink);
        assert(holder);
        qqueue = holder.release();
    } else {
        assert(!holder);
        self->setValue("q", QoreValue(), xsink);
    }
}

//! Returns async queue or @ref nothing when events are processed synchronously
/**
*/
QoreObject* QoreLoggerAppender::getQueue() {
    QoreAutoRWReadLocker awl(lock);
    return queue ? queue->objectRefSelf() : nullptr;
}

//! Opens logging resources
/**
    The appender must be opened to accept any logging events
*/
void QoreLoggerAppender::open(ExceptionSink* xsink) {
    QoreAutoRWWriteLocker awl(lock);
    if (!active) {
        pushEventLocked(xsink, EVENT_OPEN);
        active = true;
    }
}

//! Releases any resources allocated by the appender and closes it
void QoreLoggerAppender::close(ExceptionSink* xsink) {
    QoreAutoRWWriteLocker awl(lock);
    if (active) {
        pushEventLocked(xsink, EVENT_CLOSE);
        active = false;
    }
}

//! Returns @ref True if the appender is open and therefore active
bool QoreLoggerAppender::isOpen() const {
    return active;
}

//! Adds a filter to the chain
/**
    @param filter the new filter to add
    @param top if @ref True then the filter is added at the start of the filter chain, if @ref False (the default),
    the filter is added at the end of the chain
    @throw "LOGGER-ERROR" thrown if the filter is already in the list
*/
void QoreLoggerAppender::addFilter(ExceptionSink* xsink, const QoreObject* filter, bool top) {
    ReferenceHolder<QoreObject> filt(filter->objectRefSelf(), xsink);
    QoreAutoRWWriteLocker awl(lock);
    if (!filters) {
        filters = new QoreListNode(QC_LOGGERFILTER->getTypeInfo());
        filters->push(filt.release(), xsink);
        return;
    }
    ConstListIterator i(filters);
    while (i.next()) {
        if (i.getValue().get<const QoreObject>() == filter) {
            xsink->raiseException("LOGGER-ERROR", "Filter already exists in list");
            return;
        }
    }
    if (!filters->is_unique()) {
        ReferenceHolder<QoreListNode> holder(filters, xsink);
        filters = filters->copy();
    }
    if (top) {
        filters->insert(filt.release(), xsink);
    } else {
        filters->push(filt.release(), xsink);
    }
    assert(!*xsink);
}

//! Removes the given filter from the filter chain.
/**
    @param filter the filter to remove
*/
void QoreLoggerAppender::removeFilter(ExceptionSink* xsink, const QoreObject* filter) {
    QoreAutoRWWriteLocker awl(lock);
    ConstListIterator i(filters);
    while (i.next()) {
        if (i.getValue().get<const QoreObject>() == filter) {
            if (!filters->is_unique()) {
                ReferenceHolder<QoreListNode> holder(filters, xsink);
                filters = filters->copy();
            }
            // remove the filter from the list
            ReferenceHolder<QoreListNode> holder(filters->extract(i.index(), 1), xsink);
            return;
        }
    }
}

//! Clears the filter chain by removing all filters
void QoreLoggerAppender::removeAllFilters(ExceptionSink* xsink) {
    QoreAutoRWWriteLocker awl(lock);
    if (filters) {
        filters->deref(xsink);
        filters = nullptr;
    }
}

//! Returns the filter chain as a list
/**
    Note that appender filter chain may be modified as internal lock is released when copy of list is returned
*/
QoreListNode* QoreLoggerAppender::getFilters() {
    QoreAutoRWReadLocker awl(lock);
    return filters ? filters->listRefSelf() : new QoreListNode(QC_LOGGERFILTER->getTypeInfo());
}

//! Pushes the given event on the queue or calls @ref processEvent() in case of synchronous processing
/**
    @param type the event type
    @param params parameters for the event according to the event type

    @return @ref True if the event was accepted/processed, @ref False if not

    @see @ref processEvent()
*/
bool QoreLoggerAppender::pushEvent(ExceptionSink* xsink, int64 type, QoreValue params) {
    {
        QoreAutoRWReadLocker awl(lock);
        if (queue) {
            return pushQueueLocked(xsink, type, params);
        }
    }

    processEvent(type, params, xsink);
    return true;
}

// Pushes data on the queue
bool QoreLoggerAppender::pushQueueLocked(ExceptionSink* xsink, int64 type, const QoreValue params) {
    assert(queue);
    //printd(5, "QoreLoggerAppender::pushQueueLocked() queue: %p qqueue: %p type: %d\n", queue, qqueue, type);
    if (qqueue) {
        qqueue->push(xsink, self, type, params);
        return true;
    }

    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
    args->push(self->objectRefSelf(), xsink);
    args->push(type, xsink);
    args->push(params.refSelf(), xsink);
    QoreObjectContextHelper och(self, QC_LOGGERAPPENDER);
    ValueHolder v(queue->evalMethod(*pushMeth, *args, xsink), xsink);
    return v->getAsBool();
}

//! Pushes the given event on the queue or calls @ref processEvent() in case of synchronous processing
/**
    @param type the event type
    @param params parameters for the event according to the event type

    @return @ref True if the event was accepted/processed, @ref False if not

    @see @ref processEvent()
*/
bool QoreLoggerAppender::pushEventLocked(ExceptionSink* xsink, int64 type, QoreValue params) {
    if (queue) {
        return pushQueueLocked(xsink, type, params);
    }

    processEvent(type, params, xsink);
    return true;
}

//! Posts the given event to the output queue
/**
    Invokes filters; when the event is accepted then it is posted to the queue in case of
    asynchronous processing, or it is immediately logged by the @ref processEvent() method
    in case of synchronous processing.

    @see @ref processEvent()

    @param event the event to post

    @return True if the event has been posted, @ref False if not (filtered out, appender inactive,
    event not accepted on queue, etc)
 */
 bool QoreLoggerAppender::post(ExceptionSink* xsink, const QoreObject* event, QoreLoggerEvent* e) {
    // optimistically create method argument list
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
    args->push(event->objectRefSelf(), xsink);

    {
        QoreAutoRWReadLocker arl(lock);
        if (!active) {
            return false;
        }

        if (filters) {
            assert(!*xsink);
            ConstListIterator i(filters);
            QoreObjectContextHelper och(self, QC_LOGGERAPPENDER);
            while (i.next()) {
                QoreObject* filter = const_cast<QoreObject*>(i.getValue().get<QoreObject>());
                ValueHolder v(filter->evalMethod(*evalMeth, *args, xsink), xsink);
                if (*xsink) {
                    return false;
                }
                assert(v->getType() == NT_INT);
                int64 d = v->getAsBigInt();
                if (d == DENY) {
                    return false;
                }
                if (d == ACCEPT) {
                    break;
                }
            }
        }
    }
    ValueHolder v(xsink);
    if (al) {
        ReferenceHolder<QoreLoggerEvent> ev(xsink);
        if (!e) {
            ev = event->tryGetReferencedPrivateData<QoreLoggerEvent>(CID_LOGGEREVENT, xsink);
            if (*xsink) {
                return false;
            }
            e = *ev;
        }
        if (e) {
            v = al->serializeImpl(xsink, event, e);
        } else {
            // if we get one non-matching event, we set al to nullptr
            QoreAutoRWWriteLocker arl(lock);
            al->deref(xsink);
            al = nullptr;
        }
    }
    if (!al) {
        QoreObjectContextHelper och(self, QC_LOGGERAPPENDER);
        v = self->evalMethod(*serializeImplMeth, *args, xsink);
    }
    if (*xsink || !v) {
        return false;
    }
    if (direct_push_event) {
        return pushEvent(xsink, EVENT_LOG, *v);
    } else {
        args = new QoreListNode(autoTypeInfo);
        args->push(EVENT_LOG, xsink);
        args->push(v.release(), xsink);
        QoreObjectContextHelper och(self, QC_LOGGERAPPENDER);
        v = self->evalMethod(*pushEventMeth, *args, xsink);
        return v->getAsBool();
    }
}

//! Processes the event to the physical target
void QoreLoggerAppender::processEvent(int type, const QoreValue params, ExceptionSink* xsink) {
    // the following call may return a lock holder object that will be released when the local variable goes
    // out of scope
    ReferenceHolder<QoreListNode> margs(new QoreListNode(autoTypeInfo), xsink);
    ValueHolder holder0(xsink);
    if (!direct_ensure_atomic_operations) {
        margs->push(type, xsink);
        QoreObjectContextHelper och(self, QC_LOGGERAPPENDER);
        holder0 = self->evalMethod(*ensureAtomicOperationsMeth, *margs, xsink);
        margs = new QoreListNode(autoTypeInfo);
    }
    margs->push(type, xsink);
    margs->push(params.refSelf(), xsink);
    QoreObjectContextHelper och(self, QC_LOGGERAPPENDER);
    self->evalMethod(*processEventImplMeth, *margs, xsink).discard(xsink);
}

void QoreLoggerAppender::derefIntern(ExceptionSink* xsink) {
    if (filters) {
        filters->deref(xsink);
    }
    if (qqueue) {
        qqueue->deref(xsink);
    }
    if (al) {
        al->deref(xsink);
    }
}

void QoreLoggerAppender::setupMethods() {
    const QoreClass* cls = self->getClass();
    pushMeth = cls->findMethod("push");
    evalMeth = cls->findMethod("eval");
    serializeImplMeth = cls->findMethod("serializeImpl");
    pushEventMeth = cls->findMethod("pushEvent");
    ensureAtomicOperationsMeth = cls->findMethod("ensureAtomicOperations");
    processEventImplMeth = cls->findMethod("processEventImpl");
}