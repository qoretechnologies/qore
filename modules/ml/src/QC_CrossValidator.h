/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_CrossValidator.h

    Qore ml module - CrossValidator class

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

#ifndef _QORE_MODULE_ML_QC_CROSSVALIDATOR_H
#define _QORE_MODULE_ML_QC_CROSSVALIDATOR_H

#include "ml_common.h"

#include <random>

DLLEXPORT extern qore_classid_t CID_CROSSVALIDATOR;
DLLLOCAL extern QoreClass* QC_CROSSVALIDATOR;

DLLLOCAL void preinitCrossValidatorClass();
DLLLOCAL QoreClass* initCrossValidatorClass(QoreNamespace& ns);

//! CrossValidator — k-fold cross-validation split utility
class QoreCrossValidator : public AbstractPrivateData {
public:
    DLLLOCAL QoreCrossValidator(int n_folds, bool shuffle, int64_t seed)
        : n_folds(n_folds), do_shuffle(shuffle) {
        if (seed == 0) {
            std::random_device rd;
            rng.seed(rd());
        } else {
            rng.seed(static_cast<unsigned>(seed));
        }
    }

    //! Generate k-fold split indices
    DLLLOCAL QoreListNode* split(int n_samples, ExceptionSink* xsink);

    //! Getters
    DLLLOCAL int getNumFolds() const { return n_folds; }

private:
    int n_folds;
    bool do_shuffle;
    std::mt19937 rng;
};

#endif // _QORE_MODULE_ML_QC_CROSSVALIDATOR_H
