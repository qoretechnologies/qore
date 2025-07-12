/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QoreLoggerAppender.h LoggerAppender class definition */
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

#ifndef _QORE_MODULE_LOGGER_LOGGERAPPENDER_H

#define _QORE_MODULE_LOGGER_LOGGERAPPENDER_H

//! open event
constexpr int EVENT_OPEN = 1;
//! logging event
constexpr int EVENT_LOG = 2;
//! close event
constexpr int EVENT_CLOSE = 3;

// forward references
class QoreLoggerAppenderQueue;
class QoreLoggerAppenderWithLayout;

class QoreLoggerAppender : public AbstractPrivateData {
public:
    DLLLOCAL QoreLoggerAppender(ExceptionSink* xsink, QoreObject* self);

    DLLLOCAL QoreLoggerAppender(ExceptionSink* xsink, QoreObject* self, const QoreStringNode* n);

    using AbstractPrivateData::deref;
    DLLLOCAL virtual void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            derefIntern(xsink);
            delete this;
        }
    }

    //! Returns the appender name
    DLLLOCAL QoreStringNode* getName() const;

    //! Sets the appender queue
    DLLLOCAL void setQueue(ExceptionSink* xsink, const QoreObject* queue,
            ReferenceHolder<QoreLoggerAppenderQueue>& holder);

    //! Returns async queue or @ref nothing when events are processed synchronously
    DLLLOCAL QoreObject* getQueue();

    //! Opens logging resources
    DLLLOCAL void open(ExceptionSink* xsink);

    //! Releases any resources allocated by the appender and closes it
    DLLLOCAL void close(ExceptionSink* xsink);

    //! Returns @ref True if the appender is open and therefore active
    DLLLOCAL bool isOpen() const;

    //! Adds a filter to the chain
    DLLLOCAL void addFilter(ExceptionSink* xsink, const QoreObject* filter, bool top = false);

    //! Removes the given filter from the filter chain
    DLLLOCAL void removeFilter(ExceptionSink* xsink, const QoreObject* filter);

    //! Clears the filter chain by removing all filters
    DLLLOCAL void removeAllFilters(ExceptionSink* xsink);

    //! Returns a snapshot of the current filter chain as a list
    DLLLOCAL QoreListNode* getFilters();

    //! Pushes the given event on the queue or calls @ref processEvent() in case of synchronous processing
    DLLLOCAL bool pushEvent(ExceptionSink* xsink, int64 type, QoreValue params = QoreValue());

    //! Posts the given event to the output queue
    bool post(ExceptionSink* xsink, const QoreObject* event, QoreLoggerEvent* e = nullptr);

    //! Processes the event to the physical target
    DLLLOCAL void processEvent(int type, const QoreValue params, ExceptionSink* xsink);

protected:
    //! The owning object
    QoreObject* self;
    //! Method objects for faster calls
    const QoreMethod* pushMeth = nullptr,
        * evalMeth = nullptr,
        * serializeImplMeth = nullptr,
        * pushEventMeth = nullptr,
        * ensureAtomicOperationsMeth = nullptr,
        * processEventImplMeth = nullptr;

    //! Is pushEvent() native?
    bool direct_push_event;

    //! Is ensureAtomicOperations() native?
    bool direct_ensure_atomic_operations;

    // Read-write lock
    QoreRWLock lock;

    // The appender's name
    SimpleRefHolder<QoreStringNode> name;

    //! An inactive appender won't accept any logging request
    bool active = false;

    //! Filter chain
    QoreListNode* filters = nullptr;

    //! Async queue (LoggerAppenderQueue object)
    QoreObject* queue = nullptr;
    QoreLoggerAppenderQueue* qqueue = nullptr;

    // Native implementation for the serializeImpl() call
    QoreLoggerAppenderWithLayout* al = nullptr;

    //! Set direct flags
    DLLLOCAL void setDirect(ExceptionSink* xsink);

    //! Pushes the given event on the queue or calls @ref processEvent() in case of synchronous processing
    DLLLOCAL bool pushEventLocked(ExceptionSink* xsink, int64 type, QoreValue params = QoreValue());

    // Pushes data on the queue
    DLLLOCAL bool pushQueueLocked(ExceptionSink* xsink, int64 type, const QoreValue params);

    DLLLOCAL virtual void derefIntern(ExceptionSink* xsink);

    //! Setup methods
    DLLLOCAL void setupMethods();
};

#endif
