/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QoreLoggerAppenderQueue.h LoggerAppenderQueue class definition */
/*
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

#ifndef _QORE_MODULE_LOGGER_LOGGERAPPENDERQUEUE_H

#define _QORE_MODULE_LOGGER_LOGGERAPPENDERQUEUE_H

//! Default maximum bytes for memory-based backpressure (10 MiB)
constexpr int64 LOGGER_QUEUE_DEFAULT_MAX_BYTES = 10 * 1024 * 1024;

class QoreLoggerAppenderQueue : public AbstractPrivateData {
public:
    //! Constructor with unlimited memory (backwards compatible)
    DLLLOCAL QoreLoggerAppenderQueue(QoreObject* self, QoreObject* qobj, QoreQueue& q)
            : self(self), qobj(qobj), q(q), max_bytes(-1), current_bytes(0) {
    }

    //! Constructor with memory limit
    /** @param max_bytes maximum bytes allowed in queue (-1 = unlimited, >0 = limit)
    */
    DLLLOCAL QoreLoggerAppenderQueue(QoreObject* self, QoreObject* qobj, QoreQueue& q, int64 max_bytes)
            : self(self), qobj(qobj), q(q), max_bytes(max_bytes), current_bytes(0) {
    }

    //! Adds appender event with optional timeout for backpressure
    /** @param xsink exception sink
        @param appender the appender object
        @param type the event type
        @param params the event parameters
        @param timeout_ms timeout in milliseconds (0 = block indefinitely, <0 = drop if full)
        @return true if the event was queued, false if dropped due to backpressure
    */
    DLLLOCAL bool push(ExceptionSink* xsink, const QoreObject* appender, int64 type, const QoreValue params,
            int64 timeout_ms = 0);

    //! Processes queue events
    DLLLOCAL void process(int64 ms, ExceptionSink* xsink);

    //! Returns the queue entry count
    DLLLOCAL int64 size() const {
        return q.size();
    }

    //! Returns the maximum bytes limit (-1 if unlimited)
    DLLLOCAL int64 getMaxBytes() const {
        return max_bytes;
    }

    //! Returns the current queued bytes
    DLLLOCAL int64 getCurrentBytes() const {
        return current_bytes.load();
    }

    //! Returns true if the queue is at or above the memory limit
    DLLLOCAL bool isFull() const {
        return max_bytes > 0 && current_bytes.load() >= max_bytes;
    }

    //! Returns the next event on the queue
    DLLLOCAL QoreHashNode* getEvent(int64 ms, ExceptionSink* xsink);

protected:
    //! Reduces current_bytes when an event is consumed
    DLLLOCAL void reduceBytes(int64 bytes);

    QoreObject* self;
    QoreObject* qobj;
    QoreQueue& q;
    int64 max_bytes;                    //!< Maximum bytes allowed (-1 = unlimited)
    std::atomic<int64> current_bytes;   //!< Current queued bytes
    QoreThreadLock byte_lock;           //!< Lock for byte limit coordination
    QoreCondition byte_cond;            //!< Condition for waiting on byte limit
};

#endif
