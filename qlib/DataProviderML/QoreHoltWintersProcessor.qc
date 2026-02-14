# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreHoltWintersProcessor class definition

/** QoreHoltWintersProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that forecasts values using Holt-Winters triple exponential smoothing
/** Uses the ML::HoltWinters algorithm. Accumulates per-field history until at least
    2 * period values are available, then fits the model. For subsequent records,
    calls update() and emits forecast events.

    Inherits AbstractAnalyticsProcessor with ANALYTICS_MODE_RECORD.

    @since %DataProviderML 1.0
*/
public class QoreHoltWintersProcessor inherits DataProvider::AbstractAnalyticsProcessor {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreHoltWintersProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_ANALYTICS,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = AbstractAnalyticsProcessor::BaseConstructorOptions + {
            "period": <DataProviderOptionInfo>{
                "display_name": "Seasonal Period",
                "short_desc": "Number of records in one seasonal cycle",
                "desc": "The number of records in one complete seasonal cycle; for example, "
                    "7 for daily data with weekly seasonality, 12 for monthly data with "
                    "yearly seasonality; must be >= 2",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
            },
            "alpha": <DataProviderOptionInfo>{
                "display_name": "Level Smoothing (Alpha)",
                "short_desc": "Smoothing parameter for the level component",
                "desc": "Controls how quickly the level adapts to changes; values closer "
                    "to 1.0 weight recent observations more heavily; default: auto-selected",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "beta": <DataProviderOptionInfo>{
                "display_name": "Trend Smoothing (Beta)",
                "short_desc": "Smoothing parameter for the trend component",
                "desc": "Controls how quickly the trend adapts to changes; values closer "
                    "to 1.0 weight recent trend changes more heavily; default: auto-selected",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "gamma": <DataProviderOptionInfo>{
                "display_name": "Seasonal Smoothing (Gamma)",
                "short_desc": "Smoothing parameter for the seasonal component",
                "desc": "Controls how quickly the seasonal factors adapt to changes; values "
                    "closer to 1.0 weight recent seasonal observations more heavily; "
                    "default: auto-selected",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "forecast_steps": <DataProviderOptionInfo>{
                "display_name": "Forecast Steps",
                "short_desc": "Number of steps ahead to forecast",
                "desc": "The number of future steps to forecast for each record; "
                    "default: 1",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "seasonal_type": <DataProviderOptionInfo>{
                "display_name": "Seasonal Type",
                "short_desc": "Type of seasonal component",
                "desc": "The type of seasonal component: 'additive' for constant seasonal "
                    "amplitude, 'multiplicative' for seasonal amplitude proportional to "
                    "the level; default: 'additive'",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "additive",
                        "desc": "Constant seasonal amplitude",
                    },
                    <AllowedValueInfo>{
                        "value": "multiplicative",
                        "desc": "Seasonal amplitude proportional to the level",
                    },
                ),
            },
            "damped": <DataProviderOptionInfo>{
                "display_name": "Damped Trend",
                "short_desc": "Whether to damp the trend component",
                "desc": "When True, the trend is damped so long-range forecasts converge "
                    "rather than growing indefinitely; default: False",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "phi": <DataProviderOptionInfo>{
                "display_name": "Damping Factor (Phi)",
                "short_desc": "Damping coefficient for the trend",
                "desc": "The damping factor (0.0-1.0) controlling how quickly the trend "
                    "diminishes; only used when damped is True; default: auto-selected",
                "type": AbstractDataProviderTypeMap."*float",
            },
        };
    }

    private {
        #! Period for Holt-Winters
        int period;

        #! Forecast steps
        int forecast_steps = 1;

        #! HW constructor options to pass to ML::HoltWinters
        hash<auto!> hw_opts;

        #! Per-field HoltWinters instances
        hash<string, ML::HoltWinters> hw_models;

        #! Per-field accumulator for initial training data
        hash<string, list<float>> hw_history;

        #! Per-field fitted status
        hash<string, bool> hw_fitted;
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
        hw_opts = {"period": period};
        if (exists options.alpha) {
            hw_opts.alpha = options.alpha.toFloat();
        }
        if (exists options.beta) {
            hw_opts.beta = options.beta.toFloat();
        }
        if (exists options.gamma) {
            hw_opts.gamma = options.gamma.toFloat();
        }
        if (exists options.seasonal_type) {
            hw_opts.seasonal_type = options.seasonal_type;
        }
        if (exists options.damped) {
            hw_opts.damped = options.damped;
        }
        if (exists options.phi) {
            hw_opts.phi = options.phi.toFloat();
        }
        if (exists options.forecast_steps) {
            forecast_steps = options.forecast_steps.toInt();
            if (forecast_steps < 1) {
                throw "CONSTRUCTOR-ERROR",
                    sprintf("forecast_steps must be >= 1; got %d", forecast_steps);
            }
        }
        initAnalytics(options, ConstructorOptions);
    }

    #! Returns the name of this data provider
    string getName() {
        return "holt-winters";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Forecasts future values using Holt-Winters triple exponential smoothing, "
            "which accounts for level, trend, and seasonal patterns; emits forecast "
            "events after an initial training period of 2 * period records per field";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the analytics mode
    int getAnalyticsMode() {
        return ANALYTICS_MODE_RECORD;
    }

    #! Analyzes a single record for HoltWinters forecasting
    hash<auto> analyzeRecord(string field, float value, list<float> history, int total_count) {
        int min_history = 2 * period;

        # initialize per-field state if needed
        if (!hw_history.hasKey(field)) {
            hw_history{field} = ();
            hw_fitted{field} = False;
        }

        if (!hw_fitted{field}) {
            # accumulate history until we have enough for fitting
            push hw_history{field}, value;
            if (hw_history{field}.lsize() >= min_history) {
                # fit the model
                ML::HoltWinters hw(hw_opts);
                hw.fit(hw_history{field});
                hw_models{field} = hw;
                hw_fitted{field} = True;
                hw_history{field} = ();

                # return forecast from the fitted model
                list<float> fc = hw_models{field}.forecast(forecast_steps);
                return <HoltWintersForecastEventInfo>{
                    "event_type": "holt-winters",
                    "field": field,
                    "window_size": window_size,
                    "window_count": window_counts{field},
                    "forecast": fc,
                    "level": hw_models{field}.getLevel(),
                    "trend": hw_models{field}.getTrend(),
                    "seasonal": hw_models{field}.getSeasonal()[0],
                    "residual": 0.0,
                };
            }
            # not enough data yet - return empty event indicating accumulation
            return <HoltWintersForecastEventInfo>{
                "event_type": "holt-winters",
                "field": field,
                "window_size": window_size,
                "window_count": window_counts{field},
                "forecast": (),
                "level": 0.0,
                "trend": 0.0,
                "seasonal": 0.0,
                "residual": 0.0,
            };
        }

        # model is fitted - update with new observation
        hash<ML::HoltWintersResult> result = hw_models{field}.update(value);
        list<float> fc = hw_models{field}.forecast(forecast_steps);
        float fitted_val = result.forecast;
        float residual = value - fitted_val;

        return <HoltWintersForecastEventInfo>{
            "event_type": "holt-winters",
            "field": field,
            "window_size": window_size,
            "window_count": window_counts{field},
            "forecast": fc,
            "level": result.level,
            "trend": result.trend,
            "seasonal": result.seasonal,
            "residual": residual,
        };
    }

    #! Analyzes a complete window (used during flush)
    /** Processes each buffered value through analyzeRecord() to maintain correct
        internal state (accumulation, fitting, and updating). Only the final result
        is returned since intermediate events are not meaningful during flush.
        The empty history parameter is safe because analyzeRecord() manages its own
        internal state (hw_history, hw_models) rather than using the history arg.
    */
    hash<auto> analyzeWindow(string field, list<float> values, int total_count) {
        hash<auto> last_result;
        foreach float v in (values) {
            last_result = analyzeRecord(field, v, (), total_count);
        }
        return last_result;
    }

    #! Returns the output type for analytics events
    DataProvider::AbstractDataProviderType getAnalyticsOutputType() {
        return new HashDeclDataType(TypedHash::forName("DataProviderML::HoltWintersForecastEventInfo"));
    }
}
}
