/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file AvroEncoder.h Avro binary datum encoder */
/*
    Qore avro module

    Copyright (C) 2026 Qore Technologies, s.r.o.

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
*/

#ifndef _QORE_AVRO_AVROENCODER_H
#define _QORE_AVRO_AVROENCODER_H

#include "AvroSchema.h"

#include <vector>

//! encodes Qore values to Avro binary data against a schema
class AvroEncoder {
public:
    DLLLOCAL AvroEncoder() {
    }

    //! encodes one datum; returns 0 for OK, -1 if an exception was raised
    DLLLOCAL int encode(const AvroNode* node, QoreValue v, ExceptionSink* xsink) {
        return encodeIntern(node, v, 0, xsink);
    }

    //! returns the encoded bytes; the caller owns the reference returned
    DLLLOCAL BinaryNode* takeBinary();

    DLLLOCAL const std::vector<unsigned char>& getBuffer() const {
        return buf;
    }

    DLLLOCAL size_t size() const {
        return buf.size();
    }

    DLLLOCAL void clear() {
        buf.clear();
    }

    //! appends a zigzag varint-encoded integer
    DLLLOCAL void writeLong(int64 v);

    //! appends raw bytes
    DLLLOCAL void writeRaw(const void* p, size_t len) {
        const unsigned char* b = (const unsigned char*)p;
        buf.insert(buf.end(), b, b + len);
    }

    //! appends a length-prefixed byte sequence
    DLLLOCAL void writeByteSeq(const void* p, size_t len) {
        writeLong((int64)len);
        writeRaw(p, len);
    }

private:
    std::vector<unsigned char> buf;
    //! iteration counter for periodic cooperative-cancellation checks
    unsigned iteration = 0;

    DLLLOCAL int encodeIntern(const AvroNode* node, QoreValue v, unsigned depth,
            ExceptionSink* xsink);
    DLLLOCAL int encodeScalar(const AvroNode* node, QoreValue v, unsigned depth,
            ExceptionSink* xsink);
    DLLLOCAL int encodeDecimal(const AvroNode* node, QoreValue v, ExceptionSink* xsink);
    DLLLOCAL int encodeDuration(const AvroNode* node, QoreValue v, ExceptionSink* xsink);

    //! returns the index of the union branch to encode \a v with, or -1 if none matches
    DLLLOCAL int selectUnionBranch(const AvroNode* node, QoreValue v);

    DLLLOCAL void writeFloat(double d);
    DLLLOCAL void writeDouble(double d);

    DLLLOCAL int typeError(const AvroNode* node, QoreValue v, ExceptionSink* xsink);

    DLLLOCAL int checkCancel(ExceptionSink* xsink) {
        if ((++iteration % AVRO_INTERRUPT_CHECK_INTERVAL)) {
            return 0;
        }
        return qore_check_cancel(xsink, "encoding Avro data") ? -1 : 0;
    }
};

#endif // _QORE_AVRO_AVROENCODER_H
