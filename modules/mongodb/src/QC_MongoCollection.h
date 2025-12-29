/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_MongoCollection.h

    Qore mongodb module - MongoCollection class

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

#ifndef _QORE_MODULE_MONGODB_QC_MONGOCOLLECTION_H
#define _QORE_MODULE_MONGODB_QC_MONGOCOLLECTION_H

#include <qore/Qore.h>

#include <mongoc/mongoc.h>
#include "QC_MongoClient.h"

DLLEXPORT extern qore_classid_t CID_MONGOCOLLECTION;
DLLLOCAL extern QoreClass* QC_MONGOCOLLECTION;

DLLLOCAL void preinitMongoCollectionClass();
DLLLOCAL QoreClass* initMongoCollectionClass(QoreNamespace& ns);

//! MongoDB collection wrapper class
class QoreMongoCollection : public AbstractPrivateData {
public:
    //! Create from an existing mongoc_collection_t
    DLLLOCAL QoreMongoCollection(mongoc_collection_t* coll, QoreMongoClient* c)
        : collection(coll), client(c) {
    }

    //! Destructor
    DLLLOCAL virtual ~QoreMongoCollection() {
        if (collection) {
            mongoc_collection_destroy(collection);
        }
        if (client) {
            client->deref(nullptr);
        }
    }

    //! Get the underlying mongoc_collection_t
    DLLLOCAL mongoc_collection_t* getCollection() const {
        return collection;
    }

    //! Get the collection name
    DLLLOCAL QoreStringNode* getName() const {
        if (!collection) {
            return nullptr;
        }
        return new QoreStringNode(mongoc_collection_get_name(collection));
    }

    //! Insert one document
    DLLLOCAL QoreHashNode* insertOne(const QoreHashNode* document, ExceptionSink* xsink) const;

    //! Insert many documents
    DLLLOCAL QoreHashNode* insertMany(const QoreListNode* documents, ExceptionSink* xsink) const;

    //! Find documents
    DLLLOCAL mongoc_cursor_t* find(const QoreHashNode* filter, const QoreHashNode* opts, ExceptionSink* xsink) const;

    //! Find one document
    DLLLOCAL QoreHashNode* findOne(const QoreHashNode* filter, ExceptionSink* xsink) const;

    //! Update one document
    DLLLOCAL QoreHashNode* updateOne(const QoreHashNode* filter, const QoreHashNode* update,
        const QoreHashNode* opts, ExceptionSink* xsink) const;

    //! Update many documents
    DLLLOCAL QoreHashNode* updateMany(const QoreHashNode* filter, const QoreHashNode* update,
        const QoreHashNode* opts, ExceptionSink* xsink) const;

    //! Replace one document
    DLLLOCAL QoreHashNode* replaceOne(const QoreHashNode* filter, const QoreHashNode* replacement,
        const QoreHashNode* opts, ExceptionSink* xsink) const;

    //! Delete one document
    DLLLOCAL QoreHashNode* deleteOne(const QoreHashNode* filter, ExceptionSink* xsink) const;

    //! Delete many documents
    DLLLOCAL QoreHashNode* deleteMany(const QoreHashNode* filter, ExceptionSink* xsink) const;

    //! Count documents
    DLLLOCAL int64_t countDocuments(const QoreHashNode* filter, ExceptionSink* xsink) const;

    //! Drop the collection
    DLLLOCAL bool drop(ExceptionSink* xsink);

    //! Run an aggregation pipeline
    DLLLOCAL mongoc_cursor_t* aggregate(const QoreListNode* pipeline, ExceptionSink* xsink) const;

    //! Reference self
    DLLLOCAL QoreMongoCollection* refSelf() const {
        ref();
        return const_cast<QoreMongoCollection*>(this);
    }

private:
    mongoc_collection_t* collection = nullptr;
    QoreMongoClient* client = nullptr;
};

#endif // _QORE_MODULE_MONGODB_QC_MONGOCOLLECTION_H
