/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QoreLoggerAppenderQueue.cpp LoggerAppenderQueue class definition */
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

#include "qore_logger.h"
#include "QoreLoggerAppenderQueue.h"
#include "QC_LoggerAppender.h"

//! Calculates the approximate byte size of a QoreValue for memory tracking
/** @note Circular references are only possible with objects in Qore, and objects
    return a fixed estimate, so no cycle detection is needed for hashes/lists.
*/
static int64 getValueByteSize(const QoreValue& v) {
    switch (v.getType()) {
        case NT_NOTHING:
        case NT_NULL:
            return 0;
        case NT_STRING:
            return v.get<const QoreStringNode>()->size();
        case NT_BINARY:
            return v.get<const BinaryNode>()->size();
        case NT_INT:
        case NT_FLOAT:
            return 8;
        case NT_BOOLEAN:
            return 1;
        case NT_DATE:
            return 64;  // Estimate for DateTime structure
        case NT_NUMBER:
            return 32;  // Estimate for arbitrary precision number
        case NT_LIST: {
            int64 total = 0;
            const QoreListNode* l = v.get<const QoreListNode>();
            ConstListIterator li(l);
            while (li.next()) {
                total += getValueByteSize(li.getValue());
            }
            return total;
        }
        case NT_HASH: {
            int64 total = 0;
            const QoreHashNode* h = v.get<const QoreHashNode>();
            ConstHashIterator hi(h);
            while (hi.next()) {
                total += strlen(hi.getKey());
                total += getValueByteSize(hi.get());
            }
            return total;
        }
        case NT_OBJECT:
            // Objects are out of scope for logging; use default estimate
            return 64;
        default:
            return 64;  // Default estimate for other complex types
    }
}

//! Adds appender event with optional timeout for backpressure
bool QoreLoggerAppenderQueue::push(ExceptionSink* xsink, const QoreObject* appender, int64 type,
        const QoreValue params, int64 timeout_ms) {
    // Calculate byte size of params for memory tracking
    int64 param_size = getValueByteSize(params);

    // Check memory limit if set
    if (max_bytes > 0) {
        AutoLocker al(byte_lock);

        // Wait for space if blocking
        // Note: Always allow an entry when queue is empty to prevent deadlock with oversized entries
        if (timeout_ms >= 0) {
            while (current_bytes.load() + param_size > max_bytes && current_bytes.load() > 0) {
                if (timeout_ms == 0) {
                    // Block indefinitely
                    byte_cond.wait(byte_lock);
                } else {
                    // Wait with timeout
                    int rc = byte_cond.wait(byte_lock, timeout_ms);
                    if (rc == ETIMEDOUT) {
                        xsink->raiseException("QUEUE-TIMEOUT", "timeout exceeded waiting for queue space");
                        return false;
                    }
                }
            }
        } else {
            // Negative timeout = drop immediately if full (but always allow if queue is empty)
            if (current_bytes.load() + param_size > max_bytes && current_bytes.load() > 0) {
                return false;
            }
        }

        current_bytes += param_size;
    }

    // Create and push event hash
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), nullptr);
    h->setKeyValue("appender", appender->refSelf(), nullptr);
    h->setKeyValue("type", type, nullptr);
    h->setKeyValue("params", params.refSelf(), nullptr);
    h->setKeyValue("_size", param_size, nullptr);  // Store size for byte tracking
    q.push(xsink, qobj, h.release());

    // Roll back current_bytes if push failed
    if (*xsink && max_bytes > 0) {
        AutoLocker al(byte_lock);
        current_bytes -= param_size;
        byte_cond.broadcast();  // Wake any waiting producers
    }

    return !*xsink;
}

void QoreLoggerAppenderQueue::reduceBytes(int64 bytes) {
    if (max_bytes > 0) {
        AutoLocker al(byte_lock);
        current_bytes -= bytes;
        byte_cond.broadcast();  // Wake any waiting producers
    }
}

void QoreLoggerAppenderQueue::process(int64 ms, ExceptionSink* xsink) {
    while (true) {
        ReferenceHolder<QoreHashNode> rec(getEvent(ms, xsink), xsink);
        if (!rec) {
            break;
        }

        // Reduce current bytes for memory tracking
        QoreValue size_val = rec->getKeyValue("_size");
        if (size_val.getType() == NT_INT) {
            reduceBytes(size_val.getAsBigInt());
        }

        QoreValue appender = rec->getKeyValue("appender", xsink);
        assert(!*xsink);
        assert(appender.getType() == NT_OBJECT);
        // check if processEvent() method is builtin
        QoreObject* app = appender.get<QoreObject>();
        const QoreValue type = rec->getKeyValue("type");
        assert(type.getType() == NT_INT);
        const QoreValue params = rec->getKeyValue("params");
        {
            // make optimized call if possible
            const QoreMethod* m = app->getClass()->findMethod("processEvent");
            assert(m);
            if (m->isBuiltin()) {
                ReferenceHolder<QoreLoggerAppender> aholder(
                    app->tryGetReferencedPrivateData<QoreLoggerAppender>(CID_LOGGERAPPENDER, xsink), xsink
                );
                if (aholder) {
                    aholder->processEvent((int)type.getAsBigInt(), params, xsink);
                    continue;
                }
            }
        }

        ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
        args->push(type.getAsBigInt(), xsink);
        args->push(params.refSelf(), xsink);
        app->evalMethod("processEvent", *args, xsink).discard(xsink);
    }
}

QoreHashNode* QoreLoggerAppenderQueue::getEvent(int64 ms, ExceptionSink* xsink) {
    if (!q.size() && !ms) {
        return nullptr;
    }
    ValueHolder h(xsink);
    if (!ms) {
        h = q.shift(xsink, qobj, -1);
    } else if (ms > 0) {
        h = q.shift(xsink, qobj, (int)ms);
    } else {
        h = q.shift(xsink, qobj);
    }
    if (*xsink) {
        const QoreValue v = xsink->getExceptionErr();
        if (v.getType() == NT_STRING && *v.get<const QoreStringNode>() == "QUEUE-TIMEOUT") {
            xsink->clear();
        }
        return nullptr;
    }
    return h.releaseAs<QoreHashNode>();
}
