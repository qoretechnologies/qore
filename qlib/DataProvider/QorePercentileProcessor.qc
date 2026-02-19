# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QorePercentileProcessor class definition

/** QorePercentileProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that computes percentiles, IQR, skewness, and kurtosis over windows
/** @since %DataProvider 3.3
*/
public class QorePercentileProcessor inherits AbstractAnalyticsProcessor {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QorePercentileProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_ANALYTICS,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = AbstractAnalyticsProcessor::BaseConstructorOptions + {
            "percentiles": <DataProviderOptionInfo>{
                "display_name": "Percentile Values",
                "short_desc": "Comma-separated percentile values to compute",
                "desc": "A comma-separated list of percentile values to compute (`0`-`100`); "
                    "default: `\"25,50,75,90,95,99\"`",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    private {
        #! Percentile values to compute
        list<float> pct_values = (25.0, 50.0, 75.0, 90.0, 95.0, 99.0);
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        if (exists options.percentiles) {
            list<float> parsed;
            foreach string s in (options.percentiles.split(",")) {
                string trimmed = trim(s);
                if (trimmed.val()) {
                    float p = trimmed.toFloat();
                    if (p < 0.0 || p > 100.0) {
                        throw "CONSTRUCTOR-ERROR",
                            sprintf("percentile value must be between 0 and 100; got %g", p);
                    }
                    push parsed, p;
                }
            }
            if (!parsed) {
                throw "CONSTRUCTOR-ERROR", "no valid percentile values provided";
            }
            pct_values = parsed;
        }
        initAnalytics(options, ConstructorOptions);
    }

    #! Returns the name of this data provider
    string getName() {
        return "percentile";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Computes percentiles (median, quartiles, etc.), interquartile range, "
            "skewness, and kurtosis over a window of records for each configured numeric field";
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

        # sort values for percentile computation
        list<float> sorted = sort(values);

        # compute percentiles via linear interpolation
        hash<auto> pcts;
        foreach float p in (pct_values) {
            # use integer key if percentile is an integer value, otherwise use float string
            string key = p == float(int(p)) ? ("p" + string(int(p))) : ("p" + sprintf("%g", p));
            pcts{key} = computePercentile(sorted, p);
        }

        # IQR
        float p25 = computePercentile(sorted, 25.0);
        float p75 = computePercentile(sorted, 75.0);
        float iqr = p75 - p25;

        # min and max
        float min_val = sorted[0];
        float max_val = sorted[n - 1];

        # mean
        float sum = 0.0;
        foreach float v in (values) {
            sum += v;
        }
        float mean = sum / n;

        # skewness and kurtosis
        float skewness = 0.0;
        float kurtosis = 0.0;
        if (n > 2) {
            float sum_sq = 0.0;
            float sum_cube = 0.0;
            float sum_fourth = 0.0;
            foreach float v in (values) {
                float diff = v - mean;
                float d2 = diff * diff;
                sum_sq += d2;
                sum_cube += d2 * diff;
                sum_fourth += d2 * d2;
            }
            float variance = sum_sq / (n - 1);
            float stddev = sqrt(variance);
            if (stddev > 0.0) {
                # sample skewness (adjusted Fisher-Pearson)
                skewness = (float(n) / ((n - 1) * (n - 2)))
                    * (sum_cube / (stddev * stddev * stddev));
                # sample excess kurtosis (Fisher's definition, matching scipy/pandas)
                # g2 = [n(n+1) / ((n-1)(n-2)(n-3))] * [sum_fourth / s^4]
                #      - 3(n-1)^2 / ((n-2)(n-3))
                # where s^2 = sample variance = sum_sq / (n-1)
                if (n > 3) {
                    float s4 = variance * variance;
                    float coeff = float(n) * (n + 1)
                        / (float(n - 1) * (n - 2) * (n - 3));
                    float adj = 3.0 * float(n - 1) * (n - 1)
                        / (float(n - 2) * (n - 3));
                    kurtosis = coeff * (sum_fourth / s4) - adj;
                } else {
                    # n == 3: use simple population excess kurtosis
                    float m4 = sum_fourth / n;
                    float m2 = sum_sq / n;
                    kurtosis = m2 > 0.0 ? (m4 / (m2 * m2)) - 3.0 : 0.0;
                }
            }
        } else if (n == 1) {
            kurtosis = 0.0;
        }

        return <PercentileEventInfo>{
            "event_type": "percentile",
            "field": field,
            "window_size": window_size,
            "window_count": window_counts{field},
            "percentiles": pcts,
            "iqr": iqr,
            "min_val": min_val,
            "max_val": max_val,
            "skewness": skewness,
            "kurtosis": kurtosis,
            "count": n,
        };
    }

    #! Not used for window-level processors
    hash<auto> analyzeRecord(string field, float value, list<float> history, int total_count) {
        throw "INTERNAL-ERROR", "analyzeRecord() should not be called for a window-level processor";
    }

    #! Returns the output type for analytics events
    AbstractDataProviderType getAnalyticsOutputType() {
        return new HashDeclDataType(TypedHash::forName("DataProvider::PercentileEventInfo"));
    }

    #! Computes a percentile value using linear interpolation
    private static float computePercentile(list<float> sorted, float p) {
        int n = sorted.lsize();
        if (n == 1) {
            return sorted[0];
        }
        float rank = (p / 100.0) * (n - 1);
        int lower = int(floor(rank));
        int upper = int(ceil(rank));
        if (lower == upper) {
            return sorted[lower];
        }
        float frac = rank - lower;
        return sorted[lower] + frac * (sorted[upper] - sorted[lower]);
    }
}
}
