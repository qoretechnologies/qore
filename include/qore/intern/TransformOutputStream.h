/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    TransformOutputStream.h

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

#ifndef _QORE_TRANSFORMOUTPUTSTREAM_H
#define _QORE_TRANSFORMOUTPUTSTREAM_H

#include "qore/OutputStream.h"
#include "qore/Transform.h"

class TransformOutputStream : public OutputStream {
public:
    DLLLOCAL TransformOutputStream(OutputStream* os, Transform* t)
            : os(os), t(t), bufsize(t->outputBufferSize()), closed(false) {
    }

    DLLLOCAL const char* getName() override {
        return "TransformOutputStream";
    }

    DLLLOCAL bool isIoThreadSafe() const override {
        return os->isIoThreadSafe() && !os->supportsNonBlockingIo();
    }

    DLLLOCAL bool isClosed() override {
        return closed;
    }

    DLLLOCAL void close(ExceptionSink* xsink) override {
        char buf[bufsize];
        while (true) {
            std::pair<int64, int64> r = t->apply(nullptr, 0, buf, sizeof(buf), xsink);
            if (*xsink) {
                break;
            }
            if (!r.second) {
                break;
            }
            os->write(buf, r.second, xsink);
            if (*xsink) {
                break;
            }
        }
        closed = true;
    }

    DLLLOCAL void write(const void* ptr, int64 len, ExceptionSink* xsink) override {
        assert(len >= 0);
        const char *src = static_cast<const char*>(ptr);
        char buf[bufsize];
        while (len > 0) {
            std::pair<int64, int64> r = t->apply(src, len, buf, sizeof(buf), xsink);
            if (*xsink) {
                return;
            }
            if (r.second) {
                os->write(buf, r.second, xsink);
                if (*xsink) {
                    return;
                }
            }
            src += r.first;
            len -= r.first;
        }
    }

private:
    SimpleRefHolder<OutputStream> os;
    SimpleRefHolder<Transform> t;
    size_t bufsize;
    bool closed;
};

#endif // _QORE_TRANSFORMOUTPUTSTREAM_H
