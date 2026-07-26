/*
  Qore json module

  Copyright (C) 2010 - 2026 Qore Technologies

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "qore-json-module.h"

#include "QC_JsonRpcClient.h"
#include "QC_JsonSaxParser.h"
#include "QC_JsonSchema.h"
#include "QC_JsonStreamWriter.h"

#include "ql_json.h"
#include "ql_cbor.h"
#include "ql_toon.h"

#include <stdarg.h>

static void json_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink);
static void json_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink);
static void json_module_delete();

extern "C" DLLEXPORT void json_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "json";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "json module";
    mod_info.author = "David Nichols";
    mod_info.url = "http://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = json_module_init;
    mod_info.ns_init = json_module_ns_init;
    mod_info.del = json_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}

QoreNamespace JNS("Qore::Json");

void json_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
   hashdeclJwtHeader = init_hashdecl_JwtHeader(JNS);
   hashdeclJwtClaims = init_hashdecl_JwtClaims(JNS);
   hashdeclJwtDecodeResult = init_hashdecl_JwtDecodeResult(JNS);
   hashdeclJsonSchemaValidationError = init_hashdecl_JsonSchemaValidationError(JNS);
   hashdeclJsonSchemaValidationResult = init_hashdecl_JsonSchemaValidationResult(JNS);
   JNS.addSystemClass(initJsonRpcClientClass(JNS));
   JNS.addSystemClass(initJsonSaxParserClass(JNS));
   JNS.addSystemClass(initJsonSchemaClass(JNS));
   JNS.addSystemClass(initJsonStreamWriterClass(JNS));
   init_json_functions(JNS);
   init_json_constants(JNS);
   init_cbor_functions(JNS);
   init_toon_functions(JNS);
}

void json_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
   qns->addNamespace(JNS.copy());
}

void json_module_delete() {
}
