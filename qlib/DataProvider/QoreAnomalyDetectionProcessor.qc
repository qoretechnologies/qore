# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreAnomalyDetectionProcessor class definition

/** QoreAnomalyDetectionProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that performs z-score based anomaly detection per record
/** @since %DataProvider 3.3
*/
public class QoreAnomalyDetectionProcessor inherits AbstractAnalyticsProcessor {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreAnomalyDetectionProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_ANALYTICS,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = AbstractAnalyticsProcessor::BaseConstructorOptions + {
            "threshold": <DataProviderOptionInfo>{
                "display_name": "Anomaly Threshold",
                "short_desc": "Z-score threshold for flagging anomalies",
                "desc": "The z-score threshold above which a value is flagged as anomalous; "
                    "lower values increase sensitivity (more anomalies detected); "
                    "must be greater than `0`; default: `3.0`",
                "type": AbstractDataProviderTypeMap."*float",
            },
        };
    }

    private {
        #! Z-score threshold for anomaly detection
        float threshold = 3.0;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        if (exists options.threshold) {
            float t = options.threshold.toFloat();
            if (t <= 0.0) {
                throw "CONSTRUCTOR-ERROR",
                    sprintf("threshold must be greater than 0; got %g", t);
            }
            threshold = t;
        }
        initAnalytics(options, ConstructorOptions);
    }

    #! Returns the name of this data provider
    string getName() {
        return "anomaly-detection";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Performs z-score based anomaly detection for each configured numeric field, "
            "emitting one event per record with anomaly status";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the analytics mode
    int getAnalyticsMode() {
        return ANALYTICS_MODE_RECORD;
    }

    #! Computes summary statistics during flush
    hash<auto> analyzeWindow(string field, list<float> values, int total_count) {
        # During flush, compute final z-score for the last value in the buffer
        int n = values.lsize();
        float sum = 0.0;
        foreach float v in (values) {
            sum += v;
        }
        float mean = sum / n;

        float stddev_val = 0.0;
        if (n > 1) {
            float sum_sq_diff = 0.0;
            foreach float v in (values) {
                float diff = v - mean;
                sum_sq_diff += diff * diff;
            }
            stddev_val = sqrt(sum_sq_diff / (n - 1));
        }

        float last_value = values[n - 1];
        float z_score = 0.0;
        if (stddev_val > 0.0) {
            z_score = abs(last_value - mean) / stddev_val;
        }

        return <AnomalyEventInfo>{
            "event_type": "anomaly",
            "field": field,
            "window_size": window_size,
            "window_count": window_counts{field},
            "z_score": z_score,
            "threshold": threshold,
            "is_anomaly": z_score > threshold,
            "record_value": last_value,
            "mean": mean,
            "stddev": stddev_val,
        };
    }

    #! Analyzes a single record value for anomaly detection against historical window
    hash<auto> analyzeRecord(string field, float value, list<float> history, int total_count) {
        int n = history.lsize();

        # no history: cannot compute statistics, report no anomaly
        if (n == 0) {
            return <AnomalyEventInfo>{
                "event_type": "anomaly",
                "field": field,
                "window_size": window_size,
                "window_count": window_counts{field},
                "z_score": 0.0,
                "threshold": threshold,
                "is_anomaly": False,
                "record_value": value,
                "mean": value,
                "stddev": 0.0,
            };
        }

        float sum = 0.0;
        foreach float v in (history) {
            sum += v;
        }
        float mean = sum / n;

        # sample standard deviation of historical window
        float stddev_val = 0.0;
        if (n > 1) {
            float sum_sq_diff = 0.0;
            foreach float v in (history) {
                float diff = v - mean;
                sum_sq_diff += diff * diff;
            }
            stddev_val = sqrt(sum_sq_diff / (n - 1));
        }

        float z_score = 0.0;
        if (stddev_val > 0.0) {
            z_score = abs(value - mean) / stddev_val;
        }

        return <AnomalyEventInfo>{
            "event_type": "anomaly",
            "field": field,
            "window_size": window_size,
            "window_count": window_counts{field},
            "z_score": z_score,
            "threshold": threshold,
            "is_anomaly": z_score > threshold,
            "record_value": value,
            "mean": mean,
            "stddev": stddev_val,
        };
    }

    #! Returns the output type for analytics events
    AbstractDataProviderType getAnalyticsOutputType() {
        return new HashDeclDataType(TypedHash::forName("DataProvider::AnomalyEventInfo"));
    }
}
}
