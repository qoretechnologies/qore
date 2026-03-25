/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    dataframe-module.cpp

    Qore dataframe module

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "qore/Qore.h"

#include "QC_DataFrame.h"

static void dataframe_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink);
static void dataframe_module_ns_init(QoreNamespace* rns, QoreNamespace* qns,
    ExceptionSink& xsink);
static void dataframe_module_delete();

extern "C" DLLEXPORT void dataframe_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "dataframe";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "Columnar DataFrame module providing typed, SQL-like data "
                    "manipulation for Qore";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "https://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = dataframe_module_init;
    mod_info.ns_init = dataframe_module_ns_init;
    mod_info.del = dataframe_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}

QoreNamespace DFNS("Qore::DataFrame");

// Forward declarations for hashdecl init functions (generated from ql_dataframe.qpp)
DLLLOCAL TypedHashDecl* init_hashdecl_DataFrameShape(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_ColumnStats(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_CsvOptions(QoreNamespace& ns);

// Global hashdecl pointers (referenced via extern in QPP files)
const TypedHashDecl* hashdeclDataFrameShape;
const TypedHashDecl* hashdeclColumnStats;
const TypedHashDecl* hashdeclCsvOptions;

static void dataframe_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    // Phase 1: Pre-init classes
    preinitDataFrameClass();

    // Phase 2: Init hashdecls
    hashdeclDataFrameShape = init_hashdecl_DataFrameShape(DFNS);
    hashdeclColumnStats = init_hashdecl_ColumnStats(DFNS);
    hashdeclCsvOptions = init_hashdecl_CsvOptions(DFNS);

    // Phase 3: Add classes with methods
    DFNS.addSystemClass(initDataFrameClass(DFNS));
}

static void dataframe_module_ns_init(QoreNamespace* rns, QoreNamespace* qns,
        ExceptionSink& xsink) {
    qns->addNamespace(DFNS.copy());
}

static void dataframe_module_delete() {
    // nothing to clean up
}
