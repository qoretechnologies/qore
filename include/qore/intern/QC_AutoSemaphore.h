/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_AutoSemaphore.h

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

#ifndef _QORE_CLASS_AUTOSEMAPHORE_H
#define _QORE_CLASS_AUTOSEMAPHORE_H

#include "qore/intern/QC_Semaphore.h"

DLLEXPORT extern qore_classid_t CID_AUTOSEMAPHORE;
DLLLOCAL extern QoreClass* QC_AUTOSEMAPHORE;

DLLLOCAL QoreClass* initAutoSemaphoreClass(QoreNamespace& ns);

class QoreAutoSemaphore : public AbstractPrivateData {
    QoreSemaphore* sem;
    bool acquired;

public:
    DLLLOCAL QoreAutoSemaphore(QoreSemaphore* s) : sem(s), acquired(true) {
    }

    using AbstractPrivateData::deref;
    DLLLOCAL virtual void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            sem->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL void destructor(ExceptionSink* xsink) {
        if (acquired) {
            sem->release(1, xsink);
            acquired = false;
        }
    }

    DLLLOCAL void release(ExceptionSink* xsink) {
        if (acquired) {
            sem->release(1, xsink);
            acquired = false;
        } else {
            xsink->raiseException("AUTOSEMAPHORE-ERROR", "semaphore permit has already been released");
        }
    }

    DLLLOCAL int acquire(int64 timeout_ms, ExceptionSink* xsink) {
        if (acquired) {
            xsink->raiseException("AUTOSEMAPHORE-ERROR", "semaphore permit is already acquired");
            return -1;
        }
        bool timed_out = false;
        int rc = sem->acquire(timeout_ms, xsink, timed_out);
        if (rc == 0) {
            acquired = true;
        } else if (timed_out && !*xsink) {
            xsink->raiseException("SEMAPHORE-TIMEOUT",
                "timed out after " QLLD "ms waiting to acquire semaphore", timeout_ms);
        }
        return rc;
    }
};

#endif // _QORE_CLASS_AUTOSEMAPHORE_H
