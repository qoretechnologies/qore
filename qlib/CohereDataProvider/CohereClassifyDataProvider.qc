# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CohereClassifyDataProvider class definition

/** CohereClassifyDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Data provider for Cohere's `/v1/classify` endpoint
/** Classify uses Cohere's v1 surface — the request body path is absolute
    (`/v1/classify`) to address the v1 endpoint from a v2-default client.
*/
public class CohereClassifyDataProvider inherits CohereDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "classify",
            "type": "CohereClassifyDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        static CohereClassifyRequestDataType RequestType = new CohereClassifyRequestDataType();
        static CohereClassifyResponseDataType ResponseType =
            new CohereClassifyResponseDataType();
    }

    constructor(RestClientIo::RestClientIo rest) : CohereDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : CohereDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # use absolute path to target v1 from a v2-default REST client
        return doRestCommand("POST", "/v1/classify", req).body;
    }
}
}
