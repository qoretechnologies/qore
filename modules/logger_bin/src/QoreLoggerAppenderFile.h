/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QoreLoggerAppenderFile.h LoggerAppenderFile class definition */
/*
    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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

#ifndef _QORE_MODULE_LOGGER_LOGGERAPPENDERFILE_H

#define _QORE_MODULE_LOGGER_LOGGERAPPENDERFILE_H

#include <fcntl.h>

//! reopen event
constexpr int64 EVENT_REOPEN = 1001;

//! Default open flags
constexpr int64 DEFAULT_OPEN_FLAGS = O_CREAT | O_APPEND | O_WRONLY;

// forward references
class QoreLoggerAppenderWithLayout;

class QoreLoggerAppenderFile : public QoreLoggerAppenderWithLayout {
public:
    DLLLOCAL QoreLoggerAppenderFile(QoreObject* self, const QoreStringNode* name, const QoreObject* layout,
            const QoreStringNode* filename, const QoreEncoding* enc, ExceptionSink* xsink);

    DLLLOCAL QoreLoggerAppenderFile(QoreObject* self, const QoreObject* layout, const QoreStringNode* filename,
            const QoreEncoding* enc, ExceptionSink* xsink);

    //! Returns the file object for the appender
    DLLLOCAL QoreValue getFile(ExceptionSink* xsink) const;

    //! Returns the current filename
    DLLLOCAL QoreValue getFileName(ExceptionSink* xsink) const;

    //! Processes open, close, and log events with the file and ignores all other events
    DLLLOCAL void processEventImpl(ExceptionSink* xsink, int64 type, const QoreValue params);

    DLLLOCAL void openFile(ExceptionSink* xsink);

    DLLLOCAL void closeFile(ExceptionSink* xsink);

    DLLLOCAL void reopen(ExceptionSink* xsink);

    DLLLOCAL static void closeFileStatic(File* f, ExceptionSink* xsink);

    //! Writes a single comment-style lifecycle marker line to the current file when markers are enabled
    /** No-op when markers are disabled or the file is not open. The line is prefixed with @c "#"
        so line-oriented log parsers can drop it cheaply. Format:
        @code # YYYY-MM-DDTHH:mm:SS.xx logger=<name> action=<action> file=<path>[ <detail>] @endcode
    */
    DLLLOCAL void writeLifecycleMarker(ExceptionSink* xsink, const char* action, const QoreStringNode* detail);

    DLLLOCAL bool lifecycleMarkersEnabled() const { return lifecycle_markers; }

    DLLLOCAL void setLifecycleMarkers(bool enable) { lifecycle_markers = enable; }

protected:
    File* f = nullptr;
    const QoreEncoding* enc = nullptr;
    bool lifecycle_markers = false;

    DLLLOCAL void init(const QoreStringNode* filename, const QoreEncoding* enc, ExceptionSink* xsink);

    using QoreLoggerAppenderWithLayout::derefIntern;
    DLLLOCAL virtual void derefIntern(ExceptionSink* xsink);
};

#endif
