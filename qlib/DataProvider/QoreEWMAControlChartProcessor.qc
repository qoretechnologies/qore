# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreEWMAControlChartProcessor class definition

/** QoreEWMAControlChartProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DataProvider module
public namespace DataProvider {
#! Pipeline processor that implements EWMA statistical process control charts
/** @since %DataProvider 3.3
*/
public class QoreEWMAControlChartProcessor inherits AbstractAnalyticsProcessor {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreEWMAControlChartProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_ANALYTICS,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = AbstractAnalyticsProcessor::BaseConstructorOptions + {
            "lambda": <DataProviderOptionInfo>{
                "display_name": "Smoothing Factor",
                "short_desc": "EWMA smoothing parameter (0-1)",
                "desc": "How quickly the EWMA reacts to changes; higher values "
                    "(closer to 1.0) react faster but are more sensitive to noise; "
                    "lower values produce smoother trends; must be between 0 (exclusive) "
                    "and 1 (inclusive); default: 0.2",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "sigma_multiplier": <DataProviderOptionInfo>{
                "display_name": "Control Limit Width",
                "short_desc": "Number of standard deviations for control limits",
                "desc": "How wide the control limits are as a multiple of the standard "
                    "deviation; larger values mean fewer false alarms but slower detection "
                    "of real shifts; typical values: 2.0 (sensitive), 3.0 (standard); "
                    "default: 3.0",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "target": <DataProviderOptionInfo>{
                "display_name": "Target Value",
                "short_desc": "Center line target value (omit for auto from window mean)",
                "desc": "The expected center/target value for the process; when not set, "
                    "the mean of the sliding window history is used automatically; "
                    "set to any value (including 0) to use a fixed center line; "
                    "default: auto (use window mean)",
                "type": AbstractDataProviderTypeMap."*float",
            },
        };
    }

    private {
        #! EWMA lambda parameter
        float lambda_val = 0.2;

        #! Sigma multiplier for control limits
        float sigma_mult = 3.0;

        #! Target value (NOTHING = auto from window mean)
        *float target_val;

        #! Per-field EWMA state
        hash<string, float> ewma_state;

        #! Per-field observation count (for time-varying control limits)
        hash<string, int> obs_count;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        if (exists options.lambda) {
            float l = options.lambda.toFloat();
            if (l <= 0.0 || l > 1.0) {
                throw "CONSTRUCTOR-ERROR",
                    sprintf("lambda must be in (0, 1]; got %g", l);
            }
            lambda_val = l;
        }
        if (exists options.sigma_multiplier) {
            float s = options.sigma_multiplier.toFloat();
            if (s <= 0.0) {
                throw "CONSTRUCTOR-ERROR",
                    sprintf("sigma_multiplier must be > 0; got %g", s);
            }
            sigma_mult = s;
        }
        if (exists options.target) {
            float tv = options.target.toFloat();
            target_val = tv;
        }
        initAnalytics(options, ConstructorOptions);
    }

    #! Returns the name of this data provider
    string getName() {
        return "ewma-control-chart";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Monitors process stability using an Exponentially Weighted Moving Average (EWMA) "
            "control chart; detects when values drift outside the normal operating range by "
            "comparing the EWMA statistic against upper and lower control limits";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the analytics mode
    int getAnalyticsMode() {
        return ANALYTICS_MODE_RECORD;
    }

    #! Computes EWMA for flush
    hash<auto> analyzeWindow(string field, list<float> values, int total_count) {
        # during flush, compute EWMA for the last value
        float value = values[values.lsize() - 1];
        list<float> history;
        for (int i = 0; i < values.lsize() - 1; ++i) {
            push history, values[i];
        }
        return analyzeRecord(field, value, history, total_count);
    }

    #! Analyzes a single record for EWMA control chart
    hash<auto> analyzeRecord(string field, float value, list<float> history, int total_count) {
        # initialize EWMA state if needed
        if (!ewma_state.hasKey(field)) {
            ewma_state{field} = value;
            obs_count{field} = 0;
        }

        # compute baseline statistics from history (excluding current value)
        float center = target_val ?? 0.0;
        float sigma = 0.0;
        int n = history.lsize();

        if (n > 0) {
            float sum = 0.0;
            foreach float v in (history) {
                sum += v;
            }
            float mean = sum / n;
            if (!exists target_val) {
                center = mean;
            }
            if (n > 1) {
                float sum_sq = 0.0;
                foreach float v in (history) {
                    float diff = v - mean;
                    sum_sq += diff * diff;
                }
                sigma = sqrt(sum_sq / (n - 1));
            }
        } else {
            if (!exists target_val) {
                center = value;
            }
        }

        # update EWMA: Z[t] = lambda * X[t] + (1 - lambda) * Z[t-1]
        float ewma = lambda_val * value + (1.0 - lambda_val) * ewma_state{field};
        ewma_state{field} = ewma;
        obs_count{field}++;
        int t = obs_count{field};

        # compute control limits (time-varying)
        # UCL/LCL = center +/- L * sigma * sqrt(lambda/(2-lambda) * (1 - (1-lambda)^(2t)))
        float factor = 0.0;
        if (sigma > 0.0) {
            float ewma_var = (lambda_val / (2.0 - lambda_val))
                * (1.0 - pow(1.0 - lambda_val, 2.0 * t));
            factor = sigma_mult * sigma * sqrt(ewma_var);
        }
        float ucl = center + factor;
        float lcl = center - factor;

        # determine signal
        bool out_of_control = ewma > ucl || ewma < lcl;
        string signal;
        if (ewma > ucl) {
            signal = "upper_violation";
        } else if (ewma < lcl) {
            signal = "lower_violation";
        } else {
            signal = "normal";
        }

        return <EWMAControlChartEventInfo>{
            "event_type": "ewma-control-chart",
            "field": field,
            "window_size": window_size,
            "window_count": window_counts{field},
            "record_value": value,
            "ewma": ewma,
            "ucl": ucl,
            "lcl": lcl,
            "center": center,
            "out_of_control": out_of_control,
            "signal": signal,
        };
    }

    #! Returns the output type for analytics events
    AbstractDataProviderType getAnalyticsOutputType() {
        return new HashDeclDataType(TypedHash::forName("DataProvider::EWMAControlChartEventInfo"));
    }
}
}
