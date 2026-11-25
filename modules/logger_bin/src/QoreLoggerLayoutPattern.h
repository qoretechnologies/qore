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

#include <atomic>

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

    //! Re-decides whether the hostname, the PID and the environment are formatted for the code that sets a pattern
    /** A layout whose pattern code restricted from external information sets formats none of them (qore#7586)
    */
    DLLLOCAL void patternSetByCaller() {
        host_info = callerMayReadExternalInfo();
    }

    //! Returns true if the code that creates or calls the layout may read external information; fails closed
    DLLLOCAL static bool callerMayReadExternalInfo() {
#ifdef QORE_HAVE_CALLER_AMBIENT_PREDICATES
        return qore_caller_may_read_external_info();
#else
        // a libqore that cannot evaluate the rule: fail closed
        return false;
#endif
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
    /** Not if code restricted from external information created the layout or set its pattern; an appender of the
        host formats them whatever code logs through it, as the file names of the Logger file appenders do, while a
        direct call of the layout's format methods by such code gets none of them (see QoreLoggerHostInfoHelper,
        qore#7586)
    */
    std::atomic<bool> host_info;
};

//! Leaves the host's values out of the fields that a layout formats in a call of its public format methods by code
//! restricted from external information (qore#7586)
/** The formatting of an appender (QoreLoggerAppenderWithLayout) marks itself with QoreLoggerAppenderFormatHelper,
    so that the layout of an appender of the host keeps its fields when it formats for such code
*/
class QoreLoggerHostInfoHelper {
public:
    DLLLOCAL QoreLoggerHostInfoHelper();
    DLLLOCAL ~QoreLoggerHostInfoHelper();

    //! returns true if the host's values are left out in this thread
    DLLLOCAL static bool suppressed();

private:
    bool set = false;
};

//! Marks the formatting of an appender in this thread
/** For an appender that may format the host's values (see QoreLoggerAppenderWithLayout::host_info),
    QoreLoggerHostInfoHelper does not apply in it; for one that may not, the host's values are left out in it
*/
class QoreLoggerAppenderFormatHelper {
public:
    DLLLOCAL QoreLoggerAppenderFormatHelper(bool host_info);
    DLLLOCAL ~QoreLoggerAppenderFormatHelper();

    //! returns true if an appender that may format the host's values formats in this thread
    DLLLOCAL static bool active();

private:
    bool prev;
    bool suppressing;
};

#endif
