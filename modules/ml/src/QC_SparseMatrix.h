/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_SparseMatrix.h

    Qore ml module - SparseMatrix class

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

#ifndef _QORE_MODULE_ML_QC_SPARSEMATRIX_H
#define _QORE_MODULE_ML_QC_SPARSEMATRIX_H

#include "ml_sparse.h"

DLLEXPORT extern qore_classid_t CID_SPARSEMATRIX;
DLLLOCAL extern QoreClass* QC_SPARSEMATRIX;

DLLLOCAL void preinitSparseMatrixClass();
DLLLOCAL QoreClass* initSparseMatrixClass(QoreNamespace& ns);

//! Sparse matrix representation using Eigen's compressed row storage
/** Stores only non-zero entries, making it memory-efficient for high-dimensional
    data with many zeros (e.g., TF-IDF text features, one-hot encoded categoricals).

    Thread-safe: immutable after construction. All operations create new matrices.
*/
class QoreSparseMatrix : public AbstractPrivateData {
public:
    //! Construct from triplets (row, col, value)
    DLLLOCAL QoreSparseMatrix(int rows, int cols, const QoreListNode* triplets,
        ExceptionSink* xsink);

    //! Construct from dense matrix with sparsification threshold
    DLLLOCAL QoreSparseMatrix(const MatrixXd& dense, double threshold);

    //! Construct from existing sparse matrix (move)
    DLLLOCAL QoreSparseMatrix(SparseMatrixXd&& mat);

    //! Get number of rows
    DLLLOCAL int getRows() const { return static_cast<int>(matrix.rows()); }

    //! Get number of columns
    DLLLOCAL int getCols() const { return static_cast<int>(matrix.cols()); }

    //! Get number of non-zero entries
    DLLLOCAL int getNnz() const { return static_cast<int>(matrix.nonZeros()); }

    //! Convert to dense matrix (list of lists)
    DLLLOCAL QoreListNode* toDense(ExceptionSink* xsink) const;

    //! Get triplet list representation
    DLLLOCAL QoreListNode* toTriplets(ExceptionSink* xsink) const;

    //! Convert to dense Eigen matrix
    DLLLOCAL MatrixXd toDenseMatrix() const { return sparseToDense(matrix); }

    //! Access the underlying sparse matrix
    DLLLOCAL const SparseMatrixXd& getMatrix() const { return matrix; }

    //! Serialize to binary
    DLLLOCAL std::vector<uint8_t> serializeState() const;

    //! Deserialize from binary
    DLLLOCAL static QoreSparseMatrix* deserializeState(const uint8_t* data, size_t len,
        ExceptionSink* xsink);

private:
    SparseMatrixXd matrix;
};

#endif // _QORE_MODULE_ML_QC_SPARSEMATRIX_H
