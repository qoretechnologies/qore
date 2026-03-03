# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksChatCompletionProcessor class definition

/*  DatabricksChatCompletionProcessor.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Pipeline processor that gets chat completions from a Databricks Foundation Model endpoint
/** Sends each record's prompt field to a Databricks Foundation Model Serving endpoint
    using the OpenAI-compatible chat completions format and returns the completion result.
*/
public class DatabricksChatCompletionProcessor inherits DatabricksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "DatabricksChatCompletionProcessor",
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
                    "chat completions (e.g., `databricks-meta-llama-3-1-70b-instruct`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "serving_endpoints",
            },
            "prompt_field": <DataProviderOptionInfo>{
                "display_name": "Prompt Field",
                "short_desc": "Record field containing the user prompt",
                "desc": "The name of the field in the input record that contains the user prompt "
                    "text to send to the chat completion endpoint",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "system_prompt": <DataProviderOptionInfo>{
                "display_name": "System Prompt",
                "short_desc": "Optional system prompt for the model",
                "desc": "An optional system prompt to prepend to the conversation; used to set "
                    "the model's behavior and context",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "max_tokens": <DataProviderOptionInfo>{
                "display_name": "Max Tokens",
                "short_desc": "Maximum tokens in the response",
                "desc": "The maximum number of tokens to generate in the chat completion; "
                    "default: 256",
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

        #! Optional system prompt
        *string system_prompt;

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
        system_prompt = copts.system_prompt;
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
        return "chat-completion";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Gets chat completions from a Databricks Foundation Model Serving endpoint for "
            "each record in the pipeline; sends the configured prompt field as a user message "
            "and returns the completion result";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Processes a single record through the chat completion endpoint
    private *auto processRecordImpl(auto record) {
        if (!configured) {
            throw "PROCESSOR-ERROR",
                "DatabricksChatCompletionProcessor: not configured; provide constructor options";
        }

        string prompt_value = string(record{prompt_field} ?? "");

        # Build messages array
        list<hash<auto>> messages;
        if (system_prompt.val()) {
            push messages, {
                "role": "system",
                "content": system_prompt,
            };
        }
        push messages, {
            "role": "user",
            "content": prompt_value,
        };

        # Call serving endpoint with OpenAI-compatible format
        hash<auto> response = doPost(
            sprintf("serving-endpoints/%s/invocations", endpoint_name),
            {
                "messages": messages,
                "max_tokens": max_tokens,
                "temperature": temperature,
            },
        );

        return <DatabricksChatCompletionEventInfo>{
            "event_type": "databricks-chat-completion",
            "completion": response.choices.lsize() ? (response.choices[0].message.content ?? "") : "",
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
            TypedHash::forName("DatabricksDataProvider::DatabricksChatCompletionEventInfo"));
    }
}

} # namespace DatabricksDataProvider
