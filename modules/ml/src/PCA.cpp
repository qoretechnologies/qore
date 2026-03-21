/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    PCA.cpp

    Qore ml module - PCA class implementation

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

#include "QC_PCA.h"

#include <Eigen/Eigenvalues>

// Extern declaration for hashdecl (defined in ml-module.cpp)
extern const TypedHashDecl* hashdeclPCAResult;

void QorePCA::fitInternal(const MatrixXd& data, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);
    doFit(data, xsink);
}

void QorePCA::doFit(const MatrixXd& data, ExceptionSink* xsink) {
    int n = static_cast<int>(data.rows());
    int d = static_cast<int>(data.cols());

    if (n == 0 || d == 0) {
        xsink->raiseException("ML-PCA-ERROR",
            "no data provided: at least one sample with one feature is required");
        return;
    }

    n_features = d;

    // Compute mean
    mean_vec = data.colwise().mean();

    // Center data
    MatrixXd centered = data.rowwise() - mean_vec.transpose();

    // Optionally scale
    if (scale) {
        // Compute standard deviation
        stddev_vec.resize(d);
        for (int j = 0; j < d; ++j) {
            double variance = centered.col(j).squaredNorm() / (n > 1 ? n - 1 : 1);
            stddev_vec(j) = std::sqrt(variance);
            if (stddev_vec(j) < 1e-12) {
                stddev_vec(j) = 1.0;  // avoid division by zero for constant features
            }
        }
        for (int j = 0; j < d; ++j) {
            centered.col(j) /= stddev_vec(j);
        }
    } else {
        stddev_vec = VectorXd::Ones(d);
    }

    // Compute covariance matrix (1/(n-1) * X^T * X)
    MatrixXd cov;
    if (n > 1) {
        cov = (centered.transpose() * centered) / (n - 1);
    } else {
        cov = centered.transpose() * centered;
    }

    // Eigendecomposition
    Eigen::SelfAdjointEigenSolver<MatrixXd> solver(cov);
    if (solver.info() != Eigen::Success) {
        xsink->raiseException("ML-PCA-ERROR",
            "eigendecomposition failed; the covariance matrix may be singular");
        return;
    }

    VectorXd eigenvalues = solver.eigenvalues();
    MatrixXd eigenvectors = solver.eigenvectors();

    // Sort eigenvalues descending (SelfAdjointEigenSolver returns ascending)
    int total_components = d;
    std::vector<int> sorted_indices(total_components);
    for (int i = 0; i < total_components; ++i) {
        sorted_indices[i] = total_components - 1 - i;
    }

    // Compute total variance
    double total_variance = eigenvalues.sum();
    if (total_variance < 1e-12) {
        total_variance = 1.0;  // avoid division by zero
    }

    // Determine number of components to keep
    int keep = total_components;
    if (n_components > 0 && n_components < keep) {
        keep = n_components;
    }

    // If variance_threshold > 0, determine components by cumulative variance
    if (variance_threshold > 0.0) {
        double cumulative = 0.0;
        int threshold_keep = 0;
        for (int i = 0; i < total_components; ++i) {
            cumulative += eigenvalues(sorted_indices[i]) / total_variance;
            ++threshold_keep;
            if (cumulative >= variance_threshold) {
                break;
            }
        }
        if (n_components > 0) {
            keep = std::min(keep, threshold_keep);
        } else {
            keep = threshold_keep;
        }
    }

    if (keep < 1) {
        keep = 1;
    }

    actual_n_components = keep;

    // Store top components (rows = components, cols = features)
    components.resize(keep, d);
    explained_variance_ratio_vec.resize(keep);
    for (int i = 0; i < keep; ++i) {
        components.row(i) = eigenvectors.col(sorted_indices[i]).transpose();
        explained_variance_ratio_vec(i) = eigenvalues(sorted_indices[i]) / total_variance;
    }

    fitted = true;
}

QoreHashNode* QorePCA::transformInternal(const RowVectorXd& point, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-PCA-ERROR",
            "model has not been fitted: call fit() or fitMatrix() before transform()");
        return nullptr;
    }
    if (point.size() != n_features) {
        xsink->raiseException("ML-PCA-ERROR",
            "input has %d features but model was fitted with %d features",
            static_cast<int>(point.size()), n_features);
        return nullptr;
    }

    // Center and scale
    RowVectorXd centered = point - mean_vec.transpose();
    if (scale) {
        for (int j = 0; j < n_features; ++j) {
            centered(j) /= stddev_vec(j);
        }
    }

    // Project onto components: result = centered * components^T
    VectorXd projected = components * centered.transpose();

    // Build result hash
    ReferenceHolder<QoreListNode> comp_list(new QoreListNode(floatTypeInfo), xsink);
    for (int i = 0; i < actual_n_components; ++i) {
        comp_list->push(projected(i), xsink);
    }

    ReferenceHolder<QoreListNode> evr_list(new QoreListNode(floatTypeInfo), xsink);
    double total = 0.0;
    for (int i = 0; i < actual_n_components; ++i) {
        evr_list->push(explained_variance_ratio_vec(i), xsink);
        total += explained_variance_ratio_vec(i);
    }

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclPCAResult, xsink), xsink);
    rv->setKeyValue("components", comp_list.release(), xsink);
    rv->setKeyValue("explained_variance_ratio", evr_list.release(), xsink);
    rv->setKeyValue("total_explained_variance", total, xsink);

    return rv.release();
}

QoreListNode* QorePCA::transformMatrix(const MatrixXd& data, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);
    return doTransformMatrix(data, xsink);
}

QoreListNode* QorePCA::fitTransformInternal(const MatrixXd& data, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);
    doFit(data, xsink);
    if (*xsink) {
        return nullptr;
    }
    return doTransformMatrix(data, xsink);
}

QoreListNode* QorePCA::doTransformMatrix(const MatrixXd& data, ExceptionSink* xsink) {
    if (!fitted) {
        xsink->raiseException("ML-PCA-ERROR",
            "model has not been fitted: call fit() or fitMatrix() before transformMatrix()");
        return nullptr;
    }
    if (data.cols() != n_features) {
        xsink->raiseException("ML-PCA-ERROR",
            "input has %d features but model was fitted with %d features",
            static_cast<int>(data.cols()), n_features);
        return nullptr;
    }

    // Center and scale all at once
    MatrixXd centered = data.rowwise() - mean_vec.transpose();
    if (scale) {
        for (int j = 0; j < n_features; ++j) {
            centered.col(j) /= stddev_vec(j);
        }
    }

    // Project: result = centered * components^T  (n x actual_n_components)
    MatrixXd projected = centered * components.transpose();

    double total_evr = 0.0;
    for (int i = 0; i < actual_n_components; ++i) {
        total_evr += explained_variance_ratio_vec(i);
    }

    ReferenceHolder<QoreListNode> rv(new QoreListNode(hashdeclPCAResult->getTypeInfo()), xsink);
    for (Eigen::Index r = 0; r < data.rows(); ++r) {
        if (r % 100 == 0 && qore_check_cancel(xsink, "PCA transformMatrix")) {
            return nullptr;
        }
        ReferenceHolder<QoreListNode> comp_list(new QoreListNode(floatTypeInfo), xsink);
        for (int i = 0; i < actual_n_components; ++i) {
            comp_list->push(projected(r, i), xsink);
        }

        ReferenceHolder<QoreListNode> evr_list(new QoreListNode(floatTypeInfo), xsink);
        for (int i = 0; i < actual_n_components; ++i) {
            evr_list->push(explained_variance_ratio_vec(i), xsink);
        }

        ReferenceHolder<QoreHashNode> h(new QoreHashNode(hashdeclPCAResult, xsink), xsink);
        h->setKeyValue("components", comp_list.release(), xsink);
        h->setKeyValue("explained_variance_ratio", evr_list.release(), xsink);
        h->setKeyValue("total_explained_variance", total_evr, xsink);

        rv->push(h.release(), xsink);
    }

    return rv.release();
}

QoreListNode* QorePCA::getComponents(ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-PCA-ERROR",
            "model has not been fitted: call fit() or fitMatrix() first");
        return nullptr;
    }

    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    for (int i = 0; i < actual_n_components; ++i) {
        ReferenceHolder<QoreListNode> row(new QoreListNode(floatTypeInfo), xsink);
        for (int j = 0; j < n_features; ++j) {
            row->push(components(i, j), xsink);
        }
        rv->push(row.release(), xsink);
    }
    return rv.release();
}

QoreListNode* QorePCA::getExplainedVarianceRatio(ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-PCA-ERROR",
            "model has not been fitted: call fit() or fitMatrix() first");
        return nullptr;
    }

    ReferenceHolder<QoreListNode> rv(new QoreListNode(floatTypeInfo), xsink);
    for (int i = 0; i < actual_n_components; ++i) {
        rv->push(explained_variance_ratio_vec(i), xsink);
    }
    return rv.release();
}

QoreListNode* QorePCA::getMean(ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-PCA-ERROR",
            "model has not been fitted: call fit() or fitMatrix() first");
        return nullptr;
    }

    return eigenVectorToQoreList(mean_vec);
}
