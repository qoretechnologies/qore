/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    MongoCollection.cpp

    Qore mongodb module - MongoCollection class implementation

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

#include "QC_MongoCollection.h"
#include "QC_ObjectId.h"
#include "bson_conversion.h"

#include <cstdlib>
#include <memory>

// CID_MONGOCOLLECTION and QC_MONGOCOLLECTION are defined in QC_MongoCollection.cpp (generated from .qpp)

QoreHashNode* QoreMongoCollection::insertOne(const QoreHashNode* document, ExceptionSink* xsink) const {
    if (!collection) {
        xsink->raiseException("MONGODB-COLLECTION-ERROR", "collection not initialized");
        return nullptr;
    }

    bson_t* doc = qore_hash_to_bson(document, xsink);
    if (*xsink) {
        return nullptr;
    }

    // Check if document already has an _id, if not add one
    bson_iter_t iter;
    bson_oid_t oid;
    bool have_oid = false;
    bool had_id = bson_iter_init_find(&iter, doc, "_id");
    if (!had_id) {
        bson_oid_init(&oid, nullptr);
        have_oid = true;
        bson_append_oid(doc, "_id", 3, &oid);
    } else if (BSON_ITER_HOLDS_OID(&iter)) {
        // Copy existing OID if it's an OID type
        bson_oid_copy(bson_iter_oid(&iter), &oid);
        have_oid = true;
    }

    bson_t reply;
    bson_error_t error;
    bool result = mongoc_collection_insert_one(collection, doc, nullptr, &reply, &error);
    bson_destroy(doc);

    if (!result) {
        bson_destroy(&reply);
        xsink->raiseException("MONGODB-COLLECTION-ERROR", "insertOne failed: %s", error.message);
        return nullptr;
    }

    QoreHashNode* rv = bson_to_qore_hash(&reply, xsink);
    bson_destroy(&reply);

    // Add insertedId to the result
    if (rv && have_oid) {
        rv->setKeyValue("insertedId", new QoreObject(QC_OBJECTID, getProgram(), new QoreObjectId(oid)), xsink);
    }

    return rv;
}

QoreHashNode* QoreMongoCollection::insertMany(const QoreListNode* documents, ExceptionSink* xsink) const {
    if (!collection) {
        xsink->raiseException("MONGODB-COLLECTION-ERROR", "collection not initialized");
        return nullptr;
    }

    size_t n_documents = documents->size();
    if (n_documents == 0) {
        xsink->raiseException("MONGODB-COLLECTION-ERROR", "documents list is empty");
        return nullptr;
    }

    // Convert all documents to BSON
    std::vector<void*> docs(n_documents);
    std::unique_ptr<void, decltype(&free)> doc_ptrs(std::malloc(n_documents * sizeof(const bson_t*)), &free);
    if (!doc_ptrs) {
        xsink->raiseException("MONGODB-COLLECTION-ERROR", "insertMany failed: out of memory");
        return nullptr;
    }
    const bson_t** bson_docs = static_cast<const bson_t**>(doc_ptrs.get());
    for (size_t i = 0; i < n_documents; i++) {
        QoreValue v = documents->retrieveEntry(i);
        if (v.getType() != NT_HASH) {
            // Clean up already converted docs
            for (size_t j = 0; j < i; j++) {
                bson_destroy(static_cast<bson_t*>(docs[j]));
            }
            xsink->raiseException("MONGODB-COLLECTION-ERROR", "document at index %zd is not a hash", i);
            return nullptr;
        }
        bson_t* bson = qore_hash_to_bson(v.get<const QoreHashNode>(), xsink);
        docs[i] = bson;
        bson_docs[i] = bson;
        if (*xsink) {
            // Clean up already converted docs
            for (size_t j = 0; j < i; j++) {
                bson_destroy(static_cast<bson_t*>(docs[j]));
            }
            return nullptr;
        }
    }

    bson_t reply;
    bson_error_t error;
    bool result = mongoc_collection_insert_many(collection, bson_docs, n_documents, nullptr, &reply, &error);

    // Clean up BSON documents
    for (size_t i = 0; i < n_documents; i++) {
        bson_destroy(static_cast<bson_t*>(docs[i]));
    }

    if (!result) {
        bson_destroy(&reply);
        xsink->raiseException("MONGODB-COLLECTION-ERROR", "insertMany failed: %s", error.message);
        return nullptr;
    }

    QoreHashNode* rv = bson_to_qore_hash(&reply, xsink);
    bson_destroy(&reply);
    return rv;
}

mongoc_cursor_t* QoreMongoCollection::find(const QoreHashNode* filter, const QoreHashNode* opts,
        ExceptionSink* xsink) const {
    if (!collection) {
        xsink->raiseException("MONGODB-COLLECTION-ERROR", "collection not initialized");
        return nullptr;
    }

    bson_t* query = filter ? qore_hash_to_bson(filter, xsink) : bson_new();
    if (*xsink) {
        return nullptr;
    }

    bson_t* options = nullptr;
    if (opts) {
        options = qore_hash_to_bson(opts, xsink);
        if (*xsink) {
            bson_destroy(query);
            return nullptr;
        }
    }

    mongoc_cursor_t* cursor = mongoc_collection_find_with_opts(collection, query, options, nullptr);
    bson_destroy(query);
    if (options) {
        bson_destroy(options);
    }

    return cursor;
}

QoreHashNode* QoreMongoCollection::findOne(const QoreHashNode* filter, ExceptionSink* xsink) const {
    if (!collection) {
        xsink->raiseException("MONGODB-COLLECTION-ERROR", "collection not initialized");
        return nullptr;
    }

    bson_t* query = filter ? qore_hash_to_bson(filter, xsink) : bson_new();
    if (*xsink) {
        return nullptr;
    }

    // Set limit to 1
    bson_t opts = BSON_INITIALIZER;
    BSON_APPEND_INT64(&opts, "limit", 1);

    mongoc_cursor_t* cursor = mongoc_collection_find_with_opts(collection, query, &opts, nullptr);
    bson_destroy(query);
    bson_destroy(&opts);

    const bson_t* doc;
    QoreHashNode* result = nullptr;
    if (mongoc_cursor_next(cursor, &doc)) {
        result = bson_to_qore_hash(doc, xsink);
    }

    bson_error_t error;
    if (mongoc_cursor_error(cursor, &error)) {
        xsink->raiseException("MONGODB-COLLECTION-ERROR", "findOne failed: %s", error.message);
        if (result) {
            result->deref(xsink);
            result = nullptr;
        }
    }

    mongoc_cursor_destroy(cursor);
    return result;
}

QoreHashNode* QoreMongoCollection::updateOne(const QoreHashNode* filter, const QoreHashNode* update,
        const QoreHashNode* opts, ExceptionSink* xsink) const {
    if (!collection) {
        xsink->raiseException("MONGODB-COLLECTION-ERROR", "collection not initialized");
        return nullptr;
    }

    bson_t* selector = qore_hash_to_bson(filter, xsink);
    if (*xsink) {
        return nullptr;
    }

    bson_t* upd = qore_hash_to_bson(update, xsink);
    if (*xsink) {
        bson_destroy(selector);
        return nullptr;
    }

    bson_t* options = nullptr;
    if (opts) {
        options = qore_hash_to_bson(opts, xsink);
        if (*xsink) {
            bson_destroy(selector);
            bson_destroy(upd);
            return nullptr;
        }
    }

    bson_t reply;
    bson_error_t error;
    bool result = mongoc_collection_update_one(collection, selector, upd, options, &reply, &error);
    bson_destroy(selector);
    bson_destroy(upd);
    if (options) {
        bson_destroy(options);
    }

    if (!result) {
        bson_destroy(&reply);
        xsink->raiseException("MONGODB-COLLECTION-ERROR", "updateOne failed: %s", error.message);
        return nullptr;
    }

    QoreHashNode* rv = bson_to_qore_hash(&reply, xsink);
    bson_destroy(&reply);
    return rv;
}

QoreHashNode* QoreMongoCollection::updateMany(const QoreHashNode* filter, const QoreHashNode* update,
        const QoreHashNode* opts, ExceptionSink* xsink) const {
    if (!collection) {
        xsink->raiseException("MONGODB-COLLECTION-ERROR", "collection not initialized");
        return nullptr;
    }

    bson_t* selector = qore_hash_to_bson(filter, xsink);
    if (*xsink) {
        return nullptr;
    }

    bson_t* upd = qore_hash_to_bson(update, xsink);
    if (*xsink) {
        bson_destroy(selector);
        return nullptr;
    }

    bson_t* options = nullptr;
    if (opts) {
        options = qore_hash_to_bson(opts, xsink);
        if (*xsink) {
            bson_destroy(selector);
            bson_destroy(upd);
            return nullptr;
        }
    }

    bson_t reply;
    bson_error_t error;
    bool result = mongoc_collection_update_many(collection, selector, upd, options, &reply, &error);
    bson_destroy(selector);
    bson_destroy(upd);
    if (options) {
        bson_destroy(options);
    }

    if (!result) {
        bson_destroy(&reply);
        xsink->raiseException("MONGODB-COLLECTION-ERROR", "updateMany failed: %s", error.message);
        return nullptr;
    }

    QoreHashNode* rv = bson_to_qore_hash(&reply, xsink);
    bson_destroy(&reply);
    return rv;
}

QoreHashNode* QoreMongoCollection::replaceOne(const QoreHashNode* filter, const QoreHashNode* replacement,
        const QoreHashNode* opts, ExceptionSink* xsink) const {
    if (!collection) {
        xsink->raiseException("MONGODB-COLLECTION-ERROR", "collection not initialized");
        return nullptr;
    }

    bson_t* selector = qore_hash_to_bson(filter, xsink);
    if (*xsink) {
        return nullptr;
    }

    bson_t* repl = qore_hash_to_bson(replacement, xsink);
    if (*xsink) {
        bson_destroy(selector);
        return nullptr;
    }

    bson_t* options = nullptr;
    if (opts) {
        options = qore_hash_to_bson(opts, xsink);
        if (*xsink) {
            bson_destroy(selector);
            bson_destroy(repl);
            return nullptr;
        }
    }

    bson_t reply;
    bson_error_t error;
    bool result = mongoc_collection_replace_one(collection, selector, repl, options, &reply, &error);
    bson_destroy(selector);
    bson_destroy(repl);
    if (options) {
        bson_destroy(options);
    }

    if (!result) {
        bson_destroy(&reply);
        xsink->raiseException("MONGODB-COLLECTION-ERROR", "replaceOne failed: %s", error.message);
        return nullptr;
    }

    QoreHashNode* rv = bson_to_qore_hash(&reply, xsink);
    bson_destroy(&reply);
    return rv;
}

QoreHashNode* QoreMongoCollection::deleteOne(const QoreHashNode* filter, ExceptionSink* xsink) const {
    if (!collection) {
        xsink->raiseException("MONGODB-COLLECTION-ERROR", "collection not initialized");
        return nullptr;
    }

    bson_t* selector = qore_hash_to_bson(filter, xsink);
    if (*xsink) {
        return nullptr;
    }

    bson_t reply;
    bson_error_t error;
    bool result = mongoc_collection_delete_one(collection, selector, nullptr, &reply, &error);
    bson_destroy(selector);

    if (!result) {
        bson_destroy(&reply);
        xsink->raiseException("MONGODB-COLLECTION-ERROR", "deleteOne failed: %s", error.message);
        return nullptr;
    }

    QoreHashNode* rv = bson_to_qore_hash(&reply, xsink);
    bson_destroy(&reply);
    return rv;
}

QoreHashNode* QoreMongoCollection::deleteMany(const QoreHashNode* filter, ExceptionSink* xsink) const {
    if (!collection) {
        xsink->raiseException("MONGODB-COLLECTION-ERROR", "collection not initialized");
        return nullptr;
    }

    bson_t* selector = qore_hash_to_bson(filter, xsink);
    if (*xsink) {
        return nullptr;
    }

    bson_t reply;
    bson_error_t error;
    bool result = mongoc_collection_delete_many(collection, selector, nullptr, &reply, &error);
    bson_destroy(selector);

    if (!result) {
        bson_destroy(&reply);
        xsink->raiseException("MONGODB-COLLECTION-ERROR", "deleteMany failed: %s", error.message);
        return nullptr;
    }

    QoreHashNode* rv = bson_to_qore_hash(&reply, xsink);
    bson_destroy(&reply);
    return rv;
}

int64_t QoreMongoCollection::countDocuments(const QoreHashNode* filter, ExceptionSink* xsink) const {
    if (!collection) {
        xsink->raiseException("MONGODB-COLLECTION-ERROR", "collection not initialized");
        return -1;
    }

    bson_t* query = filter ? qore_hash_to_bson(filter, xsink) : bson_new();
    if (*xsink) {
        return -1;
    }

    bson_error_t error;
    int64_t count = mongoc_collection_count_documents(collection, query, nullptr, nullptr, nullptr, &error);
    bson_destroy(query);

    if (count < 0) {
        xsink->raiseException("MONGODB-COLLECTION-ERROR", "countDocuments failed: %s", error.message);
    }

    return count;
}

bool QoreMongoCollection::drop(ExceptionSink* xsink) {
    if (!collection) {
        xsink->raiseException("MONGODB-COLLECTION-ERROR", "collection not initialized");
        return false;
    }

    bson_error_t error;
    if (!mongoc_collection_drop(collection, &error)) {
        xsink->raiseException("MONGODB-COLLECTION-ERROR", "drop failed: %s", error.message);
        return false;
    }

    return true;
}

mongoc_cursor_t* QoreMongoCollection::aggregate(const QoreListNode* pipeline, ExceptionSink* xsink) const {
    if (!collection) {
        xsink->raiseException("MONGODB-COLLECTION-ERROR", "collection not initialized");
        return nullptr;
    }

    // Build pipeline array
    bson_t pipe = BSON_INITIALIZER;
    size_t n = pipeline->size();
    for (size_t i = 0; i < n; i++) {
        QoreValue v = pipeline->retrieveEntry(i);
        if (v.getType() != NT_HASH) {
            bson_destroy(&pipe);
            xsink->raiseException("MONGODB-COLLECTION-ERROR", "pipeline stage at index %zd is not a hash", i);
            return nullptr;
        }
        bson_t* stage = qore_hash_to_bson(v.get<const QoreHashNode>(), xsink);
        if (*xsink) {
            bson_destroy(&pipe);
            return nullptr;
        }
        QoreStringMaker idx("%zd", i);
        bson_append_document(&pipe, idx.c_str(), -1, stage);
        bson_destroy(stage);
    }

    mongoc_cursor_t* cursor = mongoc_collection_aggregate(collection, MONGOC_QUERY_NONE, &pipe, nullptr, nullptr);
    bson_destroy(&pipe);

    return cursor;
}
