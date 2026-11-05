/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QoreFastRoutes.h

  Qore Programming Language

  Copyright (C) 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_INTERN_QOREFASTROUTES_H

#define _QORE_INTERN_QOREFASTROUTES_H

#include <qore/Qore.h>
#include <qore/AbstractPrivateData.h>

#include "qore/intern/HttpFastRoutes.h"

//! The private data of a FastCell object: the cell, which routes share
/** This is the Qore-facing side of the fast routes.  It is not part of the isolated core that runs on the I/O threads.
*/
class QoreFastCell : public AbstractPrivateData {
public:
    DLLLOCAL QoreFastCell(int64 initial, int64 max_age_us) : c(std::make_shared<HttpFastCell>(initial, max_age_us)) {
    }

    const std::shared_ptr<HttpFastCell> c;
};

//! The private data of an HttpFastRoutes object: the routes of a listener
class QoreHttpFastRoutes : public AbstractPrivateData {
public:
    DLLLOCAL QoreHttpFastRoutes() : r(std::make_shared<HttpFastRoutes>()) {
    }

    const std::shared_ptr<HttpFastRoutes> r;
};

#endif
