# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SgLangDataProvider module definition

/** SgLangGenerateDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The SGLang native generate data provider
/** This provider uses the native SGLang \c /generate endpoint which is
    not under the \c /v1/ OpenAI-compatible API prefix.
*/
public class SgLangGenerateDataProvider inherits SgLangDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "generate",
            "type": "SgLangGenerateDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = SgLangGenerateRequestType;

        #! Response type
        const ResponseType = SgLangGenerateResponseType;
    }

    #! Creates the object from the arguments
    constructor(*RestClientIo::RestClientIo rest) : SgLangDataProviderCommon(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Makes a request and returns the response
    /** Uses the native SGLang \c /generate endpoint (not under \c /v1/).
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doNativeRestCommand("POST", "generate", req).body;
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

#! The sampling parameters data type for the native generate endpoint
public class SgLangSamplingParamsDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "max_new_tokens": {
                "display_name": "Max New Tokens",
                "short_desc": "Maximum number of output tokens",
                "desc": "Maximum number of output tokens to generate (default: `128`)",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "temperature": {
                "display_name": "Temperature",
                "short_desc": "Sampling temperature",
                "desc": "Sampling temperature for randomness in token selection",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "top_p": {
                "display_name": "Top P",
                "short_desc": "Nucleus sampling threshold",
                "desc": "Nucleus sampling threshold. `0.1` means only the tokens comprising the top 10% "
                    "probability mass are considered",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "top_k": {
                "display_name": "Top K",
                "short_desc": "Restrict to top-k tokens",
                "desc": "Restrict sampling to the top-k tokens",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "min_p": {
                "display_name": "Min P",
                "short_desc": "Minimum probability threshold",
                "desc": "Minimum probability threshold for token selection",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "frequency_penalty": {
                "display_name": "Frequency Penalty",
                "short_desc": "Penalize token frequency",
                "desc": "Penalize tokens based on their frequency in the text so far. Range: `-2.0` to `2.0`",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "presence_penalty": {
                "display_name": "Presence Penalty",
                "short_desc": "Penalize previously sampled tokens",
                "desc": "Penalize tokens that have appeared in the text so far. Range: `-2.0` to `2.0`",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "stop": {
                "display_name": "Stop Words",
                "short_desc": "Stop word(s) to end generation",
                "desc": "Stop word(s) that end generation when encountered",
                "type": AbstractDataProviderTypeMap."any",
            },
            "ignore_eos": {
                "display_name": "Ignore EOS?",
                "short_desc": "Continue past end-of-sequence token",
                "desc": "If `True`, continue generating past the end-of-sequence token",
                "type": AbstractDataProviderTypeMap."*bool",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("SgLangSamplingParams") {
        addQoreFields(Fields);
    }
}

#! Constant for the sampling parameters data type
public const SgLangSamplingParamsDataType = new SgLangSamplingParamsDataType();

#! The native generate request type
public class SgLangGenerateRequestType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "text": {
                "display_name": "Text",
                "short_desc": "The prompt text to generate from",
                "desc": "The prompt text to generate completions from",
                "type": AbstractDataProviderTypeMap."string",
                "example_value": "What is the capital of France?",
            },
            "sampling_params": {
                "display_name": "Sampling Parameters",
                "short_desc": "Parameters controlling the generation",
                "desc": "Parameters controlling the generation process, including temperature, top_p, "
                    "max_new_tokens, and other sampling settings",
                "type": SgLangSamplingParamsDataType.getOrNothingType(),
            },
            "return_logprob": {
                "display_name": "Return Log Probabilities?",
                "short_desc": "Whether to return log probabilities",
                "desc": "If `True`, return log probabilities for each generated token",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "logprob_start_len": {
                "display_name": "Log Prob Start Length",
                "short_desc": "Number of prompt tokens to skip for log probs",
                "desc": "Number of prompt tokens to skip before returning log probabilities",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("SgLangGenerateRequest") {
        addQoreFields(Fields);
    }
}

#! The native generate request type constant
public const SgLangGenerateRequestType = new SgLangGenerateRequestType();

#! The meta info data type for generate responses
public class SgLangGenerateMetaInfoDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "id": {
                "display_name": "Request ID",
                "short_desc": "The request identifier",
                "desc": "The request identifier",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "finish_reason": {
                "display_name": "Finish Reason",
                "short_desc": "The reason generation stopped",
                "desc": "The reason generation stopped",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Stop",
                        "short_desc": "Natural stop point or stop word reached",
                        "desc": "Generation stopped at a natural stop point or stop word",
                        "value": "stop",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Length",
                        "short_desc": "Maximum token limit reached",
                        "desc": "The maximum number of tokens was reached",
                        "value": "length",
                    },
                ),
            },
            "prompt_tokens": {
                "display_name": "Prompt Tokens",
                "short_desc": "Number of tokens in the prompt",
                "desc": "Number of tokens in the prompt",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "completion_tokens": {
                "display_name": "Completion Tokens",
                "short_desc": "Number of tokens generated",
                "desc": "Number of tokens generated in the completion",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("SgLangGenerateMetaInfo") {
        addQoreFields(Fields);
    }
}

#! Constant for the meta info data type
public const SgLangGenerateMetaInfoDataType = new SgLangGenerateMetaInfoDataType();

#! The native generate response type
public class SgLangGenerateResponseType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "text": {
                "display_name": "Text",
                "short_desc": "The generated text",
                "desc": "The generated text output",
                "type": AbstractDataProviderTypeMap."string",
            },
            "meta_info": {
                "display_name": "Meta Info",
                "short_desc": "Metadata about the generation",
                "desc": "Metadata about the generation, including token counts and finish reason",
                "type": SgLangGenerateMetaInfoDataType,
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("SgLangGenerateResponse") {
        addQoreFields(Fields);
    }
}

#! The native generate response type constant
public const SgLangGenerateResponseType = new SgLangGenerateResponseType();
}
