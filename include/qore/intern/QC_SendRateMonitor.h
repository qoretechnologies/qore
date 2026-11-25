/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QC_SendRateMonitor.h

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

#ifndef _QORE_CLASS_SENDRATEMONITOR_H

#define _QORE_CLASS_SENDRATEMONITOR_H

#include <qore/AbstractPrivateData.h>
#include <qore/QoreThreadLock.h>
#include "qore/intern/SendRateWindow.h"

DLLEXPORT extern qore_classid_t CID_SENDRATEMONITOR;
DLLEXPORT extern QoreClass* QC_SENDRATEMONITOR;

DLLLOCAL QoreClass* initSendRateMonitorClass(QoreNamespace& ns);

//! The private data of a SendRateMonitor object: a window with a lock
class QoreSendRateMonitor : public AbstractPrivateData {
public:
    DLLLOCAL QoreSendRateMonitor(int64 min_rate, int64 window_ms) : window(min_rate, window_ms) {
    }

    DLLLOCAL QoreSendRateMonitor(const QoreSendRateMonitor& old) : AbstractPrivateData(), window(old.get()) {
    }

    DLLLOCAL bool update(int64 total_bytes, bool waiting, int64 now_ms) {
        AutoLocker al(m);
        return window.update(total_bytes, waiting, now_ms);
    }

    DLLLOCAL void reset() {
        AutoLocker al(m);
        bool steps = window.hasProgressSteps();
        int64 max_charge = window.getMaxWaitCharge();
        window = SendRateWindow(window.getMinRate(), window.getWindowMs());
        if (steps) {
            window.enableProgressSteps();
        }
        window.setMaxWaitCharge(max_charge);
    }

    DLLLOCAL void setMaxWaitCharge(int64 ms) {
        AutoLocker al(m);
        window.setMaxWaitCharge(ms);
    }

    DLLLOCAL void enableProgressSteps() {
        AutoLocker al(m);
        window.enableProgressSteps();
    }

    DLLLOCAL SendRateWindow get() const {
        AutoLocker al(m);
        return window;
    }

private:
    mutable QoreThreadLock m;
    SendRateWindow window;
};

#endif // _QORE_CLASS_SENDRATEMONITOR_H
