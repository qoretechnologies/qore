/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QoreAvroApi.h
    @brief the C++ API published by the \c avro binary module

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

#ifndef _QORE_QOREAVROAPI_H
#define _QORE_QOREAVROAPI_H

#include <qore/Qore.h>
#include <qore/QoreModuleCppApi.h>

/** @defgroup avro_cpp_api Avro C++ API

    The <a href="https://avro.apache.org">Apache Avro</a> schema parser and binary datum codec
    implemented by the \c avro binary module, published to other binary modules through the module
    C++ API mechanism (@ref module_cpp_api).

    This exists for consumers that decode or encode many data against a schema supplied out of
    band — the framing streaming APIs such as the
    <a href="https://developer.salesforce.com/docs/platform/pub-sub-api/overview">Salesforce
    Pub/Sub API</a> deliver — and that want to do so from C++ without a %Qore-language call per
    datum.  Everything here is also available from %Qore through the
    \c Qore::Avro::AvroSchema class; the two share one implementation and one schema
    representation, and @ref QoreAvroApi::schema_object "schema_object()" converts a handle into
    the %Qore object so that a consumer can hand the same schema to %Qore code (ex: the
    \c AvroUtil module's \c AvroTypeHelper).

    The consumer includes only this header; there is no link-time dependency on the \c avro module
    and the module is loaded on demand by the first resolution.  See @ref module_cpp_api for the
    resolution, versioning and ABI rules that apply.

    @par Example
    @code{.cpp}
    static const QoreAvroApi* get_avro_api(ExceptionSink* xsink) {
        static std::atomic<const QoreAvroApi*> cache{nullptr};
        const QoreAvroApi* api = cache.load(std::memory_order_acquire);
        if (!api) {
            api = qore_avro_api(xsink);
            if (!api) {
                return nullptr;
            }
            cache.store(api, std::memory_order_release);
        }
        return api;
    }

    // ...
    const QoreAvroApi* api = get_avro_api(xsink);
    if (!api) {
        return QoreValue();
    }
    QoreAvroSchemaHolder schema(api, api->parse_schema(schema_json, xsink));
    if (!schema) {
        return QoreValue();
    }
    return api->decode(*schema, buf, len, xsink);
    @endcode

    ///@{
*/

//! the major version of the C++ API published by the \c avro module
/** Bumped only for an incompatible change to @ref QoreAvroApi; see @ref module_cpp_api_rules.
*/
#define QORE_AVRO_CPP_API_MAJOR 1

//! the minor version of the C++ API published by the \c avro module
/** Bumped whenever a member is appended to @ref QoreAvroApi.
*/
#define QORE_AVRO_CPP_API_MINOR 0

//! an opaque, reference-counted handle to a parsed Avro schema owned by the \c avro module
/** Never defined for consumers; use the \c schema_ref and \c schema_deref members of
    @ref QoreAvroApi, or @ref QoreAvroSchemaHolder.

    A handle is immutable once parsed and safe to share between threads, so a consumer that
    decodes many data against a schema fetched by ID should cache handles keyed by that ID (or by
    @ref QoreAvroApi::fingerprint "fingerprint()") and pay the schema parse once.
*/
class QoreAvroSchemaRef;

//! the C++ API published by the \c avro module
/** Append-only within major version @ref QORE_AVRO_CPP_API_MAJOR; see @ref module_cpp_api_rules.

    Resolve it with @ref qore_avro_api().
*/
struct QoreAvroApi {
    //! the version of this struct; must be the first member
    QoreModuleCppApiHeader hdr;

    // --- version 1.0 ---

    // NB: these are data members, not functions, so doxygen rejects @param / @return sections on
    // them; arguments, results and ownership are described in prose instead

    //! parses an Avro schema from its JSON text
    /** \a schema_json is the schema as JSON text in any encoding; %Qore-language exceptions are
        raised in \a xsink.

        Returns a new schema handle owned by the caller, or nullptr if an exception was raised.
        Raises \c AVRO-SCHEMA-ERROR if the text is not a valid Avro schema.
    */
    QoreAvroSchemaRef* (*parse_schema)(const QoreString& schema_json, ExceptionSink* xsink);

    //! parses an Avro schema already represented as %Qore data
    /** \a schema is the schema as a hash, a list (a top-level union) or a string (a type name),
        exactly as \c parse_json() would produce it from the schema's JSON text; the reference is
        not consumed.

        Returns a new schema handle owned by the caller, or nullptr if an exception was raised.
        Raises \c AVRO-SCHEMA-ERROR if the value is not a valid Avro schema.
    */
    QoreAvroSchemaRef* (*parse_schema_value)(QoreValue schema, ExceptionSink* xsink);

    //! increments the schema handle's reference count
    void (*schema_ref)(QoreAvroSchemaRef* schema);

    //! decrements the schema handle's reference count, freeing the schema when it reaches zero
    void (*schema_deref)(QoreAvroSchemaRef* schema);

    //! decodes an Avro binary datum against \a schema
    /** Decodes a <i>bare datum</i>: the data carry no framing, no schema and no length, only the
        values packed in schema order.  Trailing bytes after the datum are ignored, since a bare
        datum carries no length.

        \a buf and \a len give the encoded datum, and \a schema is the schema the data were
        written with.

        Returns the decoded value; the caller owns any reference returned.  Raises
        \c AVRO-DECODE-ERROR if the data are truncated or do not match the schema.
    */
    QoreValue (*decode)(QoreAvroSchemaRef* schema, const void* buf, size_t len, ExceptionSink* xsink);

    //! decodes an Avro binary datum applying the specification's schema resolution rules
    /** \a writer is the schema the data in \a buf were written with and \a reader is the schema to
        read them as.

        Returns the decoded value; the caller owns any reference returned.  Raises
        \c AVRO-DECODE-ERROR if the data are truncated or do not match the writer schema, and
        \c AVRO-RESOLUTION-ERROR if the two schemas cannot be reconciled.
    */
    QoreValue (*decode_resolved)(QoreAvroSchemaRef* writer, QoreAvroSchemaRef* reader, const void* buf,
            size_t len, ExceptionSink* xsink);

    //! encodes a %Qore value as an Avro binary datum against \a schema
    /** The reference to \a v is not consumed.

        Returns the binary encoding with no framing, or nullptr if an exception was raised; the
        caller owns the reference returned.  Raises \c AVRO-ENCODE-ERROR if the value cannot be
        represented by the schema.
    */
    BinaryNode* (*encode)(QoreAvroSchemaRef* schema, QoreValue v, ExceptionSink* xsink);

    //! returns the CRC-64-AVRO fingerprint of the schema's Parsing Canonical Form
    /** Two schemas that differ only in documentation, aliases, default values or the field order
        of their JSON text have the same fingerprint, which makes this the right key for a
        content-addressed schema cache.
    */
    int64 (*fingerprint)(QoreAvroSchemaRef* schema);

    //! returns the Avro Parsing Canonical Form of the schema
    /** The caller owns the reference returned.
    */
    QoreStringNode* (*canonical_form)(QoreAvroSchemaRef* schema);

    //! returns the schema as JSON text
    /** Returns the schema as JSON text in UTF-8, or nullptr if an exception was raised; the
        caller owns the reference returned.
    */
    QoreStringNode* (*schema_json)(QoreAvroSchemaRef* schema, ExceptionSink* xsink);

    //! returns a handle rooted at the named type \a fullname declared anywhere in the schema
    /** The returned handle shares — and holds a reference to — the same parsed schema, so a
        record type nested inside a larger schema can be used on its own.  \a fullname is the full
        name of the named type (ex: \c "com.example.Event").

        Returns a new handle owned by the caller, or nullptr if the schema declares no such type;
        no exception is raised in that case.
    */
    QoreAvroSchemaRef* (*named_type)(QoreAvroSchemaRef* schema, const char* fullname);

    //! returns the %Qore \c Qore::Avro::AvroSchema object for a schema handle
    /** Lets a consumer hand a schema resolved in C++ to %Qore-language code — the \c AvroUtil
        module's \c AvroTypeHelper, for example — without reparsing it.

        Returns a new object reference owned by the caller, or nullptr if an exception was raised.

        @note must be called on a thread with a current @ref QoreProgram "Program"
    */
    QoreObject* (*schema_object)(QoreAvroSchemaRef* schema, ExceptionSink* xsink);

    //! returns the schema handle held by the %Qore \c Qore::Avro::AvroSchema object \a obj
    /** The inverse of @ref QoreAvroApi::schema_object "schema_object()", for a schema that
        %Qore-language code parsed.

        Returns a new handle owned by the caller, or nullptr if an exception was raised.  Raises
        \c OBJECT-INCOMPATIBLE if \a obj is not a \c Qore::Avro::AvroSchema object, and
        \c OBJECT-ALREADY-DELETED if it has already been deleted.
    */
    QoreAvroSchemaRef* (*schema_from_object)(const QoreObject* obj, ExceptionSink* xsink);
};

//! resolves the \c avro module's C++ API, loading the module if it is not already loaded
/** The returned pointer is valid for the life of the process; resolution takes the module
    manager's lock, so cache it rather than resolving per call.

    @param xsink %Qore-language exceptions are raised here
    @param minor the minimum minor version required; defaults to the version this header declares.
    Pass a lower value to accept an older \c avro module when none of the members added since are
    used.

    @return the API struct, or nullptr if an exception was raised

    @throw LOAD-MODULE-ERROR the \c avro module could not be found or loaded
    @throw MODULE-CPP-API-VERSION-ERROR the \c avro module cannot serve the requested version

    @since %Qore 3.0
*/
static inline const QoreAvroApi* qore_avro_api(ExceptionSink* xsink,
        unsigned minor = QORE_AVRO_CPP_API_MINOR) {
    return static_cast<const QoreAvroApi*>(q_get_module_cpp_api("avro", QORE_AVRO_CPP_API_MAJOR,
        minor, xsink));
}

//! holds a schema handle reference and releases it when it goes out of scope
/** An opaque handle from another module has no destructor the consumer can see, so this is the
    only exception-safe way to hold one.

    @since %Qore 3.0
*/
class QoreAvroSchemaHolder {
public:
    //! takes over the reference to \a schema
    DLLLOCAL QoreAvroSchemaHolder(const QoreAvroApi* api, QoreAvroSchemaRef* schema) : api(api), schema(schema) {
        assert(api);
    }

    DLLLOCAL ~QoreAvroSchemaHolder() {
        if (schema) {
            api->schema_deref(schema);
        }
    }

    //! returns the handle held, or nullptr if there is none
    DLLLOCAL QoreAvroSchemaRef* operator*() const {
        return schema;
    }

    //! returns True if a handle is held
    DLLLOCAL operator bool() const {
        return (bool)schema;
    }

    //! returns the handle held and releases ownership of it
    DLLLOCAL QoreAvroSchemaRef* release() {
        QoreAvroSchemaRef* rv = schema;
        schema = nullptr;
        return rv;
    }

    QoreAvroSchemaHolder(const QoreAvroSchemaHolder&) = delete;
    QoreAvroSchemaHolder& operator=(const QoreAvroSchemaHolder&) = delete;

private:
    const QoreAvroApi* api;
    QoreAvroSchemaRef* schema;
};

///@}

#endif // _QORE_QOREAVROAPI_H
