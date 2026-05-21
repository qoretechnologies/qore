# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CohereEmbedDataProvider class definition

/** CohereEmbedDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace CohereDataProvider {
#! Data provider for Cohere's `/v2/embed` endpoint
public class CohereEmbedDataProvider inherits CohereDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "embed",
            "type": "CohereEmbedDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        static CohereEmbedRequestDataType RequestType = new CohereEmbedRequestDataType();
        static CohereEmbedResponseDataType ResponseType = new CohereEmbedResponseDataType();
    }

    constructor(RestClientIo::RestClientIo rest) : CohereDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : CohereDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Forwards the embedding request to Cohere's `/v2/embed` endpoint
    /** Cohere v2 requires `embedding_types`; if not supplied, defaults to
        `["float"]` so the response uses `embeddings.float`.
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        if (req.typeCode() == NT_HASH && !exists req.embedding_types) {
            req.embedding_types = ("float",);
        }
        return doRestCommand("POST", "embed", req).body;
    }
}
}
