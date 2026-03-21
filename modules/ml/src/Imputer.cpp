/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    Imputer.cpp

    Qore ml module - Imputer implementation

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

#include "QC_Imputer.h"
#include "ml_serialization.h"

#include <algorithm>
#include <map>

extern const TypedHashDecl* hashdeclImputerInfo;

double QoreImputer::computeMedian(const MatrixXd& data, Eigen::Index col) {
    std::vector<double> values;
    for (Eigen::Index i = 0; i < data.rows(); ++i) {
        if (!std::isnan(data(i, col))) {
            values.push_back(data(i, col));
        }
    }
    if (values.empty()) {
        return 0.0;
    }

    size_t n = values.size();
    std::sort(values.begin(), values.end());
    if (n % 2 == 0) {
        return (values[n / 2 - 1] + values[n / 2]) / 2.0;
    } else {
        return values[n / 2];
    }
}

double QoreImputer::computeMostFrequent(const MatrixXd& data, Eigen::Index col) {
    std::map<double, int> counts;
    for (Eigen::Index i = 0; i < data.rows(); ++i) {
        if (!std::isnan(data(i, col))) {
            counts[data(i, col)]++;
        }
    }
    if (counts.empty()) {
        return 0.0;
    }

    double most_freq = counts.begin()->first;
    int max_count = counts.begin()->second;
    for (const auto& pair : counts) {
        if (pair.second > max_count) {
            max_count = pair.second;
            most_freq = pair.first;
        }
    }
    return most_freq;
}

void QoreImputer::fit(const MatrixXd& data, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (data.rows() == 0 || data.cols() == 0) {
        xsink->raiseException("ML-IMPUTER-ERROR",
            "cannot fit on empty data: provide at least one sample with one feature");
        return;
    }

    n_features = static_cast<int>(data.cols());
    fill_values.resize(n_features);

    for (int j = 0; j < n_features; ++j) {
        if (strategy == "mean") {
            // Compute mean of non-NaN values
            double sum = 0.0;
            int count = 0;
            for (Eigen::Index i = 0; i < data.rows(); ++i) {
                if (!std::isnan(data(i, j))) {
                    sum += data(i, j);
                    ++count;
                }
            }
            fill_values(j) = (count > 0) ? sum / count : 0.0;
        } else if (strategy == "median") {
            fill_values(j) = computeMedian(data, j);
        } else if (strategy == "most_frequent") {
            fill_values(j) = computeMostFrequent(data, j);
        } else if (strategy == "constant") {
            fill_values(j) = constant_value;
        }
    }

    fitted = true;
}

RowVectorXd QoreImputer::transformRow(const RowVectorXd& row, ExceptionSink* xsink) const {
    if (row.size() != n_features) {
        xsink->raiseException("ML-IMPUTER-ERROR",
            "input has %d features but imputer was fitted with %d features",
            static_cast<int>(row.size()), n_features);
        return RowVectorXd();
    }

    RowVectorXd result = row;
    for (int j = 0; j < n_features; ++j) {
        if (std::isnan(result(j))) {
            result(j) = fill_values(j);
        }
    }
    return result;
}

MatrixXd QoreImputer::transform(const MatrixXd& data, ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-IMPUTER-ERROR",
            "imputer has not been fitted: call fit() or fitMatrix() first");
        return MatrixXd();
    }
    if (data.cols() != n_features) {
        xsink->raiseException("ML-IMPUTER-ERROR",
            "input has %d features but imputer was fitted with %d features",
            static_cast<int>(data.cols()), n_features);
        return MatrixXd();
    }

    MatrixXd result = data;
    for (Eigen::Index i = 0; i < result.rows(); ++i) {
        for (int j = 0; j < n_features; ++j) {
            if (std::isnan(result(i, j))) {
                result(i, j) = fill_values(j);
            }
        }
    }
    return result;
}

MatrixXd QoreImputer::fitTransform(const MatrixXd& data, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (data.rows() == 0 || data.cols() == 0) {
        xsink->raiseException("ML-IMPUTER-ERROR",
            "cannot fit on empty data: provide at least one sample with one feature");
        return MatrixXd();
    }

    n_features = static_cast<int>(data.cols());
    fill_values.resize(n_features);

    for (int j = 0; j < n_features; ++j) {
        if (strategy == "mean") {
            double sum = 0.0;
            int count = 0;
            for (Eigen::Index i = 0; i < data.rows(); ++i) {
                if (!std::isnan(data(i, j))) {
                    sum += data(i, j);
                    ++count;
                }
            }
            fill_values(j) = (count > 0) ? sum / count : 0.0;
        } else if (strategy == "median") {
            fill_values(j) = computeMedian(data, j);
        } else if (strategy == "most_frequent") {
            fill_values(j) = computeMostFrequent(data, j);
        } else if (strategy == "constant") {
            fill_values(j) = constant_value;
        }
    }

    fitted = true;

    // Transform in place
    MatrixXd result = data;
    for (Eigen::Index i = 0; i < result.rows(); ++i) {
        for (int j = 0; j < n_features; ++j) {
            if (std::isnan(result(i, j))) {
                result(i, j) = fill_values(j);
            }
        }
    }
    return result;
}

QoreHashNode* QoreImputer::getInfo(ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-IMPUTER-ERROR",
            "imputer has not been fitted: call fit() or fitMatrix() first");
        return nullptr;
    }

    ReferenceHolder<QoreListNode> fv_list(new QoreListNode(floatTypeInfo), xsink);
    for (int j = 0; j < n_features; ++j) {
        fv_list->push(fill_values(j), xsink);
    }

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclImputerInfo, xsink), xsink);
    rv->setKeyValue("strategy", new QoreStringNode(strategy), xsink);
    rv->setKeyValue("fill_values", fv_list.release(), xsink);
    rv->setKeyValue("n_features", static_cast<int64>(n_features), xsink);

    return rv.release();
}

std::vector<uint8_t> QoreImputer::serializeState() const {
    std::vector<uint8_t> buf;
    // Hyperparameters
    MLSerialization::writeString(buf, strategy);
    MLSerialization::writeScalar(buf, constant_value);
    // Model state
    MLSerialization::writeInt32(buf, n_features);
    MLSerialization::writeVector(buf, fill_values);
    MLSerialization::writeStringVector(buf, field_names);
    return buf;
}

QoreImputer* QoreImputer::deserializeState(const uint8_t* data, size_t len,
    ExceptionSink* xsink) {
    const uint8_t* ptr = data;
    size_t remaining = len;

    std::string strategy = MLSerialization::readString(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double constant_value = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int32_t n_features = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    VectorXd fill_values = MLSerialization::readVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::vector<std::string> field_names = MLSerialization::readStringVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }

    std::unique_ptr<QoreImputer> obj(new QoreImputer(strategy, constant_value));
    obj->n_features = n_features;
    obj->fill_values = std::move(fill_values);
    obj->field_names = std::move(field_names);
    obj->fitted = true;
    return obj.release();
}
