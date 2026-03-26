/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ml_sparse.h

    Qore ml module - sparse matrix types and conversion utilities

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

#ifndef _QORE_MODULE_ML_SPARSE_H
#define _QORE_MODULE_ML_SPARSE_H

#include "ml_common.h"

#include <Eigen/Sparse>

// Sparse matrix type alias (row-major for efficient row slicing)
using SparseMatrixXd = Eigen::SparseMatrix<double, Eigen::RowMajor>;

//! Convert a dense Eigen matrix to sparse, treating values below threshold as zero
/** @param dense the dense matrix
    @param threshold values with absolute value below this are treated as zero (default: 0.0)
    @return the sparse matrix
*/
DLLLOCAL SparseMatrixXd denseToSparse(const MatrixXd& dense, double threshold = 0.0);

//! Convert a sparse Eigen matrix to dense
/** @param sparse the sparse matrix
    @return the dense matrix
*/
DLLLOCAL MatrixXd sparseToDense(const SparseMatrixXd& sparse);

//! Convert a Qore list of triplet hashes to a sparse matrix
/** Each hash must have keys: \c row (int), \c col (int), \c value (float).
    @param rows number of rows
    @param cols number of columns
    @param triplets list of {row, col, value} hashes
    @param xsink exception sink
    @return the sparse matrix
*/
DLLLOCAL SparseMatrixXd qoreTripletListToSparse(int rows, int cols,
    const QoreListNode* triplets, ExceptionSink* xsink);

//! Convert a sparse matrix to a Qore list of triplet hashes
/** @param sparse the sparse matrix
    @param xsink exception sink
    @return list of {row, col, value} hashes
*/
DLLLOCAL QoreListNode* sparseToQoreTripletList(const SparseMatrixXd& sparse,
    ExceptionSink* xsink);

#endif // _QORE_MODULE_ML_SPARSE_H
