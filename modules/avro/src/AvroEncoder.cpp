/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file AvroEncoder.cpp Avro binary datum encoder */
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

#include "AvroEncoder.h"
#include "AvroDecimal.h"

#include <qore/ReferenceHolder.h>

#include <cstring>

BinaryNode* AvroEncoder::takeBinary() {
    SimpleRefHolder<BinaryNode> b(new BinaryNode);
    if (!buf.empty()) {
        b->append(&buf[0], buf.size());
    }
    return b.release();
}

void AvroEncoder::writeLong(int64 v) {
    // zigzag encode, then base-128 varint
    uint64_t u = ((uint64_t)v << 1) ^ (uint64_t)(v >> 63);
    while (u & ~(uint64_t)0x7f) {
        buf.push_back((unsigned char)((u & 0x7f) | 0x80));
        u >>= 7;
    }
    buf.push_back((unsigned char)u);
}

void AvroEncoder::writeFloat(double d) {
    float f = (float)d;
    uint32_t bits;
    memcpy(&bits, &f, 4);
    for (int i = 0; i < 4; ++i) {
        buf.push_back((unsigned char)((bits >> (i * 8)) & 0xff));
    }
}

void AvroEncoder::writeDouble(double d) {
    uint64_t bits;
    memcpy(&bits, &d, 8);
    for (int i = 0; i < 8; ++i) {
        buf.push_back((unsigned char)((bits >> (i * 8)) & 0xff));
    }
}

int AvroEncoder::typeError(const AvroNode* node, QoreValue v, ExceptionSink* xsink) {
    QoreStringMaker desc("cannot encode a value of Qore type '%s' as Avro type '%s'",
        v.getTypeName(), avro_type_name(node->type));
    if (node->logical != ALT_NONE) {
        desc.sprintf(" with logical type '%s'", avro_logical_type_name(node->logical));
    }
    if (!node->fullname.empty()) {
        desc.sprintf(" ('%s')", node->fullname.c_str());
    }
    xsink->raiseException("AVRO-ENCODE-ERROR", "%s", desc.c_str());
    return -1;
}

int AvroEncoder::encodeDecimal(const AvroNode* node, QoreValue v, ExceptionSink* xsink) {
    QoreStringMaker what("encoding a decimal(%d, %d) value", node->precision, node->scale);

    // route every input through an exact decimal string; see design/avro-module.md
    QoreString str;
    switch (v.getType()) {
        case NT_NUMBER:
        case NT_INT:
        case NT_FLOAT:
            if (v.getAsString(str, FMT_NONE, xsink)) {
                return -1;
            }
            break;
        case NT_STRING:
            // note: the value can be held in inline short string storage, which has no QoreStringNode
            str.concat(QoreStringDataHelper(v).c_str());
            break;
        default:
            return typeError(node, v, xsink);
    }

    std::vector<unsigned char> unscaled;
    if (avro_decimal_from_string(unscaled, str.c_str(), node->precision, node->scale, what.c_str(),
            xsink)) {
        return -1;
    }
    if (node->type == AT_FIXED) {
        if (avro_decimal_sign_extend(unscaled, node->fixed_size, what.c_str(), xsink)) {
            return -1;
        }
        writeRaw(&unscaled[0], unscaled.size());
    } else {
        writeByteSeq(&unscaled[0], unscaled.size());
    }
    return 0;
}

int AvroEncoder::encodeDuration(const AvroNode* node, QoreValue v, ExceptionSink* xsink) {
    if (v.getType() != NT_DATE) {
        return typeError(node, v, xsink);
    }
    const DateTimeNode* dt = v.get<const DateTimeNode>();
    if (!dt->isRelative()) {
        xsink->raiseException("AVRO-ENCODE-ERROR", "the Avro 'duration' logical type needs a "
            "relative date (a duration); an absolute date was supplied");
        return -1;
    }
    int64 months = (int64)dt->getYear() * 12 + dt->getMonth();
    int64 days = dt->getDay();
    int64 millis = ((int64)dt->getHour() * 3600 + (int64)dt->getMinute() * 60 + dt->getSecond())
        * 1000 + dt->getMillisecond();
    if (months < 0 || days < 0 || millis < 0 || months > 0xffffffffLL || days > 0xffffffffLL
        || millis > 0xffffffffLL) {
        xsink->raiseException("AVRO-ENCODE-ERROR", "the Avro 'duration' logical type holds three "
            "unsigned 32-bit components; this duration has %d month(s), %d day(s) and " QLLD
            " millisecond(s)", (int)months, (int)days, millis);
        return -1;
    }
    if (dt->getMicrosecond() % 1000) {
        xsink->raiseException("AVRO-ENCODE-ERROR", "the Avro 'duration' logical type has "
            "millisecond resolution; this duration has %d microsecond(s), which cannot be "
            "represented exactly", dt->getMicrosecond());
        return -1;
    }
    uint32_t part[3] = {(uint32_t)months, (uint32_t)days, (uint32_t)millis};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 4; ++j) {
            buf.push_back((unsigned char)((part[i] >> (j * 8)) & 0xff));
        }
    }
    return 0;
}

//! returns the integer the given date represents under \a node's temporal logical type
static int avro_temporal_value(const AvroNode* node, const DateTimeNode* dt, int64& out,
        ExceptionSink* xsink) {
    bool want_relative = node->logical == ALT_TIME_MILLIS || node->logical == ALT_TIME_MICROS;
    if (dt->isRelative() != want_relative) {
        xsink->raiseException("AVRO-ENCODE-ERROR", "the Avro '%s' logical type needs %s date; %s "
            "date was supplied", avro_logical_type_name(node->logical),
            want_relative ? "a relative (duration)" : "an absolute",
            dt->isRelative() ? "a relative" : "an absolute");
        return -1;
    }

    switch (node->logical) {
        case ALT_DATE: {
            int64 secs = dt->getEpochSecondsUTC();
            // floor division: days before the epoch must round down, not toward zero
            out = secs >= 0 ? secs / 86400 : -((-secs + 86399) / 86400);
            return 0;
        }
        case ALT_TIME_MILLIS: {
            int64 us = dt->getRelativeMicroseconds();
            if (us % 1000) {
                xsink->raiseException("AVRO-ENCODE-ERROR", "the Avro 'time-millis' logical type "
                    "has millisecond resolution; the value has " QLLD " microsecond(s), which "
                    "cannot be represented exactly", us % 1000);
                return -1;
            }
            out = us / 1000;
            return 0;
        }
        case ALT_TIME_MICROS:
            out = dt->getRelativeMicroseconds();
            return 0;
        case ALT_TIMESTAMP_MILLIS: {
            int64 us = dt->getEpochMicrosecondsUTC();
            if (us % 1000) {
                xsink->raiseException("AVRO-ENCODE-ERROR", "the Avro 'timestamp-millis' logical "
                    "type has millisecond resolution; the value has " QLLD " microsecond(s), "
                    "which cannot be represented exactly", us % 1000);
                return -1;
            }
            out = us / 1000;
            return 0;
        }
        case ALT_TIMESTAMP_MICROS:
            out = dt->getEpochMicrosecondsUTC();
            return 0;
        case ALT_LOCAL_TIMESTAMP_MILLIS: {
            if (dt->getMicrosecond() % 1000) {
                xsink->raiseException("AVRO-ENCODE-ERROR", "the Avro 'local-timestamp-millis' "
                    "logical type has millisecond resolution; the value has %d microsecond(s), "
                    "which cannot be represented exactly", dt->getMicrosecond());
                return -1;
            }
            // a local timestamp is the wall-clock reading as an offset from the epoch in the
            // value's own zone
            out = dt->getEpochSeconds() * 1000 + dt->getMillisecond();
            return 0;
        }
        case ALT_LOCAL_TIMESTAMP_MICROS:
            out = dt->getEpochSeconds() * 1000000 + dt->getMicrosecond();
            return 0;
        default:
            break;
    }
    assert(false);
    return -1;
}

int AvroEncoder::encodeScalar(const AvroNode* node, QoreValue v, unsigned depth,
        ExceptionSink* xsink) {
    switch (node->type) {
        case AT_NULL:
            if (!v.isNullOrNothing()) {
                return typeError(node, v, xsink);
            }
            return 0;

        case AT_BOOLEAN:
            if (v.getType() != NT_BOOLEAN) {
                return typeError(node, v, xsink);
            }
            buf.push_back(v.getAsBool() ? 1 : 0);
            return 0;

        case AT_INT:
        case AT_LONG: {
            int64 iv;
            if (node->logical != ALT_NONE && v.getType() == NT_DATE) {
                if (avro_temporal_value(node, v.get<const DateTimeNode>(), iv, xsink)) {
                    return -1;
                }
            } else if (v.getType() == NT_INT) {
                iv = v.getAsBigInt();
            } else {
                return typeError(node, v, xsink);
            }
            if (node->type == AT_INT && (iv < -2147483648LL || iv > 2147483647LL)) {
                xsink->raiseException("AVRO-ENCODE-ERROR", "value " QLLD " is outside the 32-bit "
                    "range of the Avro 'int' type", iv);
                return -1;
            }
            writeLong(iv);
            return 0;
        }

        case AT_FLOAT:
        case AT_DOUBLE: {
            if (v.getType() != NT_FLOAT && v.getType() != NT_INT && v.getType() != NT_NUMBER) {
                return typeError(node, v, xsink);
            }
            double d = v.getAsFloat();
            if (node->type == AT_FLOAT) {
                writeFloat(d);
            } else {
                writeDouble(d);
            }
            return 0;
        }

        case AT_BYTES: {
            if (node->logical == ALT_DECIMAL) {
                return encodeDecimal(node, v, xsink);
            }
            if (v.getType() == NT_BINARY) {
                const BinaryNode* b = v.get<const BinaryNode>();
                writeByteSeq(b->getPtr(), b->size());
                return 0;
            }
            if (v.getType() == NT_STRING) {
                // note: the value can be held in inline short string storage, which has no
                // QoreStringNode, so the data helper must be used to read the bytes
                QoreStringDataHelper str(v);
                writeByteSeq(str.c_str(), str.size());
                return 0;
            }
            return typeError(node, v, xsink);
        }

        case AT_STRING: {
            if (v.getType() != NT_STRING) {
                return typeError(node, v, xsink);
            }
            // note: the value can be held in inline short string storage, which has no
            // QoreStringNode; the node value helper materializes such values
            QoreStringNodeValueHelper str(v);
            TempEncodingHelper utf8(*str, QCS_UTF8, xsink);
            if (*xsink) {
                return -1;
            }
            writeByteSeq(utf8->c_str(), utf8->size());
            return 0;
        }

        case AT_FIXED: {
            if (node->logical == ALT_DECIMAL) {
                return encodeDecimal(node, v, xsink);
            }
            if (node->logical == ALT_DURATION) {
                return encodeDuration(node, v, xsink);
            }
            if (v.getType() != NT_BINARY) {
                return typeError(node, v, xsink);
            }
            const BinaryNode* b = v.get<const BinaryNode>();
            if (b->size() != node->fixed_size) {
                xsink->raiseException("AVRO-ENCODE-ERROR", "fixed type '%s' requires exactly %u "
                    "byte%s, but %d were supplied", node->fullname.c_str(), node->fixed_size,
                    node->fixed_size == 1 ? "" : "s", (int)b->size());
                return -1;
            }
            writeRaw(b->getPtr(), b->size());
            return 0;
        }

        case AT_ENUM: {
            int idx = -1;
            if (v.getType() == NT_STRING) {
                // note: the value can be held in inline short string storage, which has no
                // QoreStringNode, so the data helper must be used to read the bytes
                QoreStringDataHelper sym(v);
                idx = node->findSymbol(sym.c_str());
                if (idx < 0) {
                    xsink->raiseException("AVRO-ENCODE-ERROR", "'%s' is not a symbol of enum "
                        "'%s'", sym.c_str(), node->fullname.c_str());
                    return -1;
                }
            } else if (v.getType() == NT_INT) {
                int64 iv = v.getAsBigInt();
                if (iv < 0 || iv >= (int64)node->symbols.size()) {
                    xsink->raiseException("AVRO-ENCODE-ERROR", "symbol index " QLLD " is out of "
                        "range for enum '%s', which has %d symbol%s", iv, node->fullname.c_str(),
                        (int)node->symbols.size(), node->symbols.size() == 1 ? "" : "s");
                    return -1;
                }
                idx = (int)iv;
            } else {
                return typeError(node, v, xsink);
            }
            writeLong(idx);
            return 0;
        }

        default:
            break;
    }
    assert(false);
    xsink->raiseException("AVRO-ENCODE-ERROR", "internal error: '%s' is not a scalar Avro type",
        avro_type_name(node->type));
    return -1;
}

//! returns true if \a v could plausibly be encoded as \a node without loss
/** Used only for union branch selection; the encoder still validates the value once a branch has
    been chosen, so a false positive here becomes a clear AVRO-ENCODE-ERROR rather than bad data.
*/
static bool avro_branch_accepts(const AvroNode* node, QoreValue v) {
    switch (v.getType()) {
        case NT_NOTHING:
        case NT_NULL:
            return node->type == AT_NULL;

        case NT_BOOLEAN:
            return node->type == AT_BOOLEAN;

        case NT_INT: {
            if (node->logical != ALT_NONE) {
                return node->logical == ALT_DECIMAL;
            }
            int64 iv = v.getAsBigInt();
            switch (node->type) {
                case AT_INT: return iv >= -2147483648LL && iv <= 2147483647LL;
                case AT_LONG:
                case AT_FLOAT:
                case AT_DOUBLE: return true;
                default: return false;
            }
        }

        case NT_FLOAT:
            if (node->logical != ALT_NONE) {
                return node->logical == ALT_DECIMAL;
            }
            return node->type == AT_FLOAT || node->type == AT_DOUBLE;

        case NT_NUMBER:
            if (node->logical == ALT_DECIMAL) {
                return true;
            }
            return node->logical == ALT_NONE
                && (node->type == AT_DOUBLE || node->type == AT_FLOAT);

        case NT_STRING: {
            if (node->type == AT_ENUM) {
                // note: the value can be held in inline short string storage, which has no
                // QoreStringNode
                return node->findSymbol(QoreStringDataHelper(v).c_str()) >= 0;
            }
            if (node->type == AT_STRING) {
                return true;
            }
            if (node->type == AT_BYTES) {
                return node->logical != ALT_DECIMAL;
            }
            return false;
        }

        case NT_BINARY: {
            size_t sz = v.get<const BinaryNode>()->size();
            if (node->type == AT_BYTES) {
                return node->logical != ALT_DECIMAL;
            }
            return node->type == AT_FIXED && node->logical != ALT_DECIMAL
                && node->logical != ALT_DURATION && sz == node->fixed_size;
        }

        case NT_DATE: {
            bool relative = v.get<const DateTimeNode>()->isRelative();
            if (relative) {
                return node->logical == ALT_DURATION || node->logical == ALT_TIME_MICROS
                    || node->logical == ALT_TIME_MILLIS;
            }
            return node->logical == ALT_TIMESTAMP_MICROS
                || node->logical == ALT_TIMESTAMP_MILLIS
                || node->logical == ALT_LOCAL_TIMESTAMP_MICROS
                || node->logical == ALT_LOCAL_TIMESTAMP_MILLIS
                || node->logical == ALT_DATE;
        }

        case NT_LIST:
            return node->type == AT_ARRAY;

        case NT_HASH:
            return node->type == AT_RECORD || node->type == AT_MAP;

        default:
            return false;
    }
}

//! scores how well \a h matches record \a node: lower is better, -1 if it cannot match at all
static int avro_record_match_score(const AvroNode* node, const QoreHashNode* h) {
    int score = 0;
    for (const AvroField& f : node->fields) {
        if (!h->existsKey(f.name.c_str())) {
            if (!f.has_default) {
                return -1;
            }
            ++score;
        }
    }
    // keys the record does not declare count against the match as well
    ConstHashIterator hi(h);
    while (hi.next()) {
        if (node->findField(hi.getKey()) < 0) {
            ++score;
        }
    }
    return score;
}

int AvroEncoder::selectUnionBranch(const AvroNode* node, QoreValue v) {
    // the preference order is documented in design/avro-module.md and in the module docs
    static const AvroLogicalType date_pref[] = {
        ALT_TIMESTAMP_MICROS, ALT_TIMESTAMP_MILLIS, ALT_LOCAL_TIMESTAMP_MICROS,
        ALT_LOCAL_TIMESTAMP_MILLIS, ALT_DATE,
    };
    static const AvroLogicalType reltime_pref[] = {
        ALT_DURATION, ALT_TIME_MICROS, ALT_TIME_MILLIS,
    };
    static const AvroType int_pref[] = {AT_INT, AT_LONG, AT_FLOAT, AT_DOUBLE};
    static const AvroType float_pref[] = {AT_DOUBLE, AT_FLOAT};
    static const AvroType string_pref[] = {AT_ENUM, AT_STRING, AT_BYTES};
    static const AvroType binary_pref[] = {AT_BYTES, AT_FIXED};

    const AvroType* type_pref = nullptr;
    size_t type_pref_len = 0;
    const AvroLogicalType* logical_pref = nullptr;
    size_t logical_pref_len = 0;

    switch (v.getType()) {
        case NT_INT:
            type_pref = int_pref;
            type_pref_len = sizeof(int_pref) / sizeof(*int_pref);
            break;
        case NT_FLOAT:
        case NT_NUMBER:
            type_pref = float_pref;
            type_pref_len = sizeof(float_pref) / sizeof(*float_pref);
            break;
        case NT_STRING:
            type_pref = string_pref;
            type_pref_len = sizeof(string_pref) / sizeof(*string_pref);
            break;
        case NT_BINARY:
            type_pref = binary_pref;
            type_pref_len = sizeof(binary_pref) / sizeof(*binary_pref);
            break;
        case NT_DATE:
            if (v.get<const DateTimeNode>()->isRelative()) {
                logical_pref = reltime_pref;
                logical_pref_len = sizeof(reltime_pref) / sizeof(*reltime_pref);
            } else {
                logical_pref = date_pref;
                logical_pref_len = sizeof(date_pref) / sizeof(*date_pref);
            }
            break;
        default:
            break;
    }

    // a decimal branch is preferred over any plain numeric branch for int, float and number
    if (v.getType() == NT_INT || v.getType() == NT_FLOAT || v.getType() == NT_NUMBER) {
        for (size_t i = 0; i < node->branches.size(); ++i) {
            if (node->branches[i]->logical == ALT_DECIMAL && avro_branch_accepts(node->branches[i], v)) {
                return (int)i;
            }
        }
    }

    if (logical_pref) {
        for (size_t p = 0; p < logical_pref_len; ++p) {
            for (size_t i = 0; i < node->branches.size(); ++i) {
                if (node->branches[i]->logical == logical_pref[p]
                    && avro_branch_accepts(node->branches[i], v)) {
                    return (int)i;
                }
            }
        }
        // a bare long branch can still carry a temporal value only if the caller gave an int, so
        // no further fallback is possible for a date
        return -1;
    }

    if (v.getType() == NT_HASH) {
        // among record branches, prefer the one the hash satisfies with the fewest unmatched keys
        const QoreHashNode* h = v.get<const QoreHashNode>();
        int best = -1;
        int best_score = -1;
        for (size_t i = 0; i < node->branches.size(); ++i) {
            if (node->branches[i]->type != AT_RECORD) {
                continue;
            }
            int score = avro_record_match_score(node->branches[i], h);
            if (score >= 0 && (best < 0 || score < best_score)) {
                best = (int)i;
                best_score = score;
            }
        }
        if (best >= 0) {
            return best;
        }
        for (size_t i = 0; i < node->branches.size(); ++i) {
            if (node->branches[i]->type == AT_MAP) {
                return (int)i;
            }
        }
        return -1;
    }

    if (type_pref) {
        for (size_t p = 0; p < type_pref_len; ++p) {
            for (size_t i = 0; i < node->branches.size(); ++i) {
                if (node->branches[i]->type == type_pref[p]
                    && avro_branch_accepts(node->branches[i], v)) {
                    return (int)i;
                }
            }
        }
        return -1;
    }

    for (size_t i = 0; i < node->branches.size(); ++i) {
        if (avro_branch_accepts(node->branches[i], v)) {
            return (int)i;
        }
    }
    return -1;
}

int AvroEncoder::encodeIntern(const AvroNode* node, QoreValue v, unsigned depth,
        ExceptionSink* xsink) {
    if (depth > AVRO_MAX_NESTING_DEPTH) {
        xsink->raiseException("AVRO-ENCODE-ERROR", "value nesting exceeds the maximum depth of %d",
            AVRO_MAX_NESTING_DEPTH);
        return -1;
    }

    switch (node->type) {
        case AT_RECORD: {
            if (v.getType() != NT_HASH) {
                return typeError(node, v, xsink);
            }
            const QoreHashNode* h = v.get<const QoreHashNode>();
            for (const AvroField& f : node->fields) {
                if (checkCancel(xsink)) {
                    return -1;
                }
                QoreValue fv;
                if (h->existsKey(f.name.c_str())) {
                    fv = h->getKeyValue(f.name.c_str());
                } else if (f.has_default) {
                    fv = f.default_value;
                } else {
                    xsink->raiseException("AVRO-ENCODE-ERROR", "field '%s' of record '%s' is not "
                        "present in the hash to encode and has no default value", f.name.c_str(),
                        node->fullname.c_str());
                    return -1;
                }
                if (encodeIntern(f.type, fv, depth + 1, xsink)) {
                    return -1;
                }
            }
            return 0;
        }

        case AT_ARRAY: {
            if (v.getType() != NT_LIST) {
                return typeError(node, v, xsink);
            }
            const QoreListNode* l = v.get<const QoreListNode>();
            if (!l->empty()) {
                writeLong((int64)l->size());
                ConstListIterator li(l);
                while (li.next()) {
                    if (checkCancel(xsink)
                        || encodeIntern(node->items, li.getValue(), depth + 1, xsink)) {
                        return -1;
                    }
                }
            }
            writeLong(0);
            return 0;
        }

        case AT_MAP: {
            if (v.getType() != NT_HASH) {
                return typeError(node, v, xsink);
            }
            const QoreHashNode* h = v.get<const QoreHashNode>();
            if (!h->empty()) {
                writeLong((int64)h->size());
                ConstHashIterator hi(h);
                while (hi.next()) {
                    if (checkCancel(xsink)) {
                        return -1;
                    }
                    QoreString key(hi.getKey(), QCS_UTF8);
                    writeByteSeq(key.c_str(), key.size());
                    if (encodeIntern(node->items, hi.get(), depth + 1, xsink)) {
                        return -1;
                    }
                }
            }
            writeLong(0);
            return 0;
        }

        case AT_UNION: {
            int idx = selectUnionBranch(node, v);
            if (idx < 0) {
                QoreStringMaker desc("no branch of the union accepts a value of Qore type '%s'; "
                    "the branches are: ", v.getTypeName());
                for (size_t i = 0; i < node->branches.size(); ++i) {
                    if (i) {
                        desc.concat(", ");
                    }
                    const AvroNode* b = node->branches[i];
                    if (!b->fullname.empty()) {
                        desc.sprintf("'%s'", b->fullname.c_str());
                    } else if (b->logical != ALT_NONE) {
                        desc.sprintf("'%s' (%s)", avro_type_name(b->type),
                            avro_logical_type_name(b->logical));
                    } else {
                        desc.sprintf("'%s'", avro_type_name(b->type));
                    }
                }
                xsink->raiseException("AVRO-ENCODE-ERROR", "%s", desc.c_str());
                return -1;
            }
            writeLong(idx);
            return encodeIntern(node->branches[(size_t)idx], v, depth + 1, xsink);
        }

        default:
            return encodeScalar(node, v, depth, xsink);
    }
}
