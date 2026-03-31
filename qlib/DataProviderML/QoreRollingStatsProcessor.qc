# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreRollingStatsProcessor class definition

/** QoreRollingStatsProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that enriches records with rolling window statistics for numeric fields
/** Maintains a sliding window of the last N records and computes rolling statistics
    (mean, standard deviation, min, max, sum) for specified numeric fields. Each input
    record is passed through enriched with additional computed fields.

*/
public class QoreRollingStatsProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreRollingStatsProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_TRANSFORM,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "fields": <DataProviderOptionInfo>{
                "display_name": "Numeric Fields to Monitor",
                "short_desc": "Which numeric fields to compute rolling statistics for",
                "desc": "The numeric fields from your data that should have rolling statistics "
                    "computed. For example, select `temperature`, `pressure`, and `flow_rate` "
                    "to track their moving averages, standard deviations, and other statistics "
                    "over a sliding window.\n\n"
                    "At least 1 field is required",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "required": True,
                "input_field_names": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Window Size",
                "short_desc": "How many recent records to include in the rolling computation",
                "desc": "The number of most recent records to use when computing rolling "
                    "statistics. For example, with `window_size=20`, each record's statistics "
                    "are computed over the last 20 records (including itself).\n\n"
                    "**Tip**: use `10`\u201350 for responsive signals; `100`+ for smoother, "
                    "more stable statistics",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "example_value": 20,
            },
            "statistics": <DataProviderOptionInfo>{
                "display_name": "Statistics to Compute",
                "short_desc": "Which rolling statistics to add to each record",
                "desc": "Which statistics to compute over the sliding window for each field. "
                    "Each statistic is added as a new field named `{field}_{stat}` (e.g., "
                    "`temperature_mean`, `price_std`).\n\n"
                    "If not specified, only `mean` is computed by default",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "required": False,
                "element_allowed_values": (
                    <AllowedValueInfo>{
                        "value": "mean",
                        "display_name": "Rolling Average",
                        "desc": "The average of the field values in the current window. Useful "
                            "for smoothing out noise and identifying trends",
                    },
                    <AllowedValueInfo>{
                        "value": "std",
                        "display_name": "Rolling Standard Deviation",
                        "desc": "How spread out the values are in the current window. Useful "
                            "for detecting increased volatility or instability",
                    },
                    <AllowedValueInfo>{
                        "value": "min",
                        "display_name": "Rolling Minimum",
                        "desc": "The smallest value in the current window. Useful for tracking "
                            "lower bounds and detecting new lows",
                    },
                    <AllowedValueInfo>{
                        "value": "max",
                        "display_name": "Rolling Maximum",
                        "desc": "The largest value in the current window. Useful for tracking "
                            "upper bounds and detecting new peaks",
                    },
                    <AllowedValueInfo>{
                        "value": "sum",
                        "display_name": "Rolling Sum",
                        "desc": "The total of all values in the current window. Useful for "
                            "tracking cumulative metrics like rolling revenue or total events",
                    },
                ),
            },
        };
    }

    private {
        #! Configured field names
        list<string> fields;

        #! Configured window size
        int window_size;

        #! Which statistics to compute
        list<string> statistics;

        #! Sliding window buffer of recent records
        list<hash<auto>> buffer;

        #! Whether configured
        bool configured = False;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        fields = DataProvider::AbstractDataProvider::parseFieldList(copts.fields);
        if (!fields) {
            throw "CONSTRUCTOR-ERROR", "no valid field names provided in 'fields' option";
        }
        window_size = copts.window_size;
        if (window_size < 1) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("window_size must be >= 1; got %d", window_size);
        }
        if (copts.statistics) {
            statistics = DataProvider::AbstractDataProvider::parseFieldList(copts.statistics);
        }
        if (!statistics) {
            statistics = ("mean",);
        }
        buffer = ();
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "rolling-stats";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Enriches records with rolling window statistics (mean, std, min, max, sum) "
            "for specified numeric fields; maintains a sliding window of recent records and "
            "computes statistics over that window for each incoming record";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Processes a single record: updates the sliding window and enriches the record with rolling statistics
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreRollingStatsProcessor: not configured; provide constructor options";
        }
        # add to sliding window
        push buffer, record;
        if (buffer.lsize() > window_size) {
            splice buffer, 0, 1;
        }

        # start with the original record
        hash<auto> result = {} + record;

        # compute statistics for each monitored field
        foreach string field in (fields) {
            # extract values from the window for this field
            list<float> values = ();
            foreach hash<auto> rec in (buffer) {
                push values,
                    DataProvider::AbstractDataProvider::getFieldValue(rec, field).toFloat();
            }
            int n = values.lsize();

            foreach string stat in (statistics) {
                string output_key = field + "_" + stat;
                switch (stat) {
                    case "mean": {
                        float total = 0.0;
                        foreach float v in (values) {
                            total += v;
                        }
                        result{output_key} = total / n;
                        break;
                    }
                    case "std": {
                        float total = 0.0;
                        foreach float v in (values) {
                            total += v;
                        }
                        float mean = total / n;
                        float sq_sum = 0.0;
                        foreach float v in (values) {
                            float diff = v - mean;
                            sq_sum += diff * diff;
                        }
                        result{output_key} = sqrt(sq_sum / n);
                        break;
                    }
                    case "min": {
                        float min_val = values[0];
                        for (int i = 1; i < n; ++i) {
                            if (values[i] < min_val) {
                                min_val = values[i];
                            }
                        }
                        result{output_key} = min_val;
                        break;
                    }
                    case "max": {
                        float max_val = values[0];
                        for (int i = 1; i < n; ++i) {
                            if (values[i] > max_val) {
                                max_val = values[i];
                            }
                        }
                        result{output_key} = max_val;
                        break;
                    }
                    case "sum": {
                        float total = 0.0;
                        foreach float v in (values) {
                            total += v;
                        }
                        result{output_key} = total;
                        break;
                    }
                }
            }
        }

        return result;
    }

    #! Does not support bulk API
    private bool supportsBulkApiImpl() {
        return False;
    }

    #! Returns the processor capability bitfield
    private int getProcessorCapabilitiesImpl() {
        return DPC_TRANSFORM;
    }

    #! Returns NOTHING — output type is dynamic (input record enriched with computed fields)
    private *DataProvider::AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
        return NOTHING;
    }
}
}
