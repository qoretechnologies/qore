/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    CrossValidator.cpp

    Qore ml module - CrossValidator implementation

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

#include "QC_CrossValidator.h"

#include <algorithm>
#include <numeric>

extern const TypedHashDecl* hashdeclCrossValidationFold;

QoreListNode* QoreCrossValidator::split(int n_samples, ExceptionSink* xsink) {
    if (n_samples < n_folds) {
        xsink->raiseException("ML-CROSS-VALIDATOR-ERROR",
            "n_samples (%d) must be >= n_folds (%d)", n_samples, n_folds);
        return nullptr;
    }

    // Create index array
    std::vector<int> indices(n_samples);
    std::iota(indices.begin(), indices.end(), 0);

    if (do_shuffle) {
        std::shuffle(indices.begin(), indices.end(), rng);
    }

    // Compute fold boundaries
    int base_size = n_samples / n_folds;
    int remainder = n_samples % n_folds;

    ReferenceHolder<QoreListNode> rv(
        new QoreListNode(hashdeclCrossValidationFold->getTypeInfo()), xsink);

    int offset = 0;
    for (int fold = 0; fold < n_folds; ++fold) {
        int fold_size = base_size + (fold < remainder ? 1 : 0);

        // Test indices for this fold
        ReferenceHolder<QoreListNode> test_list(new QoreListNode(bigIntTypeInfo), xsink);
        for (int j = offset; j < offset + fold_size; ++j) {
            test_list->push(static_cast<int64>(indices[j]), xsink);
        }

        // Train indices = everything except this fold
        ReferenceHolder<QoreListNode> train_list(new QoreListNode(bigIntTypeInfo), xsink);
        for (int j = 0; j < n_samples; ++j) {
            if (j < offset || j >= offset + fold_size) {
                train_list->push(static_cast<int64>(indices[j]), xsink);
            }
        }

        ReferenceHolder<QoreHashNode> fold_hash(
            new QoreHashNode(hashdeclCrossValidationFold, xsink), xsink);
        fold_hash->setKeyValue("train_indices", train_list.release(), xsink);
        fold_hash->setKeyValue("test_indices", test_list.release(), xsink);
        fold_hash->setKeyValue("fold", static_cast<int64>(fold), xsink);

        rv->push(fold_hash.release(), xsink);

        offset += fold_size;
    }

    return rv.release();
}
