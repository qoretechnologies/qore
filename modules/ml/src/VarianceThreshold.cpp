/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    VarianceThreshold.cpp

    VarianceThreshold implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "QC_VarianceThreshold.h"
#include "ml_serialization.h"

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

    double thr = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int nf = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int ns = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    VectorXd var = MLSerialization::readVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int nsup = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::vector<bool> sup(nsup);
    for (int i = 0; i < nsup; ++i) {
        sup[i] = MLSerialization::readInt32(ptr, remaining, xsink) != 0;
        if (*xsink) { return nullptr; }
    }
    auto fnames = MLSerialization::readStringVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }

    auto* vt = new QoreVarianceThreshold(thr);
    vt->n_features = nf;
    vt->n_selected = ns;
    vt->variances = std::move(var);
    vt->support = std::move(sup);
    vt->field_names = std::move(fnames);
    vt->fitted = true;
    return vt;
}
