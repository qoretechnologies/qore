/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QoreJsonApi.h
    @brief the C++ API published by the \c json binary module

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

#ifndef _QORE_QOREJSONAPI_H
#define _QORE_QOREJSONAPI_H

#include <qore/Qore.h>
#include <qore/QoreModuleCppApi.h>

/** @defgroup json_cpp_api JSON C++ API

    Converts between JSON text (<a href="https://tools.ietf.org/html/rfc7159">RFC 7159</a>) and
    %Qore values: the implementation behind the %Qore-language \c parse_json() and \c make_json()
    functions, published to other binary modules through the module C++ API mechanism
    (@ref module_cpp_api) so that a module whose own data format is JSON — \c avro, whose schemas
    are JSON documents — can parse and generate JSON from C++ without Qore-language code.

    The consumer includes only this header; there is no link-time dependency on the \c json module
    and the module is loaded on demand by the first resolution.  See @ref module_cpp_api for the
    resolution, versioning and ABI rules that apply.

    All members respect sandboxing: nesting depth is bounded and long operations are periodically
    checked for cancellation.

    @par Example
    @code{.cpp}
    static const QoreJsonApi* get_json_api(ExceptionSink* xsink) {
        static std::atomic<const QoreJsonApi*> cache{nullptr};
        const QoreJsonApi* api = cache.load(std::memory_order_acquire);
        if (!api) {
            api = qore_json_api(xsink);
            if (!api) {
                return nullptr;
            }
            cache.store(api, std::memory_order_release);
        }
        return api;
    }

    // ...
    const QoreJsonApi* json = get_json_api(xsink);
    if (!json) {
        return QoreValue();
    }
    ValueHolder v(json->parse(text, xsink), xsink);
    @endcode

    ///@{
*/

//! the major version of the C++ API published by the \c json module
/** Bumped only for an incompatible change to @ref QoreJsonApi; see @ref module_cpp_api_rules.
*/
#define QORE_JSON_CPP_API_MAJOR 1

//! the minor version of the C++ API published by the \c json module
/** Bumped whenever a member is appended to @ref QoreJsonApi.
*/
#define QORE_JSON_CPP_API_MINOR 0

//! no flags; standard JSON generation without whitespace formatting
#define JGF_NONE           0

//! use whitespace formatting including line breaks and indentation
#define JGF_ADD_FORMATTING (1 << 0)

//! the C++ API published by the \c json module
/** Append-only within major version @ref QORE_JSON_CPP_API_MAJOR; see @ref module_cpp_api_rules.

    Resolve it with @ref qore_json_api().
*/
struct QoreJsonApi {
    //! the version of this struct; must be the first member
    QoreModuleCppApiHeader hdr;

    // NB: these are data members, not functions, so doxygen rejects @param / @return sections on
    // them; arguments, results and ownership are described in prose instead

    // --- version 1.0 ---

    //! parses a JSON string and returns the corresponding %Qore value
    /** A leading UTF-8, UTF-16 or UTF-32 byte-order mark is skipped if present.  Any
        non-whitespace text following the JSON value is an error.

        Returns the %Qore value corresponding to \a str, or a null value if an exception was
        raised; an empty input string returns @ref nothing without raising an exception.  The
        caller owns any reference returned.

        Raises \c JSON-PARSE-ERROR on a syntax error.
    */
    QoreValue (*parse)(const QoreString& str, ExceptionSink* xsink);

    //! serializes a %Qore value to a new JSON string
    /** \a format takes @ref JGF_NONE or @ref JGF_ADD_FORMATTING; \a enc is the encoding of the
        returned string, or nullptr for UTF-8.  The reference to \a data is not consumed.

        Returns the JSON string corresponding to \a data, or nullptr if an exception was raised;
        the caller owns the reference returned.

        Raises \c JSON-SERIALIZATION-ERROR if the value cannot be serialized to JSON (ex: an
        object), and \c ENCODING-CONVERSION-ERROR on an error converting strings to \a enc.

        @note <a href="https://tools.ietf.org/html/rfc7159">RFC 7159</a> allows only UTF-8, UTF-16
        and UTF-32 for JSON text, but any encoding may be given here
    */
    QoreStringNode* (*generate)(QoreValue data, int format, const QoreEncoding* enc, ExceptionSink* xsink);

    //! appends the JSON serialization of a %Qore value to an existing string
    /** Use this to compose a larger document from separately-serialized values.  \a str's own
        encoding is used for the serialized text, and \a indent is -1 for no whitespace formatting
        or the starting indent level otherwise.

        Returns 0 for OK, -1 if an exception was raised; raises \c JSON-SERIALIZATION-ERROR if the
        value cannot be serialized to JSON.
    */
    int (*serialize_value)(QoreString& str, QoreValue data, int indent, ExceptionSink* xsink);

    //! appends the JSON serialization of a list to an existing string, skipping \a offset elements
    /** Returns 0 for OK, -1 if an exception was raised; raises \c JSON-SERIALIZATION-ERROR if an
        element cannot be serialized to JSON.
    */
    int (*serialize_list)(QoreString& str, const QoreListNode* l, int indent, ExceptionSink* xsink,
            unsigned offset);
};

//! resolves the \c json module's C++ API, loading the module if it is not already loaded
/** The returned pointer is valid for the life of the process; resolution takes the module
    manager's lock, so cache it rather than resolving per call.

    @param xsink %Qore-language exceptions are raised here
    @param minor the minimum minor version required; defaults to the version this header declares.
    Pass a lower value to accept an older \c json module when none of the members added since are
    used.

    @return the API struct, or nullptr if an exception was raised

    @throw LOAD-MODULE-ERROR the \c json module could not be found or loaded
    @throw MODULE-CPP-API-VERSION-ERROR the \c json module cannot serve the requested version

    @since %Qore 3.0
*/
static inline const QoreJsonApi* qore_json_api(ExceptionSink* xsink,
        unsigned minor = QORE_JSON_CPP_API_MINOR) {
    return static_cast<const QoreJsonApi*>(q_get_module_cpp_api("json", QORE_JSON_CPP_API_MAJOR,
        minor, xsink));
}

///@}

#endif // _QORE_QOREJSONAPI_H
