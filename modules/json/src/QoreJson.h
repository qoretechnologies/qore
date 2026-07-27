/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QoreJson.h
    @brief the module-private JSON parsing and serialization codec

    Other binary modules consume this through the QoreJsonApi struct declared in
    <qore/QoreJsonApi.h> and published by JsonCppApi.cpp; see design/module-cpp-api.md.

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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

    Note: the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2.1, and GPL 2 or later; see README-LICENSE
    for more information.
*/

#ifndef _QORE_QOREJSON_H
#define _QORE_QOREJSON_H

#include <qore/Qore.h>
#include <qore/QoreJsonApi.h>

// The codec itself.  These entry points are module-private: other binary modules reach them
// through the QoreJsonApi struct published in <qore/QoreJsonApi.h>, not by resolving these
// symbols.  JGF_NONE and JGF_ADD_FORMATTING are declared with that struct, since consumers need
// them; the limits below are implementation details and stay here.

//! maximum JSON nesting depth (enforced in sandbox mode only)
#define JSON_MAX_NESTING_DEPTH 256

//! iterations between cancellation checks when serializing or parsing containers
#define JSON_INTERRUPT_CHECK_INTERVAL 100

//! parses a JSON string and returns the corresponding %Qore value
/** A leading UTF-8, UTF-16 or UTF-32 byte-order mark is skipped if present.  Any non-whitespace
    text following the JSON value is an error.

    @param str the JSON text to parse
    @param xsink %Qore-language exceptions are raised here

    @return the %Qore value corresponding to \a str, or a null value if an exception was raised;
    an empty input string returns @ref nothing without raising an exception

    @throw JSON-PARSE-ERROR syntax error parsing the JSON string

    @since %Qore 3.0
*/
DLLLOCAL QoreValue parse_json(const QoreString* str, ExceptionSink* xsink);

//! serializes a %Qore value to a new JSON string
/** @param data the value to serialize
    @param format @ref JGF_NONE or @ref JGF_ADD_FORMATTING
    @param enc the encoding of the returned string; if nullptr, UTF-8 is used
    @param xsink %Qore-language exceptions are raised here

    @return the JSON string corresponding to \a data, or nullptr if an exception was raised; the
    caller owns the reference returned

    @throw JSON-SERIALIZATION-ERROR the value cannot be serialized to JSON (ex: an object)
    @throw ENCODING-CONVERSION-ERROR an error occurred converting strings to \a enc

    @note <a href="https://tools.ietf.org/html/rfc7159">RFC 7159</a> allows only UTF-8, UTF-16 and
    UTF-32 for JSON text, but any encoding may be given here

    @since %Qore 3.0
*/
DLLLOCAL QoreStringNode* make_json(QoreValue data, int format, const QoreEncoding* enc,
        ExceptionSink* xsink);

//! appends the JSON serialization of a %Qore value to an existing string
/** Use this to compose a larger document from separately-serialized values; the \c json module's
    JSON-RPC message builders are implemented on top of it.

    @param str the string to append to; its encoding is used for the serialized text
    @param data the value to serialize
    @param indent -1 for no whitespace formatting, otherwise the starting indent level
    @param xsink %Qore-language exceptions are raised here

    @return 0 for OK, -1 if an exception was raised

    @throw JSON-SERIALIZATION-ERROR the value cannot be serialized to JSON (ex: an object)

    @since %Qore 3.0
*/
DLLLOCAL int json_serialize_value(QoreString& str, QoreValue data, int indent, ExceptionSink* xsink);

//! appends the JSON serialization of a list to an existing string, optionally skipping elements
/** @param str the string to append to; its encoding is used for the serialized text
    @param l the list to serialize
    @param indent -1 for no whitespace formatting, otherwise the starting indent level
    @param xsink %Qore-language exceptions are raised here
    @param offset the number of leading list elements to skip

    @return 0 for OK, -1 if an exception was raised

    @throw JSON-SERIALIZATION-ERROR an element cannot be serialized to JSON (ex: an object)

    @since %Qore 3.0
*/
DLLLOCAL int json_serialize_list(QoreString& str, const QoreListNode* l, int indent,
        ExceptionSink* xsink, unsigned offset = 0);


#endif // _QORE_QOREJSON_H
