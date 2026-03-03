# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksModelPredictionProcessor class definition

/*  DatabricksModelPredictionProcessor.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Pipeline processor that gets predictions from a Databricks Model Serving endpoint
/** Sends each record to a Databricks Model Serving endpoint using the DataFrame split
    format and returns the prediction result.
*/
public class DatabricksModelPredictionProcessor inherits DatabricksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "DatabricksModelPredictionProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_TRANSFORM,
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
                "ref_data": "serving_endpoints",
            },
            "input_fields": <DataProviderOptionInfo>{
                "display_name": "Input Fields",
                "short_desc": "Record fields to send as model inputs",
                "desc": "A comma-separated list of field names from the input records to send as "
                    "columns in the DataFrame split format to the serving endpoint",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
        };
    }

    private {
        #! The serving endpoint name
        string endpoint_name;

        #! The input field names
        list<string> input_fields;

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
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "model-predict";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Gets predictions from a Databricks Model Serving endpoint for each record "
            "in the pipeline; sends the configured input fields in DataFrame split format "
            "and returns the prediction result";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Processes a single record through the serving endpoint
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "DatabricksModelPredictionProcessor: not configured; provide constructor options";
        }

        # Extract input fields from the record
        list<auto> data_row;
        hash<auto> record_values;
        foreach string f in (input_fields) {
            push data_row, record{f};
            record_values{f} = record{f};
        }

        # Call serving endpoint with DataFrame split format
        hash<auto> response = doPost(
            sprintf("serving-endpoints/%s/invocations", endpoint_name),
            {
                "dataframe_split": {
                    "columns": input_fields,
                    "data": (data_row,),
                },
            },
        );

        return <DatabricksModelPredictionEventInfo>{
            "event_type": "databricks-model-predict",
            "prediction": response.predictions.lsize() ? response.predictions[0] : NOTHING,
            "record_values": record_values,
            "endpoint_name": endpoint_name,
        };
    }

    #! Does not support bulk API
    private bool supportsBulkApiImpl() {
        return False;
    }

    #! Returns the processor capability bitfield
    private int getProcessorCapabilitiesImpl() {
        return DPC_TRANSFORM;
    }

    #! Returns the output type
    private *DataProvider::AbstractDataProviderType getPipelineProcessorOutputTypeImpl() {
        return new HashDeclDataType(
            TypedHash::forName("DatabricksDataProvider::DatabricksModelPredictionEventInfo"));
    }
}

} # namespace DatabricksDataProvider
