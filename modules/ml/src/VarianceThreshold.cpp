/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    VarianceThreshold.cpp

    VarianceThreshold implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "QC_VarianceThreshold.h"
#include "ml_serialization.h"

#include <memory>

QoreVarianceThreshold::QoreVarianceThreshold(double threshold)
    : threshold(threshold) {
}

void QoreVarianceThreshold::fit(const MatrixXd& X, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (X.rows() == 0 || X.cols() == 0) {
        xsink->raiseException("ML-VARIANCE-THRESHOLD-ERROR",
            "cannot fit on empty data");
        return;
    }

    n_features = (int)X.cols();
    variances.resize(n_features);
    support.resize(n_features);
    n_selected = 0;

    // Compute per-feature variance
    VectorXd mean = X.colwise().mean();
    for (int f = 0; f < n_features; ++f) {
        double var = 0;
        for (int64_t i = 0; i < X.rows(); ++i) {
            double d = X(i, f) - mean(f);
            var += d * d;
        }
        var /= X.rows();  // population variance
        variances(f) = var;
        support[f] = (var > threshold);
        if (support[f]) {
            ++n_selected;
        }
    }

    fitted = true;
}

MatrixXd QoreVarianceThreshold::transform(const MatrixXd& X,
        ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-VARIANCE-THRESHOLD-ERROR",
            "selector has not been fitted");
        return MatrixXd();
    }
    if ((int)X.cols() != n_features) {
        xsink->raiseException("ML-VARIANCE-THRESHOLD-ERROR",
            "input has %d features, expected %d", (int)X.cols(), n_features);
        return MatrixXd();
    }

    MatrixXd result(X.rows(), n_selected);
    int col = 0;
    for (int f = 0; f < n_features; ++f) {
        if (support[f]) {
            result.col(col) = X.col(f);
            ++col;
        }
    }
    return result;
}

MatrixXd QoreVarianceThreshold::fitTransform(const MatrixXd& X,
        ExceptionSink* xsink) {
    fit(X, xsink);
    if (*xsink) {
        return MatrixXd();
    }
    return transform(X, xsink);
}

QoreListNode* QoreVarianceThreshold::getVariances(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-VARIANCE-THRESHOLD-ERROR", "not fitted");
        return nullptr;
    }
    ReferenceHolder<QoreListNode> result(new QoreListNode(autoTypeInfo), xsink);
    for (int f = 0; f < n_features; ++f) {
        result->push(variances(f), xsink);
    }
    return result.release();
}

QoreListNode* QoreVarianceThreshold::getSupport(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-VARIANCE-THRESHOLD-ERROR", "not fitted");
        return nullptr;
    }
    ReferenceHolder<QoreListNode> result(new QoreListNode(autoTypeInfo), xsink);
    for (int f = 0; f < n_features; ++f) {
        result->push((bool)support[f], xsink);
    }
    return result.release();
}

// --- Serialization ---

std::vector<uint8_t> QoreVarianceThreshold::serializeState() const {
    std::vector<uint8_t> buf;
    MLSerialization::writeScalar(buf, threshold);
    MLSerialization::writeInt32(buf, n_features);
    MLSerialization::writeInt32(buf, n_selected);
    MLSerialization::writeVector(buf, variances);
    MLSerialization::writeInt32(buf, (int32_t)support.size());
    for (bool s : support) {
        MLSerialization::writeInt32(buf, s ? 1 : 0);
    }
    MLSerialization::writeStringVector(buf, field_names);
    return buf;
}

QoreVarianceThreshold* QoreVarianceThreshold::deserializeState(
        const uint8_t* data, size_t len, ExceptionSink* xsink) {
    const uint8_t* ptr = data;
    size_t remaining = len;
    using namespace MLSerialization;

    double thr = readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int32_t nf = readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int32_t ns = readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    VectorXd var = readVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    // the support mask is a count and one value per feature, as writeIntVector() writes them
    std::vector<int> sup_values = readIntVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    auto fnames = readStringVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }

    // the state must satisfy the invariants that fit() establishes (qore#7844)
    if (checkEnd(remaining, "VarianceThreshold", xsink)
        || checkRange(nf, 1, INT32_MAX, "VarianceThreshold", "the number of features", xsink)
        || checkSize(var.size(), nf, "VarianceThreshold", "the number of variances", xsink)
        || checkSize(sup_values.size(), nf, "VarianceThreshold", "the number of support values", xsink)) {
        return nullptr;
    }
    // fit() keeps a feature when its variance is greater than the threshold, and counts the kept features
    std::vector<bool> sup(nf);
    int64_t selected = 0;
    for (int32_t i = 0; i < nf; ++i) {
        if (checkRange(sup_values[i], 0, 1, "VarianceThreshold", "a support value", xsink)) {
            return nullptr;
        }
        bool kept = sup_values[i] != 0;
        if (kept != (var(i) > thr)) {
            stateError(xsink, "VarianceThreshold", "the support of feature %d does not agree with its variance "
                "and the threshold", (int)i);
            return nullptr;
        }
        sup[i] = kept;
        if (kept) {
            ++selected;
        }
    }
    if (checkSize(ns, selected, "VarianceThreshold", "the number of selected features", xsink)) {
        return nullptr;
    }

    std::unique_ptr<QoreVarianceThreshold> vt(new QoreVarianceThreshold(thr));
    vt->n_features = nf;
    vt->n_selected = ns;
    vt->variances = std::move(var);
    vt->support = std::move(sup);
    vt->field_names = std::move(fnames);
    vt->fitted = true;
    return vt.release();
}
