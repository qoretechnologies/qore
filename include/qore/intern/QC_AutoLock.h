/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QC_AutoLock.h
 
  Qore Programming Language
 
  Copyright (C) 2003 - 2023 David Nichols
 
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

#ifndef _QORE_QC_AUTOLOCK_H

#define _QORE_QC_AUTOLOCK_H

#include <qore/Qore.h>
#include "qore/intern/QC_Mutex.h"

DLLEXPORT extern qore_classid_t CID_AUTOLOCK;
DLLLOCAL extern QoreClass* QC_AUTOLOCK;

DLLLOCAL QoreClass *initAutoLockClass(QoreNamespace& ns);

class QoreAutoLock : public AbstractPrivateData {
   SmartMutex *m;

public:
   DLLLOCAL QoreAutoLock(class SmartMutex *mt, class ExceptionSink *xsink) {
      m = mt;
      m->grab(xsink);
   }

   // Constructor for when lock is already held (used by polling implementation)
   DLLLOCAL QoreAutoLock(class SmartMutex *mt) : m(mt) {
   }

   using AbstractPrivateData::deref;
   DLLLOCAL virtual void deref(class ExceptionSink *xsink) {
      if (ROdereference()) {
	 m->deref(xsink);
	 delete this;
      }
   }

   DLLLOCAL virtual void destructor(class ExceptionSink *xsink) {
      if (m->owns_lock())
	 m->release(xsink);
   }
   
   DLLLOCAL int lock(class ExceptionSink *xsink) {
      return m->grab(xsink);
   }
   DLLLOCAL int unlock(class ExceptionSink *xsink) {
      return m->release(xsink);
   }
   DLLLOCAL int trylock() {
      return m->tryGrab();
   }
};

#endif
