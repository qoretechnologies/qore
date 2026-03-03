# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksTextEmbeddingsProcessor class definition

/*  DatabricksTextEmbeddingsProcessor.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Pipeline processor that gets text embeddings from a Databricks Foundation Model endpoint
/** Sends each record's text field to a Databricks Foundation Model Serving endpoint
    and returns the embedding vector.
*/
public class DatabricksTextEmbeddingsProcessor inherits DatabricksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "DatabricksTextEmbeddingsProcessor",
            "supports_pipeline_processor": True,
            "processor_capabilities": DPC_TRANSFORM,
            "pipeline_processor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "endpoint_name": <DataProviderOptionInfo>{
                "display_name": "Endpoint Name",
                "short_desc": "Databricks Foundation Model Serving endpoint name",
                "desc": "The name of the Databricks Foundation Model Serving endpoint to use for "
                    "text embeddings (e.g., `databricks-bge-large-en`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "serving_endpoints",
            },
            "text_field": <DataProviderOptionInfo>{
                "display_name": "Text Field",
                "short_desc": "Record field containing the text to embed",
                "desc": "The name of the field in the input record that contains the text to "
                    "generate embeddings for",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
        };
    }

    private {
        #! The serving endpoint name
        string endpoint_name;

        #! The text field name
        string text_field;

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
        text_field = copts.text_field;
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "text-embeddings";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Gets text embeddings from a Databricks Foundation Model Serving endpoint for "
            "each record in the pipeline; sends the configured text field and returns the "
            "embedding vector";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Processes a single record through the embeddings endpoint
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "DatabricksTextEmbeddingsProcessor: not configured; provide constructor options";
        }

        string text_value = string(record{text_field} ?? "");

        # Call serving endpoint with embeddings format
        hash<auto> response = doPost(
            sprintf("serving-endpoints/%s/invocations", endpoint_name),
            {
                "input": text_value,
            },
        );

        return <DatabricksTextEmbeddingsEventInfo>{
            "event_type": "databricks-text-embeddings",
            "embedding": response.data.lsize() ? (response.data[0].embedding ?? ()) : (),
            "usage": response.usage,
            "endpoint_name": endpoint_name,
            "text_value": text_value,
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
            TypedHash::forName("DatabricksDataProvider::DatabricksTextEmbeddingsEventInfo"));
    }
}

} # namespace DatabricksDataProvider
