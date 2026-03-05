# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreTrendAnalysisProcessor class definition

/** QoreTrendAnalysisProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that computes linear regression trend analysis per window
/** @since %DataProvider 3.3
*/
public class QoreTrendAnalysisProcessor inherits AbstractAnalyticsProcessor {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreTrendAnalysisProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_ANALYTICS,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = AbstractAnalyticsProcessor::BaseConstructorOptions + {
            "direction_threshold": <DataProviderOptionInfo>{
                "display_name": "Direction Threshold",
                "short_desc": "Minimum absolute slope to classify as rising or falling",
                "desc": "The minimum absolute slope value for the trend to be classified as "
                    "`rising` or `falling`; slopes with absolute value at or below this threshold "
                    "are classified as `flat`.\n\n"
                    "Use smaller values for data at large scales where meaningful trends have "
                    "small slopes; must be >= 0; default: `0.001`",
                "type": AbstractDataProviderTypeMap."*float",
            },
        };
    }

    private {
        #! Direction threshold for slope classification
        float direction_threshold = 0.001;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        if (exists options.direction_threshold) {
            float dt = options.direction_threshold.toFloat();
            if (dt < 0.0) {
                throw "CONSTRUCTOR-ERROR",
                    sprintf("direction_threshold must be >= 0; got %g", dt);
            }
            direction_threshold = dt;
        }
        initAnalytics(options, ConstructorOptions);
    }

    #! Returns the name of this data provider
    string getName() {
        return "trend-analysis";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Computes linear regression trend analysis (`slope`, `intercept`, `r_squared`, "
            "`direction`) over a window of records for each configured numeric field";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the analytics mode
    int getAnalyticsMode() {
        return ANALYTICS_MODE_WINDOW;
    }

    #! Analyzes a complete window of values using linear regression
    hash<auto> analyzeWindow(string field, list<float> values, int total_count) {
        int n = values.lsize();
        float slope;
        float intercept;
        float r_squared;
        string direction;

        if (n < 2) {
            # Single value: trivial case
            slope = 0.0;
            intercept = values[0];
            r_squared = 1.0;
            direction = "flat";
        } else {
            # Linear regression: x = 0..N-1, y = values
            float sum_x = 0.0;
            float sum_y = 0.0;
            float sum_xy = 0.0;
            float sum_x2 = 0.0;
            for (int i = 0; i < n; ++i) {
                float x = float(i);
                float y = values[i];
                sum_x += x;
                sum_y += y;
                sum_xy += x * y;
                sum_x2 += x * x;
            }
            float fn = float(n);
            float denom = fn * sum_x2 - sum_x * sum_x;
            slope = (fn * sum_xy - sum_x * sum_y) / denom;
            intercept = (sum_y - slope * sum_x) / fn;

            # R-squared
            float mean_y = sum_y / fn;
            float ss_tot = 0.0;
            float ss_res = 0.0;
            for (int i = 0; i < n; ++i) {
                float y_pred = intercept + slope * float(i);
                float diff_tot = values[i] - mean_y;
                float diff_res = values[i] - y_pred;
                ss_tot += diff_tot * diff_tot;
                ss_res += diff_res * diff_res;
            }
            if (ss_tot == 0.0) {
                r_squared = 1.0;
            } else {
                r_squared = 1.0 - ss_res / ss_tot;
            }

            # Direction
            if (slope > direction_threshold) {
                direction = "rising";
            } else if (slope < -direction_threshold) {
                direction = "falling";
            } else {
                direction = "flat";
            }
        }

        return <TrendEventInfo>{
            "event_type": "trend",
            "field": field,
            "window_size": window_size,
            "window_count": window_counts{field},
            "slope": slope,
            "intercept": intercept,
            "r_squared": r_squared,
            "direction": direction,
        };
    }

    #! Not used for window-level processors
    hash<auto> analyzeRecord(string field, float value, list<float> window_values, int total_count) {
        throw "INTERNAL-ERROR", "analyzeRecord() should not be called for a window-level processor";
    }

    #! Returns the output type for analytics events
    AbstractDataProviderType getAnalyticsOutputType() {
        return new HashDeclDataType(TypedHash::forName("DataProvider::TrendEventInfo"));
    }
}
}
