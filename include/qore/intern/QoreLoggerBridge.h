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

#include <cstdarg>

//! Logger bridge that wraps a QoreObject implementing LoggerInterfaceBase
/** Provides a C++ QoreAbstractLoggerInterface backed by a Qore object with
    logArgs(int, string, *softlist<auto>) and isEnabledFor(int) methods.

    Used by AsyncIoControllerPriv (per-controller logger) and the global
    async I/O logger singleton.

    Both calls may be made from a native thread with no program context (ex: an
    async I/O thread's event loop); the logger object's program is entered for
    the duration of the call in that case.  If that program is already being
    torn down, the logger cannot take the message: logV() then returns false,
    so that the caller reports the message itself instead of losing it.

    @since %Qore 3.0
*/
class QoreLoggerBridge : public QoreAbstractLoggerInterface {
public:
    DLLLOCAL QoreLoggerBridge(QoreObject* logger_obj);
    DLLLOCAL virtual ~QoreLoggerBridge();

    //! Gives the message to the logger's logArgs() method; the message is dropped if the logger cannot take it
    /** Use logV() to know whether the logger took the message
    */
    DLLLOCAL virtual void logArgs(int level, const QoreStringNode* msg,
        const QoreListNode* args, ExceptionSink* xsink) override;
    DLLLOCAL virtual bool isEnabledFor(int level) const override;

    //! Logs the message rendered from @a fmt and @a args at @a level, if the logger is enabled for that level
    /** @return true if the logger took the message: it logged it, it is not enabled for the level, or one of its
        methods raised an exception, which is added to @a logger_xsink (or discarded if @a logger_xsink is nullptr);
        false if the logger could not take the message, because its Program is being torn down, and the caller must
        then report the message itself (qore#7698)
    */
    DLLLOCAL bool logV(int level, const char* fmt, va_list args, ExceptionSink* logger_xsink);

    //! Releases a reference of the logger that the async I/O logging took; an exception that the logger's destructor
    //! raises (when it was replaced in the meantime and this is its last reference) is reported on stderr if
    //! @a report, else discarded
    DLLLOCAL void derefLogged(bool report);

    DLLLOCAL virtual void deref(ExceptionSink* xsink) override;

    //! Returns the wrapped Qore object (not referenced)
    DLLLOCAL QoreObject* getObject() const { return logger_obj; }

private:
    //! Whether the logger takes messages of a level
    enum class Enabled {
        Yes,            //!< the logger is enabled for the level
        No,             //!< the logger is not enabled for the level, or its isEnabledFor() raised an exception
        Unavailable,    //!< the logger cannot be asked: its Program is being torn down
    };

    QoreObject* logger_obj;                //!< Referenced
    //! The logger object's program, or nullptr for a system object
    /** Not referenced here: the object holds a weak (dep) reference to its program for the whole
        time this bridge holds a reference to the object, so the program cannot be reclaimed while
        this pointer is in use.
    */
    QoreProgram* pgm;
    const QoreMethod* logArgsMethod;       //!< logArgs(int, string, *softlist<auto>)
    const QoreMethod* isEnabledForMethod;  //!< isEnabledFor(int)

    //! Asks the logger whether it is enabled for @a level; an exception that its isEnabledFor() method raises is added
    //! to @a xsink, and the logger then counts as not enabled
    DLLLOCAL Enabled checkEnabledFor(int level, ExceptionSink* xsink) const;

    //! Gives the message to the logger's logArgs() method; returns false if the logger cannot take it
    /** An exception that the method raises is added to @a xsink; the message counts as taken
    */
    DLLLOCAL bool deliver(int level, const QoreStringNode* msg, const QoreListNode* args, ExceptionSink* xsink);
};

//! Releases a reference of a QoreLoggerBridge that the async I/O logging took when it goes out of scope
/** See QoreLoggerBridge::derefLogged()
*/
class QoreLoggerBridgeLoggedHolder {
public:
    DLLLOCAL QoreLoggerBridgeLoggedHolder(QoreLoggerBridge* lgr, bool report) : lgr(lgr), report(report) {
    }

    DLLLOCAL ~QoreLoggerBridgeLoggedHolder() {
        lgr->derefLogged(report);
    }

    QoreLoggerBridgeLoggedHolder(const QoreLoggerBridgeLoggedHolder&) = delete;
    QoreLoggerBridgeLoggedHolder& operator=(const QoreLoggerBridgeLoggedHolder&) = delete;

private:
    QoreLoggerBridge* lgr;
    bool report;
};

#endif // _QORE_QORELOGGERBRIDGE_H
