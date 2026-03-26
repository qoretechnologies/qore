/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ml_sparse.cpp

    Qore ml module - sparse matrix conversion utilities

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

#include "ml_sparse.h"

SparseMatrixXd denseToSparse(const MatrixXd& dense, double threshold) {
    std::vector<Eigen::Triplet<double>> triplets;
    triplets.reserve(dense.nonZeros());

    for (Eigen::Index i = 0; i < dense.rows(); ++i) {
        for (Eigen::Index j = 0; j < dense.cols(); ++j) {
            double v = dense(i, j);
            if (std::abs(v) > threshold) {
                triplets.emplace_back(static_cast<int>(i), static_cast<int>(j), v);
            }
        }
    }

    SparseMatrixXd sparse(dense.rows(), dense.cols());
    sparse.setFromTriplets(triplets.begin(), triplets.end());
    sparse.makeCompressed();
    return sparse;
}

MatrixXd sparseToDense(const SparseMatrixXd& sparse) {
    return MatrixXd(sparse);
}

SparseMatrixXd qoreTripletListToSparse(int rows, int cols,
    const QoreListNode* triplets, ExceptionSink* xsink) {
    if (rows <= 0 || cols <= 0) {
        xsink->raiseException("ML-SPARSE-ERROR",
            "rows (%d) and cols (%d) must be positive", rows, cols);
        return SparseMatrixXd(0, 0);
    }

    std::vector<Eigen::Triplet<double>> eigen_triplets;
    if (triplets) {
        eigen_triplets.reserve(triplets->size());

        for (size_t i = 0; i < triplets->size(); ++i) {
            QoreValue entry = triplets->retrieveEntry(i);
            if (entry.getType() != NT_HASH) {
                xsink->raiseException("ML-SPARSE-ERROR",
                    "triplet element %zu is not a hash", i);
                return SparseMatrixXd(0, 0);
            }
            const QoreHashNode* h = entry.get<const QoreHashNode>();

            QoreValue rv = h->getKeyValue("row");
            QoreValue cv = h->getKeyValue("col");
            QoreValue vv = h->getKeyValue("value");

            if (rv.isNullOrNothing() || cv.isNullOrNothing() || vv.isNullOrNothing()) {
                xsink->raiseException("ML-SPARSE-ERROR",
                    "triplet element %zu missing required key 'row', 'col', or 'value'", i);
                return SparseMatrixXd(0, 0);
            }

            int r = static_cast<int>(rv.getAsBigInt());
            int c = static_cast<int>(cv.getAsBigInt());
            double v = vv.getAsFloat();

            if (r < 0 || r >= rows || c < 0 || c >= cols) {
                xsink->raiseException("ML-SPARSE-ERROR",
                    "triplet element %zu has out-of-bounds indices (%d, %d) for matrix of size (%d, %d)",
                    i, r, c, rows, cols);
                return SparseMatrixXd(0, 0);
            }

            eigen_triplets.emplace_back(r, c, v);
        }
    }

    SparseMatrixXd sparse(rows, cols);
    sparse.setFromTriplets(eigen_triplets.begin(), eigen_triplets.end());
    sparse.makeCompressed();
    return sparse;
}

QoreListNode* sparseToQoreTripletList(const SparseMatrixXd& sparse,
    ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);

    for (int k = 0; k < sparse.outerSize(); ++k) {
        for (SparseMatrixXd::InnerIterator it(sparse, k); it; ++it) {
            ReferenceHolder<QoreHashNode> entry(new QoreHashNode(autoTypeInfo), xsink);
            entry->setKeyValue("row", static_cast<int64>(it.row()), xsink);
            entry->setKeyValue("col", static_cast<int64>(it.col()), xsink);
            entry->setKeyValue("value", it.value(), xsink);
            rv->push(entry.release(), xsink);
        }
    }

    return rv.release();
}
