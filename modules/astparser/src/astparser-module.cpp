/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  astparser-module.cpp

  Qore astparser module

  Copyright (C) 2017 - 2026 Qore Technologies s.r.o.

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

#include "qore/Qore.h"

#include "QC_AstParser.h"
#include "QC_AstTree.h"
#include "QC_AstTreeSearcher.h"
#include "ql_ast.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

static void astparser_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink);
static void astparser_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink);
static void astparser_module_delete();

extern "C" DLLEXPORT void astparser_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "astparser";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "Qore AST parser module";
    mod_info.author = "Ondrej Musil <ondrej.musil@qoretechnologies.com>";
    mod_info.url = "http://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = astparser_module_init;
    mod_info.ns_init = astparser_module_ns_init;
    mod_info.del = astparser_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}

QoreNamespace AstParserNS("Qore::astparser");

static void astparser_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    AstParserNS.addSystemClass(initAstTreeClass(AstParserNS));
    AstParserNS.addSystemClass(initAstTreeSearcherClass(AstParserNS));
    AstParserNS.addSystemClass(initAstParserClass(AstParserNS));
    init_ast_constants(AstParserNS);
}

static void astparser_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    qns->addNamespace(AstParserNS.copy());
}

static void astparser_module_delete() {
    // nothing to do here in this case
}
