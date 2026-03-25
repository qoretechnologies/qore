/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_DDM.h

    DDM (Drift Detection Method) class declaration

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_MODULE_ML_QC_DDM_H
#define _QORE_MODULE_ML_QC_DDM_H

#include "ml_common.h"

#include <mutex>
#include <vector>

DLLEXPORT extern qore_classid_t CID_DDM;
DLLLOCAL extern QoreClass* QC_DDM;

DLLLOCAL void preinitDDMClass();
DLLLOCAL QoreClass* initDDMClass(QoreNamespace& ns);

//! DDM (Drift Detection Method) drift detector
/** Monitors the error rate of a classification model. Assumes the error
    rate follows a binomial distribution. When the error rate increases
    significantly beyond its minimum observed value, drift is signaled.

    Based on: Gama, Medas, Castillo, Rodrigues, "Learning with Drift
    Detection" (SBIA 2004).
*/
class QoreDDM : public AbstractPrivateData {
public:
    DLLLOCAL QoreDDM(int min_instances, double warning_threshold, double drift_threshold);

    //! Add a new prediction result; returns true if drift was detected
    /** @param value 0.0 for correct prediction, 1.0 for error
    */
    DLLLOCAL bool add(double value);

    //! Check if drift was detected on the last add()
    DLLLOCAL bool driftDetected() const { return drift; }

    //! Check if warning was signaled on the last add()
    DLLLOCAL bool warningDetected() const { return warning; }

    //! Current error rate
    DLLLOCAL double getErrorRate() const;

    //! Total number of observations added
    DLLLOCAL int64_t getTotal() const { return n; }

    //! Reset to initial state
    DLLLOCAL void reset();

    DLLLOCAL std::vector<uint8_t> serializeState() const;
    DLLLOCAL static QoreDDM* deserializeState(const uint8_t* data,
        size_t len, ExceptionSink* xsink);

private:
    int min_instances;
    double warning_threshold;
    double drift_threshold;
    int64_t n = 0;
    int64_t error_count = 0;
    double p_min = std::numeric_limits<double>::max();
    double s_min = std::numeric_limits<double>::max();
    double ps_min = std::numeric_limits<double>::max();
    bool drift = false;
    bool warning = false;
    mutable std::mutex mtx;
};

#endif
