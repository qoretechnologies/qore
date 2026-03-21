/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    KMeans.cpp

    Qore ml module - KMeans class implementation

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

#include "QC_KMeans.h"

#include <numeric>

// Extern declaration for hashdecl (defined in ml-module.cpp)
extern const TypedHashDecl* hashdeclKMeansResult;

void QoreKMeans::initKMeansPP(const MatrixXd& data) {
    int n = static_cast<int>(data.rows());
    int d = static_cast<int>(data.cols());

    centroids.resize(k, d);

    // Choose first centroid uniformly at random
    std::uniform_int_distribution<int> dist(0, n - 1);
    int first = dist(rng);
    centroids.row(0) = data.row(first);

    // For each subsequent centroid, choose with probability proportional to D(x)^2
    VectorXd min_distances(n);
    for (int c = 1; c < k; ++c) {
        // Compute distance to nearest existing centroid for each point
        for (int i = 0; i < n; ++i) {
            double min_d = std::numeric_limits<double>::max();
            for (int j = 0; j < c; ++j) {
                double d_val = (data.row(i) - centroids.row(j)).squaredNorm();
                if (d_val < min_d) {
                    min_d = d_val;
                }
            }
            min_distances(i) = min_d;
        }

        // Normalize to get probabilities
        double total = min_distances.sum();
        if (total <= 0.0) {
            // All distances are zero; choose randomly
            int idx = dist(rng);
            centroids.row(c) = data.row(idx);
            continue;
        }

        // Choose next centroid using weighted random selection
        std::uniform_real_distribution<double> uni(0.0, total);
        double target = uni(rng);
        double cumsum = 0.0;
        int chosen = n - 1;
        for (int i = 0; i < n; ++i) {
            cumsum += min_distances(i);
            if (cumsum >= target) {
                chosen = i;
                break;
            }
        }
        centroids.row(c) = data.row(chosen);
    }
}

void QoreKMeans::initRandom(const MatrixXd& data) {
    int n = static_cast<int>(data.rows());
    int d = static_cast<int>(data.cols());

    centroids.resize(k, d);

    // Randomly select k distinct data points as initial centroids
    std::vector<int> indices(n);
    std::iota(indices.begin(), indices.end(), 0);

    // Fisher-Yates shuffle for first k elements
    for (int i = 0; i < k && i < n; ++i) {
        std::uniform_int_distribution<int> dist(i, n - 1);
        int j = dist(rng);
        std::swap(indices[i], indices[j]);
    }

    for (int i = 0; i < k; ++i) {
        centroids.row(i) = data.row(indices[i]);
    }
}

std::vector<int> QoreKMeans::assignClusters(const MatrixXd& data) const {
    int n = static_cast<int>(data.rows());
    std::vector<int> assignments(n);

    for (int i = 0; i < n; ++i) {
        double min_dist = std::numeric_limits<double>::max();
        int best = 0;
        for (int c = 0; c < k; ++c) {
            double d = (data.row(i) - centroids.row(c)).squaredNorm();
            if (d < min_dist) {
                min_dist = d;
                best = c;
            }
        }
        assignments[i] = best;
    }

    return assignments;
}

void QoreKMeans::updateCentroids(const MatrixXd& data, const std::vector<int>& assignments) {
    int d = static_cast<int>(data.cols());

    MatrixXd new_centroids = MatrixXd::Zero(k, d);
    std::vector<int> counts(k, 0);

    int n = static_cast<int>(data.rows());
    for (int i = 0; i < n; ++i) {
        new_centroids.row(assignments[i]) += data.row(i);
        counts[assignments[i]]++;
    }

    for (int c = 0; c < k; ++c) {
        if (counts[c] > 0) {
            centroids.row(c) = new_centroids.row(c) / counts[c];
        }
    }

    centroid_counts = counts;
}

void QoreKMeans::fit(const MatrixXd& data, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);
    int n = static_cast<int>(data.rows());
    if (n == 0) {
        xsink->raiseException("ML-KMEANS-ERROR",
            "no data provided: at least one sample is required");
        return;
    }
    if (k > n) {
        xsink->raiseException("ML-KMEANS-ERROR",
            "k (%d) is larger than the number of samples (%d); "
            "reduce k or provide more data", k, n);
        return;
    }

    n_features = static_cast<int>(data.cols());

    // Initialize centroids
    if (init_method == "kmeans++") {
        initKMeansPP(data);
    } else {
        initRandom(data);
    }

    centroid_counts.resize(k, 0);

    // Lloyd's algorithm
    for (int iter = 0; iter < max_iterations; ++iter) {
        if (qore_check_cancel(xsink, "KMeans fit")) {
            return;
        }

        MatrixXd old_centroids = centroids;

        std::vector<int> assignments = assignClusters(data);
        updateCentroids(data, assignments);

        // Check convergence: max centroid movement
        double max_movement = 0.0;
        for (int c = 0; c < k; ++c) {
            double movement = (centroids.row(c) - old_centroids.row(c)).norm();
            if (movement > max_movement) {
                max_movement = movement;
            }
        }
        if (max_movement < tolerance) {
            break;
        }
    }

    // Compute inertia
    inertia = 0.0;
    std::vector<int> final_assignments = assignClusters(data);
    for (int i = 0; i < n; ++i) {
        inertia += (data.row(i) - centroids.row(final_assignments[i])).squaredNorm();
    }

    fitted = true;
}

void QoreKMeans::update(const MatrixXd& data, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-KMEANS-ERROR",
            "model has not been fitted: call fit() or fitMatrix() before update()");
        return;
    }
    if (data.cols() != n_features) {
        xsink->raiseException("ML-KMEANS-ERROR",
            "input has %d features but model was trained with %d features",
            static_cast<int>(data.cols()), n_features);
        return;
    }

    int n = static_cast<int>(data.rows());

    // Mini-batch update (Sculley 2010)
    for (int i = 0; i < n; ++i) {
        if (i % 100 == 0 && qore_check_cancel(xsink, "KMeans update")) {
            return;
        }
        // Find nearest centroid
        double min_dist = std::numeric_limits<double>::max();
        int best = 0;
        for (int c = 0; c < k; ++c) {
            double d = (data.row(i) - centroids.row(c)).squaredNorm();
            if (d < min_dist) {
                min_dist = d;
                best = c;
            }
        }

        // Update centroid with learning rate 1/(count+1)
        centroid_counts[best]++;
        double lr = 1.0 / centroid_counts[best];
        centroids.row(best) = (1.0 - lr) * centroids.row(best) + lr * data.row(i);
    }

    // Recompute inertia on this batch
    inertia = 0.0;
    for (int i = 0; i < n; ++i) {
        double min_dist = std::numeric_limits<double>::max();
        for (int c = 0; c < k; ++c) {
            double d = (data.row(i) - centroids.row(c)).squaredNorm();
            if (d < min_dist) {
                min_dist = d;
            }
        }
        inertia += min_dist;
    }
}

QoreHashNode* QoreKMeans::predictInternal(const RowVectorXd& point, ExceptionSink* xsink) const {
    if (point.size() != n_features) {
        xsink->raiseException("ML-KMEANS-ERROR",
            "input has %d features but model was trained with %d features",
            static_cast<int>(point.size()), n_features);
        return nullptr;
    }

    // Compute distances to all centroids
    ReferenceHolder<QoreListNode> distances(new QoreListNode(autoTypeInfo), xsink);
    double min_dist = std::numeric_limits<double>::max();
    int best = 0;

    for (int c = 0; c < k; ++c) {
        double d = (point - centroids.row(c)).norm();
        distances->push(d, xsink);
        if (d < min_dist) {
            min_dist = d;
            best = c;
        }
    }

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclKMeansResult, xsink), xsink);
    rv->setKeyValue("cluster", static_cast<int64>(best), xsink);
    rv->setKeyValue("distance", min_dist, xsink);
    rv->setKeyValue("centroid_distances", distances.release(), xsink);

    return rv.release();
}

QoreHashNode* QoreKMeans::predict(const RowVectorXd& point, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-KMEANS-ERROR",
            "model has not been fitted: call fit() or fitMatrix() before predict()");
        return nullptr;
    }
    return predictInternal(point, xsink);
}

QoreListNode* QoreKMeans::predictMatrix(const MatrixXd& data, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-KMEANS-ERROR",
            "model has not been fitted: call fit() or fitMatrix() before predictMatrix()");
        return nullptr;
    }
    if (data.cols() != n_features) {
        xsink->raiseException("ML-KMEANS-ERROR",
            "input has %d features but model was trained with %d features",
            static_cast<int>(data.cols()), n_features);
        return nullptr;
    }

    ReferenceHolder<QoreListNode> rv(new QoreListNode(hashdeclKMeansResult->getTypeInfo()), xsink);
    for (Eigen::Index i = 0; i < data.rows(); ++i) {
        if (i % 100 == 0 && qore_check_cancel(xsink, "KMeans predict")) {
            return nullptr;
        }
        RowVectorXd row = data.row(i);
        QoreHashNode* result = predictInternal(row, xsink);
        if (*xsink) {
            return nullptr;
        }
        rv->push(result, xsink);
    }
    return rv.release();
}

QoreListNode* QoreKMeans::getCentroids(ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-KMEANS-ERROR",
            "model has not been fitted: call fit() or fitMatrix() first");
        return nullptr;
    }

    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    for (int c = 0; c < k; ++c) {
        ReferenceHolder<QoreListNode> row(new QoreListNode(autoTypeInfo), xsink);
        for (int j = 0; j < n_features; ++j) {
            row->push(centroids(c, j), xsink);
        }
        rv->push(row.release(), xsink);
    }
    return rv.release();
}
