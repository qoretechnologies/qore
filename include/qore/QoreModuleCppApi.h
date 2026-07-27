/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QoreModuleCppApi.h
    @brief versioned C++ API publication and resolution between binary modules

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

#ifndef _QORE_QOREMODULECPPAPI_H
#define _QORE_QOREMODULECPPAPI_H

#include <qore/Qore.h>

/** @defgroup module_cpp_api Module C++ API

    Lets one binary module consume another's C++ API without the API having to be promoted into
    libqore.

    An exporting module publishes a single \c extern \c "C" entry point that returns a \c const
    pointer to a static struct of function pointers; a consuming module resolves that struct
    through @ref q_get_module_cpp_api(), which loads the exporting module if it is not already
    loaded and verifies that the version it publishes can serve the version the consumer was
    compiled against.  Consumers include only the exporting module's installed header; there is no
    link-time dependency and no new installed shared library.

    @section module_cpp_api_producer Publishing a C++ API

    The exporting module declares its struct in an installed header, versioned with a major and a
    minor number of its own — unrelated to @ref QORE_MODULE_API_MAJOR — and beginning with a
    @ref QoreModuleCppApiHeader member:

    @code
    // include/qore/QoreExampleApi.h, installed by the "example" module
    #define QORE_EXAMPLE_CPP_API_MAJOR 1
    #define QORE_EXAMPLE_CPP_API_MINOR 0

    class QoreExampleHandle;   // opaque; never defined for consumers

    struct QoreExampleApi {
        QoreModuleCppApiHeader hdr;

        QoreExampleHandle* (*parse)(const QoreString& str, ExceptionSink* xsink);
        void (*handle_ref)(QoreExampleHandle* h);
        void (*handle_deref)(QoreExampleHandle* h);
        QoreValue (*decode)(QoreExampleHandle* h, const void* buf, size_t len, ExceptionSink* xsink);
        // new members are appended here, bumping QORE_EXAMPLE_CPP_API_MINOR
    };

    //! resolves the "example" module's C++ API, loading the module if necessary
    static inline const QoreExampleApi* qore_example_api(ExceptionSink* xsink,
            unsigned minor = QORE_EXAMPLE_CPP_API_MINOR) {
        return static_cast<const QoreExampleApi*>(q_get_module_cpp_api("example",
            QORE_EXAMPLE_CPP_API_MAJOR, minor, xsink));
    }
    @endcode

    and defines the struct and the entry point in the module itself:

    @code
    static const QoreExampleApi example_cpp_api = {
        {QORE_EXAMPLE_CPP_API_MAJOR, QORE_EXAMPLE_CPP_API_MINOR},
        example_parse, example_handle_ref, example_handle_deref, example_decode,
    };

    extern "C" DLLEXPORT const void* example_qore_cpp_api(unsigned major, unsigned minor) {
        return major == QORE_EXAMPLE_CPP_API_MAJOR ? &example_cpp_api : nullptr;
    }
    @endcode

    The entry point is named <tt>\<feature\>@ref QORE_MODULE_CPP_API_SUFFIX</tt>, with any hyphen in
    the feature name replaced by an underscore so that the result is a valid C identifier — the same
    transformation the module description function name uses.  It must be \c extern \c "C" and
    \c DLLEXPORT.

    A module that supports only one major version can ignore both arguments and return its struct
    unconditionally: @ref q_get_module_cpp_api() validates @ref QoreModuleCppApiHeader against the
    request in every case, so a version mismatch is reported as an exception whether or not the
    producer checks for it.  The arguments exist so that a producer that supports more than one
    major version can return the struct matching the request; such a producer must return the
    struct whose header describes it, never a struct with a header that does not match its layout.

    @section module_cpp_api_rules ABI rules

    Two rules make the boundary safe across independently released repositories:

    - The struct is <b>append-only</b> within a major version.  New members go at the end and bump
      the minor version; nothing is ever removed, reordered or has its signature changed.  Removing,
      reordering or retyping a member — or changing the meaning or ownership contract of an existing
      one — requires a new major version, which is a new struct with a new name (ex:
      \c QoreExampleApi2) and its own entry point, or a coordinated release of every consumer.
    - Only types with an already-public ABI cross the boundary: @ref QoreValue, @ref BinaryNode,
      @ref QoreString, @ref ExceptionSink, plain C types, and opaque handles with explicit
      \c ref / \c deref members in the struct.  Never a module-defined class layout, never anything
      with inline member functions, virtual functions or non-trivial members that the consumer
      would have to have compiled identically.

    Ownership must be documented per member exactly as it is for libqore's own API: whether the
    caller owns the reference returned, and whether an argument reference is consumed.

    @section module_cpp_api_consumer Consuming a C++ API

    The consumer includes only the producer's installed header and calls the producer's inline
    accessor.  Because the module manager only unloads binary modules at shutdown, the resolved
    pointer is valid for the life of the process and should be resolved once and cached:

    @code
    static const QoreExampleApi* get_example_api(ExceptionSink* xsink) {
        static std::atomic<const QoreExampleApi*> cache{nullptr};
        const QoreExampleApi* api = cache.load(std::memory_order_acquire);
        if (!api) {
            api = qore_example_api(xsink);
            if (!api) {
                return nullptr;
            }
            cache.store(api, std::memory_order_release);
        }
        return api;
    }
    @endcode

    Resolution may raise a %Qore-language exception, so the first use must be somewhere an
    exception can be reported.  A module that requires the API unconditionally may resolve it from
    its module init function instead, where the module manager's lock is not held; a module for
    which the dependency is optional simply reports the exception, or clears it and takes another
    path, giving C++ the equivalent of the %Qore-language <tt>%%try-module</tt> directive.

    ///@{
*/

//! the suffix of the entry point a module exports to publish a C++ API
/** The full symbol name is the module's feature name with any hyphen replaced by an underscore,
    followed by this suffix.
*/
#define QORE_MODULE_CPP_API_SUFFIX "_qore_cpp_api"

//! the signature of the entry point a module exports to publish a C++ API
/** @param major the major version requested by the consumer
    @param minor the minor version requested by the consumer

    @return a pointer to a static struct whose first member is a @ref QoreModuleCppApiHeader
    describing the struct returned, or nullptr if the module cannot serve the requested version

    @since %Qore 3.0
*/
typedef const void* (*qore_module_cpp_api_t)(unsigned major, unsigned minor);

//! the mandatory first member of every module C++ API struct
/** Declares the version of the struct that follows it, which @ref q_get_module_cpp_api() checks
    against the version the consumer was compiled against.

    @since %Qore 3.0
*/
struct QoreModuleCppApiHeader {
    //! the major version of the API struct; consumers must match exactly
    unsigned major;
    //! the minor version of the API struct; consumers may require this or any lower value
    unsigned minor;
};

//! resolves a module's C++ API struct, loading the module if it is not already loaded
/** The module's %Qore-language namespace is deliberately not imported into any
    @ref QoreProgram "Program"; a C++ API consumer needs the module's code, not its %Qore types.

    The returned pointer is valid for the life of the process: binary modules are only unloaded at
    shutdown.  Resolution is not free (it takes the module manager's lock), so the result should be
    cached; see @ref module_cpp_api_consumer.

    @param feature the feature name of the module publishing the API (ex: \c "avro")
    @param major the major version of the API struct the caller was compiled against; the module
    must publish exactly this major version
    @param minor the minimum minor version of the API struct the caller requires; the module must
    publish this minor version or a higher one
    @param xsink %Qore-language exceptions are raised here

    @return the API struct, or nullptr with an exception raised

    @throw LOAD-MODULE-ERROR the module could not be found or could not be loaded
    @throw MODULE-CPP-API-ERROR the module is a %Qore-language module, or exports no C++ API entry
    point
    @throw MODULE-CPP-API-VERSION-ERROR the module cannot serve the requested version

    @note thread safe; may be called from a module init function, where the module manager's lock
    is not held

    @since %Qore 3.0
*/
DLLEXPORT const void* q_get_module_cpp_api(const char* feature, unsigned major, unsigned minor,
        ExceptionSink* xsink);

///@}

#endif // _QORE_QOREMODULECPPAPI_H
