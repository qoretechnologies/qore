/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    bson_conversion.h

    Qore mongodb module - BSON to/from Qore value conversion

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

#ifndef _QORE_MODULE_MONGODB_BSON_CONVERSION_H
#define _QORE_MODULE_MONGODB_BSON_CONVERSION_H

#include <qore/Qore.h>

#include <mongoc/mongoc.h>
#include <bson/bson.h>

//! Convert a BSON document to a Qore hash
/** @param doc the BSON document to convert
    @param xsink exception sink for error handling
    @return the converted Qore hash, or nullptr on error
*/
DLLLOCAL QoreHashNode* bson_to_qore_hash(const bson_t* doc, ExceptionSink* xsink);

//! Convert a BSON value to a Qore value
/** @param value the BSON value to convert
    @param xsink exception sink for error handling
    @return the converted Qore value
*/
DLLLOCAL QoreValue bson_value_to_qore(const bson_value_t* value, ExceptionSink* xsink);

//! Convert a Qore hash to a BSON document
/** @param hash the Qore hash to convert
    @param xsink exception sink for error handling
    @return the converted BSON document (caller must call bson_destroy), or nullptr on error
*/
DLLLOCAL bson_t* qore_hash_to_bson(const QoreHashNode* hash, ExceptionSink* xsink);

//! Convert a Qore value to a BSON value and append to document
/** @param doc the BSON document to append to
    @param key the key name
    @param value the Qore value to convert
    @param xsink exception sink for error handling
    @return 0 on success, -1 on error
*/
DLLLOCAL int qore_value_to_bson_append(bson_t* doc, const char* key, const QoreValue& value, ExceptionSink* xsink);

//! Convert a Qore list to a BSON array and append to document
/** @param doc the BSON document to append to
    @param key the key name
    @param list the Qore list to convert
    @param xsink exception sink for error handling
    @return 0 on success, -1 on error
*/
DLLLOCAL int qore_list_to_bson_array(bson_t* doc, const char* key, const QoreListNode* list, ExceptionSink* xsink);

#endif // _QORE_MODULE_MONGODB_BSON_CONVERSION_H
