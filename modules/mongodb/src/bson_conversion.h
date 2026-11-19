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

#include <memory>

//! Returns UTF-8 text from libbson or libmongoc as a string tagged UTF-8
/** @param p the text
    @param len the size of the text in bytes
    @param where the function or method that takes the text, for the exception (ex: \c "MongoDB")
    @param what what the text is, for the exception (ex: \c "BSON string")
    @param xsink for the \c INVALID-ENCODING exception raised if the text is not valid UTF-8

    @return the string, or nullptr if an exception was raised
*/
DLLLOCAL QoreStringNode* mongo_utf8_string(const char* p, size_t len, const char* where, const char* what,
        ExceptionSink* xsink);

//! Returns a list of the UTF-8 names of a NULL-terminated array of names from libmongoc as strings tagged UTF-8
/** @param names the names
    @param where the method that takes the names, for the exception (ex: \c "MongoClient::listDatabaseNames()")
    @param what what a name is, for the exception (ex: \c "database name")
    @param xsink for the \c INVALID-ENCODING exception raised if a name is not valid UTF-8

    @return the list, or nullptr if an exception was raised
*/
DLLLOCAL QoreListNode* mongo_utf8_string_list(char** names, const char* where, const char* what,
        ExceptionSink* xsink);

//! Sets a UTF-8 key of a hash, converted to the default encoding (the encoding of hash keys)
/** @param h the hash
    @param key the UTF-8 key, which must be valid UTF-8
    @param len the size of the key in bytes
    @param value the value, which is released if the key cannot be set
    @param xsink for an exception converting the key

    @return 0 for OK, -1 for an error
*/
DLLLOCAL int mongo_set_utf8_key(QoreHashNode* h, const char* key, size_t len, QoreValue value, ExceptionSink* xsink);

//! Checks a string given to libmongoc in its encoding and converts it to UTF-8, the encoding of the driver
/** @param str the string
    @param utf8 set to the UTF-8 string
    @param where the method that takes the string, for the exception (ex: \c "MongoClient::getDatabase()")
    @param what what the string is, for the exception (ex: \c "database name")
    @param xsink for the \c INVALID-ENCODING exception raised if the string is not valid in its encoding, or an
    exception converting it

    @return 0 for OK, -1 if an exception was raised
*/
DLLLOCAL int mongo_utf8_arg(const QoreString& str, TempEncodingHelper& utf8, const char* where, const char* what,
        ExceptionSink* xsink);

//! A hash key (in the default encoding) as a UTF-8 key for libbson
class MongoUtf8Key {
public:
    //! converts the key; check the object with operator bool() for an exception
    DLLLOCAL MongoUtf8Key(const char* key, ExceptionSink* xsink);

    //! returns true if the key could be converted
    DLLLOCAL explicit operator bool() const {
        return key != nullptr;
    }

    //! returns the UTF-8 key
    DLLLOCAL const char* c_str() const {
        return key;
    }

private:
    const char* key;
    std::unique_ptr<QoreString> converted;
};

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
