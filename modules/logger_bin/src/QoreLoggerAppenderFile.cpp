/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QoreLoggerAppenderFile.cpp LoggerAppenderFile class definition */
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

#include "qore_logger.h"
#include "QC_LoggerAppenderFile.h"
#include "QC_LoggerAppenderWithLayout.h"

QoreLoggerAppenderFile::QoreLoggerAppenderFile(QoreObject* self, const QoreStringNode* name, const QoreObject* layout,
        const QoreStringNode* filename, const QoreEncoding* enc, ExceptionSink* xsink)
        : QoreLoggerAppenderWithLayout(self, name, layout, xsink) {
    init(filename, enc, xsink);
}

QoreLoggerAppenderFile::QoreLoggerAppenderFile(QoreObject* self, const QoreObject* layout,
        const QoreStringNode* filename, const QoreEncoding* enc, ExceptionSink* xsink)
        : QoreLoggerAppenderWithLayout(self, layout, xsink) {
    init(filename, enc, xsink);
}

void QoreLoggerAppenderFile::init(const QoreStringNode* filename, const QoreEncoding* enc, ExceptionSink* xsink) {
    SimpleRefHolder<File> f(new File(enc));
    this->f = *f;
    this->f->ref();

    ReferenceHolder<QoreObject> file(new QoreObject(QC_FILE, getProgram(), f.release()), xsink);
    self->setValue("file", file.release(), xsink);
    self->setValue("fileName", filename->stringRefSelf(), xsink);
}

//! Returns the file object for the appender
QoreValue QoreLoggerAppenderFile::getFile(ExceptionSink* xsink) const {
    return self->getReferencedMemberNoMethod("file", xsink);
}

//! Returns the current filename
QoreValue QoreLoggerAppenderFile::getFileName(ExceptionSink* xsink) const {
    return self->getReferencedMemberNoMethod("fileName", xsink);
}

//! Processes open, close, and log events with the file and ignores all other events
/**
    @param type the event type
    @param params parameters for the event
*/
void QoreLoggerAppenderFile::processEventImpl(ExceptionSink* xsink, int64 type, const QoreValue params) {
    switch (type) {
        case EVENT_OPEN:
            openFile(xsink);
            break;
        case EVENT_CLOSE:
            closeFile(xsink);
            break;
        case EVENT_REOPEN:
            reopen(xsink);
            break;
        case EVENT_LOG: {
            qore_type_t t = params.getType();
            if (t == NT_STRING) {
                QoreStringNodeValueHelper str(params);
                f->write(*str, xsink);
            } else if (t == NT_BINARY) {
                f->write(params.get<const BinaryNode>(), xsink);
            } else {
                xsink->raiseException("APPENDER-FILE-ERROR", new QoreStringNodeMaker("Cannot write type \"%s\"; "
                    "expecting \"string\" or \"binary\"", params.getFullTypeName()));
            }
            break;
        }
    }
}

void QoreLoggerAppenderFile::openFile(ExceptionSink* xsink) {
    ValueHolder fn(self->getReferencedMemberNoMethod("fileName", xsink), xsink);
    if (*xsink) {
        return;
    }
    if (fn->getType() != NT_STRING) {
        xsink->raiseException("APPENDER-OPEN-ERROR", new QoreStringNodeMaker("\"FileName\" has type \"%s\"; "
            "expecting \"string\"", fn->getFullTypeName()));
    }

    QoreStringValueHelper filename(*fn);
    f->open2(xsink, filename->c_str(), (int)DEFAULT_OPEN_FLAGS, 0644, f->getEncoding());
}

void QoreLoggerAppenderFile::closeFile(ExceptionSink* xsink) {
    closeFileStatic(f, xsink);
}

void QoreLoggerAppenderFile::closeFileStatic(File* file, ExceptionSink* xsink) {
    file->close(xsink);
    if (*xsink) {
        const QoreValue v = xsink->getExceptionErr();
        QoreStringValueHelper err(v);
        if (v.getType() == NT_STRING && !strcmp(err->c_str(), "ILLEGAL-EXPRESSION")) {
            xsink->clear();
        }
    }
}

void QoreLoggerAppenderFile::reopen(ExceptionSink* xsink) {
    // issue #4842: reopen atomically without closing
    openFile(xsink);
}

void QoreLoggerAppenderFile::derefIntern(ExceptionSink* xsink) {
    f->deref(xsink);
    QoreLoggerAppenderWithLayout::derefIntern(xsink);
}
