# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreMovingAverageProcessor class definition

/** QoreMovingAverageProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that computes simple moving average (SMA) and exponential moving average (EMA) per record
/** @since %DataProvider 3.3
*/
public class QoreMovingAverageProcessor inherits AbstractAnalyticsProcessor {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreMovingAverageProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_ANALYTICS,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = AbstractAnalyticsProcessor::BaseConstructorOptions + {
            "ema_alpha": <DataProviderOptionInfo>{
                "display_name": "EMA Smoothing Factor",
                "short_desc": "Smoothing factor for exponential moving average (0-1)",
                "desc": "The smoothing factor (alpha) for the exponential moving average; "
                    "higher values weight recent data more heavily; "
                    "must be between `0.0` and `1.0` exclusive; default: `0.2`",
                "type": AbstractDataProviderTypeMap."*float",
            },
        };
    }

    private {
        #! EMA smoothing factor
        float ema_alpha = 0.2;

        #! Per-field EMA running state
        hash<string, float> ema_state;

        #! Per-field flag for EMA initialization
        hash<string, bool> ema_initialized;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        if (exists options.ema_alpha) {
            float alpha = options.ema_alpha.toFloat();
            if (alpha <= 0.0 || alpha >= 1.0) {
                throw "CONSTRUCTOR-ERROR",
                    sprintf("ema_alpha must be between 0.0 and 1.0 exclusive; got %g", alpha);
            }
            ema_alpha = alpha;
        }
        initAnalytics(options, ConstructorOptions);
        # initialize EMA state per field
        foreach string field in (fields) {
            ema_initialized{field} = False;
        }
    }

    #! Returns the name of this data provider
    string getName() {
        return "moving-average";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Computes simple moving average (SMA) and exponential moving average (EMA) for each "
            "configured numeric field, emitting one event per record";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the analytics mode
    int getAnalyticsMode() {
        return ANALYTICS_MODE_RECORD;
    }

    #! Not used for record-level processors
    hash<auto> analyzeWindow(string field, list<float> values, int total_count) {
        # Called during flush — compute a summary SMA/EMA for remaining buffer
        int n = values.lsize();
        float sum = 0.0;
        foreach float v in (values) {
            sum += v;
        }
        float sma = sum / n;

        # Use current EMA state or compute from values
        float ema;
        if (ema_initialized{field}) {
            ema = ema_state{field};
        } else {
            ema = values[0];
            for (int i = 1; i < n; ++i) {
                ema = ema_alpha * values[i] + (1.0 - ema_alpha) * ema;
            }
        }

        return <MovingAverageEventInfo>{
            "event_type": "moving-average",
            "field": field,
            "window_size": window_size,
            "window_count": window_counts{field},
            "sma": sma,
            "ema": ema,
            "ema_alpha": ema_alpha,
        };
    }

    #! Analyzes a single record value
    hash<auto> analyzeRecord(string field, float value, list<float> history, int total_count) {
        # SMA: average of sliding window including current value
        # the sliding window is the most recent window_size values: trim history from front if needed
        list<float> window;
        if (history.lsize() >= window_size) {
            # window is last (window_size - 1) from history + current value
            window = history[(history.lsize() - window_size + 1)..];
        } else {
            window = history;
        }
        push window, value;
        float sum = 0.0;
        foreach float v in (window) {
            sum += v;
        }
        float sma = sum / window.lsize();

        # EMA: seed with first value, then apply alpha formula
        if (!ema_initialized{field}) {
            ema_state{field} = value;
            ema_initialized{field} = True;
        } else {
            ema_state{field} = ema_alpha * value + (1.0 - ema_alpha) * ema_state{field};
        }

        return <MovingAverageEventInfo>{
            "event_type": "moving-average",
            "field": field,
            "window_size": window_size,
            "window_count": window_counts{field},
            "sma": sma,
            "ema": ema_state{field},
            "ema_alpha": ema_alpha,
        };
    }

    #! Returns the output type for analytics events
    AbstractDataProviderType getAnalyticsOutputType() {
        return new HashDeclDataType(TypedHash::forName("DataProvider::MovingAverageEventInfo"));
    }
}
}
