/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_PageHinkley.h

    Page-Hinkley drift detector class declaration

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_MODULE_ML_QC_PAGEHINKLEY_H
#define _QORE_MODULE_ML_QC_PAGEHINKLEY_H

#include "ml_common.h"

#include <mutex>
#include <vector>

DLLEXPORT extern qore_classid_t CID_PAGEHINKLEY;
DLLLOCAL extern QoreClass* QC_PAGEHINKLEY;

DLLLOCAL void preinitPageHinkleyClass();
DLLLOCAL QoreClass* initPageHinkleyClass(QoreNamespace& ns);

//! Page-Hinkley drift detector
/** Maintains a cumulative sum that tracks deviations from the running mean.
    When the cumulative deviation exceeds a threshold (lambda), drift is
    signaled.

    Based on: Page, E.S., "Continuous Inspection Schemes" (Biometrika, 1954).
*/
class QorePageHinkley : public AbstractPrivateData {
public:
    DLLLOCAL QorePageHinkley(double delta, double lambda, int min_instances);

    //! Add a new observation; returns true if drift was detected
    DLLLOCAL bool add(double value);

    //! Check if drift was detected on the last add()
    DLLLOCAL bool driftDetected() const { return drift; }

    //! Current running mean
    DLLLOCAL double getMean() const;

    //! Total number of observations added
    DLLLOCAL int64_t getTotal() const { return n_observations; }

    //! Reset to initial state
    DLLLOCAL void reset();

    DLLLOCAL std::vector<uint8_t> serializeState() const;
    DLLLOCAL static QorePageHinkley* deserializeState(const uint8_t* data,
        size_t len, ExceptionSink* xsink);

private:
    double delta;
    double lambda;
    int min_instances;
    double running_mean = 0.0;
    double cumulative_sum = 0.0;
    double min_sum = 0.0;
    int64_t n_observations = 0;
    bool drift = false;
    mutable std::mutex mtx;
};

#endif
