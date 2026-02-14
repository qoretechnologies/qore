/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_KMeans.h

    Qore ml module - KMeans class

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

#ifndef _QORE_MODULE_ML_QC_KMEANS_H
#define _QORE_MODULE_ML_QC_KMEANS_H

#include "ml_common.h"

DLLEXPORT extern qore_classid_t CID_KMEANS;
DLLLOCAL extern QoreClass* QC_KMEANS;

DLLLOCAL void preinitKMeansClass();
DLLLOCAL QoreClass* initKMeansClass(QoreNamespace& ns);

//! K-Means clustering implementation class (stub)
class QoreKMeans : public AbstractPrivateData {
public:
    DLLLOCAL QoreKMeans(int k, int max_iterations, double tolerance, int64_t seed,
        const char* init_method)
        : k(k), max_iterations(max_iterations), tolerance(tolerance),
          init_method(init_method) {
        if (seed == 0) {
            std::random_device rd;
            rng.seed(rd());
        } else {
            rng.seed(static_cast<unsigned>(seed));
        }
    }

    DLLLOCAL int getK() const { return k; }
    DLLLOCAL bool isFitted() const { return fitted; }

private:
    int k;
    int max_iterations;
    double tolerance;
    std::string init_method;
    std::mt19937 rng;
    bool fitted = false;
};

#endif // _QORE_MODULE_ML_QC_KMEANS_H
