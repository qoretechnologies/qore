/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_PCA.h

    Qore ml module - PCA class

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

#ifndef _QORE_MODULE_ML_QC_PCA_H
#define _QORE_MODULE_ML_QC_PCA_H

#include "ml_common.h"

DLLEXPORT extern qore_classid_t CID_PCA;
DLLLOCAL extern QoreClass* QC_PCA;

DLLLOCAL void preinitPCAClass();
DLLLOCAL QoreClass* initPCAClass(QoreNamespace& ns);

//! PCA implementation class (stub)
class QorePCA : public AbstractPrivateData {
public:
    DLLLOCAL QorePCA(int n_components, double variance_threshold, bool center, bool scale)
        : n_components(n_components), variance_threshold(variance_threshold),
          center(center), scale(scale) {
    }

    DLLLOCAL int getNumComponents() const { return n_components; }
    DLLLOCAL bool isFitted() const { return fitted; }

private:
    int n_components;
    double variance_threshold;
    bool center;
    bool scale;
    bool fitted = false;
};

#endif // _QORE_MODULE_ML_QC_PCA_H
