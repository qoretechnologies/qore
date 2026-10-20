/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QoreLoggerAppenderWithLayout.cpp LoggerAppenderWithLayout class definition */
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
#include "QC_LoggerAppenderWithLayout.h"
#include "QC_LoggerLayoutPattern.h"
#include "QC_LoggerPattern.h"

QoreLoggerAppenderWithLayout::QoreLoggerAppenderWithLayout(QoreObject* self, const QoreObject* layout,
        ExceptionSink* xsink) : QoreLoggerAppender(xsink, self) {
    setLayout(xsink, layout);
}

QoreLoggerAppenderWithLayout::QoreLoggerAppenderWithLayout(QoreObject* self, const QoreStringNode* n,
        const QoreObject* layout, ExceptionSink* xsink) : QoreLoggerAppender(xsink, self, n) {
    setLayout(xsink, layout);
}

//! Assigns a layout to the appender
void QoreLoggerAppenderWithLayout::setLayout(ExceptionSink* xsink, const QoreObject* layout) {
    QoreAutoRWWriteLocker awl(lock);
    assert(!owns_refs);
    if (llp) {
        llp->deref(xsink);
        llp = nullptr;
    }
    // the reference to the layout is held by the layout internal member, where the garbage collector can follow it
    this->layout = const_cast<QoreObject*>(layout);
    self->setMemberValue("layout", QC_LOGGERAPPENDERWITHLAYOUT, layout->objectRefSelf(), xsink);

    const QoreClass* cls = layout->getClass();
    if (cls->isEqual(*QC_LOGGERLAYOUTPATTERN)) {
        llp = layout->tryGetReferencedPrivateData<QoreLoggerLayoutPattern>(CID_LOGGERLAYOUTPATTERN, xsink);
    }
}

//! Returns the layout for the appender
QoreObject* QoreLoggerAppenderWithLayout::getLayout() {
    QoreAutoRWReadLocker awl(lock);
    return layout ? layout->objectRefSelf() : nullptr;
}

QoreValue QoreLoggerAppenderWithLayout::serializeImpl(ExceptionSink* xsink, const QoreObject* event,
        QoreLoggerEvent* e) {
    ReferenceHolder<QoreObject> layout(xsink);
    {
        QoreAutoRWReadLocker awl(lock);
        assert(this->layout);
        layout = this->layout->objectRefSelf();
    }
    if (e && llp) {
        return llp->format(xsink, event, event, e);
    }
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
    args->push(event->objectRefSelf(), xsink);
    assert(!*xsink);
    return layout->evalMethod("format", *args, xsink);
}

void QoreLoggerAppenderWithLayout::derefIntern(ExceptionSink* xsink) {
    if (llp) {
        llp->deref(xsink);
    }
    // the layout is referenced here only after the object is destroyed, see QoreLoggerAppender::destructor()
    if (layout && owns_refs) {
        layout->deref(xsink);
    }
    QoreLoggerAppender::derefIntern(xsink);
}

