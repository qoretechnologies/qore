/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file AvroDecoder.cpp Avro binary datum decoder */
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

#include "AvroDecoder.h"
#include "AvroDecimal.h"

#include <qore/ReferenceHolder.h>

#include <cstring>

int AvroDecoder::truncated(ExceptionSink* xsink, size_t need) const {
    xsink->raiseException("AVRO-DECODE-ERROR", "truncated Avro data: %zu more byte%s needed at "
        "offset %zu, but only %zu remain", need, need == 1 ? " is" : "s are", pos, len - pos);
    return -1;
}

int AvroDecoder::checkDepth(unsigned depth, ExceptionSink* xsink) const {
    if (depth <= AVRO_MAX_NESTING_DEPTH) {
        return 0;
    }
    xsink->raiseException("AVRO-DECODE-ERROR", "Avro data nesting exceeds the maximum depth of "
        "%d at offset %zu", AVRO_MAX_NESTING_DEPTH, pos);
    return -1;
}

int AvroDecoder::readRaw(const unsigned char*& p, size_t n, ExceptionSink* xsink) {
    if (n > len - pos) {
        return truncated(xsink, n);
    }
    p = buf + pos;
    pos += n;
    return 0;
}

int AvroDecoder::readLong(int64& v, ExceptionSink* xsink) {
    uint64_t result = 0;
    for (int shift = 0; shift <= 63; shift += 7) {
        if (pos >= len) {
            return truncated(xsink, 1);
        }
        unsigned char b = buf[pos++];
        // at shift 63 only bit 0 still fits, and no continuation byte may follow
        if (shift == 63 && (b & 0xfe)) {
            xsink->raiseException("AVRO-DECODE-ERROR", "variable-length integer at offset %zu "
                "overflows 64 bits", pos - 1);
            return -1;
        }
        result |= (uint64_t)(b & 0x7f) << shift;
        if (!(b & 0x80)) {
            // zigzag decode
            v = (int64)((result >> 1) ^ (~(result & 1) + 1));
            return 0;
        }
    }
    xsink->raiseException("AVRO-DECODE-ERROR", "variable-length integer at offset %zu overflows "
        "64 bits", pos);
    return -1;
}

int AvroDecoder::readBool(bool& v, ExceptionSink* xsink) {
    if (pos >= len) {
        return truncated(xsink, 1);
    }
    unsigned char b = buf[pos++];
    if (b > 1) {
        xsink->raiseException("AVRO-DECODE-ERROR", "invalid boolean value %d at offset %zu; only "
            "0 and 1 are valid", (int)b, pos - 1);
        return -1;
    }
    v = (b != 0);
    return 0;
}

int AvroDecoder::readFloat(double& v, ExceptionSink* xsink) {
    const unsigned char* p;
    if (readRaw(p, 4, xsink)) {
        return -1;
    }
    uint32_t bits = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16)
        | ((uint32_t)p[3] << 24);
    float f;
    memcpy(&f, &bits, 4);
    v = (double)f;
    return 0;
}

int AvroDecoder::readDouble(double& v, ExceptionSink* xsink) {
    const unsigned char* p;
    if (readRaw(p, 8, xsink)) {
        return -1;
    }
    uint64_t bits = 0;
    for (int i = 7; i >= 0; --i) {
        bits = (bits << 8) | (uint64_t)p[i];
    }
    memcpy(&v, &bits, 8);
    return 0;
}

int AvroDecoder::readByteSeq(const unsigned char*& p, int64& n, ExceptionSink* xsink) {
    if (readLong(n, xsink)) {
        return -1;
    }
    if (n < 0) {
        xsink->raiseException("AVRO-DECODE-ERROR", "negative length " QLLD " for a length-prefixed "
            "value at offset %zu", n, pos);
        return -1;
    }
    if ((uint64_t)n > (uint64_t)(len - pos)) {
        return truncated(xsink, (size_t)n);
    }
    p = buf + pos;
    pos += (size_t)n;
    return 0;
}

QoreStringNode* AvroDecoder::readString(ExceptionSink* xsink) {
    const unsigned char* p;
    int64 n;
    if (readByteSeq(p, n, xsink)) {
        return nullptr;
    }
    SimpleRefHolder<QoreStringNode> str(new QoreStringNode(reinterpret_cast<const char*>(p),
        (size_t)n, QCS_UTF8));
    // Avro strings are UTF-8 by definition; a Qore string tagged UTF-8 that is not valid UTF-8
    // corrupts every downstream operation, so reject it here rather than propagate it
    bool invalid = false;
    QCS_UTF8->getLength(str->c_str(), str->c_str() + str->size(), invalid);
    if (invalid) {
        xsink->raiseException("AVRO-DECODE-ERROR", "string value at offset %zu is not valid UTF-8",
            pos - (size_t)n);
        return nullptr;
    }
    return str.release();
}

QoreStringNode* AvroDecoder::readMapKey(ExceptionSink* xsink) {
    SimpleRefHolder<QoreStringNode> key(readString(xsink));
    if (!key) {
        return nullptr;
    }
    // a Qore hash key is NUL-terminated, so a key with an embedded NUL would be silently
    // truncated -- and two distinct keys could then collide and drop an entry
    if (memchr(key->c_str(), 0, key->size())) {
        xsink->raiseException("AVRO-DECODE-ERROR", "map key at offset %zu contains an embedded "
            "NUL byte, which a Qore hash key cannot represent", pos - key->size());
        return nullptr;
    }
    return key.release();
}

BinaryNode* AvroDecoder::readBinary(ExceptionSink* xsink) {
    const unsigned char* p;
    int64 n;
    if (readByteSeq(p, n, xsink)) {
        return nullptr;
    }
    SimpleRefHolder<BinaryNode> b(new BinaryNode);
    b->append(p, (size_t)n);
    return b.release();
}

QoreValue avro_apply_logical_type(const AvroNode* node, QoreValue v, ExceptionSink* xsink) {
    // a uuid is carried as its string (or, over fixed[16], as its raw bytes), so the base value
    // is the result and its reference passes straight through
    if (node->logical == ALT_NONE || node->logical == ALT_UUID) {
        return v;
    }

    // every other logical type builds a new value from the base value, so the base value's
    // reference is consumed here; an int64 large enough to need a heap node would otherwise leak
    ValueHolder holder(v, xsink);

    switch (node->logical) {
        case ALT_NONE:
        case ALT_UUID:
            assert(false);
            return QoreValue();

        case ALT_DECIMAL: {
            const BinaryNode* b = v.get<const BinaryNode>();
            QoreString str;
            if (avro_decimal_to_string(str, static_cast<const unsigned char*>(b->getPtr()),
                    b->size(), node->scale, xsink)) {
                return QoreValue();
            }
            return new QoreNumberNode(str.c_str());
        }

        case ALT_DATE:
            return DateTimeNode::makeAbsolute(nullptr, v.getAsBigInt() * 86400, 0);

        case ALT_TIME_MILLIS:
            return DateTimeNode::makeRelativeFromSeconds(v.getAsBigInt() / 1000,
                (int)((v.getAsBigInt() % 1000) * 1000));

        case ALT_TIME_MICROS:
            return DateTimeNode::makeRelativeFromSeconds(v.getAsBigInt() / 1000000,
                (int)(v.getAsBigInt() % 1000000));

        case ALT_TIMESTAMP_MILLIS:
        case ALT_LOCAL_TIMESTAMP_MILLIS: {
            int64 ms = v.getAsBigInt();
            // C++ truncates toward zero; timestamps before the epoch need floor semantics so the
            // microsecond remainder is never negative
            int64 secs = ms / 1000;
            int64 rem = ms % 1000;
            if (rem < 0) {
                --secs;
                rem += 1000;
            }
            // a local-timestamp is a wall-clock reading with no zone, so the offset is from the
            // epoch *in the local zone*; makeAbsoluteLocal() is what reproduces the written
            // wall-clock time, makeAbsolute() would shift it by the zone's UTC offset
            return node->logical == ALT_TIMESTAMP_MILLIS
                ? DateTimeNode::makeAbsolute(nullptr, secs, (int)(rem * 1000))
                : DateTimeNode::makeAbsoluteLocal(currentTZ(), secs, (int)(rem * 1000));
        }

        case ALT_TIMESTAMP_MICROS:
        case ALT_LOCAL_TIMESTAMP_MICROS: {
            int64 us = v.getAsBigInt();
            int64 secs = us / 1000000;
            int64 rem = us % 1000000;
            if (rem < 0) {
                --secs;
                rem += 1000000;
            }
            return node->logical == ALT_TIMESTAMP_MICROS
                ? DateTimeNode::makeAbsolute(nullptr, secs, (int)rem)
                : DateTimeNode::makeAbsoluteLocal(currentTZ(), secs, (int)rem);
        }

        case ALT_DURATION: {
            const BinaryNode* b = v.get<const BinaryNode>();
            // guaranteed by the parser, which only applies the duration logical type to
            // fixed[12], but enforced here rather than with an assert() that release builds drop
            if (b->size() != 12) {
                xsink->raiseException("AVRO-DECODE-ERROR", "the 'duration' logical type needs "
                    "exactly 12 bytes; %d were read", (int)b->size());
                return QoreValue();
            }
            const unsigned char* p = static_cast<const unsigned char*>(b->getPtr());
            uint32_t part[3];
            for (int i = 0; i < 3; ++i) {
                part[i] = (uint32_t)p[i * 4] | ((uint32_t)p[i * 4 + 1] << 8)
                    | ((uint32_t)p[i * 4 + 2] << 16) | ((uint32_t)p[i * 4 + 3] << 24);
            }
            if (part[0] > (uint32_t)0x7fffffff || part[1] > (uint32_t)0x7fffffff
                || part[2] > (uint32_t)0x7fffffff) {
                xsink->raiseException("AVRO-DECODE-ERROR", "duration component out of range: a "
                    "Qore relative date cannot hold unsigned components above 2^31-1");
                return QoreValue();
            }
            return DateTimeNode::makeRelative(0, (int)part[0], (int)part[1], 0, 0,
                (int)(part[2] / 1000), (int)((part[2] % 1000) * 1000));
        }
    }
    assert(false);
    return QoreValue();
}

QoreValue AvroDecoder::decodeScalar(const AvroNode* node, ExceptionSink* xsink) {
    switch (node->type) {
        case AT_NULL:
            return QoreValue();

        case AT_BOOLEAN: {
            bool b;
            if (readBool(b, xsink)) {
                return QoreValue();
            }
            return b;
        }

        case AT_INT:
        case AT_LONG: {
            int64 v;
            if (readLong(v, xsink)) {
                return QoreValue();
            }
            if (node->type == AT_INT && (v < -2147483648LL || v > 2147483647LL)) {
                xsink->raiseException("AVRO-DECODE-ERROR", "value " QLLD " read for an Avro 'int' "
                    "at offset %zu is outside the 32-bit range", v, pos);
                return QoreValue();
            }
            return avro_apply_logical_type(node, v, xsink);
        }

        case AT_FLOAT:
        case AT_DOUBLE: {
            double d;
            if (node->type == AT_FLOAT ? readFloat(d, xsink) : readDouble(d, xsink)) {
                return QoreValue();
            }
            return d;
        }

        case AT_BYTES: {
            SimpleRefHolder<BinaryNode> b(readBinary(xsink));
            if (!b) {
                return QoreValue();
            }
            return avro_apply_logical_type(node, b.release(), xsink);
        }

        case AT_STRING: {
            SimpleRefHolder<QoreStringNode> str(readString(xsink));
            if (!str) {
                return QoreValue();
            }
            return str.release();
        }

        case AT_FIXED: {
            const unsigned char* p;
            if (readRaw(p, node->fixed_size, xsink)) {
                return QoreValue();
            }
            SimpleRefHolder<BinaryNode> b(new BinaryNode);
            b->append(p, node->fixed_size);
            return avro_apply_logical_type(node, b.release(), xsink);
        }

        case AT_ENUM: {
            int64 idx;
            if (readLong(idx, xsink)) {
                return QoreValue();
            }
            if (idx < 0 || idx >= (int64)node->symbols.size()) {
                xsink->raiseException("AVRO-DECODE-ERROR", "enum index " QLLD " at offset %zu is "
                    "out of range for enum '%s', which has %d symbol%s", idx, pos,
                    node->fullname.c_str(), (int)node->symbols.size(),
                    node->symbols.size() == 1 ? "" : "s");
                return QoreValue();
            }
            return new QoreStringNode(node->symbols[(size_t)idx].c_str(), QCS_UTF8);
        }

        default:
            break;
    }
    assert(false);
    xsink->raiseException("AVRO-DECODE-ERROR", "internal error: '%s' is not a scalar Avro type",
        avro_type_name(node->type));
    return QoreValue();
}

int AvroDecoder::readBlockCount(const AvroNode* elem, int64& count, int64& total,
        ExceptionSink* xsink) {
    if (readLong(count, xsink)) {
        return -1;
    }
    if (!count) {
        return 0;
    }
    if (count < 0) {
        if (count == INT64_MIN) {
            xsink->raiseException("AVRO-DECODE-ERROR", "block count at offset %zu overflows when "
                "negated", pos);
            return -1;
        }
        count = -count;
        // the long block form is followed by the block size in bytes
        int64 block_size;
        if (readLong(block_size, xsink)) {
            return -1;
        }
        if (block_size < 0 || (uint64_t)block_size > (uint64_t)(len - pos)) {
            xsink->raiseException("AVRO-DECODE-ERROR", "block byte size " QLLD " at offset %zu is "
                "negative or exceeds the %zu bytes remaining", block_size, pos, len - pos);
            return -1;
        }
    }

    // A block of N elements of a type that needs at least one byte cannot be valid if N exceeds
    // the remaining input.  Zero-width element types have no such bound, so they get a constant.
    if (elem->zero_width) {
        if (count > AVRO_MAX_ZERO_WIDTH_ELEMENTS || total > AVRO_MAX_ZERO_WIDTH_ELEMENTS - count) {
            xsink->raiseException("AVRO-DECODE-ERROR", "array or map of a zero-width element type "
                "declares more than the maximum of %d elements", AVRO_MAX_ZERO_WIDTH_ELEMENTS);
            return -1;
        }
    } else if ((uint64_t)count > (uint64_t)(len - pos)) {
        xsink->raiseException("AVRO-DECODE-ERROR", "block declares " QLLD " elements at offset "
            "%zu, but only %zu bytes remain and each element of type '%s' needs at least one "
            "byte", count, pos, len - pos, avro_type_name(elem->type));
        return -1;
    }
    total += count;
    return 0;
}

QoreValue AvroDecoder::decodeIntern(const AvroNode* node, unsigned depth, ExceptionSink* xsink) {
    if (checkDepth(depth, xsink)) {
        return QoreValue();
    }

    switch (node->type) {
        case AT_RECORD: {
            ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
            for (const AvroField& f : node->fields) {
                if (checkCancel(xsink)) {
                    return QoreValue();
                }
                ValueHolder v(decodeIntern(f.type, depth + 1, xsink), xsink);
                if (*xsink) {
                    return QoreValue();
                }
                h->setKeyValue(f.name.c_str(), v.release(), xsink);
                if (*xsink) {
                    return QoreValue();
                }
            }
            return h.release();
        }

        case AT_ARRAY: {
            ReferenceHolder<QoreListNode> l(new QoreListNode(autoTypeInfo), xsink);
            int64 count = 0;
            int64 total = 0;
            while (true) {
                if (readBlockCount(node->items, count, total, xsink)) {
                    return QoreValue();
                }
                if (!count) {
                    break;
                }
                for (int64 i = 0; i < count; ++i) {
                    if (checkCancel(xsink)) {
                        return QoreValue();
                    }
                    ValueHolder v(decodeIntern(node->items, depth + 1, xsink), xsink);
                    if (*xsink) {
                        return QoreValue();
                    }
                    l->push(v.release(), xsink);
                    if (*xsink) {
                        return QoreValue();
                    }
                }
            }
            return l.release();
        }

        case AT_MAP: {
            ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
            int64 count = 0;
            int64 total = 0;
            while (true) {
                if (readBlockCount(node->items, count, total, xsink)) {
                    return QoreValue();
                }
                if (!count) {
                    break;
                }
                for (int64 i = 0; i < count; ++i) {
                    if (checkCancel(xsink)) {
                        return QoreValue();
                    }
                    SimpleRefHolder<QoreStringNode> key(readMapKey(xsink));
                    if (!key) {
                        return QoreValue();
                    }
                    ValueHolder v(decodeIntern(node->items, depth + 1, xsink), xsink);
                    if (*xsink) {
                        return QoreValue();
                    }
                    h->setKeyValue(key->c_str(), v.release(), xsink);
                    if (*xsink) {
                        return QoreValue();
                    }
                }
            }
            return h.release();
        }

        case AT_UNION: {
            int64 idx;
            if (readLong(idx, xsink)) {
                return QoreValue();
            }
            if (idx < 0 || idx >= (int64)node->branches.size()) {
                xsink->raiseException("AVRO-DECODE-ERROR", "union branch index " QLLD " at offset "
                    "%zu is out of range; the union has %d branch%s", idx, pos,
                    (int)node->branches.size(), node->branches.size() == 1 ? "" : "es");
                return QoreValue();
            }
            return decodeIntern(node->branches[(size_t)idx], depth + 1, xsink);
        }

        default:
            return decodeScalar(node, xsink);
    }
}

int AvroDecoder::skipValue(const AvroNode* node, unsigned depth, ExceptionSink* xsink) {
    if (checkDepth(depth, xsink)) {
        return -1;
    }

    switch (node->type) {
        case AT_NULL:
            return 0;

        case AT_BOOLEAN: {
            bool b;
            return readBool(b, xsink);
        }

        case AT_INT:
        case AT_LONG:
        case AT_ENUM: {
            int64 v;
            return readLong(v, xsink);
        }

        case AT_FLOAT: {
            const unsigned char* p;
            return readRaw(p, 4, xsink);
        }

        case AT_DOUBLE: {
            const unsigned char* p;
            return readRaw(p, 8, xsink);
        }

        case AT_BYTES:
        case AT_STRING: {
            const unsigned char* p;
            int64 n;
            return readByteSeq(p, n, xsink);
        }

        case AT_FIXED: {
            const unsigned char* p;
            return readRaw(p, node->fixed_size, xsink);
        }

        case AT_RECORD:
            for (const AvroField& f : node->fields) {
                if (checkCancel(xsink) || skipValue(f.type, depth + 1, xsink)) {
                    return -1;
                }
            }
            return 0;

        case AT_ARRAY:
        case AT_MAP: {
            int64 count = 0;
            int64 total = 0;
            while (true) {
                if (readBlockCount(node->items, count, total, xsink)) {
                    return -1;
                }
                if (!count) {
                    return 0;
                }
                for (int64 i = 0; i < count; ++i) {
                    if (checkCancel(xsink)) {
                        return -1;
                    }
                    if (node->type == AT_MAP) {
                        const unsigned char* p;
                        int64 n;
                        if (readByteSeq(p, n, xsink)) {
                            return -1;
                        }
                    }
                    if (skipValue(node->items, depth + 1, xsink)) {
                        return -1;
                    }
                }
            }
        }

        case AT_UNION: {
            int64 idx;
            if (readLong(idx, xsink)) {
                return -1;
            }
            if (idx < 0 || idx >= (int64)node->branches.size()) {
                xsink->raiseException("AVRO-DECODE-ERROR", "union branch index " QLLD " at offset "
                    "%zu is out of range; the union has %d branch%s", idx, pos,
                    (int)node->branches.size(), node->branches.size() == 1 ? "" : "es");
                return -1;
            }
            return skipValue(node->branches[(size_t)idx], depth + 1, xsink);
        }
    }
    assert(false);
    return -1;
}
