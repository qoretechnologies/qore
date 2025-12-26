/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QoreLoggerLayoutPattern.h LoggerLayoutPattern class definition */
/*
    Qore Programming Language

    Copyright (C) 2003 - 2025 Qore Technologies, s.r.o.

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

#ifndef _QORE_MODULE_LOGGER_LOGGERLAYOUTPATTERN_H

#define _QORE_MODULE_LOGGER_LOGGERLAYOUTPATTERN_H

#include "QoreLoggerPattern.h"
#include "QoreLoggerEvent.h"

//! default layout pattern
#define DEFAULT_PATTERN "%r [%t] %p %c - %m%n"

//! default date format
#define DEFAULT_DATE_FORMAT "YYYY-MM-DD HH:mm:SS.u"

class QoreLoggerLayoutPattern : public QoreLoggerPattern {
public:
    static SimpleRefHolder<QoreStringNode> HostName;
    static SimpleRefHolder<QoreStringNode> LineDelimeter;

    DLLLOCAL QoreLoggerLayoutPattern(QoreObject* self) : QoreLoggerPattern(self) {
    }

    DLLLOCAL virtual ~QoreLoggerLayoutPattern() {
    }

    DLLLOCAL QoreValue resolveField(QoreObject* event, QoreLoggerEvent* ev, const QoreStringNode* key,
            const QoreStringNode* option, ExceptionSink* xsink) const;

    //! Format event record
    DLLLOCAL virtual QoreStringNode* format(ExceptionSink* xsink, const QoreValue data, const QoreObject* event,
            QoreLoggerEvent* e) const;
};

#endif
