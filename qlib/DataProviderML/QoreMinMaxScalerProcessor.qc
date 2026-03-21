# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreMinMaxScalerProcessor class definition

/** QoreMinMaxScalerProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that scales numeric features to a configurable range
/** Accumulates records in windows and uses the ML::MinMaxScaler to transform
    each record's selected fields into a bounded range (default [0, 1]). The first
    window fits the scaler (learning per-feature min/max); subsequent windows apply
    the already-fitted scaler.

*/
public class QoreMinMaxScalerProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreMinMaxScalerProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "fields": <DataProviderOptionInfo>{
                "display_name": "Fields to Normalize",
                "short_desc": "Which numeric fields to convert to the target range",
                "desc": "The numeric fields from your data to normalize. For example, select "
                    "`temperature`, `humidity`, and `wind_speed` to convert them all to the "
                    "same range for easy comparison.\n\n"
                    "At least 1 field is required",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "required": True,
                "input_field_names": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Batch Size",
                "short_desc": "How many records to collect before processing",
                "desc": "How many records to collect before processing them as a batch. The "
                    "first batch is used to learn the minimum and maximum of each field; all "
                    "subsequent batches are normalized using those learned values.\n\n"
                    "**Tip**: use at least `50` for reliable results; `200`+ if your data "
                    "has occasional extreme values (outliers)",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "example_value": 100,
            },
            "feature_min": <DataProviderOptionInfo>{
                "display_name": "Target Minimum",
                "short_desc": "Lowest value in the output range",
                "desc": "The lowest value that normalized output will have. For example, set "
                    "to `0.0` to have all values start at zero, or `-1.0` for a range centered "
                    "around zero; default: `0.0`",
                "type": AbstractDataProviderTypeMap."*float",
                "example_value": 0.0,
            },
            "feature_max": <DataProviderOptionInfo>{
                "display_name": "Target Maximum",
                "short_desc": "Highest value in the output range",
                "desc": "The highest value that normalized output will have. For example, set "
                    "to `1.0` for a 0-to-1 range, or `100.0` for a percentage scale; must be "
                    "greater than the target minimum; default: `1.0`",
                "type": AbstractDataProviderTypeMap."*float",
                "example_value": 1.0,
            },
        };
    }

    private {
        #! Configured field names
        list<string> fields;

        #! Configured window size
        int window_size;

        #! Target range minimum
        float feature_min = 0.0;

        #! Target range maximum
        float feature_max = 1.0;

        #! Retained MinMaxScaler instance (fitted on first window)
        *ML::MinMaxScaler scaler;

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
        if (exists copts.feature_min) {
            feature_min = copts.feature_min;
        }
        if (exists copts.feature_max) {
            feature_max = copts.feature_max;
        }
        if (feature_min >= feature_max) {
            throw "CONSTRUCTOR-ERROR",
                sprintf("feature_min (%.6f) must be less than feature_max (%.6f)",
                    feature_min, feature_max);
        }
        buffer = ();
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "min-max-scaler";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Scales numeric features to a configurable range (default [0, 1]); "
            "accumulates records in windows, fits the scaler on the first window, "
            "and applies it to all subsequent windows";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records; emits scaled records when window is full
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "QoreMinMaxScalerProcessor: not configured; provide constructor options";
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

    #! Processes a window of records through MinMaxScaler
    private list<auto> processWindow(list<hash<auto>> records) {
        # extract matrix from fields
        list<list<float>> matrix;
        foreach hash<auto> rec in (records) {
            list<float> row;
            foreach string f in (fields) {
                push row, DataProvider::AbstractDataProvider::getFieldValue(rec, f).toFloat();
            }
            push matrix, row;
        }

        list<list<float>> scaled;
        if (!scaler) {
            # first window: create scaler with range options, fit and transform
            scaler = new ML::MinMaxScaler({
                "feature_min": feature_min,
                "feature_max": feature_max,
            });
            scaler.fitMatrix(matrix);
            scaled = scaler.transformMatrix(matrix);
        } else {
            # subsequent windows: apply the already-fitted scaler
            scaled = scaler.transformMatrix(matrix);
        }

        # emit one event per record
        list<auto> events;
        for (int i = 0; i < records.lsize(); ++i) {
            hash<auto> record_values;
            hash<auto> scaled_values;
            for (int j = 0; j < fields.lsize(); ++j) {
                string f = fields[j];
                record_values{f} = DataProvider::AbstractDataProvider::getFieldValue(records[i], f);
                scaled_values{f} = scaled[i][j];
            }
            push events, <MinMaxScalerEventInfo>{
                "event_type": "min-max-scaler",
                "record_values": record_values,
                "scaled_values": scaled_values,
                "feature_min": feature_min,
                "feature_max": feature_max,
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
            TypedHash::forName("DataProviderML::MinMaxScalerEventInfo"));
    }
}
}
