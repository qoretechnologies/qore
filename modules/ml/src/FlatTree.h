/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    FlatTree.h

    Shared CART decision tree structure for DecisionTree, RandomForest, and GBT

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_MODULE_ML_FLAT_TREE_H
#define _QORE_MODULE_ML_FLAT_TREE_H

#include "ml_common.h"

#include <random>
#include <string>
#include <vector>

//! A single node in a CART decision tree (stored in flat vector)
struct CARTNode {
    int feature_index = -1;     //!< split feature (-1 = leaf)
    double threshold = 0.0;      //!< split threshold (go left if <= threshold)
    double value = 0.0;          //!< leaf: predicted value (class label or regression mean)
    std::vector<double> class_probs; //!< leaf: probability per class (classification only)
    int n_samples = 0;           //!< number of training samples at this node
    double impurity = 0.0;       //!< impurity (Gini/entropy/MSE) at this node
    double weighted_impurity_reduction = 0.0; //!< weighted reduction from this split
    int left = -1;               //!< left child index (-1 = no child)
    int right = -1;              //!< right child index (-1 = no child)
};

//! A complete CART decision tree stored as a flat array of nodes
/** Supports both classification (Gini/entropy) and regression (MSE).
    Used internally by DecisionTree, RandomForest, and GradientBoostedTrees.

    Thread-safety: immutable after fit(). Concurrent predict() calls are safe.
*/
struct FlatTree {
    std::vector<CARTNode> nodes;
    int n_features = 0;
    int n_classes = 0;               //!< 0 for regression
    std::vector<double> classes;     //!< sorted unique class labels (classification)
    int total_samples = 0;           //!< total training samples (for importance normalization)

    //! Build the tree via recursive CART splitting
    /** @param X feature matrix (n_samples x n_features)
        @param y target vector
        @param max_depth maximum tree depth (0 = unlimited)
        @param min_samples_split minimum samples to attempt a split
        @param min_samples_leaf minimum samples in each leaf
        @param criterion "gini", "entropy", or "mse"
        @param task "classification" or "regression"
        @param rng random number generator (for tie-breaking)
        @param feature_subset if non-null, only consider these feature indices
        @param sample_indices if non-null, only use these row indices
    */
    void fit(const Eigen::MatrixXd& X, const Eigen::VectorXd& y,
        int max_depth, int min_samples_split, int min_samples_leaf,
        const std::string& criterion, const std::string& task,
        std::mt19937& rng,
        const std::vector<int>* feature_subset = nullptr,
        const std::vector<int>* sample_indices = nullptr);

    //! Predict the value for a single row (class label or regression value)
    double predict(const Eigen::RowVectorXd& x) const;

    //! Predict class probabilities for a single row (classification only)
    std::vector<double> predictProba(const Eigen::RowVectorXd& x) const;

    //! Compute feature importances (normalized impurity reduction per feature)
    Eigen::VectorXd featureImportances() const;

    //! Serialize tree to a byte buffer
    void serialize(std::vector<uint8_t>& buf) const;

    //! Deserialize tree from a byte buffer
    static FlatTree deserialize(const uint8_t*& ptr, size_t& remaining,
        ExceptionSink* xsink);

private:
    //! Recursive node builder
    int buildNode(const Eigen::MatrixXd& X, const Eigen::VectorXd& y,
        const std::vector<int>& indices, int depth, int max_depth,
        int min_samples_split, int min_samples_leaf,
        const std::string& criterion, const std::string& task,
        std::mt19937& rng, const std::vector<int>* feature_subset);

    //! Compute impurity for a set of target values
    static double computeImpurity(const Eigen::VectorXd& y,
        const std::vector<int>& indices, const std::string& criterion,
        const std::vector<double>& classes);

    //! Find the best split for a node
    struct SplitResult {
        int feature = -1;
        double threshold = 0.0;
        double gain = 0.0;
        std::vector<int> left_indices;
        std::vector<int> right_indices;
    };
    SplitResult findBestSplit(const Eigen::MatrixXd& X, const Eigen::VectorXd& y,
        const std::vector<int>& indices, const std::string& criterion,
        const std::string& task, const std::vector<int>& features) const;
};

#endif // _QORE_MODULE_ML_FLAT_TREE_H
