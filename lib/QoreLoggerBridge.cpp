/* -*- indent-tabs-mode: nil -*- */
/*
    QoreLoggerBridge.cpp

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
#include "qore/intern/QoreLoggerBridge.h"

QoreLoggerBridge::QoreLoggerBridge(QoreObject* logger_obj)
    : logger_obj(logger_obj), logArgsMethod(nullptr), isEnabledForMethod(nullptr) {
    assert(logger_obj);
    logger_obj->ref();
    const QoreClass* cls = logger_obj->getClass();
    logArgsMethod = cls->findMethod("logArgs");
    isEnabledForMethod = cls->findMethod("isEnabledFor");
}

QoreLoggerBridge::~QoreLoggerBridge() {
}

void QoreLoggerBridge::logArgs(int level, const QoreStringNode* msg,
        const QoreListNode* args, ExceptionSink* xsink) {
    if (!logArgsMethod) {
        return;
    }
    ReferenceHolder<QoreListNode> call_args(new QoreListNode(autoTypeInfo), xsink);
    call_args->push(level, xsink);
    if (msg) {
        call_args->push(msg->refSelf(), xsink);
    } else {
        call_args->push(QoreValue(), xsink);
    }
    if (args) {
        call_args->push(args->refSelf(), xsink);
    }
    ValueHolder rv(logger_obj->evalMethod(*logArgsMethod, *call_args, xsink), xsink);
}

bool QoreLoggerBridge::isEnabledFor(int level) const {
    if (!isEnabledForMethod) {
        return false;
    }
    ExceptionSink xsink;
    ReferenceHolder<QoreListNode> call_args(new QoreListNode(autoTypeInfo), &xsink);
    call_args->push(level, &xsink);
    ValueHolder rv(logger_obj->evalMethod(*isEnabledForMethod, *call_args, &xsink), &xsink);
    if (xsink) {
        return false;
    }
    return rv->getAsBool();
}

void QoreLoggerBridge::deref(ExceptionSink* xsink) {
    if (ROdereference()) {
        logger_obj->deref(xsink);
        logger_obj = nullptr;
        delete this;
    }
}
