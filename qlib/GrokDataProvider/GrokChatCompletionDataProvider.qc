# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GrokDataProvider module definition

/** GrokChatCompletionDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the GrokDataProvider module
public namespace GrokDataProvider {
#! The Grok chat completion data provider class
public class GrokChatCompletionDataProvider inherits GrokDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "multi-chat",
            "type": "GrokChatCompletionDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = GrokChatCompletionRequestType;

        #! Response type
        const ResponseType = GrokChatCompletionResponseType;
    }

    #! Creates the object from the arguments
    constructor(*RestClientIo::RestClientIo rest) : GrokDataProviderCommon(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("POST", "chat/completions", req).body;
    }

    #! Returns the description of a successful request message, if any
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Chat completion request message
public class GrokMessageDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "role": {
                "display_name": "Role",
                "short_desc": "The role of the author of this message",
                "desc": "The role of the author of this message",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "User",
                        "short_desc": "The author of the message is the user",
                        "desc": "The author of the message is the user",
                        "value": "user",
                    },
                    <AllowedValueInfo>{
                        "display_name": "System",
                        "short_desc": "The author of the message is the system",
                        "desc": "The author of the message is the system",
                        "value": "system",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Assistant",
                        "short_desc": "The author of the message is an assistant",
                        "desc": "The author of the message is an assistant",
                        "value": "assistant",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Tool",
                        "short_desc": "The author of the message is a tool",
                        "desc": "The author of the message is a tool",
                        "value": "tool",
                    },
                ),
                "default_value": "user",
            },
            "content": {
                "display_name": "Content",
                "short_desc": "The contents of the message",
                "desc": "The contents of the message",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "What is the capital of France?",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "An optional name for the participant",
                "desc": "An optional name for the participant. Provides the model information to differentiate "
                    "between participants of the same role",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "tool_calls": {
                "display_name": "Tool Calls",
                "short_desc": "The tool calls generated by the model, such as function calls",
                "desc": "The tool calls generated by the model, such as function calls",
                "type": new ListDataType("*GrokToolCalls", GrokToolCallDataType, True),
            },
            "tool_call_id": {
                "display_name": "Tool Call ID",
                "short_desc": "Tool call that this message is responding to",
                "desc": "Tool call that this message is responding to",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("GrokMessage") {
        addQoreFields(Fields);
    }
}

#! Chat completion request message constant
public const GrokMessageDataType = new GrokMessageDataType();

#! The chat response format data type
public class GrokChatResponseFormatType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "type": {
                "display_name": "Type",
                "short_desc": "The response type",
                "desc": "The response type",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Text",
                        "short_desc": "Specifies standard text output",
                        "desc": "Specifies standard text output",
                        "value": "text",
                    },
                    <AllowedValueInfo>{
                        "display_name": "JSON Object",
                        "short_desc": "Specifies that the model must produce valid JSON on output",
                        "desc": "Specifies that the model must produce valid JSON on output",
                        "value": "json_object",
                    },
                ),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("GrokChatResponseFormat") {
        addQoreFields(Fields);
    }
}

#! Constant for the chat response format data type
public const GrokChatResponseFormatType = new GrokChatResponseFormatType();

#! The stream options data type
public class GrokStreamOptionsType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "include_usage": {
                "display_name": "Include Usage?",
                "short_desc": "Enables additional chunks providing usage information",
                "desc": "If set, an additional chunk will be streamed before the `data: [DONE]` message. "
                    "The usage field on this chunk shows the token usage statistics for the entire request",
                "type": AbstractDataProviderTypeMap."*bool",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("GrokStreamOptions") {
        addQoreFields(Fields);
    }
}

#! Constant for the stream options data type
public const GrokStreamOptionsType = new GrokStreamOptionsType();

#! The function data type
public class GrokFunctionDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "description": {
                "display_name": "Description",
                "short_desc": "A description of what the function does",
                "desc": "A description of what the function does, used by the model to choose when and how to call "
                    "the function",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "The name of the function to be called",
                "desc": "The name of the function to be called. Must be a-z, A-Z, 0-9, or contain underscores and "
                    "dashes, with a maximum length of 64",
                "type": AbstractDataProviderTypeMap."string",
            },
            "parameters": {
                "display_name": "Parameters",
                "short_desc": "The parameters the function accepts as a JSON Schema object",
                "desc": "The parameters the function accepts, described as a JSON Schema object.\n\n"
                    "**Example**: `{\"type\": \"object\", \"properties\": {\"location\": {\"type\": \"string\", "
                    "\"description\": \"City name\"}, \"unit\": {\"type\": \"string\", \"enum\": [\"celsius\", "
                    "\"fahrenheit\"]}}, \"required\": [\"location\"]}`",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("GrokFunction") {
        addQoreFields(Fields);
    }
}

#! Constant for the function data type
public const GrokFunctionDataType = new GrokFunctionDataType();

#! The tool data type
public class GrokToolDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "type": {
                "display_name": "Type",
                "short_desc": "The type of the tool",
                "desc": "The type of the tool. Currently, only `function` is supported",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Function",
                        "short_desc": "For a function",
                        "desc": "For a function",
                        "value": "function",
                    },
                ),
                "default_value": "function",
            },
            "function": {
                "display_name": "Function",
                "short_desc": "Describes the function to be called",
                "desc": "Describes the function to be called",
                "type": GrokFunctionDataType,
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("GrokTool") {
        addQoreFields(Fields);
    }
}

#! Constant for the tool data type
public const GrokToolDataType = new GrokToolDataType();

#! The search parameters data type
public class GrokSearchParametersType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "mode": {
                "display_name": "Search Mode",
                "short_desc": "Web search behavior",
                "desc": "Web search behavior mode",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Off",
                        "short_desc": "Disable web search",
                        "desc": "Disable web search",
                        "value": "off",
                    },
                    <AllowedValueInfo>{
                        "display_name": "On",
                        "short_desc": "Enable web search",
                        "desc": "Enable web search",
                        "value": "on",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Auto",
                        "short_desc": "Let the model decide when to search",
                        "desc": "Let the model decide when to search",
                        "value": "auto",
                    },
                ),
            },
            "sources": {
                "display_name": "Sources",
                "short_desc": "Specific sources to query",
                "desc": "Specific sources to query for web search",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "max_search_results": {
                "display_name": "Max Search Results",
                "short_desc": "Maximum number of search results per query",
                "desc": "Maximum number of search results per query",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "return_citations": {
                "display_name": "Return Citations",
                "short_desc": "Include source citations in response",
                "desc": "Include source citations in the response",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "from_date": {
                "display_name": "From Date",
                "short_desc": "Start date for search results (ISO-8601)",
                "desc": "Start date for search results in ISO-8601 format (YYYY-MM-DD)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "to_date": {
                "display_name": "To Date",
                "short_desc": "End date for search results (ISO-8601)",
                "desc": "End date for search results in ISO-8601 format (YYYY-MM-DD)",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("GrokSearchParameters") {
        addQoreFields(Fields);
    }
}

#! Constant for the search parameters data type
public const GrokSearchParametersType = new GrokSearchParametersType();

#! The chat completion request type
public class GrokChatCompletionRequestType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "messages": {
                "display_name": "Messages",
                "short_desc": "A list of messages comprising the conversation so far",
                "desc": "A list of messages comprising the conversation so far",
                "type": new ListDataType("GrokMessages", GrokMessageDataType),
            },
            "model": {
                "display_name": "Model ID",
                "short_desc": "Model to use to create the response",
                "desc": "Model to use to create the response (e.g., `grok-3`, `grok-4`)",
                "type": AbstractDataProviderTypeMap."string",
                "attr": {
                    "ref_data": "models",
                },
            },
            "frequency_penalty": {
                "display_name": "Frequency Penalty",
                "short_desc": "Penalizes token repetition (-2.0 to 2.0)",
                "desc": "Number between -2.0 and 2.0. Positive values penalize new tokens based on their existing "
                    "frequency in the text so far, decreasing the model's likelihood to repeat the same line "
                    "verbatim",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "logprobs": {
                "display_name": "Log Probability",
                "short_desc": "Whether to return log probabilities of output tokens",
                "desc": "Whether to return log probabilities of the output tokens. If `true`, returns the log "
                    "probabilities of each output token returned in the content of the message",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "top_logprobs": {
                "display_name": "Top Log Probability",
                "short_desc": "Number of most likely tokens to return at each position (0-8)",
                "desc": "An integer between 0 and 8 specifying the number of most likely tokens to return at each "
                    "token position. `logprobs` must be set to `true` if this parameter is used",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "max_completion_tokens": {
                "display_name": "Max Completion Tokens",
                "short_desc": "Upper bound for tokens generated in the response",
                "desc": "An upper bound for the number of tokens that can be generated for a completion, including "
                    "visible output tokens and reasoning tokens",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "max_tokens": {
                "display_name": "Max Tokens (Deprecated)",
                "short_desc": "The maximum number of tokens to generate (deprecated; use max_completion_tokens)",
                "desc": "The maximum number of tokens that can be generated in the chat completion. Deprecated; "
                    "use max_completion_tokens instead",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "n": {
                "display_name": "Number of Choices",
                "short_desc": "The number of choices to generate for each input message",
                "desc": "How many chat completion choices to generate for each input message. Note that you will be "
                    "charged based on the number of generated tokens across all of the choices",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "presence_penalty": {
                "display_name": "Presence Penalty",
                "short_desc": "Penalizes new tokens based on appearance in text (-2.0 to 2.0)",
                "desc": "Number between -2.0 and 2.0. Positive values penalize new tokens based on whether they "
                    "appear in the text so far, increasing the model's likelihood to talk about new topics",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "logit_bias": {
                "display_name": "Logit Bias",
                "short_desc": "Modify likelihood of specified tokens appearing in the completion",
                "desc": "Modify the likelihood of specified tokens appearing in the completion. Maps token IDs "
                    "(as strings) to bias values from `-100` to `100`. Values of `-100` or `100` result in a "
                    "ban or exclusive selection of the relevant token.\n\n"
                    "**Example**: `{\"50256\": -100}` to prevent the end-of-text token",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "reasoning_effort": {
                "display_name": "Reasoning Effort",
                "short_desc": "Reasoning model constraint level",
                "desc": "Constrains effort on reasoning for reasoning models. Only supported by reasoning-capable "
                    "models",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Low",
                        "short_desc": "Low reasoning effort",
                        "desc": "Low reasoning effort",
                        "value": "low",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Medium",
                        "short_desc": "Medium reasoning effort",
                        "desc": "Medium reasoning effort",
                        "value": "medium",
                    },
                    <AllowedValueInfo>{
                        "display_name": "High",
                        "short_desc": "High reasoning effort",
                        "desc": "High reasoning effort",
                        "value": "high",
                    },
                ),
            },
            "response_format": {
                "display_name": "Response Format",
                "short_desc": "The response format",
                "desc": "An object specifying the format that the model must output. Setting to "
                    "`{\"type\": \"json_object\"}` enables JSON mode, which guarantees the message the model "
                    "generates is valid JSON",
                "type": GrokChatResponseFormatType.getOrNothingType(),
            },
            "search_parameters": {
                "display_name": "Search Parameters",
                "short_desc": "Web search configuration",
                "desc": "Configuration for web search integration with date ranges, sources, and citation options",
                "type": GrokSearchParametersType.getOrNothingType(),
            },
            "seed": {
                "display_name": "Seed",
                "short_desc": "A value for deterministic sampling",
                "desc": "If specified, the system will make a best effort to sample deterministically, such that "
                    "repeated requests with the same seed and parameters should return the same result",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "stop": {
                "display_name": "Stop Sequences",
                "short_desc": "Up to 4 sequences where the API will stop generating further tokens",
                "desc": "Up to 4 sequences where the API will stop generating further tokens",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "stream": {
                "display_name": "Stream?",
                "short_desc": "Set to enable partial message deltas via SSE",
                "desc": "If `true`, partial message deltas will be sent as data-only server-sent events as they "
                    "become available, with the stream terminated by a `data: [DONE]` message",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "stream_options": {
                "display_name": "Stream Options",
                "short_desc": "Options for streaming response",
                "desc": "Options for streaming response. Only set this when you set `stream: true`",
                "type": GrokStreamOptionsType.getOrNothingType(),
            },
            "temperature": {
                "display_name": "Sampling Temperature",
                "short_desc": "The sampling temperature to use (0-2)",
                "desc": "What sampling temperature to use, between 0 and 2. Higher values like 0.8 will make "
                    "the output more random, while lower values like 0.2 will make it more focused and "
                    "deterministic",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "top_p": {
                "display_name": "Top P",
                "short_desc": "Nucleus sampling probability mass (0-1)",
                "desc": "An alternative to sampling with temperature, called nucleus sampling, where the model "
                    "considers the results of the tokens with top_p probability mass",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "tools": {
                "display_name": "Tools",
                "short_desc": "A list of tools the model may call (max 128)",
                "desc": "A list of tools the model may call. Currently, only functions are supported as a tool. "
                    "A max of 128 functions are supported",
                "type": new ListDataType("*GrokToolList", GrokToolDataType, True),
            },
            "tool_choice": {
                "display_name": "Tool Choice",
                "short_desc": "Controls which tool is called by the model",
                "desc": "Controls which (if any) tool is called by the model. `none` is the default when no tools "
                    "are present. `auto` is the default if tools are present",
                "type": AbstractDataProviderTypeMap."any",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "None",
                        "short_desc": "The model will not call any tool",
                        "desc": "The model will not call any tool and instead generates a message",
                        "value": "none",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Auto",
                        "short_desc": "The model can pick between generating a message or calling tools",
                        "desc": "The model can pick between generating a message or calling one or more tools",
                        "value": "auto",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Required",
                        "short_desc": "The model must call one or more tools",
                        "desc": "The model must call one or more tools",
                        "value": "required",
                    },
                ),
            },
            "parallel_tool_calls": {
                "display_name": "Parallel Tool Calls",
                "short_desc": "Enable simultaneous tool calls",
                "desc": "Whether to enable parallel function calling during tool use",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "deferred": {
                "display_name": "Deferred",
                "short_desc": "Return request_id for asynchronous retrieval",
                "desc": "If `true`, returns a request_id that can be used to poll for the completion result via "
                    "the deferred completion endpoint",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "user": {
                "display_name": "User",
                "short_desc": "A unique identifier representing your end-user",
                "desc": "A unique identifier representing your end-user for abuse monitoring",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("GrokChatCompletionRequest") {
        addQoreFields(Fields);
    }
}

#! The chat completion request type constant
public const GrokChatCompletionRequestType = new GrokChatCompletionRequestType();

#! Tool call function data type
public class GrokToolCallFunctionDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "name": {
                "display_name": "Name",
                "short_desc": "The name of the function called",
                "desc": "The name of the function called by the model",
                "type": AbstractDataProviderTypeMap."string",
            },
            "arguments": {
                "display_name": "Arguments",
                "short_desc": "The arguments passed to the function as JSON",
                "desc": "The arguments passed to the function, serialized as a JSON string",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("GrokToolCallFunction") {
        addQoreFields(Fields);
    }

    #! Creates the type with a specific base type
    constructor(Type base_type) : HashDataType(base_type, "GrokToolCallFunction") {
        addQoreFields(Fields);
    }
}

#! Constant for tool call function data type
public const GrokToolCallFunctionDataType = new GrokToolCallFunctionDataType();

#! Tool call data type
public class GrokToolCallDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "The ID of the tool call",
                "desc": "The ID of the tool call",
                "type": AbstractDataProviderTypeMap."string",
            },
            "type": {
                "display_name": "Type",
                "short_desc": "The type of the tool",
                "desc": "The type of the tool",
                "type": AbstractDataProviderTypeMap."string",
            },
            "function": {
                "display_name": "Function",
                "short_desc": "The function that the model called",
                "desc": "The function that the model called",
                "type": new GrokToolCallFunctionDataType(AutoHashOrNothingType),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("GrokToolCall") {
        addQoreFields(Fields);
    }
}

#! Constant for tool call data type
public const GrokToolCallDataType = new GrokToolCallDataType();

#! Chat completion response message
public class GrokCompletionMessageDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "role": {
                "display_name": "Role",
                "short_desc": "The role of the author",
                "desc": "The role of the author of this message",
                "type": AbstractDataProviderTypeMap."string",
            },
            "content": {
                "display_name": "Content",
                "short_desc": "The contents of the message",
                "desc": "The contents of the message",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "reasoning_content": {
                "display_name": "Reasoning Content",
                "short_desc": "Reasoning trace from the model",
                "desc": "Reasoning trace from the model when using reasoning-capable models",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "refusal": {
                "display_name": "Refusal",
                "short_desc": "Refusal reason if unable to respond",
                "desc": "Refusal reason if the model was unable to respond",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "tool_calls": {
                "display_name": "Tool Calls",
                "short_desc": "The tool calls generated by the model",
                "desc": "The tool calls generated by the model, such as function calls",
                "type": new ListDataType("*GrokResponseToolCalls", GrokToolCallDataType, True),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("GrokCompletionMessage") {
        addQoreFields(Fields);
    }
}

#! Constant for completion message data type
public const GrokCompletionMessageDataType = new GrokCompletionMessageDataType();

#! Token logprob data type
public class GrokTokenLogprobDataType inherits HashDataType {
    public {
        const Fields = {
            "token": {
                "display_name": "Token",
                "short_desc": "The token string",
                "desc": "The token string",
                "type": AbstractDataProviderTypeMap."string",
            },
            "logprob": {
                "display_name": "Log Probability",
                "short_desc": "The log probability of the token",
                "desc": "The log probability of the token",
                "type": AbstractDataProviderTypeMap."float",
            },
            "bytes": {
                "display_name": "Bytes",
                "short_desc": "UTF-8 byte representation of the token",
                "desc": "A list of integers representing the UTF-8 bytes of the token",
                "type": AbstractDataProviderTypeMap."*list",
            },
            "top_logprobs": {
                "display_name": "Top Log Probabilities",
                "short_desc": "Most likely tokens at this position",
                "desc": "List of the most likely tokens and their log probabilities at this position",
                "type": AbstractDataProviderTypeMap."*list",
            },
        };
    }

    constructor() : HashDataType("GrokTokenLogprob") {
        addQoreFields(Fields);
    }
}

public const GrokTokenLogprobDataType = new GrokTokenLogprobDataType();

#! Logprobs data type for chat completion choices
public class GrokLogprobsDataType inherits HashDataType {
    public {
        const Fields = {
            "content": {
                "display_name": "Content",
                "short_desc": "Log probabilities for output tokens",
                "desc": "A list of token log probability objects for each output token",
                "type": new ListDataType("*GrokTokenLogprobs", GrokTokenLogprobDataType, True),
            },
            "refusal": {
                "display_name": "Refusal",
                "short_desc": "Log probabilities for refusal tokens",
                "desc": "A list of token log probability objects for refusal tokens, if applicable",
                "type": new ListDataType("*GrokRefusalLogprobs", GrokTokenLogprobDataType, True),
            },
        };
    }

    constructor() : HashDataType("GrokLogprobs") {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type, "GrokLogprobs") {
        addQoreFields(Fields);
    }
}

public const GrokLogprobsDataType = new GrokLogprobsDataType();

#! Chat completion response choice type
public class GrokChoiceDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "index": {
                "display_name": "Index",
                "short_desc": "The index of the chat completion choice",
                "desc": "The index of the chat completion choice",
                "type": AbstractDataProviderTypeMap."int",
            },
            "message": {
                "display_name": "Message",
                "short_desc": "The assistant message",
                "desc": "The assistant message content",
                "type": GrokCompletionMessageDataType,
            },
            "logprobs": {
                "display_name": "Log Probability",
                "short_desc": "Log probability information for the message",
                "desc": "Log probability information for the message",
                "type": new GrokLogprobsDataType(AutoHashOrNothingType),
            },
            "finish_reason": {
                "display_name": "Finish Reason",
                "short_desc": "The reason the model stopped generating tokens",
                "desc": "The reason the model stopped generating tokens: `stop`, `length`, `end_turn`, or "
                    "`tool_calls`",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("GrokChoice") {
        addQoreFields(Fields);
    }
}

#! Constant for the choice data type
public const GrokChoiceDataType = new GrokChoiceDataType();

#! The chat completion usage type
public class GrokChatCompletionUsageDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "prompt_tokens": {
                "display_name": "Prompt Tokens",
                "short_desc": "Number of tokens in the prompt",
                "desc": "Number of tokens in the prompt",
                "type": AbstractDataProviderTypeMap."int",
            },
            "completion_tokens": {
                "display_name": "Completion Tokens",
                "short_desc": "Number of tokens in the generated completion",
                "desc": "Number of tokens in the generated completion",
                "type": AbstractDataProviderTypeMap."int",
            },
            "total_tokens": {
                "display_name": "Total Tokens",
                "short_desc": "Total number of tokens used in the request",
                "desc": "Total number of tokens used in the request (prompt + completion)",
                "type": AbstractDataProviderTypeMap."int",
            },
            "prompt_tokens_details": {
                "display_name": "Prompt Tokens Details",
                "short_desc": "Detailed breakdown of prompt tokens",
                "desc": "Detailed breakdown of the number of tokens in the prompt including cached tokens",
                "type": new GrokInputTokenDetailsDataType(AutoHashOrNothingType),
            },
            "completion_tokens_details": {
                "display_name": "Completion Tokens Details",
                "short_desc": "Detailed breakdown of completion tokens",
                "desc": "Detailed breakdown of the number of tokens in the completion including reasoning tokens",
                "type": new GrokOutputTokenDetailsDataType(AutoHashOrNothingType),
            },
            "cost_in_usd_ticks": {
                "display_name": "Cost in USD Ticks",
                "short_desc": "Billing cost in ticks (10 billion per dollar)",
                "desc": "Billing cost in USD ticks (10 billion ticks per dollar)",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "num_sources_used": {
                "display_name": "Sources Used",
                "short_desc": "Number of live search sources utilized",
                "desc": "Number of live search sources utilized",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("GrokChatCompletionUsage") {
        addQoreFields(Fields);
    }
}

#! Constant for the usage data type
public const GrokChatCompletionUsageDataType = new GrokChatCompletionUsageDataType();

#! The chat completion response type
public class GrokChatCompletionResponseType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "The ID of the response",
                "desc": "The unique identifier of the chat completion response",
                "type": AbstractDataProviderTypeMap."string",
            },
            "object": {
                "display_name": "Object",
                "short_desc": "The type of response",
                "desc": "The type of response (always `chat.completion`)",
                "type": AbstractDataProviderTypeMap."string",
            },
            "created": {
                "display_name": "Creation Timestamp",
                "short_desc": "Unix timestamp of creation",
                "desc": "The Unix timestamp when the response was created",
                "type": AbstractDataProviderTypeMap."int",
            },
            "model": {
                "display_name": "Model ID",
                "short_desc": "Model used to create the response",
                "desc": "Model used to create the response",
                "type": AbstractDataProviderTypeMap."string",
            },
            "system_fingerprint": {
                "display_name": "System Fingerprint",
                "short_desc": "Backend configuration identifier",
                "desc": "A code for the configuration of the backend",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "choices": {
                "display_name": "Choices",
                "short_desc": "A list of chat completion choices",
                "desc": "A list of chat completion choices",
                "type": new ListDataType("GrokChoicesList", GrokChoiceDataType),
            },
            "usage": {
                "display_name": "Usage",
                "short_desc": "Usage statistics for the completion request",
                "desc": "Usage statistics for the completion request",
                "type": GrokChatCompletionUsageDataType,
            },
            "citations": {
                "display_name": "Citations",
                "short_desc": "External sources used in the response",
                "desc": "A list of source URLs used by the model when web search is enabled",
                "type": new ListDataType("*GrokCitationUrls", AbstractDataProviderTypeMap."string", True),
            },
            "output_files": {
                "display_name": "Output Files",
                "short_desc": "Files generated by the model",
                "desc": "A list of files generated by the model, if any",
                "type": AbstractDataProviderTypeMap."*list",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("GrokChatCompletionResponse") {
        addQoreFields(Fields);
    }
}

#! The chat completion response type constant
public const GrokChatCompletionResponseType = new GrokChatCompletionResponseType();
}
