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
#include "qore/intern/QC_Queue.h"

QoreQueueHelper::QoreQueueHelper(QoreObject* obj, ExceptionSink* xs) : QorePrivateObjectAccessHelper(xs) {
    Queue* q = reinterpret_cast<Queue*>(obj->getReferencedPrivateData(CID_QUEUE, xs));
    if (!q) {
        if (!*xsink)
            xsink->raiseException("QUEUE-ERROR", "expecting an object derived from Queue; got class '%s' instead", obj->getClassName());
        return;
    }
    ptr = (void*)q;
}

QoreQueueHelper::~QoreQueueHelper() {
    // the constructor holds a reference to the queue for the lifetime of the helper
    if (ptr) {
        reinterpret_cast<Queue*>(ptr)->deref(xsink);
    }
}

void QoreQueueHelper::push(ExceptionSink* xsink, QoreValue n, int timeout_ms, bool* to) {
    reinterpret_cast<Queue*>(ptr)->push(xsink, n, timeout_ms, to);
}

void QoreQueueHelper::insert(ExceptionSink* xsink, QoreValue n, int timeout_ms, bool* to) {
    reinterpret_cast<Queue*>(ptr)->insert(xsink, n, timeout_ms, to);
}

QoreValue QoreQueueHelper::shift(ExceptionSink* xsink, int timeout_ms, bool* to) {
    return reinterpret_cast<Queue*>(ptr)->shift(xsink, timeout_ms, to);
}

QoreValue QoreQueueHelper::pop(ExceptionSink* xsink, int timeout_ms, bool* to) {
    return reinterpret_cast<Queue*>(ptr)->pop(xsink, timeout_ms, to);
}

int QoreQueueHelper::size() const {
    return reinterpret_cast<Queue*>(ptr)->size();
}

int QoreQueueHelper::getMax() const {
    return reinterpret_cast<Queue*>(ptr)->getMax();
}

unsigned QoreQueueHelper::getReadWaiting() const {
    return reinterpret_cast<Queue*>(ptr)->getReadWaiting();
}

unsigned QoreQueueHelper::getWriteWaiting() const {
    return reinterpret_cast<Queue*>(ptr)->getWriteWaiting();
}

void QoreQueueHelper::clear(ExceptionSink* xsink) {
    return reinterpret_cast<Queue*>(ptr)->clear(xsink);
}
