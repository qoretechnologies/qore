# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreImputerProcessor class definition

/** QoreImputerProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that fills missing values using a configurable imputation strategy
/** Accumulates records in windows and uses the ML::Imputer to replace NOTHING
    (missing) values in the selected fields with computed fill values. The first
    window fits the imputer (learning fill values from non-missing data); subsequent
    windows apply the already-fitted imputer.

    NOTHING values in the input records for the selected fields are treated as
    missing values.

*/
public class QoreImputerProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreImputerProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "fields": <DataProviderOptionInfo>{
                "display_name": "Fields to Fill",
                "short_desc": "Which numeric fields may have missing values",
                "desc": "The numeric fields from your data that may have gaps (missing values). "
                    "For example, select `temperature`, `humidity`, and `pressure` if some "
                    "sensor readings are occasionally missing.\n\n"
                    "At least 1 field is required",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "required": True,
                "input_field_names": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Batch Size",
                "short_desc": "How many records to collect before processing",
                "desc": "How many records to collect before processing them as a batch. The "
                    "first batch is used to learn what values to fill in (e.g., the average "
                    "temperature); all subsequent batches use those same fill values.\n\n"
                    "**Tip**: use at least `50` so the fill values are representative; "
                    "`200`+ if many records have gaps",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "example_value": 100,
            },
            "strategy": <DataProviderOptionInfo>{
                "display_name": "Fill Method",
                "short_desc": "How to decide what value to fill in for gaps",
                "desc": "How missing values are filled in. The method is learned from the "
                    "first batch of records and applied consistently to all data",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "mean",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "mean",
                        "display_name": "Average Value",
                        "desc": "Fill gaps with the average (mean) of the available values. "
                            "Best for data that is roughly symmetric, like daily temperatures "
                            "or response times",
                    },
                    <AllowedValueInfo>{
                        "value": "median",
                        "display_name": "Middle Value",
                        "desc": "Fill gaps with the middle value (median). Better than the "
                            "average when your data has extreme values — for example, "
                            "transaction amounts where a few large purchases would skew "
                            "the average",
                    },
                    <AllowedValueInfo>{
                        "value": "most_frequent",
                        "display_name": "Most Common Value",
                        "desc": "Fill gaps with the most frequently occurring value. Best for "
                            "data that tends to cluster around a specific value, like status "
                            "codes or rating scores",
                    },
                    <AllowedValueInfo>{
                        "value": "constant",
                        "display_name": "Fixed Value",
                        "desc": "Fill gaps with a specific number you choose (set in the "
                            "\"Constant Fill Value\" option). Use this when you want a "
                            "sentinel value like `0` or `-1` to mark where data was missing",
                    },
                ),
            },
            "constant_value": <DataProviderOptionInfo>{
                "display_name": "Constant Fill Value",
                "short_desc": "The specific number to use when Fill Method is Fixed Value",
                "desc": "The number to use for filling gaps when the fill method is set to "
                    "\"Fixed Value\". For example, `0` to fill with zero, or `-1` as a "
                    "sentinel value; ignored when using other fill methods; default: `0.0`",
                "type": AbstractDataProviderTypeMap."*float",
                "example_value": 0.0,
            },
        };
    }

    private {
        #! Configured field names
        list<string> fields;

        #! Configured window size
        int window_size;

        #! Imputation strategy
        string strategy = "mean";

        #! Constant fill value (used when strategy is "constant")
        float constant_value = 0.0;

        #! Retained Imputer instance (fitted on first window)
        *ML::Imputer imputer;

        #! Accumulated records
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
        if (window_size < 2) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("window_size must be >= 2; got %d", window_size);
        }
        if (exists copts.strategy) {
            strategy = copts.strategy;
        }
        if (exists copts.constant_value) {
            constant_value = copts.constant_value;
        }
        buffer = ();
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "imputer";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Fills missing (NOTHING) values in numeric fields using a configurable "
            "strategy (mean, median, most_frequent, or constant); accumulates records "
            "in windows, fits the imputer on the first window, and applies it to all "
            "subsequent windows";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records; emits imputed records when window is full
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreImputerProcessor: not configured; provide constructor options";
        }
        push buffer, record;
        if (buffer.lsize() >= window_size) {
            list<auto> results = processWindow(buffer);
            buffer = ();
            return results;
        }
        return;
    }

    #! Flushes remaining buffered data
    private *list<auto> flushRecordsImpl() {
        if (!configured || !buffer) {
            return;
        }
        if (buffer.lsize() >= 2) {
            list<auto> results = processWindow(buffer);
            buffer = ();
            return results;
        }
        buffer = ();
        return;
    }

    #! Processes a window of records through Imputer
    private list<auto> processWindow(list<hash<auto>> records) {
        # extract matrix from fields; NOTHING values become NaN in the matrix
        # which the Imputer treats as missing values
        list<list<*float>> matrix;
        foreach hash<auto> rec in (records) {
            list<*float> row;
            foreach string f in (fields) {
                auto val = DataProvider::AbstractDataProvider::getFieldValue(rec, f);
                if (exists val) {
                    push row, val.toFloat();
                } else {
                    push row, NOTHING;
                }
            }
            push matrix, row;
        }

        list<list<float>> imputed;
        if (!imputer) {
            # first window: create imputer with strategy options, fit and transform
            imputer = new ML::Imputer({
                "strategy": strategy,
                "constant_value": constant_value,
            });
            imputer.fitMatrix(matrix);
            imputed = imputer.transformMatrix(matrix);
        } else {
            # subsequent windows: apply the already-fitted imputer
            imputed = imputer.transformMatrix(matrix);
        }

        # emit one event per record
        list<auto> events;
        for (int i = 0; i < records.lsize(); ++i) {
            hash<auto> record_values;
            hash<auto> imputed_values;
            int missing_count = 0;
            for (int j = 0; j < fields.lsize(); ++j) {
                string f = fields[j];
                auto orig = DataProvider::AbstractDataProvider::getFieldValue(records[i], f);
                record_values{f} = orig;
                imputed_values{f} = imputed[i][j];
                if (!exists orig) {
                    ++missing_count;
                }
            }
            push events, <ImputerEventInfo>{
                "event_type": "imputer",
                "record_values": record_values,
                "imputed_values": imputed_values,
                "missing_count": missing_count,
                "strategy": strategy,
            };
        }
        return events;
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
    private *DataProvider::AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
        return new HashDeclDataType(
            TypedHash::forName("DataProviderML::ImputerEventInfo"));
    }
}
}
