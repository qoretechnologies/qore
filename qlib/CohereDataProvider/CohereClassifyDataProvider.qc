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
#! Data provider for Cohere's <tt>/v1/classify</tt> endpoint
/** Classify uses Cohere's v1 surface — the request body path is absolute
    (<tt>/v1/classify</tt>) to address the v1 endpoint from a v2-default client.
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
    }

    private {
        #! The request type, built on first use; see requestType()
        static *CohereClassifyRequestDataType request_type;
        #! The ID of the request type's builder; see requestType()
        static int request_type_id = DataProviderActionCatalog::registerLazyValue(
            auto sub () { return CohereClassifyDataProvider::buildRequestType(); });
        #! The response type, built on first use; see responseType()
        static *CohereClassifyResponseDataType response_type;
        #! The ID of the response type's builder; see responseType()
        static int response_type_id = DataProviderActionCatalog::registerLazyValue(
            auto sub () { return CohereClassifyDataProvider::buildResponseType(); });
    }

    #! Returns the request type, which is built on first use
    /** Built from the vendored API description, so loading the module does not parse it
    */
    static CohereClassifyRequestDataType requestType() {
        return request_type ?? (request_type = DataProviderActionCatalog::getLazyValue(request_type_id));
    }

    #! Builds the request type; see requestType()
    private static CohereClassifyRequestDataType buildRequestType() {
        return new CohereClassifyRequestDataType();
    }

    #! Returns the response type, which is built on first use
    /** Built from the vendored API description, so loading the module does not parse it
    */
    static CohereClassifyResponseDataType responseType() {
        return response_type ?? (response_type = DataProviderActionCatalog::getLazyValue(response_type_id));
    }

    #! Builds the response type; see responseType()
    private static CohereClassifyResponseDataType buildResponseType() {
        return new CohereClassifyResponseDataType();
    }

    constructor(RestClientIo::RestClientIo rest) : CohereDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : CohereDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return requestType(); }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return responseType(); }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return getSchemaProvider("classify").doRequest(req, request_options);
    }
}
}
