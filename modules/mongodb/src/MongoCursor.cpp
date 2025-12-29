/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    MongoCursor.cpp

    Qore mongodb module - MongoCursor class implementation

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

#include "QC_MongoCursor.h"
#include "bson_conversion.h"

// CID_MONGOCURSOR and QC_MONGOCURSOR are defined in QC_MongoCursor.cpp (generated from .qpp)

QoreHashNode* QoreMongoCursor::next(ExceptionSink* xsink) {
    if (!cursor) {
        return nullptr;
    }

    const bson_t* doc;
    if (!mongoc_cursor_next(cursor, &doc)) {
        bson_error_t error;
        if (mongoc_cursor_error(cursor, &error)) {
            xsink->raiseException("MONGODB-CURSOR-ERROR", "cursor error: %s", error.message);
        }
        return nullptr;
    }

    return bson_to_qore_hash(doc, xsink);
}

bool QoreMongoCursor::more(ExceptionSink* xsink) const {
    if (!cursor) {
        return false;
    }

    return mongoc_cursor_more(cursor);
}

QoreListNode* QoreMongoCursor::toList(ExceptionSink* xsink) {
    if (!cursor) {
        xsink->raiseException("MONGODB-CURSOR-ERROR", "cursor not initialized");
        return nullptr;
    }

    ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), xsink);

    const bson_t* doc;
    while (mongoc_cursor_next(cursor, &doc)) {
        QoreHashNode* h = bson_to_qore_hash(doc, xsink);
        if (*xsink) {
            return nullptr;
        }
        list->push(h, xsink);
    }

    bson_error_t error;
    if (mongoc_cursor_error(cursor, &error)) {
        xsink->raiseException("MONGODB-CURSOR-ERROR", "cursor error: %s", error.message);
        return nullptr;
    }

    return list.release();
}
