/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    InputStreamLineIterator.h

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

#ifndef _QORE_INPUTSTREAMLINEITERATOR_H
#define _QORE_INPUTSTREAMLINEITERATOR_H

#include <cerrno>
#include <cstring>

#include "qore/InputStream.h"
#include "qore/intern/BufferedStreamReader.h"
#include "qore/intern/StreamReader.h"
#include "qore/intern/EncodingConversionInputStream.h"

/**
 * @brief Private data for the Qore::InputStreamLineIterator class.
 */
class InputStreamLineIterator : public QoreIteratorBase {
public:
    DLLLOCAL InputStreamLineIterator(ExceptionSink* xsink, InputStream* is, const QoreEncoding* encoding,
            const QoreStringNode* n_eol, bool n_trim, size_t bufsize = DefaultStreamBufferSize) :
        src(is, xsink),
        reader(xsink),
        enc(encoding),
        trim(n_trim) {
        if (assignEol(n_eol, xsink))
            return;

        // reference src for assignment to BufferedStreamReader
        src->ref();
        reader = new BufferedStreamReader(xsink, *src, enc, bufsize);
    }

    DLLLOCAL InputStreamLineIterator(ExceptionSink* xsink, StreamReader* sr, const QoreStringNode* n_eol = nullptr,
            bool n_trim = true) :
        src(xsink),
        reader(sr, xsink),
        enc(sr->getEncoding()),
        trim(n_trim) {
        if (assignEol(n_eol, xsink))
            return;

        // update the stream reader's encoding if necessary
        if (enc != sr->getEncoding()) {
            sr->setEncoding(enc);
        }
    }

    DLLLOCAL ~InputStreamLineIterator() {
    }

    DLLLOCAL bool next(ExceptionSink* xsink) {
        // Make sure to use a new string if the iterator was already valid.
        if (validp && line && !line->empty()) {
            line.discard();
        }
        validp = getLine(xsink);
        if (validp) {
            ++num;   // Increment line number.
        } else {
            num = 0; // Reset iterator.
        }
        //printd(5, "InputStreamLineIterator::next() this: %p line: %d offset: %lld validp: %d '%s'\n", this, num,
        //    offset, validp, line->getBuffer());
        return validp;
    }

    DLLLOCAL int64 index() const {
        return num;
    }

    DLLLOCAL QoreStringNode* getValue() {
        assert(validp);
        return line ? line->stringRefSelf() : nullptr;
    }

    DLLLOCAL bool valid() const {
        return validp;
    }

    DLLLOCAL int checkValid(ExceptionSink* xsink) const {
        if (!validp) {
            xsink->raiseException("ITERATOR-ERROR", "the %s is not pointing at a valid element; make sure %s::next() "
                "returns True before calling this method", getName(), getName());
            return -1;
        }
        return 0;
    }

    DLLLOCAL StreamReader* getStreamReader() {
        return *reader;
    }

    DLLLOCAL const QoreEncoding* getEncoding() const {
        return enc;
    }

    DLLLOCAL virtual void deref() {
        if (ROdereference()) {
            delete this;
        }
    }

    DLLLOCAL virtual const char* getName() const {
        return "InputStreamLineIterator";
    }

    DLLLOCAL virtual const QoreTypeInfo* getElementType() const {
        return stringTypeInfo;
    }

private:
    DLLLOCAL int assignEol(const QoreStringNode* n_eol, ExceptionSink* xsink) {
        if (!n_eol || n_eol->empty()) {
            return 0;
        }
        if (enc != n_eol->getEncoding()) {
            SimpleRefHolder<QoreStringNode> neol(n_eol->convertEncoding(enc, xsink));
            if (*xsink) {
                return -1;
            }
            eol = q_remove_bom_utf16(neol.release(), enc);
        } else {
            eol = n_eol->stringRefSelf();
        }
        return 0;
    }

    DLLLOCAL bool getLine(ExceptionSink* xsink) {
        line = reader->readLine(*eol, trim, xsink);
        return (bool)line;
    }

private:
    ReferenceHolder<InputStream> src;
    ReferenceHolder<StreamReader> reader;
    const QoreEncoding* enc;
    SimpleRefHolder<QoreStringNode> line;
    SimpleRefHolder<QoreStringNode> eol;
    int64 num = 0;
    bool validp = false;
    bool trim;
};

#endif // _QORE_INPUTSTREAMLINEITERATOR_H
