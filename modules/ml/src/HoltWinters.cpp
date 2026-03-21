/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    HoltWinters.cpp

    Qore ml module - HoltWinters class implementation

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

#include "QC_HoltWinters.h"

// Extern declaration for hashdecl (defined in ml-module.cpp)
extern const TypedHashDecl* hashdeclHoltWintersResult;

void QoreHoltWinters::fit(const VectorXd& series, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);
    int n = static_cast<int>(series.size());
    if (n < 2 * period) {
        xsink->raiseException("ML-HOLTWINTERS-ERROR",
            "not enough data: received %d values but at least %d are needed "
            "(2 x seasonal cycle length of %d); increase the data size or "
            "reduce the seasonal cycle length", n, 2 * period, period);
        return;
    }

    bool additive = (seasonal_type == "additive");

    // Initialize level as mean of first period
    double init_level = 0.0;
    for (int i = 0; i < period; ++i) {
        init_level += series(i);
    }
    init_level /= period;

    // Initialize trend from first two periods
    double init_trend = 0.0;
    for (int i = 0; i < period; ++i) {
        init_trend += (series(i + period) - series(i));
    }
    init_trend /= (period * period);

    // Initialize seasonal factors
    seasonal.resize(period);
    if (additive) {
        // Average deviations from level across first few cycles
        int n_cycles = n / period;
        if (n_cycles > 2) {
            n_cycles = 2;  // use first 2 cycles
        }
        std::vector<double> avg(period, 0.0);
        for (int c = 0; c < n_cycles; ++c) {
            for (int j = 0; j < period; ++j) {
                // Detrend: value - (level + trend * position)
                double expected = init_level + init_trend * (c * period + j);
                avg[j] += (series(c * period + j) - expected);
            }
        }
        double sum = 0.0;
        for (int j = 0; j < period; ++j) {
            avg[j] /= n_cycles;
            sum += avg[j];
        }
        // Normalize to sum to zero
        double correction = sum / period;
        for (int j = 0; j < period; ++j) {
            seasonal[j] = avg[j] - correction;
        }
    } else {
        // Multiplicative: ratio of value to detrended level
        int n_cycles = n / period;
        if (n_cycles > 2) {
            n_cycles = 2;
        }
        std::vector<double> avg(period, 0.0);
        for (int c = 0; c < n_cycles; ++c) {
            for (int j = 0; j < period; ++j) {
                double expected = init_level + init_trend * (c * period + j);
                if (expected != 0.0) {
                    avg[j] += series(c * period + j) / expected;
                } else {
                    avg[j] += 1.0;
                }
            }
        }
        double sum = 0.0;
        for (int j = 0; j < period; ++j) {
            avg[j] /= n_cycles;
            sum += avg[j];
        }
        // Normalize so average = 1
        double correction = sum / period;
        if (correction != 0.0) {
            for (int j = 0; j < period; ++j) {
                seasonal[j] = avg[j] / correction;
            }
        }
    }

    level = init_level;
    trend = init_trend;
    t = 0;

    // Run through the historical series to warm up the model
    for (int i = 0; i < n; ++i) {
        if (i % 100 == 0 && qore_check_cancel(xsink, "HoltWinters fit")) {
            return;
        }

        double value = series(i);
        int season_idx = i % period;
        double prev_level = level;
        double prev_trend = trend;

        if (additive) {
            level = alpha * (value - seasonal[season_idx])
                  + (1.0 - alpha) * (prev_level + phi * prev_trend);
            trend = beta * (level - prev_level)
                  + (1.0 - beta) * phi * prev_trend;
            seasonal[season_idx] = gamma * (value - level)
                                 + (1.0 - gamma) * seasonal[season_idx];
        } else {
            if (seasonal[season_idx] != 0.0) {
                level = alpha * (value / seasonal[season_idx])
                      + (1.0 - alpha) * (prev_level + phi * prev_trend);
            } else {
                level = (1.0 - alpha) * (prev_level + phi * prev_trend);
            }
            trend = beta * (level - prev_level)
                  + (1.0 - beta) * phi * prev_trend;
            if (level != 0.0) {
                seasonal[season_idx] = gamma * (value / level)
                                     + (1.0 - gamma) * seasonal[season_idx];
            }
        }
    }

    t = n;
    fitted = true;
}

QoreHashNode* QoreHoltWinters::update(double value, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-HOLTWINTERS-ERROR",
            "model has not been fitted: call fit() before update()");
        return nullptr;
    }

    bool additive = (seasonal_type == "additive");
    int season_idx = t % period;
    double prev_level = level;
    double prev_trend = trend;

    if (additive) {
        level = alpha * (value - seasonal[season_idx])
              + (1.0 - alpha) * (prev_level + phi * prev_trend);
        trend = beta * (level - prev_level)
              + (1.0 - beta) * phi * prev_trend;
        seasonal[season_idx] = gamma * (value - level)
                             + (1.0 - gamma) * seasonal[season_idx];
    } else {
        if (seasonal[season_idx] != 0.0) {
            level = alpha * (value / seasonal[season_idx])
                  + (1.0 - alpha) * (prev_level + phi * prev_trend);
        } else {
            level = (1.0 - alpha) * (prev_level + phi * prev_trend);
        }
        trend = beta * (level - prev_level)
              + (1.0 - beta) * phi * prev_trend;
        if (level != 0.0) {
            seasonal[season_idx] = gamma * (value / level)
                                 + (1.0 - gamma) * seasonal[season_idx];
        }
    }

    ++t;

    // Compute one-step-ahead forecast
    double fc;
    int next_season = t % period;
    if (additive) {
        fc = level + phi * trend + seasonal[next_season];
    } else {
        fc = (level + phi * trend) * seasonal[next_season];
    }

    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclHoltWintersResult, xsink), xsink);
    rv->setKeyValue("forecast", fc, xsink);
    rv->setKeyValue("level", level, xsink);
    rv->setKeyValue("trend", trend, xsink);
    rv->setKeyValue("seasonal", seasonal[season_idx], xsink);

    return rv.release();
}

QoreListNode* QoreHoltWinters::forecast(int steps, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-HOLTWINTERS-ERROR",
            "model has not been fitted: call fit() before forecast()");
        return nullptr;
    }
    if (steps < 1) {
        xsink->raiseException("ML-HOLTWINTERS-ERROR",
            "steps must be >= 1; got %d", steps);
        return nullptr;
    }

    bool additive = (seasonal_type == "additive");

    ReferenceHolder<QoreListNode> rv(new QoreListNode(floatTypeInfo), xsink);

    for (int h = 1; h <= steps; ++h) {
        int season_idx = (t + h - 1) % period;
        double damped_trend;
        if (damped) {
            // Sum of phi^1 + phi^2 + ... + phi^h
            double phi_sum = 0.0;
            double phi_power = phi;
            for (int j = 0; j < h; ++j) {
                phi_sum += phi_power;
                phi_power *= phi;
            }
            damped_trend = phi_sum * trend;
        } else {
            damped_trend = h * trend;
        }

        double fc;
        if (additive) {
            fc = level + damped_trend + seasonal[season_idx];
        } else {
            fc = (level + damped_trend) * seasonal[season_idx];
        }

        rv->push(fc, xsink);
    }

    return rv.release();
}

QoreValue QoreHoltWinters::getLevel(ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-HOLTWINTERS-ERROR",
            "model has not been fitted: call fit() first");
        return QoreValue();
    }
    return level;
}

QoreValue QoreHoltWinters::getTrend(ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-HOLTWINTERS-ERROR",
            "model has not been fitted: call fit() first");
        return QoreValue();
    }
    return trend;
}

QoreListNode* QoreHoltWinters::getSeasonal(ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lk(mtx);
    if (!fitted) {
        xsink->raiseException("ML-HOLTWINTERS-ERROR",
            "model has not been fitted: call fit() first");
        return nullptr;
    }
    ReferenceHolder<QoreListNode> rv(new QoreListNode(floatTypeInfo), xsink);
    for (size_t i = 0; i < seasonal.size(); ++i) {
        rv->push(seasonal[i], xsink);
    }
    return rv.release();
}
