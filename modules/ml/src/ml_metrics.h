/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ml_metrics.h

    Qore ml module - evaluation metric function declarations

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

#ifndef _QORE_MODULE_ML_METRICS_H
#define _QORE_MODULE_ML_METRICS_H

#include "ml_common.h"

#include <set>

// ---- Classification Metrics ----

//! Compute accuracy: fraction of correct predictions
DLLLOCAL double mlAccuracy(const VectorXd& y_true, const VectorXd& y_pred, ExceptionSink* xsink);

//! Compute confusion matrix as a Qore hash (ConfusionMatrixResult)
DLLLOCAL QoreHashNode* mlConfusionMatrix(const VectorXd& y_true, const VectorXd& y_pred,
    ExceptionSink* xsink);

// Note: per-class precision/recall/f1 are available via mlClassificationReport()
// The standalone functions are static in ml_metrics.cpp (implementation helpers only)

//! Compute full classification report (ClassificationReport hashdecl)
DLLLOCAL QoreHashNode* mlClassificationReport(const VectorXd& y_true, const VectorXd& y_pred,
    ExceptionSink* xsink);

// ---- Regression Metrics ----

//! Mean Squared Error
DLLLOCAL double mlMSE(const VectorXd& y_true, const VectorXd& y_pred, ExceptionSink* xsink);

//! Root Mean Squared Error
DLLLOCAL double mlRMSE(const VectorXd& y_true, const VectorXd& y_pred, ExceptionSink* xsink);

//! Mean Absolute Error
DLLLOCAL double mlMAE(const VectorXd& y_true, const VectorXd& y_pred, ExceptionSink* xsink);

//! R² (coefficient of determination)
DLLLOCAL double mlR2Score(const VectorXd& y_true, const VectorXd& y_pred, ExceptionSink* xsink);

//! Explained variance score
DLLLOCAL double mlExplainedVariance(const VectorXd& y_true, const VectorXd& y_pred,
    ExceptionSink* xsink);

// ---- Clustering Metrics ----

//! Silhouette score (mean over all samples)
DLLLOCAL double mlSilhouetteScore(const MatrixXd& data, const VectorXd& labels,
    ExceptionSink* xsink);

//! Davies-Bouldin index (lower is better)
DLLLOCAL double mlDaviesBouldinScore(const MatrixXd& data, const VectorXd& labels,
    ExceptionSink* xsink);

//! Calinski-Harabasz index (higher is better)
DLLLOCAL double mlCalinskiHarabaszScore(const MatrixXd& data, const VectorXd& labels,
    ExceptionSink* xsink);

// ---- Helpers ----

//! Extract unique sorted class labels from a vector
DLLLOCAL std::vector<double> getUniqueLabels(const VectorXd& vec);

#endif // _QORE_MODULE_ML_METRICS_H
