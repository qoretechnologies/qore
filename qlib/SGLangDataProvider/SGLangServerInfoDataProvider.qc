# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SGLangDataProvider module definition

/** SGLangServerInfoDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the SGLangDataProvider module
public namespace SGLangDataProvider {
#! Data type for SGLang `/get_server_info` responses
/** The SGLang server exposes a grab-bag of configuration and runtime metadata via
    this endpoint.  The specific fields returned vary by SGLang version and launch
    configuration, so every field here is optional.
*/
public class SGLangServerInfoResponseDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "version": {
                "display_name": "SGLang Version",
                "desc": "The running SGLang server version string.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "model_path": {
                "display_name": "Model Path",
                "desc": "Filesystem path or HuggingFace repo id of the loaded model.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "tokenizer_path": {
                "display_name": "Tokenizer Path",
                "desc": "Filesystem path or repo id of the loaded tokenizer.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "max_total_tokens": {
                "display_name": "Max Total Tokens",
                "desc": "Maximum total tokens (prompt + completion) supported across all concurrent "
                    "requests.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "max_prefill_tokens": {
                "display_name": "Max Prefill Tokens",
                "desc": "Maximum number of tokens the server will prefill in a single batch.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "max_running_requests": {
                "display_name": "Max Running Requests",
                "desc": "Maximum number of concurrent in-flight requests.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "context_length": {
                "display_name": "Context Length",
                "desc": "Maximum context length of the loaded model (in tokens).",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "chat_template": {
                "display_name": "Chat Template",
                "desc": "Name or inline source of the chat template applied on the "
                    "`/v1/chat/completions` endpoint.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "tp_size": {
                "display_name": "Tensor Parallel Size",
                "desc": "Number of GPUs used for tensor parallelism.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "dp_size": {
                "display_name": "Data Parallel Size",
                "desc": "Number of data-parallel replicas.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "cuda_device": {
                "display_name": "CUDA Device",
                "desc": "CUDA device identifier(s) the server is bound to.",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for the SGLang `/get_server_info` administrative endpoint
/** This action takes no input; the response is the server's current runtime and
    configuration snapshot.  Useful for health checks, configuration validation,
    and debugging deployment drift.
*/
public class SGLangServerInfoDataProvider inherits SGLangDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "server-info",
            "type": "SGLangServerInfoDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type — no input fields
        static SGLangServerInfoResponseDataType ResponseType = new SGLangServerInfoResponseDataType();
    }

    #! Creates the object from a RestClientIo client
    constructor(RestClientIo::RestClientIo rest) : SGLangDataProviderCommon(rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : SGLangDataProviderCommon(options) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the description of a successful request message, if any
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        # no request fields
        return;
    }

    #! Returns the description of a response message, if this object represents a response message
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("GET", "get_server_info", NOTHING).body;
    }
}
}
