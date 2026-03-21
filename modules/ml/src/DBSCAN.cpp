/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    DBSCAN.cpp

    Qore ml module - DBSCAN class implementation

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

#include "QC_DBSCAN.h"

// Extern declaration for hashdecl (defined in ml-module.cpp)
extern const TypedHashDecl* hashdeclDBSCANResult;

// NOTE: Computes and stores the full n x n distance matrix, requiring O(n^2) memory.
// This is suitable for small-to-medium datasets; for large n consider a spatial index approach.
MatrixXd QoreDBSCAN::computeDistanceMatrix(const MatrixXd& data, ExceptionSink* xsink) const {
    int n = static_cast<int>(data.rows());
    MatrixXd dist(n, n);
    dist.setZero();

    if (metric == "euclidean") {
        for (int i = 0; i < n; ++i) {
            if (i % 100 == 0 && qore_check_cancel(xsink, "DBSCAN computeDistanceMatrix")) {
                return MatrixXd();
            }
            for (int j = i + 1; j < n; ++j) {
                double d = (data.row(i) - data.row(j)).norm();
                dist(i, j) = d;
                dist(j, i) = d;
            }
        }
    } else if (metric == "manhattan") {
        for (int i = 0; i < n; ++i) {
            if (i % 100 == 0 && qore_check_cancel(xsink, "DBSCAN computeDistanceMatrix")) {
                return MatrixXd();
            }
            for (int j = i + 1; j < n; ++j) {
                double d = (data.row(i) - data.row(j)).cwiseAbs().sum();
                dist(i, j) = d;
                dist(j, i) = d;
            }
        }
    } else {
        // cosine distance = 1 - cosine_similarity
        // cosine_similarity = dot(a, b) / (|a| * |b|)
        // Pre-compute norms
        VectorXd norms(n);
        for (int i = 0; i < n; ++i) {
            norms(i) = data.row(i).norm();
        }
        for (int i = 0; i < n; ++i) {
            if (i % 100 == 0 && qore_check_cancel(xsink, "DBSCAN computeDistanceMatrix")) {
                return MatrixXd();
            }
            for (int j = i + 1; j < n; ++j) {
                double d;
                if (norms(i) == 0.0 || norms(j) == 0.0) {
                    d = 1.0;  // max cosine distance when either vector is zero
                } else {
                    double sim = data.row(i).dot(data.row(j)) / (norms(i) * norms(j));
                    // Clamp to [-1, 1] for numerical stability
                    sim = std::max(-1.0, std::min(1.0, sim));
                    d = 1.0 - sim;
                }
                dist(i, j) = d;
                dist(j, i) = d;
            }
        }
    }

    return dist;
}

std::vector<int> QoreDBSCAN::findNeighbors(const MatrixXd& dist_matrix, int point_idx) const {
    std::vector<int> neighbors;
    int n = static_cast<int>(dist_matrix.rows());
    for (int i = 0; i < n; ++i) {
        if (i != point_idx && dist_matrix(point_idx, i) <= epsilon) {
            neighbors.push_back(i);
        }
    }
    return neighbors;
}

QoreListNode* QoreDBSCAN::cluster(const MatrixXd& data, ExceptionSink* xsink) {
    int n = static_cast<int>(data.rows());
    if (n == 0) {
        xsink->raiseException("ML-DBSCAN-ERROR",
            "no data provided: at least one sample is required");
        return nullptr;
    }

    // Compute distance matrix
    MatrixXd dist_matrix = computeDistanceMatrix(data, xsink);
    if (*xsink) {
        return nullptr;
    }

    // Find neighbors and identify core points
    std::vector<std::vector<int>> all_neighbors(n);
    std::vector<bool> is_core(n, false);

    for (int i = 0; i < n; ++i) {
        if (i % 100 == 0 && qore_check_cancel(xsink, "DBSCAN cluster")) {
            return nullptr;
        }
        all_neighbors[i] = findNeighbors(dist_matrix, i);
        // MinPts includes the point itself; findNeighbors() excludes it
        if (static_cast<int>(all_neighbors[i].size()) + 1 >= min_points) {
            is_core[i] = true;
        }
    }

    // Assign clusters using BFS from core points
    std::vector<int> labels(n, -1);  // -1 = noise
    int cluster_id = 0;

    for (int i = 0; i < n; ++i) {
        if (!is_core[i] || labels[i] != -1) {
            continue;
        }

        // BFS to expand cluster from core point i
        labels[i] = cluster_id;
        std::queue<int> queue;
        queue.push(i);

        while (!queue.empty()) {
            int current = queue.front();
            queue.pop();

            for (int neighbor : all_neighbors[current]) {
                if (labels[neighbor] == -1) {
                    labels[neighbor] = cluster_id;
                    // If neighbor is also a core point, expand from it
                    if (is_core[neighbor]) {
                        queue.push(neighbor);
                    }
                }
            }
        }

        ++cluster_id;
    }

    // Build result list
    ReferenceHolder<QoreListNode> rv(
        new QoreListNode(hashdeclDBSCANResult->getTypeInfo()), xsink);

    for (int i = 0; i < n; ++i) {
        ReferenceHolder<QoreHashNode> h(
            new QoreHashNode(hashdeclDBSCANResult, xsink), xsink);

        h->setKeyValue("cluster", static_cast<int64>(labels[i]), xsink);
        h->setKeyValue("is_core", static_cast<bool>(is_core[i]), xsink);
        h->setKeyValue("neighbor_count", static_cast<int64>(all_neighbors[i].size()), xsink);

        rv->push(h.release(), xsink);
    }

    return rv.release();
}
