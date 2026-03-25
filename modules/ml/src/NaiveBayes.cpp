/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    NaiveBayes.cpp

    Gaussian Naive Bayes implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "QC_NaiveBayes.h"
#include "ml_serialization.h"

#include <cmath>
#include <set>

extern const TypedHashDecl* hashdeclNaiveBayesResult;

QoreNaiveBayes::QoreNaiveBayes(double var_smoothing)
    : var_smoothing(var_smoothing) {
}

double QoreNaiveBayes::logPdf(double x, double mean, double var) const {
    // Log of Gaussian PDF: -0.5 * (log(2*pi*var) + (x-mean)^2/var)
    double diff = x - mean;
    return -0.5 * (std::log(2.0 * M_PI * var) + diff * diff / var);
}

void QoreNaiveBayes::fit(const MatrixXd& X, const VectorXd& y,
        ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (X.rows() == 0 || X.cols() == 0) {
        xsink->raiseException("ML-NAIVE-BAYES-ERROR", "cannot fit on empty data");
        return;
    }
    if (X.rows() != y.size()) {
        xsink->raiseException("ML-NAIVE-BAYES-ERROR",
            "X has " QLLD " rows but y has " QLLD " elements",
            (int64_t)X.rows(), (int64_t)y.size());
        return;
    }

    n_features = (int)X.cols();
    total_samples = (int)X.rows();

    // Determine unique classes
    std::set<double> unique_classes;
    for (int64_t i = 0; i < y.size(); ++i) {
        unique_classes.insert(y(i));
    }
    classes.assign(unique_classes.begin(), unique_classes.end());
    n_classes = (int)classes.size();

    // Compute per-class statistics using Welford's online algorithm (single pass)
    class_stats.resize(n_classes);
    for (int c = 0; c < n_classes; ++c) {
        ClassStats& cs = class_stats[c];
        cs.count = 0;
        cs.mean = VectorXd::Zero(n_features);
        cs.var = VectorXd::Zero(n_features);  // accumulates M2 (sum of squared diffs)
    }

    // Single pass: accumulate mean and M2 per class using Welford's method
    for (int64_t i = 0; i < y.size(); ++i) {
        // Find class index
        int c = -1;
        for (int k = 0; k < n_classes; ++k) {
            if (y(i) == classes[k]) { c = k; break; }
        }
        if (c < 0) { continue; }

        ClassStats& cs = class_stats[c];
        cs.count++;
        VectorXd x = X.row(i).transpose();
        VectorXd delta = x - cs.mean;
        cs.mean += delta / cs.count;
        VectorXd delta2 = x - cs.mean;
        cs.var += (delta.array() * delta2.array()).matrix();  // M2 accumulator
    }

    // Finalize: convert M2 to population variance, add smoothing, compute priors
    for (int c = 0; c < n_classes; ++c) {
        ClassStats& cs = class_stats[c];
        if (cs.count > 1) {
            cs.var /= cs.count;  // population variance (matches sklearn)
        } else {
            cs.var = VectorXd::Zero(n_features);
        }
        cs.var.array() += var_smoothing;
        cs.prior = std::log((double)cs.count / total_samples);
    }

    fitted = true;
}

void QoreNaiveBayes::update(const MatrixXd& X, const VectorXd& y,
        ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);

    if (!fitted) {
        xsink->raiseException("ML-NAIVE-BAYES-ERROR",
            "model has not been fitted; call fit() first");
        return;
    }

    // Online update: adjust means and variances with new data
    for (int c = 0; c < n_classes; ++c) {
        std::vector<int> indices;
        for (int64_t i = 0; i < y.size(); ++i) {
            if (y(i) == classes[c]) {
                indices.push_back(i);
            }
        }
        if (indices.empty()) {
            continue;
        }

        ClassStats& cs = class_stats[c];
        int new_count = (int)indices.size();
        int combined = cs.count + new_count;

        // New batch mean
        VectorXd new_mean = VectorXd::Zero(n_features);
        for (int i : indices) {
            new_mean += X.row(i).transpose();
        }
        new_mean /= new_count;

        // New batch variance
        VectorXd new_var = VectorXd::Zero(n_features);
        for (int i : indices) {
            VectorXd diff = X.row(i).transpose() - new_mean;
            new_var += diff.array().square().matrix();
        }
        if (new_count > 1) {
            new_var /= new_count;
        }

        // Combined variance (parallel algorithm)
        VectorXd mean_diff = new_mean - cs.mean;
        VectorXd combined_var = (cs.count * cs.var.array() + new_count * new_var.array()
            + cs.count * new_count * mean_diff.array().square() / combined).matrix()
            / combined;

        // Combined mean
        VectorXd combined_mean = (cs.count * cs.mean + new_count * new_mean) / combined;

        cs.mean = combined_mean;
        cs.var = combined_var;
        cs.var.array() += var_smoothing;
        cs.count = combined;
    }

    total_samples += (int)X.rows();
    // Update priors
    for (int c = 0; c < n_classes; ++c) {
        class_stats[c].prior = std::log((double)class_stats[c].count / total_samples);
    }
}

QoreHashNode* QoreNaiveBayes::predictInternal(const RowVectorXd& point,
        ExceptionSink* xsink) const {
    // Compute log posterior for each class
    std::vector<double> log_posteriors(n_classes);
    double max_log = -std::numeric_limits<double>::infinity();

    for (int c = 0; c < n_classes; ++c) {
        double log_post = class_stats[c].prior;
        for (int f = 0; f < n_features; ++f) {
            log_post += logPdf(point(f), class_stats[c].mean(f),
                class_stats[c].var(f));
        }
        log_posteriors[c] = log_post;
        if (log_post > max_log) {
            max_log = log_post;
        }
    }

    // Convert to probabilities via log-sum-exp
    double sum_exp = 0;
    for (int c = 0; c < n_classes; ++c) {
        sum_exp += std::exp(log_posteriors[c] - max_log);
    }
    double log_norm = max_log + std::log(sum_exp);

    int best_class = 0;
    double best_prob = 0;
    std::vector<double> probs(n_classes);
    for (int c = 0; c < n_classes; ++c) {
        probs[c] = std::exp(log_posteriors[c] - log_norm);
        if (probs[c] > best_prob) {
            best_prob = probs[c];
            best_class = c;
        }
    }

    ReferenceHolder<QoreHashNode> rv(
        new QoreHashNode(hashdeclNaiveBayesResult, xsink), xsink);
    rv->setKeyValue("predicted_class", (int64_t)classes[best_class], xsink);
    rv->setKeyValue("max_probability", best_prob, xsink);

    ReferenceHolder<QoreListNode> prob_list(new QoreListNode(autoTypeInfo), xsink);
    for (double p : probs) {
        prob_list->push(p, xsink);
    }
    rv->setKeyValue("probabilities", prob_list.release(), xsink);

    ReferenceHolder<QoreListNode> cls_list(new QoreListNode(autoTypeInfo), xsink);
    for (double c : classes) {
        cls_list->push(c, xsink);
    }
    rv->setKeyValue("classes", cls_list.release(), xsink);

    return rv.release();
}

QoreHashNode* QoreNaiveBayes::predict(const RowVectorXd& point,
        ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-NAIVE-BAYES-ERROR", "model has not been fitted");
        return nullptr;
    }
    if (point.size() != n_features) {
        xsink->raiseException("ML-NAIVE-BAYES-ERROR",
            "input has %d features, expected %d", (int)point.size(), n_features);
        return nullptr;
    }
    return predictInternal(point, xsink);
}

QoreListNode* QoreNaiveBayes::predictMatrix(const MatrixXd& X,
        ExceptionSink* xsink) const {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-NAIVE-BAYES-ERROR", "model has not been fitted");
        return nullptr;
    }
    if ((int)X.cols() != n_features) {
        xsink->raiseException("ML-NAIVE-BAYES-ERROR",
            "input has %d features, expected %d", (int)X.cols(), n_features);
        return nullptr;
    }

    ReferenceHolder<QoreListNode> results(new QoreListNode(autoTypeInfo), xsink);
    for (int64_t i = 0; i < X.rows(); ++i) {
        QoreHashNode* result = predictInternal(X.row(i), xsink);
        if (*xsink) { return nullptr; }
        results->push(result, xsink);
    }
    return results.release();
}

// --- Serialization ---

std::vector<uint8_t> QoreNaiveBayes::serializeState() const {
    std::vector<uint8_t> buf;
    MLSerialization::writeScalar(buf, var_smoothing);
    MLSerialization::writeInt32(buf, n_features);
    MLSerialization::writeInt32(buf, n_classes);
    MLSerialization::writeInt32(buf, total_samples);
    MLSerialization::writeInt32(buf, (int32_t)classes.size());
    for (double c : classes) {
        MLSerialization::writeScalar(buf, c);
    }
    for (const auto& cs : class_stats) {
        MLSerialization::writeScalar(buf, cs.prior);
        MLSerialization::writeVector(buf, cs.mean);
        MLSerialization::writeVector(buf, cs.var);
        MLSerialization::writeInt32(buf, cs.count);
    }
    MLSerialization::writeStringVector(buf, field_names);
    MLSerialization::writeString(buf, target_field);
    return buf;
}

QoreNaiveBayes* QoreNaiveBayes::deserializeState(const uint8_t* data,
        size_t len, ExceptionSink* xsink) {
    const uint8_t* ptr = data;
    size_t remaining = len;

    double vs = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int nf = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int nc = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int ts = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int ncl = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::vector<double> cls(ncl);
    for (int i = 0; i < ncl; ++i) {
        cls[i] = MLSerialization::readScalar(ptr, remaining, xsink);
        if (*xsink) { return nullptr; }
    }
    std::vector<ClassStats> stats(nc);
    for (int i = 0; i < nc; ++i) {
        stats[i].prior = MLSerialization::readScalar(ptr, remaining, xsink);
        if (*xsink) { return nullptr; }
        stats[i].mean = MLSerialization::readVector(ptr, remaining, xsink);
        if (*xsink) { return nullptr; }
        stats[i].var = MLSerialization::readVector(ptr, remaining, xsink);
        if (*xsink) { return nullptr; }
        stats[i].count = MLSerialization::readInt32(ptr, remaining, xsink);
        if (*xsink) { return nullptr; }
    }
    auto fnames = MLSerialization::readStringVector(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::string tf = MLSerialization::readString(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }

    auto* nb = new QoreNaiveBayes(vs);
    nb->n_features = nf;
    nb->n_classes = nc;
    nb->total_samples = ts;
    nb->classes = std::move(cls);
    nb->class_stats = std::move(stats);
    nb->field_names = std::move(fnames);
    nb->target_field = std::move(tf);
    nb->fitted = true;
    return nb;
}
