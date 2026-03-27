/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SparseMatrix.cpp

    Qore ml module - SparseMatrix implementation

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

#include "QC_SparseMatrix.h"
#include "ml_serialization.h"

QoreSparseMatrix::QoreSparseMatrix(int rows, int cols, const QoreListNode* triplets,
    ExceptionSink* xsink) {
    matrix = qoreTripletListToSparse(rows, cols, triplets, xsink);
}

QoreSparseMatrix::QoreSparseMatrix(const MatrixXd& dense, double threshold) {
    matrix = denseToSparse(dense, threshold);
}

QoreSparseMatrix::QoreSparseMatrix(SparseMatrixXd&& mat) : matrix(std::move(mat)) {
}

QoreListNode* QoreSparseMatrix::toDense(ExceptionSink* xsink) const {
    MatrixXd dense = sparseToDense(matrix);
    int rows = static_cast<int>(dense.rows());
    int cols = static_cast<int>(dense.cols());

    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    for (int i = 0; i < rows; ++i) {
        if (i % 100 == 0 && qore_check_cancel(xsink, "SparseMatrix toDense")) {
            return nullptr;
        }
        ReferenceHolder<QoreListNode> row(new QoreListNode(autoTypeInfo), xsink);
        for (int j = 0; j < cols; ++j) {
            row->push(dense(i, j), xsink);
        }
        rv->push(row.release(), xsink);
    }
    return rv.release();
}

QoreListNode* QoreSparseMatrix::toTriplets(ExceptionSink* xsink) const {
    return sparseToQoreTripletList(matrix, xsink);
}

std::vector<uint8_t> QoreSparseMatrix::serializeState() const {
    std::vector<uint8_t> buf;
    int32_t rows = static_cast<int32_t>(matrix.rows());
    int32_t cols = static_cast<int32_t>(matrix.cols());
    int32_t nnz = static_cast<int32_t>(matrix.nonZeros());

    MLSerialization::writeInt32(buf, rows);
    MLSerialization::writeInt32(buf, cols);
    MLSerialization::writeInt32(buf, nnz);

    // Write COO format: (row, col, value) triplets
    for (int k = 0; k < matrix.outerSize(); ++k) {
        for (SparseMatrixXd::InnerIterator it(matrix, k); it; ++it) {
            MLSerialization::writeInt32(buf, static_cast<int32_t>(it.row()));
            MLSerialization::writeInt32(buf, static_cast<int32_t>(it.col()));
            MLSerialization::writeScalar(buf, it.value());
        }
    }

    return buf;
}

QoreSparseMatrix* QoreSparseMatrix::deserializeState(const uint8_t* data, size_t len,
    ExceptionSink* xsink) {
    const uint8_t* ptr = data;
    size_t remaining = len;

    int32_t rows = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int32_t cols = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int32_t nnz = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }

    std::vector<Eigen::Triplet<double>> triplets;
    triplets.reserve(nnz);

    for (int32_t i = 0; i < nnz; ++i) {
        int32_t r = MLSerialization::readInt32(ptr, remaining, xsink);
        if (*xsink) { return nullptr; }
        int32_t c = MLSerialization::readInt32(ptr, remaining, xsink);
        if (*xsink) { return nullptr; }
        double v = MLSerialization::readScalar(ptr, remaining, xsink);
        if (*xsink) { return nullptr; }
        triplets.emplace_back(r, c, v);
    }

    SparseMatrixXd sparse(rows, cols);
    sparse.setFromTriplets(triplets.begin(), triplets.end());
    sparse.makeCompressed();

    return new QoreSparseMatrix(std::move(sparse));
}
