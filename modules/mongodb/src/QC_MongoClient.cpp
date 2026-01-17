/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_MongoClient.qpp

    Qore mongodb module - MongoClient class definition

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
#include "QC_MongoClient.h"
#include "QC_MongoDatabase.h"
/* Qore class mongodb::MongoClient */

qore_classid_t CID_MONGOCLIENT;
QoreClass* QC_MONGOCLIENT;

// MongoClient::constructor(string uri) {}
static void MongoClient_constructor_Vs(QoreObject* self, const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    const QoreStringNode* uri = HARD_QORE_VALUE_STRING(args, 0);
# 75 "QC_MongoClient.qpp"
SimpleRefHolder<QoreMongoClient> holder(new QoreMongoClient(uri->c_str(), xsink));
    if (*xsink) {
        return;
    }
    self->setPrivate(CID_MONGOCLIENT, holder.release());
}

// MongoDatabase MongoClient::getDatabase(string name){}
static QoreValue MongoClient_getDatabase_Vs(QoreObject* self, QoreMongoClient* client, const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    const QoreStringNode* name = HARD_QORE_VALUE_STRING(args, 0);
# 125 "QC_MongoClient.qpp"
    mongoc_database_t* db = client->getDatabase(name->c_str());
    if (!db) {
        xsink->raiseException("MONGODB-CLIENT-ERROR", "failed to get database '%s'", name->c_str());
        return QoreValue();
    }
    return new QoreObject(QC_MONGODATABASE, getProgram(), new QoreMongoDatabase(db, client->refSelf()));
}

// *string MongoClient::getUri(){}
static QoreValue MongoClient_getUri(QoreObject* self, QoreMongoClient* client, const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
# 142 "QC_MongoClient.qpp"
    return client->getUri();
}

// list<string> MongoClient::listDatabaseNames(){}
static QoreValue MongoClient_listDatabaseNames(QoreObject* self, QoreMongoClient* client, const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
# 112 "QC_MongoClient.qpp"
    return client->listDatabaseNames(xsink);
}

// bool MongoClient::ping(){}
static QoreValue MongoClient_ping(QoreObject* self, QoreMongoClient* client, const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
# 95 "QC_MongoClient.qpp"
    return client->ping(xsink);
}

DLLLOCAL void preinitMongoClientClass() {
    QC_MONGOCLIENT = new QoreBuiltinClass("MongoClient", "::mongodb::MongoClient", QDOM_DEFAULT);
    CID_MONGOCLIENT = QC_MONGOCLIENT->getID();
    QC_MONGOCLIENT->setSystem();
}

DLLLOCAL QoreClass* initMongoClientClass(QoreNamespace& ns) {
    if (!QC_MONGOCLIENT)
        preinitMongoClientClass();

    QC_MONGOCLIENT->setFinal();

    // MongoClient::constructor(string uri) {}
    QC_MONGOCLIENT->addConstructor(MongoClient_constructor_Vs, Public, QCF_NO_FLAGS, QDOM_DEFAULT, 1, stringTypeInfo, QORE_PARAM_NO_ARG, "uri");

    // MongoDatabase MongoClient::getDatabase(string name){}
    QC_MONGOCLIENT->addMethod("getDatabase", (q_method_t)MongoClient_getDatabase_Vs, Public, QCF_NO_FLAGS, QDOM_DEFAULT, QC_MONGODATABASE->getTypeInfo(), 1, stringTypeInfo, QORE_PARAM_NO_ARG, "name");

    // *string MongoClient::getUri(){}
    QC_MONGOCLIENT->addMethod("getUri", (q_method_t)MongoClient_getUri, Public, QCF_CONSTANT, QDOM_DEFAULT, stringOrNothingTypeInfo);

    // list<string> MongoClient::listDatabaseNames(){}
    QC_MONGOCLIENT->addMethod("listDatabaseNames", (q_method_t)MongoClient_listDatabaseNames, Public, QCF_NO_FLAGS, QDOM_DEFAULT, qore_get_complex_list_type(stringTypeInfo));

    // bool MongoClient::ping(){}
    QC_MONGOCLIENT->addMethod("ping", (q_method_t)MongoClient_ping, Public, QCF_NO_FLAGS, QDOM_DEFAULT, boolTypeInfo);

    return QC_MONGOCLIENT;
}
