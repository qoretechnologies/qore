/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    GMM.cpp

    Qore ml module - Gaussian Mixture Model implementation

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

#include "QC_GMM.h"

#include <algorithm>
#include <numeric>
#include <cmath>
#include <limits>

// Extern declaration for hashdecl (defined in ml-module.cpp)
extern const TypedHashDecl* hashdeclGMMResult;

static constexpr double LOG_2PI = 1.8378770664093454836;

QoreGMM::QoreGMM(int n_components, int max_iterations, double tolerance,
    int64_t seed, const std::string& covariance_type)
    : n_components(n_components), max_iterations(max_iterations),
      tolerance(tolerance), covariance_type(covariance_type) {
    if (seed == 0) {
        std::random_device rd;
        rng.seed(rd());
    } else {
        rng.seed(static_cast<unsigned>(seed));
    }
}

void QoreGMM::initializeKMeansPP(const MatrixXd& data) {
    int n = static_cast<int>(data.rows());
    int d = static_cast<int>(data.cols());

    // Choose first center randomly
    std::uniform_int_distribution<int> dist(0, n - 1);
    means.resize(n_components);
    means[0] = data.row(dist(rng)).transpose();

    // k-means++ initialization for remaining centers
    VectorXd min_distances(n);
    for (int c = 1; c < n_components; ++c) {
        for (int i = 0; i < n; ++i) {
            double min_d = std::numeric_limits<double>::max();
            for (int j = 0; j < c; ++j) {
                double d_val = (data.row(i).transpose() - means[j]).squaredNorm();
                if (d_val < min_d) {
                    min_d = d_val;
                }
            }
            min_distances(i) = min_d;
        }

        double total = min_distances.sum();
        if (total <= 0.0) {
            means[c] = data.row(dist(rng)).transpose();
            continue;
        }

        std::uniform_real_distribution<double> uni(0.0, total);
        double target = uni(rng);
        double cumsum = 0.0;
        int chosen = n - 1;
        for (int i = 0; i < n; ++i) {
            cumsum += min_distances(i);
            if (cumsum >= target) {
                chosen = i;
                break;
            }
        }
        means[c] = data.row(chosen).transpose();
    }

    // Initialize covariances as identity matrices
    covariances.resize(n_components);
    for (int c = 0; c < n_components; ++c) {
        covariances[c] = MatrixXd::Identity(d, d);
    }

    // Initialize equal weights
    weights = VectorXd::Constant(n_components, 1.0 / n_components);
}

double QoreGMM::logProbDensity(const RowVectorXd& point, int comp) const {
    int d = n_features;
    VectorXd diff = point.transpose() - means[comp];

    if (covariance_type == "diagonal") {
        // For diagonal covariances, compute efficiently
        VectorXd diag = covariances[comp].diagonal();
        double log_det = diag.array().log().sum();
        double mahal = (diff.array().square() / diag.array()).sum();
        return -0.5 * (d * LOG_2PI + log_det + mahal);
    } else {
        // Full covariance - use LLT decomposition for stability
        Eigen::LLT<MatrixXd> llt(covariances[comp]);
        if (llt.info() != Eigen::Success) {
            // Fallback: add more regularization
            MatrixXd reg_cov = covariances[comp] + MatrixXd::Identity(d, d) * 1e-4;
            Eigen::LLT<MatrixXd> llt2(reg_cov);
            if (llt2.info() != Eigen::Success) {
                return -std::numeric_limits<double>::max();
            }
            VectorXd solved = llt2.solve(diff);
            double mahal = diff.dot(solved);
            double log_det = 2.0 * llt2.matrixL().toDenseMatrix().diagonal().array().log().sum();
            return -0.5 * (d * LOG_2PI + log_det + mahal);
        }
        VectorXd solved = llt.solve(diff);
        double mahal = diff.dot(solved);
        double log_det = 2.0 * llt.matrixL().toDenseMatrix().diagonal().array().log().sum();
        return -0.5 * (d * LOG_2PI + log_det + mahal);
    }
}

MatrixXd QoreGMM::eStep(const MatrixXd& data) const {
    int n = static_cast<int>(data.rows());
    MatrixXd responsibilities(n, n_components);

    for (int i = 0; i < n; ++i) {
        // Compute log probabilities for each component
        VectorXd log_probs(n_components);
        double max_log_prob = -std::numeric_limits<double>::max();

        for (int c = 0; c < n_components; ++c) {
            log_probs(c) = std::log(weights(c)) + logProbDensity(data.row(i), c);
            if (log_probs(c) > max_log_prob) {
                max_log_prob = log_probs(c);
            }
        }

        // Log-sum-exp trick for numerical stability
        double sum_exp = 0.0;
        for (int c = 0; c < n_components; ++c) {
            sum_exp += std::exp(log_probs(c) - max_log_prob);
        }
        double log_sum = max_log_prob + std::log(sum_exp);

        for (int c = 0; c < n_components; ++c) {
            responsibilities(i, c) = std::exp(log_probs(c) - log_sum);
        }
    }

    return responsibilities;
}

void QoreGMM::mStep(const MatrixXd& data, const MatrixXd& responsibilities) {
    int n = static_cast<int>(data.rows());
    int d = n_features;
    double reg = 1e-6;

    for (int c = 0; c < n_components; ++c) {
        double resp_sum = responsibilities.col(c).sum();

        // Update weight
        weights(c) = resp_sum / n;
        if (weights(c) < 1e-10) {
            weights(c) = 1e-10;
        }

        // Update mean
        VectorXd new_mean = VectorXd::Zero(d);
        for (int i = 0; i < n; ++i) {
            new_mean += responsibilities(i, c) * data.row(i).transpose();
        }
        means[c] = new_mean / resp_sum;

        // Update covariance
        if (covariance_type == "diagonal") {
            VectorXd diag = VectorXd::Zero(d);
            for (int i = 0; i < n; ++i) {
                VectorXd diff = data.row(i).transpose() - means[c];
                diag += responsibilities(i, c) * diff.array().square().matrix();
            }
            diag /= resp_sum;
            // Regularize diagonal
            diag.array() += reg;
            covariances[c] = diag.asDiagonal();
        } else {
            MatrixXd cov = MatrixXd::Zero(d, d);
            for (int i = 0; i < n; ++i) {
                VectorXd diff = data.row(i).transpose() - means[c];
                cov += responsibilities(i, c) * (diff * diff.transpose());
            }
            cov /= resp_sum;
            // Regularize
            cov += MatrixXd::Identity(d, d) * reg;
            covariances[c] = cov;
        }
    }

    // Normalize weights
    double weight_sum = weights.sum();
    weights /= weight_sum;
}

double QoreGMM::computeLogLikelihood(const MatrixXd& data) const {
    int n = static_cast<int>(data.rows());
    double total_ll = 0.0;

    for (int i = 0; i < n; ++i) {
        VectorXd log_probs(n_components);
        double max_log_prob = -std::numeric_limits<double>::max();

        for (int c = 0; c < n_components; ++c) {
            log_probs(c) = std::log(weights(c)) + logProbDensity(data.row(i), c);
            if (log_probs(c) > max_log_prob) {
                max_log_prob = log_probs(c);
            }
        }

        double sum_exp = 0.0;
        for (int c = 0; c < n_components; ++c) {
            sum_exp += std::exp(log_probs(c) - max_log_prob);
        }
        total_ll += max_log_prob + std::log(sum_exp);
    }

    return total_ll;
}

void QoreGMM::fit(const MatrixXd& data, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (data.rows() == 0 || data.cols() == 0) {
        xsink->raiseException("ML-GMM-ERROR",
            "cannot fit on empty data: provide at least one sample with one feature");
        return;
    }

    int n = static_cast<int>(data.rows());
    if (n < n_components) {
        xsink->raiseException("ML-GMM-ERROR",
            "n_components (%d) is larger than the number of samples (%d)",
            n_components, n);
        return;
    }

    n_features = static_cast<int>(data.cols());

    // Initialize parameters using k-means++
    initializeKMeansPP(data);

    // EM iterations
    double prev_ll = -std::numeric_limits<double>::max();
    for (int iter = 0; iter < max_iterations; ++iter) {
        // E-step
        MatrixXd responsibilities = eStep(data);

        // M-step
        mStep(data, responsibilities);

        // Check convergence
        double ll = computeLogLikelihood(data);
        if (std::abs(ll - prev_ll) < tolerance) {
            log_likelihood = ll;
            break;
        }
        prev_ll = ll;
        log_likelihood = ll;
    }

    fitted = true;
}

QoreHashNode* QoreGMM::predictInternal(const RowVectorXd& point, ExceptionSink* xsink) const {
    if (point.size() != n_features) {
        xsink->raiseException("ML-GMM-ERROR",
            "input has %d features but model was trained with %d features",
            static_cast<int>(point.size()), n_features);
        return nullptr;
    }

    // Compute responsibilities for this point
    VectorXd log_probs(n_components);
    double max_log_prob = -std::numeric_limits<double>::max();

    for (int c = 0; c < n_components; ++c) {
        log_probs(c) = std::log(weights(c)) + logProbDensity(point, c);
        if (log_probs(c) > max_log_prob) {
            max_log_prob = log_probs(c);
        }
    }

    // Log-sum-exp
    double sum_exp = 0.0;
    for (int c = 0; c < n_components; ++c) {
        sum_exp += std::exp(log_probs(c) - max_log_prob);
    }
    double log_sum = max_log_prob + std::log(sum_exp);

    // Compute probabilities and find best cluster
    ReferenceHolder<QoreListNode> probs_list(new QoreListNode(autoTypeInfo), xsink);
    double max_prob = -1.0;
    int best_cluster = 0;

    for (int c = 0; c < n_components; ++c) {
        double prob = std::exp(log_probs(c) - log_sum);
        probs_list->push(prob, xsink);
        if (prob > max_prob) {
            max_prob = prob;
            best_cluster = c;
        }
    }

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclGMMResult, xsink), xsink);
    rv->setKeyValue("cluster", static_cast<int64>(best_cluster), xsink);
    rv->setKeyValue("max_probability", max_prob, xsink);
    rv->setKeyValue("probabilities", probs_list.release(), xsink);
    rv->setKeyValue("log_likelihood", log_sum, xsink);

    return rv.release();
}

QoreHashNode* QoreGMM::predict(const RowVectorXd& point, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-GMM-ERROR",
            "model has not been fitted: call fit() or fitMatrix() before predict()");
        return nullptr;
    }
    return predictInternal(point, xsink);
}

QoreListNode* QoreGMM::predictMatrix(const MatrixXd& data, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-GMM-ERROR",
            "model has not been fitted: call fit() or fitMatrix() before predictMatrix()");
        return nullptr;
    }

    if (data.cols() != n_features) {
        xsink->raiseException("ML-GMM-ERROR",
            "input has %d features but model was trained with %d features",
            static_cast<int>(data.cols()), n_features);
        return nullptr;
    }

    ReferenceHolder<QoreListNode> rv(new QoreListNode(hashdeclGMMResult->getTypeInfo()), xsink);
    for (Eigen::Index i = 0; i < data.rows(); ++i) {
        RowVectorXd row = data.row(i);
        QoreHashNode* result = predictInternal(row, xsink);
        if (*xsink) {
            return nullptr;
        }
        rv->push(result, xsink);
    }
    return rv.release();
}

QoreListNode* QoreGMM::getMeans(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-GMM-ERROR",
            "model has not been fitted: call fit() or fitMatrix() first");
        return nullptr;
    }

    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    for (int c = 0; c < n_components; ++c) {
        ReferenceHolder<QoreListNode> mean_list(new QoreListNode(autoTypeInfo), xsink);
        for (int j = 0; j < n_features; ++j) {
            mean_list->push(means[c](j), xsink);
        }
        rv->push(mean_list.release(), xsink);
    }
    return rv.release();
}

double QoreGMM::getLogLikelihood(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-GMM-ERROR",
            "model has not been fitted: call fit() or fitMatrix() first");
        return 0.0;
    }
    return log_likelihood;
}
