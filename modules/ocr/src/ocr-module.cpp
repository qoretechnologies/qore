/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ocr-module.cpp

    Qore OCR module — module initialization

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "qore/Qore.h"

static QoreNamespace OcrNS("Qore::Ocr");

// Forward declarations for QPP-generated init functions
DLLLOCAL void preinitTesseractEngineClass();
DLLLOCAL QoreClass* initTesseractEngineClass(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_OcrResultInfo(QoreNamespace& ns);
DLLLOCAL void init_ocr_functions(QoreNamespace& ns);

static void ocr_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    preinitTesseractEngineClass();

    init_hashdecl_OcrResultInfo(OcrNS);
    init_ocr_functions(OcrNS);

    OcrNS.addSystemClass(initTesseractEngineClass(OcrNS));
}

static void ocr_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    qns->addNamespace(OcrNS.copy());
}

static void ocr_module_delete() {
}

extern "C" DLLEXPORT void ocr_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "ocr";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "OCR module providing text extraction from images using Tesseract";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "https://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = ocr_module_init;
    mod_info.ns_init = ocr_module_ns_init;
    mod_info.del = ocr_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}
