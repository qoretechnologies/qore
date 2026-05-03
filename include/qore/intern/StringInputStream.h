/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    StringInputStream.h

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

#ifndef _QORE_STRINGINPUTSTREAM_H
#define _QORE_STRINGINPUTSTREAM_H

#include <stdint.h>
#include "qore/InputStream.h"

/**
 * @brief Private data for the Qore::StringInputStream class.
 */
class StringInputStream : public InputStream {
public:
    DLLLOCAL StringInputStream(const QoreStringNode *str) : src(str->stringRefSelf()) {
    }

    DLLLOCAL const char *getName() override {
        return "StringInputStream";
    }

    //! StringInputStream is memory-backed and read() never blocks; safe for I/O-thread-driven streaming
    DLLLOCAL bool isIoThreadSafe() const override { return true; }

    DLLLOCAL int64 read(void *ptr, int64 limit, ExceptionSink *xsink) override {
        assert(limit > 0);
        size_t count = src->size() - offset;
        if (count == 0) {
            return 0;
        }
        if (count > static_cast<size_t>(limit)) {
            count = limit;
        }
        memcpy(ptr, src->getBuffer() + offset, count);
        offset += count;
        return count;
    }

    DLLLOCAL int64 peek(ExceptionSink* xsink) override {
        if ((src->size() - offset) == 0) // No more data.
            return -1;
        return src->getBuffer()[offset];
    }

    DLLLOCAL const QoreEncoding* getEncoding() const {
        return src->getEncoding();
    }

private:
    SimpleRefHolder<QoreStringNode> src;
    size_t offset = 0;                          //!< @invariant offset >= 0 && offset <= src->size()
};

#endif // _QORE_STRINGINPUTSTREAM_H
