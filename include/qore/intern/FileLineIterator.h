/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    FileLineIterator.h

    Qore Programming Language

    Copyright (C) 2016 - 2024 Qore Technologies, s.r.o.

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

#ifndef _QORE_FILELINEITERATOR_H
#define _QORE_FILELINEITERATOR_H

#include <cerrno>
#include <cstring>

#include "qore/intern/FileInputStream.h"
#include "qore/intern/InputStreamLineIterator.h"

/**
 * @brief Private data for the Qore::FileLineIterator class.
 */
class FileLineIterator : public QoreIteratorBase {
public:
    DLLLOCAL FileLineIterator(ExceptionSink* xsink, const QoreStringNode* name, const QoreEncoding* enc = QCS_DEFAULT,
        const QoreStringNode* n_eol = 0, bool n_trim = true, int flags = 0) :
        eol(n_eol ? n_eol->stringRefSelf() : nullptr),
        encoding(enc),
        filename(name->stringRefSelf()),
        trim(n_trim),
        flags(flags) {
        doReset(xsink);
    }

    DLLLOCAL FileLineIterator(ExceptionSink* xsink, const FileLineIterator& old) :
        eol(old.eol ? old.eol->stringRefSelf() : nullptr),
        encoding(old.encoding),
        filename(old.filename->stringRefSelf()),
        trim(old.trim),
        flags(old.flags) {
        doReset(xsink);
    }

    DLLLOCAL ~FileLineIterator() {
    }

    DLLLOCAL bool next(ExceptionSink* xsink) {
        bool validp = src->next(xsink);
        if (!validp) {
            doReset(xsink);
        }
        return validp;
    }

    DLLLOCAL int64 index() {
        return src->index();
    }

    DLLLOCAL QoreStringNode* getValue() {
        return src->getValue();
    }

    DLLLOCAL bool valid() {
        return src->valid();
    }

    DLLLOCAL int checkValid(ExceptionSink* xsink) {
        return src->checkValid(xsink);
    }

    DLLLOCAL void reset(ExceptionSink* xsink) {
        if (src->valid()) {
            doReset(xsink);
        }
    }

    DLLLOCAL const QoreEncoding* getEncoding() {
        return src->getEncoding();
    }

    DLLLOCAL const QoreStringNode* getFileName() {
        return *filename;
    }

    DLLLOCAL QoreListNode* stat(ExceptionSink* xsink) {
        return fis->getFile().stat(xsink);
    }

    DLLLOCAL QoreHashNode* hstat(ExceptionSink* xsink) {
        return fis->getFile().hstat(xsink);
    }

    DLLLOCAL bool isTty() {
        return fis->getFile().isTty();
    }

    DLLLOCAL virtual void deref() {
        if (ROdereference())
            delete this;
    }

    DLLLOCAL virtual const char* getName() const { return "FileLineIterator"; }

    DLLLOCAL virtual const QoreTypeInfo* getElementType() const {
        return stringTypeInfo;
    }

    // Native fast-path: same shape as DataLineIterator.  next() blocks on
    // disk I/O for each line; the dispatch elision is small per call but
    // multiplies on big files (the read itself is the dominant cost).
    DLLLOCAL bool supportsNativeIteration() const override { return true; }

    DLLLOCAL bool nativeNext(ExceptionSink* xsink) override {
        if (check(xsink)) {
            return false;
        }
        return next(xsink);
    }

    DLLLOCAL QoreValue nativeGetValue(ExceptionSink* xsink) override {
        if (check(xsink)) {
            return QoreValue();
        }
        if (checkValid(xsink)) {
            return QoreValue();
        }
        return getValue();
    }

private:
    DLLLOCAL void doReset(ExceptionSink* xsink) {
        fis = new FileInputStream(*filename, -1, flags, xsink);
        if (*xsink)
            return;
        fis->ref();
        if (!encoding->isAsciiCompat())
            src = new InputStreamLineIterator(xsink, new EncodingConversionInputStream(*fis, encoding, QCS_UTF8, xsink), QCS_UTF8, *eol, trim);
        else
            src = new InputStreamLineIterator(xsink, *fis, encoding, *eol, trim);
    }

    SimpleRefHolder<InputStreamLineIterator> src = nullptr;
    SimpleRefHolder<FileInputStream> fis = nullptr;
    SimpleRefHolder<QoreStringNode> eol;
    const QoreEncoding* encoding;
    SimpleRefHolder<QoreStringNode> filename;
    bool trim;
    int flags;
};

#endif // _QORE_FILELINEITERATOR_H
