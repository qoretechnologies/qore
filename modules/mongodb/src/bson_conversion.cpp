/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    bson_conversion.cpp

    Qore mongodb module - BSON to/from Qore value conversion

    Copyright (C) 2025 Qore Technologies, s.r.o.

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

QoreValue bson_value_to_qore(const bson_value_t* value, ExceptionSink* xsink) {
    switch (value->value_type) {
        case BSON_TYPE_EOD:
            return QoreValue();

        case BSON_TYPE_DOUBLE:
            return value->value.v_double;

        case BSON_TYPE_UTF8:
            return new QoreStringNode(value->value.v_utf8.str, value->value.v_utf8.len, QCS_UTF8);

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
            h->setKeyValue("$regex", new QoreStringNode(value->value.v_regex.regex), xsink);
            h->setKeyValue("$options", new QoreStringNode(value->value.v_regex.options), xsink);
            return h.release();
        }

        case BSON_TYPE_DBPOINTER: {
            // Deprecated type - return as hash
            ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
            h->setKeyValue("$ref", new QoreStringNode(value->value.v_dbpointer.collection,
                value->value.v_dbpointer.collection_len), xsink);
            char oid_str[25];
            bson_oid_to_string(&value->value.v_dbpointer.oid, oid_str);
            h->setKeyValue("$id", new QoreStringNode(oid_str), xsink);
            return h.release();
        }

        case BSON_TYPE_CODE:
            return new QoreStringNode(value->value.v_code.code, value->value.v_code.code_len, QCS_UTF8);

        case BSON_TYPE_SYMBOL:
            return new QoreStringNode(value->value.v_symbol.symbol, value->value.v_symbol.len, QCS_UTF8);

        case BSON_TYPE_CODEWSCOPE: {
            // Return as hash with code and scope
            ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
            h->setKeyValue("$code", new QoreStringNode(value->value.v_codewscope.code,
                value->value.v_codewscope.code_len, QCS_UTF8), xsink);
            bson_t scope;
            bson_init_static(&scope, value->value.v_codewscope.scope_data,
                value->value.v_codewscope.scope_len);
            h->setKeyValue("$scope", bson_to_qore_hash(&scope, xsink), xsink);
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
        const bson_value_t* value = bson_iter_value(&iter);

        QoreValue qv = bson_value_to_qore(value, xsink);
        if (*xsink) {
            return nullptr;
        }
        hash->setKeyValue(key, qv, xsink);
        if (*xsink) {
            return nullptr;
        }
    }

    return hash.release();
}

int qore_value_to_bson_append(bson_t* doc, const char* key, const QoreValue& value, ExceptionSink* xsink) {
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
            num->toString(str);
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
            const QoreStringNode* str = value.get<const QoreStringNode>();
            TempEncodingHelper utf8(str, QCS_UTF8, xsink);
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
    bson_t child;
    if (!bson_append_array_begin(doc, key, -1, &child)) {
        xsink->raiseException("MONGODB-BSON-ERROR", "failed to begin array");
        return -1;
    }

    ConstListIterator li(list);
    while (li.next()) {
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
        const char* key = hi.getKey();
        QoreValue value = hi.get();

        if (qore_value_to_bson_append(doc, key, value, xsink)) {
            bson_destroy(doc);
            return nullptr;
        }
    }

    return doc;
}
