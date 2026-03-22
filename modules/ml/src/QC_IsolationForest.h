/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_IsolationForest.h

    Qore ml module - IsolationForest class

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

#ifndef _QORE_MODULE_ML_QC_ISOLATIONFOREST_H
#define _QORE_MODULE_ML_QC_ISOLATIONFOREST_H

#include "ml_common.h"

#include <vector>
#include <random>
#include <cmath>
#include <memory>
#include <algorithm>
#include <mutex>

DLLEXPORT extern qore_classid_t CID_ISOLATIONFOREST;
DLLLOCAL extern QoreClass* QC_ISOLATIONFOREST;

DLLLOCAL void preinitIsolationForestClass();
DLLLOCAL QoreClass* initIsolationForestClass(QoreNamespace& ns);

//! A single node in an isolation tree
struct ITreeNode {
    //! Feature index used for splitting (-1 for external/leaf nodes)
    int feature = -1;
    //! Split value
    double split_value = 0.0;
    //! Left child index (-1 if none)
    int left = -1;
    //! Right child index (-1 if none)
    int right = -1;
    //! Number of samples at this node (for external nodes)
    int size = 0;
};

//! A single isolation tree stored as a flat array of nodes
struct IsolationTree {
    std::vector<ITreeNode> nodes;
};

//! Isolation Forest implementation class
class QoreIsolationForest : public AbstractPrivateData {
public:
    //! Constructor with options
    DLLLOCAL QoreIsolationForest(int n_trees, int sample_size, int max_depth,
        double threshold, int64_t seed);

    //! Fit on an Eigen matrix (thread-safe; blocks concurrent fit/score)
    DLLLOCAL void fit(const MatrixXd& data, ExceptionSink* xsink);

    //! Score a single row vector; returns anomaly score in [0, 1] (thread-safe after fitting)
    DLLLOCAL QoreHashNode* score(const RowVectorXd& point, ExceptionSink* xsink) const;

    //! Score multiple rows (thread-safe after fitting)
    DLLLOCAL QoreListNode* scoreMatrix(const MatrixXd& data, ExceptionSink* xsink) const;

    //! Whether the model has been fitted
    DLLLOCAL bool isFitted() const { return fitted; }

    //! Getters
    DLLLOCAL int getNumTrees() const { return n_trees; }
    DLLLOCAL int getSampleSize() const { return sample_size; }
    DLLLOCAL double getThreshold() const { return threshold; }
    DLLLOCAL int getActualSampleSize() const { return actual_sample_size; }

    //! Get the tree ensemble as a Qore list of lists of hashes
    DLLLOCAL QoreListNode* getTrees(ExceptionSink* xsink) const;

    //! Store field names for hash-based input
    DLLLOCAL void setFieldNames(const std::vector<std::string>& names) { field_names = names; }
    DLLLOCAL const std::vector<std::string>& getFieldNames() const { return field_names; }

    //! Serialize model state to binary
    DLLLOCAL std::vector<uint8_t> serializeState() const;

    //! Deserialize model state from binary
    DLLLOCAL static QoreIsolationForest* deserializeState(const uint8_t* data, size_t len,
        ExceptionSink* xsink);

private:
    int n_trees;
    int sample_size;
    int max_depth;
    double threshold;
    std::mt19937 rng;
    bool fitted = false;
    int n_features = 0;
    int actual_sample_size = 0;

    std::vector<IsolationTree> trees;
    std::vector<std::string> field_names;

    //! Mutex protecting fit() against concurrent fit/score calls
    mutable std::mutex mtx;

    //! Build a single isolation tree from a subsample
    DLLLOCAL IsolationTree buildTree(const MatrixXd& data, const std::vector<int>& indices);

    //! Recursively build tree nodes
    DLLLOCAL int buildNode(IsolationTree& tree, const MatrixXd& data,
        const std::vector<int>& indices, int depth, int max_d);

    //! Score a single point (must be called with mtx held)
    DLLLOCAL QoreHashNode* scoreInternal(const RowVectorXd& point, ExceptionSink* xsink) const;

    //! Compute path length for a single point through a single tree
    DLLLOCAL double pathLength(const IsolationTree& tree, const RowVectorXd& point) const;

    //! Average path length of unsuccessful search in BST with n elements: c(n)
    DLLLOCAL static double averagePathLength(int n);
};

#endif // _QORE_MODULE_ML_QC_ISOLATIONFOREST_H
