/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QC_AutoGate.h

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

#ifndef _QORE_QC_AUTOGATE_H

#define _QORE_QC_AUTOGATE_H

#include <qore/Qore.h>
#include "qore/intern/QC_Gate.h"

DLLEXPORT extern qore_classid_t CID_AUTOGATE;
DLLLOCAL extern QoreClass* QC_AUTOGATE;

DLLLOCAL QoreClass *initAutoGateClass(QoreNamespace& ns);

class QoreAutoGate : public AbstractPrivateData {
public:
    //! Constructor that grabs the gate
    DLLLOCAL QoreAutoGate(QoreGate* gt, ExceptionSink* xsink) : g(gt) {
        g->grab(xsink);
    }

    //! Constructor for when gate is already entered (used by polling implementation)
    DLLLOCAL QoreAutoGate(QoreGate* gt) : g(gt) {
    }

    using AbstractPrivateData::deref;
    DLLLOCAL virtual void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            g->release(xsink);
            g->deref(xsink);
            delete this;
        }
    }

private:
    QoreGate* g;
};

#endif
