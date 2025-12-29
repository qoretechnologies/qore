/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    mongodb-module.cpp

    Qore mongodb module

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

#include <mongoc/mongoc.h>

#include "QC_ObjectId.h"
#include "QC_MongoClient.h"
#include "QC_MongoDatabase.h"
#include "QC_MongoCollection.h"
#include "QC_MongoCursor.h"

QoreStringNode* mongodb_module_init();
void mongodb_module_ns_init(QoreNamespace* rns, QoreNamespace* qns);
void mongodb_module_delete();

// Qore module symbols
DLLEXPORT char qore_module_name[] = "mongodb";
DLLEXPORT char qore_module_version[] = PACKAGE_VERSION;
DLLEXPORT char qore_module_description[] = "MongoDB client module for Qore";
DLLEXPORT char qore_module_author[] = "Qore Technologies, s.r.o.";
DLLEXPORT char qore_module_url[] = "http://qore.org";
DLLEXPORT int qore_module_api_major = QORE_MODULE_API_MAJOR;
DLLEXPORT int qore_module_api_minor = QORE_MODULE_API_MINOR;
DLLEXPORT qore_module_init_t qore_module_init = mongodb_module_init;
DLLEXPORT qore_module_ns_init_t qore_module_ns_init = mongodb_module_ns_init;
DLLEXPORT qore_module_delete_t qore_module_delete = mongodb_module_delete;
DLLEXPORT qore_license_t qore_module_license = QL_MIT;
DLLEXPORT char qore_module_license_str[] = "MIT";

QoreNamespace MongoDBNS("Qore::mongodb");

static bool mongoc_initialized = false;

QoreStringNode* mongodb_module_init() {
    // Initialize libmongoc
    mongoc_init();
    mongoc_initialized = true;

    // Pre-initialize all classes first (creates class objects without methods)
    // This is needed because classes reference each other's type info
    preinitObjectIdClass();
    preinitMongoClientClass();
    preinitMongoDatabaseClass();
    preinitMongoCollectionClass();
    preinitMongoCursorClass();

    // Now add classes to namespace (adds methods that may reference other classes)
    MongoDBNS.addSystemClass(initObjectIdClass(MongoDBNS));
    MongoDBNS.addSystemClass(initMongoClientClass(MongoDBNS));
    MongoDBNS.addSystemClass(initMongoDatabaseClass(MongoDBNS));
    MongoDBNS.addSystemClass(initMongoCollectionClass(MongoDBNS));
    MongoDBNS.addSystemClass(initMongoCursorClass(MongoDBNS));

    return nullptr;
}

void mongodb_module_ns_init(QoreNamespace* rns, QoreNamespace* qns) {
    qns->addNamespace(MongoDBNS.copy());
}

void mongodb_module_delete() {
    MongoDBNS.clear(nullptr);
    if (mongoc_initialized) {
        mongoc_cleanup();
        mongoc_initialized = false;
    }
}
