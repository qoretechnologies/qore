/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    mongodb-module.cpp

    Qore mongodb module

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

#include "qore/Qore.h"

#include <mongoc/mongoc.h>

#include "QC_ObjectId.h"
#include "QC_MongoClient.h"
#include "QC_MongoDatabase.h"
#include "QC_MongoCollection.h"
#include "QC_MongoCursor.h"

static void mongodb_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink);
static void mongodb_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink);
static void mongodb_module_delete();

extern "C" DLLEXPORT void mongodb_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "mongodb";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "MongoDB client module for Qore";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "http://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = mongodb_module_init;
    mod_info.ns_init = mongodb_module_ns_init;
    mod_info.del = mongodb_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
    mod_info.functional_domains = QDOM_DATABASE | QDOM_NETWORK;
}

QoreNamespace MongoDBNS("Qore::mongodb");

static bool mongoc_initialized = false;

static void mongodb_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    // Initialize libmongoc
    mongoc_init();
    mongoc_initialized = true;
    qore_mongo_set_log_handler();

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
}

static void mongodb_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    qns->addNamespace(MongoDBNS.copy());
}

static void mongodb_module_delete() {
    MongoDBNS.clear(nullptr);
    if (mongoc_initialized) {
        mongoc_cleanup();
        mongoc_initialized = false;
    }
}
