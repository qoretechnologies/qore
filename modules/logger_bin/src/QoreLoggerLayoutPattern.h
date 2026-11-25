/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QoreLoggerLayoutPattern.h LoggerLayoutPattern class definition */
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

#ifndef _QORE_MODULE_LOGGER_LOGGERLAYOUTPATTERN_H

#define _QORE_MODULE_LOGGER_LOGGERLAYOUTPATTERN_H

#include "QoreLoggerPattern.h"
#include "QoreLoggerEvent.h"

#include <qore/QoreSandboxManager.h>

//! default layout pattern
#define DEFAULT_PATTERN "%r [%t] %p %c - %m%n"

//! default date format
#define DEFAULT_DATE_FORMAT "YYYY-MM-DD HH:mm:SS.u"

class QoreLoggerLayoutPattern : public QoreLoggerPattern {
public:
    static SimpleRefHolder<QoreStringNode> HostName;
    static SimpleRefHolder<QoreStringNode> LineDelimeter;

    DLLLOCAL QoreLoggerLayoutPattern(QoreObject* self) : QoreLoggerPattern(self),
            host_info(callerMayReadExternalInfo()) {
    }

    DLLLOCAL virtual ~QoreLoggerLayoutPattern() {
    }

    DLLLOCAL QoreValue resolveField(QoreObject* event, QoreLoggerEvent* ev, const QoreStringNode* key,
            const QoreStringNode* option, ExceptionSink* xsink) const;

    //! Format event record
    DLLLOCAL virtual QoreStringNode* format(ExceptionSink* xsink, const QoreValue data, const QoreObject* event,
            QoreLoggerEvent* e) const;

private:
    //! if the hostname, the PID and the values of environment variables are formatted (\c %h, \c %P, \c %E)
    /** Not if code restricted from external information created the layout: it is decided when the layout is
        created, so a layout of the host keeps its fields whatever code logs through it, as the file names of the
        Logger file appenders do (qore#7586)
    */
    const bool host_info;

    //! Returns true if the code that creates the layout may read external information; fails closed
    DLLLOCAL static bool callerMayReadExternalInfo() {
#ifdef QORE_HAVE_CALLER_AMBIENT_PREDICATES
        return qore_caller_may_read_external_info();
#else
        // a libqore that cannot evaluate the rule: fail closed
        return false;
#endif
    }
};

#endif
