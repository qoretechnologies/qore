/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SVM.cpp

    Support Vector Machine implementation (simplified SMO)

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "QC_SVM.h"
#include "ml_serialization.h"

#include <algorithm>
#include <cmath>
#include <set>

extern const TypedHashDecl* hashdeclSVMResult;

QoreSVM::QoreSVM(double C, const std::string& kernel, double gamma,
        int degree, double tol, int max_iter, int64_t seed)
    : C(C), kernel(kernel), gamma(gamma), degree(degree), tol(tol),
      max_iter(max_iter),
      rng(seed == 0 ? std::random_device{}() : (unsigned)seed) {
}

double QoreSVM::kernelFunc(const RowVectorXd& a, const RowVectorXd& b) const {
    if (kernel == "linear") {
        return a.dot(b);
    }
    if (kernel == "rbf") {
        double dist_sq = (a - b).squaredNorm();
        return std::exp(-gamma * dist_sq);
    }
    // Default: linear
    return a.dot(b);
}

QoreSVM::BinarySVM QoreSVM::trainBinary(const MatrixXd& X, const VectorXd& y,
        double pos_class, double neg_class, ExceptionSink* xsink) {
    // Convert labels to +1/-1
    int n = (int)X.rows();
    VectorXd y_bin(n);
    for (int i = 0; i < n; ++i) {
        y_bin(i) = (y(i) == pos_class) ? 1.0 : -1.0;
    }

    // Simplified SMO (Platt 1998, simplified version)
    VectorXd alpha = VectorXd::Zero(n);
    double b = 0.0;

    // Precompute kernel matrix for small datasets (< 5000 samples)
    MatrixXd K;
    bool use_cache = (n <= 5000);
    if (use_cache) {
        K.resize(n, n);
        for (int i = 0; i < n; ++i) {
            for (int j = i; j < n; ++j) {
                double val = kernelFunc(X.row(i), X.row(j));
                K(i, j) = val;
                K(j, i) = val;
            }
        }
    }

    auto getK = [&](int i, int j) -> double {
        if (use_cache) {
            return K(i, j);
        }
        return kernelFunc(X.row(i), X.row(j));
    };

    // SMO main loop
    for (int iter = 0; iter < max_iter; ++iter) {
        if ((iter % 100) == 0 && iter > 0) {
            if (qore_check_cancel(xsink, "training SVM")) {
                return {};
            }
        }

        int num_changed = 0;
        for (int i = 0; i < n; ++i) {
            // Compute error for i
            double E_i = -y_bin(i);
            for (int k = 0; k < n; ++k) {
                if (alpha(k) > 0) {
                    E_i += alpha(k) * y_bin(k) * getK(k, i);
                }
            }
            E_i += b;
            E_i -= y_bin(i);  // E_i = f(x_i) - y_i where f(x) = sum(alpha*y*K) + b

            // Recompute: E_i = f(x_i) - y_i
            double f_i = b;
            for (int k = 0; k < n; ++k) {
                if (alpha(k) > 0) {
                    f_i += alpha(k) * y_bin(k) * getK(k, i);
                }
            }
            E_i = f_i - y_bin(i);

            // Check KKT violations
            if ((y_bin(i) * E_i < -tol && alpha(i) < C)
                    || (y_bin(i) * E_i > tol && alpha(i) > 0)) {
                // Select j randomly
                int j = i;
                std::uniform_int_distribution<int> dist(0, n - 2);
                j = dist(rng);
                if (j >= i) { ++j; }

                // Compute error for j
                double f_j = b;
                for (int k = 0; k < n; ++k) {
                    if (alpha(k) > 0) {
                        f_j += alpha(k) * y_bin(k) * getK(k, j);
                    }
                }
                double E_j = f_j - y_bin(j);

                double alpha_i_old = alpha(i);
                double alpha_j_old = alpha(j);

                // Compute bounds
                double L, H;
                if (y_bin(i) != y_bin(j)) {
                    L = std::max(0.0, alpha(j) - alpha(i));
                    H = std::min(C, C + alpha(j) - alpha(i));
                } else {
                    L = std::max(0.0, alpha(i) + alpha(j) - C);
                    H = std::min(C, alpha(i) + alpha(j));
                }
                if (L >= H) { continue; }

                double eta = 2.0 * getK(i, j) - getK(i, i) - getK(j, j);
                if (eta >= 0) { continue; }

                // Update alpha_j
                alpha(j) -= y_bin(j) * (E_i - E_j) / eta;
                alpha(j) = std::max(L, std::min(H, alpha(j)));

                if (std::abs(alpha(j) - alpha_j_old) < 1e-5) { continue; }

                // Update alpha_i
                alpha(i) += y_bin(i) * y_bin(j) * (alpha_j_old - alpha(j));

                // Update bias
                double b1 = b - E_i
                    - y_bin(i) * (alpha(i) - alpha_i_old) * getK(i, i)
                    - y_bin(j) * (alpha(j) - alpha_j_old) * getK(i, j);
                double b2 = b - E_j
                    - y_bin(i) * (alpha(i) - alpha_i_old) * getK(i, j)
                    - y_bin(j) * (alpha(j) - alpha_j_old) * getK(j, j);

                if (alpha(i) > 0 && alpha(i) < C) {
                    b = b1;
                } else if (alpha(j) > 0 && alpha(j) < C) {
                    b = b2;
                } else {
                    b = (b1 + b2) / 2.0;
                }

                ++num_changed;
            }
        }

        if (num_changed == 0) {
            break;  // converged
        }
    }

    // Extract support vectors (alpha > threshold)
    BinarySVM model;
    model.b = b;
    model.class_pos = pos_class;
    model.class_neg = neg_class;

    std::vector<int> sv_indices;
    for (int i = 0; i < n; ++i) {
        if (alpha(i) > 1e-7) {
            sv_indices.push_back(i);
        }
    }

    int n_sv = (int)sv_indices.size();
    model.support_X.resize(n_sv, n_features);
    model.support_y.resize(n_sv);
    model.support_alpha.resize(n_sv);
    for (int i = 0; i < n_sv; ++i) {
        model.support_X.row(i) = X.row(sv_indices[i]);
        model.support_y(i) = y_bin(sv_indices[i]);
        model.support_alpha(i) = alpha(sv_indices[i]);
    }

    return model;
}

double QoreSVM::predictBinary(const BinarySVM& model,
        const RowVectorXd& point) const {
    double decision = model.b;
    for (int i = 0; i < model.support_X.rows(); ++i) {
        decision += model.support_alpha(i) * model.support_y(i)
            * kernelFunc(model.support_X.row(i), point);
    }
    return decision;
}

void QoreSVM::fit(const MatrixXd& X, const VectorXd& y, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (X.rows() == 0 || X.cols() == 0) {
        xsink->raiseException("ML-SVM-ERROR", "cannot fit on empty data");
        return;
    }
    if (X.rows() != y.size()) {
        xsink->raiseException("ML-SVM-ERROR",
            "X has " QLLD " rows but y has " QLLD " elements",
            (int64_t)X.rows(), (int64_t)y.size());
        return;
    }

    n_features = (int)X.cols();

    // Auto-compute gamma if 0
    if (gamma <= 0) {
        gamma = 1.0 / n_features;
    }

    // Determine unique classes
    std::set<double> unique_classes;
    for (int64_t i = 0; i < y.size(); ++i) {
        unique_classes.insert(y(i));
    }
    classes.assign(unique_classes.begin(), unique_classes.end());
    n_classes = (int)classes.size();

    models.clear();

    if (n_classes == 2) {
        // Binary: single model
        models.push_back(trainBinary(X, y, classes[1], classes[0], xsink));
        if (*xsink) { return; }
    } else {
        // Multiclass: one-vs-one
        for (int i = 0; i < n_classes; ++i) {
            for (int j = i + 1; j < n_classes; ++j) {
                if (qore_check_cancel(xsink, "training SVM multiclass")) {
                    models.clear();
                    return;
                }
                // Extract samples for these two classes
                std::vector<int> indices;
                for (int64_t k = 0; k < y.size(); ++k) {
                    if (y(k) == classes[i] || y(k) == classes[j]) {
                        indices.push_back(k);
                    }
                }
                MatrixXd X_sub(indices.size(), n_features);
                VectorXd y_sub(indices.size());
                for (size_t k = 0; k < indices.size(); ++k) {
                    X_sub.row(k) = X.row(indices[k]);
                    y_sub(k) = y(indices[k]);
                }
                models.push_back(trainBinary(X_sub, y_sub, classes[j], classes[i], xsink));
                if (*xsink) { return; }
            }
        }
    }

    fitted = true;
}

QoreHashNode* QoreSVM::predictInternal(const RowVectorXd& point,
        ExceptionSink* xsink) const {
    if (n_classes == 2) {
        double decision = predictBinary(models[0], point);
        int predicted = (decision >= 0)
            ? (int)models[0].class_pos : (int)models[0].class_neg;
        double confidence = 1.0 / (1.0 + std::exp(-std::abs(decision)));

        ReferenceHolder<QoreHashNode> rv(
            new QoreHashNode(hashdeclSVMResult, xsink), xsink);
        rv->setKeyValue("predicted_class", (int64_t)predicted, xsink);
        rv->setKeyValue("decision_value", decision, xsink);
        rv->setKeyValue("confidence", confidence, xsink);
        return rv.release();
    }

    // Multiclass: one-vs-one voting
    std::vector<int> votes(n_classes, 0);
    int model_idx = 0;
    for (int i = 0; i < n_classes; ++i) {
        for (int j = i + 1; j < n_classes; ++j) {
            double decision = predictBinary(models[model_idx], point);
            if (decision >= 0) {
                votes[j]++;
            } else {
                votes[i]++;
            }
            ++model_idx;
        }
    }

    int best = 0;
    for (int i = 1; i < n_classes; ++i) {
        if (votes[i] > votes[best]) { best = i; }
    }

    ReferenceHolder<QoreHashNode> rv(
        new QoreHashNode(hashdeclSVMResult, xsink), xsink);
    rv->setKeyValue("predicted_class", (int64_t)classes[best], xsink);
    rv->setKeyValue("decision_value", 0.0, xsink);
    rv->setKeyValue("confidence", (double)votes[best] / ((double)n_classes * (n_classes - 1) / 2.0),
        xsink);
    return rv.release();
}

QoreHashNode* QoreSVM::predict(const RowVectorXd& point,
        ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-SVM-ERROR", "model has not been fitted");
        return nullptr;
    }
    if (point.size() != n_features) {
        xsink->raiseException("ML-SVM-ERROR",
            "input has %d features, expected %d", (int)point.size(), n_features);
        return nullptr;
    }
    return predictInternal(point, xsink);
}

QoreListNode* QoreSVM::predictMatrix(const MatrixXd& X,
        ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-SVM-ERROR", "model has not been fitted");
        return nullptr;
    }
    if ((int)X.cols() != n_features) {
        xsink->raiseException("ML-SVM-ERROR",
            "input has %d features, expected %d", (int)X.cols(), n_features);
        return nullptr;
    }

    ReferenceHolder<QoreListNode> results(
        new QoreListNode(hashdeclSVMResult->getTypeInfo()), xsink);
    for (int64_t i = 0; i < X.rows(); ++i) {
        if ((i % 100) == 0 && i > 0) {
            if (qore_check_cancel(xsink, "predicting with SVM")) {
                return nullptr;
            }
        }
        QoreHashNode* result = predictInternal(X.row(i), xsink);
        if (*xsink) { return nullptr; }
        results->push(result, xsink);
    }
    return results.release();
}

// --- Serialization ---

std::vector<uint8_t> QoreSVM::serializeState() const {
    std::vector<uint8_t> buf;
    MLSerialization::writeScalar(buf, C);
    MLSerialization::writeString(buf, kernel);
    MLSerialization::writeScalar(buf, gamma);
    MLSerialization::writeInt32(buf, degree);
    MLSerialization::writeScalar(buf, tol);
    MLSerialization::writeInt32(buf, max_iter);
    MLSerialization::writeInt32(buf, n_features);
    MLSerialization::writeInt32(buf, n_classes);
    MLSerialization::writeInt32(buf, (int32_t)classes.size());
    for (double c : classes) {
        MLSerialization::writeScalar(buf, c);
    }
    MLSerialization::writeStringVector(buf, field_names);
    MLSerialization::writeString(buf, target_field);
    MLSerialization::writeInt32(buf, (int32_t)models.size());
    for (const auto& m : models) {
        MLSerialization::writeScalar(buf, m.b);
        MLSerialization::writeScalar(buf, m.class_pos);
        MLSerialization::writeScalar(buf, m.class_neg);
        MLSerialization::writeMatrix(buf, m.support_X);
        MLSerialization::writeVector(buf, m.support_y);
        MLSerialization::writeVector(buf, m.support_alpha);
    }
    return buf;
}

QoreSVM* QoreSVM::deserializeState(const uint8_t* data, size_t len,
        ExceptionSink* xsink) {
    const uint8_t* ptr = data;
    size_t remaining = len;

    double C = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::string kernel = MLSerialization::readString(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double gamma = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int degree = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double tol = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int max_iter = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int n_features = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int n_classes = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int ncl = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::vector<double> classes(ncl);
    for (int i = 0; i < ncl; ++i) {
        classes[i] = MLSerialization::readScalar(ptr, remaining, xsink);
        if (*xsink) { return nullptr; }
    }
    auto fnames = MLSerialization::readStringVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::string tf = MLSerialization::readString(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int nm = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::vector<BinarySVM> models(nm);
    for (int i = 0; i < nm; ++i) {
        models[i].b = MLSerialization::readScalar(ptr, remaining, xsink);
        if (*xsink) { return nullptr; }
        models[i].class_pos = MLSerialization::readScalar(ptr, remaining, xsink);
        if (*xsink) { return nullptr; }
        models[i].class_neg = MLSerialization::readScalar(ptr, remaining, xsink);
        if (*xsink) { return nullptr; }
        models[i].support_X = MLSerialization::readMatrix(ptr, remaining, xsink);
        if (*xsink) { return nullptr; }
        models[i].support_y = MLSerialization::readVector(ptr, remaining, xsink);
        if (*xsink) { return nullptr; }
        models[i].support_alpha = MLSerialization::readVector(ptr, remaining, xsink);
        if (*xsink) { return nullptr; }
    }

    auto* svm = new QoreSVM(C, kernel, gamma, degree, tol, max_iter, 42);
    svm->n_features = n_features;
    svm->n_classes = n_classes;
    svm->classes = std::move(classes);
    svm->field_names = std::move(fnames);
    svm->target_field = std::move(tf);
    svm->models = std::move(models);
    svm->fitted = true;
    return svm;
}
