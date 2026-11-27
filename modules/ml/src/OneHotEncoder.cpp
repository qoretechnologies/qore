/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    OneHotEncoder.cpp

    OneHotEncoder implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "QC_OneHotEncoder.h"
#include "ml_serialization.h"

#include <algorithm>
#include <memory>
#include <set>
#include <unordered_map>

#include "qore/intern/qore_keyed_hash.h"

QoreOneHotEncoder::QoreOneHotEncoder(bool drop_first)
    : drop_first(drop_first) {
}

void QoreOneHotEncoder::fit(const MatrixXd& X, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (X.rows() == 0 || X.cols() == 0) {
        xsink->raiseException("ML-ONE-HOT-ENCODER-ERROR", "cannot fit on empty data");
        return;
    }

    n_features = (int)X.cols();
    categories.resize(n_features);
    n_output_cols = 0;

    for (int f = 0; f < n_features; ++f) {
        std::set<int> unique;
        for (int64_t i = 0; i < X.rows(); ++i) {
            unique.insert((int)X(i, f));
        }
        categories[f].assign(unique.begin(), unique.end());
        int n_cats = (int)categories[f].size();
        n_output_cols += drop_first ? (n_cats - 1) : n_cats;
    }

    fitted = true;
}

MatrixXd QoreOneHotEncoder::transform(const MatrixXd& X, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-ONE-HOT-ENCODER-ERROR", "encoder has not been fitted");
        return MatrixXd();
    }
    if ((int)X.cols() != n_features) {
        xsink->raiseException("ML-ONE-HOT-ENCODER-ERROR",
            "input has %d features, expected %d", (int)X.cols(), n_features);
        return MatrixXd();
    }

    int64_t n = X.rows();
    MatrixXd result = MatrixXd::Zero(n, n_output_cols);

    int col_offset = 0;
    for (int f = 0; f < n_features; ++f) {
        const auto& cats = categories[f];
        int start = drop_first ? 1 : 0;
        // Build lookup map for O(1) category->column mapping
        qore_keyed_fold_int_map<int, int> cat_to_col;
        QORE_KEYED_HASH_SITE("ml-3", cat_to_col);
        for (int c = start; c < (int)cats.size(); ++c) {
            cat_to_col[cats[c]] = col_offset + c - start;
        }
        for (int64_t i = 0; i < n; ++i) {
            int val = (int)X(i, f);
            auto it = cat_to_col.find(val);
            if (it != cat_to_col.end()) {
                result(i, it->second) = 1.0;
            }
        }
        col_offset += (int)cats.size() - start;
    }

    return result;
}

MatrixXd QoreOneHotEncoder::fitTransform(const MatrixXd& X, ExceptionSink* xsink) {
    fit(X, xsink);
    if (*xsink) {
        return MatrixXd();
    }
    return transform(X, xsink);
}

QoreListNode* QoreOneHotEncoder::getOutputFeatureNames(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-ONE-HOT-ENCODER-ERROR", "encoder has not been fitted");
        return nullptr;
    }

    ReferenceHolder<QoreListNode> names(new QoreListNode(stringTypeInfo), xsink);
    for (int f = 0; f < n_features; ++f) {
        std::string base = (f < (int)field_names.size()) ? field_names[f]
            : ("feature_" + std::to_string(f));
        int start = drop_first ? 1 : 0;
        for (int c = start; c < (int)categories[f].size(); ++c) {
            std::string name = base + "_" + std::to_string(categories[f][c]);
            names->push(new QoreStringNode(name), xsink);
        }
    }
    return names.release();
}

// --- Serialization ---

std::vector<uint8_t> QoreOneHotEncoder::serializeState() const {
    std::vector<uint8_t> buf;
    MLSerialization::writeInt32(buf, drop_first ? 1 : 0);
    MLSerialization::writeInt32(buf, n_features);
    MLSerialization::writeInt32(buf, n_output_cols);
    MLSerialization::writeInt32(buf, (int32_t)categories.size());
    for (const auto& cats : categories) {
        MLSerialization::writeInt32(buf, (int32_t)cats.size());
        for (int c : cats) {
            MLSerialization::writeInt32(buf, c);
        }
    }
    MLSerialization::writeStringVector(buf, field_names);
    return buf;
}

QoreOneHotEncoder* QoreOneHotEncoder::deserializeState(const uint8_t* data,
        size_t len, ExceptionSink* xsink) {
    const uint8_t* ptr = data;
    size_t remaining = len;
    using namespace MLSerialization;

    int32_t df = readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int32_t nf = readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int32_t noc = readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int32_t nc = readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    // the state must satisfy the invariants that fit() establishes (qore#7844)
    if (checkRange(df, 0, 1, "OneHotEncoder", "drop_first", xsink)
        || checkRange(nf, 1, INT32_MAX, "OneHotEncoder", "the number of features", xsink)
        || checkSize(nc, nf, "OneHotEncoder", "the number of category lists", xsink)) {
        return nullptr;
    }
    // each category list is a count and its values, as writeIntVector() writes them
    std::vector<std::vector<int>> cats;
    int64_t n_output = 0;
    for (int32_t i = 0; i < nc; ++i) {
        std::vector<int> c = readIntVector(ptr, remaining, xsink);
        if (*xsink) { return nullptr; }
        // fit() learns at least one category per feature, sorted and distinct
        if (c.empty()) {
            stateError(xsink, "OneHotEncoder", "feature %d has no categories", (int)i);
            return nullptr;
        }
        for (size_t j = 1; j < c.size(); ++j) {
            if (c[j] <= c[j - 1]) {
                stateError(xsink, "OneHotEncoder", "the categories of feature %d are not sorted and distinct",
                    (int)i);
                return nullptr;
            }
        }
        n_output += (int64_t)c.size() - df;
        cats.push_back(std::move(c));
    }
    auto fnames = readStringVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }

    if (checkEnd(remaining, "OneHotEncoder", xsink)
        || checkRange(n_output, 0, INT32_MAX, "OneHotEncoder", "the number of output columns", xsink)
        || checkSize(noc, n_output, "OneHotEncoder", "the number of output columns", xsink)) {
        return nullptr;
    }

    std::unique_ptr<QoreOneHotEncoder> enc(new QoreOneHotEncoder(df != 0));
    enc->n_features = nf;
    enc->n_output_cols = noc;
    enc->categories = std::move(cats);
    enc->field_names = std::move(fnames);
    enc->fitted = true;
    return enc.release();
}
