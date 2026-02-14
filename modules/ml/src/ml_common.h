/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ml_common.h

    Qore ml module - shared Eigen typedefs and utilities

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

#ifndef _QORE_MODULE_ML_COMMON_H
#define _QORE_MODULE_ML_COMMON_H

#include <qore/Qore.h>

#include <Eigen/Dense>

#include <random>
#include <vector>
#include <string>
#include <cmath>

// Eigen type aliases
using MatrixXd = Eigen::MatrixXd;
using VectorXd = Eigen::VectorXd;
using RowVectorXd = Eigen::RowVectorXd;

//! Convert a Qore list of hashes to an Eigen matrix (rows = samples, columns = numeric fields)
/** The field names are returned in the field_names output parameter.
    Only numeric fields (int, float) are included.

    @param data the list of hashes
    @param field_names output: the field names used as columns
    @param xsink exception sink
    @return the Eigen matrix, or empty matrix on error
*/
DLLLOCAL MatrixXd qoreListOfHashesToMatrix(const QoreListNode* data,
    std::vector<std::string>& field_names, ExceptionSink* xsink);

//! Convert a Qore list of lists (matrix) to an Eigen matrix
/** @param data the list of lists (each inner list is a row)
    @param xsink exception sink
    @return the Eigen matrix
*/
DLLLOCAL MatrixXd qoreListOfListsToMatrix(const QoreListNode* data, ExceptionSink* xsink);

//! Convert a Qore list of floats to an Eigen vector
DLLLOCAL VectorXd qoreListToVector(const QoreListNode* data, ExceptionSink* xsink);

//! Convert an Eigen vector to a Qore list of floats
DLLLOCAL QoreListNode* eigenVectorToQoreList(const VectorXd& vec);

//! Convert an Eigen row from a matrix to a Qore hash using given field names
DLLLOCAL QoreHashNode* eigenRowToQoreHash(const MatrixXd& matrix, Eigen::Index row,
    const std::vector<std::string>& field_names);

//! Convert a single Qore hash to an Eigen row vector using given field names
DLLLOCAL RowVectorXd qoreHashToRowVector(const QoreHashNode* hash,
    const std::vector<std::string>& field_names, ExceptionSink* xsink);

//! Convert a single Qore list (row) to an Eigen row vector
DLLLOCAL RowVectorXd qoreListToRowVector(const QoreListNode* list, ExceptionSink* xsink);

#endif // _QORE_MODULE_ML_COMMON_H
