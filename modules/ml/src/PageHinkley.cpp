/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    PageHinkley.cpp

    Page-Hinkley drift detector implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "QC_PageHinkley.h"
#include "ml_serialization.h"

#include <algorithm>

QorePageHinkley::QorePageHinkley(double delta, double lambda, int min_instances)
    : delta(delta), lambda(lambda), min_instances(min_instances) {
}

bool QorePageHinkley::add(double value) {
    std::lock_guard<std::mutex> lk(mtx);

    ++n_observations;

    // Update running mean incrementally
    running_mean += (value - running_mean) / n_observations;

    // Update cumulative sum
    cumulative_sum += (value - running_mean - delta);

    // Update minimum cumulative sum
    min_sum = std::min(min_sum, cumulative_sum);

    // Test for drift only after min_instances observations
    drift = false;
    if (n_observations >= min_instances) {
        if ((cumulative_sum - min_sum) > lambda) {
            drift = true;
        }
    }

    return drift;
}

double QorePageHinkley::getMean() const {
    std::lock_guard<std::mutex> lk(mtx);
    return running_mean;
}

void QorePageHinkley::reset() {
    std::lock_guard<std::mutex> lk(mtx);
    running_mean = 0.0;
    cumulative_sum = 0.0;
    min_sum = 0.0;
    n_observations = 0;
    drift = false;
}

// --- Serialization ---

std::vector<uint8_t> QorePageHinkley::serializeState() const {
    std::vector<uint8_t> buf;
    MLSerialization::writeScalar(buf, delta);
    MLSerialization::writeScalar(buf, lambda);
    MLSerialization::writeInt32(buf, min_instances);
    MLSerialization::writeScalar(buf, running_mean);
    MLSerialization::writeScalar(buf, cumulative_sum);
    MLSerialization::writeScalar(buf, min_sum);
    MLSerialization::writeScalar(buf, (double)n_observations);
    MLSerialization::writeBool(buf, drift);
    return buf;
}

QorePageHinkley* QorePageHinkley::deserializeState(const uint8_t* data, size_t len,
    ExceptionSink* xsink) {
    const uint8_t* ptr = data;
    size_t remaining = len;

    double d = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double l = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    int mi = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double rm = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double cs = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double ms = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double no = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    bool dr = MLSerialization::readBool(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }

    auto* ph = new QorePageHinkley(d, l, mi);
    ph->running_mean = rm;
    ph->cumulative_sum = cs;
    ph->min_sum = ms;
    ph->n_observations = (int64_t)no;
    ph->drift = dr;
    return ph;
}
