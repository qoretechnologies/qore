# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreRunningStatsProcessor class definition

/** QoreRunningStatsProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that computes running statistics (count, sum, avg, min, max, stddev, variance) per window
/** @since %DataProvider 3.4
*/
public class QoreRunningStatsProcessor inherits AbstractAnalyticsProcessor {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreRunningStatsProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_ANALYTICS,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = AbstractAnalyticsProcessor::BaseConstructorOptions;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        initAnalytics(options, ConstructorOptions);
    }

    #! Returns the name of this data provider
    string getName() {
        return "running-stats";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Computes running statistics (count, sum, avg, min, max, stddev, variance) over a "
            "window of records for each configured numeric field";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the analytics mode
    int getAnalyticsMode() {
        return ANALYTICS_MODE_WINDOW;
    }

    #! Analyzes a complete window of values
    hash<auto> analyzeWindow(string field, list<float> values, int total_count) {
        int n = values.lsize();
        float sum = 0.0;
        float min_val = values[0];
        float max_val = values[0];
        foreach float v in (values) {
            sum += v;
            if (v < min_val) {
                min_val = v;
            }
            if (v > max_val) {
                max_val = v;
            }
        }
        float avg = sum / n;

        # sample variance (N-1 denominator)
        float variance = 0.0;
        if (n > 1) {
            float sum_sq_diff = 0.0;
            foreach float v in (values) {
                float diff = v - avg;
                sum_sq_diff += diff * diff;
            }
            variance = sum_sq_diff / (n - 1);
        }
        float stddev = sqrt(variance);

        return <StatisticsEventInfo>{
            "event_type": "statistics",
            "field": field,
            "window_size": window_size,
            "window_count": window_counts{field},
            "count": n,
            "sum": sum,
            "avg": avg,
            "min_val": min_val,
            "max_val": max_val,
            "stddev": stddev,
            "variance": variance,
        };
    }

    #! Not used for window-level processors
    hash<auto> analyzeRecord(string field, float value, list<float> window_values, int total_count) {
        throw "INTERNAL-ERROR", "analyzeRecord() should not be called for a window-level processor";
    }

    #! Returns the output type for analytics events
    AbstractDataProviderType getAnalyticsOutputType() {
        return new HashDeclDataType(TypedHash::forName("DataProvider::StatisticsEventInfo"));
    }
}
}
