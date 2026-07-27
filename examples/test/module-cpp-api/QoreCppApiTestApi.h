/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QoreCppApiTestApi.h the C++ API published by the "cppapitest" test module

    This stands in for the installed header a real producer module ships (ex:
    <qore/QoreAvroApi.h>): the producer defines the struct and the entry point, and the consumer
    -- here the "cppapiuser" test module -- includes only this header and resolves the struct at
    run time through q_get_module_cpp_api().  Neither module links against the other.

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
*/

#ifndef _QORE_QORECPPAPITESTAPI_H
#define _QORE_QORECPPAPITESTAPI_H

#include <qore/Qore.h>
#include <qore/QoreModuleCppApi.h>

//! the major version of the C++ API published by the "cppapitest" module
#define QORE_CPPAPITEST_CPP_API_MAJOR 1

//! the minor version of the C++ API published by the "cppapitest" module
#define QORE_CPPAPITEST_CPP_API_MINOR 2

//! a major version the producer answers with a struct whose header deliberately does not match
/** The producer accepts the request instead of refusing it, so the mismatch can only be caught by
    q_get_module_cpp_api() validating QoreModuleCppApiHeader; that is what this exists to test.
*/
#define QORE_CPPAPITEST_BAD_HEADER_MAJOR 7

//! the major version of the struct the producer answers QORE_CPPAPITEST_BAD_HEADER_MAJOR with
#define QORE_CPPAPITEST_BAD_HEADER_STRUCT_MAJOR 9

//! the minor version of the struct the producer answers QORE_CPPAPITEST_BAD_HEADER_MAJOR with
#define QORE_CPPAPITEST_BAD_HEADER_STRUCT_MINOR 9

//! an opaque handle owned by the producer, with explicit ref/deref in the API struct
class QoreCppApiTestCounter;

//! the C++ API published by the "cppapitest" module
/** Append-only within major version @ref QORE_CPPAPITEST_CPP_API_MAJOR; see @ref module_cpp_api.
*/
struct QoreCppApiTestApi {
    //! the version of this struct; must be the first member
    QoreModuleCppApiHeader hdr;

    // --- version 1.0 ---

    //! returns the sum of the two arguments
    int64 (*add)(int64 a, int64 b);

    //! returns \a str reversed by character, or nullptr if an exception was raised
    /** The caller owns the reference returned.
    */
    QoreStringNode* (*reverse)(const QoreString& str, ExceptionSink* xsink);

    // --- added in version 1.1 ---

    //! creates a counter with the given initial value; the caller owns the reference returned
    QoreCppApiTestCounter* (*counter_create)(int64 init);

    //! increments the counter's reference count
    void (*counter_ref)(QoreCppApiTestCounter* c);

    //! decrements the counter's reference count, deleting the counter when it reaches zero
    void (*counter_deref)(QoreCppApiTestCounter* c);

    //! adds \a v to the counter and returns the new value
    int64 (*counter_add)(QoreCppApiTestCounter* c, int64 v);

    // --- added in version 1.2 ---

    //! returns the number of counters currently allocated by the producer module
    /** Lets a consumer prove that handles crossing the boundary are actually released.
    */
    int64 (*counter_live_count)();
};

//! resolves the "cppapitest" module's C++ API, loading the module if necessary
/** @param xsink Qore-language exceptions are raised here
    @param minor the minimum minor version required; defaults to the version this header declares

    @return the API struct, or nullptr if an exception was raised
*/
static inline const QoreCppApiTestApi* qore_cppapitest_api(ExceptionSink* xsink,
        unsigned minor = QORE_CPPAPITEST_CPP_API_MINOR) {
    return static_cast<const QoreCppApiTestApi*>(q_get_module_cpp_api("cppapitest",
        QORE_CPPAPITEST_CPP_API_MAJOR, minor, xsink));
}

#endif // _QORE_QORECPPAPITESTAPI_H
