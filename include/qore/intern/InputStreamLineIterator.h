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

//! A line iterator that reports the end-of-line marker it removed from its current line
/** A quoted field that continues on the next line is joined with this marker, so the line break in the field is the
    one in the data, also when line ends are detected automatically and any of \c "\n", \c "\r" and \c "\r\n"
    can end a line
*/
class QoreLineTerminatorSource {
public:
    DLLLOCAL virtual ~QoreLineTerminatorSource() = default;

    //! Returns the end-of-line marker removed from the current line in UTF-8
    /** @return the marker; an empty string if the line ended at the end of the data or its marker was not removed
    */
    DLLLOCAL virtual const std::string& getRemovedLineTerminator() const = 0;

protected:
    //! the empty marker
    DLLLOCAL static const std::string& noLineTerminator() {
        static const std::string none;
        return none;
    }
};

/**
 * @brief Private data for the Qore::InputStreamLineIterator class.
 */
class InputStreamLineIterator : public QoreIteratorBase, public QoreLineTerminatorSource {
public:
    DLLLOCAL InputStreamLineIterator(ExceptionSink* xsink, InputStream* is, const QoreEncoding* encoding,
            const QoreStringNode* n_eol, bool n_trim, size_t bufsize = DefaultStreamBufferSize,
            qore_encoding_errors_t encoding_errors = qore_encoding_errors_t::RAISE) :
        src(is, xsink),
        reader(xsink),
        enc(encoding),
        trim(n_trim) {
        if (assignEol(n_eol, xsink))
            return;

        // reference src for assignment to BufferedStreamReader
        src->ref();
        reader = new BufferedStreamReader(xsink, *src, enc, bufsize, encoding_errors);
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
        uint64_t consumed = reader->getConsumed();
        validp = getLine(xsink);
        if (validp) {
            ++num;   // Increment line number.
        } else if (*xsink) {
            // a line whose bytes were read, all or some, and that raised an exception is counted, so that the lines
            // after it keep their numbers: a line that is not valid in the encoding (INVALID-ENCODING), and a line
            // that the stream raised an exception in the middle of (its bytes read before the exception are lost);
            // an exception raised before any byte of a line was read (ex: by the stream) does not count a line
            if (reader->getConsumed() != consumed) {
                ++num;
            }
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

    DLLLOCAL const std::string& getRemovedLineTerminator() const override {
        return validp && trim ? reader->getLineTerminator() : noLineTerminator();
    }

    DLLLOCAL const QoreEncoding* getEncoding() const {
        return enc;
    }

    //! returns what a line that is not valid in the encoding gives (the encoding_errors option of the reader)
    DLLLOCAL qore_encoding_errors_t getEncodingErrors() const {
        return reader->getEncodingErrors();
    }

    //! sets the class name given in the exception raised for a line that is not valid in the encoding
    DLLLOCAL void setOwnerName(const char* name) {
        owner_name = name;
    }

    DLLLOCAL virtual void deref() override {
        if (ROdereference()) {
            delete this;
        }
    }

    DLLLOCAL virtual const char* getName() const override {
        return "InputStreamLineIterator";
    }

    DLLLOCAL virtual const QoreTypeInfo* getElementType() const override {
        return stringTypeInfo;
    }

    // Native fast-path: same shape as DataLineIterator/FileLineIterator —
    // next(xsink) reads the next line (may block on stream I/O), getValue()
    // requires checkValid() before yielding.
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
    DLLLOCAL int assignEol(const QoreStringNode* n_eol, ExceptionSink* xsink) {
        if (!n_eol || n_eol->empty()) {
            return 0;
        }
        if (enc != n_eol->getEncoding()) {
            SimpleRefHolder<QoreStringNode> neol(n_eol->convertEncoding(enc, xsink));
            if (*xsink) {
                return -1;
            }
            eol = q_remove_bom(neol.release(), enc);
        } else {
            eol = n_eol->stringRefSelf();
        }
        return 0;
    }

    DLLLOCAL bool getLine(ExceptionSink* xsink) {
        // a line that is not valid in the encoding is reported with its line number
        line = reader->readLine(*eol, trim, xsink, owner_name, "next", num + 1);
        return (bool)line;
    }

private:
    ReferenceHolder<InputStream> src;
    ReferenceHolder<StreamReader> reader;
    const QoreEncoding* enc;
    SimpleRefHolder<QoreStringNode> line;
    SimpleRefHolder<QoreStringNode> eol;
    int64 num = 0;
    //! the class name given in the exception raised for a line that is not valid in the encoding
    const char* owner_name = "InputStreamLineIterator";
    bool validp = false;
    bool trim;
};

#endif // _QORE_INPUTSTREAMLINEITERATOR_H
