# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreStandardScalerProcessor class definition

/** QoreStandardScalerProcessor.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Pipeline processor that standardizes numeric features to zero mean and unit variance
/** Accumulates records in windows and uses the ML::StandardScaler to transform
    each record's selected fields to zero mean and unit variance. The first window
    fits the scaler (learning means and standard deviations); subsequent windows
    apply the already-fitted scaler.

*/
public class QoreStandardScalerProcessor inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "QoreStandardScalerProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = {
            "fields": <DataProviderOptionInfo>{
                "display_name": "Fields to Scale",
                "short_desc": "Which numeric fields to put on the same scale",
                "desc": "The numeric fields from your data that should be adjusted to a common "
                    "scale. For example, select `age`, `income`, and `credit_score` to make "
                    "them comparable before grouping customers.\n\n"
                    "At least 1 field is required",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", True),
                "required": True,
                "input_field_names": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Batch Size",
                "short_desc": "How many records to collect before processing",
                "desc": "How many records to collect before processing them as a batch. The "
                    "first batch is used to learn the typical range of each field; all "
                    "subsequent batches are adjusted using those learned values.\n\n"
                    "**Tip**: use at least `50` for reliable results; `200`+ if you have "
                    "many fields or noisy data",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "example_value": 100,
            },
        };
    }

    private {
        #! Configured field names
        list<string> fields;

        #! Configured window size
        int window_size;

        #! Retained StandardScaler instance (fitted on first window)
        *ML::StandardScaler scaler;

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
        buffer = ();
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "standard-scaler";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Standardizes numeric features to zero mean and unit variance; "
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
                "QoreStandardScalerProcessor: not configured; provide constructor options";
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

    #! Processes a window of records through StandardScaler
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
            # first window: fit and transform
            scaler = new ML::StandardScaler();
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
            push events, <StandardScalerEventInfo>{
                "event_type": "standard-scaler",
                "record_values": record_values,
                "scaled_values": scaled_values,
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
            TypedHash::forName("DataProviderML::StandardScalerEventInfo"));
    }
}
}
