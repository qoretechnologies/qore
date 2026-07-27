/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file avrocppapi-module.cpp a test binary module consuming the avro module's C++ API

    Test scaffolding for @ref avro_cpp_api: this module is built into the build tree but is never
    installed.  It links against neither libavro nor the \c avro module, includes only the
    installed <qore/QoreAvroApi.h>, and resolves the API struct at run time through
    q_get_module_cpp_api() exactly as a real out-of-tree consumer (ex: \c module-grpc) does.

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

#include "avrocppapi-module.h"

#include <atomic>

static void avrocppapi_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink);
static void avrocppapi_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink);
static void avrocppapi_module_delete();

extern "C" DLLEXPORT void avrocppapi_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "avrocppapi";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "test module consuming the avro module's C++ API";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "https://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = avrocppapi_module_init;
    mod_info.ns_init = avrocppapi_module_ns_init;
    mod_info.del = avrocppapi_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}

QoreNamespace AvroCppApiNs("Qore::AvroCppApi");

// the resolved avro API; binary modules are only unloaded at shutdown, so the pointer is valid
// for the life of the process and resolution is done once
static std::atomic<const QoreAvroApi*> cached_api{nullptr};

const QoreAvroApi* avrocppapi_get_api(ExceptionSink* xsink) {
    const QoreAvroApi* api = cached_api.load(std::memory_order_acquire);
    if (!api) {
        api = qore_avro_api(xsink);
        if (!api) {
            assert(*xsink);
            return nullptr;
        }
        cached_api.store(api, std::memory_order_release);
    }
    return api;
}

QoreAvroSchemaRef* avrocppapi_parse(const QoreAvroApi* api, const QoreString& schema_json,
        ExceptionSink* xsink) {
    return api->parse_schema(schema_json, xsink);
}

static void avrocppapi_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    init_avrocppapi_functions(AvroCppApiNs);
}

static void avrocppapi_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    qns->addNamespace(AvroCppApiNs.copy());
}

static void avrocppapi_module_delete() {
}
