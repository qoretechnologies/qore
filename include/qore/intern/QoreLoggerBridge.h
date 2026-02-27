/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreLoggerBridge.h

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

#ifndef _QORE_QORELOGGERBRIDGE_H

#define _QORE_QORELOGGERBRIDGE_H

#include <qore/QoreAbstractLoggerInterface.h>

//! Logger bridge that wraps a QoreObject implementing LoggerInterfaceBase
/** Provides a C++ QoreAbstractLoggerInterface backed by a Qore object with
    logArgs(int, string, *softlist<auto>) and isEnabledFor(int) methods.

    Used by AsyncIoControllerPriv (per-controller logger) and the global
    async I/O logger singleton.

    @since %Qore 2.3
*/
class QoreLoggerBridge : public QoreAbstractLoggerInterface {
public:
    DLLLOCAL QoreLoggerBridge(QoreObject* logger_obj);
    DLLLOCAL virtual ~QoreLoggerBridge();

    DLLLOCAL virtual void logArgs(int level, const QoreStringNode* msg,
        const QoreListNode* args, ExceptionSink* xsink) override;
    DLLLOCAL virtual bool isEnabledFor(int level) const override;

    DLLLOCAL virtual void deref(ExceptionSink* xsink);

    //! Returns the wrapped Qore object (not referenced)
    DLLLOCAL QoreObject* getObject() const { return logger_obj; }

private:
    QoreObject* logger_obj;                //!< Referenced
    const QoreMethod* logArgsMethod;       //!< logArgs(int, string, *softlist<auto>)
    const QoreMethod* isEnabledForMethod;  //!< isEnabledFor(int)
};

#endif // _QORE_QORELOGGERBRIDGE_H
