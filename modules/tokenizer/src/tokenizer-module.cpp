/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    tokenizer-module.cpp

    Qore tokenizer module - HuggingFace-compatible text tokenization

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "qore/Qore.h"

static QoreNamespace TokNS("Qore::Tokenizer");

// Forward declarations for QPP-generated init functions
DLLLOCAL void preinitHFTokenizerClass();
DLLLOCAL QoreClass* initHFTokenizerClass(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_TokenizerEncoding(QoreNamespace& ns);

static void tokenizer_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    preinitHFTokenizerClass();

    init_hashdecl_TokenizerEncoding(TokNS);

    TokNS.addSystemClass(initHFTokenizerClass(TokNS));
}

static void tokenizer_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    qns->addNamespace(TokNS.copy());
}

static void tokenizer_module_delete() {
}

extern "C" DLLEXPORT void tokenizer_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "tokenizer";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "HuggingFace-compatible text tokenization module providing "
                    "BPE, WordPiece, and Unigram tokenizers with full "
                    "tokenizer.json support";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "https://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = tokenizer_module_init;
    mod_info.ns_init = tokenizer_module_ns_init;
    mod_info.del = tokenizer_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}
