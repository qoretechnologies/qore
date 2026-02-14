/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SeasonalDecomposition.cpp

    Qore ml module - SeasonalDecomposition class implementation

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

#include "QC_SeasonalDecomposition.h"

// Extern declaration for hashdecl (defined in ml-module.cpp)
extern const TypedHashDecl* hashdeclSeasonalDecompositionResult;

QoreListNode* QoreSeasonalDecomposition::decompose(const VectorXd& series, ExceptionSink* xsink) {
    int n = static_cast<int>(series.size());
    if (n < 2 * period) {
        xsink->raiseException("ML-SEASONAL-DECOMPOSITION-ERROR",
            "not enough data: received %d values but at least %d are needed "
            "(2 x seasonal cycle length of %d); increase the data size or "
            "reduce the seasonal cycle length", n, 2 * period, period);
        return nullptr;
    }

    bool additive = (type == "additive");

    // Step 1: Compute centered moving average for trend extraction
    // For even period: use 2x moving average (period + 1 weights with half-weight endpoints)
    std::vector<double> trend(n, std::numeric_limits<double>::quiet_NaN());

    int half = period / 2;
    bool even_period = (period % 2 == 0);

    if (even_period) {
        // Centered moving average for even period: average of two period-length moving averages
        // Result: (0.5*x[i-half] + x[i-half+1] + ... + x[i+half-1] + 0.5*x[i+half]) / period
        for (int i = half; i < n - half; ++i) {
            double sum = 0.5 * series(i - half) + 0.5 * series(i + half);
            for (int j = i - half + 1; j < i + half; ++j) {
                sum += series(j);
            }
            trend[i] = sum / period;
        }
    } else {
        // Centered moving average for odd period
        for (int i = half; i < n - half; ++i) {
            double sum = 0.0;
            for (int j = i - half; j <= i + half; ++j) {
                sum += series(j);
            }
            trend[i] = sum / period;
        }
    }

    // Step 2: Compute detrended series
    std::vector<double> detrended(n, std::numeric_limits<double>::quiet_NaN());
    for (int i = 0; i < n; ++i) {
        if (!std::isnan(trend[i])) {
            if (additive) {
                detrended[i] = series(i) - trend[i];
            } else {
                if (trend[i] != 0.0) {
                    detrended[i] = series(i) / trend[i];
                }
            }
        }
    }

    // Step 3: Average seasonal factors across cycles
    std::vector<double> seasonal_avg(period, 0.0);
    std::vector<int> seasonal_count(period, 0);
    for (int i = 0; i < n; ++i) {
        if (!std::isnan(detrended[i])) {
            seasonal_avg[i % period] += detrended[i];
            seasonal_count[i % period]++;
        }
    }
    for (int j = 0; j < period; ++j) {
        if (seasonal_count[j] > 0) {
            seasonal_avg[j] /= seasonal_count[j];
        }
    }

    // Step 4: Normalize seasonal factors
    if (additive) {
        // Normalize to sum = 0
        double mean = 0.0;
        for (int j = 0; j < period; ++j) {
            mean += seasonal_avg[j];
        }
        mean /= period;
        for (int j = 0; j < period; ++j) {
            seasonal_avg[j] -= mean;
        }
    } else {
        // Normalize so average = 1 (sum = period)
        double mean = 0.0;
        for (int j = 0; j < period; ++j) {
            mean += seasonal_avg[j];
        }
        mean /= period;
        if (mean != 0.0) {
            for (int j = 0; j < period; ++j) {
                seasonal_avg[j] /= mean;
            }
        }
    }

    // Step 5: Build result list
    ReferenceHolder<QoreListNode> rv(
        new QoreListNode(hashdeclSeasonalDecompositionResult->getTypeInfo()), xsink);

    for (int i = 0; i < n; ++i) {
        ReferenceHolder<QoreHashNode> h(
            new QoreHashNode(hashdeclSeasonalDecompositionResult, xsink), xsink);

        double s = seasonal_avg[i % period];
        h->setKeyValue("seasonal", s, xsink);
        h->setKeyValue("period", static_cast<int64>(period), xsink);

        if (std::isnan(trend[i])) {
            // Trend is NaN at edges - emit NOTHING
            h->setKeyValue("trend", QoreValue(), xsink);
            h->setKeyValue("residual", QoreValue(), xsink);
        } else {
            h->setKeyValue("trend", trend[i], xsink);

            // Compute residual
            double residual;
            if (additive) {
                residual = series(i) - trend[i] - s;
            } else {
                if (trend[i] != 0.0 && s != 0.0) {
                    residual = series(i) / (trend[i] * s);
                } else {
                    residual = std::numeric_limits<double>::quiet_NaN();
                }
            }

            if (std::isnan(residual)) {
                h->setKeyValue("residual", QoreValue(), xsink);
            } else {
                h->setKeyValue("residual", residual, xsink);
            }
        }

        rv->push(h.release(), xsink);
    }

    return rv.release();
}
