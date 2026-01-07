/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_MongoClient.h

    Qore mongodb module - MongoClient class

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

#ifndef _QORE_MODULE_MONGODB_QC_MONGOCLIENT_H
#define _QORE_MODULE_MONGODB_QC_MONGOCLIENT_H

#include <qore/Qore.h>

#include <mongoc/mongoc.h>

#include "QoreMongoStream.h"

DLLEXPORT extern qore_classid_t CID_MONGOCLIENT;
DLLLOCAL extern QoreClass* QC_MONGOCLIENT;

DLLLOCAL void preinitMongoClientClass();
DLLLOCAL QoreClass* initMongoClientClass(QoreNamespace& ns);

//! MongoDB client wrapper class
class QoreMongoClient : public AbstractPrivateData {
public:
    //! Create a MongoDB client from a URI string
    DLLLOCAL QoreMongoClient(const char* uri_string, ExceptionSink* xsink) {
        bson_error_t error;
        uri = mongoc_uri_new_with_error(uri_string, &error);
        if (!uri) {
            xsink->raiseException("MONGODB-CLIENT-ERROR", "failed to parse MongoDB URI: %s", error.message);
            return;
        }
        client = mongoc_client_new_from_uri(uri);
        if (!client) {
            mongoc_uri_destroy(uri);
            uri = nullptr;
            xsink->raiseException("MONGODB-CLIENT-ERROR", "failed to create MongoDB client");
            return;
        }
        // Set up interruptible streams for sandbox support
        qore_mongo_setup_interruptible_streams(client);
        // Set application name
        mongoc_client_set_appname(client, "qore-mongodb");
    }

    //! Destructor
    DLLLOCAL virtual ~QoreMongoClient() {
        if (client) {
            mongoc_client_destroy(client);
        }
        if (uri) {
            mongoc_uri_destroy(uri);
        }
    }

    //! Get the underlying mongoc_client_t
    DLLLOCAL mongoc_client_t* getClient() const {
        return client;
    }

    //! Get a database handle
    DLLLOCAL mongoc_database_t* getDatabase(const char* name) const {
        return client ? mongoc_client_get_database(client, name) : nullptr;
    }

    //! List database names
    DLLLOCAL QoreListNode* listDatabaseNames(ExceptionSink* xsink) const {
        if (!client) {
            xsink->raiseException("MONGODB-CLIENT-ERROR", "client not connected");
            return nullptr;
        }
        bson_error_t error;
        char** names = mongoc_client_get_database_names_with_opts(client, nullptr, &error);
        if (!names) {
            xsink->raiseException("MONGODB-CLIENT-ERROR", "failed to list databases: %s", error.message);
            return nullptr;
        }
        ReferenceHolder<QoreListNode> list(new QoreListNode(stringTypeInfo), xsink);
        for (int i = 0; names[i]; i++) {
            list->push(new QoreStringNode(names[i]), xsink);
        }
        bson_strfreev(names);
        return list.release();
    }

    //! Ping the server
    DLLLOCAL bool ping(ExceptionSink* xsink) const {
        if (!client) {
            xsink->raiseException("MONGODB-CLIENT-ERROR", "client not connected");
            return false;
        }
        bson_t* cmd = BCON_NEW("ping", BCON_INT32(1));
        bson_t reply;
        bson_error_t error;
        bool result = mongoc_client_command_simple(client, "admin", cmd, nullptr, &reply, &error);
        bson_destroy(cmd);
        bson_destroy(&reply);
        if (!result) {
            xsink->raiseException("MONGODB-CLIENT-ERROR", "ping failed: %s", error.message);
        }
        return result;
    }

    //! Get the URI string
    DLLLOCAL QoreStringNode* getUri() const {
        if (!uri) {
            return nullptr;
        }
        return new QoreStringNode(mongoc_uri_get_string(uri));
    }

    //! Reference self
    DLLLOCAL QoreMongoClient* refSelf() const {
        ref();
        return const_cast<QoreMongoClient*>(this);
    }

private:
    mongoc_client_t* client = nullptr;
    mongoc_uri_t* uri = nullptr;
};

#endif // _QORE_MODULE_MONGODB_QC_MONGOCLIENT_H
