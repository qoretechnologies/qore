# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GrokDataProvider module definition

/** GrokSimpleChatCompletionDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The Grok simple chat completion data provider class
public class GrokSimpleChatCompletionDataProvider inherits GrokDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "simple-chat",
            "type": "GrokSimpleChatCompletionDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = GrokSimpleChatCompletionRequestType;

        #! Response type
        const ResponseType = GrokSimpleChatCompletionResponseType;
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
        # Transform single message into message list
        req.messages = ({
            "role": remove req.role,
            "content": remove req.content,
            "name": remove req.name,
        },);
        hash<auto> rv = doRestCommand("POST", "chat/completions", req).body;
        # Extract and flatten the first choice for simple chat
        if (!rv.choices || !rv.choices.size()) {
            throw "GROK-API-ERROR", "API returned no choices in chat completion response";
        }
        hash<auto> choice = rv.choices[0];
        return {
            "content": choice.message.content,
            "role": choice.message.role,
            "reasoning_content": choice.message.reasoning_content,
            "refusal": choice.message.refusal,
            "tool_calls": choice.message.tool_calls,
            "finish_reason": choice.finish_reason,
            "model": rv.model,
            "usage": rv.usage,
            "citations": rv.citations,
        };
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

#! Simple chat completion request type
public class GrokSimpleChatCompletionRequestType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "model": GrokChatCompletionRequestType::Fields."model",
            "role": {
                "display_name": "Role",
                "short_desc": "The role of the message author",
                "desc": "The role of the author of this message",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "User",
                        "short_desc": "The author is a user",
                        "desc": "The author is a user",
                        "value": "user",
                    },
                    <AllowedValueInfo>{
                        "display_name": "System",
                        "short_desc": "The author is the system",
                        "desc": "The author is the system",
                        "value": "system",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Assistant",
                        "short_desc": "The author is an assistant",
                        "desc": "The author is an assistant",
                        "value": "assistant",
                    },
                ),
                "default_value": "user",
            },
            "content": {
                "display_name": "Content",
                "short_desc": "The message content",
                "desc": "The message content to send to the model",
                "type": AbstractDataProviderTypeMap."string",
                "example_value": "What is the capital of France?",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "An optional name for the participant",
                "desc": "An optional name for the participant",
                "type": AbstractDataProviderTypeMap."*string",
            },
        } + (GrokChatCompletionRequestType::Fields - ("messages", "model", "n"));
    }

    #! Creates the type
    constructor() : HashDataType("GrokSimpleChatCompletionRequest") {
        addQoreFields(Fields);
    }
}

#! Constant for simple chat completion request type
public const GrokSimpleChatCompletionRequestType = new GrokSimpleChatCompletionRequestType();

#! Simple chat completion response type - flattened user-friendly response
public class GrokSimpleChatCompletionResponseType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "content": {
                "display_name": "Content",
                "short_desc": "The response text from the model",
                "desc": "The text content of the model's response",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "role": {
                "display_name": "Role",
                "short_desc": "The role of the response author",
                "desc": "The role of the response author (always `assistant`)",
                "type": AbstractDataProviderTypeMap."string",
            },
            "reasoning_content": GrokCompletionMessageDataType::Fields."reasoning_content",
            "refusal": GrokCompletionMessageDataType::Fields."refusal",
            "tool_calls": GrokCompletionMessageDataType::Fields."tool_calls",
            "finish_reason": GrokChoiceDataType::Fields."finish_reason",
            "model": GrokChatCompletionResponseType::Fields."model",
            "usage": GrokChatCompletionResponseType::Fields."usage",
            "citations": GrokChatCompletionResponseType::Fields."citations",
        };
    }

    #! Creates the type
    constructor() : HashDataType("GrokSimpleChatCompletionResponse") {
        addQoreFields(Fields);
    }
}

#! Constant for simple chat completion response type
public const GrokSimpleChatCompletionResponseType = new GrokSimpleChatCompletionResponseType();
}
