# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BedrockEmbedDataProvider class

/** BedrockEmbedDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Data provider for Bedrock embedding models (Titan + Cohere)
/** Calls \c POST \c /model/\{modelId\}/invoke; the request body shape
    depends on the model family (Titan uses \c inputText; Cohere uses
    \c texts + \c input_type).  The action accepts either shape and routes
    appropriately based on the model ID prefix.
*/
public class BedrockEmbedDataProvider inherits BedrockDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "embed",
            "type": "BedrockEmbedDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = BedrockEmbedRequestDataType;
        const ResponseType = BedrockEmbedResponseDataType;
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

    #! Invokes a Bedrock embedding model and returns the JSON response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string model_id = remove req.modelId;
        hash<auto> body = BedrockEmbedDataProvider::buildEmbedBody(model_id, req);
        string path = sprintf("/model/%s/invoke", encode_url(model_id, True));
        return doRestCommand("POST", path, body).body;
    }

    #! Builds the request body for the right embedding model family
    private:internal static hash<auto> buildEmbedBody(string model_id, hash<auto> req) {
        if (model_id =~ /^amazon\.titan-embed/) {
            hash<auto!> body;
            body.inputText = req.inputText ?? (req.texts ? req.texts[0] : "");
            if (req.dimensions.typeCode() == NT_INT) {
                body.dimensions = req.dimensions;
            }
            if (exists req.normalize) {
                body.normalize = req.normalize;
            }
            return body;
        }
        if (model_id =~ /^cohere\.embed/) {
            list<auto> texts = req.texts ?? (req.inputText ? (req.inputText,) : ());
            hash<auto!> body = {
                "texts": texts,
                "input_type": req.input_type ?? "search_document",
            };
            return body;
        }
        # default to a Titan-shaped body
        return {"inputText": req.inputText ?? (req.texts ? req.texts[0] : "")};
    }
}
}
