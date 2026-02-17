/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ml_common.cpp

    Qore ml module - shared Eigen conversion utilities

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

#include "ml_common.h"

MatrixXd qoreListOfHashesToMatrix(const QoreListNode* data,
    std::vector<std::string>& field_names, ExceptionSink* xsink) {

    if (!data || data->empty()) {
        xsink->raiseException("ML-DATA-ERROR", "empty data list provided");
        return MatrixXd();
    }

    // Get field names from the first hash
    QoreValue first = data->retrieveEntry(0);
    if (first.getType() != NT_HASH) {
        xsink->raiseException("ML-DATA-ERROR", "data list must contain hash elements; got type '%s'",
            first.getTypeName());
        return MatrixXd();
    }
    const QoreHashNode* first_hash = first.get<const QoreHashNode>();

    // Collect numeric field names
    ConstHashIterator hi(first_hash);
    while (hi.next()) {
        QoreValue val = hi.get();
        qore_type_t vt = val.getType();
        if (vt == NT_INT || vt == NT_FLOAT || vt == NT_NUMBER) {
            field_names.push_back(hi.getKey());
        }
    }

    if (field_names.empty()) {
        xsink->raiseException("ML-DATA-ERROR", "no numeric fields found in data");
        return MatrixXd();
    }

    size_t n_rows = data->size();
    size_t n_cols = field_names.size();
    MatrixXd matrix(n_rows, n_cols);

    for (size_t i = 0; i < n_rows; ++i) {
        QoreValue row_val = data->retrieveEntry(i);
        if (row_val.getType() != NT_HASH) {
            xsink->raiseException("ML-DATA-ERROR", "data element %zu is not a hash", i);
            return MatrixXd();
        }
        const QoreHashNode* row = row_val.get<const QoreHashNode>();
        for (size_t j = 0; j < n_cols; ++j) {
            QoreValue v = row->getKeyValue(field_names[j].c_str());
            matrix(i, j) = v.getAsFloat();
        }
    }

    return matrix;
}

MatrixXd qoreListOfListsToMatrix(const QoreListNode* data, ExceptionSink* xsink) {
    if (!data || data->empty()) {
        xsink->raiseException("ML-DATA-ERROR", "empty matrix provided");
        return MatrixXd();
    }

    // Get dimensions from first row
    QoreValue first = data->retrieveEntry(0);
    if (first.getType() != NT_LIST) {
        xsink->raiseException("ML-DATA-ERROR", "matrix must contain list elements; got type '%s'",
            first.getTypeName());
        return MatrixXd();
    }
    const QoreListNode* first_row = first.get<const QoreListNode>();
    size_t n_cols = first_row->size();

    if (n_cols == 0) {
        xsink->raiseException("ML-DATA-ERROR", "matrix rows must not be empty");
        return MatrixXd();
    }

    size_t n_rows = data->size();
    MatrixXd matrix(n_rows, n_cols);

    for (size_t i = 0; i < n_rows; ++i) {
        QoreValue row_val = data->retrieveEntry(i);
        if (row_val.getType() != NT_LIST) {
            xsink->raiseException("ML-DATA-ERROR", "matrix element %zu is not a list", i);
            return MatrixXd();
        }
        const QoreListNode* row = row_val.get<const QoreListNode>();
        if (row->size() != n_cols) {
            xsink->raiseException("ML-DATA-ERROR",
                "matrix row %zu has %zu columns, expected %zu", i, row->size(), n_cols);
            return MatrixXd();
        }
        for (size_t j = 0; j < n_cols; ++j) {
            matrix(i, j) = row->retrieveEntry(j).getAsFloat();
        }
    }

    return matrix;
}

VectorXd qoreListToVector(const QoreListNode* data, ExceptionSink* xsink) {
    if (!data || data->empty()) {
        xsink->raiseException("ML-DATA-ERROR", "empty list provided");
        return VectorXd();
    }

    size_t n = data->size();
    VectorXd vec(n);
    for (size_t i = 0; i < n; ++i) {
        vec(i) = data->retrieveEntry(i).getAsFloat();
    }
    return vec;
}

QoreListNode* eigenVectorToQoreList(const VectorXd& vec) {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), nullptr);
    for (Eigen::Index i = 0; i < vec.size(); ++i) {
        rv->push(vec(i), nullptr);
    }
    return rv.release();
}

QoreHashNode* eigenRowToQoreHash(const MatrixXd& matrix, Eigen::Index row,
    const std::vector<std::string>& field_names) {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), nullptr);
    for (size_t j = 0; j < field_names.size(); ++j) {
        rv->setKeyValue(field_names[j].c_str(), matrix(row, j), nullptr);
    }
    return rv.release();
}

RowVectorXd qoreHashToRowVector(const QoreHashNode* hash,
    const std::vector<std::string>& field_names, ExceptionSink* xsink) {
    RowVectorXd row(field_names.size());
    for (size_t j = 0; j < field_names.size(); ++j) {
        QoreValue v = hash->getKeyValue(field_names[j].c_str());
        row(j) = v.getAsFloat();
    }
    return row;
}

RowVectorXd qoreListToRowVector(const QoreListNode* list, ExceptionSink* xsink) {
    if (!list || list->empty()) {
        xsink->raiseException("ML-DATA-ERROR", "empty row provided");
        return RowVectorXd();
    }
    size_t n = list->size();
    RowVectorXd row(n);
    for (size_t i = 0; i < n; ++i) {
        row(i) = list->retrieveEntry(i).getAsFloat();
    }
    return row;
}
