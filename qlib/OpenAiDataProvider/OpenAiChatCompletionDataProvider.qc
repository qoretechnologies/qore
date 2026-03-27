# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenAiDataProvider module definition

/** OpenAiChatCompletionDataProvider.qc Copyright 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the OpenAiDataProvider module
public namespace OpenAiDataProvider {
#! The OpenAi data provider class
public class OpenAiChatCompletionDataProvider inherits OpenAiDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "chat",
            "type": "OpenAiChatCompletionDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = OpenAiChatCompletionRequestType;

        #! Response type
        const ResponseType = OpenAiChatCompletionResponseType;
    }

    #! Creates the object from the arguments
    constructor(*RestClientIo::RestClientIo rest) : OpenAiDataProviderCommon(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("POST", "chat/completions", req).body;
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Chat completion request message
public class OpenAiMessageDataType inherits HashDataType {
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
                    <AllowedValueInfo>{
                        "display_name": "Function",
                        "short_desc": "The author of the message is a function",
                        "desc": "The author of the message is a function",
                        "value": "function",
                    },
                ),
                "default_value": "user",
            },
            "content": {
                "display_name": "Content",
                "short_desc": "The contents of the message",
                "desc": "The contents of the message",
                "type": AbstractDataProviderTypeMap."*string",
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
                "type": new ListDataType("*ToolCalls", OpenAiToolCallDataType, True),
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
    constructor() : HashDataType("OpenAiMessage") {
        addQoreFields(Fields);
    }
}

#! Chat completion reqiest message
public const OpenAiMessageDataType = new OpenAiCompletionMessageDataType();

#! The chat response data type
public class OpenAiChatResponseFormatType inherits HashDataType {
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
    constructor() : HashDataType("OpenAiChatResponse") {
        addQoreFields(Fields);
    }
}

#! Constant for the chat response data type
public const OpenAiChatResponseFormatType = new OpenAiChatResponseFormatType();

#! The stream options data type
public class OpenAiStreamOptionsType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "include_usage": {
                "display_name": "Include Usage?",
                "short_desc": "Enables additional chunks providing usage information",
                "desc": "If set, an additional chunk will be streamed before the data: `[DONE]` message. The usage "
                    "field on this chunk shows the token usage statistics for the entire request, and the choices "
                    "field will always be an empty array. All other chunks will also include a `usage` field, but with "
                    "a `null` value",
                "type": AbstractDataProviderTypeMap."*bool",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiStreamOptions") {
        addQoreFields(Fields);
    }
}

#! Constant for the stream options data type
public const OpenAiStreamOptionsType = new OpenAiStreamOptionsType();

#! The function data type
public class OpenAiFunctionDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "description": {
                "display_name": "Description",
                "short_desc": "A description of what the function does, used by the model to choose when and how to "
                    "call the function",
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
                "short_desc": "",
                "desc": "The parameters the functions accepts, described as a JSON Schema object. See the guide for "
                    "examples, and the JSON Schema reference for documentation about the format.\n\n"
                    "Omitting parameters defines a function with an empty parameter list",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiFunction") {
        addQoreFields(Fields);
    }
}

#! Constant for the function data type
public const OpenAiFunctionDataType = new OpenAiFunctionDataType();

#! The tool data type
public class OpenAiToolDataType inherits HashDataType {
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
                "type": OpenAiFunctionDataType,
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiTool") {
        addQoreFields(Fields);
    }
}

#! Constant for the tool data type
public const OpenAiToolDataType = new OpenAiToolDataType();

#! The chat completion request type
public class OpenAiChatCompletionRequestType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "messages": {
                "display_name": "Messages",
                "short_desc": "A list of messages comprising the conversation so far",
                "desc": "A list of messages comprising the conversation so far",
                "type": new ListDataType("Messages", OpenAiMessageDataType),
            },
            "model": {
                "display_name": "Model ID",
                "short_desc": "Model to use to create the response",
                "desc": "Model to use to create the response",
                "type": AbstractDataProviderTypeMap."string",
                # for dynamic lookups of allowed values
                "attr": {
                    "ref_data": "models",
                },
            },
            "frequency_penalty": {
                "display_name": "Frequency Penalty",
                "short_desc": "Penalty to reduce the chance that tokens will be repeated in the output",
                "desc": "Number between -2.0 and 2.0. Positive values penalize new tokens based on their existing "
                    "frequency in the text so far, decreasing the model's likelihood to repeat the same line "
                    "verbatim",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "logit_bias": {
                "display_name": "Logit Bias",
                "short_desc": "Modify the likelihood of specified tokens appearing in the completion",
                "desc": "Accepts a JSON object that maps tokens (specified by their token ID in the tokenizer) to an "
                    "associated bias value from `-100` to `100`. Mathematically, the bias is added to the logits "
                    "generated by the model prior to sampling. The exact effect will vary per model, but values "
                    "between `-1` and `1` should decrease or increase likelihood of selection; values like `-100` "
                    "or `100` should result in a ban or exclusive selection of the relevant token",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "logprobs": {
                "display_name": "Log Probability",
                "short_desc": "Log probability information for the message",
                "desc": "Whether to return log probabilities of the output tokens or not. If `true`, returns the log "
                    "probabilities of each output token returned in the `content` of the message.",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "top_logprobs": {
                "display_name": "Top Log Probability",
                "short_desc": "The number of most likely tokens to return at each token position",
                "desc": "An integer between 0 and 20 specifying the number of most likely tokens to return at each "
                    "token position, each with an associated log probability. `logprobs` must be set to true if this "
                    "parameter is used",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "max_tokens": {
                "display_name": "Top Log Probability",
                "short_desc": "The maximum number of tokens that can be generated in the chat completion",
                "desc": "The maximum number of tokens that can be generated in the chat completion",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "n": {
                "display_name": "Number of Choices",
                "short_desc": "The number of choices to generate for each input message",
                "desc": "How many chat completion choices to generate for each input message. Note that you will be "
                    "charged based on the number of generated tokens across all of the choices. Keep `n` as `1` (the "
                    "default) to minimize costs",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "presence_penalty": {
                "display_name": "Presence Penalty",
                "short_desc": "Positive numbers increase the model's likelihood to talk about new topics",
                "desc": "Number between -2.0 and 2.0. Positive values penalize new tokens based on whether they "
                    "appear in the text so far, increasing the model's likelihood to talk about new topics",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "response_format": {
                "display_name": "Response Format",
                "short_desc": "The response format",
                "desc": "An object specifying the format that the model must output. Compatible with GPT-4 Turbo and "
                    "all GPT-3.5 Turbo models newer than gpt-3.5-turbo-1106.\n\n"
                    "Setting to `{\"type\": \"json_object\"}` enables JSON mode, which guarantees the message the "
                    "model generates is valid JSON.\n\n"
                    "**Important**: when using JSON mode, you must also instruct the model to produce JSON yourself via "
                    "a system or user message. Without this, the model may generate an unending stream of whitespace "
                    "until the generation reaches the token limit, resulting in a long-running and seemingly "
                    "\"stuck\" request. Also note that the message content may be partially cut off if "
                    "`finish_reason=\"length\"`, which indicates the generation exceeded `max_tokens` or the "
                    "conversation exceeded the max context length",
                "type": OpenAiChatResponseFormatType.getOrNothingType(),
            },
            "seed": {
                "display_name": "Seed",
                "short_desc": "A value that helps ensure repeated requests get the same response",
                "desc": "This feature is in Beta. If specified, our system will make a best effort to sample "
                    "deterministically, such that repeated requests with the same seed and parameters should return "
                    "the same result. Determinism is not guaranteed, and you should refer to the "
                    "`system_fingerprint` response parameter to monitor changes in the backend",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "stop": {
                "display_name": "Stop Sequences",
                "short_desc": "Up to 4 sequences where the API will stop generating further tokens",
                "desc": "Up to 4 sequences where the API will stop generating further tokens",
                "type": AbstractDataProviderTypeMap."any",
            },
            "stream": {
                "display_name": "Stream?",
                "short_desc": "Set to enable partial message deltas",
                "desc": "If set, partial message deltas will be sent, like in ChatGPT. Tokens will be sent as "
                    "data-only server-sent events as they become available, with the stream terminated by a "
                    "`data: [DONE]` message",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "stream_options": {
                "display_name": "Stream Options",
                "short_desc": "Options for streaming response",
                "desc": "Options for streaming response. Only set this when you set `stream: true`",
                "type": OpenAiStreamOptionsType.getOrNothingType(),
            },
            "temperature": {
                "display_name": "Sampling Temperature",
                "short_desc": "The sampling temperature to use, between 0 and 2",
                "desc": "What sampling temperature to use, between `0` and `2`. Higher values like `0.8` will make "
                    "the output more random, while lower values like `0.2` will make it more focused and "
                    "deterministic.\n\n"
                    "We generally recommend altering this or `top_p` but not both",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "top_p": {
                "display_name": "Top P",
                "short_desc": "Top p nucleus sampling value",
                "desc": "An alternative to sampling with temperature, called nucleus sampling, where the model "
                    "considers the results of the tokens with `top_p` probability mass. So `0.1` means only the "
                    "tokens comprising the top 10% probability mass are considered.\n\n"
                    "We generally recommend altering this or `temperature` but not both",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "tools": {
                "display_name": "Tools",
                "short_desc": "A list of tools the model may call",
                "desc": "A list of tools the model may call. Currently, only functions are supported as a tool. Use "
                    "this to provide a list of functions the model may generate JSON inputs for. A max of `128` "
                    "functions are supported",
                "type": new ListDataType("*ToolList", OpenAiToolDataType, True),
            },
            "tool_choice": {
                "display_name": "Tool Choice",
                "short_desc": "",
                "desc": "Controls which (if any) tool is called by the model.\n"
                    "Specifying a particular tool via "
                    "`{\"type\": \"function\", \"function\": {\"name\": \"my_function\"}}` forces the model to call "
                    "that tool.\n\n"
                    "`none` is the default when no tools are present. `auto` is the default if tools are present",
                "type": AbstractDataProviderTypeMap."any",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "None",
                        "short_desc": "The model will not call any tool and instead generates a message",
                        "desc": "The model will not call any tool and instead generates a message",
                        "value": "none",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Auto",
                        "short_desc": "The model can pick between generating a message or calling one or more tools",
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
            "user": {
                "display_name": "User",
                "short_desc": "A unique identifier representing your end-user",
                "desc": "A unique identifier representing your end-user, which can help OpenAI to monitor and detect abuse",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiChatCompletionRequest") {
        addQoreFields(Fields);
    }
}

#! The chat completion request type constant
public const OpenAiChatCompletionRequestType = new OpenAiChatCompletionRequestType();

#! Chat completion response choices
public class OpenAiToolCallDataType inherits HashDataType {
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
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiToolCall") {
        addQoreFields(Fields);
    }
}

#! Chat completion response choices
public const OpenAiToolCallDataType = new OpenAiToolCallDataType();

#! Chat completion response message
public class OpenAiCompletionMessageDataType inherits HashDataType {
    public {
        #! fields
        const Fields = OpenAiMessageDataType::Fields{"role", "content", "tool_calls"};
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiCompletionMessage") {
        addQoreFields(Fields);
    }
}

#! Chat completion response message
public const OpenAiCompletionMessageDataType = new OpenAiCompletionMessageDataType();

#! Chat completion response choice type
public class OpenAiChoiceDataType inherits HashDataType {
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
                "short_desc": "Information comprising a message",
                "desc": "Information comprising a message",
                "type": OpenAiCompletionMessageDataType,
            },
            "logprobs": {
                "display_name": "Log Probability",
                "short_desc": "Log probability information for the message",
                "desc": "Log probability information for the message",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "finish_reason": {
                "display_name": "Finish Reason",
                "short_desc": "The reason the model stopped generating tokens",
                "desc": "The reason the model stopped generating tokens. This will be `stop` if the model hit a "
                    "natural stop point or a provided stop sequence, `length` if the maximum number of tokens "
                    "specified in the request was reached, `content_filter` if content was omitted due to a flag "
                    "from our content filters, `tool_calls` if the model called a tool, or `function_call` "
                    "(deprecated) if the model called a function",
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiChoice") {
        addQoreFields(Fields);
    }
}

#! Constant for the chat completion response choice type
public const OpenAiChoiceDataType = new OpenAiChoiceDataType();

#! The chat completion usage type
public class OpenAiChatCompletionUsageDataType inherits HashDataType {
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
                "short_desc": "Total number of tokens used in the request (prompt + completion).",
                "desc": "Total number of tokens used in the request (prompt + completion).",
                "type": AbstractDataProviderTypeMap."int",
            },
            "prompt_tokens_details": {
                "display_name": "Prompt Tokens Details",
                "short_desc": "Detailed breakdown of the number of tokens in the prompt",
                "desc": "Detailed breakdown of the number of tokens in the prompt",
                "type": new HashDataType(AutoHashOrNothingType, "PromptTokensDetails", (
                    map {$1.key: new QoreDataField({"name": $1.key} + $1.value)}, {
                        "audio_tokens": {
                            "display_name": "Audio Tokens",
                            "type": AbstractDataProviderTypeMap."int",
                            "desc": "Audio input tokens present in the prompt",
                        },
                        "cached_tokens": {
                            "display_name": "Cached Tokens",
                            "type": AbstractDataProviderTypeMap."int",
                            "desc": "Cached tokens tokens present in the prompt",
                        },
                    }.pairIterator()
                )),
            },
            "completion_tokens_details": {
                "display_name": "Completion Tokens Details",
                "short_desc": "Detailed breakdown of the number of tokens in the completion",
                "desc": "Detailed breakdown of the number of tokens in the completion",
                "type": new HashDataType(AutoHashOrNothingType, "CompletionTokensDetails", (
                    map {$1.key: new QoreDataField({"name": $1.key} + $1.value)}, {
                        "accepted_prediction_tokens": {
                            "display_name": "Accepted Prediction Tokens",
                            "type": AbstractDataProviderTypeMap."int",
                            "desc": "When using Predicted Outputs, the number of tokens in the prediction that "
                                "appeared in the completion",
                        },
                        "audio_tokens": {
                            "display_name": "Audio Tokens",
                            "type": AbstractDataProviderTypeMap."int",
                            "desc": "Audio input tokens present in the prompt",
                        },
                        "reasoning_tokens": {
                            "display_name": "Reasoning Tokens",
                            "type": AbstractDataProviderTypeMap."int",
                            "desc": "Tokens generated by the model for reasoning",
                        },
                        "rejected_prediction_tokens": {
                            "display_name": "Rejected Reasoning Tokens",
                            "type": AbstractDataProviderTypeMap."int",
                            "desc": "When using Predicted Outputs, the number of tokens in the prediction that did "
                                "not appear in the completion. However, like reasoning tokens, these tokens are still "
                                "counted in the total completion tokens for purposes of billing, output, and context "
                                "window limits",
                        },
                    }.pairIterator()
                )),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiChoice") {
        addQoreFields(Fields);
    }
}

#! Constant for the chat completion usage type
public const OpenAiChatCompletionUsageDataType = new OpenAiChatCompletionUsageDataType();

#! The chat completion response type
public class OpenAiChatCompletionResponseType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "The ID of the response",
                "desc": "The ID of the response",
                "type": AbstractDataProviderTypeMap."string",
            },
            "object": {
                "display_name": "Object",
                "short_desc": "The type of response",
                "desc": "The type of response",
                "type": AbstractDataProviderTypeMap."string",
            },
            "created": {
                "display_name": "Creation Timestamp",
                "short_desc": "The date and time the response was created",
                "desc": "The date and time the response was created",
                "type": AbstractDataProviderTypeMap."date",
            },
            "model": {
                "display_name": "Model ID",
                "short_desc": "Model used to create the response",
                "desc": "Model used to create the response",
                "type": AbstractDataProviderTypeMap."string",
            },
            "system_fingerprint": {
                "display_name": "System Fingerprint",
                "short_desc": "A code for the configuration of the OpenAI model",
                "desc": "A code for the configuration of the OpenAI model",
                "type": AbstractDataProviderTypeMap."string",
            },
            "choices": {
                "display_name": "Choices",
                "short_desc": "A list of chat completion choices",
                "desc": "A list of chat completion choices",
                "type": new ListDataType("ChoicesList", OpenAiChoiceDataType),
            },
            "usage": {
                "display_name": "Usage",
                "short_desc": "Usage statistics for the completion request",
                "desc": "Usage statistics for the completion request",
                "type": OpenAiChatCompletionUsageDataType,
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiChatCompletionRequest") {
        addQoreFields(Fields);
    }
}

#! The chat completion response type
public const OpenAiChatCompletionResponseType = new OpenAiChatCompletionResponseType();
}
