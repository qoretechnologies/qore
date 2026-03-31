/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_ADWIN.h

    ADWIN (Adaptive Windowing) drift detector class declaration

    Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License
*/

#ifndef _QORE_MODULE_ML_QC_ADWIN_H
#define _QORE_MODULE_ML_QC_ADWIN_H

#include "ml_common.h"

#include <deque>
#include <mutex>
#include <vector>

DLLEXPORT extern qore_classid_t CID_ADWIN;
DLLLOCAL extern QoreClass* QC_ADWIN;

DLLLOCAL void preinitADWINClass();
DLLLOCAL QoreClass* initADWINClass(QoreNamespace& ns);

//! ADWIN adaptive windowing drift detector
/** Maintains a variable-length window of observations. When the means of
    two sub-windows differ significantly (based on delta), the older data
    is dropped and drift is flagged.

    Based on: Bifet & Gavalda, "Learning from Time-Changing Data with
    Adaptive Windowing" (SDM 2007).
*/
class QoreADWIN : public AbstractPrivateData {
public:
    DLLLOCAL QoreADWIN(double delta);

    //! Add a new observation; returns true if drift was detected
    DLLLOCAL bool add(double value);

    //! Check if drift was detected on the last add()
    DLLLOCAL bool driftDetected() const { return drift; }

    //! Current window mean
    DLLLOCAL double getMean() const;

    //! Current window size
    DLLLOCAL int getWidth() const;

    //! Total number of observations added
    DLLLOCAL int64_t getTotal() const { return total; }

    //! Reset to initial state
    DLLLOCAL void reset();

    DLLLOCAL std::vector<uint8_t> serializeState() const;
    DLLLOCAL static QoreADWIN* deserializeState(const uint8_t* data,
        size_t len, ExceptionSink* xsink);

private:
    //! Compressed bucket for efficient storage
    struct Bucket {
        double total;      // sum of values in this bucket
        double variance;   // variance within this bucket
        int count;         // number of merged values
    };

    //! Try to shrink the window by removing old buckets
    DLLLOCAL void shrinkWindow();

    //! Test whether cutting at a given point detects drift
    DLLLOCAL bool testCut(int n0, double mean0, double var0,
        int n1, double mean1, double var1) const;

    double delta;
    bool drift = false;
    int64_t total = 0;
    double window_sum = 0;
    double window_var = 0;
    int window_count = 0;
    std::deque<Bucket> buckets;
    mutable std::mutex mtx;
};

#endif
