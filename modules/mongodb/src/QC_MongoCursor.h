/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_MongoCursor.h

    Qore mongodb module - MongoCursor class

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

#ifndef _QORE_MODULE_MONGODB_QC_MONGOCURSOR_H
#define _QORE_MODULE_MONGODB_QC_MONGOCURSOR_H

#include <qore/Qore.h>

#include <mongoc/mongoc.h>

DLLEXPORT extern qore_classid_t CID_MONGOCURSOR;
DLLLOCAL extern QoreClass* QC_MONGOCURSOR;

DLLLOCAL void preinitMongoCursorClass();
DLLLOCAL QoreClass* initMongoCursorClass(QoreNamespace& ns);

//! MongoDB cursor wrapper class
class QoreMongoCursor : public AbstractPrivateData {
public:
    //! Create from an existing mongoc_cursor_t
    DLLLOCAL QoreMongoCursor(mongoc_cursor_t* c) : cursor(c) {
    }

    //! Destructor
    DLLLOCAL virtual ~QoreMongoCursor() {
        if (cursor) {
            mongoc_cursor_destroy(cursor);
        }
    }

    //! Get the underlying mongoc_cursor_t
    DLLLOCAL mongoc_cursor_t* getCursor() const {
        return cursor;
    }

    //! Get the next document from the cursor
    DLLLOCAL QoreHashNode* next(ExceptionSink* xsink);

    //! Check if there are more documents
    DLLLOCAL bool more(ExceptionSink* xsink) const;

    //! Get all remaining documents as a list
    DLLLOCAL QoreListNode* toList(ExceptionSink* xsink);

    //! Close the cursor
    DLLLOCAL void close() {
        if (cursor) {
            mongoc_cursor_destroy(cursor);
            cursor = nullptr;
        }
    }

private:
    mongoc_cursor_t* cursor = nullptr;
};

#endif // _QORE_MODULE_MONGODB_QC_MONGOCURSOR_H
