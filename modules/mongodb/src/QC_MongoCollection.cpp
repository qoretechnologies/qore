/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_MongoCollection.qpp

    Qore mongodb module - MongoCollection class definition

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

#include "qore/Qore.h"
#include "QC_MongoCollection.h"
#include "QC_MongoCursor.h"
/* Qore class mongodb::MongoCollection */

qore_classid_t CID_MONGOCOLLECTION;
QoreClass* QC_MONGOCOLLECTION;

// MongoCursor MongoCollection::aggregate(list<hash<auto>> pipeline){}
static QoreValue MongoCollection_aggregate_C16list_hash_auto__(QoreObject* self, QoreMongoCollection* collection, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
    const QoreListNode* pipeline = HARD_QORE_VALUE_LIST(args, 0);
# 284 "QC_MongoCollection.qpp"
    mongoc_cursor_t* cursor = collection->aggregate(pipeline, xsink);
    if (*xsink) {
        return QoreValue();
    }
    return new QoreObject(QC_MONGOCURSOR, getProgram(), new QoreMongoCursor(cursor));
}

// int MongoCollection::countDocuments(*hash<auto> filter){}
static QoreValue MongoCollection_countDocuments_C11_hash_auto_(QoreObject* self, QoreMongoCollection* collection, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
    const QoreHashNode* filter = get_param_value(args, 0).get<const QoreHashNode>();
# 245 "QC_MongoCollection.qpp"
    return collection->countDocuments(filter, xsink);
}

// hash<auto> MongoCollection::deleteMany(hash<auto> filter){}
static QoreValue MongoCollection_deleteMany_C10hash_auto_(QoreObject* self, QoreMongoCollection* collection, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
    const QoreHashNode* filter = HARD_QORE_VALUE_HASH(args, 0);
# 229 "QC_MongoCollection.qpp"
    return collection->deleteMany(filter, xsink);
}

// hash<auto> MongoCollection::deleteOne(hash<auto> filter){}
static QoreValue MongoCollection_deleteOne_C10hash_auto_(QoreObject* self, QoreMongoCollection* collection, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
    const QoreHashNode* filter = HARD_QORE_VALUE_HASH(args, 0);
# 213 "QC_MongoCollection.qpp"
    return collection->deleteOne(filter, xsink);
}

// bool MongoCollection::drop(){}
static QoreValue MongoCollection_drop(QoreObject* self, QoreMongoCollection* collection, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
# 261 "QC_MongoCollection.qpp"
    return collection->drop(xsink);
}

// MongoCursor MongoCollection::find(*hash<auto> filter, *hash<auto> opts){}
static QoreValue MongoCollection_find_C11_hash_auto_C11_hash_auto_(QoreObject* self, QoreMongoCollection* collection, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
    const QoreHashNode* filter = get_param_value(args, 0).get<const QoreHashNode>();
    const QoreHashNode* opts = get_param_value(args, 1).get<const QoreHashNode>();
# 133 "QC_MongoCollection.qpp"
    mongoc_cursor_t* cursor = collection->find(filter, opts, xsink);
    if (*xsink) {
        return QoreValue();
    }
    return new QoreObject(QC_MONGOCURSOR, getProgram(), new QoreMongoCursor(cursor));
}

// *hash<auto> MongoCollection::findOne(*hash<auto> filter){}
static QoreValue MongoCollection_findOne_C11_hash_auto_(QoreObject* self, QoreMongoCollection* collection, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
    const QoreHashNode* filter = get_param_value(args, 0).get<const QoreHashNode>();
# 155 "QC_MongoCollection.qpp"
    return collection->findOne(filter, xsink);
}

// *string MongoCollection::getName(){}
static QoreValue MongoCollection_getName(QoreObject* self, QoreMongoCollection* collection, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
# 70 "QC_MongoCollection.qpp"
    return collection->getName();
}

// hash<auto> MongoCollection::insertMany(list<hash<auto>> documents){}
static QoreValue MongoCollection_insertMany_C16list_hash_auto__(QoreObject* self, QoreMongoCollection* collection, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
    const QoreListNode* documents = HARD_QORE_VALUE_LIST(args, 0);
# 106 "QC_MongoCollection.qpp"
    return collection->insertMany(documents, xsink);
}

// hash<auto> MongoCollection::insertOne(hash<auto> document){}
static QoreValue MongoCollection_insertOne_C10hash_auto_(QoreObject* self, QoreMongoCollection* collection, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
    const QoreHashNode* document = HARD_QORE_VALUE_HASH(args, 0);
# 86 "QC_MongoCollection.qpp"
    return collection->insertOne(document, xsink);
}

// hash<auto> MongoCollection::updateMany(hash<auto> filter, hash<auto> update, *hash<auto> options){}
static QoreValue MongoCollection_updateMany(QoreObject* self, QoreMongoCollection* collection, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
    const QoreHashNode* filter = HARD_QORE_VALUE_HASH(args, 0);
    const QoreHashNode* update = HARD_QORE_VALUE_HASH(args, 1);
    const QoreHashNode* options = args->size() > 2 ? args->retrieveEntry(2).get<const QoreHashNode>() : nullptr;
    return collection->updateMany(filter, update, options, xsink);
}

// hash<auto> MongoCollection::updateOne(hash<auto> filter, hash<auto> update, *hash<auto> options){}
static QoreValue MongoCollection_updateOne(QoreObject* self, QoreMongoCollection* collection, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
    const QoreHashNode* filter = HARD_QORE_VALUE_HASH(args, 0);
    const QoreHashNode* update = HARD_QORE_VALUE_HASH(args, 1);
    const QoreHashNode* options = args->size() > 2 ? args->retrieveEntry(2).get<const QoreHashNode>() : nullptr;
    return collection->updateOne(filter, update, options, xsink);
}

// hash<auto> MongoCollection::replaceOne(hash<auto> filter, hash<auto> replacement, *hash<auto> options){}
static QoreValue MongoCollection_replaceOne(QoreObject* self, QoreMongoCollection* collection, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
    const QoreHashNode* filter = HARD_QORE_VALUE_HASH(args, 0);
    const QoreHashNode* replacement = HARD_QORE_VALUE_HASH(args, 1);
    const QoreHashNode* options = args->size() > 2 ? args->retrieveEntry(2).get<const QoreHashNode>() : nullptr;
    return collection->replaceOne(filter, replacement, options, xsink);
}

DLLLOCAL void preinitMongoCollectionClass() {
    QC_MONGOCOLLECTION = new QoreBuiltinClass("MongoCollection", "::mongodb::MongoCollection", QDOM_DEFAULT);
    CID_MONGOCOLLECTION = QC_MONGOCOLLECTION->getID();
    QC_MONGOCOLLECTION->setSystem();
}

DLLLOCAL QoreClass* initMongoCollectionClass(QoreNamespace& ns) {
    if (!QC_MONGOCOLLECTION)
        preinitMongoCollectionClass();

    QC_MONGOCOLLECTION->setFinal();

    // MongoCursor MongoCollection::aggregate(list<hash<auto>> pipeline){}
    QC_MONGOCOLLECTION->addMethod("aggregate", (q_method_n_t)MongoCollection_aggregate_C16list_hash_auto__, Public, QCF_NO_FLAGS, QDOM_DEFAULT, QC_MONGOCURSOR->getTypeInfo(), 1, qore_get_complex_list_type(autoHashTypeInfo), QORE_PARAM_NO_ARG, "pipeline");

    // int MongoCollection::countDocuments(*hash<auto> filter){}
    QC_MONGOCOLLECTION->addMethod("countDocuments", (q_method_n_t)MongoCollection_countDocuments_C11_hash_auto_, Public, QCF_NO_FLAGS, QDOM_DEFAULT, bigIntTypeInfo, 1, autoHashOrNothingTypeInfo, QORE_PARAM_NO_ARG, "filter");

    // hash<auto> MongoCollection::deleteMany(hash<auto> filter){}
    QC_MONGOCOLLECTION->addMethod("deleteMany", (q_method_n_t)MongoCollection_deleteMany_C10hash_auto_, Public, QCF_NO_FLAGS, QDOM_DEFAULT, autoHashTypeInfo, 1, autoHashTypeInfo, QORE_PARAM_NO_ARG, "filter");

    // hash<auto> MongoCollection::deleteOne(hash<auto> filter){}
    QC_MONGOCOLLECTION->addMethod("deleteOne", (q_method_n_t)MongoCollection_deleteOne_C10hash_auto_, Public, QCF_NO_FLAGS, QDOM_DEFAULT, autoHashTypeInfo, 1, autoHashTypeInfo, QORE_PARAM_NO_ARG, "filter");

    // bool MongoCollection::drop(){}
    QC_MONGOCOLLECTION->addMethod("drop", (q_method_n_t)MongoCollection_drop, Public, QCF_NO_FLAGS, QDOM_DEFAULT, boolTypeInfo);

    // MongoCursor MongoCollection::find(*hash<auto> filter, *hash<auto> opts){}
    QC_MONGOCOLLECTION->addMethod("find", (q_method_n_t)MongoCollection_find_C11_hash_auto_C11_hash_auto_, Public, QCF_NO_FLAGS, QDOM_DEFAULT, QC_MONGOCURSOR->getTypeInfo(), 2, autoHashOrNothingTypeInfo, QORE_PARAM_NO_ARG, "filter", autoHashOrNothingTypeInfo, QORE_PARAM_NO_ARG, "opts");

    // *hash<auto> MongoCollection::findOne(*hash<auto> filter){}
    QC_MONGOCOLLECTION->addMethod("findOne", (q_method_n_t)MongoCollection_findOne_C11_hash_auto_, Public, QCF_NO_FLAGS, QDOM_DEFAULT, autoHashOrNothingTypeInfo, 1, autoHashOrNothingTypeInfo, QORE_PARAM_NO_ARG, "filter");

    // *string MongoCollection::getName(){}
    QC_MONGOCOLLECTION->addMethod("getName", (q_method_n_t)MongoCollection_getName, Public, QCF_CONSTANT, QDOM_DEFAULT, stringOrNothingTypeInfo);

    // hash<auto> MongoCollection::insertMany(list<hash<auto>> documents){}
    QC_MONGOCOLLECTION->addMethod("insertMany", (q_method_n_t)MongoCollection_insertMany_C16list_hash_auto__, Public, QCF_NO_FLAGS, QDOM_DEFAULT, autoHashTypeInfo, 1, qore_get_complex_list_type(autoHashTypeInfo), QORE_PARAM_NO_ARG, "documents");

    // hash<auto> MongoCollection::insertOne(hash<auto> document){}
    QC_MONGOCOLLECTION->addMethod("insertOne", (q_method_n_t)MongoCollection_insertOne_C10hash_auto_, Public, QCF_NO_FLAGS, QDOM_DEFAULT, autoHashTypeInfo, 1, autoHashTypeInfo, QORE_PARAM_NO_ARG, "document");

    // hash<auto> MongoCollection::updateMany(hash<auto> filter, hash<auto> update, *hash<auto> options){}
    QC_MONGOCOLLECTION->addMethod("updateMany", (q_method_n_t)MongoCollection_updateMany, Public, QCF_NO_FLAGS, QDOM_DEFAULT, autoHashTypeInfo, 3, autoHashTypeInfo, QORE_PARAM_NO_ARG, "filter", autoHashTypeInfo, QORE_PARAM_NO_ARG, "update", autoHashOrNothingTypeInfo, QORE_PARAM_NO_ARG, "options");

    // hash<auto> MongoCollection::updateOne(hash<auto> filter, hash<auto> update, *hash<auto> options){}
    QC_MONGOCOLLECTION->addMethod("updateOne", (q_method_n_t)MongoCollection_updateOne, Public, QCF_NO_FLAGS, QDOM_DEFAULT, autoHashTypeInfo, 3, autoHashTypeInfo, QORE_PARAM_NO_ARG, "filter", autoHashTypeInfo, QORE_PARAM_NO_ARG, "update", autoHashOrNothingTypeInfo, QORE_PARAM_NO_ARG, "options");

    // hash<auto> MongoCollection::replaceOne(hash<auto> filter, hash<auto> replacement, *hash<auto> options){}
    QC_MONGOCOLLECTION->addMethod("replaceOne", (q_method_n_t)MongoCollection_replaceOne, Public, QCF_NO_FLAGS, QDOM_DEFAULT, autoHashTypeInfo, 3, autoHashTypeInfo, QORE_PARAM_NO_ARG, "filter", autoHashTypeInfo, QORE_PARAM_NO_ARG, "replacement", autoHashOrNothingTypeInfo, QORE_PARAM_NO_ARG, "options");

    return QC_MONGOCOLLECTION;
}
