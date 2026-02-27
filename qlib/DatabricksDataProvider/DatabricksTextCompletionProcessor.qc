# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksTextCompletionProcessor class definition

/*  DatabricksTextCompletionProcessor.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Pipeline processor that gets text completions from a Databricks Foundation Model endpoint
/** Sends each record's prompt field to a Databricks Foundation Model Serving endpoint
    and returns the text completion result.
*/
public class DatabricksTextCompletionProcessor inherits DatabricksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "DatabricksTextCompletionProcessor",
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
                    "text completions (e.g., `databricks-mpt-7b-instruct`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "prompt_field": <DataProviderOptionInfo>{
                "display_name": "Prompt Field",
                "short_desc": "Record field containing the prompt text",
                "desc": "The name of the field in the input record that contains the prompt text "
                    "to send to the text completion endpoint",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "max_tokens": <DataProviderOptionInfo>{
                "display_name": "Max Tokens",
                "short_desc": "Maximum tokens in the response",
                "desc": "The maximum number of tokens to generate in the completion; default: 256",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "temperature": <DataProviderOptionInfo>{
                "display_name": "Temperature",
                "short_desc": "Sampling temperature (0.0 to 2.0)",
                "desc": "Controls randomness in the model's output; lower values make output more "
                    "deterministic, higher values make it more creative; default: 1.0",
                "type": AbstractDataProviderTypeMap."*float",
            },
        };
    }

    private {
        #! The serving endpoint name
        string endpoint_name;

        #! The prompt field name
        string prompt_field;

        #! Max tokens to generate
        int max_tokens = 256;

        #! Sampling temperature
        float temperature = 1.0;

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
        prompt_field = copts.prompt_field;
        if (exists copts.max_tokens) {
            max_tokens = copts.max_tokens;
        }
        if (exists copts.temperature) {
            temperature = copts.temperature;
        }
        configured = True;
    }

    #! Returns the name of this data provider
    string getName() {
        return "text-completion";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Gets text completions from a Databricks Foundation Model Serving endpoint for "
            "each record in the pipeline; sends the configured prompt field and returns the "
            "completion result";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Processes a single record through the text completion endpoint
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "DatabricksTextCompletionProcessor: not configured; provide constructor options";
        }

        string prompt_value = string(record{prompt_field} ?? "");

        # Call serving endpoint with completion format
        hash<auto> response = doPost(
            sprintf("serving-endpoints/%s/invocations", endpoint_name),
            {
                "prompt": prompt_value,
                "max_tokens": max_tokens,
                "temperature": temperature,
            },
        );

        return <DatabricksTextCompletionEventInfo>{
            "event_type": "databricks-text-completion",
            "completion": response.choices.lsize() ? (response.choices[0].text ?? "") : "",
            "usage": response.usage,
            "endpoint_name": endpoint_name,
            "prompt_value": prompt_value,
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
            TypedHash::forName("DatabricksDataProvider::DatabricksTextCompletionEventInfo"));
    }
}

} # namespace DatabricksDataProvider
