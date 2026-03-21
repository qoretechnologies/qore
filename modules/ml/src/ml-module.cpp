/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ml-module.cpp

    Qore ml module

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

#include "qore/Qore.h"

#include "QC_IsolationForest.h"
#include "QC_DBSCAN.h"
#include "QC_KMeans.h"
#include "QC_HoltWinters.h"
#include "QC_PCA.h"
#include "QC_SeasonalDecomposition.h"
#include "QC_LinearRegression.h"
#include "QC_LOF.h"
#include "QC_GMM.h"
#include "QC_OnnxModel.h"
#include "QC_StandardScaler.h"
#include "QC_MinMaxScaler.h"
#include "QC_Imputer.h"
#include "QC_MLPipeline.h"
#include "QC_CrossValidator.h"

static void ml_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink);
static void ml_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink);
static void ml_module_delete();

extern "C" DLLEXPORT void ml_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "ml";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "Machine learning module providing thread-safe ML algorithms "
                    "and model inference for Qore";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "https://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = ml_module_init;
    mod_info.ns_init = ml_module_ns_init;
    mod_info.del = ml_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}

QoreNamespace MLNS("Qore::ML");

// Forward declarations for hashdecl init functions (generated from ql_ml.qpp)
DLLLOCAL TypedHashDecl* init_hashdecl_IsolationForestResult(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_DBSCANResult(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_KMeansResult(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_HoltWintersResult(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_PCAResult(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_SeasonalDecompositionResult(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_LinearRegressionResult(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_LinearRegressionModelInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_LOFResult(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_GMMResult(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_OnnxTensorInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_OnnxProviderConfig(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_OnnxSessionConfig(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_OnnxModelInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_StandardScalerInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_MinMaxScalerInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_ImputerInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_ConfusionMatrixResult(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_ClassMetrics(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_ClassificationReport(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_CrossValidationFold(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_MLCapabilities(QoreNamespace& ns);

// Global hashdecl pointers (referenced by generated QPP code)
const TypedHashDecl* hashdeclIsolationForestResult;
const TypedHashDecl* hashdeclDBSCANResult;
const TypedHashDecl* hashdeclKMeansResult;
const TypedHashDecl* hashdeclHoltWintersResult;
const TypedHashDecl* hashdeclPCAResult;
const TypedHashDecl* hashdeclSeasonalDecompositionResult;
const TypedHashDecl* hashdeclLinearRegressionResult;
const TypedHashDecl* hashdeclLinearRegressionModelInfo;
const TypedHashDecl* hashdeclLOFResult;
const TypedHashDecl* hashdeclGMMResult;
const TypedHashDecl* hashdeclOnnxTensorInfo;
const TypedHashDecl* hashdeclOnnxProviderConfig;
const TypedHashDecl* hashdeclOnnxSessionConfig;
const TypedHashDecl* hashdeclOnnxModelInfo;
const TypedHashDecl* hashdeclStandardScalerInfo;
const TypedHashDecl* hashdeclMinMaxScalerInfo;
const TypedHashDecl* hashdeclImputerInfo;
const TypedHashDecl* hashdeclConfusionMatrixResult;
const TypedHashDecl* hashdeclClassMetrics;
const TypedHashDecl* hashdeclClassificationReport;
const TypedHashDecl* hashdeclCrossValidationFold;
const TypedHashDecl* hashdeclMLCapabilities;

// Forward declarations for function init (generated from ql_ml.qpp)
DLLLOCAL void init_ml_functions(QoreNamespace& ns);

static void ml_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    // Pre-initialize all classes first (creates class objects without methods)
    preinitIsolationForestClass();
    preinitDBSCANClass();
    preinitKMeansClass();
    preinitHoltWintersClass();
    preinitPCAClass();
    preinitSeasonalDecompositionClass();
    preinitLinearRegressionClass();
    preinitLOFClass();
    preinitGMMClass();
    preinitOnnxModelClass();
    preinitStandardScalerClass();
    preinitMinMaxScalerClass();
    preinitImputerClass();
    preinitMLPipelineClass();
    preinitCrossValidatorClass();

    // Initialize hashdecls (store in globals for generated QPP code)
    hashdeclIsolationForestResult = init_hashdecl_IsolationForestResult(MLNS);
    hashdeclDBSCANResult = init_hashdecl_DBSCANResult(MLNS);
    hashdeclKMeansResult = init_hashdecl_KMeansResult(MLNS);
    hashdeclHoltWintersResult = init_hashdecl_HoltWintersResult(MLNS);
    hashdeclPCAResult = init_hashdecl_PCAResult(MLNS);
    hashdeclSeasonalDecompositionResult = init_hashdecl_SeasonalDecompositionResult(MLNS);
    hashdeclLinearRegressionResult = init_hashdecl_LinearRegressionResult(MLNS);
    hashdeclLinearRegressionModelInfo = init_hashdecl_LinearRegressionModelInfo(MLNS);
    hashdeclLOFResult = init_hashdecl_LOFResult(MLNS);
    hashdeclGMMResult = init_hashdecl_GMMResult(MLNS);
    hashdeclOnnxTensorInfo = init_hashdecl_OnnxTensorInfo(MLNS);
    hashdeclOnnxProviderConfig = init_hashdecl_OnnxProviderConfig(MLNS);
    hashdeclOnnxSessionConfig = init_hashdecl_OnnxSessionConfig(MLNS);
    hashdeclOnnxModelInfo = init_hashdecl_OnnxModelInfo(MLNS);
    hashdeclStandardScalerInfo = init_hashdecl_StandardScalerInfo(MLNS);
    hashdeclMinMaxScalerInfo = init_hashdecl_MinMaxScalerInfo(MLNS);
    hashdeclImputerInfo = init_hashdecl_ImputerInfo(MLNS);
    hashdeclConfusionMatrixResult = init_hashdecl_ConfusionMatrixResult(MLNS);
    hashdeclClassMetrics = init_hashdecl_ClassMetrics(MLNS);
    hashdeclClassificationReport = init_hashdecl_ClassificationReport(MLNS);
    hashdeclCrossValidationFold = init_hashdecl_CrossValidationFold(MLNS);
    hashdeclMLCapabilities = init_hashdecl_MLCapabilities(MLNS);

    // Add classes to namespace (adds methods that may reference other classes)
    MLNS.addSystemClass(initIsolationForestClass(MLNS));
    MLNS.addSystemClass(initDBSCANClass(MLNS));
    MLNS.addSystemClass(initKMeansClass(MLNS));
    MLNS.addSystemClass(initHoltWintersClass(MLNS));
    MLNS.addSystemClass(initPCAClass(MLNS));
    MLNS.addSystemClass(initSeasonalDecompositionClass(MLNS));
    MLNS.addSystemClass(initLinearRegressionClass(MLNS));
    MLNS.addSystemClass(initLOFClass(MLNS));
    MLNS.addSystemClass(initGMMClass(MLNS));
    MLNS.addSystemClass(initOnnxModelClass(MLNS));
    MLNS.addSystemClass(initStandardScalerClass(MLNS));
    MLNS.addSystemClass(initMinMaxScalerClass(MLNS));
    MLNS.addSystemClass(initImputerClass(MLNS));
    MLNS.addSystemClass(initMLPipelineClass(MLNS));
    MLNS.addSystemClass(initCrossValidatorClass(MLNS));

    // Add namespace-level functions
    init_ml_functions(MLNS);
}

static void ml_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    qns->addNamespace(MLNS.copy());
}

static void ml_module_delete() {
    ExceptionSink xsink;
    MLNS.clear(&xsink);
}
