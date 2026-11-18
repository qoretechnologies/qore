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
class FileLineIterator : public QoreIteratorBase, public QoreLineTerminatorSource {
public:
    DLLLOCAL FileLineIterator(ExceptionSink* xsink, const QoreStringNode* name, const QoreEncoding* enc = QCS_DEFAULT,
        const QoreStringNode* n_eol = 0, bool n_trim = true, int flags = 0,
        qore_encoding_errors_t encoding_errors = qore_encoding_errors_t::RAISE) :
        eol(n_eol ? n_eol->stringRefSelf() : nullptr),
        encoding(enc),
        filename(name->stringRefSelf()),
        trim(n_trim),
        flags(flags),
        encoding_errors(encoding_errors) {
        doReset(xsink);
    }

    DLLLOCAL FileLineIterator(ExceptionSink* xsink, const FileLineIterator& old) :
        eol(old.eol ? old.eol->stringRefSelf() : nullptr),
        encoding(old.encoding),
        filename(old.filename->stringRefSelf()),
        trim(old.trim),
        flags(old.flags),
        encoding_errors(old.encoding_errors) {
        doReset(xsink);
    }

    DLLLOCAL ~FileLineIterator() {
    }

    DLLLOCAL bool next(ExceptionSink* xsink) {
        line = nullptr;
        bool validp = src->next(xsink);
        if (validp && convert) {
            // the lines of a file in an encoding that is not ASCII-compatible are returned in UTF-8; a line that is
            // not valid in the encoding has been tagged ISO-8859-1 by the reader, or raises an exception here with
            // the encoding_errors option "keep"
            SimpleRefHolder<QoreStringNode> l(src->getValue());
            line = toUtf8(**l, xsink);
            if (*xsink) {
                line = nullptr;
                return false;
            }
        }
        // the file is read from its start again after its end, not after an exception, after which reading goes on
        if (!validp && !*xsink) {
            doReset(xsink);
        }
        return validp;
    }

    DLLLOCAL int64 index() {
        return src->index();
    }

    DLLLOCAL QoreStringNode* getValue() {
        if (convert) {
            return line ? line->stringRefSelf() : nullptr;
        }
        return src->getValue();
    }

    DLLLOCAL const std::string& getRemovedLineTerminator() const override {
        return src ? src->getRemovedLineTerminator() : noLineTerminator();
    }

    DLLLOCAL bool valid() {
        return src->valid();
    }

    DLLLOCAL int checkValid(ExceptionSink* xsink) {
        return src->checkValid(xsink);
    }

    DLLLOCAL void reset(ExceptionSink* xsink) {
        // also after an exception, after which the iterator is not valid in the middle of the file
        doReset(xsink);
    }

    DLLLOCAL const QoreEncoding* getEncoding() {
        return convert ? QCS_UTF8 : src->getEncoding();
    }

    //! returns what a line that is not valid in the encoding gives (the encoding_errors option)
    DLLLOCAL qore_encoding_errors_t getEncodingErrors() const {
        return encoding_errors;
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

    DLLLOCAL virtual void deref() override {
        if (ROdereference())
            delete this;
    }

    DLLLOCAL virtual const char* getName() const override { return "FileLineIterator"; }

    DLLLOCAL virtual const QoreTypeInfo* getElementType() const override {
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
    //! returns the given line in UTF-8
    /** A line in a Unicode encoding (UTF-16, UTF-32) is converted with the character functions of the encoding, with
        no conversion to open for each line; a line that is not valid in its encoding, or in another encoding, is
        converted with iconv, which raises \c ENCODING-CONVERSION-ERROR for text that is not valid
    */
    DLLLOCAL static QoreStringNode* toUtf8(const QoreStringNode& l, ExceptionSink* xsink) {
        const QoreEncoding* e = l.getEncoding();
        const qore_encoding_private* ep = qore_encoding_private::get(*e);
        if (ep->get_unicode && e->isMultiByte()) {
            const char* p = l.c_str();
            size_t len = l.size();
            SimpleRefHolder<QoreStringNode> rv(new QoreStringNode(QCS_UTF8));
            rv->reserve(len + len / 2);
            size_t i = 0;
            while (i < len) {
                qore_offset_t cl = e->getCharLen(p + i, len - i);
                if (cl <= 0) {
                    break;
                }
                rv->concatUTF8FromUnicode(ep->getUnicode(p + i));
                i += cl;
            }
            if (i == len) {
                return rv.release();
            }
        }
        return l.convertEncoding(QCS_UTF8, xsink);
    }

    DLLLOCAL void doReset(ExceptionSink* xsink) {
        fis = new FileInputStream(*filename, -1, flags, xsink);
        if (*xsink)
            return;
        fis->ref();
        // the lines are read in the encoding of the file, so that they are checked in it; in an encoding that is
        // not ASCII-compatible, each line is then converted to UTF-8 (see next())
        src = new InputStreamLineIterator(xsink, *fis, encoding, *eol, trim, DefaultStreamBufferSize,
            encoding_errors);
        src->setOwnerName("FileLineIterator");
    }

    SimpleRefHolder<InputStreamLineIterator> src = nullptr;
    //! the current line in UTF-8, for a file in an encoding that is not ASCII-compatible
    SimpleRefHolder<QoreStringNode> line;
    SimpleRefHolder<FileInputStream> fis = nullptr;
    SimpleRefHolder<QoreStringNode> eol;
    const QoreEncoding* encoding;
    SimpleRefHolder<QoreStringNode> filename;
    bool trim;
    int flags;
    //! what a line that is not valid in the encoding gives (the encoding_errors option)
    qore_encoding_errors_t encoding_errors;
    //! true if the lines are converted to UTF-8
    bool convert = !encoding->isAsciiCompat();
};

#endif // _QORE_FILELINEITERATOR_H
