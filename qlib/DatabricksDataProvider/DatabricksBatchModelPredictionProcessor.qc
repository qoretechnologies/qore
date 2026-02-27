# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksBatchModelPredictionProcessor class definition

/*  DatabricksBatchModelPredictionProcessor.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DatabricksDataProvider module
public namespace DatabricksDataProvider {

#! Pipeline processor that gets batch predictions from a Databricks Model Serving endpoint
/** Accumulates records in a window, sends them as a batch to a Databricks Model Serving
    endpoint using the DataFrame split format, and returns the prediction results.
*/
public class DatabricksBatchModelPredictionProcessor inherits DatabricksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "DatabricksBatchModelPredictionProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_AGGREGATE,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "endpoint_name": <DataProviderOptionInfo>{
                "display_name": "Endpoint Name",
                "short_desc": "Databricks Model Serving endpoint name",
                "desc": "The name of the Databricks Model Serving endpoint to use for predictions",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "input_fields": <DataProviderOptionInfo>{
                "display_name": "Input Fields",
                "short_desc": "Record fields to send as model inputs",
                "desc": "A comma-separated list of field names from the input records to send as "
                    "columns in the DataFrame split format to the serving endpoint",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "window_size": <DataProviderOptionInfo>{
                "display_name": "Window Size",
                "short_desc": "Number of records to batch together",
                "desc": "The number of records to accumulate before sending a batch prediction "
                    "request to the Databricks serving endpoint; larger windows are more efficient "
                    "but use more memory; must be at least 2",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
            },
        };
    }

    private {
        #! The serving endpoint name
        string endpoint_name;

        #! The input field names
        list<string> input_fields;

        #! Window size for batching
        int window_size;

        #! Accumulated records
        list<hash<auto>> buffer;

        #! Whether configured
        bool configured = False;
    }

    #! Creates the object with no options (for data provider navigation)
    constructor() {
    }

    #! Creates the object with the given options
    constructor(hash<auto> options) : DatabricksDataProviderBase(options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        endpoint_name = copts.endpoint_name;
        input_fields = map trim($1), copts.input_fields.split(","), trim($1).val();
        if (!input_fields) {
            throw "CONSTRUCTOR-ERROR", "no valid field names provided in 'input_fields' option";
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
        return "batch-model-predict";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Gets batch predictions from a Databricks Model Serving endpoint; accumulates "
            "records in a window, sends them as a batch in DataFrame split format, and returns "
            "prediction results";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Accumulates records; processes batch when window is full
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "DatabricksBatchModelPredictionProcessor: not configured; provide constructor options";
        }
        push buffer, record;
        if (buffer.lsize() >= window_size) {
            auto result = processWindow(buffer);
            buffer = ();
            return result;
        }
        return;
    }

    #! Flushes remaining buffered records
    private *list<auto> flushRecordsImpl() {
        if (!configured || !buffer) {
            return;
        }
        list<auto> results = processWindow(buffer);
        buffer = ();
        return results;
    }

    #! Processes a window of records as a batch prediction
    private list<auto> processWindow(list<hash<auto>> records) {
        # Build multi-row data array in DataFrame split format
        list<list<auto>> data_rows;
        foreach hash<auto> rec in (records) {
            list<auto> row;
            foreach string f in (input_fields) {
                push row, rec{f};
            }
            push data_rows, row;
        }

        # Call serving endpoint with batched DataFrame split format
        hash<auto> response = doPost(
            sprintf("serving-endpoints/%s/invocations", endpoint_name),
            {
                "dataframe_split": {
                    "columns": input_fields,
                    "data": data_rows,
                },
            },
        );

        # Return a single batch event with all predictions
        return (<DatabricksBatchModelPredictionEventInfo>{
            "event_type": "databricks-batch-model-predict",
            "predictions": response.predictions ?? (),
            "batch_size": records.lsize(),
            "endpoint_name": endpoint_name,
        },);
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
            TypedHash::forName("DatabricksDataProvider::DatabricksBatchModelPredictionEventInfo"));
    }
}

} # namespace DatabricksDataProvider
