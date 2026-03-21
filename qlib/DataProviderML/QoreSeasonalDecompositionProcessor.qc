# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreSeasonalDecompositionProcessor class definition

/** QoreSeasonalDecompositionProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DataProviderML module
public namespace DataProviderML {
#! Pipeline processor that decomposes time series into trend, seasonal, and residual components
/** Uses the ML::SeasonalDecomposition algorithm. Accumulates per-field values in
    windows and decomposes them when the window is full.

    Inherits AbstractAnalyticsProcessor with ANALYTICS_MODE_WINDOW.

*/
public class QoreSeasonalDecompositionProcessor inherits DataProvider::AbstractAnalyticsProcessor {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreSeasonalDecompositionProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_ANALYTICS,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = AbstractAnalyticsProcessor::BaseConstructorOptions + {
            "period": <DataProviderOptionInfo>{
                "display_name": "Seasonal Period",
                "short_desc": "Records per seasonal cycle",
                "desc": "The number of records that make up one complete repeating cycle; "
                    "for example, `7` for daily data with weekly patterns, `12` for monthly "
                    "data with yearly patterns; the window size must be at least twice this "
                    "value; must be at least 2",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "example_value": 12,
            },
            "type": <DataProviderOptionInfo>{
                "display_name": "Decomposition Type",
                "short_desc": "How seasonal patterns combine with the trend",
                "desc": "Controls how the seasonal component relates to the trend; "
                    "default: `additive`",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "additive",
                        "display_name": "Additive (constant seasonal swings)",
                        "desc": "Seasonal variations have a constant size regardless of the "
                            "overall level; best when the seasonal pattern does not grow or "
                            "shrink as the data increases",
                    },
                    <AllowedValueInfo>{
                        "value": "multiplicative",
                        "display_name": "Multiplicative (proportional seasonal swings)",
                        "desc": "Seasonal variations scale with the overall level; best when "
                            "the seasonal pattern grows as the data values increase",
                    },
                ),
            },
        };
    }

    private {
        #! Period for decomposition
        int period;

        #! Decomposition type
        string decomp_type = "additive";
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        if (!exists options.period) {
            throw "CONSTRUCTOR-ERROR", "required option 'period' not provided";
        }
        period = options.period.toInt();
        if (period < 2) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("period must be >= 2; got %d", period);
        }
        if (exists options{"type"}) {
            decomp_type = options{"type"};
        }
        initAnalytics(options, ConstructorOptions);
        # validate window size after initAnalytics sets it
        if (window_size < 2 * period) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("window_size (%d) must be >= 2 * period (%d) = %d",
                    window_size, period, 2 * period);
        }
    }

    #! Returns the name of this data provider
    string getName() {
        return "seasonal-decomposition";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Decomposes time series into trend, seasonal, and residual components "
            "using classical seasonal decomposition; accumulates per-field values in "
            "windows and emits decomposition events when the window is full";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the analytics mode
    int getAnalyticsMode() {
        return ANALYTICS_MODE_WINDOW;
    }

    #! Analyzes a complete window for seasonal decomposition
    hash<auto> analyzeWindow(string field, list<float> values, int total_count) {
        # need enough data for decomposition
        if (values.lsize() < 2 * period) {
            # return a minimal event if not enough data
            return <SeasonalDecompositionEventInfo>{
                "event_type": "seasonal-decomposition",
                "field": field,
                "window_size": window_size,
                "window_count": window_counts{field},
                "observed": values[values.lsize() - 1],
                "seasonal_value": 0.0,
                "decomposition_type": decomp_type,
                "period": period,
            };
        }

        # create decomposition
        ML::SeasonalDecomposition sd({"period": period, "type": decomp_type});
        list<hash<ML::SeasonalDecompositionResult>> results = sd.decompose(values);

        # return a summary for the last value with full decomposition arrays
        int last_idx = results.lsize() - 1;
        return <SeasonalDecompositionEventInfo>{
            "event_type": "seasonal-decomposition",
            "field": field,
            "window_size": window_size,
            "window_count": window_counts{field},
            "observed": values[last_idx],
            "trend_value": results[last_idx].trend,
            "seasonal_value": results[last_idx].seasonal,
            "residual_value": results[last_idx].residual,
            "decomposition_type": decomp_type,
            "period": period,
        };
    }

    #! Not used for window-level processors
    hash<auto> analyzeRecord(string field, float value, list<float> history, int total_count) {
        throw "INTERNAL-ERROR",
            "analyzeRecord() should not be called for a window-level processor";
    }

    #! Returns the output type for analytics events
    DataProvider::AbstractDataProviderType getAnalyticsOutputType() {
        return new HashDeclDataType(
            TypedHash::forName("DataProviderML::SeasonalDecompositionEventInfo"));
    }
}
}
