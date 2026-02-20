# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreSeasonalityDetectionProcessor class definition

/** QoreSeasonalityDetectionProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that detects periodic/seasonal patterns via autocorrelation
/** @since %DataProvider 3.3
*/
public class QoreSeasonalityDetectionProcessor inherits AbstractAnalyticsProcessor {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreSeasonalityDetectionProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_ANALYTICS,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = AbstractAnalyticsProcessor::BaseConstructorOptions + {
            "max_period": <DataProviderOptionInfo>{
                "display_name": "Maximum Period",
                "short_desc": "Maximum seasonal period to search for",
                "desc": "The maximum period (in records) to search for; set to `0` to "
                    "use `window_size / 3` automatically; the window size should be at least "
                    "3 times the expected maximum period; default: `0` (auto)",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "min_correlation": <DataProviderOptionInfo>{
                "display_name": "Minimum Correlation",
                "short_desc": "Minimum autocorrelation to report as significant",
                "desc": "The minimum autocorrelation value (`0`-`1`) at which a period is "
                    "considered significant; lower values detect weaker patterns; "
                    "default: `0.3`",
                "type": AbstractDataProviderTypeMap."*float",
            },
        };
    }

    private {
        #! Maximum period to search for
        int max_period = 0;

        #! Minimum correlation threshold
        float min_correlation = 0.3;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        if (exists options.max_period) {
            int mp = options.max_period.toInt();
            if (mp < 0) {
                throw "CONSTRUCTOR-ERROR",
                    sprintf("max_period must be >= 0; got %d", mp);
            }
            max_period = mp;
        }
        if (exists options.min_correlation) {
            float mc = options.min_correlation.toFloat();
            if (mc < 0.0 || mc > 1.0) {
                throw "CONSTRUCTOR-ERROR",
                    sprintf("min_correlation must be between 0 and 1; got %g", mc);
            }
            min_correlation = mc;
        }
        initAnalytics(options, ConstructorOptions);
    }

    #! Returns the name of this data provider
    string getName() {
        return "seasonality-detection";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Detects repeating cyclical patterns in data using autocorrelation analysis; "
            "identifies the dominant period(s) without requiring the user to specify "
            "the period upfront; useful for discovering weekly, monthly, or other patterns";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the analytics mode
    int getAnalyticsMode() {
        return ANALYTICS_MODE_WINDOW;
    }

    #! Analyzes a complete window to detect seasonality
    hash<auto> analyzeWindow(string field, list<float> values, int total_count) {
        int n = values.lsize();

        # determine max lag
        int mp = max_period > 0 ? max_period : n / 3;
        if (mp < 1) {
            mp = 1;
        }
        if (mp >= n) {
            mp = n - 1;
        }

        # compute mean
        float sum = 0.0;
        foreach float v in (values) {
            sum += v;
        }
        float mean = sum / n;

        # compute variance (denominator for ACF)
        float var_sum = 0.0;
        foreach float v in (values) {
            float diff = v - mean;
            var_sum += diff * diff;
        }

        # compute autocorrelation function
        list<float> acf;
        for (int lag = 1; lag <= mp; ++lag) {
            float cov = 0.0;
            for (int t = 0; t < n - lag; ++t) {
                cov += (values[t] - mean) * (values[t + lag] - mean);
            }
            float r;
            if (var_sum > 0.0) {
                r = cov / var_sum;
            } else {
                r = 0.0;
            }
            push acf, r;
        }

        # find peaks in ACF that exceed min_correlation
        list<hash<auto>> significant_periods = ();
        int dominant_period = 0;
        float dominant_corr = 0.0;

        for (int i = 0; i < acf.lsize(); ++i) {
            int lag = i + 1;
            float corr = acf[i];
            if (corr < min_correlation) {
                continue;
            }
            # check if this is a local peak
            bool is_peak = True;
            if (i > 0 && acf[i - 1] >= corr) {
                is_peak = False;
            }
            if (i < acf.lsize() - 1 && acf[i + 1] >= corr) {
                is_peak = False;
            }
            if (is_peak) {
                push significant_periods, {
                    "period": lag,
                    "correlation": corr,
                };
                if (corr > dominant_corr) {
                    dominant_corr = corr;
                    dominant_period = lag;
                }
            }
        }

        # sort significant periods by correlation descending
        if (significant_periods) {
            significant_periods = sort(significant_periods, int sub (hash<auto> a, hash<auto> b) {
            if (a.correlation > b.correlation) {
                return -1;
            }
            if (a.correlation < b.correlation) {
                return 1;
            }
            return 0;
        });
        }

        return <SeasonalityDetectionEventInfo>{
            "event_type": "seasonality-detection",
            "field": field,
            "window_size": window_size,
            "window_count": window_counts{field},
            "has_seasonality": dominant_period > 0,
            "dominant_period": dominant_period,
            "dominant_correlation": dominant_corr,
            "periods": significant_periods,
            "autocorrelation": acf,
        };
    }

    #! Not used for window-level processors
    hash<auto> analyzeRecord(string field, float value, list<float> history, int total_count) {
        throw "INTERNAL-ERROR", "analyzeRecord() should not be called for a window-level processor";
    }

    #! Returns the output type for analytics events
    AbstractDataProviderType getAnalyticsOutputType() {
        return new HashDeclDataType(TypedHash::forName("DataProvider::SeasonalityDetectionEventInfo"));
    }
}
}
