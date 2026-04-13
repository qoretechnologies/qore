# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SGLangDataProvider module definition

/** SGLangGenerateDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Data provider for SGLang's native `/generate` endpoint with constrained decoding
/** Calls the `POST /generate` endpoint (not `/v1/*`).  This is SGLang's native
    interface and the one that enables constrained decoding: the model is
    guaranteed to emit output matching a JSON schema, regex, or EBNF grammar.

    The provider accepts flattened convenience fields (`temperature`, `max_new_tokens`,
    `json_schema`, `regex`, `ebnf`) and merges them into the `sampling_params` hash
    before dispatching, so callers can set them at the top level in action forms
    without having to build the nested structure manually.
*/
public class SGLangGenerateDataProvider inherits SGLangDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "generate",
            "type": "SGLangGenerateDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        static SGLangGenerateRequestDataType RequestType = new SGLangGenerateRequestDataType();

        #! Response type
        static SGLangGenerateResponseDataType ResponseType = new SGLangGenerateResponseDataType();

        #! Flattened sampling parameter keys merged into `sampling_params`
        const FlatSamplingKeys = ("temperature", "max_new_tokens", "json_schema", "regex", "ebnf");
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

    #! Makes a request and returns the response
    /** Merges any flattened sampling-parameter convenience fields into `sampling_params`
        before dispatching.  The `/generate` endpoint does not take a `model` field —
        the server serves exactly one model, so there is nothing to override per-request.
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        if (req.typeCode() == NT_HASH) {
            hash<auto> sampling = req.sampling_params.typeCode() == NT_HASH ? req.sampling_params : {};
            foreach string k in (FlatSamplingKeys) {
                if (exists req{k} && req{k}.val()) {
                    sampling{k} = req{k};
                    remove req{k};
                }
            }
            if (sampling) {
                req.sampling_params = sampling;
            }
        }
        return doRestCommand("POST", "generate", req).body;
    }
}
}
