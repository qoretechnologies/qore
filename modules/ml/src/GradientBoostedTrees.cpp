/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    GradientBoostedTrees.cpp

    GradientBoostedTrees implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "QC_GradientBoostedTrees.h"
#include "ml_serialization.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>

extern const TypedHashDecl* hashdeclGBTClassificationResult;
extern const TypedHashDecl* hashdeclGBTRegressionResult;
extern const TypedHashDecl* hashdeclFeatureImportanceInfo;

QoreGradientBoostedTrees::QoreGradientBoostedTrees(int n_estimators,
        double learning_rate, int max_depth, int min_samples_split,
        int min_samples_leaf, double subsample, const std::string& task,
        double validation_fraction, int n_iter_no_change, int64_t seed)
    : n_estimators(n_estimators), learning_rate(learning_rate),
      max_depth(max_depth), min_samples_split(min_samples_split),
      min_samples_leaf(min_samples_leaf), subsample(subsample),
      task(task), validation_fraction(validation_fraction),
      n_iter_no_change(n_iter_no_change),
      rng(seed == 0 ? std::random_device{}() : (unsigned)seed) {
}

void QoreGradientBoostedTrees::fit(const MatrixXd& X, const VectorXd& y,
        ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (X.rows() == 0 || X.cols() == 0) {
        xsink->raiseException("ML-GBT-ERROR", "cannot fit on empty data");
        return;
    }
    if (X.rows() != y.size()) {
        xsink->raiseException("ML-GBT-ERROR",
            "X has " QLLD " rows but y has " QLLD " elements",
            (int64_t)X.rows(), (int64_t)y.size());
        return;
    }

    n_features = (int)X.cols();
    int n_samples = (int)X.rows();
    trees.clear();
    actual_n_estimators = 0;

    // Split train/validation if early stopping enabled
    int n_train = n_samples;
    int n_val = 0;
    std::vector<int> train_idx, val_idx;
    if (validation_fraction > 0 && validation_fraction < 1.0) {
        n_val = (int)(n_samples * validation_fraction);
        n_train = n_samples - n_val;
        std::vector<int> all_idx(n_samples);
        std::iota(all_idx.begin(), all_idx.end(), 0);
        std::shuffle(all_idx.begin(), all_idx.end(), rng);
        train_idx.assign(all_idx.begin(), all_idx.begin() + n_train);
        val_idx.assign(all_idx.begin() + n_train, all_idx.end());
    } else {
        train_idx.resize(n_train);
        std::iota(train_idx.begin(), train_idx.end(), 0);
    }

    if (task == "classification") {
        // Determine classes
        std::set<double> unique_classes;
        for (int i : train_idx) {
            unique_classes.insert(y(i));
        }
        classes.assign(unique_classes.begin(), unique_classes.end());
        n_classes = (int)classes.size();

        if (n_classes == 2) {
            // Binary classification: log-loss with sigmoid
            // F_0 = log(p / (1-p)) where p = mean(y == class_1)
            int pos_count = 0;
            for (int i : train_idx) {
                if (y(i) == classes[1]) {
                    ++pos_count;
                }
            }
            double p = (double)pos_count / n_train;
            p = std::max(1e-7, std::min(1.0 - 1e-7, p));
            initial_prediction = std::log(p / (1.0 - p));

            // Initialize F(x) for all training samples
            VectorXd F = VectorXd::Constant(n_samples, initial_prediction);

            double best_val_loss = std::numeric_limits<double>::max();
            int no_improve_count = 0;

            for (int m = 0; m < n_estimators; ++m) {
                if ((m % 10) == 0 && m > 0) {
                    if (qore_check_cancel(xsink, "training gradient boosted trees")) {
                        trees.clear();
                        return;
                    }
                }

                // Compute residuals: y_binary - sigmoid(F)
                VectorXd residuals(n_samples);
                for (int i : train_idx) {
                    double y_bin = (y(i) == classes[1]) ? 1.0 : 0.0;
                    residuals(i) = y_bin - sigmoid(F(i));
                }

                // Subsample
                std::vector<int> sample_idx;
                if (subsample < 1.0) {
                    int n_sub = std::max(1, (int)(n_train * subsample));
                    sample_idx.resize(n_sub);
                    std::uniform_int_distribution<int> dist(0, n_train - 1);
                    for (int i = 0; i < n_sub; ++i) {
                        sample_idx[i] = train_idx[dist(rng)];
                    }
                } else {
                    sample_idx = train_idx;
                }

                // Fit tree to residuals
                FlatTree tree;
                tree.fit(X, residuals, max_depth, min_samples_split,
                    min_samples_leaf, "mse", "regression", rng,
                    nullptr, &sample_idx);
                trees.push_back(std::move(tree));
                ++actual_n_estimators;

                // Update F
                for (int i = 0; i < n_samples; ++i) {
                    F(i) += learning_rate * trees.back().predict(X.row(i));
                }

                // Early stopping
                if (n_val > 0) {
                    double val_loss = 0;
                    for (int i : val_idx) {
                        double y_bin = (y(i) == classes[1]) ? 1.0 : 0.0;
                        double p_val = sigmoid(F(i));
                        p_val = std::max(1e-7, std::min(1.0 - 1e-7, p_val));
                        val_loss -= y_bin * std::log(p_val)
                            + (1.0 - y_bin) * std::log(1.0 - p_val);
                    }
                    val_loss /= n_val;
                    if (val_loss < best_val_loss - 1e-7) {
                        best_val_loss = val_loss;
                        no_improve_count = 0;
                    } else {
                        ++no_improve_count;
                        if (no_improve_count >= n_iter_no_change) {
                            break;
                        }
                    }
                }
            }
        } else {
            // Multiclass: simplified — treat as regression on class labels
            // (full softmax multiclass deferred to v2)
            initial_prediction = 0;
            VectorXd F = VectorXd::Zero(n_samples);

            for (int m = 0; m < n_estimators; ++m) {
                if ((m % 10) == 0 && m > 0) {
                    if (qore_check_cancel(xsink, "training gradient boosted trees")) {
                        trees.clear();
                        return;
                    }
                }
                VectorXd residuals = y - F.head(n_samples);
                // Use training indices only
                FlatTree tree;
                tree.fit(X, residuals, max_depth, min_samples_split,
                    min_samples_leaf, "mse", "regression", rng,
                    nullptr, &train_idx);
                trees.push_back(std::move(tree));
                ++actual_n_estimators;

                for (int i = 0; i < n_samples; ++i) {
                    F(i) += learning_rate * trees.back().predict(X.row(i));
                }
            }
        }
    } else {
        // Regression: MSE loss
        // F_0 = mean(y)
        double sum = 0;
        for (int i : train_idx) {
            sum += y(i);
        }
        initial_prediction = sum / n_train;

        VectorXd F = VectorXd::Constant(n_samples, initial_prediction);

        double best_val_loss = std::numeric_limits<double>::max();
        int no_improve_count = 0;

        for (int m = 0; m < n_estimators; ++m) {
            if ((m % 10) == 0 && m > 0) {
                if (qore_check_cancel(xsink, "training gradient boosted trees")) {
                    trees.clear();
                    return;
                }
            }

            // Residuals = y - F
            VectorXd residuals = y - F;

            // Subsample
            std::vector<int> sample_idx;
            if (subsample < 1.0) {
                int n_sub = std::max(1, (int)(n_train * subsample));
                sample_idx.resize(n_sub);
                std::uniform_int_distribution<int> dist(0, n_train - 1);
                for (int i = 0; i < n_sub; ++i) {
                    sample_idx[i] = train_idx[dist(rng)];
                }
            } else {
                sample_idx = train_idx;
            }

            FlatTree tree;
            tree.fit(X, residuals, max_depth, min_samples_split,
                min_samples_leaf, "mse", "regression", rng,
                nullptr, &sample_idx);
            trees.push_back(std::move(tree));
            ++actual_n_estimators;

            // Update F
            for (int i = 0; i < n_samples; ++i) {
                F(i) += learning_rate * trees.back().predict(X.row(i));
            }

            // Early stopping
            if (n_val > 0) {
                double val_loss = 0;
                for (int i : val_idx) {
                    double diff = y(i) - F(i);
                    val_loss += diff * diff;
                }
                val_loss /= n_val;
                if (val_loss < best_val_loss - 1e-7) {
                    best_val_loss = val_loss;
                    no_improve_count = 0;
                } else {
                    ++no_improve_count;
                    if (no_improve_count >= n_iter_no_change) {
                        break;
                    }
                }
            }
        }
    }

    fitted = true;
}

double QoreGradientBoostedTrees::predictRaw(const RowVectorXd& point) const {
    double F = initial_prediction;
    for (const auto& tree : trees) {
        F += learning_rate * tree.predict(point);
    }
    return F;
}

QoreHashNode* QoreGradientBoostedTrees::predictInternal(const RowVectorXd& point,
        ExceptionSink* xsink) const {
    if (task == "classification" && n_classes == 2) {
        double raw = predictRaw(point);
        double prob_1 = sigmoid(raw);
        double prob_0 = 1.0 - prob_1;
        int predicted = (prob_1 >= 0.5) ? (int)classes[1] : (int)classes[0];
        double max_prob = std::max(prob_0, prob_1);

        ReferenceHolder<QoreHashNode> rv(
            new QoreHashNode(hashdeclGBTClassificationResult, xsink), xsink);
        rv->setKeyValue("predicted_class", (int64_t)predicted, xsink);
        rv->setKeyValue("max_probability", max_prob, xsink);

        ReferenceHolder<QoreListNode> prob_list(new QoreListNode(autoTypeInfo), xsink);
        prob_list->push(prob_0, xsink);
        prob_list->push(prob_1, xsink);
        rv->setKeyValue("probabilities", prob_list.release(), xsink);

        ReferenceHolder<QoreListNode> cls_list(new QoreListNode(autoTypeInfo), xsink);
        for (double c : classes) {
            cls_list->push(c, xsink);
        }
        rv->setKeyValue("classes", cls_list.release(), xsink);
        return rv.release();
    }

    if (task == "classification") {
        // Multiclass fallback: nearest class to raw prediction
        double raw = predictRaw(point);
        int best_class = (int)classes[0];
        double best_dist = std::abs(raw - classes[0]);
        for (int c = 1; c < n_classes; ++c) {
            double dist = std::abs(raw - classes[c]);
            if (dist < best_dist) {
                best_dist = dist;
                best_class = (int)classes[c];
            }
        }

        ReferenceHolder<QoreHashNode> rv(
            new QoreHashNode(hashdeclGBTClassificationResult, xsink), xsink);
        rv->setKeyValue("predicted_class", (int64_t)best_class, xsink);
        rv->setKeyValue("max_probability", 0.0, xsink);
        ReferenceHolder<QoreListNode> prob_list(new QoreListNode(autoTypeInfo), xsink);
        rv->setKeyValue("probabilities", prob_list.release(), xsink);
        ReferenceHolder<QoreListNode> cls_list(new QoreListNode(autoTypeInfo), xsink);
        for (double c : classes) {
            cls_list->push(c, xsink);
        }
        rv->setKeyValue("classes", cls_list.release(), xsink);
        return rv.release();
    }

    // Regression
    double prediction = predictRaw(point);
    ReferenceHolder<QoreHashNode> rv(
        new QoreHashNode(hashdeclGBTRegressionResult, xsink), xsink);
    rv->setKeyValue("prediction", prediction, xsink);
    return rv.release();
}

QoreHashNode* QoreGradientBoostedTrees::predict(const RowVectorXd& point,
        ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-GBT-ERROR", "model has not been fitted");
        return nullptr;
    }
    if (point.size() != n_features) {
        xsink->raiseException("ML-GBT-ERROR",
            "input has %d features, expected %d", (int)point.size(), n_features);
        return nullptr;
    }
    return predictInternal(point, xsink);
}

QoreListNode* QoreGradientBoostedTrees::predictMatrix(const MatrixXd& X,
        ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-GBT-ERROR", "model has not been fitted");
        return nullptr;
    }
    if ((int)X.cols() != n_features) {
        xsink->raiseException("ML-GBT-ERROR",
            "input has %d features, expected %d", (int)X.cols(), n_features);
        return nullptr;
    }

    ReferenceHolder<QoreListNode> results(new QoreListNode(autoTypeInfo), xsink);
    for (int64_t i = 0; i < X.rows(); ++i) {
        if ((i % 100) == 0 && i > 0) {
            if (qore_check_cancel(xsink, "predicting with GBT")) {
                return nullptr;
            }
        }
        QoreHashNode* result = predictInternal(X.row(i), xsink);
        if (*xsink) { return nullptr; }
        results->push(result, xsink);
    }
    return results.release();
}

QoreHashNode* QoreGradientBoostedTrees::getFeatureImportances(
        ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-GBT-ERROR", "model has not been fitted");
        return nullptr;
    }

    VectorXd imp = VectorXd::Zero(n_features);
    for (const auto& tree : trees) {
        imp += tree.featureImportances();
    }
    if (trees.size() > 0) {
        imp /= trees.size();
    }
    double total = imp.sum();
    if (total > 0) { imp /= total; }

    ReferenceHolder<QoreHashNode> rv(
        new QoreHashNode(hashdeclFeatureImportanceInfo, xsink), xsink);
    ReferenceHolder<QoreListNode> names(new QoreListNode(stringTypeInfo), xsink);
    ReferenceHolder<QoreListNode> values(new QoreListNode(autoTypeInfo), xsink);
    for (int i = 0; i < n_features; ++i) {
        if (i < (int)field_names.size()) {
            names->push(new QoreStringNode(field_names[i]), xsink);
        } else {
            names->push(new QoreStringNode("feature_" + std::to_string(i)), xsink);
        }
        values->push(imp(i), xsink);
    }
    rv->setKeyValue("feature_names", names.release(), xsink);
    rv->setKeyValue("importances", values.release(), xsink);
    return rv.release();
}

// --- Serialization ---

std::vector<uint8_t> QoreGradientBoostedTrees::serializeState() const {
    std::vector<uint8_t> buf;
    MLSerialization::writeInt32(buf, n_estimators);
    MLSerialization::writeScalar(buf, learning_rate);
    MLSerialization::writeInt32(buf, max_depth);
    MLSerialization::writeInt32(buf, min_samples_split);
    MLSerialization::writeInt32(buf, min_samples_leaf);
    MLSerialization::writeScalar(buf, subsample);
    MLSerialization::writeString(buf, task);
    MLSerialization::writeScalar(buf, validation_fraction);
    MLSerialization::writeInt32(buf, n_iter_no_change);
    MLSerialization::writeScalar(buf, initial_prediction);
    MLSerialization::writeInt32(buf, actual_n_estimators);
    MLSerialization::writeInt32(buf, n_features);
    MLSerialization::writeInt32(buf, n_classes);
    MLSerialization::writeInt32(buf, (int32_t)classes.size());
    for (double c : classes) {
        MLSerialization::writeScalar(buf, c);
    }
    MLSerialization::writeStringVector(buf, field_names);
    MLSerialization::writeString(buf, target_field);
    MLSerialization::writeInt32(buf, (int32_t)trees.size());
    for (const auto& tree : trees) {
        tree.serialize(buf);
    }
    return buf;
}

QoreGradientBoostedTrees* QoreGradientBoostedTrees::deserializeState(
        const uint8_t* data, size_t len, ExceptionSink* xsink) {
    const uint8_t* ptr = data;
    size_t remaining = len;

    int n_est = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double lr = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int md = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int mss = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int msl = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double ss = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::string task = MLSerialization::readString(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double vf = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int ninc = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double init_pred = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int actual = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int nf = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int nc = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int ncl = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::vector<double> cls(ncl);
    for (int i = 0; i < ncl; ++i) {
        cls[i] = MLSerialization::readScalar(ptr, remaining, xsink);
        if (*xsink) { return nullptr; }
    }
    auto fnames = MLSerialization::readStringVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::string tf = MLSerialization::readString(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int nt = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::vector<FlatTree> trees(nt);
    for (int i = 0; i < nt; ++i) {
        trees[i] = FlatTree::deserialize(ptr, remaining, xsink);
        if (*xsink) { return nullptr; }
    }

    auto* gbt = new QoreGradientBoostedTrees(n_est, lr, md, mss, msl, ss, task, vf, ninc, 42);
    gbt->initial_prediction = init_pred;
    gbt->actual_n_estimators = actual;
    gbt->n_features = nf;
    gbt->n_classes = nc;
    gbt->classes = std::move(cls);
    gbt->field_names = std::move(fnames);
    gbt->target_field = std::move(tf);
    gbt->trees = std::move(trees);
    gbt->fitted = true;
    return gbt;
}
