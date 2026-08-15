/*
    QoreRWLock.cpp

    Read-Write Lock objects; readers are always preferred, which is what allows the read lock to be
    grabbed recursively.  Preferring writers is not implemented: a thread holding the read lock
    that grabs it again would deadlock behind a queued writer.

    Qore Programming Language

    Copyright (C) 2003 - 2026 David Nichols

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
#include "qore/intern/qore_var_rwlock_priv.h"

QoreRWLock::QoreRWLock() : priv(new qore_var_rwlock_priv) {
}

QoreRWLock::~QoreRWLock() {
    delete priv;
}

int QoreRWLock::wrlock() {
    priv->wrlock();
    return 0;
}

int QoreRWLock::trywrlock() {
    return priv->trywrlock();
}

int QoreRWLock::unlock() {
    priv->unlock();
    return 0;
}

int QoreRWLock::rdlock() {
    priv->rdlock();
    return 0;
}

int QoreRWLock::tryrdlock() {
    return priv->tryrdlock();
}

QoreVarRWLock::QoreVarRWLock(qore_var_rwlock_priv* p) : priv(p) {
}

QoreVarRWLock::QoreVarRWLock() : priv(new qore_var_rwlock_priv) {
}

//! destroys the lock
QoreVarRWLock::~QoreVarRWLock() {
    delete priv;
}

//! grabs the write lock
void QoreVarRWLock::wrlock() {
    priv->wrlock();
}

//! tries to grab the write lock; does not block if unsuccessful; returns 0 if successful
int QoreVarRWLock::trywrlock() {
    return priv->trywrlock();
}

//! unlocks the lock (assumes the lock is locked)
void QoreVarRWLock::unlock() {
    priv->unlock();
}

//! grabs the read lock
void QoreVarRWLock::rdlock() {
    priv->rdlock();
}

//! tries to grab the read lock; does not block if unsuccessful; returns 0 if successful
int QoreVarRWLock::tryrdlock() {
    return priv->tryrdlock();
}
