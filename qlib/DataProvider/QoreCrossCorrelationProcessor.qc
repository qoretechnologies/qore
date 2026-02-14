# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreCrossCorrelationProcessor class definition

/** QoreCrossCorrelationProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that computes time-lagged cross-correlation between two fields
/** Accumulates records in windows and computes the cross-correlation function between
    two fields at various lags to discover delayed relationships.

    @since %DataProvider 3.3
*/
public class QoreCrossCorrelationProcessor inherits AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreCrossCorrelationProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "field_x": <DataProviderOptionInfo>{
                "display_name": "First Field",
                "short_desc": "First numeric field name",
                "desc": "The first numeric field for cross-correlation analysis",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "field_y": <DataProviderOptionInfo>{
                "display_name": "Second Field",
                "short_desc": "Second numeric field name",
                "desc": "The second numeric field; positive lag means this field is "
                    "delayed relative to the first field",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Window Size",
                "short_desc": "Number of records per analysis window",
                "desc": "The number of records to accumulate before computing cross-correlations",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
            },
            "max_lag": <DataProviderOptionInfo>{
                "display_name": "Maximum Lag",
                "short_desc": "Maximum lag to compute (0 = auto)",
                "desc": "The maximum number of time steps to shift one field relative to "
                    "the other; set to 0 to use window_size / 4 automatically; "
                    "default: 0 (auto)",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    private {
        #! First field name
        string field_x;

        #! Second field name
        string field_y;

        #! Window size
        int window_size;

        #! Max lag
        int max_lag = 0;

        #! Accumulated records
        list<hash<auto>> buffer;

        #! Window counter
        int window_count = 0;

        #! Whether configured
        bool configured = False;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        field_x = copts.field_x;
        field_y = copts.field_y;
        if (field_x == field_y) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("field_x and field_y must be different; both are %y", field_x);
        }
        window_size = copts.window_size;
        if (window_size < 3) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("window_size must be >= 3 for cross-correlation; got %d", window_size);
        }
        if (exists copts.max_lag) {
            max_lag = copts.max_lag.toInt();
            if (max_lag < 0) {
                throw "CONSTRUCTOR-ERROR",
                    sprintf("max_lag must be >= 0; got %d", max_lag);
            }
        }
        buffer = ();
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "cross-correlation";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Computes time-lagged cross-correlation between two numeric fields to discover "
            "delayed relationships; for example, whether changes in one field predict changes "
            "in another after a delay";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records; emits cross-correlation when window is full
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreCrossCorrelationProcessor: not configured; provide constructor options";
        }
        push buffer, record;
        if (buffer.lsize() >= window_size) {
            hash<auto> result = computeCrossCorrelation(buffer);
            buffer = ();
            return result;
        }
        return;
    }

    #! Flushes remaining buffered data
    private *list<auto> flushRecordsImpl() {
        if (!configured || !buffer) {
            return;
        }
        if (buffer.lsize() >= 3) {
            hash<auto> result = computeCrossCorrelation(buffer);
            buffer = ();
            return (result,);
        }
        buffer = ();
        return;
    }

    #! Computes cross-correlation from buffered records
    private hash<auto> computeCrossCorrelation(list<hash<auto>> records) {
        window_count++;
        int n = records.lsize();

        # extract values
        list<float> x_vals;
        list<float> y_vals;
        foreach hash<auto> rec in (records) {
            push x_vals, rec.hasKey(field_x) ? rec{field_x}.toFloat() : 0.0;
            push y_vals, rec.hasKey(field_y) ? rec{field_y}.toFloat() : 0.0;
        }

        # compute means
        float x_mean = 0.0;
        float y_mean = 0.0;
        foreach float v in (x_vals) {
            x_mean += v;
        }
        x_mean /= n;
        foreach float v in (y_vals) {
            y_mean += v;
        }
        y_mean /= n;

        # compute normalization factors: sqrt(sum((x - mean)^2))
        # note: we use the full-window sum of squared deviations (not divided by n or n-1)
        # as the normalization denominator; this ensures cross-correlations at all lags are
        # normalized against the same total energy and bounded by [-1, 1]
        float x_var = 0.0;
        float y_var = 0.0;
        foreach float v in (x_vals) {
            float diff = v - x_mean;
            x_var += diff * diff;
        }
        foreach float v in (y_vals) {
            float diff = v - y_mean;
            y_var += diff * diff;
        }
        float x_norm = sqrt(x_var);
        float y_norm = sqrt(y_var);

        # determine max lag
        int ml = max_lag > 0 ? max_lag : n / 4;
        if (ml >= n) {
            ml = n - 1;
        }

        # compute cross-correlation for each lag
        hash<auto> lag_correlations;
        int best_lag = 0;
        float best_corr = 0.0;

        for (int lag = -ml; lag <= ml; ++lag) {
            float cov = 0.0;
            int overlap = 0;
            if (lag >= 0) {
                for (int t = 0; t < n - lag; ++t) {
                    cov += (x_vals[t] - x_mean) * (y_vals[t + lag] - y_mean);
                    overlap++;
                }
            } else {
                for (int t = -lag; t < n; ++t) {
                    cov += (x_vals[t] - x_mean) * (y_vals[t + lag] - y_mean);
                    overlap++;
                }
            }

            float r;
            if (x_norm > 0.0 && y_norm > 0.0 && overlap > 0) {
                r = cov / (x_norm * y_norm);
            } else {
                r = 0.0;
            }

            lag_correlations{string(lag)} = r;

            if (abs(r) > abs(best_corr)) {
                best_corr = r;
                best_lag = lag;
            }
        }

        return <CrossCorrelationEventInfo>{
            "event_type": "cross-correlation",
            "field_x": field_x,
            "field_y": field_y,
            "window_size": window_size,
            "window_count": window_count,
            "max_lag": ml,
            "lag_correlations": lag_correlations,
            "best_lag": best_lag,
            "best_correlation": best_corr,
            "count": n,
        };
    }

    #! Does not support bulk API
    private bool supportsBulkApiImpl() {
        return False;
    }

    #! Returns the processor capability bitfield
    private int getProcessorCapabilitiesImpl() {
        return DPC_AGGREGATE;
    }

    #! Returns the output type
    private *AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
        return new HashDeclDataType(TypedHash::forName("DataProvider::CrossCorrelationEventInfo"));
    }
}
}
