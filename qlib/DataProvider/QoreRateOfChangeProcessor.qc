# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreRateOfChangeProcessor class definition

/** QoreRateOfChangeProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that computes rate of change (first and second derivatives) per record
/** @since %DataProvider 3.3
*/
public class QoreRateOfChangeProcessor inherits AbstractAnalyticsProcessor {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreRateOfChangeProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_ANALYTICS,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = AbstractAnalyticsProcessor::BaseConstructorOptions + {
            "smoothing": <DataProviderOptionInfo>{
                "display_name": "Apply Smoothing",
                "short_desc": "Apply simple moving average smoothing before differentiation",
                "desc": "When enabled, applies a simple moving average over the window "
                    "before computing derivatives, reducing noise in the rate of change; "
                    "disable for raw point-to-point rates; default: `True`",
                "type": AbstractDataProviderTypeMap."*bool",
                "example_value": True,
            },
        };
    }

    private {
        #! Whether to apply SMA smoothing
        bool smoothing = True;

        #! Per-field previous rate of change for acceleration
        hash<string, float> prev_rates;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        if (exists options.smoothing) {
            smoothing = boolean(options.smoothing);
        }
        initAnalytics(options, ConstructorOptions);
    }

    #! Returns the name of this data provider
    string getName() {
        return "rate-of-change";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Computes the `rate_of_change` (first derivative), `acceleration` (second "
            "derivative), and `percent_change` for each configured numeric field per record";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the analytics mode
    int getAnalyticsMode() {
        return ANALYTICS_MODE_RECORD;
    }

    #! Computes rate of change for flush
    hash<auto> analyzeWindow(string field, list<float> values, int total_count) {
        # during flush, compute final rate for the last value
        int n = values.lsize();
        float value = values[n - 1];
        float rate = 0.0;
        float accel = 0.0;
        float pct = 0.0;

        if (n > 1) {
            float prev;
            if (smoothing && n >= 2) {
                # compute SMA of the available values
                float sum = 0.0;
                foreach float v in (values) {
                    sum += v;
                }
                prev = (sum - value) / (n - 1);
            } else {
                prev = values[n - 2];
            }
            rate = value - prev;
            if (prev != 0.0) {
                pct = (rate / abs(prev)) * 100.0;
            }
            if (prev_rates.hasKey(field)) {
                accel = rate - prev_rates{field};
            }
        }

        return <RateOfChangeEventInfo>{
            "event_type": "rate-of-change",
            "field": field,
            "window_size": window_size,
            "window_count": window_counts{field},
            "record_value": value,
            "rate_of_change": rate,
            "acceleration": accel,
            "percent_change": pct,
        };
    }

    #! Analyzes a single record value for rate of change
    hash<auto> analyzeRecord(string field, float value, list<float> history, int total_count) {
        float rate = 0.0;
        float accel = 0.0;
        float pct = 0.0;

        if (history.lsize() > 0) {
            float prev;
            if (smoothing && history.lsize() > 1) {
                # SMA of history
                float sum = 0.0;
                foreach float v in (history) {
                    sum += v;
                }
                prev = sum / history.lsize();
            } else {
                prev = history[history.lsize() - 1];
            }
            rate = value - prev;
            if (prev != 0.0) {
                pct = (rate / abs(prev)) * 100.0;
            }
            if (prev_rates.hasKey(field)) {
                accel = rate - prev_rates{field};
            }
        }

        prev_rates{field} = rate;

        return <RateOfChangeEventInfo>{
            "event_type": "rate-of-change",
            "field": field,
            "window_size": window_size,
            "window_count": window_counts{field},
            "record_value": value,
            "rate_of_change": rate,
            "acceleration": accel,
            "percent_change": pct,
        };
    }

    #! Returns the output type for analytics events
    AbstractDataProviderType getAnalyticsOutputType() {
        return new HashDeclDataType(TypedHash::forName("DataProvider::RateOfChangeEventInfo"));
    }
}
}
