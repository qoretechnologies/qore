/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file AvroCppApi.cpp the C++ API the avro module publishes to other binary modules */
/*
    Qore avro module

    Copyright (C) 2026 Qore Technologies, s.r.o.

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

#include "avro-module.h"
#include "AvroDecoder.h"
#include "AvroEncoder.h"
#include "QC_AvroSchema.h"

#include <qore/QoreAvroApi.h>

// QoreAvroSchemaRef is opaque to consumers; inside the module it is exactly a QoreAvroSchema
static QoreAvroSchema* schema_of(QoreAvroSchemaRef* ref) {
    return reinterpret_cast<QoreAvroSchema*>(ref);
}

static QoreAvroSchemaRef* ref_of(QoreAvroSchema* schema) {
    return reinterpret_cast<QoreAvroSchemaRef*>(schema);
}

static QoreAvroSchemaRef* avro_api_parse_schema(const QoreString& schema_json, ExceptionSink* xsink) {
    TempEncodingHelper utf8(schema_json, QCS_UTF8, xsink);
    if (*xsink) {
        return nullptr;
    }
    AvroSchemaData* data = AvroSchemaData::parseJson(**utf8, xsink);
    if (!data) {
        return nullptr;
    }
    // QoreAvroSchema takes over the reference returned by the parser
    return ref_of(new QoreAvroSchema(data, data->getRoot()));
}

static QoreAvroSchemaRef* avro_api_parse_schema_value(QoreValue schema, ExceptionSink* xsink) {
    AvroSchemaData* data = AvroSchemaData::parseValue(schema, xsink);
    if (!data) {
        return nullptr;
    }
    return ref_of(new QoreAvroSchema(data, data->getRoot()));
}

static void avro_api_schema_ref(QoreAvroSchemaRef* schema) {
    schema_of(schema)->ref();
}

static void avro_api_schema_deref(QoreAvroSchemaRef* schema) {
    schema_of(schema)->deref();
}

static QoreValue avro_api_decode(QoreAvroSchemaRef* schema, const void* buf, size_t len,
        ExceptionSink* xsink) {
    AvroDecoder decoder(static_cast<const unsigned char*>(buf), len);
    return decoder.decode(schema_of(schema)->getRoot(), xsink);
}

static QoreValue avro_api_decode_resolved(QoreAvroSchemaRef* writer, QoreAvroSchemaRef* reader,
        const void* buf, size_t len, ExceptionSink* xsink) {
    AvroDecoder decoder(static_cast<const unsigned char*>(buf), len);
    return decoder.decodeResolved(schema_of(writer)->getRoot(), schema_of(reader)->getRoot(), xsink);
}

static BinaryNode* avro_api_encode(QoreAvroSchemaRef* schema, QoreValue v, ExceptionSink* xsink) {
    AvroEncoder encoder;
    if (encoder.encode(schema_of(schema)->getRoot(), v, xsink)) {
        return nullptr;
    }
    return encoder.takeBinary();
}

static int64 avro_api_fingerprint(QoreAvroSchemaRef* schema) {
    return schema_of(schema)->getFingerprint();
}

static QoreStringNode* avro_api_canonical_form(QoreAvroSchemaRef* schema) {
    return schema_of(schema)->getCanonicalForm();
}

static QoreStringNode* avro_api_schema_json(QoreAvroSchemaRef* schema, ExceptionSink* xsink) {
    return schema_of(schema)->getJson(xsink);
}

static QoreAvroSchemaRef* avro_api_named_type(QoreAvroSchemaRef* schema, const char* fullname) {
    QoreAvroSchema* s = schema_of(schema);
    const AvroNode* n = s->getData()->findNamedType(fullname);
    if (!n) {
        return nullptr;
    }
    // the new handle shares the arena, so it takes its own reference to the schema data
    s->getData()->ref();
    return ref_of(new QoreAvroSchema(s->getData(), n));
}

static QoreObject* avro_api_schema_object(QoreAvroSchemaRef* schema, ExceptionSink* xsink) {
    if (!getProgram()) {
        xsink->raiseException("AVRO-SCHEMA-ERROR", "QoreAvroApi::schema_object() requires a current Program "
            "context to create the AvroSchema object");
        return nullptr;
    }
    QoreAvroSchema* s = schema_of(schema);
    return avro_make_schema_object(s->getData(), s->getRoot());
}

static QoreAvroSchemaRef* avro_api_schema_from_object(const QoreObject* obj, ExceptionSink* xsink) {
    PrivateDataRefHolder<QoreAvroSchema> s(obj, CID_AVROSCHEMA, xsink);
    if (!s) {
        if (!*xsink) {
            // cannot happen: getReferencedPrivateData() raises on every failure path
            xsink->raiseException("AVRO-SCHEMA-ERROR", "QoreAvroApi::schema_from_object() was called with an "
                "object of class '%s'; expecting Qore::Avro::AvroSchema", obj->getClassName());
        }
        return nullptr;
    }
    // PrivateDataRefHolder already took a reference; hand it to the caller
    return ref_of(s.release());
}

//! the C++ API published by this module
static const QoreAvroApi avro_cpp_api = {
    {QORE_AVRO_CPP_API_MAJOR, QORE_AVRO_CPP_API_MINOR},
    avro_api_parse_schema,
    avro_api_parse_schema_value,
    avro_api_schema_ref,
    avro_api_schema_deref,
    avro_api_decode,
    avro_api_decode_resolved,
    avro_api_encode,
    avro_api_fingerprint,
    avro_api_canonical_form,
    avro_api_schema_json,
    avro_api_named_type,
    avro_api_schema_object,
    avro_api_schema_from_object,
};

extern "C" DLLEXPORT const void* avro_qore_cpp_api(unsigned major, unsigned minor) {
    // only one major version is implemented; q_get_module_cpp_api() validates the header the
    // struct declares against the caller's request in every case, so this check is belt and
    // braces rather than the mechanism
    if (major != QORE_AVRO_CPP_API_MAJOR || minor > QORE_AVRO_CPP_API_MINOR) {
        return nullptr;
    }
    return &avro_cpp_api;
}
