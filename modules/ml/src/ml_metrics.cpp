/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ml_metrics.cpp

    Qore ml module - evaluation metric implementations

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

#include "ml_metrics.h"

#include <algorithm>
#include <map>

extern const TypedHashDecl* hashdeclConfusionMatrixResult;
extern const TypedHashDecl* hashdeclClassificationReport;
extern const TypedHashDecl* hashdeclClassMetrics;

// ---- Helpers ----

std::vector<double> getUniqueLabels(const VectorXd& vec) {
    std::set<double> unique;
    for (Eigen::Index i = 0; i < vec.size(); ++i) {
        unique.insert(vec(i));
    }
    return std::vector<double>(unique.begin(), unique.end());
}

static int checkVectorLengths(const VectorXd& y_true, const VectorXd& y_pred,
        ExceptionSink* xsink) {
    if (y_true.size() == 0) {
        xsink->raiseException("ML-METRICS-ERROR", "y_true is empty");
        return -1;
    }
    if (y_true.size() != y_pred.size()) {
        xsink->raiseException("ML-METRICS-ERROR",
            "y_true has %d elements but y_pred has %d elements; they must match",
            static_cast<int>(y_true.size()), static_cast<int>(y_pred.size()));
        return -1;
    }
    return 0;
}

// ---- Classification Metrics ----

double mlAccuracy(const VectorXd& y_true, const VectorXd& y_pred, ExceptionSink* xsink) {
    if (checkVectorLengths(y_true, y_pred, xsink)) {
        return 0.0;
    }
    int correct = 0;
    for (Eigen::Index i = 0; i < y_true.size(); ++i) {
        if (y_true(i) == y_pred(i)) {
            ++correct;
        }
    }
    return static_cast<double>(correct) / y_true.size();
}

QoreHashNode* mlConfusionMatrix(const VectorXd& y_true, const VectorXd& y_pred,
        ExceptionSink* xsink) {
    if (checkVectorLengths(y_true, y_pred, xsink)) {
        return nullptr;
    }

    // Get all unique labels from both vectors
    std::set<double> all_labels;
    for (Eigen::Index i = 0; i < y_true.size(); ++i) {
        all_labels.insert(y_true(i));
        all_labels.insert(y_pred(i));
    }
    std::vector<double> labels(all_labels.begin(), all_labels.end());
    int n_classes = static_cast<int>(labels.size());

    // Build label-to-index map
    std::map<double, int> label_idx;
    for (int i = 0; i < n_classes; ++i) {
        label_idx[labels[i]] = i;
    }

    // Build confusion matrix
    std::vector<std::vector<int>> cm(n_classes, std::vector<int>(n_classes, 0));
    for (Eigen::Index i = 0; i < y_true.size(); ++i) {
        int true_idx = label_idx[y_true(i)];
        int pred_idx = label_idx[y_pred(i)];
        cm[true_idx][pred_idx]++;
    }

    // Convert to Qore types
    ReferenceHolder<QoreListNode> matrix_list(new QoreListNode(autoTypeInfo), xsink);
    for (int i = 0; i < n_classes; ++i) {
        ReferenceHolder<QoreListNode> row(new QoreListNode(bigIntTypeInfo), xsink);
        for (int j = 0; j < n_classes; ++j) {
            row->push(static_cast<int64>(cm[i][j]), xsink);
        }
        matrix_list->push(row.release(), xsink);
    }

    ReferenceHolder<QoreListNode> label_list(new QoreListNode(floatTypeInfo), xsink);
    for (double lbl : labels) {
        label_list->push(lbl, xsink);
    }

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclConfusionMatrixResult, xsink), xsink);
    rv->setKeyValue("matrix", matrix_list.release(), xsink);
    rv->setKeyValue("labels", label_list.release(), xsink);

    return rv.release();
}

// Per-class precision/recall/f1 helpers
static void computePerClass(const VectorXd& y_true, const VectorXd& y_pred,
        double class_label, int& tp, int& fp, int& fn) {
    tp = 0;
    fp = 0;
    fn = 0;
    for (Eigen::Index i = 0; i < y_true.size(); ++i) {
        bool is_true = (y_true(i) == class_label);
        bool is_pred = (y_pred(i) == class_label);
        if (is_true && is_pred) {
            ++tp;
        } else if (!is_true && is_pred) {
            ++fp;
        } else if (is_true && !is_pred) {
            ++fn;
        }
    }
}

static double mlPrecision(const VectorXd& y_true, const VectorXd& y_pred,
        double class_label, ExceptionSink* xsink) {
    if (checkVectorLengths(y_true, y_pred, xsink)) {
        return 0.0;
    }
    int tp, fp, fn;
    computePerClass(y_true, y_pred, class_label, tp, fp, fn);
    int denom = tp + fp;
    return (denom > 0) ? static_cast<double>(tp) / denom : 0.0;
}

static double mlRecall(const VectorXd& y_true, const VectorXd& y_pred,
        double class_label, ExceptionSink* xsink) {
    if (checkVectorLengths(y_true, y_pred, xsink)) {
        return 0.0;
    }
    int tp, fp, fn;
    computePerClass(y_true, y_pred, class_label, tp, fp, fn);
    int denom = tp + fn;
    return (denom > 0) ? static_cast<double>(tp) / denom : 0.0;
}

static double mlF1Score(const VectorXd& y_true, const VectorXd& y_pred,
        double class_label, ExceptionSink* xsink) {
    if (checkVectorLengths(y_true, y_pred, xsink)) {
        return 0.0;
    }
    int tp, fp, fn;
    computePerClass(y_true, y_pred, class_label, tp, fp, fn);
    double prec = (tp + fp > 0) ? static_cast<double>(tp) / (tp + fp) : 0.0;
    double rec = (tp + fn > 0) ? static_cast<double>(tp) / (tp + fn) : 0.0;
    return (prec + rec > 0.0) ? 2.0 * prec * rec / (prec + rec) : 0.0;
}

QoreHashNode* mlClassificationReport(const VectorXd& y_true, const VectorXd& y_pred,
        ExceptionSink* xsink) {
    if (checkVectorLengths(y_true, y_pred, xsink)) {
        return nullptr;
    }

    std::vector<double> labels = getUniqueLabels(y_true);
    int n_classes = static_cast<int>(labels.size());
    int n = static_cast<int>(y_true.size());

    // Compute per-class metrics
    double macro_prec = 0.0, macro_rec = 0.0, macro_f1 = 0.0;
    double weighted_f1 = 0.0;
    int total_correct = 0;

    ReferenceHolder<QoreListNode> per_class(
        new QoreListNode(hashdeclClassMetrics->getTypeInfo()), xsink);

    for (int c = 0; c < n_classes; ++c) {
        int tp, fp, fn;
        computePerClass(y_true, y_pred, labels[c], tp, fp, fn);
        int support = tp + fn;

        double prec = (tp + fp > 0) ? static_cast<double>(tp) / (tp + fp) : 0.0;
        double rec = (tp + fn > 0) ? static_cast<double>(tp) / (tp + fn) : 0.0;
        double f1 = (prec + rec > 0.0) ? 2.0 * prec * rec / (prec + rec) : 0.0;

        macro_prec += prec;
        macro_rec += rec;
        macro_f1 += f1;
        weighted_f1 += f1 * support;
        total_correct += tp;

        ReferenceHolder<QoreHashNode> cm(new QoreHashNode(hashdeclClassMetrics, xsink), xsink);
        // Format label as string
        QoreStringNode* label_str = new QoreStringNode;
        label_str->sprintf("%g", labels[c]);
        cm->setKeyValue("label", label_str, xsink);
        cm->setKeyValue("precision", prec, xsink);
        cm->setKeyValue("recall", rec, xsink);
        cm->setKeyValue("f1", f1, xsink);
        cm->setKeyValue("support", static_cast<int64>(support), xsink);
        per_class->push(cm.release(), xsink);
    }

    macro_prec /= n_classes;
    macro_rec /= n_classes;
    macro_f1 /= n_classes;
    weighted_f1 = (n > 0) ? weighted_f1 / n : 0.0;
    double accuracy = (n > 0) ? static_cast<double>(total_correct) / n : 0.0;

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclClassificationReport, xsink), xsink);
    rv->setKeyValue("accuracy", accuracy, xsink);
    rv->setKeyValue("per_class", per_class.release(), xsink);
    rv->setKeyValue("macro_precision", macro_prec, xsink);
    rv->setKeyValue("macro_recall", macro_rec, xsink);
    rv->setKeyValue("macro_f1", macro_f1, xsink);
    rv->setKeyValue("weighted_f1", weighted_f1, xsink);

    return rv.release();
}

// ---- Regression Metrics ----

double mlMSE(const VectorXd& y_true, const VectorXd& y_pred, ExceptionSink* xsink) {
    if (checkVectorLengths(y_true, y_pred, xsink)) {
        return 0.0;
    }
    return (y_true - y_pred).squaredNorm() / y_true.size();
}

double mlRMSE(const VectorXd& y_true, const VectorXd& y_pred, ExceptionSink* xsink) {
    if (checkVectorLengths(y_true, y_pred, xsink)) {
        return 0.0;
    }
    return std::sqrt((y_true - y_pred).squaredNorm() / y_true.size());
}

double mlMAE(const VectorXd& y_true, const VectorXd& y_pred, ExceptionSink* xsink) {
    if (checkVectorLengths(y_true, y_pred, xsink)) {
        return 0.0;
    }
    return (y_true - y_pred).cwiseAbs().mean();
}

double mlR2Score(const VectorXd& y_true, const VectorXd& y_pred, ExceptionSink* xsink) {
    if (checkVectorLengths(y_true, y_pred, xsink)) {
        return 0.0;
    }
    double ss_res = (y_true - y_pred).squaredNorm();
    double y_mean = y_true.mean();
    double ss_tot = (y_true.array() - y_mean).square().sum();
    if (ss_tot < 1e-10) {
        return (ss_res < 1e-10) ? 1.0 : 0.0;
    }
    return 1.0 - ss_res / ss_tot;
}

double mlExplainedVariance(const VectorXd& y_true, const VectorXd& y_pred, ExceptionSink* xsink) {
    if (checkVectorLengths(y_true, y_pred, xsink)) {
        return 0.0;
    }
    VectorXd residual = y_true - y_pred;
    double var_residual = (residual.array() - residual.mean()).square().mean();
    double var_y = (y_true.array() - y_true.mean()).square().mean();
    if (var_y < 1e-10) {
        return (var_residual < 1e-10) ? 1.0 : 0.0;
    }
    return 1.0 - var_residual / var_y;
}

// ---- Clustering Metrics ----

double mlSilhouetteScore(const MatrixXd& data, const VectorXd& labels,
        ExceptionSink* xsink) {
    int n = static_cast<int>(data.rows());
    if (n == 0) {
        xsink->raiseException("ML-METRICS-ERROR", "data is empty");
        return 0.0;
    }
    if (data.rows() != labels.size()) {
        xsink->raiseException("ML-METRICS-ERROR",
            "data has %d rows but labels has %d elements",
            n, static_cast<int>(labels.size()));
        return 0.0;
    }

    std::vector<double> unique = getUniqueLabels(labels);
    int n_clusters = static_cast<int>(unique.size());
    if (n_clusters < 2) {
        xsink->raiseException("ML-METRICS-ERROR",
            "silhouette score requires at least 2 clusters; got %d", n_clusters);
        return 0.0;
    }

    // Build label-to-index map
    std::map<double, int> label_idx;
    for (int i = 0; i < n_clusters; ++i) {
        label_idx[unique[i]] = i;
    }

    // Assign each point to its cluster index
    std::vector<int> assignments(n);
    for (int i = 0; i < n; ++i) {
        assignments[i] = label_idx[labels(i)];
    }

    double total_silhouette = 0.0;
    for (int i = 0; i < n; ++i) {
        if (i % 100 == 0 && qore_check_cancel(xsink, "silhouette score")) {
            return 0.0;
        }

        int my_cluster = assignments[i];

        // Compute mean distance to same-cluster points (a_i)
        double sum_same = 0.0;
        int count_same = 0;
        for (int j = 0; j < n; ++j) {
            if (j != i && assignments[j] == my_cluster) {
                sum_same += (data.row(i) - data.row(j)).norm();
                ++count_same;
            }
        }
        double a_i = (count_same > 0) ? sum_same / count_same : 0.0;

        // Compute min mean distance to other clusters (b_i)
        double b_i = std::numeric_limits<double>::max();
        for (int c = 0; c < n_clusters; ++c) {
            if (c == my_cluster) {
                continue;
            }
            double sum_other = 0.0;
            int count_other = 0;
            for (int j = 0; j < n; ++j) {
                if (assignments[j] == c) {
                    sum_other += (data.row(i) - data.row(j)).norm();
                    ++count_other;
                }
            }
            if (count_other > 0) {
                double mean_other = sum_other / count_other;
                if (mean_other < b_i) {
                    b_i = mean_other;
                }
            }
        }

        double max_ab = std::max(a_i, b_i);
        double s_i = (max_ab > 1e-10) ? (b_i - a_i) / max_ab : 0.0;
        total_silhouette += s_i;
    }

    return total_silhouette / n;
}

double mlDaviesBouldinScore(const MatrixXd& data, const VectorXd& labels,
        ExceptionSink* xsink) {
    int n = static_cast<int>(data.rows());
    if (n == 0) {
        xsink->raiseException("ML-METRICS-ERROR", "data is empty");
        return 0.0;
    }
    if (data.rows() != labels.size()) {
        xsink->raiseException("ML-METRICS-ERROR",
            "data has %d rows but labels has %d elements",
            n, static_cast<int>(labels.size()));
        return 0.0;
    }

    std::vector<double> unique = getUniqueLabels(labels);
    int n_clusters = static_cast<int>(unique.size());
    if (n_clusters < 2) {
        xsink->raiseException("ML-METRICS-ERROR",
            "Davies-Bouldin score requires at least 2 clusters; got %d", n_clusters);
        return 0.0;
    }

    std::map<double, int> label_idx;
    for (int i = 0; i < n_clusters; ++i) {
        label_idx[unique[i]] = i;
    }

    // Compute cluster centroids and within-cluster scatter (S_i)
    int d = static_cast<int>(data.cols());
    std::vector<VectorXd> centroids(n_clusters, VectorXd::Zero(d));
    std::vector<int> counts(n_clusters, 0);

    for (int i = 0; i < n; ++i) {
        int c = label_idx[labels(i)];
        centroids[c] += data.row(i).transpose();
        counts[c]++;
    }
    for (int c = 0; c < n_clusters; ++c) {
        if (counts[c] > 0) {
            centroids[c] /= counts[c];
        }
    }

    // S_i = average distance of points in cluster i to centroid i
    std::vector<double> scatter(n_clusters, 0.0);
    for (int i = 0; i < n; ++i) {
        int c = label_idx[labels(i)];
        scatter[c] += (data.row(i).transpose() - centroids[c]).norm();
    }
    for (int c = 0; c < n_clusters; ++c) {
        if (counts[c] > 0) {
            scatter[c] /= counts[c];
        }
    }

    // DB index = (1/k) * sum_i max_{j!=i} (S_i + S_j) / d(c_i, c_j)
    double db_sum = 0.0;
    for (int i = 0; i < n_clusters; ++i) {
        double max_ratio = 0.0;
        for (int j = 0; j < n_clusters; ++j) {
            if (j == i) {
                continue;
            }
            double dist_ij = (centroids[i] - centroids[j]).norm();
            if (dist_ij > 1e-10) {
                double ratio = (scatter[i] + scatter[j]) / dist_ij;
                if (ratio > max_ratio) {
                    max_ratio = ratio;
                }
            }
        }
        db_sum += max_ratio;
    }

    return db_sum / n_clusters;
}

double mlCalinskiHarabaszScore(const MatrixXd& data, const VectorXd& labels,
        ExceptionSink* xsink) {
    int n = static_cast<int>(data.rows());
    if (n == 0) {
        xsink->raiseException("ML-METRICS-ERROR", "data is empty");
        return 0.0;
    }
    if (data.rows() != labels.size()) {
        xsink->raiseException("ML-METRICS-ERROR",
            "data has %d rows but labels has %d elements",
            n, static_cast<int>(labels.size()));
        return 0.0;
    }

    std::vector<double> unique = getUniqueLabels(labels);
    int n_clusters = static_cast<int>(unique.size());
    if (n_clusters < 2) {
        xsink->raiseException("ML-METRICS-ERROR",
            "Calinski-Harabasz score requires at least 2 clusters; got %d", n_clusters);
        return 0.0;
    }
    if (n_clusters >= n) {
        xsink->raiseException("ML-METRICS-ERROR",
            "Calinski-Harabasz score requires more samples (%d) than clusters (%d)",
            n, n_clusters);
        return 0.0;
    }

    std::map<double, int> label_idx;
    for (int i = 0; i < n_clusters; ++i) {
        label_idx[unique[i]] = i;
    }

    int d = static_cast<int>(data.cols());

    // Overall centroid
    VectorXd overall_centroid = data.colwise().mean();

    // Cluster centroids and counts
    std::vector<VectorXd> centroids(n_clusters, VectorXd::Zero(d));
    std::vector<int> counts(n_clusters, 0);
    for (int i = 0; i < n; ++i) {
        int c = label_idx[labels(i)];
        centroids[c] += data.row(i).transpose();
        counts[c]++;
    }
    for (int c = 0; c < n_clusters; ++c) {
        if (counts[c] > 0) {
            centroids[c] /= counts[c];
        }
    }

    // Between-cluster dispersion (B_k)
    double bk = 0.0;
    for (int c = 0; c < n_clusters; ++c) {
        bk += counts[c] * (centroids[c] - overall_centroid).squaredNorm();
    }

    // Within-cluster dispersion (W_k)
    double wk = 0.0;
    for (int i = 0; i < n; ++i) {
        int c = label_idx[labels(i)];
        wk += (data.row(i).transpose() - centroids[c]).squaredNorm();
    }

    if (wk < 1e-10) {
        return 0.0;
    }

    // CH = (B_k / (k-1)) / (W_k / (n-k))
    return (bk / (n_clusters - 1)) / (wk / (n - n_clusters));
}
