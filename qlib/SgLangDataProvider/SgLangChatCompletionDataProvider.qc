# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SgLangDataProvider module definition

/** SgLangChatCompletionDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the SgLangDataProvider module
public namespace SgLangDataProvider {
#! The SGLang chat completion data provider
public class SgLangChatCompletionDataProvider inherits SgLangDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "multi-chat",
            "type": "SgLangChatCompletionDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = SgLangChatCompletionRequestType;

        #! Response type
        const ResponseType = SgLangChatCompletionResponseType;
    }

    #! Creates the object from the arguments
    constructor(*RestClientIo::RestClientIo rest) : SgLangDataProviderCommon(rest) {
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
        return doRestCommand("POST", "chat/completions", injectModel(req)).body;
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

#! Chat completion request message data type
public class SgLangMessageDataType inherits HashDataType {
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
                "short_desc": "The tool calls generated by the model",
                "desc": "The tool calls generated by the model, such as function calls",
                "type": AbstractDataProviderTypeMap."*list",
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
    constructor() : HashDataType("SgLangMessage") {
        addQoreFields(Fields);
    }
}

#! Chat completion message constant
public const SgLangMessageDataType = new SgLangMessageDataType();

#! Chat completion response message data type
public class SgLangCompletionMessageDataType inherits HashDataType {
    public {
        #! fields
        const Fields = SgLangMessageDataType::Fields{"role", "content", "tool_calls"};
    }

    #! Creates the type
    constructor() : HashDataType("SgLangCompletionMessage") {
        addQoreFields(Fields);
    }
}

#! Constant for the completion message data type
public const SgLangCompletionMessageDataType = new SgLangCompletionMessageDataType();

#! The function data type
public class SgLangFunctionDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "description": {
                "display_name": "Description",
                "short_desc": "A description of what the function does",
                "desc": "A description of what the function does, used by the model to choose when and how to "
                    "call the function",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "The name of the function to be called",
                "desc": "The name of the function to be called. Must be a-z, A-Z, 0-9, or contain underscores "
                    "and dashes, with a maximum length of 64",
                "type": AbstractDataProviderTypeMap."string",
            },
            "parameters": {
                "display_name": "Parameters",
                "short_desc": "The parameters the function accepts",
                "desc": "The parameters the function accepts, described as a JSON Schema object",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("SgLangFunction") {
        addQoreFields(Fields);
    }
}

#! Constant for the function data type
public const SgLangFunctionDataType = new SgLangFunctionDataType();

#! The tool data type
public class SgLangToolDataType inherits HashDataType {
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
                "type": SgLangFunctionDataType,
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("SgLangTool") {
        addQoreFields(Fields);
    }
}

#! Constant for the tool data type
public const SgLangToolDataType = new SgLangToolDataType();

#! The chat completion request type
public class SgLangChatCompletionRequestType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "messages": {
                "display_name": "Messages",
                "short_desc": "A list of messages comprising the conversation so far",
                "desc": "A list of messages comprising the conversation so far",
                "type": new ListDataType("SgLangMessages", SgLangMessageDataType),
            },
            "model": {
                "display_name": "Model ID",
                "short_desc": "Model to use for the chat completion",
                "desc": "Model to use for the chat completion. If not provided, the connection's configured "
                    "model will be used",
                "type": AbstractDataProviderTypeMap."*string",
                "attr": {
                    "ref_data": "models",
                },
            },
            "frequency_penalty": {
                "display_name": "Frequency Penalty",
                "short_desc": "Penalty to reduce repeated tokens in the output",
                "desc": "Number between `-2.0` and `2.0`. Positive values penalize new tokens based on their "
                    "existing frequency in the text so far, decreasing the model's likelihood to repeat the "
                    "same line verbatim",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "max_tokens": {
                "display_name": "Max Tokens",
                "short_desc": "The maximum number of tokens to generate",
                "desc": "The maximum number of tokens that can be generated in the chat completion",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "n": {
                "display_name": "Number of Choices",
                "short_desc": "The number of choices to generate for each input message",
                "desc": "How many chat completion choices to generate for each input message. Keep `n` as "
                    "`1` (the default) to minimize costs",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "presence_penalty": {
                "display_name": "Presence Penalty",
                "short_desc": "Penalty to encourage new topics",
                "desc": "Number between `-2.0` and `2.0`. Positive values penalize new tokens based on "
                    "whether they appear in the text so far, increasing the model's likelihood to talk "
                    "about new topics",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "response_format": {
                "display_name": "Response Format",
                "short_desc": "The response format",
                "desc": "An object specifying the format that the model must output.\n\n"
                    "Setting to `{\"type\": \"json_object\"}` enables JSON mode, which guarantees the "
                    "message the model generates is valid JSON",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "seed": {
                "display_name": "Seed",
                "short_desc": "A value for deterministic sampling",
                "desc": "If specified, the system will make a best effort to sample deterministically, "
                    "such that repeated requests with the same seed and parameters should return the "
                    "same result",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "stop": {
                "display_name": "Stop Sequences",
                "short_desc": "Up to 4 sequences where the API will stop generating",
                "desc": "Up to 4 sequences where the API will stop generating further tokens",
                "type": AbstractDataProviderTypeMap."any",
            },
            "stream": {
                "display_name": "Stream?",
                "short_desc": "Set to enable partial message deltas",
                "desc": "If set, partial message deltas will be sent as server-sent events as they become "
                    "available, with the stream terminated by a `data: [DONE]` message",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "temperature": {
                "display_name": "Sampling Temperature",
                "short_desc": "The sampling temperature to use, between 0 and 2",
                "desc": "What sampling temperature to use, between `0` and `2`. Higher values like `0.8` "
                    "will make the output more random, while lower values like `0.2` will make it more "
                    "focused and deterministic.\n\n"
                    "We generally recommend altering this or `top_p` but not both",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "top_p": {
                "display_name": "Top P",
                "short_desc": "Top p nucleus sampling value",
                "desc": "An alternative to sampling with temperature, called nucleus sampling, where the "
                    "model considers the results of the tokens with `top_p` probability mass. So `0.1` "
                    "means only the tokens comprising the top 10% probability mass are considered.\n\n"
                    "We generally recommend altering this or `temperature` but not both",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "tools": {
                "display_name": "Tools",
                "short_desc": "A list of tools the model may call",
                "desc": "A list of tools the model may call. Currently, only functions are supported as a "
                    "tool. Use this to provide a list of functions the model may generate JSON inputs for",
                "type": new ListDataType("*SgLangToolList", SgLangToolDataType, True),
            },
            "tool_choice": {
                "display_name": "Tool Choice",
                "short_desc": "Controls which tool is called by the model",
                "desc": "Controls which (if any) tool is called by the model.\n\n"
                    "- `none`: the model will not call any tool and instead generates a message\n"
                    "- `auto`: the model can pick between generating a message or calling one or more tools\n"
                    "- `required`: the model must call one or more tools",
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
            "user": {
                "display_name": "User",
                "short_desc": "A unique identifier representing your end-user",
                "desc": "A unique identifier representing your end-user",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("SgLangChatCompletionRequest") {
        addQoreFields(Fields);
    }
}

#! The chat completion request type constant
public const SgLangChatCompletionRequestType = new SgLangChatCompletionRequestType();

#! Chat completion response choice type
public class SgLangChoiceDataType inherits HashDataType {
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
                "type": SgLangCompletionMessageDataType,
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
                "desc": "The reason the model stopped generating tokens",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Stop",
                        "short_desc": "Natural stop point or provided stop sequence",
                        "desc": "The model hit a natural stop point or a provided stop sequence",
                        "value": "stop",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Length",
                        "short_desc": "Maximum number of tokens reached",
                        "desc": "The maximum number of tokens was reached",
                        "value": "length",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Tool Calls",
                        "short_desc": "The model called a tool",
                        "desc": "The model called a tool",
                        "value": "tool_calls",
                    },
                ),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("SgLangChoice") {
        addQoreFields(Fields);
    }
}

#! Constant for the chat completion response choice type
public const SgLangChoiceDataType = new SgLangChoiceDataType();

#! The chat completion usage type
public class SgLangChatCompletionUsageDataType inherits HashDataType {
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
        };
    }

    #! Creates the type
    constructor() : HashDataType("SgLangChatCompletionUsage") {
        addQoreFields(Fields);
    }
}

#! Constant for the chat completion usage type
public const SgLangChatCompletionUsageDataType = new SgLangChatCompletionUsageDataType();

#! The chat completion response type
public class SgLangChatCompletionResponseType inherits HashDataType {
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
            "choices": {
                "display_name": "Choices",
                "short_desc": "A list of chat completion choices",
                "desc": "A list of chat completion choices",
                "type": new ListDataType("SgLangChoicesList", SgLangChoiceDataType),
            },
            "usage": {
                "display_name": "Usage",
                "short_desc": "Usage statistics for the completion request",
                "desc": "Usage statistics for the completion request",
                "type": SgLangChatCompletionUsageDataType,
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("SgLangChatCompletionResponse") {
        addQoreFields(Fields);
    }
}

#! The chat completion response type constant
public const SgLangChatCompletionResponseType = new SgLangChatCompletionResponseType();
}
