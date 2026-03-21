/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_HoltWinters.h

    Qore ml module - HoltWinters class

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

#ifndef _QORE_MODULE_ML_QC_HOLTWINTERS_H
#define _QORE_MODULE_ML_QC_HOLTWINTERS_H

#include "ml_common.h"

#include <mutex>

DLLEXPORT extern qore_classid_t CID_HOLTWINTERS;
DLLLOCAL extern QoreClass* QC_HOLTWINTERS;

DLLLOCAL void preinitHoltWintersClass();
DLLLOCAL QoreClass* initHoltWintersClass(QoreNamespace& ns);

//! Holt-Winters triple exponential smoothing implementation class
class QoreHoltWinters : public AbstractPrivateData {
public:
    DLLLOCAL QoreHoltWinters(int period, double alpha, double beta, double gamma,
        const char* seasonal_type, bool damped, double phi)
        : period(period), alpha(alpha), beta(beta), gamma(gamma),
          seasonal_type(seasonal_type), damped(damped), phi(phi) {
    }

    DLLLOCAL int getPeriod() const { return period; }
    DLLLOCAL bool isFitted() const { return fitted; }

    //! Initialize model from historical data
    DLLLOCAL void fit(const VectorXd& series, ExceptionSink* xsink);

    //! Process one new observation
    DLLLOCAL QoreHashNode* update(double value, ExceptionSink* xsink);

    //! Forecast N steps ahead
    DLLLOCAL QoreListNode* forecast(int steps, ExceptionSink* xsink);

    //! Getters for model state (thread-safe; check fitted under lock)
    DLLLOCAL QoreValue getLevel(ExceptionSink* xsink);
    DLLLOCAL QoreValue getTrend(ExceptionSink* xsink);
    DLLLOCAL QoreListNode* getSeasonal(ExceptionSink* xsink);

    //! Serialize model state to binary
    DLLLOCAL std::vector<uint8_t> serializeState() const;

    //! Deserialize model state from binary
    DLLLOCAL static QoreHoltWinters* deserializeState(const uint8_t* data, size_t len,
        ExceptionSink* xsink);

private:
    int period;
    double alpha;
    double beta;
    double gamma;
    std::string seasonal_type;
    bool damped;
    double phi;  // damping factor

    // Model state
    double level = 0.0;
    double trend = 0.0;
    std::vector<double> seasonal;
    int t = 0;  // step counter
    bool fitted = false;

    mutable std::mutex mtx;
};

#endif // _QORE_MODULE_ML_QC_HOLTWINTERS_H
