/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  i18n-module.cpp

  Qore i18n module

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

#include "i18n-module.h"

QoreNamespace I18nNS("Qore::I18n");

static void i18n_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    init_i18n_functions(I18nNS);
}

static void i18n_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    qns->addNamespace(I18nNS.copy());
}

static void i18n_module_delete() {
}

extern "C" DLLEXPORT void i18n_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "i18n";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "Internationalization module with ICU-backed locale data";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "https://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = i18n_module_init;
    mod_info.ns_init = i18n_module_ns_init;
    mod_info.del = i18n_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}
