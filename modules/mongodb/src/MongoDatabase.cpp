/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    MongoDatabase.cpp

    Qore mongodb module - MongoDatabase class implementation

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

#include "QC_MongoDatabase.h"
#include "bson_conversion.h"

// CID_MONGODATABASE and QC_MONGODATABASE are defined in QC_MongoDatabase.cpp (generated from .qpp)

QoreHashNode* QoreMongoDatabase::runCommand(const QoreHashNode* command, ExceptionSink* xsink) const {
    if (!database) {
        xsink->raiseException("MONGODB-DATABASE-ERROR", "database not initialized");
        return nullptr;
    }

    bson_t* cmd = qore_hash_to_bson(command, xsink);
    if (*xsink) {
        return nullptr;
    }

    bson_t reply;
    bson_error_t error;
    bool result = mongoc_database_command_simple(database, cmd, nullptr, &reply, &error);
    bson_destroy(cmd);

    if (!result) {
        bson_destroy(&reply);
        xsink->raiseException("MONGODB-DATABASE-ERROR", "command failed: %s", error.message);
        return nullptr;
    }

    QoreHashNode* rv = bson_to_qore_hash(&reply, xsink);
    bson_destroy(&reply);
    return rv;
}
