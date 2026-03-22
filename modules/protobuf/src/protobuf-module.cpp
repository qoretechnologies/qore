/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file protobuf-module.cpp protobuf module implementation */
/*
    Qore protobuf module

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

#include "protobuf-module.h"
#include "QC_ProtobufSchema.h"

#include <google/protobuf/stubs/common.h>

static void protobuf_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink);
static void protobuf_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink);
static void protobuf_module_delete();

extern "C" DLLEXPORT void protobuf_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "protobuf";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "Qore protobuf module for dynamic protocol buffer schema loading, "
                    "encoding, and decoding";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "https://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = protobuf_module_init;
    mod_info.ns_init = protobuf_module_ns_init;
    mod_info.del = protobuf_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}

// Global hashdecl pointers
const TypedHashDecl* hashdeclProtobufServiceInfo = nullptr;
const TypedHashDecl* hashdeclProtobufMethodInfo = nullptr;

QoreNamespace ProtobufNs("Qore::Protobuf");

static void protobuf_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    // Initialize hashdecls (defined in QPP files for documentation)
    hashdeclProtobufMethodInfo = init_hashdecl_ProtobufMethodInfo(ProtobufNs);
    hashdeclProtobufServiceInfo = init_hashdecl_ProtobufServiceInfo(ProtobufNs);

    // Initialize ProtobufSchema class
    ProtobufNs.addSystemClass(initProtobufSchemaClass(ProtobufNs));
}

static void protobuf_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    qns->addNamespace(ProtobufNs.copy());
}

static void protobuf_module_delete() {
    google::protobuf::ShutdownProtobufLibrary();
}
