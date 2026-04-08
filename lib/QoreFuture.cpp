/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreFuture.cpp

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
#include "qore/intern/QoreFuture.h"

// --- QoreFuture ---

QoreFuture::QoreFuture(qore_future_private* p) : priv(p) {
    priv->ref();
}

QoreFuture::~QoreFuture() {
}

QoreValue QoreFuture::get(int64 timeout_ms, ExceptionSink* xsink) {
    return priv->get(timeout_ms, xsink);
}

bool QoreFuture::isDone() const {
    return priv->isDone();
}

bool QoreFuture::isError() const {
    return priv->isError();
}

bool QoreFuture::cancel() {
    return priv->cancel();
}

void QoreFuture::destructor(ExceptionSink* xsink) {
    priv->futureDestroyed(xsink);
}

void QoreFuture::deref(ExceptionSink* xsink) {
    if (ROdereference()) {
        priv->deref();
        delete this;
    }
}

// --- QorePromise ---

QorePromise::QorePromise() : priv(new qore_future_private()) {
}

QorePromise::~QorePromise() {
}

void QorePromise::set(QoreValue val, ExceptionSink* xsink) {
    priv->set(val, xsink);
}

void QorePromise::setError(const char* err, const char* desc, QoreValue arg, ExceptionSink* xsink) {
    priv->setError(err, desc, arg, xsink);
}

void QorePromise::setException(ExceptionSink& xs) {
    priv->setException(xs);
}

QoreFuture* QorePromise::getFuture(ExceptionSink* xsink) {
    if (!priv->markFutureRetrieved()) {
        xsink->raiseException("PROMISE-ERROR", "Future has already been retrieved from this Promise");
        return nullptr;
    }
    return new QoreFuture(priv);
}

void QorePromise::destructor(ExceptionSink* xsink) {
    priv->promiseDestroyed(xsink);
}

void QorePromise::deref(ExceptionSink* xsink) {
    if (ROdereference()) {
        priv->deref();
        delete this;
    }
}

qore_future_private* qore_future_private::getFromPromise(QorePromise* p) {
    return p->priv;
}
