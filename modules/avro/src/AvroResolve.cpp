/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file AvroResolve.cpp reader/writer Avro schema resolution */
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

#include <qore/ReferenceHolder.h>

#include <vector>

//! returns true if the reader's named type accepts the writer's, by fullname, alias or short name
/** The specification matches named types on the unqualified name; every widely-used
    implementation also matches fullnames and reader aliases, and both are needed in practice, so
    all three are accepted here.
*/
static bool named_types_match(const AvroNode* writer, const AvroNode* reader) {
    return reader->matchesName(writer->fullname)
        || reader->getSimpleName() == writer->getSimpleName();
}

//! returns true if a datum written with \a writer can be read with \a reader
bool avro_schemas_match(const AvroNode* writer, const AvroNode* reader) {
    if (writer->type == AT_UNION || reader->type == AT_UNION) {
        return true;
    }
    if (writer->type == reader->type) {
        switch (writer->type) {
            case AT_RECORD:
            case AT_ENUM:
                return named_types_match(writer, reader);
            case AT_FIXED:
                return writer->fixed_size == reader->fixed_size
                    && named_types_match(writer, reader);
            case AT_ARRAY:
            case AT_MAP:
                return avro_schemas_match(writer->items, reader->items);
            default:
                return true;
        }
    }
    // the promotions permitted by the specification
    switch (writer->type) {
        case AT_INT:
            return reader->type == AT_LONG || reader->type == AT_FLOAT
                || reader->type == AT_DOUBLE;
        case AT_LONG:
            return reader->type == AT_FLOAT || reader->type == AT_DOUBLE;
        case AT_FLOAT:
            return reader->type == AT_DOUBLE;
        case AT_STRING:
            return reader->type == AT_BYTES;
        case AT_BYTES:
            return reader->type == AT_STRING;
        default:
            return false;
    }
}

//! raises AVRO-RESOLUTION-ERROR describing why two schemas cannot be reconciled
static void resolution_error(const AvroNode* writer, const AvroNode* reader,
        ExceptionSink* xsink) {
    QoreStringMaker desc("a datum written with Avro type '%s'", avro_type_name(writer->type));
    if (!writer->fullname.empty()) {
        desc.sprintf(" ('%s')", writer->fullname.c_str());
    }
    desc.sprintf(" cannot be read with reader type '%s'", avro_type_name(reader->type));
    if (!reader->fullname.empty()) {
        desc.sprintf(" ('%s')", reader->fullname.c_str());
    }
    xsink->raiseException("AVRO-RESOLUTION-ERROR", "%s", desc.c_str());
}

//! discards any values left in the reader field slots when a record resolution fails part-way
class AvroSlotCleanup {
public:
    DLLLOCAL AvroSlotCleanup(std::vector<QoreValue>& slots, ExceptionSink* xsink) : slots(slots),
            xsink(xsink) {
    }

    DLLLOCAL ~AvroSlotCleanup() {
        for (QoreValue& v : slots) {
            v.discard(xsink);
        }
    }

private:
    std::vector<QoreValue>& slots;
    ExceptionSink* xsink;
};

QoreValue AvroDecoder::resolveIntern(const AvroNode* writer, const AvroNode* reader, unsigned depth,
        ExceptionSink* xsink) {
    if (checkDepth(depth, xsink)) {
        return QoreValue();
    }

    // the writer's union is resolved first: the branch index is on the wire, and the branch it
    // selects is then resolved against the reader's schema, union or not
    if (writer->type == AT_UNION) {
        int64 idx;
        if (readLong(idx, xsink)) {
            return QoreValue();
        }
        if (idx < 0 || idx >= (int64)writer->branches.size()) {
            xsink->raiseException("AVRO-DECODE-ERROR", "union branch index " QLLD " at offset %zu "
                "is out of range; the writer's union has %d branch%s", idx, pos,
                (int)writer->branches.size(), writer->branches.size() == 1 ? "" : "es");
            return QoreValue();
        }
        return resolveIntern(writer->branches[(size_t)idx], reader, depth + 1, xsink);
    }

    if (reader->type == AT_UNION) {
        for (const AvroNode* b : reader->branches) {
            if (avro_schemas_match(writer, b)) {
                return resolveIntern(writer, b, depth + 1, xsink);
            }
        }
        QoreStringMaker desc("no branch of the reader's union matches writer type '%s'",
            avro_type_name(writer->type));
        if (!writer->fullname.empty()) {
            desc.sprintf(" ('%s')", writer->fullname.c_str());
        }
        xsink->raiseException("AVRO-RESOLUTION-ERROR", "%s", desc.c_str());
        return QoreValue();
    }

    if (writer->type != reader->type) {
        // the numeric and string/bytes promotions; anything else is an error
        switch (writer->type) {
            case AT_INT:
            case AT_LONG: {
                if (reader->type != AT_LONG && reader->type != AT_FLOAT
                    && reader->type != AT_DOUBLE) {
                    break;
                }
                if (writer->type == AT_LONG && reader->type == AT_LONG) {
                    break;
                }
                int64 v;
                if (readLong(v, xsink)) {
                    return QoreValue();
                }
                if (reader->type == AT_LONG) {
                    return avro_apply_logical_type(reader, v, xsink);
                }
                return (double)v;
            }

            case AT_FLOAT: {
                if (reader->type != AT_DOUBLE) {
                    break;
                }
                double d;
                if (readFloat(d, xsink)) {
                    return QoreValue();
                }
                return d;
            }

            case AT_STRING: {
                if (reader->type != AT_BYTES) {
                    break;
                }
                SimpleRefHolder<QoreStringNode> str(readString(xsink));
                if (!str) {
                    return QoreValue();
                }
                SimpleRefHolder<BinaryNode> b(new BinaryNode);
                b->append(str->c_str(), str->size());
                return avro_apply_logical_type(reader, b.release(), xsink);
            }

            case AT_BYTES: {
                if (reader->type != AT_STRING) {
                    break;
                }
                SimpleRefHolder<BinaryNode> b(readBinary(xsink));
                if (!b) {
                    return QoreValue();
                }
                SimpleRefHolder<QoreStringNode> str(new QoreStringNode(
                    static_cast<const char*>(b->getPtr()), b->size(), QCS_UTF8));
                bool invalid = false;
                QCS_UTF8->getLength(str->c_str(), str->c_str() + str->size(), invalid);
                if (invalid) {
                    xsink->raiseException("AVRO-DECODE-ERROR", "a 'bytes' value promoted to the "
                        "reader's 'string' type is not valid UTF-8");
                    return QoreValue();
                }
                return str.release();
            }

            default:
                break;
        }
        resolution_error(writer, reader, xsink);
        return QoreValue();
    }

    switch (writer->type) {
        case AT_RECORD: {
            if (!named_types_match(writer, reader)) {
                resolution_error(writer, reader, xsink);
                return QoreValue();
            }
            // decode into reader field slots; writer fields the reader does not declare are
            // skipped without materialising a value
            std::vector<QoreValue> slots(reader->fields.size());
            std::vector<bool> filled(reader->fields.size(), false);
            AvroSlotCleanup cleanup(slots, xsink);

            for (const AvroField& wf : writer->fields) {
                if (checkCancel(xsink)) {
                    return QoreValue();
                }
                int ri = reader->findField(wf.name);
                if (ri < 0) {
                    if (skipValue(wf.type, depth + 1, xsink)) {
                        return QoreValue();
                    }
                    continue;
                }
                if (filled[(size_t)ri]) {
                    xsink->raiseException("AVRO-RESOLUTION-ERROR", "writer record '%s' has more "
                        "than one field matching field '%s' of reader record '%s'",
                        writer->fullname.c_str(), reader->fields[(size_t)ri].name.c_str(),
                        reader->fullname.c_str());
                    return QoreValue();
                }
                slots[(size_t)ri] = resolveIntern(wf.type, reader->fields[(size_t)ri].type,
                    depth + 1, xsink);
                if (*xsink) {
                    return QoreValue();
                }
                filled[(size_t)ri] = true;
            }

            ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
            for (size_t i = 0; i < reader->fields.size(); ++i) {
                const AvroField& rf = reader->fields[i];
                QoreValue v;
                if (filled[i]) {
                    v = slots[i];
                    slots[i] = QoreValue();
                } else if (rf.has_default) {
                    v = rf.default_value.refSelf();
                } else {
                    xsink->raiseException("AVRO-RESOLUTION-ERROR", "field '%s' of reader record "
                        "'%s' is not present in writer record '%s' and has no default value",
                        rf.name.c_str(), reader->fullname.c_str(), writer->fullname.c_str());
                    return QoreValue();
                }
                h->setKeyValue(rf.name.c_str(), v, xsink);
                if (*xsink) {
                    return QoreValue();
                }
            }
            return h.release();
        }

        case AT_ENUM: {
            if (!named_types_match(writer, reader)) {
                resolution_error(writer, reader, xsink);
                return QoreValue();
            }
            int64 idx;
            if (readLong(idx, xsink)) {
                return QoreValue();
            }
            if (idx < 0 || idx >= (int64)writer->symbols.size()) {
                xsink->raiseException("AVRO-DECODE-ERROR", "enum index " QLLD " at offset %zu is "
                    "out of range for writer enum '%s', which has %d symbol%s", idx, pos,
                    writer->fullname.c_str(), (int)writer->symbols.size(),
                    writer->symbols.size() == 1 ? "" : "s");
                return QoreValue();
            }
            const std::string& sym = writer->symbols[(size_t)idx];
            if (reader->findSymbol(sym) >= 0) {
                return new QoreStringNode(sym.c_str(), QCS_UTF8);
            }
            if (reader->enum_default >= 0) {
                return new QoreStringNode(reader->symbols[(size_t)reader->enum_default].c_str(),
                    QCS_UTF8);
            }
            xsink->raiseException("AVRO-RESOLUTION-ERROR", "writer enum '%s' produced symbol "
                "'%s', which reader enum '%s' does not declare and for which it has no default",
                writer->fullname.c_str(), sym.c_str(), reader->fullname.c_str());
            return QoreValue();
        }

        case AT_FIXED: {
            if (writer->fixed_size != reader->fixed_size || !named_types_match(writer, reader)) {
                resolution_error(writer, reader, xsink);
                return QoreValue();
            }
            const unsigned char* p;
            if (readRaw(p, reader->fixed_size, xsink)) {
                return QoreValue();
            }
            SimpleRefHolder<BinaryNode> b(new BinaryNode);
            b->append(p, reader->fixed_size);
            return avro_apply_logical_type(reader, b.release(), xsink);
        }

        case AT_ARRAY: {
            ReferenceHolder<QoreListNode> l(new QoreListNode(autoTypeInfo), xsink);
            int64 count = 0;
            int64 total = 0;
            while (true) {
                if (readBlockCount(writer->items, count, total, xsink)) {
                    return QoreValue();
                }
                if (!count) {
                    break;
                }
                for (int64 i = 0; i < count; ++i) {
                    if (checkCancel(xsink)) {
                        return QoreValue();
                    }
                    ValueHolder v(resolveIntern(writer->items, reader->items, depth + 1, xsink),
                        xsink);
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
                if (readBlockCount(writer->items, count, total, xsink)) {
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
                    ValueHolder v(resolveIntern(writer->items, reader->items, depth + 1, xsink),
                        xsink);
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

        default:
            // identical scalar types: the reader's logical type governs the Qore representation
            return decodeScalar(reader, xsink);
    }
}
