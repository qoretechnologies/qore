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
public class QoreRollingStatsProcessor inherits AbstractDataProviderMLProcessor {
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

        #! Sliding numeric windows per configured field
        hash<auto> field_buffers;

        #! Whether to emit each configured statistic
        bool emit_mean = False;
        bool emit_std = False;
        bool emit_min = False;
        bool emit_max = False;
        bool emit_sum = False;

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
        configureStatisticFlags();
        initializeFieldBuffers();
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
        hash<auto> row_values = getRecordFieldValues(record);
        hash<auto> next_buffers = copyFieldBuffers();
        appendRowValues(\next_buffers, row_values);

        hash<auto> result = {} + record;
        addStatisticValues(\result, next_buffers);
        field_buffers = next_buffers;
        return result;
    }

    #! Processes records in bulk hash-of-lists format without row materialization when possible
    private *hash<auto> processRecordsBulkImpl(hash<auto> records) {
        updatePendingOutputShape(DataProvider::DataProviderShape::HashOfLists);
        *auto result = processRecordsShapeImpl(records, DataProvider::DataProviderShape::HashOfLists);
        return exists result ? cast<hash>(result) : NOTHING;
    }

    #! Processes an explicitly negotiated pipeline shape without row materialization when possible
    private *auto processRecordsShapeImpl(auto records, DataProvider::DataProviderShape shape) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreRollingStatsProcessor: not configured; provide constructor options";
        }

        updatePendingOutputShape(shape);
        if (shape == DataProvider::DataProviderShape::RowRecords) {
            return processRecordImpl(records);
        }

        if (hasNestedFields()) {
            return processShapeRowsFallback(records, shape);
        }

        int count = DataProvider::AbstractDataProcessor::shapeRecordCount(records, shape);
        if (!count) {
            return;
        }

        *hash<auto> shape_values = getShapeFieldValues(records, shape, count);
        if (!exists shape_values) {
            return processShapeRowsFallback(records, shape);
        }

        hash<auto> next_buffers = copyFieldBuffers();
        hash<auto> output_columns = makeOutputColumns(count);
        for (int row = 0; row < count; ++row) {
            foreach string field in (fields) {
                push next_buffers{field}, shape_values{field}[row];
                trimWindow(\next_buffers, field);
                setStatisticColumnValues(\output_columns, field, next_buffers{field}, row);
            }
        }

        auto result = DataProvider::AbstractDataProcessor::appendShapeColumns(records, shape, output_columns);
        field_buffers = next_buffers;
        return result;
    }

    #! Initializes per-field numeric windows
    private initializeFieldBuffers() {
        field_buffers = {};
        foreach string field in (fields) {
            field_buffers{field} = ();
        }
    }

    #! Configures fast statistic switches from the validated option list
    private configureStatisticFlags() {
        emit_mean = statistics.contains("mean");
        emit_std = statistics.contains("std");
        emit_min = statistics.contains("min");
        emit_max = statistics.contains("max");
        emit_sum = statistics.contains("sum");
    }

    #! Returns @ref True if any configured field requires row materialization
    private bool hasNestedFields() {
        foreach string field in (fields) {
            if (field.find(".") >= 0) {
                return True;
            }
        }
        return False;
    }

    #! Reads configured numeric fields from one row before updating state
    private hash<auto> getRecordFieldValues(auto record) {
        hash<auto> values;
        foreach string field in (fields) {
            values{field} = DataProvider::AbstractDataProvider::getFieldValue(record, field).toFloat();
        }
        return values;
    }

    #! Reads configured numeric fields directly from shaped columns
    private *hash<auto> getShapeFieldValues(auto records, DataProvider::DataProviderShape shape, int count) {
        hash<auto> values;
        foreach string field in (fields) {
            *auto column = DataProvider::AbstractDataProcessor::getShapeColumn(records, shape, field);
            if (!exists column) {
                return;
            }
            values{field} = DataProvider::AbstractDataProcessor::columnValuesToFloatList(column, count, field);
        }
        return values;
    }

    #! Returns a deep copy of the current sliding numeric windows
    private hash<auto> copyFieldBuffers() {
        hash<auto> result;
        foreach string field in (fields) {
            result{field} = field_buffers.hasKey(field) ? field_buffers{field} + () : ();
        }
        return result;
    }

    #! Appends one row of numeric values to copied sliding windows
    private appendRowValues(reference<hash<auto>> buffers, hash<auto> row_values) {
        foreach string field in (fields) {
            push buffers{field}, row_values{field};
            trimWindow(\buffers, field);
        }
    }

    #! Keeps a copied field window within the configured window size
    private trimWindow(reference<hash<auto>> buffers, string field) {
        int overflow = buffers{field}.lsize() - window_size;
        if (overflow > 0) {
            splice buffers{field}, 0, overflow;
        }
    }

    #! Adds row-oriented statistic fields to a copied result record
    private addStatisticValues(reference<hash<auto>> result, hash<auto> buffers) {
        foreach string field in (fields) {
            setStatisticHashValues(\result, field, buffers{field});
        }
    }

    #! Creates dense output buffers for all requested statistic columns
    private hash<auto> makeOutputColumns(int count) {
        hash<auto> result;
        foreach string field in (fields) {
            if (emit_mean) {
                result{field + "_mean"} = buffer<float64>::sized(count);
            }
            if (emit_std) {
                result{field + "_std"} = buffer<float64>::sized(count);
            }
            if (emit_min) {
                result{field + "_min"} = buffer<float64>::sized(count);
            }
            if (emit_max) {
                result{field + "_max"} = buffer<float64>::sized(count);
            }
            if (emit_sum) {
                result{field + "_sum"} = buffer<float64>::sized(count);
            }
        }
        return result;
    }

    #! Adds statistic values to row output
    private setStatisticHashValues(reference<hash<auto>> result, string field, list<auto> values) {
        int n = values.lsize();
        float total = 0.0;
        float min_val = values[0];
        float max_val = values[0];
        foreach float value in (values) {
            total += value;
            if (value < min_val) {
                min_val = value;
            }
            if (value > max_val) {
                max_val = value;
            }
        }
        float mean = total / n;
        if (emit_mean) {
            result{field + "_mean"} = mean;
        }
        if (emit_std) {
            result{field + "_std"} = calculateStd(values, mean);
        }
        if (emit_min) {
            result{field + "_min"} = min_val;
        }
        if (emit_max) {
            result{field + "_max"} = max_val;
        }
        if (emit_sum) {
            result{field + "_sum"} = total;
        }
    }

    #! Adds statistic values to dense shaped output columns
    private setStatisticColumnValues(reference<hash<auto>> output_columns, string field, list<auto> values, int row) {
        int n = values.lsize();
        float total = 0.0;
        float min_val = values[0];
        float max_val = values[0];
        foreach float value in (values) {
            total += value;
            if (value < min_val) {
                min_val = value;
            }
            if (value > max_val) {
                max_val = value;
            }
        }
        float mean = total / n;
        if (emit_mean) {
            output_columns{field + "_mean"}[row] = mean;
        }
        if (emit_std) {
            output_columns{field + "_std"}[row] = calculateStd(values, mean);
        }
        if (emit_min) {
            output_columns{field + "_min"}[row] = min_val;
        }
        if (emit_max) {
            output_columns{field + "_max"}[row] = max_val;
        }
        if (emit_sum) {
            output_columns{field + "_sum"}[row] = total;
        }
    }

    #! Calculates population standard deviation for a prepared rolling window
    private float calculateStd(list<auto> values, float mean) {
        float sq_sum = 0.0;
        foreach float value in (values) {
            float diff = value - mean;
            sq_sum += diff * diff;
        }
        return sqrt(sq_sum / values.lsize());
    }

    #! Uses the row-oriented implementation when a column fast path is not safe
    private *auto processShapeRowsFallback(auto records, DataProvider::DataProviderShape shape) {
        list<auto> rows = DataProvider::AbstractDataProcessor::convertShape(
            records, shape, DataProvider::DataProviderShape::RowRecords);
        *list<auto> results;
        foreach auto row in (rows) {
            *auto row_result = processRecordImpl(row);
            if (exists row_result) {
                if (row_result.typeCode() == NT_LIST) {
                    results += row_result;
                } else {
                    push results, row_result;
                }
            }
        }
        if (!results) {
            return;
        }
        hash<auto> bulk = DataProvider::AbstractDataProcessor::recordsToHashList(results);
        return AbstractDataProviderMLProcessor::convertBulkOutputShape(bulk, shape);
    }

    #! Supports shaped bulk API
    private bool supportsBulkApiImpl() {
        return True;
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
