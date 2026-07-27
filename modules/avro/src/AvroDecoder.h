/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file AvroDecoder.h Avro binary datum decoder */
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

#ifndef _QORE_AVRO_AVRODECODER_H
#define _QORE_AVRO_AVRODECODER_H

#include "AvroSchema.h"

//! the largest number of elements accepted in an array or map of a zero-width element type
/** For any element type that needs at least one byte the decoder bounds the element count by the
    remaining input, which is a tighter and self-adjusting limit; only zero-width element types
    (`null`, `fixed` of size 0, records of those) need a constant.
*/
#define AVRO_MAX_ZERO_WIDTH_ELEMENTS 1000000

//! decodes Avro binary data against a schema
/** The decoder holds a borrowed view of the input buffer, which must outlive it.
*/
class AvroDecoder {
public:
    DLLLOCAL AvroDecoder(const unsigned char* buf, size_t len) : buf(buf), len(len) {
    }

    //! decodes one datum written with \a node
    DLLLOCAL QoreValue decode(const AvroNode* node, ExceptionSink* xsink) {
        return decodeIntern(node, 0, xsink);
    }

    //! decodes one datum written with \a writer and projects it onto the reader schema \a reader
    DLLLOCAL QoreValue decodeResolved(const AvroNode* writer, const AvroNode* reader,
            ExceptionSink* xsink) {
        return resolveIntern(writer, reader, 0, xsink);
    }

    //! advances past one datum written with \a node without materialising a value
    DLLLOCAL int skipValue(const AvroNode* node, unsigned depth, ExceptionSink* xsink);

    DLLLOCAL size_t getPos() const {
        return pos;
    }

    DLLLOCAL size_t remaining() const {
        return len - pos;
    }

    DLLLOCAL bool atEnd() const {
        return pos >= len;
    }

    //! reads a zigzag varint-encoded integer
    DLLLOCAL int readLong(int64& v, ExceptionSink* xsink);

    //! reads \a n raw bytes, returning a pointer into the input buffer
    DLLLOCAL int readRaw(const unsigned char*& p, size_t n, ExceptionSink* xsink);

    //! reads a length-prefixed byte sequence, returning a pointer into the input buffer
    DLLLOCAL int readByteSeq(const unsigned char*& p, int64& n, ExceptionSink* xsink);

    //! raises AVRO-DECODE-ERROR reporting a truncated input
    DLLLOCAL int truncated(ExceptionSink* xsink, size_t need) const;

    //! reads a map key; rejects an embedded NUL, which a %Qore hash key cannot represent
    DLLLOCAL QoreStringNode* readMapKey(ExceptionSink* xsink);

private:
    const unsigned char* buf;
    size_t len;
    size_t pos = 0;
    //! iteration counter for periodic cooperative-cancellation checks
    unsigned iteration = 0;

    DLLLOCAL QoreValue decodeIntern(const AvroNode* node, unsigned depth, ExceptionSink* xsink);
    DLLLOCAL QoreValue resolveIntern(const AvroNode* writer, const AvroNode* reader, unsigned depth,
            ExceptionSink* xsink);

    DLLLOCAL int readBool(bool& v, ExceptionSink* xsink);
    DLLLOCAL int readFloat(double& v, ExceptionSink* xsink);
    DLLLOCAL int readDouble(double& v, ExceptionSink* xsink);
    DLLLOCAL QoreStringNode* readString(ExceptionSink* xsink);
    DLLLOCAL BinaryNode* readBinary(ExceptionSink* xsink);

    //! decodes a primitive or logical-type value; \a node must not be a container type
    DLLLOCAL QoreValue decodeScalar(const AvroNode* node, ExceptionSink* xsink);

    //! reads the count of the next array/map block; sets \a count to 0 at the end of the container
    /** Validates the count against the remaining input and against the block byte size when the
        long-block form is used.
    */
    DLLLOCAL int readBlockCount(const AvroNode* elem, int64& count, int64& total,
            ExceptionSink* xsink);

    DLLLOCAL int checkDepth(unsigned depth, ExceptionSink* xsink) const;

    DLLLOCAL int checkCancel(ExceptionSink* xsink) {
        if ((++iteration % AVRO_INTERRUPT_CHECK_INTERVAL)) {
            return 0;
        }
        return qore_check_cancel(xsink, "decoding Avro data") ? -1 : 0;
    }
};

//! converts a scalar Avro value to the Qore representation of \a node's logical type
/** Used by both the plain and the resolved decode paths.

    @param node the schema node the value was read against
    @param v the base-type value: an int64 for int/long, a double for float/double, a
        QoreStringNode for string, or a BinaryNode for bytes/fixed; the reference is consumed
        for the two node types
*/
DLLLOCAL QoreValue avro_apply_logical_type(const AvroNode* node, QoreValue v, ExceptionSink* xsink);

//! returns true if a datum written with \a writer can be read with the reader schema \a reader
DLLLOCAL bool avro_schemas_match(const AvroNode* writer, const AvroNode* reader);

#endif // _QORE_AVRO_AVRODECODER_H
