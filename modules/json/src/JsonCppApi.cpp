/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file JsonCppApi.cpp the C++ API the json module publishes to other binary modules */
/*
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

#include "qore-json-module.h"

// the codec takes a QoreString* while the published API takes a reference, which cannot be null;
// the thin wrappers below are the only difference between the two
static QoreValue json_api_parse(const QoreString& str, ExceptionSink* xsink) {
    return parse_json(&str, xsink);
}

static QoreStringNode* json_api_generate(QoreValue data, int format, const QoreEncoding* enc,
        ExceptionSink* xsink) {
    return make_json(data, format, enc, xsink);
}

static int json_api_serialize_value(QoreString& str, QoreValue data, int indent, ExceptionSink* xsink) {
    return json_serialize_value(str, data, indent, xsink);
}

static int json_api_serialize_list(QoreString& str, const QoreListNode* l, int indent,
        ExceptionSink* xsink, unsigned offset) {
    return json_serialize_list(str, l, indent, xsink, offset);
}

//! the C++ API published by this module
static const QoreJsonApi json_cpp_api = {
    {QORE_JSON_CPP_API_MAJOR, QORE_JSON_CPP_API_MINOR},
    json_api_parse,
    json_api_generate,
    json_api_serialize_value,
    json_api_serialize_list,
};

extern "C" DLLEXPORT const void* json_qore_cpp_api(unsigned major, unsigned minor) {
    // only one major version is implemented; q_get_module_cpp_api() validates the header the
    // struct declares against the caller's request in every case, so this check is belt and
    // braces rather than the mechanism
    if (major != QORE_JSON_CPP_API_MAJOR || minor > QORE_JSON_CPP_API_MINOR) {
        return nullptr;
    }
    return &json_cpp_api;
}
