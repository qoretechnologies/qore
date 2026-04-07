# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SgLangDataProvider module definition

/** SgLangCompletionsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The SGLang text completions data provider
public class SgLangCompletionsDataProvider inherits SgLangDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "completions",
            "type": "SgLangCompletionsDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = SgLangCompletionsRequestType;

        #! Response type
        const ResponseType = SgLangCompletionsResponseType;
    }

    #! Creates the object from the arguments
    constructor(*RestClientIo::RestClientIo rest) : SgLangDataProviderCommon(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("POST", "completions", injectModel(req)).body;
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

#! Text completion choice type
public class SgLangCompletionChoiceDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "text": {
                "display_name": "Text",
                "short_desc": "The generated text",
                "desc": "The generated text completion",
                "type": AbstractDataProviderTypeMap."string",
            },
            "index": {
                "display_name": "Index",
                "short_desc": "The index of the completion choice",
                "desc": "The index of the completion choice",
                "type": AbstractDataProviderTypeMap."int",
            },
            "logprobs": {
                "display_name": "Log Probability",
                "short_desc": "Log probability information",
                "desc": "Log probability information for the completion",
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
                ),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("SgLangCompletionChoice") {
        addQoreFields(Fields);
    }
}

#! Constant for the text completion choice type
public const SgLangCompletionChoiceDataType = new SgLangCompletionChoiceDataType();

#! The text completions request type
public class SgLangCompletionsRequestType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "prompt": {
                "display_name": "Prompt",
                "short_desc": "The prompt to generate completions for",
                "desc": "The prompt to generate completions for. Can be a string or a list of strings",
                "type": AbstractDataProviderTypeMap."any",
                "example_value": "The meaning of life is",
            },
            "model": {
                "display_name": "Model ID",
                "short_desc": "Model to use for the completion",
                "desc": "Model to use for the completion. If not provided, the connection's configured "
                    "model will be used",
                "type": AbstractDataProviderTypeMap."*string",
                "attr": {
                    "ref_data": "models",
                },
            },
            "max_tokens": {
                "display_name": "Max Tokens",
                "short_desc": "The maximum number of tokens to generate",
                "desc": "The maximum number of tokens to generate in the completion",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "temperature": {
                "display_name": "Temperature",
                "short_desc": "Sampling temperature between 0 and 2",
                "desc": "What sampling temperature to use, between `0` and `2`",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "top_p": {
                "display_name": "Top P",
                "short_desc": "Top p nucleus sampling value",
                "desc": "An alternative to sampling with temperature, called nucleus sampling",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "n": {
                "display_name": "Number of Completions",
                "short_desc": "How many completions to generate",
                "desc": "How many completions to generate for the prompt",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "stop": {
                "display_name": "Stop Sequences",
                "short_desc": "Sequences where the API will stop generating",
                "desc": "Up to 4 sequences where the API will stop generating further tokens",
                "type": AbstractDataProviderTypeMap."any",
            },
            "echo": {
                "display_name": "Echo?",
                "short_desc": "Echo back the prompt in addition to the completion",
                "desc": "If `True`, echo back the prompt in addition to the completion",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "suffix": {
                "display_name": "Suffix",
                "short_desc": "The suffix that comes after a completion of inserted text",
                "desc": "The suffix that comes after a completion of inserted text",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "frequency_penalty": {
                "display_name": "Frequency Penalty",
                "short_desc": "Penalty for repeated tokens",
                "desc": "Number between `-2.0` and `2.0`. Positive values penalize new tokens based on "
                    "their existing frequency",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "presence_penalty": {
                "display_name": "Presence Penalty",
                "short_desc": "Penalty to encourage new topics",
                "desc": "Number between `-2.0` and `2.0`. Positive values penalize new tokens based on "
                    "whether they appear in the text so far",
                "type": AbstractDataProviderTypeMap."*float",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("SgLangCompletionsRequest") {
        addQoreFields(Fields);
    }
}

#! The text completions request type constant
public const SgLangCompletionsRequestType = new SgLangCompletionsRequestType();

#! The text completions response type
public class SgLangCompletionsResponseType inherits HashDataType {
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
                "short_desc": "Model used to create the completion",
                "desc": "Model used to create the completion",
                "type": AbstractDataProviderTypeMap."string",
            },
            "choices": {
                "display_name": "Choices",
                "short_desc": "A list of completion choices",
                "desc": "A list of completion choices",
                "type": new ListDataType("SgLangCompletionChoicesList", SgLangCompletionChoiceDataType),
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
    constructor() : HashDataType("SgLangCompletionsResponse") {
        addQoreFields(Fields);
    }
}

#! The text completions response type constant
public const SgLangCompletionsResponseType = new SgLangCompletionsResponseType();
}
