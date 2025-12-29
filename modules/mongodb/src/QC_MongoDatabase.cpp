/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_MongoDatabase.qpp

    Qore mongodb module - MongoDatabase class definition

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
#include "QC_MongoDatabase.h"
#include "QC_MongoCollection.h"
/* Qore class mongodb::MongoDatabase */

qore_classid_t CID_MONGODATABASE;
QoreClass* QC_MONGODATABASE;

// MongoCollection MongoDatabase::createCollection(string name){}
static QoreValue MongoDatabase_createCollection_Vs(QoreObject* self, QoreMongoDatabase* database, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
    const QoreStringNode* name = HARD_QORE_VALUE_STRING(args, 0);
# 115 "QC_MongoDatabase.qpp"
    mongoc_collection_t* coll = database->createCollection(name->c_str(), xsink);
    if (*xsink) {
        return QoreValue();
    }
    return new QoreObject(QC_MONGOCOLLECTION, getProgram(),
        new QoreMongoCollection(coll, database->getClient()->refSelf()));
    return QoreValue();
}

// bool MongoDatabase::drop(){}
static QoreValue MongoDatabase_drop(QoreObject* self, QoreMongoDatabase* database, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
# 136 "QC_MongoDatabase.qpp"
    return database->drop(xsink);
}

// MongoCollection MongoDatabase::getCollection(string name){}
static QoreValue MongoDatabase_getCollection_Vs(QoreObject* self, QoreMongoDatabase* database, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
    const QoreStringNode* name = HARD_QORE_VALUE_STRING(args, 0);
# 94 "QC_MongoDatabase.qpp"
    mongoc_collection_t* coll = database->getCollection(name->c_str());
    if (!coll) {
        xsink->raiseException("MONGODB-DATABASE-ERROR", "failed to get collection '%s'", name->c_str());
        return QoreValue();
    }
    return new QoreObject(QC_MONGOCOLLECTION, getProgram(),
        new QoreMongoCollection(coll, database->getClient()->refSelf()));
    return QoreValue();
}

// *string MongoDatabase::getName(){}
static QoreValue MongoDatabase_getName(QoreObject* self, QoreMongoDatabase* database, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
# 64 "QC_MongoDatabase.qpp"
    return database->getName();
}

// list<string> MongoDatabase::listCollectionNames(){}
static QoreValue MongoDatabase_listCollectionNames(QoreObject* self, QoreMongoDatabase* database, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
# 81 "QC_MongoDatabase.qpp"
    return database->listCollectionNames(xsink);
}

// hash<auto> MongoDatabase::runCommand(hash<auto> command){}
static QoreValue MongoDatabase_runCommand_C10hash_auto_(QoreObject* self, QoreMongoDatabase* database, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
    const QoreHashNode* command = HARD_QORE_VALUE_HASH(args, 0);
# 152 "QC_MongoDatabase.qpp"
    return database->runCommand(command, xsink);
}

DLLLOCAL void preinitMongoDatabaseClass() {
    QC_MONGODATABASE = new QoreBuiltinClass("MongoDatabase", "::mongodb::MongoDatabase", QDOM_DEFAULT);
    CID_MONGODATABASE = QC_MONGODATABASE->getID();
    QC_MONGODATABASE->setSystem();
}

DLLLOCAL QoreClass* initMongoDatabaseClass(QoreNamespace& ns) {
    if (!QC_MONGODATABASE)
        preinitMongoDatabaseClass();

    QC_MONGODATABASE->setFinal();

    // MongoCollection MongoDatabase::createCollection(string name){}
    QC_MONGODATABASE->addMethod("createCollection", (q_method_n_t)MongoDatabase_createCollection_Vs, Public, QCF_NO_FLAGS, QDOM_DEFAULT, QC_MONGOCOLLECTION->getTypeInfo(), 1, stringTypeInfo, QORE_PARAM_NO_ARG, "name");

    // bool MongoDatabase::drop(){}
    QC_MONGODATABASE->addMethod("drop", (q_method_n_t)MongoDatabase_drop, Public, QCF_NO_FLAGS, QDOM_DEFAULT, boolTypeInfo);

    // MongoCollection MongoDatabase::getCollection(string name){}
    QC_MONGODATABASE->addMethod("getCollection", (q_method_n_t)MongoDatabase_getCollection_Vs, Public, QCF_NO_FLAGS, QDOM_DEFAULT, QC_MONGOCOLLECTION->getTypeInfo(), 1, stringTypeInfo, QORE_PARAM_NO_ARG, "name");

    // *string MongoDatabase::getName(){}
    QC_MONGODATABASE->addMethod("getName", (q_method_n_t)MongoDatabase_getName, Public, QCF_CONSTANT, QDOM_DEFAULT, stringOrNothingTypeInfo);

    // list<string> MongoDatabase::listCollectionNames(){}
    QC_MONGODATABASE->addMethod("listCollectionNames", (q_method_n_t)MongoDatabase_listCollectionNames, Public, QCF_NO_FLAGS, QDOM_DEFAULT, qore_get_complex_list_type(stringTypeInfo));

    // hash<auto> MongoDatabase::runCommand(hash<auto> command){}
    QC_MONGODATABASE->addMethod("runCommand", (q_method_n_t)MongoDatabase_runCommand_C10hash_auto_, Public, QCF_NO_FLAGS, QDOM_DEFAULT, autoHashTypeInfo, 1, autoHashTypeInfo, QORE_PARAM_NO_ARG, "command");

    return QC_MONGODATABASE;
}
