/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    bson_conversion.cpp

    Qore mongodb module - BSON to/from Qore value conversion

    Copyright (C) 2025 - 2026 Qore Technologies, s.r.o.

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

#include "bson_conversion.h"
#include "QC_ObjectId.h"

/* BSON strings and the names of the driver are UTF-8 text, which libbson does not check when it reads a document, and
   documents from a server can have strings that are not valid UTF-8 (written by other drivers or older servers); the
   text is checked where it enters the program (qore#6604)
*/
QoreStringNode* mongo_utf8_string(const char* p, size_t len, const char* where, const char* what,
        ExceptionSink* xsink) {
    if (q_check_text_encoding(QCS_UTF8, p, len, where, what, xsink)) {
        return nullptr;
    }
    return new QoreStringNode(p, len, QCS_UTF8);
}

QoreListNode* mongo_utf8_string_list(char** names, const char* where, const char* what, ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> list(new QoreListNode(stringTypeInfo), xsink);
    for (size_t i = 0; names[i]; ++i) {
        QoreStringNode* name = mongo_utf8_string(names[i], strlen(names[i]), where, what, xsink);
        if (!name) {
            return nullptr;
        }
        if (list->push(name, xsink)) {
            return nullptr;
        }
    }
    return list.release();
}

int mongo_set_utf8_key(QoreHashNode* h, const char* key, size_t len, QoreValue value, ExceptionSink* xsink) {
    // a hash key is in the default encoding: a UTF-8 key is converted to it, as parse_json() converts its keys
    // (a BSON key has no NUL character: it is a NUL-terminated string)
    if (QCS_DEFAULT == QCS_UTF8) {
        return h->setKeyValue(key, value, xsink);
    }
    return h->setKeyValue(QoreString(key, len, QCS_UTF8), value, xsink);
}

int mongo_utf8_arg(const QoreString& str, TempEncodingHelper& utf8, const char* where, const char* what,
        ExceptionSink* xsink) {
    if (q_check_text_encoding(str.getEncoding(), str.c_str(), str.size(), where, what, xsink)) {
        return -1;
    }
    return utf8.set(&str, QCS_UTF8, xsink) ? 0 : -1;
}

MongoUtf8Key::MongoUtf8Key(const char* key, ExceptionSink* xsink) : key(key) {
    // a hash key is in the default encoding, and a BSON key is UTF-8 text
    if (QCS_DEFAULT != QCS_UTF8) {
        QoreString str(key, QCS_DEFAULT);
        converted.reset(str.convertEncoding(QCS_UTF8, xsink));
        this->key = converted ? converted->c_str() : nullptr;
    }
}

static QoreStringNode* bson_string(const char* p, size_t len, const char* what, ExceptionSink* xsink) {
    return mongo_utf8_string(p, len, "MongoDB", what, xsink);
}

QoreValue bson_value_to_qore(const bson_value_t* value, ExceptionSink* xsink) {
    // conversion recurses with the nesting of the document, which only the size of the document bounds
    if (q_check_stack(xsink)) {
        return QoreValue();
    }
    switch (value->value_type) {
        case BSON_TYPE_EOD:
            return QoreValue();

        case BSON_TYPE_DOUBLE:
            return value->value.v_double;

        case BSON_TYPE_UTF8:
            return bson_string(value->value.v_utf8.str, value->value.v_utf8.len, "BSON string", xsink);

        case BSON_TYPE_DOCUMENT: {
            bson_t doc;
            bson_init_static(&doc, value->value.v_doc.data, value->value.v_doc.data_len);
            return bson_to_qore_hash(&doc, xsink);
        }

        case BSON_TYPE_ARRAY: {
            bson_t arr;
            bson_init_static(&arr, value->value.v_doc.data, value->value.v_doc.data_len);
            bson_iter_t iter;
            if (!bson_iter_init(&iter, &arr)) {
                return QoreValue();
            }
            ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), xsink);
            while (bson_iter_next(&iter)) {
                const bson_value_t* elem = bson_iter_value(&iter);
                list->push(bson_value_to_qore(elem, xsink), xsink);
                if (*xsink) {
                    return QoreValue();
                }
            }
            return list.release();
        }

        case BSON_TYPE_BINARY: {
            SimpleRefHolder<BinaryNode> bin(new BinaryNode);
            bin->append(value->value.v_binary.data, value->value.v_binary.data_len);
            return bin.release();
        }

        case BSON_TYPE_UNDEFINED:
            return QoreValue();

        case BSON_TYPE_OID: {
            char oid_str[25];
            bson_oid_to_string(&value->value.v_oid, oid_str);
            return new QoreObject(QC_OBJECTID, getProgram(), new QoreObjectId(value->value.v_oid));
        }

        case BSON_TYPE_BOOL:
            return value->value.v_bool;

        case BSON_TYPE_DATE_TIME: {
            int64_t ms = value->value.v_datetime;
            return DateTimeNode::makeAbsolute(currentTZ(), ms / 1000, (int)(ms % 1000) * 1000);
        }

        case BSON_TYPE_NULL:
            return QoreValue();

        case BSON_TYPE_REGEX: {
            // Return as hash with pattern and options
            ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
            QoreStringNode* regex = bson_string(value->value.v_regex.regex, strlen(value->value.v_regex.regex),
                "BSON regular expression", xsink);
            if (!regex) {
                return QoreValue();
            }
            h->setKeyValue("$regex", regex, xsink);
            QoreStringNode* options = bson_string(value->value.v_regex.options,
                strlen(value->value.v_regex.options), "BSON regular expression options", xsink);
            if (!options) {
                return QoreValue();
            }
            h->setKeyValue("$options", options, xsink);
            return h.release();
        }

        case BSON_TYPE_DBPOINTER: {
            // Deprecated type - return as hash
            ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
            QoreStringNode* ref = bson_string(value->value.v_dbpointer.collection,
                value->value.v_dbpointer.collection_len, "BSON DBPointer collection", xsink);
            if (!ref) {
                return QoreValue();
            }
            h->setKeyValue("$ref", ref, xsink);
            char oid_str[25];
            bson_oid_to_string(&value->value.v_dbpointer.oid, oid_str);
            h->setKeyValue("$id", new QoreStringNode(oid_str), xsink);
            return h.release();
        }

        case BSON_TYPE_CODE:
            return bson_string(value->value.v_code.code, value->value.v_code.code_len, "BSON code", xsink);

        case BSON_TYPE_SYMBOL:
            return bson_string(value->value.v_symbol.symbol, value->value.v_symbol.len, "BSON symbol", xsink);

        case BSON_TYPE_CODEWSCOPE: {
            // Return as hash with code and scope
            ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
            QoreStringNode* code = bson_string(value->value.v_codewscope.code, value->value.v_codewscope.code_len,
                "BSON code", xsink);
            if (!code) {
                return QoreValue();
            }
            h->setKeyValue("$code", code, xsink);
            bson_t scope;
            bson_init_static(&scope, value->value.v_codewscope.scope_data,
                value->value.v_codewscope.scope_len);
            QoreHashNode* scope_hash = bson_to_qore_hash(&scope, xsink);
            if (!scope_hash) {
                return QoreValue();
            }
            h->setKeyValue("$scope", scope_hash, xsink);
            return h.release();
        }

        case BSON_TYPE_INT32:
            return (int64)value->value.v_int32;

        case BSON_TYPE_TIMESTAMP: {
            // Return as hash with timestamp and increment
            ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
            h->setKeyValue("t", (int64)value->value.v_timestamp.timestamp, xsink);
            h->setKeyValue("i", (int64)value->value.v_timestamp.increment, xsink);
            return h.release();
        }

        case BSON_TYPE_INT64:
            return value->value.v_int64;

        case BSON_TYPE_DECIMAL128: {
            char str[BSON_DECIMAL128_STRING];
            bson_decimal128_to_string(&value->value.v_decimal128, str);
            return new QoreNumberNode(str);
        }

        case BSON_TYPE_MINKEY: {
            ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
            h->setKeyValue("$minKey", 1ll, xsink);
            return h.release();
        }

        case BSON_TYPE_MAXKEY: {
            ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
            h->setKeyValue("$maxKey", 1ll, xsink);
            return h.release();
        }

        default:
            xsink->raiseException("MONGODB-BSON-ERROR", "unknown BSON type: %d", (int)value->value_type);
            return QoreValue();
    }
}

QoreHashNode* bson_to_qore_hash(const bson_t* doc, ExceptionSink* xsink) {
    bson_iter_t iter;
    if (!bson_iter_init(&iter, doc)) {
        xsink->raiseException("MONGODB-BSON-ERROR", "failed to initialize BSON iterator");
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> hash(new QoreHashNode(autoTypeInfo), xsink);

    while (bson_iter_next(&iter)) {
        const char* key = bson_iter_key(&iter);
        size_t key_len = bson_iter_key_len(&iter);
        // a key is UTF-8 text, as a string is
        if (q_check_text_encoding(QCS_UTF8, key, key_len, "MongoDB", "BSON key", xsink)) {
            return nullptr;
        }
        const bson_value_t* value = bson_iter_value(&iter);

        // the holder releases a value returned with an exception
        ValueHolder qv(bson_value_to_qore(value, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        if (mongo_set_utf8_key(*hash, key, key_len, qv.release(), xsink) || *xsink) {
            return nullptr;
        }
    }

    return hash.release();
}

int qore_value_to_bson_append(bson_t* doc, const char* key, const QoreValue& v_in, ExceptionSink* xsink) {
    // conversion recurses with the nesting of the value, which is unbounded
    if (q_check_stack(xsink)) {
        return -1;
    }
    // a value read through the language unwraps itself, but walking a container gives the stored
    // representation, so a value assigned with ':=' or '@=' arrives here as the reference itself; the target of a
    // weak reference is read with a reference of its own, as another thread can free its list or hash while it is
    // converted (qore#7231)
    QoreValue value = v_in;
    ValueHolder target(xsink);
    switch (value.getType()) {
        case NT_WEAKREF:
        case NT_WEAKREF_HASH:
        case NT_WEAKREF_LIST:
            target = value.resolveIndirectRef();
            value = *target;
            break;
        default:
            // the target of an opaque reference is held by the container that holds the reference
            if (value.isOpaque()) {
                value = value.resolveIndirect();
            }
            break;
    }
    switch (value.getType()) {
        case NT_NOTHING:
        case NT_NULL:
            if (!bson_append_null(doc, key, -1)) {
                xsink->raiseException("MONGODB-BSON-ERROR", "failed to append null value");
                return -1;
            }
            break;

        case NT_BOOLEAN:
            if (!bson_append_bool(doc, key, -1, value.getAsBool())) {
                xsink->raiseException("MONGODB-BSON-ERROR", "failed to append boolean value");
                return -1;
            }
            break;

        case NT_INT:
            if (!bson_append_int64(doc, key, -1, value.getAsBigInt())) {
                xsink->raiseException("MONGODB-BSON-ERROR", "failed to append int64 value");
                return -1;
            }
            break;

        case NT_FLOAT:
            if (!bson_append_double(doc, key, -1, value.getAsFloat())) {
                xsink->raiseException("MONGODB-BSON-ERROR", "failed to append double value");
                return -1;
            }
            break;

        case NT_NUMBER: {
            const QoreNumberNode* num = value.get<const QoreNumberNode>();
            QoreString str;
            // the exact decimal value of the number if it has one of at most the 34 digits of a decimal128 value,
            // otherwise its display string (issue #6837)
            if (num->getDecimalString(str, 34, -1, xsink)) {
                return -1;
            }
            bson_decimal128_t dec;
            if (!bson_decimal128_from_string(str.c_str(), &dec)) {
                xsink->raiseException("MONGODB-BSON-ERROR", "failed to convert number to decimal128");
                return -1;
            }
            if (!bson_append_decimal128(doc, key, -1, &dec)) {
                xsink->raiseException("MONGODB-BSON-ERROR", "failed to append decimal128 value");
                return -1;
            }
            break;
        }

        case NT_STRING: {
            QoreStringValueHelper str(value);
            TempEncodingHelper utf8(*str, QCS_UTF8, xsink);
            if (*xsink) {
                return -1;
            }
            if (!bson_append_utf8(doc, key, -1, utf8->c_str(), utf8->size())) {
                xsink->raiseException("MONGODB-BSON-ERROR", "failed to append string value");
                return -1;
            }
            break;
        }

        case NT_DATE: {
            const DateTimeNode* dt = value.get<const DateTimeNode>();
            int64_t ms = dt->getEpochMillisecondsUTC();
            if (!bson_append_date_time(doc, key, -1, ms)) {
                xsink->raiseException("MONGODB-BSON-ERROR", "failed to append datetime value");
                return -1;
            }
            break;
        }

        case NT_BINARY: {
            const BinaryNode* bin = value.get<const BinaryNode>();
            if (!bson_append_binary(doc, key, -1, BSON_SUBTYPE_BINARY,
                    (const uint8_t*)bin->getPtr(), bin->size())) {
                xsink->raiseException("MONGODB-BSON-ERROR", "failed to append binary value");
                return -1;
            }
            break;
        }

        case NT_HASH: {
            const QoreHashNode* hash = value.get<const QoreHashNode>();
            bson_t* child = qore_hash_to_bson(hash, xsink);
            if (*xsink) {
                return -1;
            }
            if (!bson_append_document(doc, key, -1, child)) {
                bson_destroy(child);
                xsink->raiseException("MONGODB-BSON-ERROR", "failed to append document value");
                return -1;
            }
            bson_destroy(child);
            break;
        }

        case NT_LIST: {
            const QoreListNode* list = value.get<const QoreListNode>();
            if (qore_list_to_bson_array(doc, key, list, xsink)) {
                return -1;
            }
            break;
        }

        case NT_OBJECT: {
            const QoreObject* obj = value.get<const QoreObject>();
            // Check if it's an ObjectId
            QoreObjectId* oid = (QoreObjectId*)obj->getReferencedPrivateData(CID_OBJECTID, xsink);
            if (oid) {
                if (!bson_append_oid(doc, key, -1, oid->getOid())) {
                    oid->deref(xsink);
                    xsink->raiseException("MONGODB-BSON-ERROR", "failed to append ObjectId value");
                    return -1;
                }
                oid->deref(xsink);
                break;
            }
            if (*xsink) {
                return -1;
            }
            // Other objects - convert to hash representation
            xsink->raiseException("MONGODB-BSON-ERROR", "unsupported object type for BSON conversion");
            return -1;
        }

        default:
            xsink->raiseException("MONGODB-BSON-ERROR", "unsupported Qore type for BSON conversion: %s",
                value.getTypeName());
            return -1;
    }

    return 0;
}

int qore_list_to_bson_array(bson_t* doc, const char* key, const QoreListNode* list, ExceptionSink* xsink) {
    if (qore_check_cancel(xsink, "converting BSON array")) {
        return -1;
    }
    bson_t child;
#if BSON_CHECK_VERSION(2, 3, 0)
    // The renamed API requires sequential numeric keys, which the list
    // iterator supplies below.  The old name is a deprecated alias in 2.3+.
    if (!bson_append_array_unsafe_begin(doc, key, -1, &child)) {
#else
    if (!bson_append_array_begin(doc, key, -1, &child)) {
#endif
        xsink->raiseException("MONGODB-BSON-ERROR", "failed to begin array");
        return -1;
    }

    ConstListIterator li(list);
    while (li.next()) {
        if (li.index() && !(li.index() % 100) && qore_check_cancel(xsink, "converting BSON array")) {
            return -1;
        }
        QoreStringMaker idx("%zd", li.index());
        if (qore_value_to_bson_append(&child, idx.c_str(), li.getValue(), xsink)) {
            return -1;
        }
    }

    if (!bson_append_array_end(doc, &child)) {
        xsink->raiseException("MONGODB-BSON-ERROR", "failed to end array");
        return -1;
    }

    return 0;
}

bson_t* qore_hash_to_bson(const QoreHashNode* hash, ExceptionSink* xsink) {
    bson_t* doc = bson_new();

    ConstHashIterator hi(hash);
    while (hi.next()) {
        MongoUtf8Key key(hi.getKey(), xsink);
        if (!key) {
            bson_destroy(doc);
            return nullptr;
        }
        QoreValue value = hi.get();

        if (qore_value_to_bson_append(doc, key.c_str(), value, xsink)) {
            bson_destroy(doc);
            return nullptr;
        }
    }

    return doc;
}
