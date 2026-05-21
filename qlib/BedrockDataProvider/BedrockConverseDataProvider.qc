# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BedrockConverseDataProvider class

/** BedrockConverseDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BedrockDataProvider module
public namespace BedrockDataProvider {
#! Data provider for the unified Bedrock Converse API
/** Calls \c POST \c /model/\{modelId\}/converse with the model-agnostic
    Converse body shape that works across the Anthropic, Llama, Mistral,
    Cohere, AI21, Titan, and DeepSeek-on-Bedrock model families.
*/
public class BedrockConverseDataProvider inherits BedrockDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "converse",
            "type": "BedrockConverseDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = BedrockConverseRequestDataType;
        const ResponseType = BedrockConverseResponseDataType;
    }

    constructor(AwsRestClient::AwsRestClientIo rest) : BedrockDataProviderCommon(rest) {
    }

    constructor(*hash<auto> options) : BedrockDataProviderCommon(options) {
    }

    string getName() {
        return ProviderInfo.name;
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Makes a Bedrock Converse request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string model_id = remove req.modelId;
        string path = sprintf("/model/%s/converse", encode_url(model_id, True));
        return doRestCommand("POST", path, req).body;
    }
}
}
