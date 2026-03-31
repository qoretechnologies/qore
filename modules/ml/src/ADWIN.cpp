/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ADWIN.cpp

    ADWIN (Adaptive Windowing) drift detector implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "QC_ADWIN.h"
#include "ml_serialization.h"

#include <cmath>

QoreADWIN::QoreADWIN(double delta) : delta(delta) {
}

bool QoreADWIN::add(double value) {
    std::lock_guard<std::mutex> lk(mtx);

    // Add as a new bucket with count=1
    buckets.push_back(Bucket{value, 0.0, 1});
    window_sum += value;
    window_count += 1;
    ++total;

    // Update running variance (Welford's incremental)
    if (window_count > 1) {
        double old_mean = (window_sum - value) / (window_count - 1);
        double new_mean = window_sum / window_count;
        window_var += (value - old_mean) * (value - new_mean);
    }

    // Compress: merge adjacent equal-count buckets (logarithmic compression)
    // Keep at most 2 buckets of each size level
    int max_buckets_per_level = 2;
    for (int i = (int)buckets.size() - 1; i > 0; --i) {
        if (buckets[i].count == buckets[i - 1].count) {
            // Count consecutive buckets of the same count
            int same_count = 0;
            for (int j = i; j >= 0 && buckets[j].count == buckets[i].count; --j) {
                ++same_count;
            }
            if (same_count > max_buckets_per_level) {
                // Merge the two oldest buckets of this level
                int merge_idx = i - same_count + 1;
                if (merge_idx + 1 < (int)buckets.size()) {
                    Bucket& a = buckets[merge_idx];
                    Bucket& b = buckets[merge_idx + 1];
                    int n_a = a.count;
                    int n_b = b.count;
                    double mean_a = a.total / n_a;
                    double mean_b = b.total / n_b;
                    int n_new = n_a + n_b;
                    double total_new = a.total + b.total;
                    // Combined variance: var(A+B) = var(A) + var(B) + nA*nB/(nA+nB) * (meanA-meanB)^2
                    double var_new = a.variance + b.variance
                        + (double)n_a * n_b / n_new * (mean_a - mean_b) * (mean_a - mean_b);
                    a.total = total_new;
                    a.variance = var_new;
                    a.count = n_new;
                    buckets.erase(buckets.begin() + merge_idx + 1);
                    // Continue checking at the same level
                    i = merge_idx;
                }
            }
        }
    }

    // Try to detect drift by checking cuts
    drift = false;
    shrinkWindow();

    return drift;
}

void QoreADWIN::shrinkWindow() {
    // Try removing the oldest bucket and check if means differ significantly
    while (buckets.size() > 1) {
        // Compute stats for the oldest bucket (window 0) vs rest (window 1)
        const Bucket& oldest = buckets.front();
        int n0 = oldest.count;
        double sum0 = oldest.total;
        double var0 = oldest.variance;
        double mean0 = sum0 / n0;

        int n1 = window_count - n0;
        double sum1 = window_sum - sum0;
        double var1 = window_var - var0
            - (double)n0 * n1 / window_count * (mean0 - sum1 / n1) * (mean0 - sum1 / n1);
        if (var1 < 0) { var1 = 0; }
        double mean1 = sum1 / n1;

        if (n1 < 1) {
            break;
        }

        if (testCut(n0, mean0, var0, n1, mean1, var1)) {
            // Drift detected — remove oldest bucket
            window_sum -= oldest.total;
            window_count -= oldest.count;
            // Recompute window_var from remaining buckets
            window_var = 0;
            double running_sum = 0;
            int running_count = 0;
            for (size_t i = 1; i < buckets.size(); ++i) {
                double bucket_mean = buckets[i].total / buckets[i].count;
                if (running_count > 0) {
                    double old_mean = running_sum / running_count;
                    int new_count = running_count + buckets[i].count;
                    window_var += buckets[i].variance
                        + (double)running_count * buckets[i].count / new_count
                        * (old_mean - bucket_mean) * (old_mean - bucket_mean);
                } else {
                    window_var = buckets[i].variance;
                }
                running_sum += buckets[i].total;
                running_count += buckets[i].count;
            }

            buckets.pop_front();
            drift = true;
        } else {
            break;
        }
    }
}

bool QoreADWIN::testCut(int n0, double mean0, double var0,
    int n1, double mean1, double var1) const {
    // ADWIN cut test: |mean0 - mean1| >= epsilon_cut
    // epsilon_cut = sqrt((2/m) * variance * ln(2/delta')) + (2/(3m)) * ln(2/delta')
    // where m = 1/(1/n0 + 1/n1), delta' = delta / ln(n)
    int n = n0 + n1;
    if (n < 2) {
        return false;
    }

    double m = 1.0 / (1.0 / n0 + 1.0 / n1);
    double delta_prime = delta / std::log((double)n);
    if (delta_prime <= 0 || delta_prime >= 1.0) {
        return false;
    }

    double variance = (var0 + var1) / n;
    if (variance < 0) { variance = 0; }

    double ln_term = std::log(2.0 / delta_prime);
    double epsilon = std::sqrt(2.0 * variance * ln_term / m)
        + 2.0 * ln_term / (3.0 * m);

    return std::abs(mean0 - mean1) >= epsilon;
}

double QoreADWIN::getMean() const {
    std::lock_guard<std::mutex> lk(mtx);
    if (window_count == 0) {
        return 0.0;
    }
    return window_sum / window_count;
}

int QoreADWIN::getWidth() const {
    std::lock_guard<std::mutex> lk(mtx);
    return window_count;
}

void QoreADWIN::reset() {
    std::lock_guard<std::mutex> lk(mtx);
    buckets.clear();
    window_sum = 0;
    window_var = 0;
    window_count = 0;
    drift = false;
    total = 0;
}

// --- Serialization ---

std::vector<uint8_t> QoreADWIN::serializeState() const {
    std::vector<uint8_t> buf;
    MLSerialization::writeScalar(buf, delta);
    MLSerialization::writeBool(buf, drift);
    MLSerialization::writeScalar(buf, (double)total);
    MLSerialization::writeScalar(buf, window_sum);
    MLSerialization::writeScalar(buf, window_var);
    MLSerialization::writeInt32(buf, window_count);
    MLSerialization::writeInt32(buf, (int32_t)buckets.size());
    for (const auto& b : buckets) {
        MLSerialization::writeScalar(buf, b.total);
        MLSerialization::writeScalar(buf, b.variance);
        MLSerialization::writeInt32(buf, b.count);
    }
    return buf;
}

QoreADWIN* QoreADWIN::deserializeState(const uint8_t* data, size_t len,
    ExceptionSink* xsink) {
    const uint8_t* ptr = data;
    size_t remaining = len;

    double d = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    bool dr = MLSerialization::readBool(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double tot = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double ws = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double wv = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int wc = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int nb = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    std::deque<Bucket> bkts;
    for (int i = 0; i < nb; ++i) {
        double bt = MLSerialization::readScalar(ptr, remaining, xsink);
        if (*xsink) { return nullptr; }
        double bv = MLSerialization::readScalar(ptr, remaining, xsink);
        if (*xsink) { return nullptr; }
        int bc = MLSerialization::readInt32(ptr, remaining, xsink);
        if (*xsink) { return nullptr; }
        bkts.push_back(Bucket{bt, bv, bc});
    }

    auto* adwin = new QoreADWIN(d);
    adwin->drift = dr;
    adwin->total = (int64_t)tot;
    adwin->window_sum = ws;
    adwin->window_var = wv;
    adwin->window_count = wc;
    adwin->buckets = std::move(bkts);
    return adwin;
}
