/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    DDM.cpp

    DDM (Drift Detection Method) drift detector implementation

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#include "QC_DDM.h"
#include "ml_serialization.h"

#include <cmath>

QoreDDM::QoreDDM(int min_instances, double warning_threshold, double drift_threshold)
    : min_instances(min_instances), warning_threshold(warning_threshold),
      drift_threshold(drift_threshold) {
}

bool QoreDDM::add(double value) {
    std::lock_guard<std::mutex> lk(mtx);

    ++n;
    error_count += (int64_t)(value != 0.0 ? 1 : 0);

    // Compute current error rate and standard deviation
    double p = (double)error_count / n;
    double s = std::sqrt(p * (1.0 - p) / n);
    double ps = p + s;

    // Reset drift/warning flags
    drift = false;
    warning = false;

    // Update minimums
    if (ps < ps_min) {
        p_min = p;
        s_min = s;
        ps_min = ps;
    }

    // Only test after minimum number of instances
    // Use strict inequality (>) to avoid false positives when ps == ps_min
    // (e.g., when all predictions are correct, ps == ps_min == 0)
    if (n >= min_instances && ps > ps_min) {
        if (s_min > 0) {
            // Standard DDM: compare against threshold in terms of s_min
            if (ps >= ps_min + drift_threshold * s_min) {
                drift = true;
            } else if (ps >= ps_min + warning_threshold * s_min) {
                warning = true;
            }
        } else {
            // s_min == 0 means baseline had perfect accuracy (p=0) or
            // all errors (p=1); any increase in ps is drift
            drift = true;
        }
    }

    return drift;
}

double QoreDDM::getErrorRate() const {
    std::lock_guard<std::mutex> lk(mtx);
    if (n == 0) {
        return 0.0;
    }
    return (double)error_count / n;
}

void QoreDDM::reset() {
    std::lock_guard<std::mutex> lk(mtx);
    n = 0;
    error_count = 0;
    p_min = std::numeric_limits<double>::max();
    s_min = std::numeric_limits<double>::max();
    ps_min = std::numeric_limits<double>::max();
    drift = false;
    warning = false;
}

// --- Serialization ---

std::vector<uint8_t> QoreDDM::serializeState() const {
    std::vector<uint8_t> buf;
    MLSerialization::writeInt32(buf, min_instances);
    MLSerialization::writeScalar(buf, warning_threshold);
    MLSerialization::writeScalar(buf, drift_threshold);
    MLSerialization::writeScalar(buf, (double)n);
    MLSerialization::writeScalar(buf, (double)error_count);
    MLSerialization::writeScalar(buf, p_min);
    MLSerialization::writeScalar(buf, s_min);
    MLSerialization::writeScalar(buf, ps_min);
    MLSerialization::writeBool(buf, drift);
    MLSerialization::writeBool(buf, warning);
    return buf;
}

QoreDDM* QoreDDM::deserializeState(const uint8_t* data, size_t len,
    ExceptionSink* xsink) {
    const uint8_t* ptr = data;
    size_t remaining = len;

    int mi = MLSerialization::readInt32(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double wt = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double dt = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double n_val = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double ec_val = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double pm = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double sm = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    double psm = MLSerialization::readScalar(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    bool dr = MLSerialization::readBool(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }
    bool wr = MLSerialization::readBool(ptr, remaining, xsink);
    if (*xsink) { return nullptr; }

    auto* ddm = new QoreDDM(mi, wt, dt);
    ddm->n = (int64_t)n_val;
    ddm->error_count = (int64_t)ec_val;
    ddm->p_min = pm;
    ddm->s_min = sm;
    ddm->ps_min = psm;
    ddm->drift = dr;
    ddm->warning = wr;
    return ddm;
}
