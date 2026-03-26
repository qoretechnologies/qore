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

#include "ml_serialization.h"
#include "QC_IsolationForest.h"
#include "QC_DBSCAN.h"
#include "QC_KMeans.h"
#include "QC_HoltWinters.h"
#include "QC_PCA.h"
#include "QC_SeasonalDecomposition.h"
#include "QC_LinearRegression.h"
#include "QC_Ridge.h"
#include "QC_Lasso.h"
#include "QC_ElasticNet.h"
#include "QC_LogisticRegression.h"
#include "QC_LOF.h"
#include "QC_KNN.h"
#include "QC_GMM.h"
#include "QC_OnnxModel.h"
#include "QC_StandardScaler.h"
#include "QC_MinMaxScaler.h"
#include "QC_Imputer.h"
#include "QC_MLPipeline.h"
#include "QC_CrossValidator.h"
#include "QC_DecisionTree.h"
#include "QC_RandomForest.h"
#include "QC_GradientBoostedTrees.h"
#include "QC_SVM.h"
#include "QC_NaiveBayes.h"
#include "QC_OneHotEncoder.h"
#include "QC_LabelEncoder.h"
#include "QC_VarianceThreshold.h"
#include "QC_PolynomialFeatures.h"
#include "QC_SelectKBest.h"
#include "QC_RFE.h"
#include "QC_ADWIN.h"
#include "QC_PageHinkley.h"
#include "QC_DDM.h"
#include "QC_DateTimeFeatures.h"
#include "QC_TextFeatures.h"
#include "QC_SparseMatrix.h"

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
DLLLOCAL TypedHashDecl* init_hashdecl_RidgeResult(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_RidgeModelInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_LassoResult(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_LassoModelInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_ElasticNetResult(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_ElasticNetModelInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_LogisticRegressionResult(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_LOFResult(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_GMMResult(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_KNNClassificationResult(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_KNNRegressionResult(QoreNamespace& ns);
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
DLLLOCAL TypedHashDecl* init_hashdecl_DecisionTreeClassificationResult(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_DecisionTreeRegressionResult(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_FeatureImportanceInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_RandomForestClassificationResult(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_RandomForestRegressionResult(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_GBTClassificationResult(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_GBTRegressionResult(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_SVMResult(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_NaiveBayesResult(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_HypothesisTestResult(QoreNamespace& ns);
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
const TypedHashDecl* hashdeclRidgeResult;
const TypedHashDecl* hashdeclRidgeModelInfo;
const TypedHashDecl* hashdeclLassoResult;
const TypedHashDecl* hashdeclLassoModelInfo;
const TypedHashDecl* hashdeclElasticNetResult;
const TypedHashDecl* hashdeclElasticNetModelInfo;
const TypedHashDecl* hashdeclLogisticRegressionResult;
const TypedHashDecl* hashdeclLOFResult;
const TypedHashDecl* hashdeclGMMResult;
const TypedHashDecl* hashdeclKNNClassificationResult;
const TypedHashDecl* hashdeclKNNRegressionResult;
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
const TypedHashDecl* hashdeclDecisionTreeClassificationResult;
const TypedHashDecl* hashdeclDecisionTreeRegressionResult;
const TypedHashDecl* hashdeclFeatureImportanceInfo;
const TypedHashDecl* hashdeclRandomForestClassificationResult;
const TypedHashDecl* hashdeclRandomForestRegressionResult;
const TypedHashDecl* hashdeclGBTClassificationResult;
const TypedHashDecl* hashdeclGBTRegressionResult;
const TypedHashDecl* hashdeclSVMResult;
const TypedHashDecl* hashdeclNaiveBayesResult;
const TypedHashDecl* hashdeclHypothesisTestResult;
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
    preinitRidgeClass();
    preinitLassoClass();
    preinitElasticNetClass();
    preinitLogisticRegressionClass();
    preinitLOFClass();
    preinitKNNClass();
    preinitGMMClass();
    preinitOnnxModelClass();
    preinitStandardScalerClass();
    preinitMinMaxScalerClass();
    preinitImputerClass();
    preinitMLPipelineClass();
    preinitCrossValidatorClass();
    preinitDecisionTreeClass();
    preinitRandomForestClass();
    preinitGradientBoostedTreesClass();
    preinitSVMClass();
    preinitNaiveBayesClass();
    preinitOneHotEncoderClass();
    preinitLabelEncoderClass();
    preinitVarianceThresholdClass();
    preinitPolynomialFeaturesClass();
    preinitSelectKBestClass();
    preinitRFEClass();
    preinitADWINClass();
    preinitPageHinkleyClass();
    preinitDDMClass();
    preinitDateTimeFeaturesClass();
    preinitTextFeaturesClass();
    preinitSparseMatrixClass();

    // Initialize hashdecls (store in globals for generated QPP code)
    hashdeclIsolationForestResult = init_hashdecl_IsolationForestResult(MLNS);
    hashdeclDBSCANResult = init_hashdecl_DBSCANResult(MLNS);
    hashdeclKMeansResult = init_hashdecl_KMeansResult(MLNS);
    hashdeclHoltWintersResult = init_hashdecl_HoltWintersResult(MLNS);
    hashdeclPCAResult = init_hashdecl_PCAResult(MLNS);
    hashdeclSeasonalDecompositionResult = init_hashdecl_SeasonalDecompositionResult(MLNS);
    hashdeclLinearRegressionResult = init_hashdecl_LinearRegressionResult(MLNS);
    hashdeclLinearRegressionModelInfo = init_hashdecl_LinearRegressionModelInfo(MLNS);
    hashdeclRidgeResult = init_hashdecl_RidgeResult(MLNS);
    hashdeclRidgeModelInfo = init_hashdecl_RidgeModelInfo(MLNS);
    hashdeclLassoResult = init_hashdecl_LassoResult(MLNS);
    hashdeclLassoModelInfo = init_hashdecl_LassoModelInfo(MLNS);
    hashdeclElasticNetResult = init_hashdecl_ElasticNetResult(MLNS);
    hashdeclElasticNetModelInfo = init_hashdecl_ElasticNetModelInfo(MLNS);
    hashdeclLogisticRegressionResult = init_hashdecl_LogisticRegressionResult(MLNS);
    hashdeclLOFResult = init_hashdecl_LOFResult(MLNS);
    hashdeclGMMResult = init_hashdecl_GMMResult(MLNS);
    hashdeclKNNClassificationResult = init_hashdecl_KNNClassificationResult(MLNS);
    hashdeclKNNRegressionResult = init_hashdecl_KNNRegressionResult(MLNS);
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
    hashdeclDecisionTreeClassificationResult = init_hashdecl_DecisionTreeClassificationResult(MLNS);
    hashdeclDecisionTreeRegressionResult = init_hashdecl_DecisionTreeRegressionResult(MLNS);
    hashdeclFeatureImportanceInfo = init_hashdecl_FeatureImportanceInfo(MLNS);
    hashdeclRandomForestClassificationResult = init_hashdecl_RandomForestClassificationResult(MLNS);
    hashdeclRandomForestRegressionResult = init_hashdecl_RandomForestRegressionResult(MLNS);
    hashdeclGBTClassificationResult = init_hashdecl_GBTClassificationResult(MLNS);
    hashdeclGBTRegressionResult = init_hashdecl_GBTRegressionResult(MLNS);
    hashdeclSVMResult = init_hashdecl_SVMResult(MLNS);
    hashdeclNaiveBayesResult = init_hashdecl_NaiveBayesResult(MLNS);
    hashdeclHypothesisTestResult = init_hashdecl_HypothesisTestResult(MLNS);
    hashdeclMLCapabilities = init_hashdecl_MLCapabilities(MLNS);

    // Add classes to namespace (adds methods that may reference other classes)
    MLNS.addSystemClass(initIsolationForestClass(MLNS));
    MLNS.addSystemClass(initDBSCANClass(MLNS));
    MLNS.addSystemClass(initKMeansClass(MLNS));
    MLNS.addSystemClass(initHoltWintersClass(MLNS));
    MLNS.addSystemClass(initPCAClass(MLNS));
    MLNS.addSystemClass(initSeasonalDecompositionClass(MLNS));
    MLNS.addSystemClass(initLinearRegressionClass(MLNS));
    MLNS.addSystemClass(initRidgeClass(MLNS));
    MLNS.addSystemClass(initLassoClass(MLNS));
    MLNS.addSystemClass(initElasticNetClass(MLNS));
    MLNS.addSystemClass(initLogisticRegressionClass(MLNS));
    MLNS.addSystemClass(initLOFClass(MLNS));
    MLNS.addSystemClass(initKNNClass(MLNS));
    MLNS.addSystemClass(initGMMClass(MLNS));
    MLNS.addSystemClass(initOnnxModelClass(MLNS));
    MLNS.addSystemClass(initStandardScalerClass(MLNS));
    MLNS.addSystemClass(initMinMaxScalerClass(MLNS));
    MLNS.addSystemClass(initImputerClass(MLNS));
    MLNS.addSystemClass(initMLPipelineClass(MLNS));
    MLNS.addSystemClass(initCrossValidatorClass(MLNS));
    MLNS.addSystemClass(initDecisionTreeClass(MLNS));
    MLNS.addSystemClass(initRandomForestClass(MLNS));
    MLNS.addSystemClass(initGradientBoostedTreesClass(MLNS));
    MLNS.addSystemClass(initSVMClass(MLNS));
    MLNS.addSystemClass(initNaiveBayesClass(MLNS));
    MLNS.addSystemClass(initOneHotEncoderClass(MLNS));
    MLNS.addSystemClass(initLabelEncoderClass(MLNS));
    MLNS.addSystemClass(initVarianceThresholdClass(MLNS));
    MLNS.addSystemClass(initPolynomialFeaturesClass(MLNS));
    MLNS.addSystemClass(initSelectKBestClass(MLNS));
    MLNS.addSystemClass(initRFEClass(MLNS));
    MLNS.addSystemClass(initADWINClass(MLNS));
    MLNS.addSystemClass(initPageHinkleyClass(MLNS));
    MLNS.addSystemClass(initDDMClass(MLNS));
    MLNS.addSystemClass(initDateTimeFeaturesClass(MLNS));
    MLNS.addSystemClass(initTextFeaturesClass(MLNS));
    MLNS.addSystemClass(initSparseMatrixClass(MLNS));

    // Add namespace-level functions
    init_ml_functions(MLNS);

    // Register algorithm deserializers for model persistence
    MLSerialization::registerAlgorithm("KMeans", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QoreKMeans::deserializeState(data, len, xsink);
    });
    MLSerialization::registerAlgorithm("LinearRegression", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QoreLinearRegression::deserializeState(data, len, xsink);
    });
    MLSerialization::registerAlgorithm("Ridge", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QoreRidge::deserializeState(data, len, xsink);
    });
    MLSerialization::registerAlgorithm("Lasso", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QoreLasso::deserializeState(data, len, xsink);
    });
    MLSerialization::registerAlgorithm("ElasticNet", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QoreElasticNet::deserializeState(data, len, xsink);
    });
    MLSerialization::registerAlgorithm("LogisticRegression", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QoreLogisticRegression::deserializeState(data, len, xsink);
    });
    MLSerialization::registerAlgorithm("PCA", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QorePCA::deserializeState(data, len, xsink);
    });
    MLSerialization::registerAlgorithm("IsolationForest", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QoreIsolationForest::deserializeState(data, len, xsink);
    });
    MLSerialization::registerAlgorithm("LOF", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QoreLOF::deserializeState(data, len, xsink);
    });
    MLSerialization::registerAlgorithm("GMM", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QoreGMM::deserializeState(data, len, xsink);
    });
    MLSerialization::registerAlgorithm("HoltWinters", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QoreHoltWinters::deserializeState(data, len, xsink);
    });
    MLSerialization::registerAlgorithm("KNN", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QoreKNN::deserializeState(data, len, xsink);
    });
    MLSerialization::registerAlgorithm("StandardScaler", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QoreStandardScaler::deserializeState(data, len, xsink);
    });
    MLSerialization::registerAlgorithm("MinMaxScaler", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QoreMinMaxScaler::deserializeState(data, len, xsink);
    });
    MLSerialization::registerAlgorithm("Imputer", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QoreImputer::deserializeState(data, len, xsink);
    });
    MLSerialization::registerAlgorithm("DecisionTree", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QoreDecisionTree::deserializeState(data, len, xsink);
    });
    MLSerialization::registerAlgorithm("RandomForest", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QoreRandomForest::deserializeState(data, len, xsink);
    });
    MLSerialization::registerAlgorithm("GradientBoostedTrees", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QoreGradientBoostedTrees::deserializeState(data, len, xsink);
    });
    MLSerialization::registerAlgorithm("SVM", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QoreSVM::deserializeState(data, len, xsink);
    });
    MLSerialization::registerAlgorithm("NaiveBayes", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QoreNaiveBayes::deserializeState(data, len, xsink);
    });
    MLSerialization::registerAlgorithm("OneHotEncoder", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QoreOneHotEncoder::deserializeState(data, len, xsink);
    });
    MLSerialization::registerAlgorithm("LabelEncoder", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QoreLabelEncoder::deserializeState(data, len, xsink);
    });
    MLSerialization::registerAlgorithm("VarianceThreshold", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QoreVarianceThreshold::deserializeState(data, len, xsink);
    });
    MLSerialization::registerAlgorithm("PolynomialFeatures", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QorePolynomialFeatures::deserializeState(data, len, xsink);
    });
    MLSerialization::registerAlgorithm("SelectKBest", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QoreSelectKBest::deserializeState(data, len, xsink);
    });
    MLSerialization::registerAlgorithm("RFE", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QoreRFE::deserializeState(data, len, xsink);
    });
    MLSerialization::registerAlgorithm("SparseMatrix", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QoreSparseMatrix::deserializeState(data, len, xsink);
    });
    MLSerialization::registerAlgorithm("DateTimeFeatures", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QoreDateTimeFeatures::deserializeState(data, len, xsink);
    });
    MLSerialization::registerAlgorithm("TextFeatures", [](const uint8_t* data, size_t len,
        ExceptionSink* xsink) -> AbstractPrivateData* {
        return QoreTextFeatures::deserializeState(data, len, xsink);
    });
}

static void ml_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    qns->addNamespace(MLNS.copy());
}

static void ml_module_delete() {
    ExceptionSink xsink;
    MLNS.clear(&xsink);
}
