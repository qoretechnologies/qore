/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    StreamWriter.h

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

#ifndef _QORE_STREAMWRITER_H
#define _QORE_STREAMWRITER_H

#include <cstdint>

#include "qore/qore_bitopts.h"
#include "qore/OutputStream.h"

/**
 * @brief Private data for the Qore::StreamWriter class.
 */
class StreamWriter : public AbstractPrivateData {
public:
    DLLLOCAL StreamWriter(ExceptionSink* xsink, OutputStream* os, const QoreEncoding* enc = QCS_DEFAULT) :
            out(os, xsink),
            encoding(enc) {
    }

    DLLLOCAL const QoreEncoding* getEncoding() const {
        return encoding;
    }

    DLLLOCAL OutputStream* getOutputStream() {
        return *out;
    }

    DLLLOCAL const OutputStream* getOutputStream() const {
        return *out;
    }

    DLLLOCAL int write(const void* ptr, int64 count, ExceptionSink* xsink) {
        // OutputStream uses assertion but we need rather raise exception not to coredump
        if (!out->check(xsink)) {
            return -1;
        }
        out->write(ptr, count, xsink);
        return *xsink ? -1 : 0;
    }

    DLLLOCAL int print(const QoreStringNode* str, ExceptionSink* xsink) {
        TempEncodingHelper stmp(str, encoding, xsink);
        return write(stmp->getBuffer(), stmp->size(), xsink);
    }

    DLLLOCAL int printf(const QoreListNode* args, ExceptionSink* xsink) {
        SimpleRefHolder<QoreStringNode> str(q_sprintf(args, 0, 0, xsink));
        return str ? print(*str, xsink) : 0;
    }

    DLLLOCAL int vprintf(const QoreListNode* args, ExceptionSink* xsink) {
        SimpleRefHolder<QoreStringNode> str(q_vsprintf(args, 0, 0, xsink));
        return str ? print(*str, xsink) : 0;
    }

    DLLLOCAL int f_printf(const QoreListNode* args, ExceptionSink* xsink) {
        SimpleRefHolder<QoreStringNode> str(q_sprintf(args, 1, 0, xsink));
        return str ? print(*str, xsink) : 0;
    }

    DLLLOCAL int f_vprintf(const QoreListNode* args, ExceptionSink* xsink) {
        SimpleRefHolder<QoreStringNode> str(q_vsprintf(args, 1, 0, xsink));
        return str ? print(*str, xsink) : 0;
    }

    DLLLOCAL int write(const BinaryNode* b, ExceptionSink* xsink) {
        write(b->getPtr(), b->size(), xsink);
        return *xsink ? -1 : 0;
    }

    DLLLOCAL int writei1(signed char i, ExceptionSink* xsink) {
        write(&i, 1, xsink);
        return *xsink ? -1 : 0;
    }

    DLLLOCAL int writei2(int16_t i, ExceptionSink* xsink) {
        i = htons(i);
        write(&i, 2, xsink);
        return *xsink ? -1 : 0;
    }

    DLLLOCAL int writei4(int32_t i, ExceptionSink* xsink) {
        i = htonl(i);
        write(&i, 4, xsink);
        return *xsink ? -1 : 0;
    }

    DLLLOCAL int writei8(int64 i, ExceptionSink* xsink) {
        i = i8MSB(i);
        write(&i, 8, xsink);
        return *xsink ? -1 : 0;
    }

    DLLLOCAL int writei2LSB(int16_t i, ExceptionSink* xsink) {
        i = i2LSB(i);
        write(&i, 2, xsink);
        return *xsink ? -1 : 0;
    }

    DLLLOCAL int writei4LSB(int32_t i, ExceptionSink* xsink) {
        i = i4LSB(i);
        write(&i, 4, xsink);
        return *xsink ? -1 : 0;
    }

    DLLLOCAL int writei8LSB(int64 i, ExceptionSink* xsink) {
        i = i8LSB(i);
        write(&i, 8, xsink);
        return *xsink ? -1 : 0;
    }

    DLLLOCAL int writeu1(unsigned char i, ExceptionSink* xsink) {
        return writei1((signed char)i, xsink);
    }

    // the unsigned value is written with the bits of the signed integer of the same width (not of one byte)
    DLLLOCAL int writeu2(uint16_t i, ExceptionSink* xsink) {
        return writei2(static_cast<int16_t>(i), xsink);
    }

    DLLLOCAL int writeu4(uint32_t i, ExceptionSink* xsink) {
        return writei4(static_cast<int32_t>(i), xsink);
    }

    DLLLOCAL virtual const char* getName() const { return "StreamWriter"; }

private:
    ReferenceHolder<OutputStream> out;
    const QoreEncoding* encoding;
};

#endif // _QORE_STREAMWRITER_H
