/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_MongoDatabase.h

    Qore mongodb module - MongoDatabase class

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

#ifndef _QORE_MODULE_MONGODB_QC_MONGODATABASE_H
#define _QORE_MODULE_MONGODB_QC_MONGODATABASE_H

#include <qore/Qore.h>

#include <mongoc/mongoc.h>
#include "QC_MongoClient.h"

DLLEXPORT extern qore_classid_t CID_MONGODATABASE;
DLLLOCAL extern QoreClass* QC_MONGODATABASE;

DLLLOCAL void preinitMongoDatabaseClass();
DLLLOCAL QoreClass* initMongoDatabaseClass(QoreNamespace& ns);

//! MongoDB database wrapper class
class QoreMongoDatabase : public AbstractPrivateData {
public:
    //! Create from an existing mongoc_database_t
    DLLLOCAL QoreMongoDatabase(mongoc_database_t* db, QoreMongoClient* c)
        : database(db), client(c) {
    }

    //! Destructor
    DLLLOCAL virtual ~QoreMongoDatabase() {
        if (database) {
            mongoc_database_destroy(database);
        }
        if (client) {
            client->deref(nullptr);
        }
    }

    //! Get the underlying mongoc_database_t
    DLLLOCAL mongoc_database_t* getDatabase() const {
        return database;
    }

    //! Get the database name
    DLLLOCAL QoreStringNode* getName() const {
        if (!database) {
            return nullptr;
        }
        return new QoreStringNode(mongoc_database_get_name(database));
    }

    //! Get a collection handle
    DLLLOCAL mongoc_collection_t* getCollection(const char* name) const {
        return database ? mongoc_database_get_collection(database, name) : nullptr;
    }

    //! List collection names
    DLLLOCAL QoreListNode* listCollectionNames(ExceptionSink* xsink) const {
        if (!database) {
            xsink->raiseException("MONGODB-DATABASE-ERROR", "database not initialized");
            return nullptr;
        }
        bson_error_t error;
        char** names = mongoc_database_get_collection_names_with_opts(database, nullptr, &error);
        if (!names) {
            xsink->raiseException("MONGODB-DATABASE-ERROR", "failed to list collections: %s", error.message);
            return nullptr;
        }
        ReferenceHolder<QoreListNode> list(new QoreListNode(stringTypeInfo), xsink);
        for (int i = 0; names[i]; i++) {
            list->push(new QoreStringNode(names[i]), xsink);
        }
        bson_strfreev(names);
        return list.release();
    }

    //! Create a collection
    DLLLOCAL mongoc_collection_t* createCollection(const char* name, ExceptionSink* xsink) const {
        if (!database) {
            xsink->raiseException("MONGODB-DATABASE-ERROR", "database not initialized");
            return nullptr;
        }
        bson_error_t error;
        mongoc_collection_t* coll = mongoc_database_create_collection(database, name, nullptr, &error);
        if (!coll) {
            xsink->raiseException("MONGODB-DATABASE-ERROR", "failed to create collection '%s': %s",
                name, error.message);
        }
        return coll;
    }

    //! Drop the database
    DLLLOCAL bool drop(ExceptionSink* xsink) {
        if (!database) {
            xsink->raiseException("MONGODB-DATABASE-ERROR", "database not initialized");
            return false;
        }
        bson_error_t error;
        if (!mongoc_database_drop(database, &error)) {
            xsink->raiseException("MONGODB-DATABASE-ERROR", "failed to drop database: %s", error.message);
            return false;
        }
        return true;
    }

    //! Run a command
    DLLLOCAL QoreHashNode* runCommand(const QoreHashNode* command, ExceptionSink* xsink) const;

    //! Get the client reference
    DLLLOCAL QoreMongoClient* getClient() const {
        return client;
    }

    //! Reference self
    DLLLOCAL QoreMongoDatabase* refSelf() const {
        ref();
        return const_cast<QoreMongoDatabase*>(this);
    }

private:
    mongoc_database_t* database = nullptr;
    QoreMongoClient* client = nullptr;
};

#endif // _QORE_MODULE_MONGODB_QC_MONGODATABASE_H
